# [NET-01] Walkthrough: Diseño de Estructuras de Red y HTTP Response

¡Misión cumplida! Hemos diseñado e implementado con éxito la primera capa fundamental del módulo de red: las estructuras de datos que representarán las respuestas HTTP, prestando especial atención a la seguridad, la eficiencia de la memoria y la escalabilidad de la arquitectura.

## 🛠️ Qué se ha construido

1. **Enum Fuertemente Tipado (`http_status_code.hpp`)**
   - Se ha creado el `enum class HttpStatusCode` para encapsular todos los códigos de estado HTTP comunes, eliminando de una vez por todas los temidos "números mágicos" o enteros desnudos.

2. **Estructura Híbrida Avanzada (`http_response.hpp`)**
   - **Zero-Copy & Move Semantics:** La clase `HttpResponse` tiene los constructores de copia explícitamente borrados (`= delete`) y solo permite movimiento (`std::move`).
   - **Eficiencia en Memoria:** Se ha incorporado un constructor que recibe el `expected_body_size` y llama internamente a `reserve()` en el `std::vector<uint8_t>`. Además, el método `appendToBody()` permite ir inyectando los paquetes de red sin reallocaciones pesadas.
   - **Const-correctness:** Todos los getters retornan referencias constantes o valores bajo la estricta mirada de `[[nodiscard]] const noexcept`.
   - **Estructura Anidada Metadata:** Los metadatos secundarios (cabeceras, redirecciones, conteo de reintentos) han sido abstraídos dentro de `HttpResponse::Metadata`, haciendo la clase principal mucho más ligera en operaciones comunes pero preparada para extenderse.
   - **Builder Pattern:** Un elegante `HttpResponseBuilder` para garantizar que la respuesta solo nazca si tiene coherencia interna (e.g. que haya un código de estado válido asignado obligatoriamente).

3. **Pruebas Unitarias Robustas (`test_http_response.cpp`)**
   - 8 nuevos tests con GTest.
   - Prueban la preallocación, inyección de memoria con *chunks*, semántica de movimiento estricta, la validación del Builder y la extensibilidad de los metadatos.
   - Todos los tests pasaron el 100% bajo Clang y la batería de Sanitizers.

## 🚀 Impacto en el Proyecto

> [!TIP]
> **Eficiencia Desatada**
> Con este diseño, cuando comencemos a descargar segmentos de video de 10 MB, la red inyectará directamente sobre el buffer final y la estructura se pasará al hilo de decodificación en O(1) vía `std::move`. Cero copias de datos masivos.

> [!IMPORTANT]
> **Arquitectura Protegida**
> No hay ninguna cabecera externa contaminando este archivo. Cumple el mandato de arquitectura FSD: interfaces puras en la capa pública sin arrastrar pesadillas como `<curl/curl.h>`.

## ⏭️ Próximos Pasos

El issue **NET-01** está técnicamente resuelto. El siguiente paso lógico, basándonos en tu lista, sería abordar:

- **[NET-02] RetryPolicy y NetworkConfig**
- O hacer *commit* de estos cambios a tu rama actual.
