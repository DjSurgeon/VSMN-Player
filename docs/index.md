# VSMN-Player: High-Performance C++20 IPTV Engine

Welcome to the technical documentation of **VSMN-Player**, a next-generation IPTV streaming engine built entirely in Modern C++20.

## 🚀 Mission Statement

Our goal is to build an industrial-grade, zero-copy video streaming pipeline capable of handling massive HLS/MPEG-TS manifests with zero dropped frames. This engine is built from the ground up for maximum performance, deterministic memory management, and rigorous thread safety.

## 🔑 Key Architectural Pillars

* **Zero-Copy Native:** Absolute elimination of unnecessary memory allocations. Data flows from the network socket to the decoder without heap fragmentation.
* **Resilient Networking:** Industrial backoff strategies (Full Jitter), anti-stall mechanics, and smart chunking.
* **Thread-Safety Guaranteed:** Engineered for multi-threading and validated continuously by ThreadSanitizer (TSan).
* **Modern C++20 Standard:** Leveraging concepts, ranges, `<charconv>`, and strict move-only semantics to guarantee memory safety at compile time.

## 🏗️ Architecture Overview

```mermaid
graph TD
    subgraph Network Layer
        HC[HttpClient] --> |Zero-Copy Payload| HR[HttpResponse]
    end
    
    subgraph Pipeline
        HR --> |M3U8 Data| HLS[HLS Parser]
        HLS --> |Parsed Playlist| ORC[Playback Orchestrator]
        ORC --> |MPEG-TS Segments| CQ[ConcurrentQueue]
        CQ --> |Pop| DEC[Demuxer / Decoder]
    end
    
    subgraph Core Features
        Jitter[Jitter & Backoff] -.-> HC
        ABR[ABR Manager] -.-> ORC
    end

    style HC fill:#3f51b5,stroke:#fff,stroke-width:2px,color:#fff
    style HR fill:#3f51b5,stroke:#fff,stroke-width:2px,color:#fff
    style HLS fill:#009688,stroke:#fff,stroke-width:2px,color:#fff
    style ORC fill:#ff9800,stroke:#fff,stroke-width:2px,color:#fff
    style CQ fill:#9c27b0,stroke:#fff,stroke-width:2px,color:#fff
    style DEC fill:#e91e63,stroke:#fff,stroke-width:2px,color:#fff
```

Explore the documentation through the navigation menu to deep dive into our network layer, CI/CD pipelines, and API references.
