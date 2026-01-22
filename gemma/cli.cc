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

// Simple CLI interface for gemma.cpp (similar to llama-cli)

#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

#include "compression/types.h"
#include "evals/benchmark_helper.h"
#include "gemma/gemma.h"
#include "gemma/gemma_args.h"
#include "gemma/tokenizer.h"
#include "ops/matmul.h"
#include "paligemma/image.h"
#include "util/args.h"
#include "hwy/base.h"
#include "hwy/profiler.h"

namespace gcpp {

void RunCLI(const LoaderArgs& loader, const ThreadingArgs& threading,
            const InferenceArgs& inference) {
  // Initialize model
  ThreadingContext ctx(threading);
  MatMulEnv env(ctx);
  const Gemma gemma(loader, inference, ctx);
  KVCache kv_cache(gemma.Config(), inference, ctx.allocator);

  const ModelConfig& config = gemma.Config();
  size_t abs_pos = 0;
  size_t tokens_generated = 0;
  size_t prompt_size = 0;

  // Handle image input for PaliGemma
  const bool have_image = !inference.image_file.path.empty();
  Image image;
  const size_t pool_dim = config.vit_config.pool_dim;
  ImageTokens image_tokens(
      "image_tokens",
      have_image ? Extents2D(config.vit_config.seq_len / (pool_dim * pool_dim),
                             config.model_dim)
                 : Extents2D(0, 0),
      env.ctx.allocator, MatPadding::kOdd);
  image_tokens.AllocateAndAttachRowPtrs(env.row_ptrs);
  
  if (have_image) {
    HWY_ASSERT(config.wrapping == PromptWrapping::PALIGEMMA ||
               config.wrapping == PromptWrapping::GEMMA_VLM);
    HWY_ASSERT(image.ReadPPM(inference.image_file.path));
    const size_t image_size = config.vit_config.image_size;
    image.Resize(image_size, image_size);
    RuntimeConfig runtime_config = {.verbosity = 0,
                                    .use_spinning = threading.spin};
    gemma.GenerateImageTokens(runtime_config, kv_cache.SeqLen(), image,
                              image_tokens, env);
  }

  // Get prompt
  std::string prompt_string;
  if (!inference.prompt.empty()) {
    prompt_string = inference.prompt;
  } else if (!inference.prompt_file.Empty()) {
    prompt_string = ReadFileToString(inference.prompt_file);
  } else {
    std::cerr << "Error: No prompt provided. Use --prompt or --prompt_file" << std::endl;
    return;
  }

  // Token streaming callback
  auto batch_stream_token = [&](size_t query_idx, size_t pos, int token, float) {
    std::string token_text;
    HWY_ASSERT(gemma.Tokenizer().Decode(std::vector<int>{token}, &token_text));

    ++abs_pos;
    const bool in_prompt = tokens_generated < prompt_size;
    ++tokens_generated;
    
    if (in_prompt) {
      return true;
    } else if (config.IsEOS(token)) {
      return true;
    }
    
    // Output token
    std::cout << token_text << std::flush;
    return true;
  };

  // Set up runtime config
  TimingInfo timing_info = {.verbosity = inference.verbosity};
  RuntimeConfig runtime_config = {.verbosity = 0,
                                  .batch_stream_token = batch_stream_token,
                                  .use_spinning = threading.spin};
  inference.CopyTo(runtime_config);

  // Tokenize prompt
  std::vector<int> prompt;
  size_t prefix_end = 0;
  
  if (have_image) {
    prompt = WrapAndTokenize(gemma.Tokenizer(), gemma.ChatTemplate(),
                           config.wrapping, abs_pos, prompt_string,
                           image_tokens.Rows());
    runtime_config.image_tokens = &image_tokens;
    prompt_size = prompt.size();
    if (config.wrapping == PromptWrapping::PALIGEMMA) {
      prefix_end = prompt_size;
      runtime_config.prefill_tbatch_size = prompt_size;
    }
  } else {
    prompt = WrapAndTokenize(gemma.Tokenizer(), gemma.ChatTemplate(),
                           config.wrapping, abs_pos, prompt_string);
    prompt_size = prompt.size();
  }

  // Generate response
  if (abs_pos > 0) --abs_pos;
  gemma.Generate(runtime_config, prompt, abs_pos, prefix_end, kv_cache, env,
                 timing_info);
  
  std::cout << std::endl;

  // Print timing info if verbose
  if (inference.verbosity >= 1) {
    std::cerr << "\nTokens generated: " << (tokens_generated - prompt_size) << std::endl;
    std::cerr << "Prompt tokens: " << prompt_size << std::endl;
  }
}

}  // namespace gcpp

int main(int argc, char** argv) {
  gcpp::InternalInit();

  gcpp::LoaderArgs loader(argc, argv);
  gcpp::ThreadingArgs threading(argc, argv);
  gcpp::InferenceArgs inference(argc, argv);

  if (gcpp::HasHelp(argc, argv)) {
    std::cout << "\nGemma CLI - Simple command-line interface\n";
    std::cout << "==========================================\n\n";
    std::cout << "Usage: " << argv[0] << " --weights <path> --tokenizer <path> --prompt <text>\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << argv[0] << " --weights model.sbs --tokenizer tokenizer.spm --prompt \"Hello, how are you?\"\n";
    std::cout << "  " << argv[0] << " --weights model.sbs --prompt \"Explain quantum computing\" --temperature 0.7\n\n";
    std::cout << "\n*Model Loading Arguments*\n\n";
    loader.Help();
    std::cout << "\n*Threading Arguments*\n\n";
    threading.Help();
    std::cout << "\n*Inference Arguments*\n\n";
    inference.Help();
    std::cout << "\n";
    return 0;
  }

  // Validate required arguments
  if (loader.weights.path.empty()) {
    std::cerr << "Error: --weights is required\n";
    std::cerr << "Use --help for usage information\n";
    return 1;
  }

  if (inference.prompt.empty() && inference.prompt_file.Empty()) {
    std::cerr << "Error: --prompt or --prompt_file is required\n";
    std::cerr << "Use --help for usage information\n";
    return 1;
  }

  gcpp::RunCLI(loader, threading, inference);

  PROFILER_PRINT_RESULTS();
  return 0;
}
