# 📅 IPTV PLAYER - ROADMAP DETALLADO & DECISION LOG

> Semanas específicas, tasks, milestones y decisiones arqutitectónicas

---

## 🎯 VISIÓN RÁPIDA

```
SEMANA 1-2:    SETUP ARQUITECTURA (sin código)
SEMANA 3-6:    MVP FUNCIONAL (play video end-to-end)
SEMANA 7-10:   PRODUCCIÓN (robustez, performance)
SEMANA 11-12:  PORTFOLIO (documentación, showcase)

TOTAL: 12 SEMANAS (15h/week part-time) o 8 SEMANAS (full-time)
```

---

## 📊 SEMANA 1-2: PHASE 0 - SETUP & ARCHITECTURE

### **SEMANA 1: Planning & Design (40 horas)**

**Objetivo:** Finalizar ALL planning documents, setup repo, zero código

**Tareas:**

#### **Monday (8 hours)**

- [ ] Finalizar arquitectura general (DONE: IPTV_PLAYER_ARQUITECTURA.md)
- [ ] Finalizar data structures design (DONE: IPTV_PLAYER_DATA_DESIGN.md)
- [ ] Crear decision log (este documento)
- [ ] Setup GitHub repo (private → public after phase 1)
- [ ] GitHub Pages skeleton (for docs)
- [ ] Definir conventions (code style, naming, folder structure)

**Decision Log Entry:**

```
DECISION #1: Repository Visibility
- PUBLIC from start (portfolio benefit)
- Alternative: PRIVATE until MVP done, then public
- CHOSEN: PUBLIC (shows progress, community engagement)
- Risk: Incomplete project visible (mitigated by good README)
```

---

#### **Tuesday (8 hours)**

- [ ] Crear estructura de carpetas (folder tree)
- [ ] Crear CMakeLists.txt skeleton (compila pero no hace nada)
- [ ] Crear conanfile.txt con todas las deps
- [ ] Crear .github/workflows skeleton (builds, tests, coverage)
- [ ] Setup Dockerfile & docker-compose.yml
- [ ] Documentar setup instructions (BUILD.md)

**Decision Log Entry:**

```
DECISION #2: Build System & Package Manager
- CMake 3.22+ (industry standard)
- Conan 2.0+ (modern, version conflicts)
- Alternative: vcpkg (also good)
- Alternative: Manual downloads (no!)
- CHOSEN: CMake + Conan 2.0
- Why: Both widely used, excellent C++20 support
```

---

#### **Wednesday (8 hours)**

- [ ] Diseñar headers principales (sin .cpp)
  - [ ] network/network_subsystem.h
  - [ ] decoder/decoder_subsystem.h
  - [ ] render/render_subsystem.h
  - [ ] controller/controller.h
  - [ ] gui/gui_manager.h
  - [ ] common/types.h (all shared types)
  - [ ] common/queue.h (thread-safe queue)
  - [ ] common/logging.h

**Decision Log Entry:**

```
DECISION #3: Thread-Safe Queue Implementation
- Option 1: Lock-free (moodycamel::ConcurrentQueue)
  Pro: Best performance, low latency
  Con: Complex, harder to debug
- Option 2: Mutex + Condition Variable (std::mutex)
  Pro: Simple, easy to debug
  Con: Slightly higher latency
- CHOSEN: Option 2 for MVP, optimizable in phase 2
- Reasoning: MVP priority is correctness, not max performance
```

---

#### **Thursday (8 hours)**

- [ ] Diseñar types principales (common/types.h)
  - [ ] ByteBuffer
  - [ ] VideoFrame
  - [ ] AudioFrame
  - [ ] Channel
  - [ ] PlaybackState enum
  - [ ] Structs para Network/Decoder/Render stats
- [ ] Crear stubs para funciones principales (declarations only)
- [ ] Documentar invariants y pre/post-conditions

**Decision Log Entry:**

```
DECISION #4: Memory Management Strategy
- Option 1: Raw pointers (C-style)
  Pro: Most flexible
  Con: Easy to make mistakes
- Option 2: std::unique_ptr (modern C++)
  Pro: RAII, exception-safe
  Con: Some overhead
- Option 3: Mixed (unique_ptr where possible, raw where needed)
  Pro: Balanced
  Con: Inconsistent
- CHOSEN: Option 2 (unique_ptr as default)
- Reasoning: Portfolio shows modern C++ best practices
```

---

#### **Friday (8 hours)**

- [ ] Crear test skeleton (tests/CMakeLists.txt)
- [ ] Setup GTest framework
- [ ] Crear dummy unit tests (empty, just compile)
- [ ] Crear CI/CD workflows (GitHub Actions)
  - [ ] build.yml (build on push)
  - [ ] test.yml (ctest on push)
  - [ ] sanitizers.yml (ASan/TSan)
  - [ ] coverage.yml (code coverage)
- [ ] Documentar testing strategy (TESTING.md)

**Decision Log Entry:**

```
DECISION #5: Testing Framework
- GTest (Google Test)
- Alternative: Catch2 (also good)
- CHOSEN: GTest (wider industry adoption)
```

---

### **SEMANA 2: Infrastructure & Skeleton (40 horas)**

#### **Monday (8 hours)**

- [ ] Setup local development environment
  - [ ] Docker dev container (optional but recommended)
  - [ ] Documentation: env setup instructions
  - [ ] VSCode dev container config (.devcontainer)
- [ ] Create DEVELOPMENT.md with:
  - [ ] How to clone and build
  - [ ] How to run tests locally
  - [ ] How to debug
  - [ ] IDE setup (VSCode, CLion, etc)
- [ ] Setup git workflow
  - [ ] main (stable, PR only)
  - [ ] develop (integration branch)
  - [ ] feature/* (feature branches)
  - [ ] Branch protection rules

---

#### **Tuesday (8 hours)**

- [ ] Create initial CI/CD pipeline
  - [ ] Push to develop → runs build + tests
  - [ ] PR to main → runs all checks
  - [ ] Merging to main → creates GitHub release
- [ ] Setup code quality tools
  - [ ] clang-format (code formatting)
  - [ ] clang-tidy (static analysis)
  - [ ] cppcheck (additional analysis)
- [ ] Create pre-commit hooks (optional)

---

#### **Wednesday (8 hours)**

- [ ] Design CMake structure
  - [ ] Root CMakeLists.txt
  - [ ] src/CMakeLists.txt
  - [ ] tests/CMakeLists.txt
  - [ ] Separate builds: main app, tests, tools
- [ ] Create build configurations
  - [ ] Debug (with symbols, no optimization)
  - [ ] Release (optimized, stripped)
  - [ ] RelWithDebInfo (optimized + symbols)
- [ ] Document build variables

**Decision Log Entry:**

```
DECISION #6: Compiler & C++ Version
- Compiler: GCC 11+ or Clang 14+
- C++ Version: C++20 (latest, portfolio impact)
- C++ Standard: std=c++20
- Alternatives: C++17 (more portable)
- CHOSEN: C++20 (shows modern knowledge)
```

---

#### **Thursday (8 hours)**

- [ ] Setup Conan configuration
  - [ ] Define all dependencies (FFmpeg, SDL2, OpenGL, spdlog, GTest, ImGui)
  - [ ] Version pinning (reproducible builds)
  - [ ] Platform-specific variants
- [ ] Create conan profiles (Debug, Release, Sanitizer)
- [ ] Document dependency management

**Decision Log Entry:**

```
DECISION #7: GUI Framework (ImGui vs Qt)
- ImGui: Lighter, faster to MVP (2 weeks)
- Qt: More polished, professional (4 weeks)
- MVP Timeline: 4 weeks total
- DECISION: ImGui for MVP phase 1
- PLAN: Qt upgrade in phase 2 if time permits
- Reasoning: Time-boxed, focus on core architecture
```

---

#### **Friday (8 hours)**

- [ ] Create Docker configuration
  - [ ] Dockerfile (multi-stage)
  - [ ] docker-compose.yml (dev environment)
  - [ ] Documentation (DOCKER.md)
- [ ] Test Docker build end-to-end
- [ ] Create .dockerignore (optimize image)
- [ ] Final README.md skeleton (pre-phase 1)

---

### **PHASE 0 DELIVERABLES**

```
✅ GitHub repo fully structured
✅ CMakeLists.txt (compiles but nothing runs)
✅ conanfile.txt with all dependencies
✅ Dockerfile & docker-compose.yml working
✅ All architecture documents (3 markdown files)
✅ Headers with interfaces (no implementation)
✅ Shared types (common/types.h)
✅ CI/CD workflows configured
✅ Test framework skeleton (GTest setup)
✅ Development environment docs
✅ Decision log started
```

---

## 📊 SEMANA 3-6: PHASE 1 - MVP (FUNCIONAL)

### **SEMANA 3: Network Subsystem**

**Objetivo:** Descargar y parsear m3u8 + segmentos .ts

#### **Monday-Tuesday: HTTP Client Basics**

- [ ] Implement HTTP client (libcurl wrapper)
- [ ] GET request handler
- [ ] Response parsing
- [ ] Error handling (404, 500, timeout)
- [ ] Unit tests

```cpp
Interface:
HTTPClient::fetchURL(url, timeout_ms) → HTTPResponse
HTTPResponse: { status, headers, body, latency }
```

#### **Wednesday: M3U8 Parser**

- [ ] Parse M3U8 format
- [ ] Extract channels (EXTINF, URL)
- [ ] Handle variants (adaptive bitrate)
- [ ] Unit tests

#### **Thursday: Segment Download**

- [ ] Implement segment downloader
- [ ] Sequence-based (download current, next, prefetch)
- [ ] Retry logic (exponential backoff)
- [ ] Metrics collection

#### **Friday: Network Thread + Integration**

- [ ] Implement NetworkThread main loop
- [ ] Thread-safe output queue
- [ ] Controller communication
- [ ] Integration test (download real m3u8)

**Success Criteria:**

- [ ] Can download m3u8 from real IPTV playlist
- [ ] Can download .ts segments
- [ ] Unit tests: > 80%
- [ ] ASan/TSan clean
- [ ] GCS network stats

---

### **SEMANA 4: Decoder Subsystem**

**Objetivo:** Decodificar H.264 y AAC usando FFmpeg

#### **Monday-Tuesday: FFmpeg Initialization**

- [ ] Create FFmpeg context wrapper
- [ ] Open file / stream
- [ ] Find video/audio streams
- [ ] Handle multiple formats (H.264, AAC, VP9, Opus, etc)
- [ ] Error handling

#### **Wednesday: Video Decoding**

- [ ] Decode H.264 NAL units
- [ ] Convert YUV to renderable format (RGBA)
- [ ] Frame timing (PTS)
- [ ] Key frame handling

#### **Thursday: Audio Decoding**

- [ ] Decode AAC samples
- [ ] Resample if needed
- [ ] Sync with video (timing)

#### **Friday: Decoder Thread + Integration**

- [ ] Implement DecoderThread main loop
- [ ] Input from network queue
- [ ] Output to render queues
- [ ] Metrics collection
- [ ] Integration test

**Success Criteria:**

- [ ] Decode actual .ts segments
- [ ] Extract video frames + audio samples
- [ ] FFmpeg memory clean (ASan)
- [ ] Unit tests: > 80%
- [ ] Decode latency < 500ms

---

### **SEMANA 5: Render Subsystem + GUI**

**Objetivo:** Display video, play audio, basic GUI

#### **Monday: OpenGL Rendering**

- [ ] SDL2 window creation
- [ ] OpenGL context
- [ ] Shader compilation (quad rendering)
- [ ] YUV → RGBA conversion (shader or CPU)
- [ ] Texture upload

#### **Tuesday: Audio Output**

- [ ] SDL Audio device
- [ ] Audio callback setup
- [ ] Sample queuing
- [ ] A/V synchronization basics

#### **Wednesday: Render Thread**

- [ ] RenderThread main loop
- [ ] 60 FPS timing
- [ ] Frame dropping (if needed)
- [ ] Stats collection

#### **Thursday: ImGui Integration**

- [ ] ImGui + OpenGL backend
- [ ] Channel list
- [ ] Play/Pause/Stop buttons
- [ ] Basic stats display

#### **Friday: End-to-End Integration Test**

- [ ] Select channel → Network thread starts
- [ ] Network downloads m3u8 + segments
- [ ] Decoder decodes
- [ ] Render displays video
- [ ] Audio plays
- [ ] Manual integration test

**Success Criteria:**

- [ ] IPTV player plays actual video
- [ ] GUI responsive (60 FPS)
- [ ] Audio synchronized
- [ ] No crashes in 30-minute test

---

### **SEMANA 6: Polish & Testing**

**Objetivo:** MVP completo, robusto, testeado

#### **Monday: Error Handling**

- [ ] Network failures → UI notification
- [ ] Decoder errors → skip frame, continue
- [ ] Render errors → fallback resolution
- [ ] Graceful shutdown

#### **Tuesday: Synchronization & Deadlock Prevention**

- [ ] Review all mutex usage
- [ ] ThreadSanitizer clean
- [ ] Graceful pause/stop sequences
- [ ] No race conditions

#### **Wednesday: Metrics & Logging**

- [ ] Network stats (bandwidth, packet loss)
- [ ] Decoder stats (fps, resolution, latency)
- [ ] Render stats (fps, dropped frames)
- [ ] Structured logging (spdlog)
- [ ] Log to file + console

#### **Thursday: Testing**

- [ ] Unit tests for all subsystems
- [ ] Integration tests end-to-end
- [ ] Stress test (1 hour continuous)
- [ ] Performance benchmarks
- [ ] Code coverage > 80%

#### **Friday: Documentation & GitHub Polish**

- [ ] README.md (final version)
- [ ] Feature list
- [ ] Screenshots / demo video
- [ ] Known limitations
- [ ] Building & running instructions

**Success Criteria:**

- [ ] MVP fully functional
- [ ] All unit tests pass (80%+ coverage)
- [ ] ASan/TSan clean
- [ ] GCS all passing
- [ ] Demo-able (select channel, watch video)

---

## 📊 SEMANA 7-10: PHASE 2 - PRODUCTION-READY

### **SEMANA 7: Advanced Features**

**Objetivo:** Seeking, adaptive bitrate, better A/V sync

#### **Tasks:**

- [ ] Implement seeking (jump to position)
- [ ] Adaptive bitrate (switch based on bandwidth)
- [ ] Advanced A/V sync (audio clock reference)
- [ ] Resolution change handling mid-stream
- [ ] Performance optimization profiling

**Decision Log Entry:**

```
DECISION #8: Adaptive Bitrate Strategy
- Approach: Monitor bandwidth, switch variants
- Metrics: Rolling average bandwidth
- Switch threshold: ± 20% change
- Smoothness: Gradual quality degradation
- Alternative: ABR library (DASH.js, shaka-player concepts)
- CHOSEN: Custom simple implementation (learning opportunity)
```

---

### **SEMANA 8: Performance & Optimization**

**Objetivo:** Latency < 100ms, sustained 20+ Mbps, <300MB memory

#### **Tasks:**

- [ ] Profile with perf / gprof
- [ ] Identify hotspots
- [ ] Optimize FFmpeg usage
- [ ] Memory pool optimization
- [ ] Lock-free queue evaluation
- [ ] Benchmark results documentation

---

### **SEMANA 9: Quality & Security**

**Objetivo:** Production standards

#### **Tasks:**

- [ ] Code coverage > 85%
- [ ] Security audit
  - [ ] TLS certificate validation
  - [ ] Input sanitization (m3u8 parsing)
  - [ ] Buffer overflows (ASan clean)
- [ ] Cross-platform testing (Linux, macOS, Windows)
- [ ] Dependency vulnerability check
- [ ] Documentation completeness

---

### **SEMANA 10: Docker & Deployment**

**Objetivo:** Containerized, deployable, releasable

#### **Tasks:**

- [ ] Docker image optimization (multi-stage)
- [ ] Docker Compose for local dev
- [ ] Docker Compose for cloud deployment (if applicable)
- [ ] Kubernetes manifests (optional)
- [ ] Release tagging (v1.0.0)
- [ ] Binary distribution setup

**Decision Log Entry:**

```
DECISION #9: Release Strategy
- Version scheme: SemVer (1.0.0)
- Release cadence: One release at end of phase 2
- Alternative: Rolling releases
- CHOSEN: Single 1.0.0 release (shows completeness)
- Distribution: GitHub releases (binaries + source)
```

---

## 📊 SEMANA 11-12: PHASE 3 - PORTFOLIO SHOWCASE

### **SEMANA 11: Documentation & Blog**

**Objetivo:** Make project portfolio-worthy

#### **Tasks:**

- [ ] Comprehensive README
- [ ] Architecture documentation (with diagrams)
- [ ] API documentation (doxygen?)
- [ ] Building from source guide
- [ ] Deployment guide
- [ ] Performance benchmarks report
- [ ] Blog post: "Building an IPTV Player in C++"
  - [ ] Technical decisions made
  - [ ] Challenges faced
  - [ ] Learnings

---

### **SEMANA 12: Final Polish & GitHub**

**Objetivo:** Showcase ready

#### **Tasks:**

- [ ] Demo video (5-10 minutes)
  - [ ] Screen recording of player
  - [ ] Playing multiple channels
  - [ ] Stats display
  - [ ] GUI interaction
- [ ] GitHub project board setup
- [ ] Issue templates (bug, feature request)
- [ ] Contributing guide
- [ ] License (MIT)
- [ ] Final code review & cleanup
- [ ] LinkedIn post announcing release

**Final Deliverable:**

```
✅ Production-grade IPTV player
✅ 85%+ code coverage
✅ Zero ASan/TSan findings
✅ Comprehensive documentation
✅ Demo video
✅ GitHub polished (README, issues, releases)
✅ Performance benchmarks
✅ Blog post
✅ Ready for job interviews
```

---

## 🎯 KEY MILESTONES & GATES

### **Gate 1: End of Phase 0 (Semana 2)**

- ✅ All architecture documented
- ✅ Repo structure complete
- ✅ CI/CD functional
- **Decision:** Proceed to Phase 1 or redesign?
- **Success:** Yes (if setup complete)

### **Gate 2: End of Phase 1 (Semana 6)**

- ✅ IPTV player plays actual video
- ✅ MVP functional
- ✅ Unit tests > 80%
- **Decision:** Proceed to Phase 2 (optimization) or Phase 3 (release)?
- **Success:** Yes (if MVP working and stable)

### **Gate 3: End of Phase 2 (Semana 10)**

- ✅ Production-grade (latency < 100ms, memory < 300MB)
- ✅ 85%+ coverage
- ✅ All features working
- **Decision:** Release 1.0.0 or continue developing?
- **Success:** Yes → Release v1.0.0

### **Gate 4: End of Phase 3 (Semana 12)**

- ✅ Documentation complete
- ✅ Portfolio-ready
- ✅ Demo video
- **Decision:** Ship to jobs!
- **Success:** Completed → Ready to apply

---

## 📋 DECISION LOG

### **Format for all decisions:**

```
DECISION #N: [Title]
Context:
  - Why this decision matters
  - Alternatives considered
  
Options:
  A) [Option 1] → Pros: ... Cons: ...
  B) [Option 2] → Pros: ... Cons: ...
  C) [Option 3] → Pros: ... Cons: ...

Chosen: [Option X]
Reasoning:
  - Why this option was selected
  - Risk mitigation

Impact:
  - What changes as a result
  - Measurable outcomes

Reversibility: [Easy/Hard/Impossible]
  - How easy to change later?

Related Decisions: [#X, #Y]
```

---

### **DECISION LOG (Ongoing)**

**DECISION #1: Repository Visibility** [RECORDED ABOVE]

**DECISION #2: Build System** [RECORDED ABOVE]

**DECISION #3: Thread-Safe Queue** [RECORDED ABOVE]

**DECISION #4: Memory Management** [RECORDED ABOVE]

**DECISION #5: Testing Framework** [RECORDED ABOVE]

**DECISION #6: Compiler & C++** [RECORDED ABOVE]

**DECISION #7: GUI Framework** [RECORDED ABOVE]

**DECISION #8: Adaptive Bitrate** [RECORDED ABOVE]

**DECISION #9: Release Strategy** [RECORDED ABOVE]

---

## 📅 TIMELINE & TIME ESTIMATES

```
Week 1-2:   Phase 0 Setup           16-20 hours (flexible, setup work)
Week 3:     Network Subsystem       15-20 hours (API learning)
Week 4:     Decoder Subsystem       15-20 hours (FFmpeg heavy)
Week 5:     Render + GUI            15-20 hours (OpenGL basics)
Week 6:     Testing & Polish        15-20 hours (integration work)
Week 7:     Advanced Features       15-20 hours (feature work)
Week 8:     Performance             15-20 hours (profiling, optimization)
Week 9:     Quality & Security      15-20 hours (testing, audit)
Week 10:    Docker & Deployment     15-20 hours (DevOps work)
Week 11:    Documentation & Blog    15-20 hours (writing, recording)
Week 12:    Final Polish            15-20 hours (review, polish)

TOTAL: 165-220 hours
Average: 180 hours / 12 weeks = 15 hours per week

For reference:
- Part-time (15h/week) = 12 weeks
- Full-time (40h/week) = 4.5 weeks (but burnout risk!)
- Recommended: Mix of both (20-25h/week) = 7-9 weeks
```

---

## ✅ PHASE 0 VERIFICATION CHECKLIST

Before moving to Phase 1, verify:

- [ ] GitHub repo created and public
- [ ] All 3 architecture documents finalized
- [ ] CMakeLists.txt compiles cleanly
- [ ] conanfile.txt defines all dependencies
- [ ] Docker image builds successfully
- [ ] CI/CD workflows running (green)
- [ ] All headers written (interfaces, no implementation)
- [ ] common/types.h complete (ByteBuffer, VideoFrame, etc)
- [ ] Test framework (GTest) set up
- [ ] Git workflow established (main, develop, feature/*)
- [ ] Development environment docs complete
- [ ] README.md skeleton in place
- [ ] Decision log started with ≥ 5 decisions

**Go / No-Go:** [Decision at end of Week 2]

---

## 🎯 CONCLUSION

This roadmap provides:

- ✅ Week-by-week specifics
- ✅ Clear deliverables per week
- ✅ Success criteria at each stage
- ✅ Decision log for architectural choices
- ✅ Risk mitigation strategies
- ✅ Time estimates (realistic)
- ✅ Gates for go/no-go decisions

**Next action:** Start PHASE 0 Week 1 → Create GitHub repo skeleton
