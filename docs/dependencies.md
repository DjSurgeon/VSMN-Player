# 📦 Gestión de Dependencias (Conan 2.0)

VSMN-Player está construido para ser industrial y multiplataforma. En C++, descargar código fuente o usar `git submodules` suele acabar en problemas de compatibilidad (dependencias ocultas, librerías del sistema no encontradas, colisiones de ABI).

Para solucionar esto de raíz, usamos **Conan 2**.

## 1. El Archivo `conanfile.py`

En lugar del antiguo `conanfile.txt`, usamos la versión Python (`.py`) porque nos da un control programático absoluto sobre las recetas.

Nuestro archivo define:

- **`requirements()`**: Las librerías de producción. Cuando el orquestador de dependencias corre, compila desde cero (o descarga pre-compilados) cosas tan masivas como `FFmpeg`, `SDL2` e `ImGui`.
- **`build_requirements()`**: Las herramientas de validación (`benchmark`, `cpp-httplib`). No acaban en el ejecutable final, solo se usan en tiempo de prueba.
- **`default_options`**: Podemos pedirle a Conan cosas específicas, como que traiga `ImGui` pero con `SDL2` inyectado para evitarnos el boilerplate manual.

## 2. Perfiles Estrictos (Profiles)

En vez de depender de la configuración del sistema host, hemos blindado nuestros propios perfiles en `conan/profiles/`:

- **`debug`**: Compilación local rápida, llena de símbolos.
- **`release`**: `-O3 -march=native`. El compilador genera binarios bestiales optimizados para la arquitectura exacta que está ejecutando la compilación.
- **`sanitizers`**: Obliga a que cualquier paquete que compilemos localmente inyecte `-fsanitize=address,undefined`. Fundamental si dudamos de un fallo en una dependencia.

## 3. Uso

Para instalar todo el motor de dependencias (tardará un poco la primera vez, FFmpeg es gigantesco):

```bash
# Debug Mode
conan install . -pr conan/profiles/debug --build=missing

# Release Mode
conan install . -pr conan/profiles/release --build=missing
```

Conan generará los archivos `.cmake` en la carpeta `build/`. Solo tenemos que apuntar CMake hacia ellos (`-DCMAKE_TOOLCHAIN_FILE=...`) y el enlazado será mágico.
