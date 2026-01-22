# Gemma.cpp CLI Tools

This document describes the command-line tools available in gemma.cpp, similar to llama.cpp's llama-cli and llama-server.

## Available Tools

### 1. gemma-cli - Command Line Interface

A simple CLI tool for running Gemma models locally with single prompts.

#### Usage

```bash
./build/gemma-cli \
  --weights path/to/model.sbs \
  --tokenizer path/to/tokenizer.spm \
  --prompt "Your prompt here"
```

#### Examples

**Basic completion:**
```bash
./build/gemma-cli \
  --weights gemma2-2b-it-sfp.sbs \
  --tokenizer tokenizer.spm \
  --prompt "Explain quantum computing in simple terms"
```

**With custom parameters:**
```bash
./build/gemma-cli \
  --weights gemma2-2b-it-sfp.sbs \
  --tokenizer tokenizer.spm \
  --prompt "Write a haiku about programming" \
  --temperature 0.9 \
  --max_generated_tokens 100
```

**From file:**
```bash
./build/gemma-cli \
  --weights gemma2-2b-it-sfp.sbs \
  --tokenizer tokenizer.spm \
  --prompt_file my_prompt.txt
```

**PaliGemma with image:**
```bash
./build/gemma-cli \
  --weights paligemma2-3b-mix-224-sfp.sbs \
  --tokenizer paligemma_tokenizer.model \
  --image_file image.ppm \
  --prompt "Describe this image"
```

#### Key Arguments

- `--weights` - Path to model weights file (.sbs) **[Required]**
- `--tokenizer` - Path to tokenizer file (.spm) [Optional if using single-file format]
- `--prompt` - Text prompt **[Required if --prompt_file not used]**
- `--prompt_file` - Path to file containing prompt
- `--temperature` - Sampling temperature (default: 1.0)
- `--top_k` - Top-K sampling (default: 1)
- `--max_generated_tokens` - Maximum tokens to generate (default: 2048)
- `--verbosity` - Output verbosity level (0-2, default: 0)

---

### 2. gemma-server - HTTP Server

A simple HTTP server for running Gemma models with a REST API.

#### Usage

```bash
./build/gemma-server \
  --weights path/to/model.sbs \
  --tokenizer path/to/tokenizer.spm \
  --port 8080
```

#### Endpoints

##### POST /completion
Generate a completion (non-streaming).

**Request:**
```json
{
  "prompt": "Hello, how are you?",
  "temperature": 0.7,
  "top_k": 1,
  "max_tokens": 1024,
  "session_id": "optional-session-id"
}
```

**Response:**
```json
{
  "text": "I'm doing well, thank you for asking!",
  "session_id": "session_abc123",
  "tokens_generated": 10,
  "prompt_tokens": 5
}
```

##### POST /completion/stream
Generate a completion with streaming (Server-Sent Events).

**Request:** Same as `/completion`

**Response:** Stream of SSE events:
```
data: {"text":"I'm"}

data: {"text":" doing"}

data: {"text":" well"}

data: {"done":true,"session_id":"session_abc123","tokens_generated":10}
```

##### POST /v1/completions
OpenAI-compatible completion endpoint (same format as `/completion`).

##### GET /health
Health check endpoint.

**Response:**
```json
{
  "status": "ok",
  "model": "gemma"
}
```

#### Examples

**Start server:**
```bash
./build/gemma-server \
  --weights gemma2-2b-it-sfp.sbs \
  --tokenizer tokenizer.spm \
  --port 8080
```

**Test with curl:**
```bash
# Non-streaming
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "What is the capital of France?",
    "temperature": 0.7,
    "max_tokens": 100
  }'

# Streaming
curl -X POST http://localhost:8080/completion/stream \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "Tell me a short story",
    "temperature": 0.9
  }'

# Health check
curl http://localhost:8080/health
```

**Multi-turn conversation (using session_id):**
```bash
# First message
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt": "My name is Alice"}'

# Response includes: "session_id": "session_abc123"

# Follow-up message with same session
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "What is my name?",
    "session_id": "session_abc123"
  }'
```

**Python example:**
```python
import requests

response = requests.post('http://localhost:8080/completion',
  json={
    'prompt': 'Explain machine learning',
    'temperature': 0.7,
    'max_tokens': 500
  }
)

result = response.json()
print(result['text'])
```

**Streaming with Python:**
```python
import requests
import json

response = requests.post('http://localhost:8080/completion/stream',
  json={'prompt': 'Write a poem about AI'},
  stream=True
)

for line in response.iter_lines():
  if line:
    line = line.decode('utf-8')
    if line.startswith('data: '):
      data = json.loads(line[6:])
      if 'text' in data:
        print(data['text'], end='', flush=True)
      elif data.get('done'):
        print(f"\n\nGenerated {data['tokens_generated']} tokens")
```

#### Key Arguments

- `--weights` - Path to model weights file (.sbs) **[Required]**
- `--tokenizer` - Path to tokenizer file (.spm) [Optional if using single-file format]
- `--port` - Server port (default: 8080)
- `--model` - Model name (default: gemma3-4b)

---

### 3. gemma_api_server - Google API Compatible Server

An HTTP server implementing the Google Generative AI API format.

#### Usage

```bash
./build/gemma_api_server \
  --weights path/to/model.sbs \
  --tokenizer path/to/tokenizer.spm \
  --port 8080 \
  --model gemma3-4b
```

See [API_SERVER_README.md](API_SERVER_README.md) for detailed documentation.

---

### 4. gemma_api_client - API Client

A unified client for testing both local and public Google APIs.

#### Usage

**Local server:**
```bash
./build/gemma_api_client \
  --host localhost \
  --port 8080 \
  --interactive 1
```

**Public Google API:**
```bash
export GOOGLE_API_KEY="your-api-key"
./build/gemma_api_client --interactive 1
```

See [API_SERVER_README.md](API_SERVER_README.md) for detailed documentation.

---

### 5. gemma - Interactive Terminal (Original)

The original interactive terminal interface with full features.

#### Usage

```bash
./build/gemma \
  --weights path/to/model.sbs \
  --tokenizer path/to/tokenizer.spm
```

This provides an interactive chat interface with conversation history and special commands:
- `%q` or `%Q` - Quit
- `%c` or `%C` - Clear conversation history

---

## Building

Build all CLI tools:

```bash
# Configure
cmake -B build

# Build all tools
cmake --build build -j$(nproc)

# Or build specific tools
cmake --build build --target gemma-cli -j$(nproc)
cmake --build build --target gemma-server -j$(nproc)
cmake --build build --target gemma_api_server -j$(nproc)
cmake --build build --target gemma_api_client -j$(nproc)
```

The executables will be in the `build/` directory:
- `build/gemma-cli`
- `build/gemma-server`
- `build/gemma_api_server`
- `build/gemma_api_client`
- `build/gemma`

---

## Comparison with llama.cpp

| llama.cpp | gemma.cpp | Description |
|-----------|-----------|-------------|
| `llama-cli` | `gemma-cli` | Simple CLI for single prompts |
| `llama-server` | `gemma-server` | HTTP server with simple API |
| `llama-server` | `gemma_api_server` | HTTP server with Google API format |
| - | `gemma` | Interactive terminal interface |
| - | `gemma_api_client` | API client for testing |

---

## Common Use Cases

### 1. Quick Testing
```bash
./build/gemma-cli --weights model.sbs --prompt "Hello!"
```

### 2. Integration with Applications
```bash
# Start server
./build/gemma-server --weights model.sbs --port 8080

# Use from any application
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Your prompt"}'
```

### 3. Interactive Development
```bash
./build/gemma --weights model.sbs --tokenizer tokenizer.spm
```

### 4. Batch Processing
```bash
# Process multiple prompts
for prompt in "Question 1" "Question 2" "Question 3"; do
  ./build/gemma-cli --weights model.sbs --prompt "$prompt"
done
```

### 5. API-Compatible Integration
```bash
# Start Google API compatible server
./build/gemma_api_server --weights model.sbs --port 8080

# Use with Google API client libraries
# (See API_SERVER_README.md for examples)
```

---

## Performance Tips

1. **Use SFP models** - 8-bit switched floating point models (`-sfp`) are faster
2. **Adjust threading** - Use `--num_threads` to match your CPU cores
3. **Power mode** - Ensure laptop is plugged in and performance mode is enabled
4. **Warm-up** - Second and third queries are faster due to auto-tuning
5. **Batch size** - Adjust `--prefill_tbatch_size` for your memory constraints

---

## Troubleshooting

**"Error: --weights is required"**
- Make sure to provide the path to your model weights file

**"Failed to start server on port 8080"**
- Port might be in use, try a different port with `--port 8081`

**Model produces strange output**
- Make sure you're using an instruction-tuned model (`-it`), not pre-trained (`-pt`)
- Check that tokenizer matches the model

**Slow performance**
- Use `-sfp` models instead of `bf16`
- Check CPU power mode settings
- Close other CPU-intensive applications

**Connection refused (client)**
- Make sure the server is running
- Check the host and port are correct
- Verify firewall settings

---

## License

Copyright 2024 Google LLC. Licensed under the Apache License, Version 2.0.
