// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// HTTP server interface for gemma.cpp (similar to llama-server)

#include <stdio.h>
#include <signal.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <mutex>
#include <unordered_map>

#undef CPPHTTPLIB_OPENSSL_SUPPORT
#undef CPPHTTPLIB_ZLIB_SUPPORT
#include "httplib.h"
#include "nlohmann/json.hpp"

#include "compression/types.h"
#include "gemma/gemma.h"
#include "gemma/gemma_args.h"
#include "gemma/tokenizer.h"
#include "ops/matmul.h"
#include "util/args.h"
#include "hwy/base.h"
#include "hwy/profiler.h"

using json = nlohmann::json;

namespace gcpp {

static std::atomic<bool> server_running{true};

struct ServerState {
  std::unique_ptr<Gemma> gemma;
  MatMulEnv* env;
  ThreadingContext* ctx;

  struct Session {
    std::unique_ptr<KVCache> kv_cache;
    size_t abs_pos = 0;
    std::chrono::steady_clock::time_point last_access;
  };

  std::unordered_map<std::string, Session> sessions;
  std::mutex sessions_mutex;
  std::mutex inference_mutex;

  void CleanupOldSessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto now = std::chrono::steady_clock::now();
    for (auto it = sessions.begin(); it != sessions.end();) {
      if (now - it->second.last_access > std::chrono::minutes(30)) {
        it = sessions.erase(it);
      } else {
        ++it;
      }
    }
  }

  Session& GetOrCreateSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto& session = sessions[session_id];
    if (!session.kv_cache) {
      session.kv_cache = std::make_unique<KVCache>(gemma->Config(), InferenceArgs(), env->ctx.allocator);
    }
    session.last_access = std::chrono::steady_clock::now();
    return session;
  }
};

std::string GenerateSessionId() {
  static std::atomic<uint64_t> counter{0};
  std::stringstream ss;
  ss << "session_" << std::hex
     << std::chrono::steady_clock::now().time_since_epoch().count() << "_"
     << counter.fetch_add(1);
  return ss.str();
}

// Simple completion endpoint
void HandleCompletion(ServerState& state, const httplib::Request& req, httplib::Response& res) {
  try {
    json request = json::parse(req.body);
    
    std::string prompt = request.value("prompt", "");
    if (prompt.empty()) {
      res.status = 400;
      res.set_content(json{{"error", "Missing 'prompt' field"}}.dump(), "application/json");
      return;
    }

    std::string session_id = request.value("session_id", GenerateSessionId());
    auto& session = state.GetOrCreateSession(session_id);

    std::lock_guard<std::mutex> lock(state.inference_mutex);

    RuntimeConfig runtime_config;
    runtime_config.verbosity = 0;
    runtime_config.temperature = request.value("temperature", 1.0f);
    runtime_config.top_k = request.value("top_k", 1);
    runtime_config.max_generated_tokens = request.value("max_tokens", 2048);

    std::string full_response;
    std::vector<int> tokens = WrapAndTokenize(
        state.gemma->Tokenizer(), state.gemma->ChatTemplate(),
        state.gemma->Config().wrapping, session.abs_pos, prompt);

    TimingInfo timing_info = {.verbosity = 0};
    size_t prefix_end = 0;
    size_t prompt_size = tokens.size();

    runtime_config.stream_token = [&](int token, float) {
      if (session.abs_pos < prompt_size) {
        session.abs_pos++;
        return true;
      }
      session.abs_pos++;
      if (state.gemma->Config().IsEOS(token)) {
        return true;
      }
      std::string token_text;
      state.gemma->Tokenizer().Decode(std::vector<int>{token}, &token_text);
      full_response += token_text;
      return true;
    };

    state.gemma->Generate(runtime_config, tokens, session.abs_pos, prefix_end,
                          *session.kv_cache, *state.env, timing_info);

    json response = {
      {"text", full_response},
      {"session_id", session_id},
      {"tokens_generated", session.abs_pos - prompt_size},
      {"prompt_tokens", prompt_size}
    };

    res.set_content(response.dump(), "application/json");

  } catch (const json::exception& e) {
    res.status = 400;
    res.set_content(json{{"error", std::string("JSON error: ") + e.what()}}.dump(), "application/json");
  } catch (const std::exception& e) {
    res.status = 500;
    res.set_content(json{{"error", std::string("Server error: ") + e.what()}}.dump(), "application/json");
  }
}

// Streaming completion endpoint
void HandleCompletionStream(ServerState& state, const httplib::Request& req, httplib::Response& res) {
  try {
    json request = json::parse(req.body);
    
    std::string prompt = request.value("prompt", "");
    if (prompt.empty()) {
      res.status = 400;
      res.set_content(json{{"error", "Missing 'prompt' field"}}.dump(), "application/json");
      return;
    }

    std::string session_id = request.value("session_id", GenerateSessionId());

    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    res.set_header("X-Session-Id", session_id);

    res.set_chunked_content_provider(
      "text/event-stream",
      [&state, request, prompt, session_id](size_t offset, httplib::DataSink& sink) {
        try {
          std::lock_guard<std::mutex> lock(state.inference_mutex);
          auto& session = state.GetOrCreateSession(session_id);

          RuntimeConfig runtime_config;
          runtime_config.verbosity = 0;
          runtime_config.temperature = request.value("temperature", 1.0f);
          runtime_config.top_k = request.value("top_k", 1);
          runtime_config.max_generated_tokens = request.value("max_tokens", 2048);

          std::vector<int> tokens = WrapAndTokenize(
              state.gemma->Tokenizer(), state.gemma->ChatTemplate(),
              state.gemma->Config().wrapping, session.abs_pos, prompt);

          size_t prompt_size = tokens.size();
          
          runtime_config.stream_token = [&](int token, float) {
            if (session.abs_pos < prompt_size) {
              session.abs_pos++;
              return true;
            }
            session.abs_pos++;
            if (state.gemma->Config().IsEOS(token)) {
              return true;
            }
            std::string token_text;
            state.gemma->Tokenizer().Decode(std::vector<int>{token}, &token_text);
            
            json event = {{"text", token_text}};
            std::string sse_data = "data: " + event.dump() + "\n\n";
            sink.write(sse_data.data(), sse_data.size());
            return true;
          };

          TimingInfo timing_info = {.verbosity = 0};
          size_t prefix_end = 0;

          state.gemma->Generate(runtime_config, tokens, session.abs_pos,
                                prefix_end, *session.kv_cache, *state.env,
                                timing_info);

          json final_event = {
            {"done", true},
            {"session_id", session_id},
            {"tokens_generated", session.abs_pos - prompt_size}
          };
          std::string final_sse = "data: " + final_event.dump() + "\n\n";
          sink.write(final_sse.data(), final_sse.size());
          sink.done();
          return false;

        } catch (const std::exception& e) {
          json error_event = {{"error", e.what()}};
          std::string error_sse = "data: " + error_event.dump() + "\n\n";
          sink.write(error_sse.data(), error_sse.size());
          return false;
        }
      }
    );

  } catch (const json::exception& e) {
    res.status = 400;
    res.set_content(json{{"error", std::string("JSON error: ") + e.what()}}.dump(), "application/json");
  }
}

// Health check endpoint
void HandleHealth(const httplib::Request&, httplib::Response& res) {
  json response = {
    {"status", "ok"},
    {"model", "gemma"}
  };
  res.set_content(response.dump(), "application/json");
}

void RunServer(const LoaderArgs& loader, const ThreadingArgs& threading,
               const InferenceArgs& inference) {
  std::cout << "Loading model..." << std::endl;

  ThreadingContext ctx(threading);
  MatMulEnv env(ctx);

  ServerState state;
  state.gemma = std::make_unique<Gemma>(loader, inference, ctx);
  state.env = &env;
  state.ctx = &ctx;

  httplib::Server server;

  // Set up routes
  server.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("Gemma Server - Use POST /completion or /v1/completions", "text/plain");
  });

  server.Get("/health", HandleHealth);
  
  server.Post("/completion", [&state](const httplib::Request& req, httplib::Response& res) {
    HandleCompletion(state, req, res);
  });

  server.Post("/completion/stream", [&state](const httplib::Request& req, httplib::Response& res) {
    HandleCompletionStream(state, req, res);
  });

  // OpenAI-compatible endpoint
  server.Post("/v1/completions", [&state](const httplib::Request& req, httplib::Response& res) {
    HandleCompletion(state, req, res);
  });

  // Periodic cleanup
  std::thread cleanup_thread([&state]() {
    while (server_running) {
      std::this_thread::sleep_for(std::chrono::minutes(5));
      state.CleanupOldSessions();
    }
  });

  std::cout << "Model loaded successfully" << std::endl;
  std::cout << "Starting server on http://0.0.0.0:" << inference.port << std::endl;
  std::cout << "\nEndpoints:" << std::endl;
  std::cout << "  GET  /health" << std::endl;
  std::cout << "  POST /completion" << std::endl;
  std::cout << "  POST /completion/stream (SSE)" << std::endl;
  std::cout << "  POST /v1/completions (OpenAI-compatible)" << std::endl;
  std::cout << "\nExample usage:" << std::endl;
  std::cout << "  curl -X POST http://localhost:" << inference.port << "/completion \\" << std::endl;
  std::cout << "    -H \"Content-Type: application/json\" \\" << std::endl;
  std::cout << "    -d '{\"prompt\": \"Hello, how are you?\", \"temperature\": 0.7}'" << std::endl;
  std::cout << std::endl;

  if (!server.listen("0.0.0.0", inference.port)) {
    std::cerr << "Failed to start server on port " << inference.port << std::endl;
  }

  cleanup_thread.join();
}

}  // namespace gcpp

int main(int argc, char** argv) {
  gcpp::InternalInit();

  gcpp::LoaderArgs loader(argc, argv);
  gcpp::ThreadingArgs threading(argc, argv);
  gcpp::InferenceArgs inference(argc, argv);

  if (gcpp::HasHelp(argc, argv)) {
    std::cout << "\nGemma Server - HTTP API server\n";
    std::cout << "===============================\n\n";
    std::cout << "Usage: " << argv[0] << " --weights <path> --tokenizer <path> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --port PORT      Server port (default: 8080)\n";
    std::cout << "  --model MODEL    Model name (default: gemma3-4b)\n\n";
    std::cout << "\n*Model Loading Arguments*\n\n";
    loader.Help();
    std::cout << "\n*Threading Arguments*\n\n";
    threading.Help();
    std::cout << "\n*Inference Arguments*\n\n";
    inference.Help();
    std::cout << "\n";
    return 0;
  }

  if (loader.weights.path.empty()) {
    std::cerr << "Error: --weights is required\n";
    std::cerr << "Use --help for usage information\n";
    return 1;
  }

  gcpp::RunServer(loader, threading, inference);

  return 0;
}
