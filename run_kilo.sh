#!/bin/bash
set -e

if [ ! -f TASK_SPEC.md ]; then
  echo "Error: TASK_SPEC.md not found"
  exit 1
fi

PROMPT=$(cat TASK_SPEC.md)

echo "🚀 Starting Kilo Code agent..."
docker compose run --rm dev kilo run --auto "$PROMPT"
