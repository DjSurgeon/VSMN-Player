# 🔬 Guía Completa: Cómo Ejecutar el Pipeline de Calidad Localmente

Esta guía te enseña, paso a paso, cómo reproducir **exactamente** lo que hace nuestro CI/CD de GitHub Actions pero en tu propia máquina. Si la pipeline de GitHub falla, con este documento puedes diagnosticar y arreglar cualquier problema sin esperar a que GitHub te diga qué pasó.

---

## Prerrequisitos

Antes de empezar, asegúrate de tener el entorno de desarrollo levantado:

- Docker y docker-compose instalados
- El contenedor de desarrollo corriendo (`docker-compose up -d dev`)

Todos los comandos de esta guía se ejecutan **dentro del contenedor**. Puedes entrar así:

```bash
docker-compose exec dev bash
```

O puedes ejecutar comandos directamente desde tu terminal host prefijándolos con:

```bash
docker-compose exec dev bash -c "COMANDO_AQUÍ"
```

> [!TIP]
> Si usas VSCode con DevContainers, ya estás dentro del contenedor automáticamente. Todos los comandos los puedes ejecutar directamente en la terminal integrada.

---

## Paso 1: Verificar el Formato del Código (`clang-format`)

### ¿Qué es?

`clang-format` es una herramienta que reformatea automáticamente el código C++ según unas reglas definidas en el archivo `.clang-format` de la raíz del proyecto. Nuestras reglas están basadas en el **Google C++ Style Guide**.

### ¿Por qué es importante?

Si un solo espacio, tabulación o llave está fuera de lugar, la pipeline de GitHub **fallará**. Esto garantiza que todo el código del proyecto se lee exactamente igual, sin importar quién lo escribió.

### Comando: Solo VERIFICAR (no toca nada)

```bash
cd /workspace

find src include tests benchmarks \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror
```

**Explicación línea por línea:**

- `find src include tests benchmarks`: Busca recursivamente en estas 4 carpetas.
- `-name '*.cpp' -o -name '*.hpp' -o -name '*.h'`: Filtra solo archivos C++ (fuentes y cabeceras).
- `| xargs clang-format`: Pasa cada archivo encontrado como argumento a `clang-format`.
- `--dry-run`: **NO modifica nada**. Solo simula lo que haría.
- `--Werror`: Si encuentra alguna diferencia entre el formato actual y el esperado, devuelve un código de error (exit code 1). Esto es lo que hace que la pipeline falle.

### Comando: CORREGIR automáticamente

Si el paso anterior falla, ejecuta esto para que `clang-format` reformatee todos los archivos:

```bash
find src include tests benchmarks \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format -i
```

- `-i`: Significa "in-place". Modifica los archivos directamente.

> [!IMPORTANT]
> Después de corregir, vuelve a ejecutar el comando de verificación para asegurarte de que todo está limpio.

---

## Paso 2: Análisis Estático del Código (`clang-tidy`)

### ¿Qué es?

`clang-tidy` es un analizador estático que lee tu código C++ y busca bugs potenciales, malas prácticas, y oportunidades de modernización. Es mucho más profundo que `clang-format`: analiza el AST (Árbol de Sintaxis Abstracta) completo de cada archivo.

### ¿Por qué es importante?

Detecta cosas como:

- Variables sin inicializar
- Conversiones de tipos peligrosas
- Código que podría usar features modernos de C++20
- Violaciones de buenas prácticas (como olvidar `const` donde debería ir)

Las reglas están definidas en el archivo `.clang-tidy` de la raíz del proyecto.

### Comando

```bash
cd /workspace

run-clang-tidy -p build/Debug -quiet src/
```

**Explicación:**

- `run-clang-tidy`: Es un wrapper oficial de LLVM que ejecuta `clang-tidy` en paralelo sobre múltiples archivos.
- `-p build/Debug`: Le dice dónde está el archivo `compile_commands.json` (generado por CMake). Este archivo contiene los flags exactos de compilación de cada `.cpp`, y `clang-tidy` los necesita para entender tu código.
- `-quiet`: Reduce la cantidad de output. Solo muestra los warnings/errores reales.
- `src/`: La carpeta a analizar.

> [!NOTE]
> Este comando tarda entre 1 y 3 minutos porque parsea el AST completo de cada unidad de traducción. Los warnings de "complejidad ciclomática" en el parser M3U8 son normales y esperados — un parser de texto siempre tiene muchos `if/for` anidados.

---

## Paso 3: Análisis Estático Adicional (`cppcheck`)

### ¿Qué es?

`cppcheck` es otra herramienta de análisis estático, complementaria a `clang-tidy`. Encuentra cosas que `clang-tidy` no ve, como:

- Memory leaks sutiles
- Variables usadas antes de ser inicializadas
- Código muerto (dead code)
- Desbordamientos de buffer

### Comando

```bash
cd /workspace

cppcheck --enable=all \
         --suppress=missingIncludeSystem \
         --error-exitcode=1 \
         -I include \
         src/
```

**Explicación:**

- `--enable=all`: Activa TODOS los checks (style, performance, portability, warning, information).
- `--suppress=missingIncludeSystem`: Silencia los avisos de cabeceras del sistema (`<vector>`, `<string>`, etc.) que cppcheck no puede encontrar porque no tiene acceso a las rutas de GCC.
- `--error-exitcode=1`: Si encuentra un error real, devuelve exit code 1 (hace fallar la pipeline).
- `-I include`: Le dice dónde están nuestras cabeceras públicas.
- `src/`: La carpeta a analizar.

> [!NOTE]
> Si `cppcheck` no está instalado en tu contenedor, puedes instalarlo con `apt-get install -y cppcheck` o reconstruir la imagen Docker.

---

## Paso 4: Compilar el Proyecto (Build)

### ¿Qué es?

CMake es nuestro sistema de generación de builds. Primero "configura" (genera los Makefiles) y luego "compila" (ejecuta `make`).

### Prerrequisito: Configurar CMake (solo la primera vez)

Si es la primera vez que compilas, o si has cambiado el `CMakeLists.txt`:

```bash
cd /workspace

# Instalar dependencias con Conan
conan install . --output-folder=build/Debug --build=missing

# Configurar CMake en modo Debug
cmake -B build/Debug \
      -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_COVERAGE=ON
```

**Explicación:**

- `conan install .`: Lee el `conanfile.py` y descarga/compila todas las dependencias (spdlog, curl, gtest, ffmpeg...).
- `--output-folder=build/Debug`: Genera los archivos de configuración de Conan dentro de `build/Debug/`.
- `--build=missing`: Si no hay un binario pre-compilado disponible, lo compila desde el código fuente.
- `cmake -B build/Debug`: Genera los Makefiles en la carpeta `build/Debug`.
- `-DCMAKE_TOOLCHAIN_FILE=...`: Le dice a CMake que use el archivo generado por Conan para encontrar las librerías.
- `-DCMAKE_BUILD_TYPE=Debug`: Compilación con símbolos de depuración (`-g`) y sin optimización (`-O0`).
- `-DENABLE_COVERAGE=ON`: Inyecta los flags de cobertura (`--coverage`) para que `gcovr` funcione después.

### Comando: Compilar

```bash
cmake --build build/Debug -j$(nproc)
```

**Explicación:**

- `cmake --build build/Debug`: Ejecuta el build dentro de la carpeta especificada.
- `-j$(nproc)`: Usa todos los núcleos de CPU disponibles para compilar en paralelo. `$(nproc)` devuelve el número de cores de tu máquina (ejemplo: `-j8` en una máquina de 8 cores).

### Compilar en modo Release (producción)

```bash
cmake --build build/Release -j$(nproc)
```

> [!TIP]
> Si solo quieres verificar que el código compila limpiamente sin warnings, añade `-DENABLE_WARNINGS_AS_ERRORS=ON` durante la configuración. Cualquier warning será un error fatal.

---

## Paso 5: Ejecutar los Tests Unitarios

### ¿Qué es?

Usamos **Google Test (GTest)** como framework de testing. Nuestros tests cubren:

- Respuestas HTTP (move-semantics, preallocación de memoria)
- Política de reintentos (retry policy)
- Parser M3U8 (VOD, Live, Master, edge cases)
- Tests de red avanzados (descargas concurrentes, stall detection)

### Comando

```bash
cd /workspace/build/Debug

ctest --output-on-failure
```

**Explicación:**

- `ctest`: Es el ejecutor de tests integrado en CMake. Descubre automáticamente todos los tests registrados.
- `--output-on-failure`: Si algún test falla, muestra la salida completa del test (qué esperaba vs qué obtuvo). Sin este flag, solo verías "FAILED" sin contexto.

### Ejecutar un test específico

Si solo quieres correr un test concreto (por ejemplo, para depurarlo):

```bash
ctest -R "M3u8ParserTest.ParsesValidVODPlaylist" --output-on-failure
```

- `-R "NOMBRE"`: Filtra tests por expresión regular. Solo ejecuta los que coincidan.

### Ejecutar los tests directamente (sin ctest)

```bash
./tests/unit_tests --gtest_filter="M3u8ParserTest.*"
```

- `--gtest_filter="PATRÓN"`: Filtro nativo de GTest. Útil para depuración rápida.

---

## Paso 6: Generar Reporte de Cobertura de Código

### ¿Qué es?

La cobertura de código mide qué porcentaje de tu código fuente fue ejecutado durante los tests. Si tienes un 80% de cobertura, significa que el 20% de tu código no fue tocado por ningún test — y eso es potencialmente peligroso.

### Prerrequisito

El proyecto debe haberse compilado con `-DENABLE_COVERAGE=ON` (ver Paso 4).

### Comando

```bash
cd /workspace/build/Debug

# 1. Limpiar datos de cobertura antiguos
find . -name '*.gcda' -delete

# 2. Ejecutar los tests (generan los archivos .gcda)
ctest --output-on-failure

# 3. Generar el reporte
gcovr --root /workspace \
      --filter /workspace/src/ \
      --filter /workspace/include/ \
      --print-summary
```

**Explicación:**

- `find . -name '*.gcda' -delete`: Borra los datos de cobertura de ejecuciones anteriores. Si no lo haces, los datos se mezclan y los porcentajes son incorrectos.
- `gcovr`: Lee los archivos `.gcda` (generados al ejecutar los tests) y `.gcno` (generados al compilar) y calcula qué líneas fueron ejecutadas.
- `--root /workspace`: Le dice cuál es la raíz del proyecto.
- `--filter /workspace/src/ --filter /workspace/include/`: Solo analiza nuestro código. Sin esto, incluiría las librerías de GTest y las dependencias de Conan en las estadísticas.
- `--print-summary`: Muestra un resumen en la terminal.

### Generar reporte HTML (más visual)

```bash
gcovr --root /workspace \
      --filter /workspace/src/ \
      --filter /workspace/include/ \
      --html --html-details \
      -o coverage.html
```

Esto genera un archivo `coverage.html` que puedes abrir en el navegador. Te marca en verde las líneas cubiertas y en rojo las que ningún test ejecutó.

---

## Paso 7: Lint de Markdown

### ¿Qué es?

`markdownlint` verifica que todos nuestros archivos `.md` (README, TESTING, DEVELOPMENT, etc.) siguen un formato consistente. Cosas como:

- Líneas en blanco alrededor de los headings
- Listas correctamente formateadas
- Sin espacios en blanco al final de las líneas

### Comando: Solo VERIFICAR

```bash
cd /workspace

npx -y markdownlint-cli "*.md" "docs/**/*.md" --config .markdownlint.json
```

**Explicación:**

- `npx -y markdownlint-cli`: Descarga y ejecuta `markdownlint-cli` sin instalarlo globalmente. El `-y` acepta automáticamente la instalación.
- `"*.md"`: Todos los archivos markdown en la raíz.
- `"docs/**/*.md"`: Todos los archivos markdown en `docs/` y sus subdirectorios.
- `--config .markdownlint.json`: Usa nuestro archivo de configuración que define qué reglas activar/desactivar.

### Comando: CORREGIR automáticamente

```bash
npx -y markdownlint-cli --fix "*.md" "docs/**/*.md" --config .markdownlint.json
```

- `--fix`: Corrige automáticamente los errores que puede (espacios en blanco, líneas faltantes alrededor de headings, etc.).

> [!IMPORTANT]
> Algunos errores no se pueden corregir automáticamente (por ejemplo, headings duplicados). Esos tendrás que arreglarlos a mano.

---

## Paso 8: Build de Producción con Docker

### ¿Qué es?

Este comando construye la imagen Docker de producción completa. Simula exactamente lo que haría el CI de GitHub cuando hace el release.

### Comando

```bash
cd /workspace

docker build -f Dockerfile.prod -t iptv-player:prod .
```

**Explicación:**

- `docker build`: Construye una imagen Docker.
- `-f Dockerfile.prod`: Usa nuestro Dockerfile de producción (multi-stage).
- `-t iptv-player:prod`: Le pone el nombre `iptv-player` con la etiqueta `prod`.
- `.`: El contexto de build es el directorio actual. Docker enviará todos los archivos (excepto los de `.dockerignore`) al daemon.

> [!WARNING]
> Este comando tarda entre 10 y 30 minutos la primera vez porque compila FFmpeg y todas las dependencias desde cero dentro del contenedor. Las siguientes ejecuciones son mucho más rápidas gracias a la caché de Docker.

---

## Resumen: El Pipeline Completo en un Solo Script

Si quieres ejecutar todo de una vez (exactamente lo que hace GitHub Actions):

```bash
#!/bin/bash
set -e  # Abortar al primer error

echo "🔍 [1/7] clang-format..."
find src include tests benchmarks \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror

echo "🔍 [2/7] clang-tidy..."
run-clang-tidy -p build/Debug -quiet src/

echo "🔍 [3/7] cppcheck..."
cppcheck --enable=all --suppress=missingIncludeSystem --error-exitcode=1 -I include src/

echo "🔨 [4/7] Build Debug..."
cmake --build build/Debug -j$(nproc)

echo "🧪 [5/7] Unit Tests..."
cd build/Debug && ctest --output-on-failure && cd ../..

echo "📊 [6/7] Code Coverage..."
cd build/Debug && find . -name '*.gcda' -delete && ctest --output-on-failure \
  && gcovr --root /workspace --filter /workspace/src/ --filter /workspace/include/ --print-summary \
  && cd ../..

echo "📝 [7/7] Markdown Lint..."
if command -v npx &> /dev/null; then
  npx -y markdownlint-cli "*.md" "docs/**/*.md" --config .markdownlint.json
else
  echo "⚠️ npx no instalado, saltando lint de markdown."
fi

echo ""
echo "🎉 ¡PIPELINE COMPLETO! Todo verde."
```

Guarda esto como `scripts/local-pipeline.sh`, dale permisos (`chmod +x scripts/local-pipeline.sh`) y ejecútalo cuando quieras verificar que todo está correcto antes de hacer push.

---

## Troubleshooting: Errores Comunes

### "command not found: clang-format"

Estás fuera del contenedor Docker. Entra con `docker-compose exec dev bash`.

### "npx: command not found" al ejecutar Markdown Lint

El contenedor de C++ no incluye Node.js por defecto. El script moderno de `local-pipeline.sh` lo ignora de forma segura usando `command -v npx`. Si usas una versión vieja del script, añade esa comprobación o ignora el error.

### "compile_commands.json not found"

No has configurado CMake aún. Ejecuta el paso de "Configurar CMake" del Paso 4.

### "libgcov profiling error: overwriting..."

Inofensivo. Significa que hay datos de cobertura de una ejecución anterior. Ejecuta `find . -name '*.gcda' -delete` antes de los tests.

### "gcovr: command not found"

Instálalo con `pip3 install gcovr` dentro del contenedor.

### Un test falla pero en GitHub pasa (o viceversa)

Asegúrate de que tu contenedor Docker está actualizado. Reconstruye con `docker-compose build dev`.

### "ERROR: vaapi/system: Error in system_requirements()"

Este es uno de los errores más comunes al añadir FFmpeg como dependencia. El log completo se ve así:

```text
Skipped binaries
    expat/2.8.4, flex/2.6.4, libffi/3.4.8, libiconv/1.17, libxml2/2.15.3, zlib/1.3.2
egl/system: System requirements: libegl-dev already installed
opengl/system: System requirements: libgl-dev already installed
ERROR: vaapi/system: Error in system_requirements() method, line 38
    apt.install(["libva-dev"], update=True, check=True)
    ConanException: System requirements: 'libva-dev' are missing but can't install
    because tools.system.package_manager:mode is 'check'.
    Please update packages manually or set 'tools.system.package_manager:mode'
    to 'install' in the [conf] section of the profile, or in the command line
    using '-c tools.system.package_manager:mode=install'
Error: Process completed with exit code 1.
```

**¿Qué significa?** FFmpeg depende de `vaapi/system` (Video Acceleration API), que necesita instalar el paquete del sistema operativo `libva-dev` con `apt-get`. Conan por defecto está en modo `check` (solo comprueba si existe, pero NO instala nada). Esto falla si la librería no está preinstalada.

**Solución:** Añade estos dos flags al comando `conan install`:

```bash
conan install . --build=missing -s build_type=Debug \
  -c tools.system.package_manager:mode=install \
  -c tools.system.package_manager:sudo=True
```

- `tools.system.package_manager:mode=install`: Autoriza a Conan a ejecutar `apt-get install` automáticamente cuando una dependencia del sistema no está presente.
- `tools.system.package_manager:sudo=True`: Usa `sudo` para la instalación (necesario en GitHub Actions y en máquinas donde no eres root). En Docker donde ya eres root, usa `sudo=False`.

> [!IMPORTANT]
> Este fix debe aplicarse en **todos** los `conan install` del proyecto: en `ci-linux.yml` (4 jobs), en `Dockerfile.prod`, y en cualquier script local.

### "ConanException: Current Conan version (2.0.0) does not satisfy the defined one (>=2.0.5)"

```text
ERROR: Package 'libtool/2.4.7' not resolved: libtool/2.4.7: Cannot load recipe.
Error loading conanfile at '...': Current Conan version (2.0.0) does not satisfy
the defined one (>=1.60.0 <2 || >=2.0.5).
```

**¿Qué significa?** Las recetas de ConanCenter (el repositorio central de paquetes) se actualizan continuamente. Algunas recetas recientes exigen una versión mínima de Conan superior a `2.0.0`. Si tu `Dockerfile.dev` o `Dockerfile.prod` tiene fijado `pip install conan==2.0.0`, fallará.

**Solución:** Actualiza la versión de Conan:

```bash
pip3 install "conan>=2.5.0"
```

Y asegúrate de que los `Dockerfile.dev` y `Dockerfile.prod` usan `"conan>=2.5.0"` en vez de `conan==2.0.0`.
