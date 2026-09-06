# 🎬 IPTV Player

A production-grade IPTV player built in C++20.

## Building with Docker

### Development

```bash
docker-compose up -d dev
docker-compose exec dev bash

# Inside container:
conan install . --output-folder=build
cmake -B build
cmake --build build
ctest
```

## Project Structure