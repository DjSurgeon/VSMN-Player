# 🏗️ IPTV PLAYER — DOCUMENTO DE ARQUITECTURA

**Versión:** 1.0  
**Fecha:** Septiembre 2026  
**Estado:** Design Phase - Listo para implementación  
**Audiencia:** Recruiters, Interviewers, Future Maintainers

## 1. VISIÓN ESTRATÉGICA

### 1.1 Objetivo del Proyecto

Construir un **IPTV Player de calidad profesional** en C++20 que demuestre capacidad en:

- **Networking:** Descarga HTTP de streams, parseo de playlists M3U8, manejo de TLS
- **Concurrencia:** 3+ threads coordinados, sincronización sin race conditions
- **Procesamiento media:** Integración con FFmpeg para decodificación H.264/AAC
- **UI responsiva:** Desktop GUI que no se bloquea bajo carga de red
- **DevOps/Calidad:** CI/CD, sanitizers, testing, containerización Docker

### 1.2 Por qué este proyecto impacta en portfolio

**No es un "hello world"** — es un sistema real donde:

- El network thread falla → aplicación debería recuperarse
- El decoder thread se atrasa → UI debe mostrar buffer state
- El render thread se ralentiza → frames se dropean, audio continúa
- Cada cambio requiere testing riguroso (no hay margen para bugs)

**Esto es lo que los recruiters quieren ver.**

### 1.3 Definición de "listo para producción"

- ✅ Compila sin warnings
- ✅ Corre tests sin fallos
- ✅ Sanitizers (ASan, TSan) pasan limpio
- ✅ Documentación clara
- ✅ Docker funciona
- ✅ README impresiona

---

## 2. DECISIONES ARQUITECTÓNICAS CLAVE

### 2.1 Arquitectura Multi-Thread vs Single-Thread

**Decisión:** Multi-thread con 3 threads especializados

**Justificación:**

| Aspecto | Single-Thread | Multi-Thread (Nuestro) |
| -------- | --- | --- |
| Complejidad | ✅ Simple | ❌ Compleja, pero realista |
| Performance | ❌ Bajo (bloques de I/O) | ✅ Alto (paralelo) |
| Realismo corporativo | ❌ No existe en producción | ✅ Standard en streaming |
| Que aprendo | ❌ Casi nada sobre concurrencia | ✅ Sincronización real |
| Diferenciador en CV | ❌ Básico | ✅ **Altamente impresionante** |

**Threads:**

1. **Network Thread:** Descarga data, maneja retries, metricas
2. **Decoder Thread:** FFmpeg, demux, decode
3. **Render Thread:** OpenGL/SDL2, audio playback, timing

### 2.2 Sincronización: Lock-Free vs Mutex

**Decisión:** Mutex + Condition Variables (fase 1), lock-free para fase 2

**Justificación:**

- **Fase 1 (MVP):** Mutex es correcto, más simple de debuggear
- **Fase 2 (Production):** Lock-free circular buffer demuestra conocimiento avanzado
- Los recruiters entienden ambos enfoques; el mutex-based es suficiente

**Pattern:** Producer-Consumer con circular buffer

```
Network Thread (Productor)
    ↓ enqueue(ByteBuffer)
[Circular Buffer de 50MB]
    ↑ dequeue(ByteBuffer)
Decoder Thread (Consumidor)
    ↓ enqueue(VideoFrame)
[Circular Buffer de video]
    ↑ dequeue(VideoFrame)
Render Thread (Consumidor final)
```

### 2.3 Frameworking: ImGui vs Qt

**Decisión:** ImGui (MVP) → Qt (fase 2 si tiempo)

**Justificación:**

| Aspecto | ImGui | Qt |
| -------- | --- | --- |
| Curva de aprendizaje | ✅ Plana | ❌ Steep |
| Líneas de código para GUI | ✅ ~200 | ❌ ~1000 |
| Apariencia | ⚠️ Funcional | ✅ Profesional |
| Integración con OpenGL | ✅ Nativa | ⚠️ Posible |
| Tamaño ejecutable | ✅ 10MB | ❌ 100MB+ |
| Tiempo implementación | ✅ 1-2 semanas | ❌ 3+ semanas |

**Enfoque híbrido:** ImGui para MVP (conseguir funcionalidad), luego opcional Qt polish.

### 2.4 Networking: libcurl vs Raw POSIX Sockets

**Decisión:** libcurl (MVP) → Raw sockets (fase 2/bonus)

**Justificación:**

- **libcurl:** Production-ready, HTTPS out-of-box, HTTP pipelining automático
- **Raw sockets:** Más control, demuestra conocimiento networking, es overkill para MVP

**Plan:**

1. MVP: libcurl para funcionamiento rápido
2. Fase 2: Opcional refactor a POSIX sockets bajo libcurl (wrapper architecture)

### 2.5 Testing: GTest vs Catch2

**Decisión:** GTest

**Justificación:**

- Estándar industrial
- Excelente con CMake/Conan
- GitHub Actions integration nativa
- Los recruiters lo reconocen inmediatamente

---

## 3. ESTRUCTURA DE COMPONENTES

### 3.1 Diagrama de Componentes de Alto Nivel

```
┌─────────────────────────────────────────────────────────────────┐
│                     IPTV PLAYER APPLICATION                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌────────────────────────────────────────────────────────┐     │
│  │           PRESENTATION LAYER (GUI)                     │     │
│  │  ┌─ ImGui Window                                       │     │
│  │  │  ├─ Channel List (filterable)                       │     │
│  │  │  ├─ Playback Controls (play, pause, seek)           │     │
│  │  │  ├─ Video Canvas (OpenGL texture)                   │     │
│  │  │  └─ Stats Panel (bitrate, fps, buffer %)            │     │
│  │  └─ Event Loop (60fps)                                 │     │
│  └────────────────────────────────────────────────────────┘     │
│                            △                                     │
│                            │ (commands & state queries)          │
│                            │                                     │
│  ┌────────────────────────────────────────────────────────┐     │
│  │        CONTROL LAYER (State Machine)                   │     │
│  │  ┌─ PlayerController (thread-safe)                     │     │
│  │  │  ├─ State: Stopped, Loading, Playing, Paused       │     │
│  │  │  ├─ Commands: play(), pause(), stop(), seek()      │     │
│  │  │  ├─ Error handling & recovery                       │     │
│  │  │  └─ Metrics aggregation                             │     │
│  │  └─ Observers (channels watch controller state)        │     │
│  └────────────────────────────────────────────────────────┘     │
│         △              △              △              △           │
│         │              │              │              │           │
│   ┌─────┴──────┐  ┌────┴─────┐  ┌───┴──────┐  ┌───┴────────┐  │
│   │   Network  │  │  Decoder  │  │  Render  │  │   Logger   │  │
│   │  Component │  │ Component │  │Component │  │ Component  │  │
│   └──────┬─────┘  └────┬──────┘  └───┬──────┘  └────────────┘  │
│          │             │             │                           │
│   ┌──────▼─────────────▼─────────────▼──────┐                  │
│   │   DATA LAYER (Circular Buffers)          │                  │
│   │  ┌─ NetworkBuffer: raw TS bytes          │                  │
│   │  ├─ VideoFrameBuffer: decoded YUV       │                  │
│   │  └─ AudioFrameBuffer: PCM samples       │                  │
│   │  Sincronización: mutex + condition_var  │                  │
│   └──────────────────────────────────────────┘                  │
│          △              △              △                         │
│          │              │              │                         │
│   ┌──────┴──────┐ ┌────┴─────┐  ┌───┴──────┐                  │
│   │  libcurl    │ │ FFmpeg   │  │ SDL2 +   │                  │
│   │ + OpenSSL   │ │(decode)  │  │ OpenGL   │                  │
│   └─────────────┘ └──────────┘  └──────────┘                  │
│                                                                   │
│  EXTERNAL DEPENDENCIES (third-party libraries)                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Componentes Detallados

#### **A) NETWORK COMPONENT**

**Responsabilidad:**

- Descargar M3U8 playlist
- Parsear canales y URLs
- Descargar segmentos TS vía HTTP GET
- Manejar errores de red, retries, timeouts
- Reportar métricas (Mbps, packet loss, etc.)

**Interfaz Pública:**

```
NetworkComponent:
  - loadPlaylist(url: string) → vector<Channel>
  - downloadSegment(url: string) → ByteBuffer
  - getStats() → NetworkStats
  - stop()
```

**Datos Internos:**

- HTTP client context (libcurl handle)
- Circular buffer output (thread-safe)
- Retry policy (exponential backoff)
- Connection pool (keep-alive)

**Threads:** 1 (Network thread)

**Responsabilidades no explícitas:**

- ✅ Validar URLs (no descargar si URL es mal formada)
- ✅ Timeout después de N segundos
- ✅ Cerrar conexiones no usadas
- ✅ Reportar errores al Controller
- ✅ No bloquear threads otros

---

#### **B) DECODER COMPONENT**

**Responsabilidad:**

- Consumir TS bytes del buffer de red
- Demux (separar video + audio streams)
- Decodificar H.264 → YUV frames
- Decodificar AAC → PCM samples
- Mantener A/V sync

**Interfaz Pública:**

```
DecoderComponent:
  - start(network_buffer: CircularBuffer)
  - getVideoFrame() → VideoFrame (next decoded)
  - getAudioFrame() → AudioFrame (next decoded)
  - getStats() → DecoderStats
  - stop()
```

**Datos Internos:**

- FFmpeg AVFormatContext (demux)
- FFmpeg AVCodecContext (decode video)
- FFmpeg AVCodecContext (decode audio)
- Circular buffers: VideoFrame, AudioFrame
- PTS tracking (presentation timestamps)

**Threads:** 1 (Decoder thread)

**Responsabilidades no explícitas:**

- ✅ Manejar corrupt frames (skip, no crash)
- ✅ Sincronizar audio/video (PTS)
- ✅ Memory management (no leaks en FFmpeg)
- ✅ Graceful shutdown (cleanup FFmpeg contexts)
- ✅ Buffer underrun handling

---

#### **C) RENDER COMPONENT**

**Responsabilidad:**

- Consumir VideoFrame del decoder buffer
- Renderizar YUV → RGB en OpenGL texture
- Consumir AudioFrame, playback via SDL2 audio
- Mantener timing (60 FPS)
- Sincronizar audio/video en presentación

**Interfaz Pública:**

```
RenderComponent:
  - start(video_buffer: CircularBuffer, audio_buffer: CircularBuffer)
  - getStats() → RenderStats
  - requestFrame() → void (pull-based)
  - stop()
```

**Datos Internos:**

- SDL2 Window
- OpenGL Shader program
- OpenGL texture for YUV
- SDL2 AudioDeviceID
- Presentation clock (high-res timer)
- Frame drop counter

**Threads:** 1 (Render thread, runs event loop at 60 FPS)

**Responsabilidades no explícitas:**

- ✅ Drop frames si se atrasa (no acumular lag)
- ✅ Mantener audio playing (más tolerante que video)
- ✅ Respond to window resize (smoothly)
- ✅ Handle pause/resume
- ✅ Graceful degradation (reduce quality if slow)

---

#### **D) CONTROLLER COMPONENT**

**Responsabilidad:**

- Orquestar Network, Decoder, Render threads
- Implementar state machine (Stopped → Loading → Playing → Paused)
- Manejar errores en cualquier thread
- Exponer API al GUI
- Agregar métricas

**Interfaz Pública:**

```
PlayerController:
  - play(channel: Channel) → Status
  - pause() → Status
  - stop() → Status
  - seek(ms: int) → Status
  - getCurrentState() → State
  - getStats() → AggregatedStats
  - registerObserver(observer: PlayerObserver) → void
```

**State Machine:**

```
┌─────────┐
│ STOPPED │ ◄──┐
└────┬────┘    │
     │ play()  │
     ▼         │
┌─────────┐    │
│ LOADING │    │
└────┬────┘    │
     │ (network ready)
     ▼         │
┌─────────┐ pause()
│ PLAYING │─────┐
└────┬────┘     ▼
     │       ┌────────┐
     └──────►│ PAUSED │
             └────┬───┘
                  │ play()
                  └────────────┘
```

**Error Handling:**

- Network error → pause, retry
- Decoder error → skip frame, continue
- Render error → show error message, attempt recovery

---

#### **E) LOGGER COMPONENT**

**Responsabilidad:**

- Log estructurado (spdlog)
- Diferentes niveles (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)
- Thread-safe logging desde threads múltiples
- Output a file + console

**Interfaz Pública:**

```
Logger:
  - debug(msg: string)
  - info(msg: string)
  - warn(msg: string)
  - error(msg: string)
  - (thread-safe singleton)
```

---

### 3.3 Comunicación Entre Componentes

```
GUI                 Controller            Network/Decoder/Render
 │                      │                       │
 ├─ play(channel) ─────►│                       │
 │                      ├─ spawn thread ───────►│
 │                      │  (Network)            │
 │                      │                       ├─ download segments
 │                      │◄─ NetworkStats ──────┤
 │◄─ update UI ─────────┤                       │
 │                      │  (Decoder)            │
 │                      │                       ├─ decode frames
 │                      │◄─ DecoderStats ──────┤
 │◄─ update UI ─────────┤                       │
 │                      │  (Render)             │
 │                      │                       ├─ render + audio
 │                      │◄─ RenderStats ───────┤
 │◄─ update UI ─────────┤                       │
 │                      │                       │
```

**Mecanismo de comunicación:**

- Observer pattern para estado (GUI se suscribe a cambios)
- Buffers thread-safe para data (VideoFrame, AudioFrame)
- Callbacks para errores

---

## 4. PATRONES DE SINCRONIZACIÓN

### 4.1 Circular Buffer Pattern

**Problema:** Network thread produce data rápido, Decoder lo consume lentamente → necesitamos buffer

**Solución:** Circular buffer con mutex

```
Estructura:
  - data: array<T, CAPACITY>
  - write_pos: size_t (write pointer)
  - read_pos: size_t (read pointer)
  - mutex: std::mutex
  - not_empty: std::condition_variable
  - not_full: std::condition_variable

Operaciones:
  - enqueue(item): espera si full, añade, notifica not_empty
  - dequeue(item): espera si empty, saca, notifica not_full
  - size(): retorna items actuales
  - capacity(): retorna tamaño máximo
```

**Tamaños:**

- NetworkBuffer: 50 MB (2-3 segundos @ 20 Mbps)
- VideoFrameBuffer: 300 MB max (10-20 frames HD @ 30fps)
- AudioFrameBuffer: 10 MB (2-3 segundos audio)

### 4.2 Condition Variables para Sincronización

**Casos de uso:**

1. Network produce, Decoder espera → not_empty CV
2. Decoder produce, Render espera → not_empty CV
3. Render consume rápido, Network no puede escribir → not_full CV

**Implementación:**

```cpp
// En circular buffer
while (buffer.full()) {
    not_full.wait(lock);  // Sleep hasta que hay espacio
}
buffer.enqueue(item);
not_empty.notify_one();  // Despierta consumer
```

### 4.3 Thread-Safe State Machine

**Problema:** Múltiples threads leen/escriben state → race condition

**Solución:** Proteger state con mutex

```
PlayerState (guarded by mutex):
  current_state: State
  last_error: string
  is_playing: bool
  current_time_ms: int64_t
```

**API:**

```cpp
void setState(State s) {
    std::lock_guard<std::mutex> lock(state_mutex);
    current_state = s;
    notifyObservers();  // Alert GUI
}

State getState() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return current_state;
}
```

### 4.4 Error Propagation

**Problema:** Error en decoder thread debe llegar a GUI thread

**Solución:** Error queue thread-safe

```
ErrorComponent:
  - post(error: Error) → void
  - poll(error: Error&) → bool (non-blocking)
  - clear()
```

**Flujo:**

```
Decoder catches exception
    ↓
post(Error::CORRUPT_FRAME)
    ↓
Controller checks error queue regularly
    ↓
Updates state to PAUSED, logs error
    ↓
GUI reads state, shows error message
```

---

## 5. FLUJO DE DATOS

### 5.1 Caso: Usuario presiona "Play" en canal

```
Step 1: GUI
  Button clicked → PlayerController::play(Channel)

Step 2: Controller
  - Valida channel URL
  - Crea Network thread con URL
  - Crea Decoder thread (va a esperar en buffer vacío)
  - Crea Render thread (va a esperar en buffer vacío)
  - setState(LOADING)

Step 3: Network Thread
  - conecta a server HTTP
  - descarga .m3u8
  - parsea segmentos
  - comienza a descargar primer segmento
  - enqueue(TS bytes) en NetworkBuffer
  - continúa en loop: descargar segmento, enqueue
  - reporta stats cada 100ms (bitrate, packets)

Step 4: Decoder Thread (despertado por not_empty CV)
  - dequeue(TS bytes) de NetworkBuffer
  - parseByFFmpeg(TS bytes)
  - decodifica video frame
  - decodifica audio sample
  - enqueue(VideoFrame) en VideoBuffer
  - enqueue(AudioFrame) en AudioBuffer
  - loop continúa

Step 5: Render Thread (despertado)
  - Cada 16.67ms (60 FPS):
    - dequeue(VideoFrame) de VideoBuffer
    - renderiza YUV → OpenGL texture
    - dequeue(AudioFrame) de AudioBuffer
    - plays audio via SDL2
    - reports FPS, frame times

Step 6: Controller (loop agrega stats)
  - Cada 500ms:
    - recolecta stats de Network, Decoder, Render
    - calcula estado agregado (buffer %)
    - notifica observadores (GUI)

Step 7: GUI (observador)
  - recibe notificación de cambio de estado
  - actualiza UI (labels, progress bar)
  - muestra video en canvas OpenGL
```

### 5.2 Caso: Network thread falla

```
Network Thread Exception
    ↓
catch(NetworkException)
    ↓
ErrorComponent::post(NETWORK_ERROR)
post(RETRY_ATTEMPT_1)
    ↓
sleep(1000ms) exponential backoff
    ↓
retry HTTP connection
    ↓
If success: continue
If fail after 5 retries:
    ↓
    Controller::setState(PAUSED)
    GUI muestra "Network Error - Retry?"
```

### 5.3 Caso: Usuario hace seek

```
GUI: slider moved to 30 segundos
    ↓
PlayerController::seek(30000 ms)
    ↓
Controller:
    - calcular target segment (segment #10)
    - setState(SEEKING)
    - Network thread saltará a segment #10
    - Decoder thread vaciará buffers (flush)
    - Render thread vaciará buffers
    ↓
Buffers flushed:
    - Network comienza download desde segment #10
    - Decoder espera nuevo TS data
    - Render espera nuevo video frame
    ↓
Nuevo data llega:
    - Controllers setState(PLAYING)
    - GUI actualiza slider

Note: Seeking en streaming vivo es complicado (live no soporta seek)
      → mostrar error "Cannot seek live stream"
```

---

## 6. STACK TECNOLÓGICO JUSTIFICADO

### 6.1 Core Language & Compiler

| Componente | Opción | Justificación |
| --- | --- | --- |
| Lenguaje | C++20 | Modern, std::thread, concepts, ranges |
| Compilador | GCC 11+ / Clang 13+ | Production-grade, sanitizers |
| Standard | C++20 | No C++23/26 (menos widespread) |

### 6.2 Build System

| Componente | Opción | Justificación |
| --- | --- | --- |
| Build | CMake 3.24+ | Standard industrial, Conan integration |
| Package manager | Conan 2.x | reproducible builds, cross-platform |
| Test framework | GTest | Estándar, excelente GitHub Actions |

### 6.3 Media & Networking

| Componente | Opción | Justificación |
| --- | --- | --- |
| Media decoding | FFmpeg 6.x | Production-grade, H.264/AAC support |
| Networking (MVP) | libcurl 8.x | HTTPS out-of-box, HTTP pipelining |
| Networking (Bonus) | POSIX sockets | Low-level, demonstrates knowledge |
| TLS | OpenSSL 3.x | Required for HTTPS, FFmpeg uses it |

### 6.4 GUI & Graphics

| Componente | Opción | Justificación |
| --- | --- | --- |
| GUI (MVP) | ImGui 1.89+ | Fast to implement, OpenGL native |
| Windowing | SDL2 2.28+ | Cross-platform, OpenGL context |
| Graphics | OpenGL 4.6 | Standard, texture rendering simple |
| (Optional) GUI (Polished) | Qt 6.x | Professional look, signals-slots |

### 6.5 Logging & Observability

| Componente | Opción | Justificación |
| --- | --- | --- |
| Logging | spdlog 1.12+ | Fast, structured, thread-safe |
| Metrics | Custom + spdlog | Telemetry básico |

### 6.6 Quality Assurance

| Componente | Opción | Justificación |
| --- | --- | --- |
| Unit Testing | GTest | Standard, GMock support |
| Memory Sanitizer | AddressSanitizer | Catches memory bugs, ASAN+UBSAN |
| Thread Sanitizer | ThreadSanitizer | Race condition detection |
| Static Analysis | clang-tidy | Find code issues early |
| Code Coverage | LCOV + gcov | Target >80% coverage |
| Profile | perf / valgrind | Performance optimization |

### 6.7 CI/CD & Deployment

| Componente | Opción | Justificación |
| --- | --- | --- |
| CI/CD | GitHub Actions | Free, native to GitHub |
| Containerization | Docker | Production deployment, isolation |
| Container Registry | Docker Hub | Free, standard |

### 6.8 Documentation

| Componente | Opción | Justificación |
| --- | --- | --- |
| Code docs | Doxygen | Extract from comments, HTML output |
| README | Markdown + diagrams | GitHub native, accessible |
| Architecture | This document | Design decisions clarity |

---

## 7. ESTRUCTURA DE DIRECTORIOS

```
iptv-player/
├── .github/
│   └── workflows/
│       ├── ci-linux.yml          # Build + test + sanitizers
│       ├── ci-windows.yml        # Cross-platform
│       └── ci-macos.yml          # macOS builds
│
├── src/
│   ├── main.cpp                  # Entry point
│   ├── app/
│   │   ├── player_controller.h   # State machine
│   │   ├── player_controller.cpp
│   │   └── observer.h            # Observer pattern
│   │
│   ├── network/
│   │   ├── network_component.h
│   │   ├── network_component.cpp
│   │   ├── http_client.h
│   │   ├── http_client.cpp
│   │   ├── playlist_parser.h
│   │   └── playlist_parser.cpp
│   │
│   ├── decoder/
│   │   ├── decoder_component.h
│   │   ├── decoder_component.cpp
│   │   ├── ffmpeg_wrapper.h
│   │   ├── ffmpeg_wrapper.cpp
│   │   ├── av_frame.h           # RAII wrapper around AVFrame
│   │   └── av_frame.cpp
│   │
│   ├── render/
│   │   ├── render_component.h
│   │   ├── render_component.cpp
│   │   ├── gl_shader.h
│   │   ├── gl_shader.cpp
│   │   ├── sdl_window.h
│   │   └── sdl_window.cpp
│   │
│   ├── gui/
│   │   ├── imgui_window.h
│   │   ├── imgui_window.cpp
│   │   ├── imgui_impl_*.cpp     # ImGui backends
│   │   └── widgets/
│   │       ├── channel_list.h
│   │       ├── playback_panel.h
│   │       ├── stats_panel.h
│   │       └── video_canvas.h
│   │
│   ├── common/
│   │   ├── circular_buffer.h    # Template circular buffer
│   │   ├── circular_buffer.cpp
│   │   ├── types.h              # Structs (VideoFrame, AudioFrame, etc)
│   │   ├── logger.h
│   │   ├── logger.cpp
│   │   ├── error.h              # Error codes enum
│   │   └── config.h             # Configuration
│   │
│   └── threading/
│       ├── thread_pool.h        # (Optional) Thread pool for multiple networks
│       └── safe_queue.h         # Thread-safe queue wrapper
│
├── include/
│   └── public API headers (re-exports from src/)
│
├── test/
│   ├── CMakeLists.txt
│   ├── network/
│   │   ├── test_playlist_parser.cpp
│   │   ├── test_http_client.cpp
│   │   └── test_network_component.cpp
│   │
│   ├── decoder/
│   │   ├── test_ffmpeg_wrapper.cpp
│   │   ├── test_decoder_component.cpp
│   │   └── fixtures/
│   │       └── sample.ts         # Test data (small TS segment)
│   │
│   ├── render/
│   │   └── test_render_component.cpp
│   │
│   ├── common/
│   │   ├── test_circular_buffer.cpp
│   │   ├── test_logger.cpp
│   │   └── test_types.cpp
│   │
│   └── integration/
│       ├── test_full_pipeline.cpp    # E2E test
│       └── fixtures/
│           └── playlist.m3u8         # Test playlist
│
├── cmake/
│   ├── FindFFmpeg.cmake         # FFmpeg module
│   ├── FindImGui.cmake          # (Or use Conan)
│   ├── Sanitizers.cmake         # Sanitizer options
│   └── ClangTidy.cmake          # Static analysis
│
├── conanfile.txt / conanfile.py # Dependencies (Conan)
├── CMakeLists.txt               # Root CMake
│
├── docker/
│   ├── Dockerfile               # Final image
│   ├── Dockerfile.dev           # Development image
│   └── docker-compose.yml       # Local setup
│
├── docs/
│   ├── ARCHITECTURE.md          # This file
│   ├── BUILDING.md              # How to build
│   ├── API.md                   # Public API reference
│   ├── DESIGN_DECISIONS.md      # Rationale
│   └── diagrams/
│       ├── architecture.png
│       ├── dataflow.png
│       └── state_machine.png
│
├── scripts/
│   ├── download_test_playlist.sh  # Fetch real M3U8 for testing
│   ├── run_sanitizers.sh
│   ├── run_coverage.sh
│   ├── format_code.sh
│   └── setup_dev_env.sh
│
├── .clang-format                # Code style
├── .clang-tidy                  # Linting rules
├── .gitignore
├── README.md                    # Project overview
├── LICENSE                      # MIT or Apache 2.0
└── CONTRIBUTING.md             # Dev guidelines
```

---

## 8. CRITERIOS DE ÉXITO

### 8.1 Technical Success

| Criterio | Métrica |
| --- | --- |
| Build | ✅ `cmake -B build && cmake --build build` compila sin warnings |
| Tests | ✅ `ctest` pasa 100% |
| Memory Safety | ✅ AddressSanitizer clean (0 leaks, 0 errors) |
| Thread Safety | ✅ ThreadSanitizer clean (0 race conditions) |
| Static Analysis | ✅ clang-tidy clean (0 warnings) |
| Code Coverage | ✅ >80% statement coverage (GTest + fixtures) |
| CI/CD | ✅ GitHub Actions verde (Linux, Windows, macOS) |
| Docker | ✅ Image builds, container runs sin errors |

### 8.2 Functional Success

| Criterio | Métrica |
| --- | --- |
| Playback | ✅ Reproduce .m3u8 playlists reales |
| Stability | ✅ 1 hora playback sin crash |
| Performance | ✅ 60 FPS sustained |
| Responsiveness | ✅ UI no freeze en network hiccups |
| Error Recovery | ✅ Network error → retry automático → recovery |
| Metrics | ✅ Mostrar bitrate, FPS, buffer % en tiempo real |

### 8.3 Portfolio Success

| Criterio | Métrica |
| --- | --- |
| GitHub | ✅ README impresiona, stars/forks muestra interés |
| Presentability | ✅ Demostrable en interview (video + live code) |
| Documentation | ✅ Architectural doc clara, decisiones justificadas |
| Code Quality | ✅ Código limpio, comments útiles, no random |
| Completeness | ✅ Todas features promised están implemented |

---

## 9. TIMELINE REALISTA

### 9.1 Phase 1: MVP (Semanas 1-6, 60-80 horas)

**Objetivo:** Functional streaming player with basic UI

**Semana 1-2: Boilerplate & Network (20 horas)**

- Setup GitHub repo, CMake skeleton
- Conanfile.py (dependencies)
- CI/CD skeleton (GitHub Actions)
- NetworkComponent MVP:
  - libcurl integration
  - M3U8 parser (simple regex)
  - TS segment downloader
  - Unit tests for parser
- Deliverable: `cmake -B build && cmake --build build` works

**Semana 3: Decoder Integration (15 horas)**

- FFmpeg wrapper (AVFormatContext, AVCodecContext)
- Demux + decode loop
- RAII wrappers (AVFrame, AVCodecContext cleanup)
- Unit tests for decoding (with sample.ts)
- Deliverable: Can decode sample TS segment

**Semana 4: Render Thread (15 horas)**

- SDL2 window creation
- OpenGL context + shaders
- YUV → RGB rendering
- Audio playback skeleton (SDL_OpenAudioDevice)
- Unit tests for rendering
- Deliverable: Shows color on screen (at least)

**Semana 5: GUI + Integration (15 horas)**

- ImGui window
- Channel list widget
- Play/pause buttons
- Playback canvas
- Stats panel (bitrate, FPS)
- Thread synchronization (circular buffer + CV)
- Deliverable: Press play → video shows up (maybe with buffering issues)

**Semana 6: Stabilization (10 horas)**

- Thread safety hardening
- Error handling
- Retry logic
- AddressSanitizer clean
- Tests passing
- Deliverable: MVP v1.0 ready

**Phase 1 Output:**

- ✅ Playable (simple playlists)
- ✅ Compilable & testable
- ✅ Memory safe (ASan clean)
- ✅ Basic CI/CD working
- NOT: Polished UI, fancy features, performance optimized

---

### 9.2 Phase 2: Production-Ready (Semanas 7-12, 60-80 horas)

**Objetivo:** Professional quality, comprehensive testing, Docker deployment

**Semana 7-8: Advanced Features (20 horas)**

- Seeking support (if stream supports it)
- Adaptive bitrate (switch quality based on network)
- Buffer status tracking
- Audio/video synchronization (PTS-based)
- Error recovery improvements
- Deliverable: Robust against network issues

**Semana 9: Testing & Quality (15 horas)**

- ThreadSanitizer clean (race condition detection)
- Code coverage >80% (add missing unit tests)
- Performance benchmarks (frame decode time, latency)
- Integration tests (full pipeline)
- Deliverable: All sanitizers + coverage targets met

**Semana 10: Deployment (10 horas)**

- Dockerfile (multi-stage)
- docker-compose.yml (local development)
- Cross-platform builds (Linux, Windows, macOS)
- Package for distribution (AppImage, .exe, .dmg)
- Deliverable: `docker build .` + `docker run` works

**Semana 11: Documentation & Polish (10 horas)**

- Complete API documentation (Doxygen)
- Architecture diagrams (draw.io → PNG)
- README with features, screenshots, benchmarks
- Contributing guidelines
- Performance profile (show benchmarks)
- Deliverable: README impresiona a recruiters

**Semana 12: Final Polish & Testing (10 horas)**

- Code review (self), clean up comments
- clang-tidy clean
- Final test run
- Prepare demo (video, live coding)
- Tag release v1.0
- Deliverable: Portfolio-ready project

**Phase 2 Output:**

- ✅ Production-grade code
- ✅ Comprehensive testing
- ✅ Docker ready
- ✅ Impressive README
- ✅ Demo-able

---

### 9.3 Phase 3: Bonus Features (Semanas 13+, optional)

**Si tiempo/energía:**

- Qt GUI polish (professional appearance)
- POSIX sockets rewrite (low-level networking)
- MQTT control (remote playback)
- WebSocket telemetry backend
- Performance optimization (lock-free buffers)
- Kubernetes manifests
- Load testing (simulate many concurrent streams)

---

## 10. RIESGOS Y MITIGACIÓN

| Riesgo | Probabilidad | Impacto | Mitigación |
| --- | --- | --- | --- |
| FFmpeg API complexity | Alta | Alto | Start with simple wrapper, increment complexity |
| Thread synchronization bugs | Media | Alto | Use TSan early, test concurrency heavily |
| Network timeout/retry logic | Media | Medio | Unit test with mocked HTTP failures |
| OpenGL context issues | Baja | Medio | Tested on Linux first, then Windows/macOS |
| Build on multiple platforms | Alta | Medio | Conan handles dependencies, CI tests 3 OSes |
| Real IPTV playlists not available | Baja | Medio | Create test M3U8 + sample TS files locally |
| UI feels amateurish | Alta | Bajo | ImGui sufficient for MVP; Qt polish optional |
| Performance insufficient | Media | Medio | Profiling early, optimize bottleneck thread |
| Scope creep (too ambitious) | Alta | Alto | **Strict phase separation**, MVP first |

---

## 11. PRESENTACIÓN PARA RECRUITERS

### 11.1 GitHub README Structure

```markdown
# IPTV Player — Production-Grade C++20 Streaming Engine

[Hero image: Screenshot of player]

## Overview

A high-performance IPTV player demonstrating professional-grade systems engineering:
- Real-time multithreaded streaming architecture
- FFmpeg media decoding integration
- Zero-copy producer-consumer pattern
- Cross-platform desktop UI (ImGui, future Qt)
- Comprehensive CI/CD with memory/thread sanitizers

## Key Architecture Decisions

[Link to ARCHITECTURE.md]

## Technical Highlights

- **C++20**: Modern C++ (std::thread, ranges, concepts)
- **Concurrency**: 3 coordinated threads, 0 race conditions
- **Media**: FFmpeg H.264/AAC decoding
- **Networking**: libcurl HTTPS, custom POSIX sockets (bonus)
- **Testing**: GTest + fixtures, AddressSanitizer/ThreadSanitizer clean
- **DevOps**: CMake+Conan, Docker, GitHub Actions

## Getting Started

[Build instructions]

## Performance

- Decode latency: <100ms
- Network throughput: 20+ Mbps
- GUI frame rate: 60 FPS (stable)
- Memory: <200MB typical

## Architecture

[Include diagrams here - high-level threads, data flow]

## Testing & Quality

- Unit tests: 50+ tests
- Code coverage: 85%+
- Memory sanitizer: Clean
- Thread sanitizer: Clean
- Static analysis: clang-tidy clean

## Building & Running

[Docker + native instructions]

## Contributing

[Contributing guidelines]

## License

MIT
```

### 11.2 CV Bullet Points (Adaptable por rol)

```
IPTV Streaming Engine — Production-Grade C++20 System

• Architected real-time 3-thread pipeline (network I/O, FFmpeg 
  decode, GPU render) with zero race conditions; ThreadSanitizer validated

• Engineered zero-copy producer-consumer circular buffers for 
  sustained 20+ Mbps streaming with <100ms decode latency

• Integrated complex FFmpeg libraries (libavcodec, libavformat) with 
  rigorous memory safety; AddressSanitizer + Valgrind clean

• Implemented thread-safe state machine (STOPPED→LOADING→PLAYING) 
  with error recovery and metrics aggregation

• Built responsive OpenGL rendering engine maintaining 60 FPS under 
  network congestion; graceful frame dropping

• Designed comprehensive CI/CD (GitHub Actions, sanitizers, coverage 
  >85%) with cross-platform builds (Linux/Windows/macOS)

• Created production-ready Docker image and docker-compose setup

• Technologies: C++20, CMake, Conan, GTest, spdlog, FFmpeg, 
  libcurl, SDL2, OpenGL, Docker
```

---

## 12. SIGUIENTE PASO: IMPLEMENTACIÓN

### 12.1 Pre-implementación

Antes de escribir 1 línea de código:

1. ✅ Crear GitHub repo + initial structure
2. ✅ Setup CMakeLists.txt skeleton
3. ✅ Add conanfile.txt (FFmpeg, libcurl, GTest, etc)
4. ✅ Setup GitHub Actions workflows
5. ✅ Create CONTRIBUTING.md
6. ✅ Merge architecture doc to repo

### 12.2 Implementación

Seguir Phase 1-2 timeline estrictamente. **No scope creep.**

### 12.3 Validación

- Every commit passes CI
- Every PR has tests
- Every major change documented

---

## CONCLUSIÓN

Este proyecto demuestra capabilidad en:

✅ **Modern C++** (C++20 features, RAII, STL)  
✅ **Concurrency** (threads, mutex, condition variables, no race conditions)  
✅ **Systems Engineering** (networking, media, performance)  
✅ **Software Architecture** (modular design, observer pattern, state machine)  
✅ **Quality Assurance** (testing, sanitizers, CI/CD)  
✅ **DevOps** (Docker, CMake, cross-platform builds)  
✅ **Professional Standards** (docs, code review, error handling)  

**Es un proyecto que los recruiters reconocen como "verdadero engineering", no ejercicios.**

---

**Versión:** 1.0  
**Última actualización:** Septiembre 2026  
**Status:** Listo para implementación ✅
