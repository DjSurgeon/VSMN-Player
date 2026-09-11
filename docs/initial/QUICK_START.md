# 🚀 IPTV PLAYER — QUICK START GUIDE

## ¿Por dónde empiezo?

Tienes **5 documentos** en tu carpeta. Lee en este orden:

### 📍 PASO 1: Entiende el Big Picture (30 minutos)

```
EXECUTIVE_SUMMARY.md
└─ Lee primero
└─ Responde: ¿Qué es? ¿Por qué me va a ayudar? ¿Cuánto tiempo?
└─ Si en 30 min no lo entiendes → Lee ROADMAP_VISUAL.txt
```

**Preguntas que deberías poder responder:**

- ¿Cuántas semanas necesito?
- ¿Qué es la arquitectura multi-thread?
- ¿Por qué importa ThreadSanitizer?
- ¿Qué es un "portfolio project"?

---

### 📍 PASO 2: Aprende la Arquitectura (45 minutos)

```
ARCHITECTURE.md
└─ Diseño completo sin código
└─ CERO implementación, PURO pensamiento
└─ Secciones importantes:
    ├─ Diagrama de threads
    ├─ Componentes (Network, Decoder, Render, GUI)
    ├─ Circular buffer pattern
    ├─ Stack tecnológico justificado
    └─ Estructura de directorios esperada
```

**Después deberías poder:**

- Explicar qué hace Network thread vs Decoder thread
- Dibujar circular buffer en una servilleta
- Justificar por qué mutex > lock-free para MVP
- Listar las 3 capas (network, decode, render)

---

### 📍 PASO 3: Entiende las Decisiones (30 minutos)

```
TECHNICAL_DECISIONS.md
└─ Responde: "¿Por qué elegiste X vs Y?"
└─ 16 decisiones clave documentadas
└─ Cada una tiene: opciones, justificación, trade-offs
└─ Ejemplos de código (ilustrativos)
```

**Esto es lo que dirás en interviews:**

- "¿Por qué C++20 y no Rust?" → tienes respuesta
- "¿Por qué ImGui y no Qt?" → tienes respuesta
- "¿Por qué mutex y no lock-free?" → tienes respuesta

---

### 📍 PASO 4: Planifica Semana-a-Semana (1 hora)

```
EXECUTION_PLAN.md
└─ Checklist detallado semana-a-semana
└─ Cada semana: tareas concretas, hitos, métricas
└─ COPIA el Week 1 checklist → agenda en tu calendario
```

**Markers importantes:**

- Week 6: ✅ MVP LISTO (video plays, no crashes)
- Week 12: ✅ PRODUCCIÓN LISTA (portfolio-worthy)

---

### 📍 PASO 5: Visualiza el Plan (15 minutos)

```
ROADMAP_VISUAL.txt
└─ Timeline visual de 12 semanas
└─ 9 skill categories demostradas
└─ Success metrics
└─ "Golden rules" (no romper estas)
```

---

## ✅ CHECKLIST: ANTES DE ESCRIBIR 1 LÍNEA DE CÓDIGO

Valida que tienes:

### Ambiente Técnico

- [ ] **C++20 compiler** instalado

  ```bash
  g++ --version  # Necesitas GCC 11+ o Clang 13+
  ```

- [ ] **CMake 3.24+** instalado

  ```bash
  cmake --version
  ```

- [ ] **Conan 2.x** instalado

  ```bash
  conan --version
  ```

- [ ] **Docker** instalado (para week 10, pero mejor tener ahora)

  ```bash
  docker --version
  ```

- [ ] **Git** configurado

  ```bash
  git config --list
  ```

### Conocimiento

- [ ] Leído **EXECUTIVE_SUMMARY.md** (30 min)
- [ ] Leído **ARCHITECTURE.md** (45 min)
- [ ] Leído **TECHNICAL_DECISIONS.md** (30 min)
- [ ] Entiendes arquitectura 3-threads ✅
- [ ] Entiendes circular buffer pattern ✅
- [ ] Sabes qué es std::thread + mutex ✅

### Configuración GitHub

- [ ] Cuenta GitHub creada
- [ ] SSH keys configurado

  ```bash
  ssh -T git@github.com
  ```

- [ ] Nombre + email configurado

  ```bash
  git config --global user.name "Tu Nombre"
  git config --global user.email "tu@email.com"
  ```

### Workspace

- [ ] Carpeta para proyecto: `~/projects/iptv-player/`
- [ ] Editor/IDE listo (VSCode, CLion, etc)
- [ ] Calendario: **12 semanas reservadas** (10-15 h/semana)

---

## 🎯 ACCIÓN INMEDIATA (Hoy)

### En 30 minutos

1. **Crea GitHub repo**

   ```bash
   # En GitHub UI:
   # New repo → iptv-player
   # Public
   # README.md
   # .gitignore: C++
   # License: MIT
   ```

2. **Clona repo**

   ```bash
   git clone git@github.com:TU_USER/iptv-player.git
   cd iptv-player
   ```

3. **Copia documentación al repo**

   ```bash
   # Copia los 5 archivos .md a:
   # iptv-player/docs/ARCHITECTURE.md
   # iptv-player/docs/EXECUTIVE_SUMMARY.md
   # etc.
   
   mkdir docs
   cp ARCHITECTURE.md docs/
   cp EXECUTIVE_SUMMARY.md docs/
   cp TECHNICAL_DECISIONS.md docs/
   cp EXECUTION_PLAN.md docs/
   cp ROADMAP_VISUAL.txt docs/
   ```

4. **Primer commit**

   ```bash
   git add docs/
   git commit -m "Add architecture & planning documentation"
   git push origin main
   ```

5. **GitHub Projects**
   - Abre repo en GitHub
   - Click "Projects"
   - "New project" (Kanban template)
   - Columns: Backlog, In Progress, Done
   - Add issues from EXECUTION_PLAN.md

---

## 📊 ESTA SEMANA

### Plan

- [ ] **Hoy:** GitHub repo + documentación pushada
- [ ] **Mañana:** Leer ARCHITECTURE.md completamente
- [ ] **Miércoles:** Setup ambiente (CMake, Conan)
- [ ] **Jueves:** Crear proyecto C++20 boilerplate
- [ ] **Viernes:** Setup GitHub Actions CI/CD

### Resultado esperado

```
iptv-player/
├── .github/workflows/
│   └── ci-linux.yml         (skeleton)
├── docs/
│   ├── ARCHITECTURE.md
│   ├── EXECUTIVE_SUMMARY.md
│   ├── TECHNICAL_DECISIONS.md
│   ├── EXECUTION_PLAN.md
│   └── ROADMAP_VISUAL.txt
├── src/
│   └── main.cpp             (vacío, solo "hello world")
├── test/
│   └── CMakeLists.txt       (vacío)
├── CMakeLists.txt           (skeleton)
├── conanfile.txt            (skeleton)
├── .clang-format            (Google style)
├── .gitignore               (C++)
└── README.md                (link a docs/)
```

Primera línea de commit:

```bash
git commit -m "Initial project structure with CMake skeleton"
```

---

## ⏱️ PRÓXIMA SEMANA

Empieza **Week 1** de EXECUTION_PLAN.md:

```
WEEK 1: BOILERPLATE & NETWORK FOUNDATION

Lunes:
  - [ ] Estructura de directorios
  - [ ] CMakeLists.txt principal
  - [ ] Commit: "Initial project structure"

Martes-Miércoles:
  - [ ] conanfile.txt (dependencies)
  - [ ] CMakeLists.txt en src/, test/
  - [ ] Commit: "CMake + Conan setup"

Jueves:
  - [ ] .clang-format + .clang-tidy
  - [ ] GitHub Actions workflow (ci-linux.yml)
  - [ ] Commit: "GitHub Actions CI/CD setup"

Viernes:
  - [ ] README.md template
  - [ ] CONTRIBUTING.md
  - [ ] GitHub Projects (Kanban)
  - [ ] Commit: "Documentation skeleton"
```

---

## 🆘 SI ALGO NO ESTÁ CLARO

### Preguntas comunes

**P: ¿Realmente necesito 12 semanas?**
A: A 15 h/semana sí. Si trabajas 40 h/semana, 6-8 semanas es factible pero con riesgo de burnout. Mantén disciplina sobre velocidad.

**P: ¿Puedo empezar sin CMake/Conan?**
A: No. Esto es parte del proyecto. Pasa 1-2 días aprendiendo. Vale la pena.

**P: ¿Y si no tengo experiencia con FFmpeg?**
A: Perfecto. Week 3 es cuando lo aprendes. Empieza Week 1 (networking), eso es más simple.

**P: ¿Docker es obligatorio?**
A: Para MVP (week 6) no. Para producción (week 12) sí. Paciencia.

**P: ¿Qué pasa si me atrasó?**
A: Common. Ajusta timeline, pero NO hagas scope creep. Mejor 6 weeks + MVP que 12 weeks incomplete.

---

## 📚 RECURSOS EXTERNOS

### Si necesitas learn más

**CMake:**

- Oficiale: <https://cmake.org/cmake/help/latest/>
- Tutorial rápido: <https://www.youtube.com/watch?v=HPMvU64QPIA>

**Conan:**

- Docs: <https://docs.conan.io/>

**FFmpeg:**

- API docs: <https://ffmpeg.org/doxygen/>
- Tutorials: <https://github.com/leandromoreira/ffmpeg-libav-tutorial>

**Multithreading C++:**

- "C++ Concurrency in Action" (book)
- cppreference.com/std::thread

**GTest:**

- <https://github.com/google/googletest/blob/main/docs/primer.md>

**ImGui:**

- <https://github.com/ocornut/imgui> (+ examples/)

---

## 💡 MINDSET

### Recuerda

1. **Done is better than perfect**
   - Semana 6 MVP > Semana 20 perfecto

2. **MVP is MVP**
   - Video plays = éxito
   - 60 FPS smooth = fase 2

3. **Documenta decisiones**
   - No es overhead
   - Es diferenciador

4. **Confía en timeline**
   - No es optimista
   - Está basado en realidad

5. **Pide ayuda**
   - Stack Overflow es tu amigo
   - Recruiters aprecian decisiones, no que hayas hecho todo solo

---

## 🎬 TÚ ESTÁS LISTO?

### Responde sí a todo

- [ ] ¿Entiendo qué es este proyecto?
- [ ] ¿Tengo 12 semanas disponibles?
- [ ] ¿Puedo dedicar 10-15 horas/semana?
- [ ] ¿Estoy dispuesto a documentar decisiones?
- [ ] ¿Voy a respetar el timeline (no scope creep)?
- [ ] ¿Tengo ambiente técnico setup?

**Si todo es SÍ:**

## 🚀 EMPECEMOS

```bash
git clone git@github.com:TU_USER/iptv-player.git
cd iptv-player
# Start Week 1 from EXECUTION_PLAN.md
```

---

**Buena suerte. Te vas a sorprender de lo que puedes hacer en 12 semanas. 💪**

---

**Quick Start Version:** 1.0  
**Status:** Ready to execute ✅
