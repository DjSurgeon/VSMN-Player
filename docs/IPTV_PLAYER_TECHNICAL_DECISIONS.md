# 🎯 IPTV PLAYER — DECISION LOG

**Propósito:** Documentar decisiones arquitectónicas + justificación  
**Audiencia:** Recruiters, code reviewers, future maintainers  
**Valor:** Demuestra pensamiento crítico, no solo random coding

---

## DECISIÓN 1: Multi-Thread vs Single-Thread Architecture

### Pregunta

¿Construir el player como single-thread (simple, async-await style) o multi-thread (complex, producer-consumer)?

### Opciones Consideradas

| Opción | Ventajas | Desventajas |
|--------|----------|-------------|
| **Single-thread + async I/O** | Simple de entender, sin race conditions, menos bugs | Réplicas de la realidad: no existe en producción; demux FFmpeg es blocking; no demuestra concurrencia |
| **Multi-thread (nuestra elección)** | Real-world pattern, demuestra sincronización avanzada, separación de concerns | Más difícil de debuggear, race conditions posibles, requiere testing riguroso |

### Decisión

**Multi-thread architecture con 3 threads especializados:**
1. Network thread (HTTP download)
2. Decoder thread (FFmpeg)
3. Render thread (OpenGL + audio)

### Justificación

- **Realismo:** Esto es lo que hacen Netflix, YouTube, Twitch, VLC
- **Performance:** Cada thread en su CPU core sin bloqueos
- **Portfolio impact:** Demuestra competencia en concurrencia (HIGH VALUE)
- **Learning:** Sincronización, race conditions, debugging con ThreadSanitizer
- **Interview prep:** Recruiters preguntarán "cómo evitas deadlock?" — tienes respuesta

### Trade-offs Aceptados

- ✅ Más testing (necesario pero educativo)
- ✅ Más debugging (pero aprendes gdb + valgrind)
- ❌ Líneas de código (+2000 líneas para threading)
- ❌ Timeline más largo (+1-2 semanas)

---

## DECISIÓN 2: Mutex + Condition Variables vs Lock-Free Data Structures

### Pregunta

Para sincronizar Network → Decoder → Render, ¿usar mutex o implementar lock-free?

### Opciones Consideradas

| Opción | Complejidad | Performance | Learning Value |
|--------|---|---|---|
| **Mutex + CV (MVP)** | Baja | Adecuada para streaming | Media (basics) |
| **Lock-free (bonus)** | Alta | Óptima para HFT | Alta (advanced) |
| **Atomic<bool> flags (risky)** | Muy baja | Mala | Ninguna |

### Decisión

**Fase 1 (MVP): Mutex + Condition Variables**
**Fase 2 (bonus): Lock-free ring buffer**

### Justificación Fase 1

- Correctness > performance en MVP
- Mutex es fácil de debuggear (vs lock-free es pesadilla)
- **Suficientemente rápido** (streaming, no HFT)
- ThreadSanitizer detecta problemas automáticamente
- Recruiters lo entienden inmediatamente

### Justificación Fase 2 (bonus)

Si tiempo permite:
- Lock-free demuestra conocimiento avanzado
- Muestra optimización de performance
- Modern C++ (std::atomic, std::memory_order)
- Diferenciador en interviews (muy pocos lo hacen)

### Implementación

```cpp
// MVP: Simple circular buffer with mutex
template<typename T>
class CircularBuffer {
    std::mutex mtx;
    std::condition_variable not_empty, not_full;
    std::queue<T> data;
public:
    void enqueue(const T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        while (data.size() >= CAPACITY) {
            not_full.wait(lock);
        }
        data.push(item);
        not_empty.notify_one();
    }
};

// Bonus: Lock-free (if implemented)
template<typename T>
class LockFreeRingBuffer {
    std::vector<T> buffer;
    std::atomic<size_t> write_pos{0};
    std::atomic<size_t> read_pos{0};
public:
    bool tryEnqueue(const T& item) {
        size_t wp = write_pos.load(std::memory_order_relaxed);
        if ((wp + 1) % CAPACITY == read_pos.load(std::memory_order_acquire)) {
            return false;  // Full
        }
        buffer[wp] = item;
        write_pos.store((wp + 1) % CAPACITY, std::memory_order_release);
        return true;
    }
};
```

---

## DECISIÓN 3: ImGui vs Qt Framework

### Pregunta

¿Usar ImGui (simple, rápido) o Qt (profesional, featured)?

### Opciones Consideradas

| Aspecto | ImGui | Qt | Raw OpenGL |
|---------|-------|----|----|
| **Apariencia** | Funcional | Profesional | Custom |
| **Curva aprendizaje** | Plana | Steep | Vertical |
| **Tamaño ejecutable** | 10 MB | 100+ MB | Pequeño |
| **Líneas GUI code** | 200 | 1000+ | Miles |
| **Integración OpenGL** | Nativa | Via QOpenGL | Nativa |
| **Tiempo MVP** | 1 semana | 3+ semanas | 2 semanas |

### Decisión

**MVP (Weeks 1-6): ImGui**
**Bonus (if time): Qt rewrite**

### Justificación

ImGui es la **decisión correcta para MVP**:
- Funcionalmente completa (channel list, buttons, stats)
- Integración OpenGL trivial (ImGui dibuja directamente en contexto)
- Prueba rápida de arquitectura
- No interfiere con threads (ImGui es single-threaded, GUI en main thread)

Qt es overkill para MVP:
- Qt threading model es complejo (signals/slots across threads)
- Más líneas de código = más bugs
- No agrega valor real en funcionalidad

### Trade-off

- ✅ MVP listo rápido
- ✅ Prove arquitectura threading
- ❌ UI no será "pretty" (but functional)
- ❌ Qt rewrite es late game (if energy remains)

### Si Qt es Requerimiento

If recruiter demands "professional UI", rewrite en semana 11-12. Pero código es same, solo UI layer cambia.

---

## DECISIÓN 4: libcurl vs Raw POSIX Sockets

### Pregunta

¿Usar libcurl (abstracción HTTP) o POSIX sockets (low-level)?

### Opciones Consideradas

| Aspecto | libcurl | Raw Sockets |
|---------|---------|------------|
| **Líneas HTTP code** | 20 | 200+ |
| **HTTPS/TLS** | Automático | Manual OpenSSL |
| **HTTP pipelining** | Automático | Manual |
| **Error handling** | Robust | DIY |
| **Learning value** | Media | Alta |
| **Producción-ready** | Sí | Risky |

### Decisión

**MVP: libcurl**
**Bonus: POSIX wrapper si tiempo**

### Justificación

libcurl es industrial-standard para MVP:
- FFmpeg usa libcurl
- VLC usa libcurl
- Compilar sin errores en semanas 1-2
- Permite testing de architecture sin networking rabbit holes

POSIX sockets bonus:
- Demuestra conocimiento networking bajo-nivel
- Más control (epoll, non-blocking, custom timeouts)
- Diferenciador en interviews
- Pero riesgo: custom bugs, TLS complexity

### Implementación Dos-Capas

```cpp
// Layer 1 (MVP): libcurl wrapper
class HttpClient {
    CURL* handle;
public:
    std::vector<uint8_t> get(const std::string& url) {
        // Use libcurl internally
    }
};

// Layer 2 (bonus): POSIX wrapper
class PosixHttpClient {
    int socket;
public:
    std::vector<uint8_t> get(const std::string& url) {
        // Use raw socket + OpenSSL
    }
};

// Common interface
class IHttpClient {
    virtual std::vector<uint8_t> get(const std::string& url) = 0;
};
```

Ventaja: Ambas implementaciones under same interface → easy swap.

---

## DECISIÓN 5: FFmpeg vs OpenH264 vs NVIDIA NVDEC

### Pregunta

¿Cuál decodificador de video usar?

### Opciones Consideradas

| Aspecto | FFmpeg | OpenH264 | NVIDIA NVDEC |
|---------|--------|----------|------------|
| **Formatos** | Todos | Solo H.264 | H.264/H.265 |
| **Portabilidad** | Excelente | Buena | Nvidia GPU only |
| **Aceleración HW** | Sí (vaapi, nvenc) | No | Sí, excelente |
| **Producción** | Industry standard | Niche | Gaming/ML |
| **Complejidad** | Media | Baja | Alta |
| **Android/iOS support** | No (native) | Sí | No (mobile) |

### Decisión

**FFmpeg, porque:**
- Netflix, YouTube, VLC, OBS usan FFmpeg
- Soporta todos los formatos (H.264, H.265, VP9, AV1)
- Hardware aceleración available (vaapi en Linux)
- RAII management es difícil pero doable
- Recruiters lo reconocen inmediatamente

### Por qué NO OpenH264

- OpenH264 solo H.264
- Si stream es H.265 → stuck
- Menos educativo

### Por qué NO NVIDIA NVDEC (MVP)

- Requiere GPU Nvidia
- Overkill para MVP (CPU decode is fine)
- Bonus: si tiempo, agregar NVDEC optional

### Implementation Strategy

```cpp
// Phase 1: Software decode (CPU)
class Decoder {
    AVCodecContext* codec_ctx;  // H.264 codec
    AVFrame* frame;
    
    bool decode(const uint8_t* data, size_t size) {
        // FFmpeg software decode
        av_codec_send_packet(codec_ctx, packet);
        av_codec_receive_frame(codec_ctx, frame);
    }
};

// Phase 2 bonus: Hardware decode
class DecoderHW {
    AVBufferRef* hw_device_ctx;  // NVIDIA GPU context
    AVCodecContext* codec_ctx;
    
    bool decode(const uint8_t* data, size_t size) {
        // FFmpeg hardware decode via NVIDIA
        av_hwframe_get_buffer(codec_ctx->hw_frames_ctx, frame);
    }
};
```

---

## DECISIÓN 6: C++17 vs C++20 vs C++23

### Pregunta

¿Qué standard de C++?

### Opciones Consideradas

| Standard | Release | Features | Adoptado |
|----------|---------|----------|----------|
| **C++17** | 2017 | std::optional, structured bindings | Ancho |
| **C++20** | 2020 | Concepts, ranges, coroutines | Creciendo |
| **C++23** | 2023 | std::flat_map, deducing this | Muy nuevo |

### Decisión

**C++20**

### Justificación

- Modern enough (std::thread, std::atomic disponibles desde C++11)
- Features útiles:
  - **Concepts:** Type safety en templates
  - **Ranges:** Cleaner algorithms
  - **Coroutines:** Async patterns (if needed)
- Compiladores soportan: GCC 10+, Clang 10+
- Recruiters esperan modern C++ (C++17 mínimo)

### NO C++23

- Muy nuevo, algunos compiladores no soportan
- Herramientas (clang-tidy, debuggers) menos maduras
- Marginal benefit para nuestro scope

### Ejemplos de C++20 Usage

```cpp
// Concepts: Compilación type-safe
template<typename T>
concept Drawable = requires(T t) {
    { t.draw() } -> std::same_as<void>;
};

template<Drawable T>
void renderAll(const std::vector<T>& objects) { }

// Ranges: Cleaner than old-style iterators
std::vector<int> nums = {1, 2, 3, 4, 5};
auto evens = nums
    | std::views::filter([](int n) { return n % 2 == 0; })
    | std::views::transform([](int n) { return n * 2; });

// Structured bindings: Cleaner unpacking
struct Point { int x, y; };
auto [x, y] = point;
```

---

## DECISIÓN 7: GTest vs Catch2 vs Doctest

### Pregunta

¿Qué testing framework?

### Opciones Consideradas

| Framework | Popularidad | Learning | GitHub Actions | Mock Support |
|-----------|------------|----------|---------------|----|
| **GTest (nuestro)** | Industrial | Media | Excelente | GMock built-in |
| **Catch2** | Creciendo | Baja | Buena | No built-in |
| **Doctest** | Emergente | Baja | Okay | No |
| **Boost.Test** | Legacy | Steep | Media | Posible |

### Decisión

**GTest (Google Test)**

### Justificación

- Estándar industrial (Google, Facebook, major projects)
- GMock integrado (para mocking de dependencias)
- GitHub Actions integration nativa
- Recruiters lo reconocen
- Excelente documentación

### Ejemplos

```cpp
// Unit test
TEST(PlaylistParser, ParseValidM3U8) {
    PlaylistParser parser;
    auto channels = parser.parseM3U8(sample_m3u8);
    ASSERT_EQ(channels.size(), 5);
    EXPECT_EQ(channels[0].name, "Channel 1");
}

// Mock dependency
class MockNetworkClient : public INetworkClient {
public:
    MOCK_METHOD(std::vector<uint8_t>, get, 
                (const std::string&), (override));
};

TEST(Decoder, HandlesNetworkError) {
    MockNetworkClient mock;
    EXPECT_CALL(mock, get).WillOnce(Throw(NetworkException()));
    // Assert error handling
}
```

---

## DECISIÓN 8: Logging: spdlog vs boost::log vs std::cerr

### Pregunta

¿Cómo loggear de forma structured?

### Opciones Consideradas

| Logger | Performance | Thread-safe | Estructura |
|--------|-------------|-------------|-----------|
| **spdlog (nuestro)** | Ultra-fast | Sí | JSON support |
| **boost::log** | Bueno | Sí | Flexible |
| **std::cerr** | Lento | Risky | Ninguno |

### Decisión

**spdlog**

### Justificación

- **Cero overhead** si logs están deshabilitados
- Thread-safe out-of-box (crítico para multi-threaded app)
- Structured logging (JSON output para análisis)
- Fácil filtrado por nivel (DEBUG, INFO, WARN, ERROR)
- Usado en industry (Discord, etc)

### Implementación

```cpp
// Singleton logger
class Logger {
    static std::shared_ptr<spdlog::logger> instance;
public:
    static void info(const std::string& msg) {
        instance->info(msg);  // Thread-safe
    }
};

// En threads
void networkThread() {
    Logger::info(fmt::format("Downloaded {} bytes", total_bytes));
}

void decoderThread() {
    Logger::warn(fmt::format("Corrupt frame at {}", timestamp));
}
```

---

## DECISIÓN 9: Error Handling: Exceptions vs Return Codes

### Pregunta

¿Propagar errores mediante exceptions o return codes?

### Opciones Consideradas

| Enfoque | Pros | Contras |
|---------|------|---------|
| **Exceptions (nuestro)** | Limpio, fuerza handling | Performance, stack unwinding |
| **Return codes** | Rápido, control | Verbose, fácil ignorar |
| **Result<T, E>** | Modern, type-safe | Template complexity |

### Decisión

**Exceptions para error handling, con cuidado en hot paths**

### Justificación

```cpp
// Clean interface
try {
    auto channels = networkClient.downloadPlaylist(url);
    for (const auto& ch : channels) {
        player.play(ch);
    }
} catch (const NetworkException& e) {
    logger.error(fmt::format("Network error: {}", e.what()));
    player.setState(State::PAUSED);
} catch (const DecoderException& e) {
    logger.error(fmt::format("Decoder error: {}", e.what()));
    player.skipFrame();
}
```

Vs return codes (verbose):

```cpp
// Tedious
auto channels = networkClient.downloadPlaylist(url);
if (!channels) {
    logger.error("Network error");
    // ... handle
}
```

### Caveat: No en render/decode hot path

```cpp
// DON'T throw in hot loop (performance hit)
void decoderThread() {
    for (;;) {
        try {
            decodeFrame();  // Throws on corrupt
        } catch (const CorruptFrameException&) {
            // Just skip, don't throw
            continue;
        }
    }
}
```

---

## DECISIÓN 10: Circular Buffer: Fixed Size vs Dynamic

### Pregunta

¿Buffer de tamaño fijo (simpler) o dinámico (flexible)?

### Opciones Consideradas

| Tipo | Memoria | Predictabilidad | Complejidad |
|------|---------|-----------------|-------------|
| **Fixed (nuestro)** | Preallocated | Sí, memoria constante | Baja |
| **Dynamic** | On-demand | Impredictible | Media |
| **Ring buffer** | Fixed, reutilizable | Sí, mejor utilización | Media |

### Decisión

**Fixed-size circular ring buffer (50 MB para network)**

### Justificación

```cpp
// Fixed ring buffer
class CircularBuffer {
    std::vector<uint8_t> buffer;  // 50MB preallocated
    size_t write_pos = 0;
    size_t read_pos = 0;
    
    void enqueue(const std::vector<uint8_t>& data) {
        // Wrap around when reaching end
        // No allocation, no fragmentation
    }
};
```

Ventajas:
- ✅ Predecible (no GC pauses)
- ✅ Memory efficient (no fragmentation)
- ✅ No alloc en render thread (RT-safe)
- ❌ Size fijo (tuning required)

### Buffer Sizes (Justificados)

| Buffer | Size | Justificación |
|--------|------|---------------|
| Network → Decoder | 50 MB | 2-3 seg @ 20 Mbps |
| Decoder → Render | 300 MB (video) | 10-20 frames HD @ 30fps |
| Decoder → Render | 10 MB (audio) | 2-3 seg PCM |

---

## DECISIÓN 11: Testing Strategy: Unit vs Integration vs E2E

### Pregunta

¿Qué tipo de testing, en qué proporción?

### Proporción de Tests

```
Unit Tests (50%)
├─ Playlist parser (5 tests)
├─ HTTP client (5 tests)
├─ Circular buffer (5 tests)
├─ Decoder wrapper (5 tests)
├─ And more...

Integration Tests (30%)
├─ Network → Decoder (3 tests)
├─ Decoder → Render (3 tests)
├─ Full pipeline (3 tests)

E2E / Manual (20%)
├─ Real IPTV playlist (manual)
├─ Error recovery (manual)
├─ Long-running stability (manual)
```

### Justificación

- **50% Unit:** Fast CI feedback, catch early bugs
- **30% Integration:** Verify components work together
- **20% E2E:** Real scenarios, confidence

---

## DECISIÓN 12: GitHub Actions Platforms

### Pregunta

¿Qué plataformas testear en CI?

### Decisión

**Linux (primary), Windows + macOS (secondary)**

### Justificación

| Platform | Rationale |
|----------|-----------|
| **Linux** | Primary dev platform, fastest CI, cheapest |
| **Windows** | Validar MSVC + win32 APIs (threads, sockets) |
| **macOS** | Validar Clang + Objective-C interop (if GUI touches) |

### CI Matrix

```yaml
jobs:
  linux:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc-11, clang-13]
        
  windows:
    runs-on: windows-latest
    compiler: msvc
    
  macos:
    runs-on: macos-latest
    compiler: clang
```

---

## DECISIÓN 13: Code Style: Google vs LLVM vs Custom

### Pregunta

¿Qué estilo de código?

### Decisión

**Google C++ Style Guide (via .clang-format)**

### Justificación

- Estándar recognizable
- clang-format automáticamente enforce
- Readable (2-space indents)
- Usado en industria

---

## DECISIÓN 14: Documentation: Doxygen vs Sphinx vs Asciidoc

### Pregunta

¿Cómo generar API docs?

### Decisión

**Doxygen (API) + Markdown (guides)**

### Justificación

```bash
doxygen Doxyfile  # Generate HTML from comments
```

Ejemplo:

```cpp
/// Downloads an M3U8 playlist from the given URL.
/// 
/// @param url The URL of the M3U8 file
/// @return Vector of channels parsed from the playlist
/// @throws NetworkException if download fails
std::vector<Channel> downloadPlaylist(const std::string& url);
```

Genera HTML documentation automáticamente.

---

## DECISIÓN 15: Deployment: Docker vs Native Binaries

### Pregunta

¿Cómo distribuir la aplicación?

### Decisión

**Docker primary, native binaries secondary**

### Justificación

```dockerfile
# Dockerfile: reproducible, dependency-free environment
FROM ubuntu:22.04
RUN apt-get install ...
COPY conanfile.txt conanfile.py
RUN conan install ...
RUN cmake -B build && cmake --build build
ENTRYPOINT ["./build/iptv_player"]
```

Ventajas:
- ✅ Reproducible across machines
- ✅ No dependency hell
- ✅ Easy for interviews (just `docker run`)

Native binaries:
- Linux: AppImage
- Windows: .exe (or .msi)
- macOS: .dmg

---

## DECISION 16: Bonus Features Priority (if time permits)

### Phase 2 Features (weeks 7-8)

| Feature | Effort | Learning | Priority |
|---------|--------|----------|----------|
| Seeking | Medium | Medium | HIGH |
| Adaptive bitrate | Medium | High | MEDIUM |
| A/V Sync | Low | Low | HIGH |
| WebSocket telemetry | High | High | LOW |
| Qt GUI | High | Medium | LOW |
| POSIX sockets | High | High | MEDIUM |
| NVIDIA NVDEC | High | Medium | LOW |

### Recomendación

1. Prioritize: Seeking, A/V Sync, Error recovery
2. Medium: Adaptive bitrate, Performance optimization
3. Bonus: POSIX sockets (if energy), NVIDIA NVDEC (risky)
4. Skip (out of scope): Qt rewrite, MQTT, Kubernetes

---

## MATRIZ DE DECISIONES RESUMIDA

```
┌──────────────────────────────────────────────────────────┐
│ DECISION SUMMARY                                         │
├─────────┬──────────────────┬───────────┬─────────────────┤
│ Aspecto │ Opción Elegida   │ MVP Only? │ Bonus Upgrade?  │
├─────────┼──────────────────┼───────────┼─────────────────┤
│ Threads │ Multi (3)        │ Sí        │ N/A             │
│ Sync    │ Mutex + CV       │ Sí        │ Lock-free       │
│ GUI     │ ImGui            │ Sí        │ Qt rewrite      │
│ HTTP    │ libcurl          │ Sí        │ POSIX sockets   │
│ Decode  │ FFmpeg           │ Sí        │ NVIDIA NVDEC    │
│ C++     │ C++20            │ Sí        │ N/A             │
│ Testing │ GTest + fixtures │ Sí        │ More tests      │
│ Logging │ spdlog           │ Sí        │ N/A             │
│ Errors  │ Exceptions       │ Sí        │ N/A             │
│ Buffer  │ Fixed ring       │ Sí        │ Adaptive size   │
│ Deploy  │ Docker           │ Sí        │ Native packages │
└─────────┴──────────────────┴───────────┴─────────────────┘
```

---

## CONCLUSIÓN

Estas decisiones no son "random." Cada una está justificada:

✅ **Por realismo:** Esto es lo que hacen compañías reales  
✅ **Por learning:** Demuestra competencia real  
✅ **Por portfolio:** Impresiona recruiters  
✅ **Por práctica:** Tiempos realistas respetados

**Cuando alguien pregunta "¿por qué C++ y no Rust?"** → tienes documento.  
**Cuando alguien pregunta "¿por qué threads?"** → tienes respuesta clara.  
**Cuando alguien pregunta "¿FFmpeg es overkill?"** → explicas tradeoff.

---

**Version:** 1.0  
**Last updated:** September 2026  
**Status:** Decision framework complete ✅
