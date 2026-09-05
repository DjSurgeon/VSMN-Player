# 📊 IPTV PLAYER - DESIGN DE ESTRUCTURAS DE DATOS

> **Documento de Referencia** | Tipos, estructuras y formatos de datos sin implementación

---

## 📋 ÍNDICE

1. [Core Data Types](#core-data-types)
2. [Network Layer Data](#network-layer-data)
3. [Decoder Layer Data](#decoder-layer-data)
4. [Render Layer Data](#render-layer-data)
5. [Controller & State](#controller--state)
6. [Serialization Formats](#serialization-formats)
7. [Thread-Safe Communication](#thread-safe-communication)

---

## 🎯 CORE DATA TYPES

### **ByteBuffer: Raw Binary Data**

**Propósito:** Contener bytes descargados (segmentos .ts)

```
ByteBuffer
├── data: uint8_t*              // Pointer to raw bytes
├── size: size_t                // Number of bytes
├── capacity: size_t            // Allocated space
├── timestamp: int64_t          // PTS (Presentation Time Stamp)
└── metadata: BufferMetadata    // Extra info
    ├── segment_index: uint32_t
    ├── url: string             // Source URL
    ├── download_time_ms: uint32_t
    └── crc32: uint32_t         // For integrity check
```

**Consideraciones:**
- Pre-allocado (no malloc en hot path)
- Move-semantics (transferencia de ownership entre threads)
- Optional CRC32 para validar integridad

---

### **Channel: IPTV Channel Info**

**Propósito:** Metadata de un canal de IPTV

```
Channel
├── id: uint32_t                // Unique identifier
├── name: string                // Display name (e.g., "HBO")
├── logo_url: string            // Channel logo URL
├── url: string                 // Stream URL (.m3u8)
├── group: string               // Category (e.g., "Movies", "Sports")
├── tvg_id: string              // EPG ID (optional)
├── duration: int64_t           // Duration in ms (-1 if live)
├── is_live: bool               // Is this a live stream?
├── variants: vector<StreamVariant>  // Multiple bitrates
│   └── StreamVariant
│       ├── bandwidth: uint32_t // Bandwidth in bits/s
│       ├── resolution: string  // "1920x1080"
│       ├── fps: float          // Frames per second
│       └── url: string         // Variant-specific URL
└── metadata: ChannelMetadata
    ├── last_played: timestamp
    ├── play_count: uint32_t
    └── favorite: bool
```

**Consideraciones:**
- Parseable from .m3u8 extended format
- Soporta múltiples variantes (adaptive bitrate)
- Optional EPG metadata

---

### **PlaybackState: Application State**

**Propósito:** Representar estado actual de la aplicación

```cpp
enum class PlaybackState {
    IDLE = 0,          // Application started, no stream
    CONNECTING = 1,    // Downloading playlist / connecting
    PLAYING = 2,       // Video is flowing
    PAUSED = 3,        // Paused by user
    BUFFERING = 4,     // Waiting for data (network slow)
    ERROR = 5,         // Error occurred
    STOPPED = 6        // Explicitly stopped
};

struct PlaybackStateInfo {
    PlaybackState state: PlaybackState
    int64_t state_change_time: timestamp  // When state changed
    string error_message: string          // If ERROR state
    float progress_percent: float         // 0-100%, if known
};
```

---

## 🌐 NETWORK LAYER DATA

### **NetworkStats: Network Metrics**

**Propósito:** Estadísticas en tiempo real de la red

```
NetworkStats
├── bytes_downloaded: uint64_t          // Total bytes so far
├── segments_received: uint32_t         // Number of segments
├── segments_failed: uint32_t           // Failed downloads
├── current_bandwidth_mbps: float       // Real-time Mbps
├── average_bandwidth_mbps: float       // Average
├── packet_loss_percent: float          // Estimated PLoss %
├── latency_ms: uint32_t                // RTT milliseconds
├── retry_count: uint32_t               // Total retries done
├── last_error: string                  // Last error message
├── connection_time_ms: uint32_t        // Time to first byte
├── last_update_time: timestamp
└── queued_bytes: uint32_t              // Bytes in output queue
```

**Actualización:** Every 1-2 seconds

---

### **Playlist: Colección de Canales**

**Propósito:** Parsed .m3u8 file

```
Playlist
├── version: string                     // #EXTM3U version
├── channels: vector<Channel>           // All parsed channels
├── tvg_info: TVGMetadata              // EPG information
│   ├── tvg_url: string                // EPG source
│   └── tvg_shift: int32_t             // Time offset
├── source_url: string                 // Original m3u8 URL
├── last_updated: timestamp
├── refresh_interval_seconds: uint32_t  // How often to refresh
└── metadata: PlaylistMetadata
    ├── title: string
    ├── author: string
    └── thumbnail_url: string
```

**Consideraciones:**
- Puede ser grande (1000+ canales)
- Parseable incrementalmente (streaming parse)
- Updateable in-place

---

### **HTTPRequest: Outgoing Request**

**Propósito:** Descripción de HTTP request

```
HTTPRequest
├── method: string          // "GET" mostly
├── url: string             // Full URL
├── headers: map<string, string>
│   ├── "User-Agent": "IPTV-Player/1.0"
│   ├── "Accept-Encoding": "gzip"
│   └── ...
├── timeout_ms: uint32_t    // Max wait time
├── retry_policy: RetryPolicy
│   ├── max_retries: uint32_t
│   ├── backoff_strategy: enum (LINEAR, EXPONENTIAL)
│   └── max_backoff_ms: uint32_t
├── ssl_verify: bool        // TLS certificate validation
└── follow_redirects: bool  // HTTP 301/302 handling
```

---

### **HTTPResponse: Incoming Response**

**Propósito:** Resultado de HTTP request

```
HTTPResponse
├── status_code: uint32_t   // 200, 404, 500, etc
├── headers: map<string, string>
│   ├── "Content-Type": "video/mp2t"
│   ├── "Content-Length": "1048576"
│   └── ...
├── body: ByteBuffer        // Response body
├── latency_ms: uint32_t    // Request → Response time
├── timestamp: int64_t      // When received
└── error: optional<NetworkError>
    ├── code: NetworkErrorCode
    ├── message: string
    └── is_retryable: bool
```

---

## 🎬 DECODER LAYER DATA

### **VideoFrame: Decoded Video**

**Propósito:** Single video frame ready for rendering

```
VideoFrame
├── data: vector<uint8_t>       // Raw pixel data (YUV or RGBA)
├── format: PixelFormat         // YUV420p, YUV422p, RGBA, etc
├── width: uint32_t             // Pixel width
├── height: uint32_t            // Pixel height
├── stride: vector<uint32_t>    // Bytes per line (3 planes for YUV)
├── pts: int64_t                // Presentation timestamp (microseconds)
├── dts: int64_t                // Decode timestamp
├── duration_us: uint32_t       // Frame duration in microseconds
├── key_frame: bool             // Is this an IDR frame?
├── sequence_number: uint64_t   // For ordering / debugging
├── timestamp_received: int64_t // When received from decoder
└── metadata: FrameMetadata
    ├── color_space: ColorSpace // BT709, BT601, etc
    ├── color_range: ColorRange // Full, limited
    └── aspect_ratio: string    // "16:9", "4:3"
```

**Tamaño estimado:**
- 1080p YUV420p: 3.1 MB
- 720p YUV420p: 1.4 MB
- 360p YUV420p: 350 KB

---

### **AudioFrame: Decoded Audio**

**Propósito:** Single audio frame (samples)

```
AudioFrame
├── samples: vector<float>      // PCM samples [-1.0, 1.0]
├── channels: uint32_t          // 1 (mono), 2 (stereo), etc
├── sample_rate: uint32_t       // Samples per second (44100, 48000)
├── pts: int64_t                // Presentation timestamp
├── duration_us: uint32_t       // Duration in microseconds
├── sequence_number: uint64_t   // For ordering
└── timestamp_received: int64_t
```

**Sample size:** ~48000 samples @ 48kHz ≈ 384 KB (1 second)

---

### **DecoderStats: Decode Metrics**

**Propósito:** Estadísticas del decodificador

```
DecoderStats
├── frames_decoded: uint64_t         // Total video frames
├── frames_dropped: uint32_t         // Skipped (corrupt)
├── audio_frames_decoded: uint64_t   // Total audio frames
├── current_fps: float               // Current playback FPS
├── current_resolution: string       // "1920x1080"
├── current_bitrate_mbps: float      // Estimated decode bitrate
├── decode_latency_ms: float         // Avg time to decode frame
├── video_buffer_depth: uint32_t     // Frames queued for render
├── audio_buffer_depth: uint32_t     // Audio frames queued
├── last_error: string
└── last_update_time: timestamp
```

---

### **CodecContext: FFmpeg State**

**Propósito:** Encapsulate FFmpeg decoder state (opaque from perspective)

```
CodecContext (opaque, FFmpeg internals)
├── AVFormatContext*            // Demux context
├── AVCodecContext* (video)      // Video decoder
├── AVCodecContext* (audio)      // Audio decoder
├── AVFrame* (video)             // Buffer for video decode
├── AVFrame* (audio)             // Buffer for audio decode
├── SwsContext*                  // Color space converter
├── SwrContext*                  // Audio resampler
└── state: enum
    ├── UNINITIALIZED
    ├── READY
    ├── DECODING
    ├── ERROR
    └── CLOSED
```

---

## 🎨 RENDER LAYER DATA

### **RenderStats: Render Metrics**

**Propósito:** Estadísticas del renderer

```
RenderStats
├── frames_rendered: uint64_t        // Total frames displayed
├── frames_dropped: uint32_t         // Frames not rendered (too late)
├── current_fps: float               // Actual FPS
├── target_fps: uint32_t             // Target FPS (60)
├── frame_time_ms: float             // Avg time to render one frame
├── gpu_utilization_percent: float   // GPU load (if available)
├── last_frame_latency_ms: float     // Decode → Display time
├── vblank_missed: uint32_t          // Missed VSync count
├── audio_underruns: uint32_t        // Audio playback glitches
└── last_update_time: timestamp
```

---

### **GLTexture: GPU Resource (opaque)**

**Propósito:** OpenGL texture handle

```
GLTexture (opaque)
├── handle: GLuint               // OpenGL texture ID
├── width: uint32_t
├── height: uint32_t
├── format: GLenum               // GL_RGB, GL_RGBA, etc
├── target: GLenum               // GL_TEXTURE_2D, etc
└── last_update_time: timestamp
```

---

## 🎛️ CONTROLLER & STATE

### **Controller Config**

**Propósito:** Configuration parameters para el controller

```
ControllerConfig
├── auto_start_playback: bool         // Auto-play first channel?
├── buffer_size_bytes: uint32_t       // Network buffer capacity
├── decode_buffer_frames: uint32_t    // Max frames buffered
├── render_buffer_frames: uint32_t    // Max frames queued for render
├── max_bandwidth_mbps: float         // -1 for unlimited
├── min_bandwidth_mbps: float         // Auto-pause if below
├── adaptive_bitrate: bool            // Select best quality?
├── preferred_resolution: string      // "1080p", "720p", auto
├── network_timeout_ms: uint32_t      // Per-segment timeout
├── network_retry_max: uint32_t       // Max retries
├── audio_device_index: int32_t       // SDL audio device
├── display_stats: bool               // Show FPS/stats?
├── log_level: enum (TRACE, DEBUG, INFO, WARN, ERROR)
└── data_dir: string                  // For configs, cache
```

---

### **PlaybackCommand: User Action**

**Propósito:** Commands from GUI → Controller

```cpp
enum class CommandType {
    PLAY = 0,
    PAUSE = 1,
    STOP = 2,
    SEEK = 3,
    NEXT_CHANNEL = 4,
    PREV_CHANNEL = 5,
    SELECT_CHANNEL = 6,
    SELECT_VARIANT = 7,
    SET_VOLUME = 8,
    FULLSCREEN_TOGGLE = 9,
};

struct PlaybackCommand {
    CommandType type: CommandType
    int64_t parameter: int64_t         // seek position, channel id, etc
    string string_parameter: string    // channel name, etc
    timestamp created_at
};
```

---

### **PlaybackEvent: Notifications**

**Propósito:** Events from Controller → GUI (observer pattern)

```cpp
enum class EventType {
    STATE_CHANGED = 0,
    STATS_UPDATED = 1,
    ERROR_OCCURRED = 2,
    STREAM_STARTED = 3,
    STREAM_ENDED = 4,
    CHANNEL_CHANGED = 5,
    BUFFER_UNDERRUN = 6,
    BUFFER_FULL = 7,
    SEEK_COMPLETE = 8,
};

struct PlaybackEvent {
    EventType type: EventType
    PlaybackState new_state: PlaybackState
    AggregateStats stats: AggregateStats
    string message: string
    timestamp created_at
};
```

---

### **AggregateStats: Combined Statistics**

**Propósito:** All stats rolled into one (for GUI display)

```
AggregateStats
├── network: NetworkStats
├── decoder: DecoderStats
├── render: RenderStats
├── current_channel: Channel
├── playback_state: PlaybackState
├── current_bitrate_mbps: float  // Blended estimate
├── total_elapsed_ms: int64_t
├── buffer_health: enum
│   ├── HEALTHY (> 50% full)
│   ├── WARNING (25-50%)
│   └── CRITICAL (< 25%)
└── timestamp: int64_t
```

---

## 📋 SERIALIZATION FORMATS

### **M3U8 Playlist Format (Input)**

```
#EXTM3U
#EXT-X-VERSION:3
#EXT-X-ALLOW-CACHE:YES

#EXTINF:-1 tvg-id="hbo" tvg-name="HBO" tvg-logo="url.png" group-title="Movies"
#EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080,FRAME-RATE=30
https://example.com/hbo/playlist.m3u8

#EXTINF:-1 tvg-id="espn" tvg-name="ESPN" group-title="Sports"
#EXT-X-STREAM-INF:BANDWIDTH=2500000,RESOLUTION=1280x720
https://example.com/espn/playlist.m3u8
```

---

### **Config File Format (YAML)**

```yaml
# config.yaml
application:
  title: "IPTV Player"
  version: "1.0.0"

playback:
  auto_start: true
  buffer_size_mb: 100
  adaptive_bitrate: true
  preferred_quality: "720p"

network:
  timeout_ms: 10000
  max_retries: 3
  backoff_strategy: "exponential"

logging:
  level: "INFO"
  file: "logs/iptv_player.log"
  file_size_mb: 50
  backup_count: 5

display:
  fullscreen: false
  resolution: "1280x720"
  show_stats: true
  theme: "dark"

audio:
  device: -1  # Default device
  volume: 0.8
```

---

### **Telemetry/Metrics Format (JSON)**

```json
{
  "timestamp": "2024-01-15T10:30:45Z",
  "session_id": "uuid-xxx",
  "playback": {
    "state": "PLAYING",
    "channel": "HBO",
    "elapsed_seconds": 3600
  },
  "network": {
    "bandwidth_mbps": 12.5,
    "packet_loss_pct": 0.2,
    "segments_received": 180
  },
  "decoder": {
    "fps": 30,
    "resolution": "1920x1080",
    "latency_ms": 120
  },
  "render": {
    "fps": 60,
    "frames_dropped": 2,
    "gpu_utilization_pct": 45
  },
  "errors": []
}
```

---

## 🔄 THREAD-SAFE COMMUNICATION

### **Queue Interface (Template)**

**Propósito:** Thread-safe data handoff entre threads

```cpp
template<typename T>
class ThreadSafeQueue {
    
    // Core operations
    void push(T item);                    // Producer: añade item
    bool try_pop(T& item);                // Consumer: intenta sacarlo
    bool wait_and_pop(T& item, timeout_ms);  // Consumer: espera hasta timeout
    
    // Inspection (no blocking)
    size_t size() const;                  // Current queue size
    bool empty() const;
    
    // Shutdown
    void shutdown();                      // Signal: no más items
    bool is_shutdown() const;
};
```

**Implementaciones posibles:**
- Lock-free (moodycamel::ConcurrentQueue) → mejor performance
- Mutex + condition_variable → más simple, fácil de debuggear

---

### **Circular Buffer Pattern**

**Propósito:** Pre-allocated, fixed-size buffer for zero-allocation playback

```
Memory Layout:
[Slot 0] [Slot 1] [Slot 2] [Slot 3] ... [Slot N]
  ↑
write_pos (producer advances)
                     ↑
              read_pos (consumer advances)
```

**Properties:**
- Fixed size (no memory allocation after init)
- Blocks producer if full (backpressure)
- Blocks consumer if empty (wait for data)

---

### **Condition Variable Protocol**

**Ejemplo: Pause orchestration**

```
Thread A (Controller):           Thread B (Network):       Thread C (Decoder):
│                                │                          │
├─ set state = PAUSED            │                          │
│                                                            │
├─ notify network                 │                          │
│  (condition var)               ├─ wake up                │
│                                ├─ see PAUSED             │
├─ notify decoder                 │                          ├─ wake up
│  (condition var)               │                          ├─ see PAUSED
│                                ├─ stop()                 ├─ stop()
├─ wait_for(network_ready) ─────┤  (flush buffers)         │
│                                ├─ signal ready ─────┐    │
│                                │                     ├────┤
│                                │                     │    ├─ signal ready ─┐
├─ wait_for(decoder_ready) ──────────────────────────────────┤ (signal ready) │
│                                │                          │
├─ confirm PAUSED to GUI          │                          │
└────────────────────────────────┴──────────────────────────┘
```

---

## 📐 MEMORY LAYOUT & ALLOCATION STRATEGY

### **Static Allocation (at startup)**

```
Total Budget: ~500 MB per instance

├── Network Buffers: 150 MB
│   ├── 10 x 15 MB ByteBuffer (circular pool)
│   └── Queue pointers
│
├── Decoder Buffers: 200 MB
│   ├── FFmpeg contexts (50 MB)
│   ├── 10 x VideoFrame (30 MB for 1080p)
│   └── Audio sample pool (20 MB)
│
├── Render Buffers: 80 MB
│   ├── GPU textures (64 MB)
│   ├── Audio ring buffer (16 MB @ 48kHz)
│
├── Other: 70 MB
│   ├── GUI resources (20 MB)
│   ├── Logging (30 MB circular)
│   └── Config/Metadata (20 MB)
└── TOTAL: ~500 MB
```

### **Zero-Allocation During Playback**

- Buffers pre-allocated
- Move semantics (no copying)
- Queue operations pointer-only
- Metrics: sampling-based (not logging everything)

---

## 🎯 CONCLUSIÓN

Este documento define:
- ✅ Todos los tipos principales
- ✅ Formatos de datos
- ✅ Interfaces de comunicación
- ✅ Patrones de sincronización
- ✅ Estrategia de memoria

**Próximo paso:** Con este documento + arquitectura, iniciar FASE 0 con GitHub repo skeleton.
