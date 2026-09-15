# 📓 DEV_LOG - IPTV Player

Este documento servirá como bitácora de desarrollo (Dev Log) para registrar todos los pasos, decisiones técnicas, configuraciones y avances realizados durante la construcción del reproductor IPTV.

---

## 📅 Hito 0: Configuración del Entorno de Desarrollo (Semana 1)

**Objetivo:** Establecer un entorno de desarrollo profesional, robusto, reproducible y aislado mediante Docker, asegurando que las herramientas de C++ (compilador, CMake, Conan) funcionen correctamente junto con X11/Wayland para interfaces gráficas y PulseAudio/PipeWire para el sonido.

### 1. Sistema Base y Docker

- **`Dockerfile.dev`**: Se configuró la imagen basada en Ubuntu 22.04.
  - Se añadieron las dependencias del sistema, herramientas de compilación (`build-essential`, `ninja-build`, `cmake`) y herramientas de testing/calidad (`valgrind`, `gdb`, `clang-format`, `clang-tidy`).
  - Instalación de librerías multimedia y gráficas (FFmpeg, SDL2, OpenGL, X11, PulseAudio).
  - Actualización de Conan a la versión `2.0.17`.
  - Configuración del usuario `developer` con el mismo `UID` y `GID` del host (inyectados mediante variables de entorno) para evitar problemas de permisos de archivos.
  - Creación explícita del directorio `.conan2` con permisos correctos para el usuario `developer` y configuración de su perfil base.

- **`docker-compose.yml`**: Orquestación del contenedor.
  - Inyección dinámica de las variables `MY_UID` y `MY_GID` a través de `.env`.
  - Configuración de volúmenes compartidos: código fuente local a `/workspace` (`:z` flag) y un volumen gestionado por Docker (`conan-cache`) para la caché persistente de dependencias.
  - Mapeo del socket de audio en Fedora (PipeWire) hacia el contenedor mediante `PULSE_SERVER=/run/user/$MY_UID/pulse/native`.
  - Mapeo de X11 (`/tmp/.X11-unix`) y `.Xauthority` para habilitar el despliegue de GUI (ImGui) en el host.
  - Inclusión de un servicio secundario (`ci`) simulando un pipeline de Integración Continua (CI).

### 2. Scripts de Soporte (`scripts/`)

- **`run-env.sh`**:
  - Script para la generación del archivo local `.env` con los IDs del usuario anfitrión.
  - Habilita las conexiones GUI desde Docker hacia el servidor X local (`xhost +local:docker`).
  - Automatiza la compilación del contenedor con los `build-args` correspondientes al `MY_UID` y `MY_GID`.
- **`validation.sh`**:
  - Script de validación interna a ejecutar dentro del contenedor que comprueba: versiones del Toolchain, librerías del sistema, disponibilidad de la red, herramientas de desarrollo, perfil de Conan y hace una prueba de compilación de sanity.

### 3. Gestión de Dependencias (Conan)

- **`conanfile.py`**:
  - Transición a las prácticas de Conan 2.0 (se usó `CMakeDeps` y `CMakeToolchain`).
  - Se definieron dependencias iniciales clave:
    - `spdlog/1.13.0` (Logging avanzado).
    - `gtest/1.14.0` (Testing unitario).

### 4. Sistema de Construcción (CMake)

- **`CMakeLists.txt` (Raíz)**:
  - Estándar forzado a C++20 sin extensiones de compilador (portabilidad estricta).
  - Integración de opciones para sanitizers: **AddressSanitizer (ASan)**, **UndefinedBehaviorSanitizer (UBSan)** y **ThreadSanitizer (TSan)**.
  - Configuración paranoica de advertencias (`-Wall`, `-Wextra`, `-Wpedantic`, etc.) preparadas para `-Werror`.
  - Enlace con las dependencias proporcionadas por Conan.
  - Inicialización de la librería core (`iptv_core`) y ejecutable final (`iptv_player`).
  
- **`tests/CMakeLists.txt`**:
  - Habilitación del sistema de test y vinculación de Google Test (GTest) con descubrimiento automático (`gtest_discover_tests`).

### 5. Esqueleto Inicial de Código (`src/` y `test/`)

- **`src/main.cpp`**: Punto de entrada inicial ("Coming Soon!").
- **`src/pipeline/dummy.cpp`**: Archivo de prueba para generar la librería estática core y asegurar que la estructura FSD (Feature-Sliced Design) u otra arquitectura pueda escalarse.
- **`test/unit/test_sanity.cpp`**: Assert básico `EXPECT_EQ(2 + 2, 4)` para confirmar que el binario de GTest se ensambla y ejecuta exitosamente bajo ASan/UBSan.

### 6. Documentación y Control de Versiones

- **`.gitignore`**: Ignora directorios de CMake, Python cache, binarios, `.env` y configuraciones específicas del IDE.
- **`README.md`**: Actualización con instrucciones de inicialización del entorno en Docker y arquitectura preliminar.

### 7. Integración Continua (CI) y Resolución de Problemas (07/09/2026)

Durante la configuración de la pipeline en GitHub Actions (`.github/workflows/ci-linux.yml`), nos encontramos y resolvimos varios problemas técnicos valiosos para el futuro:

- **Depreciación en GitHub Actions:** Las acciones `actions/cache@v3` y `actions/upload-artifact@v3` fallaban automáticamente por estar obsoletas. Se migraron a `v4`.
- **Simplificación ("Menos es más"):** Se eliminaron las pipelines de Windows y macOS para reducir la complejidad inicial, centrando el Hito 0/1 estrictamente en Linux/Docker.
- **Integración Conan 2 + CMake:**
  - En entornos antiguos (Ubuntu 22.04 con CMake 3.22), CMake no soporta los *Presets* (`CMakePresets.json`) generados por Conan 2. Se solucionó invocando explícitamente el `-DCMAKE_TOOLCHAIN_FILE`.
  - En la CI, pasamos de usar una ruta estática (`--output-folder=build`) a delegar la estructura a Conan (`-s build_type=Debug/Release`), inyectando luego el toolchain correcto a CMake.
  - Para evitar bloqueos si Conan Center no tiene binarios precompilados para compiladores recientes (ej. GCC 13), se añadió la bandera `--build=missing`.
  - Se ajustó el orden en la CI para seleccionar el compilador (`CC` y `CXX`) **antes** de ejecutar `conan profile detect`, asegurando que Conan descargue dependencias coherentes con la matriz de compilación.
- **Soporte para Clang 18:** Fallos de "Invalid compiler version" al usar Clang 18 en `ubuntu-latest` se debían a que la versión de Conan estaba anclada a una muy antigua (`2.0.17`). Se solucionó desanclando la versión (`pip3 install conan`) en CI y Docker para instalar la última rama 2.x, garantizando compatibilidad con compiladores modernos.

---
*Fin del Hito 0. El entorno está listo, testeado en local y validado con éxito en CI.*

---

## 🚀 Hito 1: Auditoría YAGNI, Arquitectura DOD y Micro-optimizaciones

Durante el desarrollo de los módulos de red (`HttpClient`) y parseo (`M3u8Parser`), se llevó a cabo una auditoría arquitectónica extrema guiada por principios de **Data-Oriented Design (DOD)** y **YAGNI (You Aren't Gonna Need It)**.

### 1. La Gran Purga (YAGNI)

Se detectó sobreingeniería prematura. Las decisiones clave fueron:

- **Dependencias Fantasma:** Se eliminaron `ffmpeg`, `sdl2`, `imgui`, `opengl` y `spdlog` del `conanfile.py` y `CMakeLists.txt` porque aún no se estaban usando. Esto redujo drásticamente el peso del entorno y el tiempo de CI.
- **Fiebre de Interfaces:** Se eliminó `IPlaylistParser` ya que solo existía una implementación (`M3u8Parser`) y no se "mockeaba" en los tests. Sin embargo, se mantuvo `IHttpClient` porque era indispensable para simular la red en los tests sin tocar servidores reales.
- **Métricas Muertas y Códigos HTTP:** Se eliminó el campo `latency_` de `HttpResponse` por no usarse, y se purgaron los códigos de estado HTTP puramente anecdóticos (ej. 418 I'm a teapot), manteniendo solo los nucleares (200, 403, 404, 500) hasta que la política de reintentos avanzada (`RetryPolicy`) requiera otros (ej. 429 para Exponential Backoff).

### 2. Decisiones de Testing (White-Box vs Black-Box)

- Al testear el `AttributeScanner` (un componente fuertemente encapsulado), optamos por usar herencia protegida (`TestableAttributeScanner`) en los tests.
- **Justificación:** Aunque en la industria el *Black-Box testing* (testear solo la API pública) es la norma para evitar fragilidad, en componentes de infraestructura críticos de bajo nivel (parsers, códecs), el *White-Box testing* garantiza que los estados internos de la máquina de estados funcionen perfectamente ante bordes lógicos muy complejos, previniendo fallos catastróficos silenciosos.

### 3. Rendimiento Extremo (Zero-Allocation y Caché L1)

El objetivo de la app es poder parsear manifiestos HLS gigantes en milisegundos y evitar saturar el colector de basura y fragmentar el *Heap* de los dispositivos (móviles, Smart TVs).

- **El Heurístico SIMD:** En lugar de dejar que `std::vector` se redimensione dinámicamente decenas de veces, se inyectó una heurística que escanea el archivo con `content.find()` (acelerado por instrucciones SIMD `memmem`) para pre-reservar la memoria exacta en base al tamaño en bytes.
- **El Hito (Benchmarks):**
  - Pasamos de **18 allocations** a exactamente **2 allocations** (O(1) constante sin importar el tamaño del manifiesto).
  - La velocidad de procesamiento (Throughput) superó la asombrosa cifra de **~597 MB/s**.
  - Un archivo gigante de 5MB y 250.000 líneas se procesa en menos de **7 milisegundos**.

### 4. Resiliencia de Red (Stress Tests)

Se configuró una batería de tests de estrés integrados en la CI:

- **LowSpeedLimitStall:** Simula conexiones de 1 byte/segundo para verificar que el estrangulamiento interno (`CURLOPT_LOW_SPEED_LIMIT`) aborte y no cuelgue el hilo (Anti-Stall).
- **ConcurrentDownloads:** Somete al cliente HTTP a descargas masivas multihilo evaluadas bajo *ThreadSanitizer (TSan)* para descartar Race Conditions.
- **Fuzzer (`iptv_fuzzer`):** Habilitado con libFuzzer y ASan para inyectar basura de red y probar la invulnerabilidad de la memoria del parser.

## 🧠 Hito 2: Red Inteligente, Orquestador y ABR (Semana 3)

Con los cimientos de red (HTTP) y el parseo (M3U8) construidos, el foco fue dotar al reproductor de "inteligencia artificial" básica para adaptarse a redes hostiles y de concurrencia avanzada.

### 1. El Hilo del Orquestador (`std::jthread`)

Se ha implementado el **PlaybackOrchestrator**, un cerebro multihilo que desacopla completamente la red de la UI.

- Uso del `std::jthread` nativo de C++20, garantizando limpieza y uniones (`join`) automáticas al destruirse, eliminando fugas de hilos.
- **Cancelación Inmediata (Fast-Cancellation):** Se ha propagado `std::stop_token` desde la UI hasta el mismísimo interior de los callbacks C-Style de `libcurl` en el `NetworkComponent`. El cierre de la aplicación aborta las descargas TCP en pleno vuelo sin esperas pasivas ni *timeouts*.

### 2. Memoria Concurrente (Lock-Based)

Para la entrega de los segmentos de 5MB desde el hilo de red al hilo decodificador, se implementó `ConcurrentQueue<T>`:

- Abandono de soluciones *Lock-Free* ultra-complejas en favor de robustez industrial con `std::mutex` y `std::condition_variable`.
- El cuello de botella no está en la transferencia de punteros entre hilos (nanosegundos) sino en la red, por lo que un sistema basado en bloqueos es seguro y 100% *ThreadSanitizer-proof*.
- Se usa estricta semántica de movimiento (`std::move`) para transferir el `MediaSegmentBundle`, impidiendo copias profundas no deseadas de los bloques MPEG-TS crudos.

### 3. El Algoritmo ABR (Adaptive Bitrate) Matemático

Se diseñó el `AbrManager` como un motor "Sin Estado" puramente matemático:

- **Suavizado EWMA ($\alpha = 0.3$):** Previene que el reproductor sobrerreaccione ante picos puntuales de Wi-Fi, estabilizando la percepción del *throughput*.
- **Histéresis Asimétrica:** Bajar de resolución es instantáneo (para salvar el búfer y evitar cortes), pero subir de resolución requiere confirmación constante (3 segmentos seguidos por encima de la marca), erradicando el temido "efecto acordeón" de calidad.

**Resultado:** El reproductor es completamente Thread-Safe, aborta conexiones al instante y elige el bitrate con prudencia matemática. Los tests de integración con *Gmock* se han cubierto en verde bajo el riguroso escrutinio de TSAN/ASAN.

---

## ⚡ Hito 3: Recuperación de Zero-Copy y Benchmarks Reales (Semana 4)

El parseador M3U8 original era ultra-rápido (`O(1)` allocations), pero al resolver las URLs relativas dentro del `M3u8Parser`, se introdujeron variables `std::string` en los structs de los segmentos, lo que infló la estructura más allá del límite de la línea de caché L1 (64 bytes) y disparó las allocations en el Heap.

### 1. El Coste Oculto de std::string
Al usar `std::string` en structs masivos (250,000 segmentos), el rendimiento cayó en picado porque el Heap fue asaltado por cientos de miles de alojamientos de memoria dinámica.

### 2. Retorno al Origen (Zero-Copy)
- **`std::string_view`**: Se restauraron las referencias inmutables al buffer monolítico que descansa en RAM.
- **Delegación de Responsabilidad**: La resolución de URLs relativas a absolutas fue extirpada del parser inmutable y movida al `PlaybackOrchestrator`, que realiza la resolución dinámicamente "al vuelo" solo sobre el segmento que se va a descargar, sin acumular estado.

### 3. Resultados de los Benchmarks (Release Mode)
Se restauró el rendimiento de clase industrial:
- **Allocations**: Regresamos a 2 allocations dinámicas (el string inicial y el buffer base).
- **Throughput**: Alcanzamos **~500 MB/s** de procesado de manifiesto.
- **Latencia**: Parseo completo de 250,000 líneas en **~7.8 milisegundos**.

---

## 🧹 Hito 4: Limpieza Arquitectónica (Clean Code)

La función `downloadSegments` del `PlaybackOrchestrator` había llegado a acumular **7 parámetros**, violando las Core Guidelines de C++ y propiciando el antipatrón *Data Clump*.

### 1. PlaybackContext (Context Object)
Se implementó un struct `PlaybackContext` que envuelve las referencias a `master_url`, `master_playlist`, `current_variant_uri`, `next_sequence_index` y `current_throughput`.

### 2. Refactorización Estructural
- Se redujeron masivamente las firmas de los métodos `processVariantLoop`, `downloadSegments` y `evaluateAbrSwitch`.
- Se parcheó un bug silencioso que pasaba una URL relativa a `downloadPlaylist` gracias a un mal rebote del Hito 3.
- Todo testeado y verificado (100% Pass) contra nuestra batería Chaos Monkey, demostrando la solidez del diseño.
