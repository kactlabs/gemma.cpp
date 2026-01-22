#!/bin/bash
# Example usage of gemma.cpp CLI tools

# Set your model paths here
WEIGHTS="2b-it-sfp.sbs"
TOKENIZER="tokenizer.spm"
PORT=8080

echo "==================================="
echo "Gemma.cpp CLI Tools Examples"
echo "==================================="
echo ""

# Check if model files exist
if [ ! -f "$WEIGHTS" ]; then
    echo "Error: Model weights not found at $WEIGHTS"
    echo "Please download from Kaggle and update the WEIGHTS variable"
    exit 1
fi

if [ ! -f "$TOKENIZER" ]; then
    echo "Error: Tokenizer not found at $TOKENIZER"
    echo "Please download from Kaggle and update the TOKENIZER variable"
    exit 1
fi

# Example 1: Simple CLI usage
echo "Example 1: Simple CLI"
echo "---------------------"
./build/gemma-cli \
  --weights "$WEIGHTS" \
  --tokenizer "$TOKENIZER" \
  --prompt "What is the capital of France?" \
  --verbosity 0

echo ""
echo ""

# Example 2: CLI with custom parameters
echo "Example 2: CLI with custom temperature"
echo "---------------------------------------"
./build/gemma-cli \
  --weights "$WEIGHTS" \
  --tokenizer "$TOKENIZER" \
  --prompt "Write a creative haiku about coding" \
  --temperature 1.2 \
  --max_generated_tokens 50 \
  --verbosity 0

echo ""
echo ""

# Example 3: Server mode (background)
echo "Example 3: Starting HTTP Server"
echo "--------------------------------"
echo "Starting server on port $PORT..."

# Start server in background
./build/gemma-server \
  --weights "$WEIGHTS" \
  --tokenizer "$TOKENIZER" \
  --port "$PORT" &

SERVER_PID=$!
echo "Server started with PID: $SERVER_PID"

# Wait for server to start
sleep 3

# Test health endpoint
echo ""
echo "Testing health endpoint..."
curl -s http://localhost:$PORT/health | python3 -m json.tool

echo ""
echo ""

# Test completion endpoint
echo "Testing completion endpoint..."
curl -s -X POST http://localhost:$PORT/completion \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "What is 2+2?",
    "temperature": 0.1,
    "max_tokens": 50
  }' | python3 -m json.tool

echo ""
echo ""

# Test streaming endpoint
echo "Testing streaming endpoint..."
curl -X POST http://localhost:$PORT/completion/stream \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "Count from 1 to 5",
    "temperature": 0.1,
    "max_tokens": 50
  }'

echo ""
echo ""

# Cleanup
echo "Stopping server (PID: $SERVER_PID)..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "==================================="
echo "Examples completed!"
echo "==================================="
echo ""
echo "For more examples, see:"
echo "  - CLI_README.md"
echo "  - QUICKSTART_CLI.md"
echo "  - API_SERVER_README.md"
