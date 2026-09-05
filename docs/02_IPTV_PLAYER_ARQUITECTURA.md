# 🏗️ IPTV PLAYER - ARQUITECTURA COMPLETA

> **Fase 0: Diseño Puro** | Solo arquitectura, cero código | Semana 1 de planificación

---

## 📋 ÍNDICE

1. [Visión General](#visión-general)
2. [Análisis de Requisitos](#análisis-de-requisitos)
3. [Arquitectura de Sistema](#arquitectura-de-sistema)
4. [Diseño de Componentes](#diseño-de-componentes)
5. [Patrones de Comunicación](#patrones-de-comunicación)
6. [Estrategia de Testing](#estrategia-de-testing)
7. [DevOps & CI/CD](#devops--cicd)
8. [Plan de Desarrollo (Fases)](#plan-de-desarrollo-fases)
9. [Riesgos & Mitigación](#riesgos--mitigación)
10. [Stack Técnico Definido](#stack-técnico-definido)

---

## 🎯 VISIÓN GENERAL

### **Objetivo Principal**
Construir un **IPTV Player production-grade en C++20** que demuestre:
- Arquitectura multithreaded real
- Integración con librerías complejas (FFmpeg)
- Networking robusto (HTTP, TLS)
- GUI responsiva
- DevOps completo (Docker, CI/CD, testing, monitoring)

### **Por qué este proyecto es FUERTE para portfolio**

| Factor | Valor |
|--------|-------|
| **Complejidad técnica** | Alta (3 threads, sincronización, media handling) |
| **Visibilidad** | Alta (GUI funcional, demo-able) |
| **Relevancia profesional** | Altísima (streaming es industria $B) |
| **Profundidad vs Anchura** | Profundidad (1 producto robusto vs 5 ejercicios) |
| **Curva de aprendizaje** | Natural (network → decode → render) |
| **Appeal a recruiters** | "Puedo construir sistemas reales" |

---

## 📊 ANÁLISIS DE REQUISITOS

### **REQUISITOS FUNCIONALES**

#### **RF1: Reproducción de Streams IPTV**
- **Qué:** Reproducir video en tiempo real desde URL de playlist IPTV (.m3u8)
- **Cómo:** HTTP GET → m3u8 parsing → descargar segmentos .ts → decode → render
- **Éxito:** Video fluyendo en pantalla sin buffering visible

#### **RF2: Control de Reproducción**
- **Play/Pause:** Parar/reanudar descarga y decode
- **Stop:** Limpiar buffers, detener todos los threads
- **Seek:** Saltar a otro punto (avanzado, fase 2)
- **Interfaz:** Botones GUI + teclado shortcuts

#### **RF3: Visualización de Canales**
- **Listado:** Mostrar todos los canales del m3u8
- **Búsqueda:** Filter por nombre
- **Selección:** Click para cambiar canal
- **Feedback:** Indicación visual de canal actual

#### **RF4: Estadísticas en Tiempo Real**
- **Metrics mostradas:**
  - Bitrate actual (Mbps)
  - FPS (frames por segundo)
  - Resolución (width x height)
  - Buffers state (network, decode, render)
  - Latencia (network → display)

#### **RF5: Manejo de Errores Gracioso**
- **Red:** Reintentos automáticos, timeout handling
- **Decode:** Skip de frames corruptos, logging detallado
- **Render:** Fallback a resolución menor si GPU saturada
- **UI:** Notificaciones al usuario

---

### **REQUISITOS NO-FUNCIONALES**

#### **RNF1: Performance**
- Decode latency: < 200ms desde descarga a pantalla
- Render: 60 FPS estable
- Network: 10+ Mbps sustained
- Memory: < 300MB durante playback

#### **RNF2: Confiabilidad**
- Cero memory leaks (validado con ASan)
- Cero race conditions (validado con ThreadSanitizer)
- Code coverage > 80%
- MTBF (Mean Time Between Failures) > 1 hora

#### **RNF3: Escalabilidad**
- Manejar streams 720p, 1080p, 4K
- Adaptive bitrate (seleccionar mejor variante)
- Múltiples conexiones simultáneas

#### **RNF4: Usabilidad**
- UI responsiva (no freeza con red lenta)
- Cross-platform (Linux, Windows, macOS)
- Docker-deployable

#### **RNF5: Mantenibilidad**
- Código limpio, bien documentado
- Componentes desacoplados
- Fácil de testear unitariamente
- CI/CD automatizado

---

## 🏗️ ARQUITECTURA DE SISTEMA

### **NIVEL 1: CAPAS HORIZONTALES**

```
┌─────────────────────────────────────────────────────────────┐
│                    PRESENTATION LAYER                       │
│                    (GUI / CLI Interface)                     │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│                   BUSINESS LOGIC LAYER                       │
│            (Controller, State Management, Orchestration)     │
└──────────┬──────────────┬──────────────┬────────────────────┘
           │              │              │
┌──────────▼────┐ ┌──────▼──────┐ ┌────▼─────────┐
│  NETWORK      │ │  DECODER    │ │   RENDER     │
│  SUBSYSTEM    │ │  SUBSYSTEM  │ │   SUBSYSTEM  │
└──────────┬────┘ └──────┬──────┘ └────┬─────────┘
           │              │              │
┌──────────▼──────────────▼──────────────▼─────────────────────┐
│                 DATA LAYER                                   │
│         (Circular Buffers, Queues, Shared Memory)           │
└──────────────────────────────────────────────────────────────┘
           │              │              │
┌──────────▼──────────────▼──────────────▼─────────────────────┐
│              INFRASTRUCTURE LAYER                            │
│  (Logging, Config, Telemetry, Error Handling, Monitoring)  │
└──────────────────────────────────────────────────────────────┘
```

---

### **NIVEL 2: ARQUITECTURA CONCURRENTE**

```
                        MAIN THREAD (GUI Loop)
                               │
                    ┌──────────┼──────────┐
                    │          │          │
              [Network]    [Decoder]  [Render]
              Thread         Thread    Thread
                    │          │          │
              ┌─────▼──────────▼──────────▼────┐
              │   CIRCULAR BUFFERS + QUEUES    │
              │  (Thread-Safe Communication)   │
              └────────────────────────────────┘
```

**Características:**
- Main thread: GUI y state management
- 3 worker threads: I/O intensivo (network), CPU intensivo (decode), GPU intensivo (render)
- Comunicación: Lock-free queues / circular buffers
- Sincronización: Condition variables para coordinar threads
- Shutdown ordenado: Graceful termination de todos los threads

---

### **NIVEL 3: FLUJO DE DATOS**

```
┌─────────────────────────────────────────────────────────────┐
│ 1. USER SELECTS CHANNEL VIA GUI                             │
└──────────────┬──────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────┐
│ 2. CONTROLLER BROADCASTS "PLAY(channel)" TO NETWORK THREAD  │
└──────────────┬──────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────┐
│ 3. NETWORK THREAD:                                          │
│    - Download .m3u8 playlist                                │
│    - Start downloading .ts segments in sequence             │
│    - Push ByteBuffer to NetworkOutputQueue                  │
└──────────────┬──────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────┐
│ 4. DECODER THREAD:                                          │
│    - Pop ByteBuffer from NetworkOutputQueue                 │
│    - Demux .ts file (extract H.264 + AAC streams)          │
│    - Decode H.264 frames → VideoFrame                       │
│    - Decode AAC samples → AudioFrame                        │
│    - Push VideoFrame → VideoOutputQueue                     │
│    - Push AudioFrame → AudioOutputQueue                     │
│    - Update stats (fps, resolution)                         │
└──────────────┬──────────────────────────────────────────────┘
               │
        ┌──────┴─────┐
        │             │
┌───────▼────┐  ┌────▼──────┐
│  5. RENDER  │  │  AUDIO    │
│  THREAD     │  │  THREAD   │
│  - Consume  │  │  - Consume│
│    VideoFr  │  │    AudioFr│
│  - Render   │  │  - Queue  │
│    to GPU   │  │    audio  │
│  @ 60fps    │  │  samples  │
└─────┬──────┘  └────┬──────┘
      │              │
      └──────┬───────┘
             │
┌────────────▼──────────────────────────────────────────────┐
│ 6. DISPLAY + AUDIO OUTPUT TO USER                        │
└───────────────────────────────────────────────────────────┘
```

---

## 🔧 DISEÑO DE COMPONENTES

### **COMPONENTE 1: NETWORK SUBSYSTEM**

#### **Responsabilidades Clave**
1. **Playlist Parsing**
   - Descargar URL m3u8
   - Parsear formato (extUSIG, ext-x-stream-inf, etc)
   - Extraer lista de canales con URLs
   - Validar URLs (formato, TLS support)

2. **Segment Downloading**
   - HTTP GET individual .ts files
   - Respeto del timing (no all-at-once)
   - Reintentos automáticos (exponential backoff)
   - Timeout handling (máx N segundos por segment)

3. **TLS/HTTPS Support**
   - Certificate validation
   - Cipher suite negotiation
   - Certificate pinning (opcional avanzado)

4. **Metrics Collection**
   - Bytes descargados
   - Segmentos completados/fallidos
   - Current bandwidth (Mbps)
   - Packet loss estimado

#### **Interfaces Principales**
```
NetworkSubsystem:
  - start() → void
  - stop() → void
  - play(channel_url: string) → void
  - pause() → void
  - getNetworkOutputQueue() → queue<ByteBuffer>
  - getStats() → NetworkStats
```

#### **Datos de Entrada**
- Channel URL (from GUI/Controller)
- Retry policy (max attempts, backoff strategy)
- Bandwidth limits (optional throttling)

#### **Datos de Salida**
- ByteBuffer queue (to decoder)
- NetworkStats (to GUI)
- Error events (to controller)

#### **Decisiones de Diseño Clave**
- **Buffer Size:** 2-3 segmentos en memoria (típicamente 10-50 MB)
- **Download Strategy:** Sliding window (descargar siguiente mientras se decodifica actual)
- **Error Recovery:** Reintentos con backoff exponencial, timeout 10s por segmento
- **Thread Model:** Single thread (I/O bound, waiting on network)

#### **Desafíos Técnicos**
- ⚠️ HTTP pipelining (mantener conexión abierta)
- ⚠️ Handling redirects (m3u8 puede apuntar a otras URLs)
- ⚠️ Time-based segments (algunos streams son en vivo)
- ⚠️ Region blocking / GeoIP restrictions
- ⚠️ Rate limiting (ser respetuoso con servidores)

---

### **COMPONENTE 2: DECODER SUBSYSTEM**

#### **Responsabilidades Clave**
1. **Demuxing**
   - Consumir ByteBuffer (contiene .ts file)
   - Identificar streams (video, audio, subtitles)
   - Extraer H.264 NAL units (video)
   - Extraer AAC frames (audio)

2. **Video Decoding**
   - H.264 → raw YUV frames
   - Format conversion (YUV420p → RGBA if needed)
   - Maintain frame rate (timestamps)
   - Handle IDR frames (key frames for seeking)

3. **Audio Decoding**
   - AAC → PCM samples
   - Resampling (si needed, e.g., 44.1kHz → 48kHz)
   - Audio sync (timestamps aligned with video)

4. **Error Handling**
   - Skip corrupt frames
   - Resync on stream corruption
   - Graceful degradation

#### **Interfaces Principales**
```
DecoderSubsystem:
  - start() → void
  - stop() → void
  - getNetworkInputQueue() → queue<ByteBuffer>  [input]
  - getVideoOutputQueue() → queue<VideoFrame>   [output]
  - getAudioOutputQueue() → queue<AudioFrame>   [output]
  - getStats() → DecoderStats
```

#### **Datos de Entrada**
- ByteBuffer queue (from network)

#### **Datos de Salida**
- VideoFrame queue (to render)
- AudioFrame queue (to audio output)
- DecoderStats (fps, resolution, decoded count)

#### **Decisiones de Diseño Clave**
- **FFmpeg Context:** Una por thread, reutilizada entre segmentos
- **Memory Strategy:** Pre-allocate frame buffers (circular pool)
- **Queue Depth:** 5-10 frames buffered (balance latency vs robustness)
- **Thread Model:** Single thread (CPU bound, heavy decode work)

#### **Desafíos Técnicos**
- ⚠️ FFmpeg memory management (leaks, crashes)
- ⚠️ Stream format variations (diferentes codecs, resolutions)
- ⚠️ Timing discontinuities (.ts segments may have PTS gaps)
- ⚠️ A/V sync (audio y video deben estar sincronizados)
- ⚠️ Performance (decode must be faster than playback speed)

---

### **COMPONENTE 3: RENDER SUBSYSTEM**

#### **Responsabilidades Clave**
1. **Video Rendering**
   - Consume VideoFrame queue
   - Upload texture to GPU (OpenGL)
   - Render quad con frame actual
   - Maintain 60 FPS timing
   - Handle resolution changes

2. **Audio Playback**
   - Consume AudioFrame queue
   - Queue samples to SDL Audio device
   - Maintain audio callback synchronization
   - Handle underruns / overruns

3. **Timing & Synchronization**
   - VSync-locked rendering (60 FPS)
   - PTS-based frame timing
   - A/V sync (audio clock as reference)
   - Frame drop strategy (si decode es lento)

#### **Interfaces Principales**
```
RenderSubsystem:
  - start() → void
  - stop() → void
  - getVideoInputQueue() → queue<VideoFrame>  [input]
  - getAudioInputQueue() → queue<AudioFrame>  [input]
  - getStats() → RenderStats
```

#### **Datos de Entrada**
- VideoFrame queue (from decoder)
- AudioFrame queue (from decoder)

#### **Datos de Salida**
- Window + rendered video (visual output)
- Audio through speakers
- RenderStats (fps, dropped frames, audio latency)

#### **Decisiones de Diseño Clave**
- **Rendering Backend:** OpenGL 4.5 (modern, cross-platform)
- **Audio Backend:** SDL2 Audio (simple, funciona everywhere)
- **Target FPS:** 60 (o vSync del monitor)
- **Thread Model:** Renderiza en main thread (GUI + render)
- **Frame Dropping:** Inteligente (drop late frames, keep recent)

#### **Desafíos Técnicos**
- ⚠️ OpenGL context affinity (solo el thread que crea context puede usarlo)
- ⚠️ Audio underrun prevention (audio callback may be real-time)
- ⚠️ Variable resolution streams (resolution changes mid-playback)
- ⚠️ Frame rate mismatch (stream @ 24fps, monitor @ 60fps)
- ⚠️ Latency minimization (debe verse "en vivo")

---

### **COMPONENTE 4: CONTROLLER (Orquestación)**

#### **Responsabilidades Clave**
1. **State Management**
   - Estados: IDLE, CONNECTING, PLAYING, PAUSED, STOPPED, ERROR
   - Transiciones válidas
   - Invariants (e.g., no puede estar PLAYING + PAUSED)

2. **User Command Handling**
   - Play(channel)
   - Pause()
   - Stop()
   - Seek(time) [fase 2]
   - SetVolume(level)
   - Comandos desde GUI thread-safe

3. **Subsystem Orchestration**
   - Start/stop network thread
   - Start/stop decoder thread
   - Coordinate render thread
   - Handle subsystem failures

4. **Event Broadcasting**
   - State changes → GUI
   - Error events → logging, GUI
   - Stats updates → GUI
   - Graceful shutdown sequence

#### **Interfaces Principales**
```
Controller:
  - playChannel(url: string) → void
  - pause() → void
  - stop() → void
  - seek(position_ms: int64) → void [future]
  - getState() → PlaybackState
  - getStats() → AggregateStats (network + decoder + render)
  - onChannelSelected(channel: Channel) → callback
  - onStateChanged(listener) → observer pattern
```

#### **State Machine**

```
┌─────┐
│IDLE │
└──┬──┘
   │ playChannel()
   ▼
┌─────────────┐
│CONNECTING   │ ◄─── Network thread starts, downloads m3u8
└──┬──────────┘
   │ Playlist loaded
   ▼
┌─────────────┐
│PLAYING      │ ◄─── All 3 threads running, video flowing
└──┬──────────┘
   │ pause()
   ▼
┌─────────────┐
│PAUSED       │ ◄─── Network paused, decoder/render wait
└──┬──────────┘
   │ play() → back to PLAYING
   │ stop()  → STOPPED
   ▼
┌─────────────┐
│STOPPED      │ ◄─── All threads shutdown, buffers cleared
└──┬──────────┘
   │ playChannel() → back to CONNECTING
   ▼
```

---

### **COMPONENTE 5: GUI SUBSYSTEM**

#### **Responsabilidades Clave**
1. **Channel Management UI**
   - List all channels from current playlist
   - Search/filter
   - Highlight current channel
   - Double-click to play

2. **Playback Controls**
   - Play, Pause, Stop buttons
   - Volume slider
   - Visual feedback (playing, paused, buffering, error)

3. **Statistics Display**
   - Real-time bitrate (Mbps)
   - FPS
   - Resolution
   - Decoder queue depth
   - Network queue depth

4. **Video Display**
   - SDL2 window with OpenGL rendering
   - Responsive to resize
   - Full-screen mode (optional)

#### **Technology Choice: ImGui vs Qt**

| Aspecto | ImGui | Qt |
|---------|-------|-----|
| **Aprendizaje** | Rápido (immediate-mode) | Lento (signal-slot) |
| **Render latency** | Bajo | Medio |
| **Polish** | Funcional | Profesional |
| **Effort para MVP** | ~2 weeks | ~4 weeks |
| **Final result** | Playable | Polished |
| **Para portfolio** | Suficiente | Mejor impresión |

**DECISIÓN:** ImGui para MVP (fase 1), posible Qt upgrade en fase 2

---

## 🔗 PATRONES DE COMUNICACIÓN

### **PATRÓN 1: THREAD-SAFE QUEUES**

**Uso:** Comunicación entre threads (network → decoder, decoder → render)

```
┌──────────────┐          ┌────────────────┐          ┌──────────────┐
│ Network      │  .push() │ CircularBuffer │ .pop()   │ Decoder      │
│ Thread       │─────────►│ (Lock-free)    │─────────►│ Thread       │
└──────────────┘          └────────────────┘          └──────────────┘
```

**Implementación:**
- Lock-free queue (moodycamel::ConcurrentQueue) O manual con mutex + condition_variable
- Buffer circular pre-allocado (zero allocation durante playback)
- Size límite (backpressure if decoder slow)

**Ventajas:**
- Desacoplamiento total entre threads
- Timing-isolated (cada thread a su ritmo)
- Easy debugging (inspeccionar queue sizes)

---

### **PATRÓN 2: CONDITION VARIABLES (Coordinación)**

**Uso:** Sincronizar acciones entre threads (e.g., "pause" debe esperar que todos los threads se detengan)

```cpp
Controller quiere pausar:
  1. Set state = PAUSED
  2. Notify network thread (stop downloading)
  3. Notify decoder thread (stop decoding)
  4. Wait for condition_variable (cuando ambos ready)
  5. Confirm pause a GUI
```

**Implementación:**
- mutex + condition_variable per subsystem
- Cada thread reports readiness (paused, stopped, etc)
- Controller waits for all confirmations

---

### **PATRÓN 3: OBSERVER PATTERN (Events)**

**Uso:** GUI escucha eventos del controller (state changes, stats updates)

```
Controller:
  - on_state_changed(listener) → register callback
  - on_stats_updated(listener) → register callback
  
GUI:
  - Callback triggered → update widgets
  - Safe from main thread (Qt signals/slots o async callback)
```

---

## 🧪 ESTRATEGIA DE TESTING

### **NIVEL 1: UNIT TESTS (Componentes Aislados)**

#### **Network Subsystem Tests**
- Parsear m3u8 válido → extraer canales correctos
- Parsear m3u8 inválido → error handling
- Download con timeout → reintentos
- TLS handshake failure → error
- HTTP redirects → seguir
- Bandwidth calculation → valores razonables

#### **Decoder Subsystem Tests**
- Decodificar .ts válido → frames correctos
- Manejo de frames corruptos → skip sin crash
- Cambio de resolución → buffer resizing
- Memory leaks con FFmpeg → ASan clean
- Timestamp handling → sincronización correcta

#### **Render Subsystem Tests**
- Frame upload a GPU → sin crashes
- Audio buffering → sin underruns
- Resolution change → adaptive rendering
- FPS stability → 60fps guaranteed
- Window resize → layout adjustment

#### **Controller Tests**
- State transitions válidas → permitidas
- State transitions inválidas → rechazadas
- Play → pause → stop → play → secuencia correcta
- Error handling → graceful degradation
- Concurrent commands → thread-safe

### **NIVEL 2: INTEGRATION TESTS**

#### **End-to-End Scenarios**
1. Start application → IDLE state
2. Select channel → CONNECTING state
3. Network downloads m3u8 → OK
4. First segment downloaded → decoder starts
5. Decoder produces frame → render displays
6. Final state: PLAYING with video flowing

#### **Stress Tests**
- Play 10 hours continuously → no memory leaks
- Switch channels 100 times → proper cleanup
- Pause/resume rapidly → no deadlocks
- Network failures mid-stream → recovery

#### **Performance Tests**
- Decode latency < 200ms
- Render at 60 FPS stable
- Network throughput 10+ Mbps
- Memory < 300MB typical

### **NIVEL 3: SANITIZER VALIDATION**

- **AddressSanitizer (ASan):** Detecta memory leaks, heap overflows
- **ThreadSanitizer (TSan):** Detecta race conditions, data races
- **Valgrind:** Análisis de memory detallado (optional, slower)

**Criterio de éxito:** Zero findings de ASan/TSan en CI/CD

### **NIVEL 4: MANUAL TESTING**

- Play real IPTV streams (YouTube Live, Twitch, etc)
- Test con diferentes resolutions (360p, 720p, 1080p)
- Test con bandwidths variables (simular slow network)
- Validate visual quality + audio sync

---

## 🚀 DEVOPS & CI/CD

### **REPOSITORY STRUCTURE**

```
iptv-player/
├── README.md                      # Project overview
├── ARCHITECTURE.md                # This document
├── CMakeLists.txt                 # Build config
├── conanfile.txt                  # Dependency management
├── Dockerfile                     # Docker image
├── docker-compose.yml             # Local dev environment
│
├── .github/workflows/
│   ├── build.yml                  # Build on push
│   ├── test.yml                   # Run tests
│   ├── sanitizers.yml             # ASan/TSan checks
│   ├── coverage.yml               # Code coverage report
│   └── docker-push.yml            # Push to Docker Hub
│
├── src/
│   ├── network/
│   │   ├── network_subsystem.h
│   │   ├── network_subsystem.cpp
│   │   ├── playlist_parser.h
│   │   └── ...
│   ├── decoder/
│   │   ├── decoder_subsystem.h
│   │   ├── decoder_subsystem.cpp
│   │   └── ...
│   ├── render/
│   │   ├── render_subsystem.h
│   │   ├── render_subsystem.cpp
│   │   └── ...
│   ├── controller/
│   │   ├── controller.h
│   │   ├── controller.cpp
│   │   └── ...
│   ├── gui/
│   │   ├── gui_manager.h
│   │   ├── gui_manager.cpp
│   │   └── ...
│   ├── common/
│   │   ├── queue.h                # Thread-safe queue
│   │   ├── logging.h              # Logging infrastructure
│   │   ├── config.h               # Configuration
│   │   └── types.h                # Shared types
│   └── main.cpp
│
├── tests/
│   ├── unit/
│   │   ├── network_tests.cpp
│   │   ├── decoder_tests.cpp
│   │   ├── render_tests.cpp
│   │   ├── controller_tests.cpp
│   │   └── ...
│   ├── integration/
│   │   ├── end_to_end_tests.cpp
│   │   └── stress_tests.cpp
│   └── CMakeLists.txt
│
├── docs/
│   ├── ARCHITECTURE.md            # Detailed design docs
│   ├── BUILD.md                   # Building instructions
│   ├── TESTING.md                 # Testing guide
│   └── DEPLOYMENT.md              # Deployment guide
│
├── config/
│   ├── default.yaml               # Default configuration
│   └── development.yaml           # Dev overrides
│
└── third_party/
    └── [vendored dependencies if needed]
```

### **BUILD PIPELINE**

```
┌──────────────────────────────────────────────────────────┐
│ 1. PUSH TO GITHUB (or PR)                               │
└──────────────────┬───────────────────────────────────────┘
                   │
        ┌──────────┴──────────┬────────────┬──────────────┐
        │                     │            │              │
┌───────▼────────┐ ┌──────────▼────┐ ┌────▼────────┐ ┌──▼────────┐
│  BUILD JOB     │ │  TEST JOB     │ │ SANITIZER   │ │ COVERAGE  │
│  cmake build   │ │  ctest        │ │ ASan/TSan   │ │ gcov      │
│  Linux/Win/Mac │ │  all tests    │ │ clean build │ │ codacy    │
└───────┬────────┘ └──────────┬────┘ └────┬────────┘ └──┬────────┘
        │                     │            │             │
        └──────────┬──────────┴────────────┴─────────────┘
                   │
        ┌──────────▼─────────────────────────────┐
        │ ALL CHECKS PASS?                      │
        └──────────┬──────────────────────────────┘
                   │ YES
        ┌──────────▼─────────────────────────────┐
        │ DOCKER IMAGE BUILD & PUSH              │
        │ (tag: commit-sha, latest)              │
        └──────────┬──────────────────────────────┘
                   │
        ┌──────────▼─────────────────────────────┐
        │ RELEASE? CREATE GITHUB RELEASE         │
        │ Attach binaries                        │
        └───────────────────────────────────────┘
```

### **CI/CD CHECKS**

| Check | Tool | Frekuencia | Criterio Éxito |
|-------|------|------------|----------------|
| Build | CMake | Per push | Compile exitoso |
| Unit Tests | GTest | Per push | 100% pass |
| Integration Tests | Custom | Per push | 100% pass |
| AddressSanitizer | Clang ASan | Per push | Zero findings |
| ThreadSanitizer | Clang TSan | Per push | Zero findings |
| Code Coverage | gcov/lcov | Daily | > 80% |
| Static Analysis | clang-tidy | Per push | < N issues |
| Code Format | clang-format | Per push | Formatting OK |
| Dependency Check | Conan | Weekly | No vulnerabilities |

---

## 📅 PLAN DE DESARROLLO (FASES)

### **FASE 0: SETUP (Semana 1) - SIN CODING**

**Objetivo:** Arquitectura definida, estructura lista, cero código

**Tareas:**
- ✅ Finalizar documento de arquitectura (ESTE)
- ✅ Definir estructura de directorios
- ✅ Crear GitHub repo skeleton
- ✅ Setup CMake básico
- ✅ Setup Conan para deps
- ✅ Design database de tipos (ByteBuffer, VideoFrame, etc)
- ✅ Definir interfaces (headers) sin implementación

**Deliverables:**
- GitHub repo con estructura
- CMakeLists.txt que compila (empty targets)
- conanfile.txt con todas las deps
- Headers con interfaces principales
- ARCHITECTURE.md documento final
- Roadmap detallado

**Éxito:** `cmake -B build && cmake --build build` compila sin errores

---

### **FASE 1: MVP (Semanas 2-5) - FUNCIONALIDAD BÁSICA**

**Objetivo:** Sistema funcional end-to-end jugando IPTV

#### **Sprint 1.1: Network Subsystem (1.5 weeks)**
- Implement HTTP client (libcurl)
- M3U8 parsing
- Segment downloading loop
- Basic retry logic
- Thread-safe queue output

**Testing:** Unit tests + manual with real m3u8

#### **Sprint 1.2: Decoder Subsystem (1.5 weeks)**
- FFmpeg context setup
- Demux .ts files
- H.264 decode
- AAC decode
- Thread-safe queue management

**Testing:** Unit tests + decode actual segments

#### **Sprint 1.3: Render Subsystem (1 week)**
- SDL2 window creation
- OpenGL texture upload
- Render loop @ 60fps
- Basic audio playback
- Window resize handling

**Testing:** Render decoded frames + play audio

#### **Sprint 1.4: Controller + GUI (1 week)**
- State machine implementation
- Thread orchestration
- ImGui channel list + controls
- Connect controller to GUI
- Basic stats display

**Testing:** Full end-to-end: select channel → video plays

**Deliverables:**
- Playable IPTV player (may be rough)
- All unit tests passing
- 70%+ code coverage
- GitHub Actions CI/CD working
- Dockerfile builds

**Success Metrics:**
- Play real IPTV stream
- Display video and audio
- UI responsive (no freezes)
- No crashes in 30-minute session
- ASan/TSan clean

---

### **FASE 2: PRODUCTION-READY (Semanas 6-10) - ROBUSTEZ + POLISH**

#### **Sprint 2.1: Error Handling & Recovery (1 week)**
- Network failure handling + retries
- Decoder error handling (skip corrupt frames)
- Graceful degradation
- Detailed error logging
- User notifications (UI)

#### **Sprint 2.2: Performance & Optimization (1 week)**
- Profile decode latency (goal < 200ms)
- Optimize FFmpeg context usage
- Zero-copy optimizations
- Memory pool pre-allocation
- Bandwidth adaptive streaming (select best variant)

#### **Sprint 2.3: Advanced Features (1.5 weeks)**
- Seeking support
- A/V sync refinement
- Buffer depth indicators
- Quality selection UI
- Aspect ratio / scaling

#### **Sprint 2.4: Quality & Testing (1.5 weeks)**
- Expand unit test coverage → > 85%
- Stress tests (24h playback, 100+ channel switches)
- Performance benchmarks
- Security audit (TLS validation, input sanitization)
- Documentation (BUILD.md, API docs, design docs)

#### **Sprint 2.5: Deployment & Polish (1 week)**
- Docker image optimization
- Docker-compose for dev
- Cross-platform testing (Linux, macOS, Windows)
- Package binaries for each OS
- README + showcase videos

**Deliverables:**
- Production-ready IPTV player
- 85%+ code coverage
- Zero ASan/TSan findings
- Docker image
- Comprehensive documentation
- Performance benchmarks

**Success Metrics:**
- Sustained 20+ Mbps streaming
- Decode latency < 100ms
- 60 FPS rendering guarantee
- Memory < 300MB
- Uptime > 10 hours without issues

---

### **FASE 3: PORTFOLIO POLISH (Semana 11-12) - PRESENTATION**

#### **Sprint 3.1: Documentation & Showcase**
- Professional README
- Architecture diagrams (ASCII art + SVG)
- Setup instructions
- Deployment guide
- Feature list

#### **Sprint 3.2: GitHub & Demo**
- Organize issues (bug tracking)
- Create project board (kanban)
- Tag releases (v1.0)
- Demo video (5min walkthrough)
- Performance benchmark report

#### **Sprint 3.3: Portfolio Optimization**
- Tailor README for each job type
- Extract testimonials / metrics
- Create "lessons learned" post
- Blog post: "Building an IPTV Player in C++20"
- LinkedIn post

**Deliverables:**
- Showcase-ready GitHub repo
- Demo video
- Metrics report
- Blog post(s)

---

## ⚠️ RIESGOS & MITIGACIÓN

### **RIESGO 1: FFmpeg Complexity**

**Problema:** FFmpeg es librería compleja, fácil memory leaks, crashes
**Probabilidad:** Alta
**Impacto:** Alto (puede bloquear todo proyecto)

**Mitigación:**
- Fase 0: Investigar FFmpeg API en profundidad
- Encapsular FFmpeg en wrapper seguro (RAII, unique_ptr)
- Unit tests tempranos con segmentos de prueba
- ASan/TSan desde día 1
- Considerar usar ffmpeg CLI como fallback (si FFmpeg API fallido)

---

### **RIESGO 2: Thread Synchronization Issues**

**Problema:** Race conditions, deadlocks, data corruption
**Probabilidad:** Media-Alta
**Impacto:** Alto (produtcion killer)

**Mitigación:**
- Usar lock-free structures donde posible
- ThreadSanitizer CI/CD checks
- Minimize shared state (copiar datos en vez de compartir)
- Stress testing temproano
- Code review especialmente de threading code

---

### **RIESGO 3: Network Resilience**

**Problema:** Network failures, timeouts, malformed m3u8
**Probabilidad:** Media
**Impacto:** Medio (user sees errors)

**Mitigación:**
- Robust m3u8 parser (handle variants)
- Retry logic con exponential backoff
- Timeout handling (10s max per segment)
- Unit tests con network mocking
- Real-world testing con slow networks

---

### **RIESGO 4: Scope Creep**

**Problema:** Agregar features en exceso, proyecto nunca termina
**Probabilidad:** Media
**Impacto:** Medio (timeline slip)

**Mitigación:**
- MVP scope bien definido (este documento)
- Phase-based approach (MVP first, polish second)
- "No feature" por defecto (only if planned)
- Weekly checkins (scope review)

---

### **RIESGO 5: Performance Not Acceptable**

**Problema:** Decode too slow, latency > 200ms, FPS drops
**Probabilidad:** Baja-Media
**Impacto:** Alto (producto unusable)

**Mitigación:**
- Performance budgets claros (< 200ms decode latency)
- Profile early and often
- Benchmark comparisons
- Fallback strategies (lower resolution, drop frames)
- Pre-testing con representativo streams

---

## 🛠️ STACK TÉCNICO DEFINIDO

### **LENGUAJE & COMPILACIÓN**

| Componente | Decisión | Justificación |
|-----------|----------|---------------|
| Lenguaje | C++20 | Modern, efficient, portfolio impact |
| Standard | C++20 | Latest features (coroutines, concepts) |
| Compiler | GCC 11+ / Clang 14+ | Good C++20 support |
| Build System | CMake 3.20+ | Industry standard |
| Package Manager | Conan 2.0 | Dependency management |

### **DEPENDENCIAS CLAVE**

| Librería | Versión | Uso | Link |
|----------|---------|-----|------|
| FFmpeg | 5.1+ | Media decode/demux | ffmpeg.org |
| libcurl | 7.80+ | HTTP requests | curl.se |
| SDL2 | 2.0.18+ | Windowing, audio | libsdl.org |
| OpenGL | 4.5+ | GPU rendering | khronos.org |
| spdlog | 1.10+ | Logging | github.com/gabime/spdlog |
| GTest | 1.12+ | Unit testing | google.github.io/googletest |
| ImGui | 1.88+ | GUI (option 1) | github.com/ocornut/imgui |
| Qt6 | 6.3+ | GUI (option 2, future) | qt.io |

### **HERRAMIENTAS DE DESARROLLO**

| Herramienta | Uso |
|------------|-----|
| GDB / LLDB | Debugging |
| Valgrind | Memory profiling |
| AddressSanitizer | Memory error detection |
| ThreadSanitizer | Race condition detection |
| clang-tidy | Static analysis |
| cppcheck | Code analysis |
| lcov/gcov | Code coverage |
| gprof / Perf | Performance profiling |
| Docker | Containerization |
| GitHub Actions | CI/CD |

### **REQUISITOS DEL SISTEMA**

**Mínimo:**
- OS: Linux, macOS, Windows (con WSL2)
- CPU: Quad-core, 2.0+ GHz
- RAM: 4 GB
- GPU: OpenGL 4.5 capable

**Recomendado para desarrollo:**
- OS: Ubuntu 22.04 LTS (development target)
- CPU: 8+ cores
- RAM: 16 GB
- GPU: NVIDIA / AMD with latest drivers

---

## 📋 CHECKLIST PRE-CODING

Antes de escribir UNA SOLA LÍNEA de código:

- [ ] Este documento revisado y aprobado
- [ ] GitHub repo creado con estructura correcta
- [ ] CMakeLists.txt skeleton compilando
- [ ] conanfile.txt definido
- [ ] Dockerfile básico funcionando
- [ ] Headers (interfaces) escritos sin implementación
- [ ] GitHub Actions workflows configurados
- [ ] Decisión ImGui vs Qt tomada
- [ ] Equipo de desarrollo informado (si aplica)
- [ ] Git workflow establecido (main, develop, feature branches)
- [ ] Development environment setup documentado

---

## 🎯 CONCLUSIÓN

Esta arquitectura proporciona:
- ✅ Roadmap claro en 12 semanas
- ✅ Componentes desacoplados (fácil testing)
- ✅ Production-grade design patterns
- ✅ Portfolio-worthy project
- ✅ Escalable (puede agregar features sin rediseño)
- ✅ Testeable (unit, integration, stress)
- ✅ Deployable (Docker, CI/CD)

**Próximo paso:** Crear el GitHub repo skeleton y comenzar FASE 0.

**No code yet. Solo structure. Esto es correcto.** ✅
