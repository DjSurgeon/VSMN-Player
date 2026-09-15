# Arquitectura de Resiliencia: Playback Orchestrator (ABR)

Para lograr que el reproductor VSMN-Player ofrezca estabilidad total en entornos de red hostiles (conexiones móviles, Wi-Fi inestable), la fase del Orquestador (Playback Orchestrator / StreamController) se diseñará sobre cuatro pilares fundamentales de resiliencia activa.

## Pilar 1: Gestión de Búfer Elástico (Dual-Watermark Buffering)

El objetivo es proteger la reproducción de microcortes en la red y fluctuaciones de latencia.

- **Low Watermark (ej. 3 segundos):** Nivel crítico. Si el búfer cae por debajo, el reproductor pausa la reproducción ("re-buffering") para acumular datos.
- **High Watermark (ej. 30s en VOD, 10s en Live):** Nivel de saciedad. El reproductor descarga por adelantado hasta este límite. Si se llena, el hilo de descarga se suspende para ahorrar batería y RAM.
- **Consideración (Trade-off):** En retransmisiones en vivo (Live), un High Watermark elevado incrementa el retraso respecto a la emisión real (latencia end-to-end), por lo que debe ajustarse dinámicamente según la playlist.

## Pilar 2: Estimación de Red con Filtros EWMA (Adaptive Bitrate)

La velocidad de descarga fluctúa constantemente. El orquestador necesita predecir el ancho de banda futuro de forma estable.

- **Filtro EWMA (Exponentially Weighted Moving Average):** Promedia la velocidad histórica ponderando fuertemente las descargas más recientes, mitigando el ruido de los picos puntuales de Wi-Fi.
- **Fast Downswitch (Modo Pánico):** Si la métrica de red actual cae drásticamente por debajo del bitrate necesario para el segmento en descarga, se aborta la transferencia actual (`std::stop_token` en `HttpClient`) y se pide inmediatamente un segmento de menor resolución (ej. 480p).
- **Consideración:** Depende de la disponibilidad de variantes (Master Playlist) con diferentes resoluciones y bitrates.

## Pilar 3: Pipelining y Pre-fetching Concurrente

Minimizar el impacto de la latencia (RTT) en el establecimiento de conexiones TCP/TLS (handshake).

- **Estrategia:** Mientras el segmento `N` está al 80% de su descarga, se abre asíncronamente el socket TCP para iniciar la petición del segmento `N+1`.
- **Reutilización:** Obligatorio el uso estricto de HTTP Keep-Alive en las sesiones de libcurl subyacentes.
- **Consideración:** En aplicaciones IPTV, el zapping (cambio rápido de canal) es crucial. El pre-fetching agresivo puede desperdiciar ancho de banda si el usuario cambia de canal a mitad, por lo que el cancelamiento debe ser instantáneo.

## Pilar 4: Degradación Elegante a "Audio-Only" (Fallback Extremo)

Supervivencia bajo condiciones de red extremas (ej. 2G / túneles).

- **Estrategia:** Cuando la red es insuficiente incluso para el nivel más bajo de vídeo (ej. 144p), el orquestador descarta los *frames* de vídeo y canaliza exclusivamente los de audio hacia el decodificador.
- **Experiencia de usuario:** Se prefiere imagen congelada con audio continuo antes que un "loading" silencioso.
- **Consideración:** El Demuxer (FFmpeg) deberá tolerar "agujeros" (gaps) masivos en los timestamps del stream de vídeo sin desincronizar la pista de audio principal, logrando reenganchar limpiamente cuando el vídeo vuelva a estar disponible.

## Integración C++20 Lograda (V1)

Para aislar esta complejidad y no contaminar la capa de red ni la del parser, hemos conseguido la siguiente arquitectura en nuestra versión actual (V1):

1. **Hilo de Ingesta (Productor):** El `PlaybackOrchestrator` arranca un hilo C++20 (`std::jthread`) autónomo.
2. **Evaluación ABR Pura:** El `AbrManager` inyectado aplica el EWMA ($\alpha = 0.3$) y una histéresis asimétrica matemáticamente estable sobre las métricas que nos provee el `HttpClient`.
3. **Comunicación Segura:** La ingesta inyecta los `MediaSegmentBundle` en una `ConcurrentQueue` basada en cerrojos (`std::mutex` y `std::condition_variable`), asegurando paso por movimiento (`std::move`) sin copias de memoria (Zero-Copy).
4. **Fast Cancellation (Abortos en Vuelo):** A través del `std::stop_token` nativo, si el usuario hace "Stop" o el orquestador requiere vaciar el *buffer* por un cambio abrupto de calidad, las conexiones `libcurl` se detienen internamente devolviendo `CURLE_ABORTED_BY_CALLBACK`.

El siguiente paso en la evolución será el consumidor final: conectar el Demuxer a la cola para iniciar el canal de decodificación.
