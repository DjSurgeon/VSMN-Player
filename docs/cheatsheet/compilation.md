# 📜 VSMN-Player: Build & Compilation Master Cheatsheet

Como tu mentor, he preparado esta **Cheatsheet definitiva de Compilación y Construcción**. Guárdala como oro en paño: resume todo lo que hemos aprendido sobre Conan 2, Modern CMake, ciclos de iteración rápida y cómo dominar el motor de compilación del VSMN-Player.

---

## 1. El Ciclo de Vida del Desarrollador (Workflow Paso a Paso)

Cada vez que arranques tu entorno de desarrollo en el contenedor Docker, este es el ritual exacto para compilar y probar el proyecto desde cero:

### Paso A: Gestión de Dependencias (Conan 2)

Conan descarga y prepara las librerías externas (CURL, GTest, Google Benchmark, cpp-httplib) y genera el toolchain que CMake utilizará.

```bash
conan install . --build=missing -s build_type=Release

```

* `--build=missing`: Compila desde código fuente si no encuentra un binario exacto para tu arquitectura.
* `-s build_type=Release`: Prepara las dependencias optimizadas (puedes cambiar a `Debug` si vas a debugear con GDB).

### Paso B: Configuración de CMake (El Generador)

Creamos y entramos en nuestra carpeta aislada de compilación, indicándole a CMake dónde encontrar el toolchain de Conan y qué módulos opcionales activar.

```bash
mkdir -p build/Release && cd build/Release

cmake ../.. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake \
  -DBUILD_TESTS=ON \
  -DENABLE_BENCHMARKS=ON

```

### Paso C: Compilación Paralela

Invocamos al compilador utilizando todos los núcleos de CPU disponibles para máxima velocidad:

```bash
cmake --build . -j$(nproc)

```

---

## 2. Panel de Control: Flags de Configuración de CMake

Nuestro `CMakeLists.txt` raíz incluye opciones personalizadas (`option()`) que puedes activar añadiendo `-D<FLAG>=ON` en el Paso B:

| Flag de CMake | Descripción | Estado Habitual |
| --- | --- | --- |
| `-DBUILD_TESTS=ON` | Compila la suite de tests unitarios e integración (GTest). | `ON` (Imprescindible) |
| `-DENABLE_BENCHMARKS=ON` | Compila los benchmarks de rendimiento (Google Benchmark). | `ON` / `OFF` |
| `-DENABLE_SANITIZERS=ON` | Activa AddressSanitizer (ASan) y UBSan para cazar bugs de memoria. | `OFF` (Para CI o auditorías) |
| `-DENABLE_TSAN=ON` | Activa ThreadSanitizer para cazar condiciones de carrera (*data races*). | `OFF` (Para auditorías de hilos) |
| `-DENABLE_COVERAGE=ON` | Inyecta contadores de cobertura de código (`gcov`/`lcov`). | `OFF` (Para reportes de CI) |
| `-DENABLE_WARNINGS_AS_ERRORS=ON` | Convierte cualquier warning del compilador en un error fatal. | `ON` (En CI/CD) |

---

## 3. El Truco del Almendruco: Iteración Ultrarrápida

Una vez que has hecho el `conan install` y el `cmake ..` iniciales, **no tienes que repetirlos nunca más** a menos que cambies el `conanfile.py` o añadas nuevos archivos de configuración a CMake.

Para el día a día picando código (`m3u8_parser.cpp`, `http_client.cpp`, etc.), el comando de recarga rápida es:

```bash
cd build/Release
cmake --build . -j$(nproc) && ctest --output-on-failure

```

*CMake es inteligente:* detectará exactamente qué fichero `.cpp` has modificado, recompilará solo ese archivo (en milisegundos) y relanzará los 56 tests automáticamente.

---

## 4. Dónde encontrar y cómo ejecutar los Binarios

Una vez compilado el proyecto dentro de `build/Release/`, dispones de los siguientes ejecutables listos para usar:

1. **La Suite de Tests Unitarios e Integración:**
```bash
./tests/unit_tests            # Ejecuta solo unitarios
./tests/integration_tests     # Ejecuta los tests de caos (Chaos Monkey)
ctest --output-on-failure     # Ejecuta TODOS los tests de forma unificada

```


2. **Los Benchmarks de Rendimiento (Throughput de Parser):**
```bash
./benchmarks/iptv_benchmarks

```


3. **El Reproductor Principal (CLI):**
```bash
./src/iptv_player

```



---

## 5. Troubleshooting: Errores Comunes de Principiante

* **Error de `CMakeCache.txt` cruzado:**
* *Síntoma:* `The current CMakeCache.txt directory ... is different than the directory ...`
* *Causa:* Intentaste ejecutar `cmake` desde el directorio incorrecto o mezclaste rutas del sistema operativo host con el contenedor Docker.
* *Solución:* Limpia la carpeta de compilación (`rm -rf build/Release`) y vuelve a empezar los pasos 2 y 3 estrictamente desde dentro de la carpeta `build/Release`.


* **Warnings del Compilador:**
* *Regla de oro:* Nuestro proyecto compila con `-Wall -Wextra -Wconversion`. Si ves texto amarillo durante la compilación, detente y lánzale un `static_cast` o limpia la variable no usada. ¡Queremos ceros amarillos en el log!