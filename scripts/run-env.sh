#!/bin/bash
set -e

# Grant permissions to local X server to accept clients from containers
xhost +local:docker > /dev/null 2>&1

# Create .env file with custom variables for docker-compose
export MY_UID=$(id -u)
export MY_GID=$(id -g)
echo "MY_UID=$MY_UID" > .env
echo "MY_GID=$MY_GID" >> .env

echo "🐳 Building IPTV Player Development Container..."

# Build dev image
docker build --build-arg UID=$MY_UID --build-arg GID=$MY_GID -f ../Dockerfile.dev -t iptv-player:dev .

echo "✅ Development container built!"
echo ""
echo "🚀 To start development, run:"
echo "   docker-compose up -d dev"
echo "   docker-compose exec dev bash"
echo ""
echo "📝 Inside container:"
echo "   conan install ."
echo "   cmake -B build"
echo "   cmake --build build"
echo "   ctest"
echo ""

# Run the development container interactively
