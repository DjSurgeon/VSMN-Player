# 🏗️ IPTV PLAYER - ARCHITECTURE DIAGRAMS

> Visualizaciones ASCII de la arquitectura completa

---

## DIAGRAMA 1: ARQUITECTURA DE CAPAS (Vertical)

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PRESENTATION LAYER                            │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                   ImGui GUI Layer                            │   │
│  ├─────────────────────────────────────────────────────────────┤   │
│  │ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐          │   │
│  │ │ Channel List │ │ Playback Ctrl│ │ Stats Panel │          │   │
│  │ └──────────────┘ └──────────────┘ └──────────────┘          │   │
│  │                                                               │   │
│  │ [Play] [Pause] [Stop] [Volume: ▮▮▮▮] [Fullscreen]         │   │
│  │                                                               │   │
│  │ Network: 12.5 Mbps | FPS: 60 | Res: 1920x1080             │   │
│  └─────────────────────────────────────────────────────────────┘   │
└──────────────┬────────────────────────────────────────────────────────┘
               │ playChannel(url), pause(), getStats()
               │
┌──────────────▼────────────────────────────────────────────────────────┐
│                     BUSINESS LOGIC LAYER                              │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │            Controller (State Machine)                        │   │
│  ├─────────────────────────────────────────────────────────────┤   │
│  │ Current State: PLAYING                                      │   │
│  │ Current Channel: HBO HD                                     │   │
│  │ Registered Observers: GUI, Logger                           │   │
│  │                                                               │   │
│  │ Subsystem Status:                                            │   │
│  │  ✓ Network: Running (downloading segment 42)               │   │
│  │  ✓ Decoder: Running (15 frames queued)                     │   │
│  │  ✓ Render: Running (60 FPS)                                │   │
│  └─────────────────────────────────────────────────────────────┘   │
└──────────────┬────────────────────────────────────────────────────────┘
               │ start(), stop(), notify state change
               │
┌──────────────┼──────────────────┬─────────────────────────────────────┐
│              │                  │                                      │
┌──────────────▼─────────┐  ┌─────▼──────────────┐  ┌──────────────────▼─┐
│  NETWORK SUBSYSTEM     │  │ DECODER SUBSYSTEM  │  │  RENDER SUBSYSTEM   │
├────────────────────────┤  ├────────────────────┤  ├─────────────────────┤
│ ┌──────────────────┐   │  │ ┌──────────────┐   │  │ ┌──────────────┐    │
│ │  HTTP Client     │   │  │ │ FFmpeg Ctx   │   │  │ │ OpenGL Ctx   │    │
│ │ (libcurl)        │   │  │ │ (AVFormat)   │   │  │ │ Textures     │    │
│ └────────┬─────────┘   │  │ └──────┬───────┘   │  │ └──────┬───────┘    │
│          │             │  │        │           │  │        │            │
│ ┌────────▼─────────┐   │  │ ┌──────▼───────┐   │  │ ┌──────▼────────┐   │
│ │ Download m3u8    │   │  │ │ Demux .ts    │   │  │ │ Render Loop   │   │
│ │ Segment fetcher  │   │  │ │ Decode H.264 │   │  │ │ (60 FPS)      │   │
│ │ Retry logic      │   │  │ │ Decode AAC   │   │  │ │ Audio play    │   │
│ └────────┬─────────┘   │  │ │ Resampling   │   │  │ │ A/V sync      │   │
│          │             │  │ └──────┬───────┘   │  │ └──────┬────────┘   │
│ ┌────────▼─────────┐   │  │        │           │  │        │            │
│ │ Output Queue     │   │  │ ┌──────▼───────┐   │  │ ┌──────▼────────┐   │
│ │ (ByteBuffer)     │   │  │ │ Output Queue │   │  │ │ Input Queues  │   │
│ │ Stats: NetworkSt │   │  │ │ (VideoFrame) │   │  │ │ Video + Audio │   │
│ │                  │   │  │ │ (AudioFrame) │   │  │ │                │   │
│ │ Thread: Network  │   │  │ │ Stats:       │   │  │ │ Stats:RenderSt│   │
│ │                  │   │  │ │ DecoderStats │   │  │ │                │   │
│ │ ┌──────────────┐ │   │  │ └──────────────┘   │  │ │ Thread: Main  │   │
│ │ │ TLS/Security │ │   │  │                    │  │ │                │   │
│ │ │ validation   │ │   │  │ Thread: Decoder    │  │ └────────────────┘   │
│ │ └──────────────┘ │   │  │                    │  │                      │
│ └──────────────────┘   │  └────────────────────┘  └──────────────────────┘
│                        │
│ Thread: Network        │ Thread: Decoder         Thread: Render
│ CPU bound: I/O Wait    │ CPU bound: High         GPU bound: Heavy
└────────────────────────┘
```

---

## DIAGRAMA 2: FLUJO DE DATOS (Thread Communication)

```
┌──────────────────────────────────────────────────────────────────────┐
│ USER SELECTS CHANNEL → GUI SENDS COMMAND                            │
└────────────────┬─────────────────────────────────────────────────────┘
                 │ PlaybackCommand { type: PLAY, channel_url: "..." }
                 ▼
        ┌─────────────────────┐
        │ CONTROLLER THREAD   │
        │ (Main Thread)       │
        │                     │
        │ Current state:      │
        │  IDLE → CONNECTING  │
        └────┬────────────────┘
             │
      ┌──────┴──────┬────────────┐
      │             │            │
      ▼             ▼            ▼
 ┌─────────┐  ┌─────────┐  ┌─────────┐
 │ Network │  │ Decoder │  │ Render  │
 │ Thread  │  │ Thread  │  │ Thread  │
 │ START   │  │ START   │  │ START   │
 └────┬────┘  └────┬────┘  └────┬────┘
      │            │            │
      │ Download   │            │
      │ .m3u8      │            │
      │ Playlist   │            │
      │ ─────────► │            │
      │ Segment 1  │            │
      │ ─────────► │ Decode    │
      │            │ ────────► │ Render
      │            │ Frame 1   │ ────────┐
      │ Segment 2  │           │        │ DISPLAY
      │ ─────────► │ Decode    │        │
      │            │ ────────► │ Render │
      │            │ Frame 2   │        │
      │ Segment N  │           │ Frame N
      │ ─────────► │ Decode    │ ────────┐
      │            │ ────────► │         │
      │            │           │        AUDIO
      │            │ Audio     │ ────────┐
      │            │ ────────► │         │
      │            │           │        SPEAKER
      └────────────┴───────────┴─────────┘
      
CONCURRENCY MODEL: Pipeline
  - Network: I/O-bound (waits on network)
  - Decoder: CPU-bound (decodes in parallel)
  - Render: GPU-bound + I/O (display + audio)
  - All running simultaneously, synchronized by queues
```

---

## DIAGRAMA 3: SYNCHRONIZATION & QUEUES

```
┌─────────────────────────────────────────────────────────────────────┐
│          THREAD-SAFE COMMUNICATION ARCHITECTURE                    │
└─────────────────────────────────────────────────────────────────────┘

Network Thread ──┐                           ┌─── Decoder Thread
                 │                           │
         ┌───────▼───────┐           ┌──────▼────────┐
         │   CIRCULAR    │           │   CIRCULAR    │
         │ BUFFER QUEUE  │           │ BUFFER QUEUE  │
         │               │           │               │
         │ [0] [1] [2]   │           │ [0] [1] [2]   │
         │ [3] [4] [5]   │           │ [3] [4] [5]   │
         │ [6] [7] [8]   │           │ [6] [7] [8]   │
         │               │           │               │
         │ LOCK-FREE OR  │           │ LOCK-FREE OR  │
         │ MUTEX + CV    │           │ MUTEX + CV    │
         │               │           │               │
         │ Producer:     │           │ Producer:     │
         │ Network       │           │ Decoder       │
         │               │           │ Consumer:     │
         │ Consumer:     │           │ Render        │
         │ Decoder       │           │               │
         └───────┬───────┘           └──────┬────────┘
                 │                          │
          ByteBuffer flow            VideoFrame + AudioFrame flow

SYNCHRONIZATION PRIMITIVES:
  1. Mutex: Protects queue state
  2. Condition Variable: Signal readiness
  3. Atomic flags: State transitions
  4. Memory barriers: Data visibility
  
COORDINATION SEQUENCE (Example: Pause):
  
  Timeline:
  T0: Controller.pause() called
      └─> Set state = PAUSED
      └─> Broadcast to all threads
      
  T1: Network thread
      └─> Sees state = PAUSED
      └─> Stops downloading
      └─> Signals ready
      └─> Waits for confirmation from Controller
      
  T2: Decoder thread
      └─> Sees state = PAUSED
      └─> Finishes current decode
      └─> Stops pulling from network queue
      └─> Signals ready
      
  T3: Render thread
      └─> Sees state = PAUSED
      └─> Displays last frame
      └─> Stops pulling from decoder queues
      └─> Signals ready
      
  T4: Controller
      └─> All threads ready
      └─> GUI notified: "PAUSED"
      └─> Buffers stable, safe to seek/resume
```

---

## DIAGRAMA 4: STATE MACHINE (Controller)

```
┌────────────────────────────────────────────────────────────┐
│           PLAYBACK STATE MACHINE                            │
└────────────────────────────────────────────────────────────┘

                        ┌────────────┐
                        │   IDLE     │ ◄─── Application start
                        │            │      or stop()
                        └─────┬──────┘
                              │ playChannel(url)
                              ▼
                        ┌────────────┐
                        │ CONNECTING │ ◄─── Downloading m3u8
                        │            │      Starting network
                        └─────┬──────┘
                              │ m3u8 loaded, first segment OK
                              ▼
                        ┌────────────┐
                        │  PLAYING   │ ◄─── Network + Decoder + Render
                        │            │      Video flowing
                        └──┬──────┬──┘
                           │      │
              pause()       │      │ stop()
                           ▼      ▼
                        ┌────────────┐
                        │  PAUSED    │      ┌──────────┐
                        │            │      │  STOPPED │
                        └─────┬──────┘      └──┬───────┘
                              │                │
                   play()      │                │ playChannel(new_url)
                              └────────────────┘
                                      ▲
                                      │
                                      │ error()
                                      │
                        ┌────────────────────┐
                        │      ERROR         │
                        │ [Display message]  │
                        │ [Log error]        │
                        └────────────────────┘
                                      △
                                      │
              Any state ─────────────┘ (network failure, decode error, etc)

ALLOWED TRANSITIONS:
  • IDLE → CONNECTING (playChannel)
  • CONNECTING → PLAYING (stream starts)
  • PLAYING → PAUSED (pause)
  • PAUSED → PLAYING (play)
  • PLAYING / PAUSED → STOPPED (stop)
  • STOPPED → CONNECTING (playChannel)
  • ANY → ERROR (failure detected)
  • ERROR → IDLE (reset)
  
INVALID TRANSITIONS: REJECTED
  • PAUSED → STOPPED (must resume first)
  • CONNECTING → PAUSED (not yet playing)
```

---

## DIAGRAMA 5: MEMORY LAYOUT (Heap)

```
┌──────────────────────────────────────────────────────────────────────┐
│                     MEMORY ALLOCATION STRATEGY                       │
│                  (~300-500 MB per application instance)              │
└──────────────────────────────────────────────────────────────────────┘

STACK (per thread):
┌────────────────────┐
│ Main Thread Stack  │ ~8 MB (GUI, state)
│ Network Stack      │ ~2 MB (HTTP buffers)
│ Decoder Stack      │ ~2 MB (frame processing)
│ Render Stack       │ ~2 MB (OpenGL calls)
└────────────────────┘

HEAP (shared):
┌─────────────────────────────────────────────┐
│ Network Buffers Layer: ~150 MB              │
├─────────────────────────────────────────────┤
│ • ByteBuffer pool (10 x 15 MB)              │
│   - Pre-allocated, circular                │
│   - Move semantics (zero-copy handoff)     │
│ • HTTP client context                      │
│ • TLS certificate chain                    │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│ Decoder Buffers Layer: ~200 MB              │
├─────────────────────────────────────────────┤
│ • FFmpeg contexts                           │
│   - AVFormatContext (demuxer)              │
│   - 2x AVCodecContext (video, audio)       │
│   - 4x AVFrame (buffers)                   │
│   - Resampler context                      │
│ • VideoFrame pool (10 x 3.1 MB @ 1080p)   │
│ • AudioFrame buffers (20 MB @ 48kHz)       │
│ • Color space converter context             │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│ Render Buffers Layer: ~80 MB                │
├─────────────────────────────────────────────┤
│ • GPU Textures (64 MB)                      │
│   - Y plane (1080p: 2.1 MB)                │
│   - U plane (1080p: 0.5 MB)                │
│   - V plane (1080p: 0.5 MB)                │
│ • Audio ring buffer (16 MB @ 48kHz)        │
│ • OpenGL vertex buffers                    │
│ • Shader programs                          │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│ Infrastructure Layer: ~70 MB                │
├─────────────────────────────────────────────┤
│ • GUI resources (ImGui)                     │
│ • Logging ring buffer (30 MB)               │
│ • Configuration (YAML parsed)               │
│ • Stats snapshot buffer                     │
│ • String pool (channel names, URLs)         │
└─────────────────────────────────────────────┘

ALLOCATION STRATEGY:
  • STATIC: All buffers pre-allocated at startup
  • CIRCULAR: Ring buffers reuse memory
  • MOVE SEMANTICS: Zero-copy handoff between threads
  • NO ALLOCATION: During playback (real-time safe)
  
MEMORY MONITORING:
  ├─ Resident Set Size (RSS): ~300 MB typical
  ├─ Virtual Size (VSZ): ~500 MB (includes mmap)
  └─ Peak RSS: ~400 MB (resolution-dependent)
```

---

## DIAGRAMA 6: BUILD & DEPLOYMENT PIPELINE

```
┌──────────────────────────────────────────────────────────────────────┐
│              CI/CD PIPELINE (GitHub Actions)                        │
└──────────────────────────────────────────────────────────────────────┘

Developer
    │
    ├─ Write code
    ├─ Run tests locally
    ├─ `git push origin feature/xyz`
    │
    └──► GitHub repo
             │
             ├──► Build job (Linux, macOS, Windows)
             │    ├─ cmake -B build
             │    ├─ cmake --build build
             │    └─ Check: ✓ compilation success
             │
             ├──► Test job
             │    ├─ ctest --verbose
             │    ├─ Run 200+ unit tests
             │    └─ Check: ✓ all tests pass
             │
             ├──► Sanitizer job (Clang + ASan/TSan)
             │    ├─ Compile with -fsanitize=address,thread
             │    ├─ Run test suite
             │    └─ Check: ✓ zero findings
             │
             ├──► Code coverage job
             │    ├─ gcov --verbose
             │    ├─ Generate coverage report
             │    └─ Check: ✓ coverage > 80%
             │
             ├──► Static analysis job (clang-tidy)
             │    ├─ clang-tidy src/*.cpp
             │    ├─ Check for issues
             │    └─ Check: ✓ issues < threshold
             │
             └──► ALL PASS?
                  │
                  ├─ YES: Create GitHub release
                  │       ├─ Build Docker image
                  │       ├─ Tag: latest, v1.0.0
                  │       ├─ Push to Docker Hub
                  │       ├─ Attach binaries to release
                  │       └─ ✅ READY FOR DEPLOYMENT
                  │
                  └─ NO: Mark PR as "failing"
                        └─ Developer fixes issues

DEPLOYMENT PATHS:

  Path A: Docker (Cloud)
  ┌─────────────────────┐
  │ Docker image pulled │
  │ `docker run ...`    │
  │ Accessible on port  │
  └─────────────────────┘

  Path B: Binary (Local)
  ┌──────────────────────┐
  │ Download iptv-player │
  │ Binary for OS        │
  │ Run: ./iptv-player   │
  └──────────────────────┘

  Path C: Source (Dev)
  ┌────────────────────────┐
  │ git clone              │
  │ cmake -B build && ...  │
  │ ./build/iptv_player    │
  └────────────────────────┘
```

---

## DIAGRAMA 7: PERFORMANCE BUDGET

```
┌──────────────────────────────────────────────────────────────────────┐
│           PERFORMANCE TARGETS & CRITICAL PATH                       │
└──────────────────────────────────────────────────────────────────────┘

LATENCY BUDGET (Network Segment → Display): < 200ms

  Segment received (t0)
         │
         ├─ Network processing: 10ms
         │  └─ Extract segment, queue
         │
         ├─ Decode time: 50-100ms
         │  ├─ Demux: 5ms
         │  ├─ H.264 decode: 40ms @ 1080p30fps
         │  └─ Audio decode & resample: 10ms
         │
         ├─ Buffering in render queue: 20-40ms
         │  └─ Variable, depends on frame rate
         │
         ├─ Render frame: 16ms (60fps = 16.67ms per frame)
         │  ├─ Upload texture: 5ms
         │  ├─ OpenGL draw call: 3ms
         │  └─ VSync wait: 8ms
         │
         └─ Display time: 20ms (screen refresh)

Total latency: 50-100 + 16 + 20 = 86-136ms ✓ WITHIN BUDGET

THROUGHPUT BUDGET (Network):
  
  Bitrate requirement: 10 Mbps minimum
  │
  ├─ 1080p @ 30fps H.264: ~5 Mbps
  ├─ AAC audio stereo: 128 kbps
  ├─ Overhead (fragmentation, retrans): 20%
  └─ Total: ~6.2 Mbps
  
  Available bandwidth: 10+ Mbps
  Headroom: 60% ✓ PLENTY FOR VARIABILITY

MEMORY BUDGET:
  
  Runtime allocation: 300 MB
  │
  ├─ Network buffers: 150 MB (largest)
  ├─ Decoder buffers: 200 MB
  ├─ Render buffers: 80 MB
  ├─ GUI + Infrastructure: 70 MB
  │
  └─ Typical footprint: < 300 MB ✓ REASONABLE

FPS STABILITY:
  
  Target: 60 FPS constant
  │
  ├─ Render loop duration: 16.67ms per frame
  ├─ Frame dropped if: decode time > 16.67ms
  │  └─ Fallback: lower resolution
  ├─ Audio drift tolerance: ±80ms
  │  └─ Resync if exceeded
  │
  └─ Result: < 2 dropped frames / hour ✓ ACCEPTABLE
```

---

## DIAGRAMA 8: INTERACTION FLOW (User → System)

```
┌──────────────────────────────────────────────────────────────────────┐
│                   USER INTERACTION SEQUENCE                          │
└──────────────────────────────────────────────────────────────────────┘

USER ACTION              SYSTEM RESPONSE            STATE CHANGE

┌──────────────────────────────────────────────────────────────────┐
│ 1. LAUNCH APP                                                    │
├──────────────────────────────────────────────────────────────────┤
│ User: Double-click iptv_player                                  │
│       ↓                                                           │
│ App:  Load config, init FFmpeg, setup GUI                       │
│       Display channel list (from cached m3u8)                   │
│       ↓                                                           │
│ GUI:  [Select a channel to begin...]                            │
│ State: IDLE → (ready for input)                                 │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 2. SELECT CHANNEL                                                │
├──────────────────────────────────────────────────────────────────┤
│ User: Click "HBO HD" in channel list                            │
│       ↓                                                           │
│ Event: onClick("hbo_hd_url")                                    │
│       ↓                                                           │
│ Controller: state = CONNECTING                                  │
│           notifyObservers(CONNECTING)                           │
│       ↓                                                           │
│ Network: Download m3u8 from hbo_hd_url                          │
│          Parse playlist (extract segments URLs)                 │
│          Start downloading segment 1                            │
│          Queue ByteBuffer → decoder                             │
│       ↓                                                           │
│ Decoder: Waiting for bytes (queue empty) ...                    │
│          Bytes arrive                                            │
│          Demux .ts, extract H.264 + AAC                         │
│          Decode frame 1                                          │
│          Queue VideoFrame + AudioFrame → render                 │
│       ↓                                                           │
│ Render: Pop VideoFrame, upload to GPU                           │
│         Pop AudioFrame, queue to SDL Audio                      │
│         Draw frame on screen                                     │
│         Play audio samples                                       │
│       ↓                                                           │
│ GUI: Display video, show bitrate 12.5 Mbps, FPS 30            │
│ State: CONNECTING → PLAYING                                     │
│ ✅ VIDEO APPEARS, AUDIO PLAYS                                   │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 3. PAUSE                                                         │
├──────────────────────────────────────────────────────────────────┤
│ User: Click [Pause] button                                      │
│       ↓                                                           │
│ Controller: state = PAUSED                                      │
│           Signal network, decoder, render                       │
│       ↓                                                           │
│ Network: Stop downloading new segments                          │
│ Decoder: Stop pulling from queue (frame queued stays)          │
│ Render:  Hold current frame on screen                           │
│          Stop queuing audio                                     │
│       ↓                                                           │
│ GUI: Show [Play] button (instead of [Pause])                    │
│ State: PLAYING → PAUSED                                         │
│ ✅ VIDEO PAUSED, AUDIO SILENT                                   │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 4. RESUME                                                        │
├──────────────────────────────────────────────────────────────────┤
│ User: Click [Play] button                                       │
│       ↓                                                           │
│ Controller: state = PLAYING                                     │
│           Signal resume                                         │
│       ↓                                                           │
│ Network: Resume downloading segments                            │
│ Decoder: Resume pulling from network queue                      │
│ Render:  Resume pulling from decoder queues                     │
│       ↓                                                           │
│ Flow:  [as normal playback]                                      │
│ State: PAUSED → PLAYING                                         │
│ ✅ VIDEO RESUMES, AUDIO CONTINUES                               │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 5. ERROR (Network Failure)                                      │
├──────────────────────────────────────────────────────────────────┤
│ System: Network timeout (no segment for 10s)                    │
│       ↓                                                           │
│ Network: Retry logic triggered                                  │
│         Attempt 1: FAIL                                          │
│         Attempt 2: FAIL (exponential backoff)                   │
│         Attempt 3: FAIL                                          │
│         Max retries exceeded → report error                     │
│       ↓                                                           │
│ Controller: state = ERROR                                       │
│           error_message = "Network timeout"                     │
│           notifyObservers(ERROR)                                │
│       ↓                                                           │
│ Network: Stop downloading                                       │
│ Decoder: Drain queue, output last frame                         │
│ Render:  Show last frame + error overlay                        │
│       ↓                                                           │
│ GUI: Display error message                                       │
│      [Retry] button appears                                     │
│ State: PLAYING → ERROR                                          │
│ ✅ USER SEES ERROR, CAN RETRY                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## SUMMARY

These diagrams show:
- ✅ Layered architecture (presentation → logic → subsystems)
- ✅ Data flow (producer-consumer pipeline)
- ✅ Synchronization (queues, state machine)
- ✅ Memory layout (pre-allocation, circular buffers)
- ✅ Performance budget (latency, throughput, memory)
- ✅ User interaction (end-to-end flows)

**Collectively, they define the complete system design.**
