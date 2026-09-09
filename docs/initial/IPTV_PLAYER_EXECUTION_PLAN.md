# 📋 IPTV PLAYER — PLAN DE EJECUCIÓN

**Versión:** 1.0  
**Objetivo:** Guía paso-a-paso para implementar IPTV Player en 12 semanas  
**Formato:** Checklist detallado por semana

---

## 🎯 ANTES DE EMPEZAR (Pre-week)

### Pre-requisitos Técnicos

- [ ] Git configurado + GitHub cuenta
- [ ] C++20 compiler installed (GCC 11+ or Clang 13+)
- [ ] CMake 3.24+ installed
- [ ] Conan 2.x installed
- [ ] Docker + docker-compose installed (para deployment)
- [ ] Visual Studio Code o CLion configurado

### Pre-requisitos de Conocimiento

- [ ] Leído ARCHITECTURE.md completamente
- [ ] Entendido diagrama threads + circular buffer
- [ ] Conocimiento básico FFmpeg (qué es, para qué sirve)
- [ ] Conocimiento mutex + condition_variable (std::thread)

### Procesos

- [ ] Crear GitHub repo: `iptv-player`
- [ ] Habilitar GitHub Actions
- [ ] Crear proyecto GitHub (Kanban board)
- [ ] Crear Discord/Telegram para tracking (opcional)

---

## SEMANA 1-2: BOILERPLATE & NETWORK FOUNDATION

**Horas:** 20  
**Hito:** Puedo compilar código vacío, CI/CD ejecuta, libcurl funciona

### Semana 1.1: GitHub + CMake (5 horas)

#### Lunes

- [ ] Clonar template repo C++20 (o crear desde cero)
- [ ] Crear estructura de directorios:
  ```
  src/
  test/
  cmake/
  conanfile.txt
  CMakeLists.txt
  .github/workflows/
  docker/
  ```
- [ ] Commit: "Initial project structure"

#### Martes-Miércoles

- [ ] Escribir CMakeLists.txt principal:
  - [ ] Project name + C++20 standard
  - [ ] Enable testing
  - [ ] Add subdirectories (src/, test/)
  - [ ] Setup compiler flags (warnings, sanitizers)
- [ ] Escribir conanfile.txt:
  - [ ] libcurl/8.x
  - [ ] gtest/cci.20210126
  - [ ] spdlog/1.12.x
  - [ ] (FFmpeg lo hacemos luego, primer commit solo boilerplate)
- [ ] Crear CMakeLists.txt en src/, test/
- [ ] Commit: "CMake + Conan setup"

#### Jueves

- [ ] Setup .clang-format (Google style o LLVM)
- [ ] Setup .clang-tidy config
- [ ] Setup GitHub Actions workflow (ci-linux.yml):
  ```yaml
  - checkout
  - setup conan
  - conan install
  - cmake -B build
  - cmake --build build
  - ctest
  - Run clang-tidy
  - Run AddressSanitizer
  ```
- [ ] Commit: "GitHub Actions CI/CD setup"
- [ ] Verificar que PR pasa CI

#### Viernes

- [ ] Crear README.md template
- [ ] Crear CONTRIBUTING.md
- [ ] Crear ARCHITECTURE link (link a documento)
- [ ] Crear proyecto Kanban GitHub
  - [ ] Columns: Backlog, In Progress, Done
  - [ ] Create cards for Phase 1
- [ ] Commit: "Documentation skeleton"

**Hito Semana 1:** ✅ Compilación limpia, CI pasa, estructura lista

---

### Semana 1.2: Network Component MVP (15 horas)

#### Lunes-Martes

- [ ] Crear `src/network/` directory
- [ ] Escribir `src/network/types.h`:
  ```cpp
  struct Channel {
      std::string name;
      std::string url;  // M3U8 URL
      std::string logo;
  };
  
  struct NetworkStats {
      uint64_t bytes_downloaded;
      float mbps;
      uint32_t segments;
  };
  ```
- [ ] Escribir stub `src/network/http_client.h`:
  ```cpp
  class HttpClient {
  public:
      std::vector<uint8_t> get(const std::string& url);
      // More methods later
  };
  ```
- [ ] Escribir implementación mínima con libcurl
- [ ] Commit: "HttpClient basic implementation"
- [ ] **Crear unit test:** `test/network/test_http_client.cpp`
  - [ ] Test: GET simple URL (use httpbin.org for testing)
  - [ ] Test: Validate status code handling
  - [ ] Compile & run: `ctest --verbose`
- [ ] Commit: "HTTP client tests"

#### Miércoles-Jueves

- [ ] Crear `src/network/playlist_parser.h`:
  ```cpp
  class PlaylistParser {
  public:
      std::vector<Channel> parseM3U8(const std::string& content);
  };
  ```
- [ ] Implementar parser:
  - [ ] Simple regex para `#EXTINF:` (duración)
  - [ ] Extraer URL siguiente
  - [ ] Extraer nombre de canal (anterior a URL)
- [ ] **Crear unit test:** `test/network/test_playlist_parser.cpp`
  - [ ] Test: Parse sample.m3u8 (crear archivo fixture)
  - [ ] Test: Validate 5 channels parsed
  - [ ] Test: Invalid M3U8 handling
- [ ] Commit: "PlaylistParser with tests"

#### Viernes

- [ ] Crear `src/network/network_component.h` (interface):
  ```cpp
  class NetworkComponent {
  public:
      std::vector<Channel> loadPlaylist(const std::string& url);
      ByteBuffer downloadSegment(const std::string& url);
      NetworkStats getStats();
      void start();  // Thread entry
      void stop();
  };
  ```
- [ ] Crear implementación stub (no threads aún)
- [ ] Commit: "NetworkComponent interface & stub"

**Hito Semana 2:** ✅ Network component compilable & testeable, CI pasa

**Suma Total:** Network compilable, tests passing, ready para threading

---

## SEMANA 3: DECODER INTEGRATION

**Horas:** 15  
**Hito:** FFmpeg wrapper funciona, puede decodificar sample TS

### Semana 3.1: FFmpeg Wrapper (10 horas)

#### Lunes-Martes

- [ ] Add FFmpeg a conanfile.txt
- [ ] Crear `src/decoder/ffmpeg_wrapper.h`:
  ```cpp
  class FFmpegWrapper {
  public:
      bool open(const uint8_t* data, size_t size);  // Open demuxer
      bool readNextPacket(AVPacket* pkt);
      AVStream* getVideoStream();
      AVStream* getAudioStream();
      void close();
  };
  ```
- [ ] Implementar wrapper (manejo básico de AVFormatContext)
- [ ] Commit: "FFmpeg wrapper - demux"

#### Miércoles-Jueves

- [ ] Crear `src/decoder/av_frame.h` (RAII wrapper):
  ```cpp
  class AVFrameWrapper {
  private:
      AVFrame* frame;
  public:
      AVFrameWrapper();
      ~AVFrameWrapper();  // av_frame_free
      AVFrame* get();
  };
  ```
- [ ] Implementar RAII cleanup
- [ ] Crear `src/decoder/decoder_component.h`:
  ```cpp
  class DecoderComponent {
  public:
      bool decode(const uint8_t* data, size_t size);
      bool getVideoFrame(AVFrame*& frame);
      bool getAudioFrame(AVFrame*& frame);
      DecoderStats getStats();
  };
  ```
- [ ] Implementar decodificación:
  - [ ] Abrir demuxer con FFmpeg
  - [ ] Loop: leer packet → decode
  - [ ] Video: H.264 → YUV
  - [ ] Audio: AAC → PCM
- [ ] Commit: "DecoderComponent basic decode"

#### Viernes

- [ ] Crear `test/decoder/test_ffmpeg_wrapper.cpp`
  - [ ] Fixture: crear sample.ts pequeño (o usar público)
  - [ ] Test: open() válido
  - [ ] Test: readNextPacket() retorna datos
- [ ] Crear `test/decoder/test_decoder_component.cpp`
  - [ ] Test: decode() sin crash
  - [ ] Test: getVideoFrame() retorna algo
- [ ] Commit: "Decoder tests"

**Hito Semana 3:** ✅ FFmpeg integration functional, puede decodificar bytes

---

## SEMANA 4: RENDER THREAD

**Horas:** 15  
**Hito:** Puede mostrar color en pantalla, audio plays

### Semana 4.1: SDL2 + OpenGL (8 horas)

#### Lunes-Martes

- [ ] Add SDL2 a conanfile
- [ ] Crear `src/render/sdl_window.h`:
  ```cpp
  class SDLWindow {
  public:
      bool create(int width, int height);
      void swap();
      void* getGLContext();
      bool shouldClose();
      void close();
  };
  ```
- [ ] Implementar SDL window + OpenGL context
- [ ] Commit: "SDL2 window"

#### Miércoles

- [ ] Crear `src/render/gl_shader.h`:
  ```cpp
  class GLShader {
  public:
      bool compile(const char* vertex, const char* fragment);
      void use();
      void setUniform(...);
  };
  ```
- [ ] Implementar simple vertex + fragment shader (YUV → RGB)
- [ ] Commit: "OpenGL shader"

#### Jueves-Viernes

- [ ] Crear `src/render/render_component.h`:
  ```cpp
  class RenderComponent {
  public:
      bool renderFrame(const AVFrame* yuv_frame);
      bool playAudio(const AVFrame* pcm_frame);
      RenderStats getStats();
      void start();
      void stop();
  };
  ```
- [ ] Implementar:
  - [ ] OpenGL texture from YUV data
  - [ ] Draw quad con shader
  - [ ] SDL2 audio output (simple)
- [ ] Test: Compile & run (shows something)
- [ ] Commit: "RenderComponent"

**Hito Semana 4:** ✅ Window shows, can render texture, basic audio

---

## SEMANA 5: GUI + THREADING SYNCHRONIZATION

**Horas:** 15  
**Hito:** ImGui renders, press play botton → video starts

### Semana 5.1: ImGui Integration (8 horas)

#### Lunes-Martes

- [ ] Add ImGui a conanfile (o build from source)
- [ ] Crear `src/gui/imgui_window.h`:
  ```cpp
  class ImGuiWindow {
  public:
      void create(SDL_Window* window);
      void beginFrame();
      void endFrame();
      void render();
  };
  ```
- [ ] Implementar ImGui init + SDL2 backend
- [ ] Commit: "ImGui setup"

#### Miércoles

- [ ] Crear `src/gui/widgets/`:
  - [ ] channel_list.h (ImGui::ListBox)
  - [ ] playback_panel.h (play/pause/stop buttons)
  - [ ] stats_panel.h (mostrar bitrate, fps)
  - [ ] video_canvas.h (OpenGL texture display)
- [ ] Implementar widgets (stubs)
- [ ] Commit: "GUI widgets"

#### Jueves-Viernes

- [ ] Crear `src/app/player_controller.h` (interface):
  ```cpp
  class PlayerController {
  public:
      void play(const Channel& ch);
      void pause();
      void stop();
      PlayerState getState();
  };
  ```
- [ ] Crear circular buffer thread-safe en `src/common/circular_buffer.h`:
  ```cpp
  template<typename T>
  class CircularBuffer {
  private:
      std::vector<T> data;
      std::mutex mtx;
      std::condition_variable not_empty, not_full;
  public:
      void enqueue(const T& item);
      bool dequeue(T& item);
  };
  ```
- [ ] Commit: "PlayerController + CircularBuffer"

### Semana 5.2: Integration (7 horas)

#### Lunes-Martes

- [ ] Main loop architecture:
  ```cpp
  while (!quit) {
      gui.beginFrame();
      gui.render(controller.getState());
      gui.endFrame();
      
      network_thread.update();
      decoder_thread.update();
      render_thread.update();
  }
  ```
- [ ] Crear `src/main.cpp` básico
- [ ] Threading:
  - [ ] Network thread: `std::thread`
  - [ ] Decoder thread: `std::thread`
  - [ ] Render thread: main thread (for OpenGL)
  - [ ] GUI thread: main thread (for ImGui)
- [ ] Commit: "Threading skeleton"

#### Miércoles-Viernes

- [ ] Integration tests:
  - [ ] Start application
  - [ ] Click "Play"
  - [ ] Verify threads create
  - [ ] Verify buffers sync
- [ ] Debug threading issues
- [ ] Commit: "Full MVP integration"
- [ ] Test con sample playlist

**Hito Semana 5:** ✅ GUI renderi, threads run, basic playback works

---

## SEMANA 6: STABILIZATION & MVP RELEASE

**Horas:** 10  
**Hito:** AddressSanitizer clean, tests pass, MVP v1.0 ready

### Semana 6.1: Quality Assurance (6 horas)

#### Lunes-Miércoles

- [ ] Run AddressSanitizer:
  - [ ] Enable in CMakeLists: `add_compile_options(-fsanitize=address)`
  - [ ] Run application
  - [ ] Fix all reported issues
  - [ ] Commit: "AddressSanitizer clean"

#### Jueves

- [ ] Run ThreadSanitizer (TSan):
  - [ ] Enable in CMakeLists
  - [ ] Run with light workload (avoid false positives)
  - [ ] Fix real race conditions
- [ ] Update GitHub Actions:
  - [ ] Add ASan build
  - [ ] Add TSan build (optional, heavy)
  - [ ] All must pass
- [ ] Commit: "ThreadSanitizer setup"

#### Viernes

- [ ] Final testing:
  - [ ] Manual playback test (real IPTV or sample)
  - [ ] Verify no crashes for 5 minutes
  - [ ] Verify stats display correctly
- [ ] Tagging:
  - [ ] Tag release `v1.0-mvp`
  - [ ] Push to GitHub
  - [ ] Create release notes
- [ ] Commit: "MVP v1.0 release"

### Semana 6.2: Documentation (4 horas)

#### Lunes-Martes

- [ ] Update README.md:
  - [ ] Add features list
  - [ ] Add architecture diagram (from ARCHITECTURE.md)
  - [ ] Add building instructions
  - [ ] Add screenshots/GIFs (if possible)
- [ ] Create BUILDING.md:
  - [ ] Native build steps
  - [ ] Docker build steps
  - [ ] Troubleshooting
- [ ] Commit: "README + BUILDING docs"

**Fin Semana 6:** 
✅ MVP COMPLETE
✅ Compila sin warnings
✅ Tests passing
✅ AddressSanitizer clean
✅ CI/CD verde
✅ Repository showcase-ready

---

## SEMANA 7-8: ADVANCED FEATURES

**Horas:** 20  
**Hito:** Seeking, adaptive bitrate, robust error handling

### Semana 7: Seeking + Error Recovery (10 horas)

#### Lunes-Martes

- [ ] Implementar seeking en playlist parser:
  - [ ] Calcular timestamp de cada segment
  - [ ] Al buscar, encontrar segment más cercano
- [ ] Añadir `PlayerController::seek(int64_t ms)`
- [ ] Implementar:
  - [ ] Pausar reproducción
  - [ ] Flush de buffers
  - [ ] Redirigir network thread a nuevo segment
  - [ ] Resume
- [ ] Commit: "Seeking support"

#### Miércoles-Jueves

- [ ] Error recovery:
  - [ ] Network error → auto-retry (exponential backoff)
  - [ ] Decoder error → skip frame (log warning)
  - [ ] Render error → pause (show error UI)
- [ ] Crear error queue thread-safe
- [ ] Implementar en controller
- [ ] Commit: "Error handling + recovery"

#### Viernes

- [ ] Tests:
  - [ ] test_seeking.cpp
  - [ ] test_network_error_recovery.cpp
- [ ] Commit: "Advanced feature tests"

### Semana 8: Adaptive Bitrate + Audio/Video Sync (10 horas)

#### Lunes-Martes

- [ ] Adaptive bitrate:
  - [ ] Monitor buffer state (% full)
  - [ ] Si buffer <20% → reduce quality
  - [ ] Si buffer >80% → increase quality
  - [ ] Smooth transitions
- [ ] Commit: "Adaptive bitrate"

#### Miércoles-Jueves

- [ ] A/V Sync:
  - [ ] Track PTS (presentation timestamp) from FFmpeg
  - [ ] Render thread syncs audio/video via PTS
  - [ ] Tolerate small jitter
- [ ] Performance benchmarking:
  - [ ] Measure decode latency
  - [ ] Measure render time
  - [ ] Identify bottleneck
- [ ] Commit: "AV sync + benchmarks"

#### Viernes

- [ ] Integration test con real IPTV stream
- [ ] Manual testing 30+ minutos
- [ ] Commit: "Stability improvements"

---

## SEMANA 9: TESTING & QUALITY

**Horas:** 15  
**Hito:** Code coverage >80%, ThreadSanitizer clean, benchmarks documented

### Semana 9.1: Test Coverage (8 horas)

#### Lunes-Miércoles

- [ ] Analizar coverage actual:
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage"
  cmake --build build
  ctest
  gcov output (analyze)
  ```
- [ ] Identificar uncovered lines
- [ ] Escribir tests para:
  - [ ] Edge cases en parser
  - [ ] Malformed data handling
  - [ ] Timeout scenarios
- [ ] Commit: "Increased test coverage"

#### Jueves-Viernes

- [ ] Code coverage >80%:
  - [ ] Add LCOV reporting to CI/CD
  - [ ] Prevent PR merge si coverage baja
- [ ] Commit: "Coverage enforcement"

### Semana 9.2: ThreadSanitizer + Static Analysis (7 horas)

#### Lunes-Martes

- [ ] ThreadSanitizer intensive testing:
  - [ ] Run with real stress (many buffers, high throughput)
  - [ ] Fix any legitimate race conditions
  - [ ] Document false positives
- [ ] Commit: "ThreadSanitizer clean"

#### Miércoles-Jueves

- [ ] clang-tidy analysis:
  - [ ] Run over entire codebase
  - [ ] Fix warnings
  - [ ] Configure .clang-tidy para stricter rules
- [ ] Commit: "clang-tidy clean"

#### Viernes

- [ ] Valgrind (optional, heavy):
  ```bash
  valgrind --leak-check=full ./iptv_player
  ```
- [ ] Commit: "Valgrind analysis"

---

## SEMANA 10: DEPLOYMENT

**Horas:** 10  
**Hito:** Docker image works, cross-platform builds pass

### Semana 10.1: Docker (6 horas)

#### Lunes-Martes

- [ ] Crear `docker/Dockerfile`:
  ```dockerfile
  FROM ubuntu:22.04
  WORKDIR /app
  COPY . /app
  RUN apt-get update && apt-get install -y ...
  RUN conan install ...
  RUN cmake -B build && cmake --build build
  ENTRYPOINT ["./build/iptv_player"]
  ```
- [ ] Crear `docker/Dockerfile.dev` (with dev tools)
- [ ] Crear `docker-compose.yml`:
  - [ ] Service: iptv_player
  - [ ] Volume: mounts source
  - [ ] Network: exposes port (if needed)
- [ ] Test locally:
  - [ ] `docker build .`
  - [ ] `docker run`
  - [ ] Verify application starts
- [ ] Commit: "Docker setup"

#### Miércoles

- [ ] Docker image optimization:
  - [ ] Multi-stage build (reduce size)
  - [ ] Cache layers efficiently
  - [ ] Document image size + startup time
- [ ] Commit: "Optimized Docker image"

### Semana 10.2: Cross-Platform (4 horas)

#### Jueves

- [ ] GitHub Actions updates:
  - [ ] Add Windows build (MSVC)
  - [ ] Add macOS build (Clang)
  - [ ] All three (Linux/Windows/macOS) must pass
- [ ] Fix platform-specific issues:
  - [ ] Header compatibility (#ifdef)
  - [ ] Path handling (/ vs \)
  - [ ] Library availability
- [ ] Commit: "Cross-platform CI/CD"

#### Viernes

- [ ] Package for distribution:
  - [ ] Linux: AppImage or .tar.gz
  - [ ] Windows: .exe installer (optional, complex)
  - [ ] macOS: .dmg (optional)
- [ ] Commit: "Packaging"

---

## SEMANA 11: DOCUMENTATION & POLISH

**Horas:** 10  
**Hito:** README impresiona, architecture doc completa, demo listo

### Semana 11.1: Technical Docs (6 horas)

#### Lunes-Martes

- [ ] Generar API docs (Doxygen):
  ```bash
  doxygen Doxyfile
  ```
- [ ] Escribir:
  - [ ] API.md (public interfaces)
  - [ ] DESIGN_DECISIONS.md (why we chose X over Y)
  - [ ] PERFORMANCE.md (benchmarks, latency profiles)
- [ ] Commit: "Technical documentation"

#### Miércoles

- [ ] Diagrams:
  - [ ] Architecture diagram (draw.io → PNG)
  - [ ] Data flow diagram
  - [ ] State machine diagram
  - [ ] Thread synchronization diagram
- [ ] Add to README + docs/
- [ ] Commit: "Architecture diagrams"

#### Jueves-Viernes

- [ ] Code examples:
  - [ ] How to build
  - [ ] How to run
  - [ ] How to extend (custom decoder, etc)
- [ ] Commit: "Code examples + tutorials"

### Semana 11.2: Polish (4 horas)

#### Lunes-Martes

- [ ] README final:
  - [ ] Hero image/GIF
  - [ ] Feature list
  - [ ] Quick start
  - [ ] Architecture link
  - [ ] Performance stats
  - [ ] Contributing guide
- [ ] Commit: "Final README"

#### Miércoles-Viernes

- [ ] Code review (self):
  - [ ] Read through every file
  - [ ] Comments clear?
  - [ ] Naming conventions consistent?
  - [ ] No debug code left?
- [ ] Final cleanup:
  - [ ] Remove TODOs that weren't done
  - [ ] Check compiler warnings
  - [ ] Verify all tests pass
- [ ] Commit: "Final polish"

---

## SEMANA 12: FINAL TESTING & RELEASE

**Horas:** 10  
**Hito:** v1.0 released, portfolio-ready, demo prepared

### Semana 12.1: Final Testing (5 horas)

#### Lunes-Martes

- [ ] Full system test:
  - [ ] Build from scratch (clean clone)
  - [ ] All tests pass
  - [ ] All sanitizers pass
  - [ ] CI/CD verde
- [ ] Manual testing:
  - [ ] 1 hour playback test
  - [ ] Network dropout handling
  - [ ] Memory usage monitoring (no leaks)
- [ ] Commit: "Final system test"

#### Miércoles

- [ ] Performance validation:
  - [ ] Decode latency <100ms?
  - [ ] Render @ 60 FPS sustained?
  - [ ] Buffer efficiency?
- [ ] Commit: "Performance validated"

### Semana 12.2: Release & Demo (5 horas)

#### Jueves

- [ ] Release tagging:
  - [ ] Tag `v1.0-production`
  - [ ] Create GitHub release with:
    - [ ] Release notes (features, bug fixes from MVP)
    - [ ] Download links (Docker, native build instructions)
    - [ ] Known issues/limitations
  - [ ] Push to GitHub
- [ ] Commit: "v1.0 production release"

#### Viernes

- [ ] Demo preparation:
  - [ ] Record 3-5 minute demo video (optional)
    - [ ] Opening application
    - [ ] Loading playlist
    - [ ] Playing stream
    - [ ] Showing stats
  - [ ] Create presentation slides (optional, for interviews)
  - [ ] Prepare "walking through code" for interviews
- [ ] Final GitHub checks:
  - [ ] Repository looks professional
  - [ ] README is compelling
  - [ ] Stars/forks coming in? (unlikely immediate, but okay)
- [ ] Commit: "Demo content"

---

## 📊 TRACKING & METRICS

### Commit Frequency Target

- **MVP Phase (Weeks 1-6):** 3-4 commits/week (stable features)
- **Feature Phase (Weeks 7-8):** 2-3 commits/week
- **Quality Phase (Weeks 9-12):** 2-3 commits/week

### GitHub Actions Success Rate

- **Target:** 100% passing
- **Rule:** Never merge failing PR
- **Measure:** Add badge to README

### Code Quality Metrics

| Metric | Target | Check Method |
|---|---|---|
| Code Coverage | >80% | `gcov` + CI report |
| Compiler Warnings | 0 | CI build step |
| Clang-tidy issues | 0 | CI step |
| ASan errors | 0 | CI step |
| TSan errors | 0 | CI step (optional) |
| Valgrind leaks | 0 | Manual `valgrind` |

### Testing Metrics

| Metric | Target |
|---|---|
| Unit tests | >50 |
| Integration tests | >5 |
| Manual test hours | >10 |
| Test pass rate | 100% |

---

## 🚨 RIESGOS & CONTINGENCIAS

### Riesgo: FFmpeg Complexity

**Señal:** No puedo compilar FFmpeg code, crashes en decode

**Acción:**
1. Revert a librería más simple (libav, openh264)
2. O, usar wrapper existente (MXE, ffmpeg-python bindings)
3. Delay Phase 2, focus en Phase 1 stability

### Riesgo: Threading Bugs

**Señal:** Deadlock, race condition (TSan), crashes

**Acción:**
1. Simplificar sincronización (mutex is fine, don't rush lock-free)
2. Add debug logging copiously
3. Run with DDD debugger
4. Ask Stack Overflow / FFmpeg forum if needed

### Riesgo: UI Freeze

**Señal:** ImGui or render thread blocks, GUI unresponsive

**Acción:**
1. Move blocking operations to separate thread
2. Use non-blocking I/O (libcurl)
3. Profile with perf/profiling tool
4. Optimize bottleneck

### Riesgo: Network Failures

**Señal:** Can't test with real IPTV (no access)

**Acción:**
1. Create fake M3U8 + sample TS files locally
2. Mock HTTP server (simple Python Flask)
3. Test with mocked network errors

### Riesgo: Scope Creep

**Señal:** "Oh wait, let me also add Qt, MQTT, Kubernetes..."

**Acción:**
1. **Ignore.** Stick to MVP.
2. Phase 2 is already generous with features.
3. Phase 3 is bonus.
4. Done is better than perfect.

---

## ✅ DEFINITION OF DONE

### Per Week

- [ ] All planned items in checklist completed
- [ ] New code has tests
- [ ] All tests passing locally + CI
- [ ] No compiler warnings
- [ ] No sanitizer warnings
- [ ] Commit message is descriptive
- [ ] Code review (self): passing

### Per Phase

- [ ] All Phase checklist items done
- [ ] Documentation updated
- [ ] README reflects current state
- [ ] GitHub project updated
- [ ] Demo or video prepared

### Project Complete (Week 12)

- [ ] All technical criteria met (memory safe, concurrent, tested)
- [ ] All portfolio criteria met (impressive README, clear architecture)
- [ ] v1.0 released
- [ ] Demo prepared for interviews
- [ ] Can explain every design decision
- [ ] Ready to present to recruiters

---

## 📝 TEMPLATE: WEEKLY CHECKLIST

Copy this para cada semana:

```markdown
## Week X: [TITLE]

**Objective:** [what we're building]
**Hours planned:** X
**Hito:** [success criteria]

### Lunes
- [ ] Task 1
- [ ] Task 2

### Martes
- [ ] Task 3

...

### Viernes
- [ ] Final review
- [ ] Commits pushed
- [ ] CI passing

**Summary:**
- What went well?
- What was difficult?
- Next week dependencies?

**Metrics:**
- Lines of code added: X
- Tests added: Y
- Commits: Z
```

---

## 🎯 FINAL REMINDERS

1. **Commit frequently** (at least daily)
2. **Test continuously** (before pushing)
3. **Don't skip weeks** (timeline is realistic, not optimistic)
4. **Ask for help** (Stack Overflow, FFmpeg docs, friends)
5. **Celebrate wins** (week 6 MVP is a big deal!)
6. **Document decisions** (future you will thank you)
7. **Keep scope tight** (MVP first, features second)

---

**Version:** 1.0  
**Last updated:** September 2026  
**Status:** Ready to execute ✅

**Good luck! 🚀**
