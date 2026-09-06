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
iptv-player/ 
├── src/ (source code) 
├── test/ (unit tests) 
├── cmake/ (CMake modules) 
├── CMakeLists.txt
├── conanfile.py 
└── Dockerfile.dev


## Architecture

See docs/ARCHITECTURE.md

## Timeline

12 weeks (100-120 hours)
- Week 1-6: MVP
- Week 7-12: Production-ready