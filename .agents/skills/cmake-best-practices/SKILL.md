# Skill: Generador de CMake Moderno con Consumo Eficiente

## Propósito
Generar y revisar archivos `CMakeLists.txt` aplicando buenas prácticas de CMake Moderno (Target-based, CMake 3.15+), minimizando el consumo de tokens y llamadas de red.

## 1. Política de Acceso a Recursos (Ahorro de Tokens)

### Nivel 1: Consulta Rápida Local (Por defecto)
- Para tareas habituales (crear targets, enlazar librerías con `target_link_libraries`, añadir subdirectorios, configurar `target_include_directories`), consulta **únicamente** el archivo local:
  `reference/cheatsheet.md`
- No hagas llamadas a internet si la respuesta se resuelve con este resumen.

### Nivel 2: Fetch Web Quirúrgico (Bajo Demanda)
- **Solo** invoca la herramienta MCP de red (`fetch-web`) hacia la web oficial si ocurre uno de estos supuestos:
  1. El usuario solicita explícitamente validar una característica contra la web más reciente.
  2. La tarea implica funcionalidades avanzadas o dependencias externas complejas (ej. `FetchContent`, integración de tests modernos, generadores específicos de CMake) que no estén detalladas en el archivo local.
- **Regla de extracción eficiente:** Al hacer `fetch` de la web, extrae únicamente la sección o encabezado concreto que responda a la duda. No cargues ni resumas la página completa en el contexto.

## 2. Directrices Obligatorias de CMake
- Siempre Target-based: Usa `target_*` (`target_include_directories`, `target_compile_features`, etc.) con visibilidad explícita (`PRIVATE`, `PUBLIC`, `INTERFACE`).
- Prohibidas funciones globales obsoletas: Nada de `include_directories()`, `link_libraries()` o manipulación global de `CMAKE_CXX_FLAGS`.

## 3. Verificación Local
Si el entorno dispone de CMake, valida la sintaxis sin compilar ejecutando:
```bash
cmake -B build/dry_run -S . -N
