#!/usr/bin/env python3
"""
Example Python client for gemma-server

Usage:
    # Start server first:
    ./build/gemma-server --weights model.sbs --tokenizer tokenizer.spm --port 8080
    
    # Then run this script:
    python examples/python_client.py
"""

import requests
import json
import sys
from typing import Optional, Iterator

class GemmaClient:
    """Simple client for gemma-server"""
    
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.base_url = f"http://{host}:{port}"
    
    def health(self) -> dict:
        """Check server health"""
        response = requests.get(f"{self.base_url}/health")
        response.raise_for_status()
        return response.json()
    
    def complete(
        self,
        prompt: str,
        temperature: float = 0.7,
        max_tokens: int = 500,
        top_k: int = 1,
        session_id: Optional[str] = None
    ) -> dict:
        """Generate a completion (non-streaming)"""
        payload = {
            "prompt": prompt,
            "temperature": temperature,
            "max_tokens": max_tokens,
            "top_k": top_k
        }
        if session_id:
            payload["session_id"] = session_id
        
        response = requests.post(
            f"{self.base_url}/completion",
            json=payload
        )
        response.raise_for_status()
        return response.json()
    
    def complete_stream(
        self,
        prompt: str,
        temperature: float = 0.7,
        max_tokens: int = 500,
        top_k: int = 1,
        session_id: Optional[str] = None
    ) -> Iterator[dict]:
        """Generate a completion with streaming"""
        payload = {
            "prompt": prompt,
            "temperature": temperature,
            "max_tokens": max_tokens,
            "top_k": top_k
        }
        if session_id:
            payload["session_id"] = session_id
        
        response = requests.post(
            f"{self.base_url}/completion/stream",
            json=payload,
            stream=True
        )
        response.raise_for_status()
        
        for line in response.iter_lines():
            if line:
                line = line.decode('utf-8')
                if line.startswith('data: '):
                    data = json.loads(line[6:])
                    yield data


def example_basic():
    """Basic completion example"""
    print("=" * 50)
    print("Example 1: Basic Completion")
    print("=" * 50)
    
    client = GemmaClient()
    
    # Check health
    health = client.health()
    print(f"Server status: {health['status']}")
    print()
    
    # Generate completion
    result = client.complete(
        prompt="What is the capital of France?",
        temperature=0.1
    )
    
    print(f"Prompt: What is the capital of France?")
    print(f"Response: {result['text']}")
    print(f"Tokens generated: {result['tokens_generated']}")
    print(f"Session ID: {result['session_id']}")
    print()


def example_streaming():
    """Streaming completion example"""
    print("=" * 50)
    print("Example 2: Streaming Completion")
    print("=" * 50)
    
    client = GemmaClient()
    
    prompt = "Write a short haiku about programming"
    print(f"Prompt: {prompt}")
    print("Response: ", end="", flush=True)
    
    for event in client.complete_stream(prompt, temperature=0.9):
        if 'text' in event:
            print(event['text'], end="", flush=True)
        elif event.get('done'):
            print()
            print(f"\nTokens generated: {event['tokens_generated']}")
            print(f"Session ID: {event['session_id']}")
    print()


def example_conversation():
    """Multi-turn conversation example"""
    print("=" * 50)
    print("Example 3: Multi-turn Conversation")
    print("=" * 50)
    
    client = GemmaClient()
    
    # First message
    result1 = client.complete(
        prompt="My favorite color is blue",
        temperature=0.7
    )
    session_id = result1['session_id']
    
    print("User: My favorite color is blue")
    print(f"Assistant: {result1['text']}")
    print()
    
    # Follow-up message using same session
    result2 = client.complete(
        prompt="What is my favorite color?",
        temperature=0.7,
        session_id=session_id
    )
    
    print("User: What is my favorite color?")
    print(f"Assistant: {result2['text']}")
    print()


def example_creative_writing():
    """Creative writing with higher temperature"""
    print("=" * 50)
    print("Example 4: Creative Writing")
    print("=" * 50)
    
    client = GemmaClient()
    
    result = client.complete(
        prompt="Write a creative opening line for a sci-fi story",
        temperature=1.2,
        max_tokens=100
    )
    
    print("Prompt: Write a creative opening line for a sci-fi story")
    print(f"Response: {result['text']}")
    print()


def example_code_generation():
    """Code generation example"""
    print("=" * 50)
    print("Example 5: Code Generation")
    print("=" * 50)
    
    client = GemmaClient()
    
    result = client.complete(
        prompt="Write a Python function to calculate the factorial of a number",
        temperature=0.3,
        max_tokens=300
    )
    
    print("Prompt: Write a Python function to calculate the factorial of a number")
    print("Response:")
    print(result['text'])
    print()


def interactive_mode():
    """Interactive chat mode"""
    print("=" * 50)
    print("Interactive Mode")
    print("=" * 50)
    print("Type 'quit' to exit")
    print()
    
    client = GemmaClient()
    session_id = None
    
    while True:
        try:
            prompt = input("You: ").strip()
            if not prompt:
                continue
            if prompt.lower() in ['quit', 'exit', 'q']:
                break
            
            print("Assistant: ", end="", flush=True)
            
            for event in client.complete_stream(
                prompt,
                temperature=0.7,
                session_id=session_id
            ):
                if 'text' in event:
                    print(event['text'], end="", flush=True)
                elif event.get('done'):
                    session_id = event['session_id']
                    print()
            
            print()
            
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"\nError: {e}")


def main():
    """Run all examples"""
    if len(sys.argv) > 1 and sys.argv[1] == "--interactive":
        interactive_mode()
        return
    
    try:
        example_basic()
        example_streaming()
        example_conversation()
        example_creative_writing()
        example_code_generation()
        
        print("=" * 50)
        print("All examples completed!")
        print("=" * 50)
        print()
        print("Run with --interactive for interactive mode:")
        print("  python examples/python_client.py --interactive")
        
    except requests.exceptions.ConnectionError:
        print("Error: Could not connect to server")
        print("Make sure gemma-server is running:")
        print("  ./build/gemma-server --weights model.sbs --tokenizer tokenizer.spm --port 8080")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
