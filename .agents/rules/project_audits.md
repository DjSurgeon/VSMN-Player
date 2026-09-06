# Auditoría y Reglas Arquitectónicas del Proyecto IPTV Player

Este archivo contiene el checklist completo y exhaustivo de auditoría arquitectónica y buenas prácticas que DEBE cumplirse al picar la lógica de negocio del reproductor IPTV. Úsalo como referencia continua para blindar el proyecto ante cualquier evaluación técnica.

## 1. Entorno de Desarrollo y Contenedores (Docker / Host)
- **Alineación de UID/GID:** Confirmar que los archivos creados dentro del contenedor sigan perteneciendo a tu usuario de Fedora y no a `root`.
- **Aceleración Hardware Activa:** Comprobar que `/dev/dri` responde sin fallback a software (`llvmpipe`) cuando se levante la ventana OpenGL.
- **Socket de Audio Persistente:** Validar que `PULSE_SERVER` apunte al socket activo de PipeWire/PulseAudio y no silencie la salida SDL2.
- **Persistencia de Conan:** Asegurar que el volumen de caché (`~/.conan2` o named volume) conserve los paquetes precompilados tras reiniciar el contenedor.
- **Flags SELinux en Fedora:** Confirmar que los montajes de directorios mantengan la flag `:z` para evitar bloqueos silenciosos de permisos.
- **Hermeticidad de Build:** El contenedor debe compilar exactamente igual en tu máquina local que en una máquina limpia sin dependencias previas instaladas en el host.

## 2. Cadena de Construcción (CMake y Conan 2)
- **Estándar C++20 Forzado:** Verificar que el perfil de Conan (`compiler.cppstd=gnu20` o `20`) coincida estrictamente con `CMAKE_CXX_STANDARD 20`.
- **Flags de Advertencia Estrictos:** Comprobar que `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` estén activos en targets propios.
- **Sanitizers Mutuamente Excluyentes:** Garantizar que el `CMakeLists.txt` aborte si se intentan activar ASan y TSan a la vez (son incompatibles en compilación simultánea).
- **Compilación de Dependencias Faltantes:** Mantener `--build=missing` en llamadas de Conan para que no aborte ante la falta de binarios para versiones modernas de GCC/Clang.
- **Separación Interfaz/Implementación:** Mantener la política de headers públicos bajo `include/iptv/` y detalles privados bajo `src/`.
- **Generación de `compile_commands.json`:** `CMAKE_EXPORT_COMPILE_COMMANDS ON` debe estar activo para alimentar a herramientas de análisis estático e indexadores (Clangd).

## 3. CI/CD y Calidad de Código (GitHub Actions)
- **Matriz de Compiladores:** Pipelines configurados con GCC y Clang en modo Release y Debug.
- **Job Dedicado para Sanitizers:** Ejecución de tests bajo ASan (con detección de memory leaks y dangling pointers).
- **Job Específico para ThreadSanitizer:** Workflow que corra la suite con `-fsanitize=thread` en cada PR.
- **Formateo Automatizado:** Verificación de `.clang-format` en la CI para rechazar PRs que no cumplan el estilo de código del proyecto.
- **Análisis Estático (Linters):** Paso de `clang-tidy` con comprobaciones de C++ Core Guidelines y descarte de llamadas inseguras.
- **Protección de Ramas:** Bloquear push directo a `main` y `develop`; exigir PR con CI en verde y sin conflictos.

## 4. Modelo de Concurrencia y Sincronización
- **Estrategia de Parada Cooperativa:** Garantizar que todo hilo use `std::stop_token` (C++20) y que ninguna cola o llamada a red se quede colgada en bloqueos indefinidos.
- **Mecanismo de Backpressure:** Diseñar colas acotadas (bounded queues); si el decodificador es más lento que la descarga, la red debe pausarse para no desbordar la memoria.
- **Orden de Adquisición de Locks:** Documentar la jerarquía de mutexes para asegurar que dos hilos nunca intenten adquirir cerrojos en orden inverso (prevención de deadlocks).
- **Contención y Granularidad:** Mantener los bloques bajo mutex reducidos al mínimo tiempo posible; nunca decodificar paquetes ni descargar sockets dentro de una sección crítica.
- **Aislamiento de la GUI:** El bucle de eventos/render (ImGui/SDL2) debe ser exclusivamente consumidor desacoplado; nunca ejecutar I/O de red ni decodificación en el hilo principal.

## 5. Gestión de Memoria y Ciclo de Vida (C++ & FFmpeg)
- **Propiedad Exclusiva RAII:** Cero llamadas manuales a `new`/`delete` o `malloc`/`free`.
- **Wrappers para FFmpeg (C API):** Diseñar smart pointers con custom deleters para estructuras C de FFmpeg (`std::unique_ptr<AVPacket, decltype(&av_packet_free)>`, `AVFrame`, `AVFormatContext`).
- **Transferencias Zero-Copy:** Uso de semántica de movimiento (`std::move`) y vistas inmutables (`std::span`, `std::string_view`) entre los hilos del pipeline para evitar copias masivas de vídeo.
- **Política de Errores Sin Excepciones en Hot-Paths:** Usar tipos semánticos (`std::expected` o `Result<T, E>`) en rutas críticas en lugar de lanzar excepciones que degraden el rendimiento.

## 6. Contratos de Red y Streaming (HLS / MPEG-TS)
- **Abstracción del Cliente HTTP:** Crear la interfaz `INetworkClient` para inyectar mocks en pruebas unitarias sin depender de servidores externos activos.
- **Casos Límite del Parser M3U8:** Contemplar master playlists multivariante, saltos de secuencia (`#EXT-X-DISCONTINUITY`), listas dinámicas (Live) frente a estáticas (VOD), y URLs relativas frente a absolutas.
- **Gestión de Timeouts y Reintentos:** Estrategia de reintento exponencial ante fallos de conexión sin bloquear el hilo de control.
- **Validación de Paquetes TS:** Detección de paquetes corruptos o desincronizados antes de alimentar el demuxer.

## 7. Gobernanza del Proyecto y Documentación
- **Repositorio de Decisiones (ADRs):** Crear el archivo inicial `docs/adr/0001-thread-synchronization.md` justificando la elección de colas acotadas sobre modelos lock-free o reactivos.
- **Issues y Trazabilidad:** Las ramas deben nacer siempre de un Issue asignado a un Milestone (`feature/#12-bounded-queue`).
- **Git Commits Semánticos:** Aplicar convención estricta (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `ci:`).
- **Documentación Viva (Docs as Code):** Mantener sincronizado el sitio en GitHub Pages mediante `mkdocs.yml` con diagramas de flujo que expliquen la interacción entre los hilos.
