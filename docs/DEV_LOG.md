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

---
*Fin del Hito 0. El entorno está listo y el código compila de inicio a fin.*
