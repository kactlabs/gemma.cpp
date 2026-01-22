#!/usr/bin/env node
/**
 * Example Node.js client for gemma-server
 * 
 * Usage:
 *   # Start server first:
 *   ./build/gemma-server --weights model.sbs --tokenizer tokenizer.spm --port 8080
 *   
 *   # Install dependencies:
 *   npm install axios
 *   
 *   # Then run this script:
 *   node examples/nodejs_client.js
 */

const axios = require('axios');
const readline = require('readline');

class GemmaClient {
  constructor(host = 'localhost', port = 8080) {
    this.baseUrl = `http://${host}:${port}`;
  }

  async health() {
    const response = await axios.get(`${this.baseUrl}/health`);
    return response.data;
  }

  async complete(prompt, options = {}) {
    const {
      temperature = 0.7,
      maxTokens = 500,
      topK = 1,
      sessionId = null
    } = options;

    const payload = {
      prompt,
      temperature,
      max_tokens: maxTokens,
      top_k: topK
    };

    if (sessionId) {
      payload.session_id = sessionId;
    }

    const response = await axios.post(`${this.baseUrl}/completion`, payload);
    return response.data;
  }

  async *completeStream(prompt, options = {}) {
    const {
      temperature = 0.7,
      maxTokens = 500,
      topK = 1,
      sessionId = null
    } = options;

    const payload = {
      prompt,
      temperature,
      max_tokens: maxTokens,
      top_k: topK
    };

    if (sessionId) {
      payload.session_id = sessionId;
    }

    const response = await axios.post(
      `${this.baseUrl}/completion/stream`,
      payload,
      { responseType: 'stream' }
    );

    let buffer = '';

    for await (const chunk of response.data) {
      buffer += chunk.toString();
      const lines = buffer.split('\n');
      buffer = lines.pop();

      for (const line of lines) {
        if (line.startsWith('data: ')) {
          const data = JSON.parse(line.slice(6));
          yield data;
        }
      }
    }
  }
}

async function exampleBasic() {
  console.log('='.repeat(50));
  console.log('Example 1: Basic Completion');
  console.log('='.repeat(50));

  const client = new GemmaClient();

  // Check health
  const health = await client.health();
  console.log(`Server status: ${health.status}`);
  console.log();

  // Generate completion
  const result = await client.complete(
    'What is the capital of France?',
    { temperature: 0.1 }
  );

  console.log('Prompt: What is the capital of France?');
  console.log(`Response: ${result.text}`);
  console.log(`Tokens generated: ${result.tokens_generated}`);
  console.log(`Session ID: ${result.session_id}`);
  console.log();
}

async function exampleStreaming() {
  console.log('='.repeat(50));
  console.log('Example 2: Streaming Completion');
  console.log('='.repeat(50));

  const client = new GemmaClient();

  const prompt = 'Write a short haiku about programming';
  console.log(`Prompt: ${prompt}`);
  process.stdout.write('Response: ');

  for await (const event of client.completeStream(prompt, { temperature: 0.9 })) {
    if (event.text) {
      process.stdout.write(event.text);
    } else if (event.done) {
      console.log();
      console.log(`\nTokens generated: ${event.tokens_generated}`);
      console.log(`Session ID: ${event.session_id}`);
    }
  }
  console.log();
}

async function exampleConversation() {
  console.log('='.repeat(50));
  console.log('Example 3: Multi-turn Conversation');
  console.log('='.repeat(50));

  const client = new GemmaClient();

  // First message
  const result1 = await client.complete(
    'My favorite color is blue',
    { temperature: 0.7 }
  );
  const sessionId = result1.session_id;

  console.log('User: My favorite color is blue');
  console.log(`Assistant: ${result1.text}`);
  console.log();

  // Follow-up message using same session
  const result2 = await client.complete(
    'What is my favorite color?',
    { temperature: 0.7, sessionId }
  );

  console.log('User: What is my favorite color?');
  console.log(`Assistant: ${result2.text}`);
  console.log();
}

async function exampleCreativeWriting() {
  console.log('='.repeat(50));
  console.log('Example 4: Creative Writing');
  console.log('='.repeat(50));

  const client = new GemmaClient();

  const result = await client.complete(
    'Write a creative opening line for a sci-fi story',
    { temperature: 1.2, maxTokens: 100 }
  );

  console.log('Prompt: Write a creative opening line for a sci-fi story');
  console.log(`Response: ${result.text}`);
  console.log();
}

async function exampleCodeGeneration() {
  console.log('='.repeat(50));
  console.log('Example 5: Code Generation');
  console.log('='.repeat(50));

  const client = new GemmaClient();

  const result = await client.complete(
    'Write a JavaScript function to calculate the factorial of a number',
    { temperature: 0.3, maxTokens: 300 }
  );

  console.log('Prompt: Write a JavaScript function to calculate the factorial of a number');
  console.log('Response:');
  console.log(result.text);
  console.log();
}

async function interactiveMode() {
  console.log('='.repeat(50));
  console.log('Interactive Mode');
  console.log('='.repeat(50));
  console.log("Type 'quit' to exit");
  console.log();

  const client = new GemmaClient();
  let sessionId = null;

  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

  const question = (prompt) => new Promise((resolve) => {
    rl.question(prompt, resolve);
  });

  while (true) {
    try {
      const prompt = await question('You: ');
      
      if (!prompt.trim()) continue;
      if (['quit', 'exit', 'q'].includes(prompt.toLowerCase())) {
        break;
      }

      process.stdout.write('Assistant: ');

      for await (const event of client.completeStream(
        prompt,
        { temperature: 0.7, sessionId }
      )) {
        if (event.text) {
          process.stdout.write(event.text);
        } else if (event.done) {
          sessionId = event.session_id;
          console.log();
        }
      }

      console.log();

    } catch (error) {
      console.error(`\nError: ${error.message}`);
    }
  }

  rl.close();
}

async function main() {
  const args = process.argv.slice(2);

  if (args.includes('--interactive')) {
    await interactiveMode();
    return;
  }

  try {
    await exampleBasic();
    await exampleStreaming();
    await exampleConversation();
    await exampleCreativeWriting();
    await exampleCodeGeneration();

    console.log('='.repeat(50));
    console.log('All examples completed!');
    console.log('='.repeat(50));
    console.log();
    console.log('Run with --interactive for interactive mode:');
    console.log('  node examples/nodejs_client.js --interactive');

  } catch (error) {
    if (error.code === 'ECONNREFUSED') {
      console.error('Error: Could not connect to server');
      console.error('Make sure gemma-server is running:');
      console.error('  ./build/gemma-server --weights model.sbs --tokenizer tokenizer.spm --port 8080');
      process.exit(1);
    } else {
      console.error(`Error: ${error.message}`);
      process.exit(1);
    }
  }
}

if (require.main === module) {
  main();
}

module.exports = { GemmaClient };
