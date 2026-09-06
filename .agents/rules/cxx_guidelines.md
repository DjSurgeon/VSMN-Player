# Reglas de Desarrollo C++20 para el IPTV Player

Este archivo contiene las directrices de código para el proyecto. Como agente de IA, DEBES cumplir siempre con estas reglas al escribir o refactorizar código para asegurar la coherencia arquitectónica y el cumplimiento del estilo.

## 1. Arquitectura y Estructura (Feature-Sliced Design - FSD)
- El proyecto usa una aproximación de arquitectura en componentes/capas. 
- Los módulos principales deben estar débilmente acoplados. 
- Todo el código interno del dominio IPTV debe pertenecer al namespace `iptv` (y sub-namespaces correspondientes como `iptv::network`, `iptv::decoder`, etc.).
- Las interfaces de los componentes deben ocultar la implementación mediante abstracciones u ocultación (p. ej. pImpl o interfaces puras) cuando sea necesario.

## 2. Gestión de Memoria y Recursos (RAII)
- **Cero raw pointers (`*`)** dueños de memoria. 
- Usa SIEMPRE smart pointers (`std::unique_ptr` para propiedad única, `std::shared_ptr` para propiedad compartida).
- C/C++ libs (como FFmpeg, SDL2) devuelven punteros raw de C. DEBES envolver estos recursos inmediatamente en clases RAII o en `std::unique_ptr` con *custom deleters* (e.g., `av_free`, `SDL_DestroyWindow`). ¡Evita memory leaks!

## 3. Concurrencia y Sincronización
- Se usarán 3 hilos principales: Network, Decoder, y Render/GUI.
- Nunca compartas estado mutable sin protección. Usa `std::mutex`, `std::scoped_lock` o `std::unique_lock`.
- Para pasar datos entre hilos (ej. paquetes de red a decoder, frames a render) implementa y usa estructuras como Ring Buffers concurrentes o colas *thread-safe* con `std::condition_variable`.
- Evita el *deadlock* tomando los locks siempre en el mismo orden o minimizando la duración del lock (copia datos y libera el lock rápido).

## 4. Modern C++ (C++20)
- Usa los features de C++20 siempre que aporten claridad: `concepts`, `ranges`, `std::span`.
- Emplea inicialización uniforme (llaves `{}`).
- Usa `[[nodiscard]]` en funciones donde ignorar el valor de retorno sea un error (especialmente códigos de error de C).
- Evita macros `#define`, usa `constexpr` o `consteval`.

## 5. Estilo de Código (Google C++ Style adaptado)
- **Nomenclatura**:
  - Clases/Structs: `PascalCase` (Ej. `VideoDecoder`)
  - Funciones/Métodos: `camelCase` (Ej. `startPlayback()`)
  - Variables/Miembros: `snake_case` (Miembros de clase terminan en guion bajo `my_variable_`)
  - Constantes/Enums: `kCamelCase` o `ALL_CAPS` para macros inevitables.
- Formato: Dependeremos de `clang-format` para la tabulación y los saltos de línea.
- Incluye cabeceras ordenadas: `<system>`, `<dependencies>`, `"project_headers.hpp"`.

## 6. Tratamiento de Errores
- Utiliza excepciones estándar de C++ (`std::runtime_error`, etc.) para errores irrecuperables.
- Para errores esperados (como un stream de red cortado), usa tipos de retorno ricos como `std::expected` (si está disponible/polyfilled) o estructuras de tipo `Result` antes que lanzar excepciones en el *hot path* (flujo de render de video).
- Usa `spdlog` masivamente para dejar rastro de lo que ocurre (DEBUG, INFO, ERROR).

## 7. Testing
- Todo código de lógica y estructura de datos debe ser altamente testeable.
- Se prefiere Inyección de Dependencias (mediante interfaces o plantillas) para poder hacer mocking de la red y del hardware gráfico.
