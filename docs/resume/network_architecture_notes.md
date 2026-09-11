# 🏛️ Notas de Arquitectura: Módulo de Red (C++20)

Esta guía documenta todas las decisiones críticas de diseño y micro-optimizaciones implementadas en la capa de red (`HttpClient`) del VSMN-Player. Está diseñada para servir como hoja de ruta técnica y como base sólida para defender el código en entrevistas técnicas (Tech Interviews / Code Reviews).

---

## 1. Patrón Pimpl (Pointer to Implementation)

Hemos utilizado de manera exhaustiva el Idioma Pimpl (`class HttpClient::Impl`) para esconder los detalles internos de *libcurl*.

* **Problema Resuelto:** Evitar la "Infección de Dependencias" (*Dependency Leak*). Si incluimos `#include <curl/curl.h>` en nuestras cabeceras públicas, cualquier archivo del proyecto que use `HttpClient` arrastrará indirectamente la pesada librería *libcurl*, disparando los tiempos de compilación.
* **Beneficio Principal:** **Estabilidad de la Interfaz Binaria (ABI)**. El objeto `HttpClient` solo contiene un *Smart Pointer* (`std::unique_ptr<Impl>`). Esto garantiza que el tamaño de la clase en memoria no cambiará jamás, aunque añadamos mil variables privadas nuevas al `Impl`.
* **Concepto Clave (Cold Path):** Mantuvimos la inicialización global en `http_init.cpp` y no la hicimos `inline` en el *header* para evitar filtrar el `#include` de libcurl. Al ser una función que se llama una sola vez por ciclo de vida (*Cold Path*), no hay penalización real de rendimiento.

## 2. Refactorización SRP y Namespaces Anónimos

La función monolítica original `download()` fue despedazada aplicando el Principio de Responsabilidad Única (SRP).

* **Lógica *Stateless* (Sin Estado):** Movemos funciones matemáticas como el generador aleatorio de Jitter o la evaluación de errores a un `namespace { ... }` anónimo dentro del `.cpp`.
  * **Por qué:** El compilador inyecta (*inlinea*) el código de forma agresiva y le otorga *Internal Linkage*, haciendo imposible la colisión de nombres con otras partes del proyecto.
* **Orquestador Declarativo:** La función `download()` pública se redujo a ~20 líneas sin código sucio de punteros. Se lee de arriba abajo de forma semántica, delegando todo el trabajo pesado a los métodos encapsulados del `Impl`.

## 3. Arquitectura Zero-Copy y Rule of 5

En el transporte masivo de vídeo (HLS/MPEG-TS), la memoria RAM es el cuello de botella.

* **Regla de 5 (Move Semantics):** La clase `HttpResponse` tiene explícitamente borrados (`= delete`) sus constructores de copia. El compilador lanzará un error fatal si un desarrollador júnior intenta copiar por valor un segmento de vídeo de 10MB en la RAM. Obligamos al uso de `std::move`.
* **Inlining en Hot Paths:** Los métodos *Getters* y *Setters* cortos de `HttpResponse` (ej. `getBody()`, `getStatusCode()`) se implementan directamente en la cabecera `.hpp`.
  * **Por qué:** Son funciones implícitamente `inline`. Al llamarse miles de veces por segundo (*Hot Path*), nos ahorramos la penalización en nanosegundos de saltar en memoria al `.cpp` (Zero overhead).

## 4. Parser Zero-Allocation y SIMD (`headerCallback`)

Para evitar la fragmentación del *Heap* durante las descargas, evitamos que el `std::vector` crezca a ciegas (`.push_back`).

* **Extracción de Cabeceras:** Interceptamos el `Content-Length` en pleno vuelo usando `<charconv>` (`std::from_chars`) sobre un `std::string_view`. Es el parseo numérico más rápido de C++20 porque no requiere instanciar ni un solo `std::string`.
* **Pre-reserva Exacta:** Sabiendo el tamaño exacto, invocamos `.reserve()` antes de recibir el cuerpo del archivo, garantizando **cero realocaciones** dinámicas.
* **Copia de Bloques:** En vez de bucles manuales, inyectamos los paquetes de red al vector con un `resize()` y `std::memcpy()` marcado con `noexcept`, permitiendo que el compilador use instrucciones vectorizadas (SIMD/AVX) para exprimir el ancho de banda.

## 5. Resiliencia: Full Jitter y Control de Tormentas

Implementamos un sistema de reintentos industrial.

* **Fallo Rápido (Fail-Fast):** Clasificamos rigurosamente los errores. Si el servidor devuelve `404 Not Found`, se aborta inmediatamente. Solo hacemos reintentos en fallos transitorios (`5xx`, timeouts).
* **Backoff Exponencial con *Full Jitter*:** Para evitar ataques DDoS involuntarios (*Thundering Herd Problem*) cuando se cae un servidor y miles de clientes intentan reconectar a la vez. Usamos `thread_local std::mt19937` como un PRNG ultrarrápido sin bloqueos de hilos (*lock-free*) para añadir "ruido blanco" a los tiempos de espera.
