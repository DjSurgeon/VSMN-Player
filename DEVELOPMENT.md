# 🛠️ Guía Rápida de Desarrollo (VSMN-Player)

¡Bienvenido al motor C++ de VSMN-Player!
Hemos configurado este repositorio para que tu experiencia de desarrollo sea **Plug & Play**. No necesitas instalar compiladores, librerías complejas ni pelear con CMake en tu sistema operativo. Todo está encapsulado.

---

## 🚀 Método 1: La Magia de VSCode (Recomendado para todos)

Este es el *Happy Path*. Si sigues esto, estarás programando en 2 minutos.

### Prerrequisitos

1. [Docker](https://www.docker.com/) instalado y encendido.
2. [Visual Studio Code](https://code.visualstudio.com/) instalado.
3. La extensión **Dev Containers** instalada en tu VSCode (`ms-vscode-remote.remote-containers`).

### Pasos

1. Abre VSCode y clona este repositorio.
2. Te aparecerá un cartel verde abajo a la derecha diciendo: *"Folder contains a Dev Container configuration file"*. Haz clic en **"Reopen in Container"**.
3. **¡Eso es todo!**
   - VSCode levantará un entorno Ubuntu aislado.
   - Instalará automáticamente CMake, GCC 13, Clang, FFmpeg, SDL2 y Conan 2.0.
   - Instalará las mejores extensiones de C++ por ti (`clangd` para autocompletado inteligente).

### Compilar y Ejecutar Visualmente

- **Compilar:** Mira la barra inferior azul de VSCode. Haz clic en `[Build]`.
- **Ejecutar Tests:** Ve a la pestaña del "Matraz" (Testing) en la barra lateral izquierda y dale al botón de Play verde.
- **Debuggear:** Pon un punto de interrupción rojo al lado del número de línea y pulsa `F5`.

---

## 💻 Método 2: La Terminal (Hardcore Mode)

Si usas CLion, Vim, o prefieres controlar la terminal directamente, este es el flujo manual. Asegúrate de tener dependencias básicas instaladas (C++20 Compiler, CMake, Conan, FFmpeg, SDL2).

### Setup y Build

```bash
# 1. Instalar dependencias con Conan (Crea los archivos de toolchain de CMake)
conan install . --build=missing -s build_type=Debug

# 2. Configurar el proyecto con CMake apuntando al toolchain de Conan
cmake -B build/Debug -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug

# 3. Compilar usando todos tus núcleos
cmake --build build/Debug -j$(nproc)
```

### Correr el Proyecto y Tests

```bash
# Ejecutar el reproductor principal
./build/Debug/bin/iptv_player

# Ejecutar la suite de tests sanitizada
cd build/Debug && ctest --verbose --output-on-failure
```

---

## 🌿 Flujo de Trabajo en Git (Git Workflow)

Para proteger la calidad del código, aplicamos reglas estrictas. No permitimos *commits* directos a la rama principal.

### El Modelo de Ramas

- `main`: Es sagrada. Refleja la versión en producción. Nadie programa aquí. Solo recibe código mediante Pull Requests.
- `develop`: Es la rama de integración. Si `main` es producción, `develop` es "staging".
- `feature/*`: **Aquí es donde debes trabajar.** (Ejemplo: `feature/añadir-hls-parser`).

### Cómo Colaborar

1. Actualiza tu local: `git checkout develop && git pull`.
2. Crea tu rama: `git checkout -b feature/mi-nueva-idea`.
3. Escribe el código y, sobre todo, **¡añade tests!** (Revisa `TESTING.md` para aprender cómo).
4. Sube la rama y abre un **Pull Request (PR)** apuntando a `develop`.
5. Los GitHub Actions (nuestro robot de Integración Continua) compilarán tu código y le pasarán los sanitizers de memoria (ASan/UBSan). Si la luz se pone verde, un mantenedor aprobará tu código.
