# 🐳 Docker en VSMN-Player

Nuestra infraestructura de Docker está diseñada con dos objetivos: **Desarrollo sin fricción** y **Despliegues ligeros**.

## 1. Entorno de Desarrollo (`Dockerfile.dev`)

Este contenedor es el motor que alimenta nuestro VSCode DevContainer y el entorno local.
Contiene todos los compiladores, dependencias (FFmpeg, SDL2, OpenGL) y herramientas de análisis estático (`clang-tidy`, `cppcheck`).

- **¿Cómo usarlo?**
  Abre VSCode, haz clic en "Reopen in Container". Listo.
- **¿Qué pasa internamente?**
  Usa `docker-compose.yml` (servicio `dev`), que mapea tus carpetas locales al contenedor y redirige la salida del Servidor X11 para que la GUI (ImGui) se dibuje en tu pantalla host.

## 2. Producción / Release (`Dockerfile.prod`)

Este archivo usa **Multi-stage builds** para crear una imagen final minúscula.

- **Fase 1 (Builder):** Arranca una máquina Ubuntu pesada, clona el código, baja dependencias con Conan y compila todo el proyecto con máxima optimización (`-O3 -march=native`).
- **Fase 2 (Runtime):** Arranca un Ubuntu limpio. Instala *únicamente* las librerías dinámicas necesarias (`libcurl`, `ffmpeg` runtimes, `sdl2`). Después, copia el ejecutable compilado `iptv_player` desde la Fase 1. El resto (código fuente, compiladores) se tira a la basura.

- **¿Cómo probarlo?**

```bash
docker build -f Dockerfile.prod -t iptv-player:prod .
docker run --rm iptv-player:prod
```

## 3. Optimización del Contexto (`.dockerignore`)

El archivo `.dockerignore` está configurado para que al hacer `docker build`, no subamos al Daemon los gigabytes de las carpetas `build/`, `docs/`, o `.git/`. Esto hace que los builds en CI/CD pasen de tardar 2 minutos a 10 segundos.
