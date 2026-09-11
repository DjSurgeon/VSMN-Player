# 🌐 Arquitectura de Red: `HttpClient`

## 🎯 ¿Qué estamos construyendo y por qué?

Dentro del ecosistema del **VSMN-Player** (y la plataforma de datos *Prismatica*), la capa de red no es un simple canal para bajarse una web. Es el motor principal que alimenta los flujos de *streaming* de vídeo (como listas HLS o segmentos de medios) a la decodificación en tiempo real.

Queremos que este módulo represente el concepto de **Zero Overhead, Maximum Reliability** (Cero sobrecarga, Máxima Fiabilidad). Si la capa de red se bloquea, el *player* se congela. Si la capa de red fuga memoria, el dispositivo del usuario se queda sin RAM reproduciendo una película de 2 horas.

Por eso no hemos usado soluciones mágicas de alto nivel. Hemos bajado al barro con **`libcurl`** (el estándar de oro de la industria en C) y lo hemos recubierto de una armadura de **C++20 moderno**, creando un componente de grado militar.

---

## 🏗️ Patrones de Diseño Utilizados

### 1. El Idioma Pimpl (Pointer to Implementation)

- **Dónde:** En `HttpClient` (`http_client.cpp` / `http_client.hpp`).
- **Por qué:** Ocultamos toda la basura y los punteros crudos de `libcurl` (`CURL*`) dentro de un `struct Impl` privado en el archivo `.cpp`.
- **Beneficio:** Las cabeceras del sistema (como `<curl/curl.h>`) no contaminan el código de la aplicación. Cualquier cambio en la lógica interna no fuerza a recompilar a las decenas de clases que usen el `HttpClient`. Mantenemos una API pública prístina y puramente orientada a objetos.

### 2. Semántica de Movimiento (Move Semantics) y Zero-Copy

- **Dónde:** En `HttpResponse`.
- **Por qué:** Cuando nos bajamos un segmento de vídeo de 10 MB, no queremos hacer copias en RAM.
- **Beneficio:** La memoria alojada para la respuesta se transfiere (se "mueve") entre objetos en tiempo constante `O(1)`. Libcurl escribe directamente en el vector interno pre-reservado del `HttpResponse`, eliminando cuellos de botella de memoria y copias innecesarias.

### 3. Inyección de Dependencias y Mocking (Polimorfismo)

- **Dónde:** Interfaz `IHttpClient`.
- **Por qué:** Prismatica exige arquitecturas modulares. Cualquier componente que necesite descargar datos debe depender de `IHttpClient`, no de `HttpClient` directamente.
- **Beneficio:** En los tests unitarios de otras piezas del reproductor, podemos inyectar un *MockHttpClient* sin necesidad de levantar servidores ni usar red real, facilitando un TDD (Test Driven Development) rapidísimo.

---

## 🛡️ Mecanismos de Tolerancia a Fallos (Resilience)

Hemos programado el módulo para que sobreviva a las peores condiciones de la red móvil o del Wi-Fi de un usuario:

1. **Backoff Aritmético e Inteligente (Retry Policy):**
   No hacemos ataques DDoS a los servidores cuando fallan. El sistema implementa pausas escalonadas (`Fixed` o `Exponential`) antes de reintentar.

2. **Clasificación *Fail-Fast*:**
   Siguiendo el estándar de la industria (HLS Spec, DASH-IF), el cliente diferencia entre errores **Permanentes** (ej. un *404 Not Found*) y errores **Transitorios** (ej. *500 Internal Error*, o caídas de DNS). Si el error es permanente, corta la hemorragia de CPU y aborta al instante sin reintentar inútilmente.

3. **Guillotina Anti-Stall (Prevención de cuelgues):**
   Si una conexión se establece, pero el servidor empieza a enviar datos ridículamente lentos (ej. 1 byte por segundo), el reproductor normalmente se quedaría colgado infinitamente esperando. Hemos programado a libcurl para que mida la velocidad: si baja de **10 KB/s durante 3 segundos**, guillotina la conexión y lanza un fallo para que el sistema recupere el control y reintente.

---

## 🧪 La Red de Seguridad: Sanitizers y CI

No hemos dado por terminada la lógica hasta pasar por los tribunales más estrictos del ecosistema de C++:

- **ThreadSanitizer (TSan):** Certifica que puedes usar múltiples clientes a la vez desde distintos hilos sin destrozar la memoria global de cURL.
- **AddressSanitizer (ASan):** Garantiza matemáticamente que los punteros crudos se limpian solos al destruirse el objeto (RAII). 0 fugas de memoria.
- **GTest & cpp-httplib:** Hemos levantado *Mock Servers* embebidos en los binarios de testing para simular caídas maliciosas, tormentas de redirecciones (`301/302`) y tiempos de espera extremos.

## 🏁 Conclusión Inicial

El `HttpClient` no es solo un *wrapper* alrededor de una librería; es el cimiento de red del reproductor. Está diseñado para ser:

- **Silencioso:** Hace su trabajo sin quejarse ni bloquear el hilo principal.
- **Económico:** No desperdicia un solo byte de RAM ni un ciclo de reloj extra.
- **Implacable:** Sobrevive a redes hostiles garantizando que la aplicación matriz (el VSMN-Player) jamás sufra un *Crash* por culpa de internet.
