# 🧹 YAGNI Cleanup Record (Fase 1)

Este documento registra el andamiaje especulativo que fue diseñado y posteriormente eliminado durante la auditoría pre-Fase 2 (Concurrencia).
Si en iteraciones posteriores es necesario recuperar la estructura original de los subsistemas, se puede consultar este documento o el historial de Git (commit previo a la limpieza).

## ¿Por qué se eliminaron?

El proyecto presentaba un diseño *Over-engineered* (Fiebre de las Interfaces). Subsistemas enteros fueron declarados sin implementaciones `.cpp`, sin tests y sin llamadas activas, añadiendo complejidad de mantenimiento y peso al pipeline.
Siguiendo los principios **YAGNI (You Aren't Gonna Need It)** y **Data-Oriented Design**, se purgó todo el código muerto para mantener el MVP estrictamente funcional.

## Archivos Eliminados

### 1. Subsistemas e Interfaces Fantasma

- `include/iptv/controller/stream_controller.hpp`: Orquestador principal (usaba `std::shared_ptr` como inyección, lo cual fue considerado un *bad smell* de concurrencia).
- `include/iptv/decoder/decoder_subsystem.hpp`: Interfaz `IDecoderSubsystem`.
- `include/iptv/render/render_subsystem.hpp`: Interfaz `IRenderSubsystem`.
- `include/iptv/network/network_subsystem.hpp`: Interfaz `INetworkSubsystem`.
- `include/iptv/gui/gui_manager.hpp`: Interfaz `IGuiManager`.

### 2. Tipos y Logging No Utilizados

- `include/iptv/common/types.hpp`: 12 tipos base (`PlaybackState`, `Packet`, `VideoFrame`, `AudioFrame`, etc.) que no se utilizaban en la base de código actual.
- `include/iptv/common/logging.hpp`: Wrappers de `spdlog` que nunca fueron invocados.

### 3. Placeholders y Scaffolding

- `src/pipeline/dummy.cpp`: Placeholder vacío.
- Directorios vacíos en `src/` y `include/iptv/`.

## Lecciones Arquitectónicas para la Fase 2 / 3

- **Object Pools:** Cuando se re-implementen los subsistemas de Red y Decodificador, NO pasar búferes de 5MB asignados dinámicamente. Implementar un anillo de memoria o *Object Pool* para evitar la fragmentación severa del Heap.
- **Dependency Injection:** El nuevo `StreamController` debe recibir dependencias estrictas (`std::unique_ptr` o referencias `&`), evitando `std::shared_ptr` para componentes que tienen un único ciclo de vida compartido con la App.
- **Zero-Cost Abstractions:** Evitar interfaces `I-` para clases de las que se sabe que solo habrá una implementación (ej: el decodificador FFmpeg). Utilizar herencia solo cuando el polimorfismo sea inevitable en tiempo de ejecución.
