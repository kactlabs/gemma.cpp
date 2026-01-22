# CLI Tools Comparison

This document compares the different CLI tools available in gemma.cpp to help you choose the right one for your use case.

## Quick Comparison Table

| Feature | gemma-cli | gemma-server | gemma | gemma_api_server |
|---------|-----------|--------------|-------|------------------|
| **Use Case** | Single prompts | HTTP API | Interactive chat | Google API compatible |
| **Interface** | Command line | REST API | Terminal UI | REST API (Google format) |
| **Multi-turn** | ❌ No | ✅ Yes (sessions) | ✅ Yes | ✅ Yes (sessions) |
| **Streaming** | ❌ No | ✅ Yes (SSE) | ✅ Yes | ✅ Yes (SSE) |
| **Scripting** | ✅ Excellent | ✅ Good | ❌ Limited | ✅ Good |
| **Web Integration** | ❌ No | ✅ Yes | ❌ No | ✅ Yes |
| **Complexity** | Simple | Medium | Simple | Medium |
| **Session Management** | ❌ No | ✅ Yes | ✅ Built-in | ✅ Yes |
| **API Format** | N/A | Simple JSON | N/A | Google API |

## Detailed Comparison

### 1. gemma-cli

**Best for:** Quick tests, scripting, batch processing, CI/CD pipelines

**Pros:**
- Simplest to use
- Perfect for shell scripts
- Fast startup
- Direct output to stdout
- No server overhead

**Cons:**
- No multi-turn conversations
- No streaming
- New process for each prompt
- No session management

**Example:**
```bash
./build/gemma-cli --weights model.sbs --prompt "Hello!"
```

**When to use:**
- Quick one-off queries
- Batch processing multiple prompts
- Shell scripts and automation
- CI/CD pipelines
- Testing model outputs

---

### 2. gemma-server

**Best for:** Web applications, API integration, multiple clients

**Pros:**
- Simple REST API
- Streaming support (SSE)
- Session management
- Multiple concurrent clients
- Easy to integrate
- OpenAI-compatible endpoint

**Cons:**
- Requires server setup
- More complex than CLI
- Network overhead

**Example:**
```bash
# Start server
./build/gemma-server --weights model.sbs --port 8080

# Use from anywhere
curl -X POST http://localhost:8080/completion \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Hello!"}'
```

**When to use:**
- Web applications
- Mobile app backends
- Microservices
- Multiple clients
- Long-running service
- API integration

---

### 3. gemma (Interactive Terminal)

**Best for:** Exploration, development, testing, conversations

**Pros:**
- Interactive interface
- Multi-turn conversations
- Built-in session management
- Rich terminal UI
- Special commands (%q, %c)
- Streaming output

**Cons:**
- Not suitable for automation
- Terminal-only
- Manual interaction required

**Example:**
```bash
./build/gemma --weights model.sbs --tokenizer tokenizer.spm
> Hello, how are you?
I'm doing well, thank you!
> %q
```

**When to use:**
- Exploring model capabilities
- Development and testing
- Interactive conversations
- Debugging prompts
- Learning and experimentation

---

### 4. gemma_api_server

**Best for:** Google API compatibility, existing Google API clients

**Pros:**
- Google API format
- Compatible with Google API clients
- Streaming support (SSE)
- Session management
- Well-documented API format

**Cons:**
- More complex API format
- Requires understanding of Google API
- Heavier JSON payloads

**Example:**
```bash
# Start server
./build/gemma_api_server --weights model.sbs --port 8080

# Use Google API format
curl -X POST http://localhost:8080/v1beta/models/gemma3-4b:generateContent \
  -H "Content-Type: application/json" \
  -d '{
    "contents": [{"parts": [{"text": "Hello!"}]}],
    "generationConfig": {"temperature": 0.9}
  }'
```

**When to use:**
- Migrating from Google API
- Using Google API client libraries
- Need Google API compatibility
- Existing Google API integrations

---

## Use Case Recommendations

### Scripting and Automation
**Recommended:** `gemma-cli`

```bash
#!/bin/bash
for file in *.txt; do
  ./build/gemma-cli --weights model.sbs --prompt "Summarize: $(cat $file)"
done
```

### Web Application Backend
**Recommended:** `gemma-server`

```python
import requests

def get_completion(prompt):
    response = requests.post('http://localhost:8080/completion',
        json={'prompt': prompt})
    return response.json()['text']
```

### Interactive Development
**Recommended:** `gemma`

```bash
./build/gemma --weights model.sbs --tokenizer tokenizer.spm
```

### Google API Migration
**Recommended:** `gemma_api_server`

```python
# Use existing Google API client code
from google.generativeai import GenerativeModel

# Point to local server instead
model = GenerativeModel('gemma3-4b', 
    api_endpoint='http://localhost:8080')
```

### Real-time Chat Application
**Recommended:** `gemma-server` (streaming)

```javascript
const response = await fetch('http://localhost:8080/completion/stream', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({prompt: userInput})
});

for await (const chunk of response.body) {
  // Display tokens as they arrive
  displayToken(chunk);
}
```

### Batch Processing
**Recommended:** `gemma-cli`

```bash
# Process 1000 prompts
cat prompts.txt | while read prompt; do
  ./build/gemma-cli --weights model.sbs --prompt "$prompt" >> results.txt
done
```

### Microservice
**Recommended:** `gemma-server`

```yaml
# docker-compose.yml
services:
  gemma:
    image: gemma-server
    ports:
      - "8080:8080"
    command: --weights /models/model.sbs --port 8080
```

---

## Performance Comparison

| Tool | Startup Time | Memory Usage | Throughput | Latency |
|------|--------------|--------------|------------|---------|
| gemma-cli | Fast (per request) | Low (per request) | Low | Low |
| gemma-server | Slow (once) | High (persistent) | High | Medium |
| gemma | Fast | Medium | N/A | Low |
| gemma_api_server | Slow (once) | High (persistent) | High | Medium |

**Notes:**
- CLI tools start fresh for each request (fast startup, but no session reuse)
- Server tools keep model in memory (slow startup, but fast subsequent requests)
- Server tools can handle multiple concurrent requests
- Interactive tool is optimized for single-user experience

---

## API Format Comparison

### gemma-server (Simple Format)

**Request:**
```json
{
  "prompt": "Hello!",
  "temperature": 0.7,
  "max_tokens": 100
}
```

**Response:**
```json
{
  "text": "Hello! How can I help you?",
  "session_id": "session_123",
  "tokens_generated": 6
}
```

### gemma_api_server (Google API Format)

**Request:**
```json
{
  "contents": [
    {"parts": [{"text": "Hello!"}], "role": "user"}
  ],
  "generationConfig": {
    "temperature": 0.7,
    "maxOutputTokens": 100
  }
}
```

**Response:**
```json
{
  "candidates": [{
    "content": {
      "parts": [{"text": "Hello! How can I help you?"}],
      "role": "model"
    },
    "finishReason": "STOP"
  }],
  "usageMetadata": {
    "promptTokenCount": 2,
    "candidatesTokenCount": 6
  }
}
```

---

## Migration Guide

### From llama.cpp

| llama.cpp | gemma.cpp | Notes |
|-----------|-----------|-------|
| `llama-cli` | `gemma-cli` | Similar interface, different args |
| `llama-server` | `gemma-server` | Different API format |
| `llama-server` | `gemma_api_server` | For Google API format |

### From Python/Transformers

```python
# Before (Transformers)
from transformers import AutoModelForCausalLM, AutoTokenizer

model = AutoModelForCausalLM.from_pretrained("google/gemma-2b")
tokenizer = AutoTokenizer.from_pretrained("google/gemma-2b")

# After (gemma.cpp server)
import requests

def generate(prompt):
    response = requests.post('http://localhost:8080/completion',
        json={'prompt': prompt})
    return response.json()['text']
```

### From Google API

```python
# Before (Public Google API)
import google.generativeai as genai
genai.configure(api_key="YOUR_API_KEY")
model = genai.GenerativeModel('gemini-pro')

# After (gemma_api_server)
# Same code, just point to local server
genai.configure(api_endpoint='http://localhost:8080')
model = genai.GenerativeModel('gemma3-4b')
```

---

## Choosing the Right Tool

**Ask yourself:**

1. **Do I need a persistent service?**
   - Yes → `gemma-server` or `gemma_api_server`
   - No → `gemma-cli` or `gemma`

2. **Do I need multi-turn conversations?**
   - Yes → `gemma-server`, `gemma_api_server`, or `gemma`
   - No → `gemma-cli`

3. **Do I need web/API integration?**
   - Yes → `gemma-server` or `gemma_api_server`
   - No → `gemma-cli` or `gemma`

4. **Do I need Google API compatibility?**
   - Yes → `gemma_api_server`
   - No → `gemma-server`

5. **Is this for interactive use?**
   - Yes → `gemma`
   - No → Other tools

6. **Is this for automation/scripting?**
   - Yes → `gemma-cli`
   - No → Other tools

---

## Summary

- **gemma-cli**: Simple, fast, perfect for scripting
- **gemma-server**: Flexible HTTP API, great for applications
- **gemma**: Interactive terminal, best for exploration
- **gemma_api_server**: Google API compatible, for migrations

Choose based on your specific needs and integration requirements!
