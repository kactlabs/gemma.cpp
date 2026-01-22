# Quick Start Guide - CLI Tools

Get started with gemma.cpp CLI tools in 5 minutes.

## Prerequisites

- CMake
- Clang C++ compiler (C++17 or later)
- Model weights from [Kaggle](https://www.kaggle.com/models/google/gemma-2/gemmaCpp)

## Step 1: Build

```bash
# Clone the repository (if you haven't already)
git clone https://github.com/google/gemma.cpp.git
cd gemma.cpp

# Configure and build
cmake -B build
cmake --build build -j$(nproc)
```

This creates the following executables in `build/`:
- `gemma-cli` - Simple CLI tool
- `gemma-server` - HTTP server
- `gemma` - Interactive terminal
- `gemma_api_server` - Google API compatible server
- `gemma_api_client` - API client

## Step 2: Download Model

Download a model from Kaggle (we recommend starting with `gemma2-2b-it-sfp`):

```bash
# Download from Kaggle and extract
tar -xf archive.tar.gz

# You should now have:
# - 2b-it-sfp.sbs (or similar model file)
# - tokenizer.spm
```

## Step 3: Choose Your Tool

### Option A: Simple CLI (gemma-cli)

Best for: Single prompts, scripting, batch processing

```bash
./build/gemma-cli \
  --weights 2b-it-sfp.sbs \
  --tokenizer tokenizer.spm \
  --prompt "Explain quantum computing in simple terms"
```

**Output:**
```
Quantum computing is a type of computing that uses quantum-mechanical phenomena...
```

### Option B: HTTP Server (gemma-server)

Best for: Web applications, API integration, multiple clients

**Start server:**
```bash
./build/gemma-server \
  --weights 2b-it-sfp.sbs \
  --tokenizer tokenizer.spm \
  --port 8080
```

**Use from another terminal:**
```bash
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt": "What is the capital of France?"}'
```

**Output:**
```json
{
  "text": "The capital of France is Paris.",
  "session_id": "session_abc123",
  "tokens_generated": 8,
  "prompt_tokens": 7
}
```

### Option C: Interactive Terminal (gemma)

Best for: Exploration, multi-turn conversations, development

```bash
./build/gemma \
  --weights 2b-it-sfp.sbs \
  --tokenizer tokenizer.spm
```

**Interactive session:**
```
> Hello, how are you?

I'm doing well, thank you for asking! How can I help you today?

> What's 2+2?

2 + 2 = 4

> %q
```

## Common Examples

### 1. Creative Writing

```bash
./build/gemma-cli \
  --weights 2b-it-sfp.sbs \
  --prompt "Write a haiku about programming" \
  --temperature 0.9
```

### 2. Code Generation

```bash
./build/gemma-cli \
  --weights 2b-it-sfp.sbs \
  --prompt "Write a Python function to calculate fibonacci numbers"
```

### 3. Question Answering

```bash
./build/gemma-cli \
  --weights 2b-it-sfp.sbs \
  --prompt "What are the main differences between Python and JavaScript?"
```

### 4. Multi-turn Conversation (Server)

```bash
# Terminal 1: Start server
./build/gemma-server --weights 2b-it-sfp.sbs --port 8080

# Terminal 2: First message
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt": "My favorite color is blue"}'
# Note the session_id in response

# Terminal 2: Follow-up (use same session_id)
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt": "What is my favorite color?", "session_id": "session_abc123"}'
```

### 5. Streaming Response

```bash
curl -X POST http://localhost:8080/completion/stream \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Tell me a short story about a robot"}'
```

### 6. Image Understanding (PaliGemma)

```bash
# Convert image to PPM format first
convert image.jpg -resize 224x224^ image.ppm

# Run with image
./build/gemma-cli \
  --weights paligemma2-3b-mix-224-sfp.sbs \
  --tokenizer paligemma_tokenizer.model \
  --image_file image.ppm \
  --prompt "Describe this image in detail"
```

## Configuration Options

### Temperature
Controls randomness (0.0 = deterministic, 2.0 = very random)

```bash
# More creative
./build/gemma-cli --weights model.sbs --prompt "Write a story" --temperature 1.2

# More focused
./build/gemma-cli --weights model.sbs --prompt "What is 2+2?" --temperature 0.1
```

### Max Tokens
Limit response length

```bash
./build/gemma-cli \
  --weights model.sbs \
  --prompt "Explain AI" \
  --max_generated_tokens 100
```

### Top-K Sampling
Limit vocabulary choices

```bash
./build/gemma-cli \
  --weights model.sbs \
  --prompt "Complete this: Once upon a" \
  --top_k 40
```

## Integration Examples

### Bash Script

```bash
#!/bin/bash
# ask_gemma.sh

WEIGHTS="2b-it-sfp.sbs"
TOKENIZER="tokenizer.spm"

./build/gemma-cli \
  --weights "$WEIGHTS" \
  --tokenizer "$TOKENIZER" \
  --prompt "$1" \
  --verbosity 0
```

Usage:
```bash
chmod +x ask_gemma.sh
./ask_gemma.sh "What is machine learning?"
```

### Python Integration

```python
#!/usr/bin/env python3
# gemma_client.py

import requests
import sys

def ask_gemma(prompt, temperature=0.7):
    response = requests.post('http://localhost:8080/completion',
        json={
            'prompt': prompt,
            'temperature': temperature,
            'max_tokens': 500
        }
    )
    return response.json()['text']

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python gemma_client.py 'your prompt'")
        sys.exit(1)
    
    result = ask_gemma(sys.argv[1])
    print(result)
```

Usage:
```bash
# Start server first
./build/gemma-server --weights 2b-it-sfp.sbs --port 8080

# Use from Python
python gemma_client.py "Explain neural networks"
```

### Node.js Integration

```javascript
// gemma_client.js
const axios = require('axios');

async function askGemma(prompt, temperature = 0.7) {
  const response = await axios.post('http://localhost:8080/completion', {
    prompt: prompt,
    temperature: temperature,
    max_tokens: 500
  });
  return response.data.text;
}

// Usage
askGemma('What is TypeScript?')
  .then(result => console.log(result))
  .catch(err => console.error(err));
```

## Next Steps

- Read [CLI_README.md](CLI_README.md) for detailed documentation
- Check [API_SERVER_README.md](API_SERVER_README.md) for Google API format
- See [README.md](README.md) for model information and advanced usage
- Join the [Discord community](https://discord.gg/H5jCBAWxAe)

## Troubleshooting

**Build fails:**
- Ensure you have CMake 3.11+ and Clang with C++17 support
- Try `rm -rf build/*` and rebuild

**Model not loading:**
- Verify file paths are correct
- Check you have enough RAM (2B model needs ~4GB)
- Ensure model file is not corrupted

**Slow performance:**
- Use `-sfp` models (8-bit) instead of `bf16`
- Ensure laptop is plugged in
- Close other applications
- Try running a few queries (auto-tuning improves speed)

**Server won't start:**
- Check if port is already in use: `lsof -i :8080`
- Try a different port: `--port 8081`

**Strange output:**
- Make sure you're using instruction-tuned (`-it`) models
- Check temperature isn't too high
- Verify tokenizer matches the model

## Getting Help

- GitHub Issues: https://github.com/google/gemma.cpp/issues
- Discord: https://discord.gg/H5jCBAWxAe
- Documentation: See README files in the repository

---

Happy coding! 🚀
