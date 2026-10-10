#!/bin/bash

# Ensure we exit on failure
set -e

if [ ! -f TASK_SPEC.md ]; then
  echo "Error: TASK_SPEC.md not found"
  exit 1
fi

PROMPT=$(cat TASK_SPEC.md)
MODEL=$(jq -r .model opencode.json)

if [ -z "$OPENROUTER_API_KEY" ]; then
  echo "Error: OPENROUTER_API_KEY is not set in environment"
  exit 1
fi

echo "🚀 Bypassing opencode CLI..."
echo "🤖 Routing task directly to OpenRouter ($MODEL)..."

# Construct JSON payload using jq to safely escape the prompt
PAYLOAD=$(jq -n \
  --arg model "$MODEL" \
  --arg system_prompt "You are an elite C++ systems engineer. You are operating inside a headless Docker container. Your ONLY way to create files is to output raw, executable bash commands. Implement the user's task strictly adhering to their rules (C++20, RAII, zero leaks). Use 'mkdir -p' to create directories. Use 'cat << '\''EOF'\'' > path/to/file' to write code to files. DO NOT wrap your response in markdown code blocks like \`\`\`bash. Output ONLY raw bash text that can be piped directly into bash." \
  --arg user_prompt "$PROMPT" \
  '{
    model: $model,
    messages: [
      {role: "system", content: $system_prompt},
      {role: "user", content: $user_prompt}
    ]
  }')

# Call OpenRouter API
RESPONSE=$(curl -s -X POST https://openrouter.ai/api/v1/chat/completions \
  -H "Authorization: Bearer $OPENROUTER_API_KEY" \
  -H "Content-Type: application/json" \
  -d "$PAYLOAD")

# Check if the API returned an error
if echo "$RESPONSE" | jq -e '.error' > /dev/null; then
  echo "❌ OpenRouter API Error:"
  echo "$RESPONSE" | jq '.error'
  exit 1
fi

# Extract the response content
CODE=$(echo "$RESPONSE" | jq -r '.choices[0].message.content')

if [ -z "$CODE" ] || [ "$CODE" == "null" ]; then
  echo "❌ Failed to get response from model"
  echo "$RESPONSE"
  exit 1
fi

# Clean up possible markdown block if the model disobeyed
CODE=$(echo "$CODE" | sed -e 's/^```bash//' -e 's/^```sh//' -e 's/^```//' -e 's/```$//')

echo "✅ Received implementation from model. Executing file writes..."

# Save the script for auditing
echo "$CODE" > scratch/agent_output.sh
chmod +x scratch/agent_output.sh

# Execute the agent's bash commands
bash scratch/agent_output.sh

echo "🎉 Agent task completed successfully!"
