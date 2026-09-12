# 🧪 Guía Completa de Testing para Colaboradores (VSMN-Player)

¡Bienvenido al equipo de VSMN-Player! 🎬
Si estás aquí, es porque quieres ayudarnos a mantener este reproductor de IPTV sólido como una roca. En este proyecto nos tomamos la calidad muy en serio: **nuestro código es C++20 industrial y está protegido por múltiples escudos automatizados.**

No te preocupes si nunca has escrito un test en C++ o si Google Test te suena a chino. Esta guía está diseñada para llevarte de la mano, paso a paso, desde cero hasta escribir tu primer test profesional.

---

## 1. Conceptos Básicos: ¿Qué es un Test Unitario?

Imagina que creas una función que suma dos números: `int sumar(int a, int b)`.
Un test unitario es simplemente un pequeño programa que ejecuta esa función con valores conocidos y comprueba que el resultado sea correcto.

```cpp
// Si ejecuto sumar(2, 3), ESPERO_QUE_SEA_IGUAL a 5.
EXPECT_EQ(sumar(2, 3), 5);
```

Si alguien en el futuro modifica tu función y por error rompe la lógica (haciendo que devuelva `6`), este test fallará y nos avisará antes de que el código llegue a los usuarios.

---

## 2. Nuestro Entorno: Google Test (GTest)

Usamos el framework **Google Test**, que es el estándar más utilizado en la industria del C++.
Todos nuestros tests viven en la carpeta `tests/unit/`. La estructura de esta carpeta refleja exactamente la estructura de nuestro código fuente (`src/`).

Por ejemplo, si quieres testear el archivo `src/manifest/playlist_m3u8_parser.cpp`, tu test debe ir en `tests/unit/manifest/test_playlist_m3u8_parser.cpp`.

---

## 3. Tutorial: Escribiendo tu Primer Test

Imaginemos que hemos añadido una función al `HttpClient` que limpia espacios en blanco de una URL.
Vamos a escribir un test para ella.

### Paso A: Crear el archivo

Crea (o abre) el archivo del test correspondiente, por ejemplo: `tests/unit/network/test_http_client.cpp`.

### Paso B: Incluir las cabeceras

Todo test necesita incluir la librería de GTest y el código que vas a probar:

```cpp
#include <gtest/gtest.h>
#include "iptv/network/http_client.hpp" // La clase que vamos a probar
```

### Paso C: Escribir el Macro del Test

Un test independiente se define usando la macro `TEST(NombreDelGrupo, NombreDelTest)`.

```cpp
TEST(HttpClientTest, LimpiaEspaciosEnBlanco) {
    // 1. Arrange (Preparar)
    iptv::network::HttpClient client;
    std::string url_sucia = "  http://example.com/stream.m3u8  ";
    
    // 2. Act (Actuar)
    std::string url_limpia = client.cleanUrl(url_sucia);
    
    // 3. Assert (Comprobar)
    EXPECT_EQ(url_limpia, "http://example.com/stream.m3u8");
}
```

### Paso D: Los Comandos Mágicos de GTest

En el paso 3 ("Assert"), usamos macros para comprobar los resultados:

- `EXPECT_EQ(a, b)`: Comprueba que `a` es igual a `b`. (Si falla, el test falla, pero **sigue** ejecutando la siguiente línea).
- `ASSERT_EQ(a, b)`: Igual que el anterior, pero si falla, **aborta** el test inmediatamente. (Úsalo para punteros nulos o tamaños de listas).
- `EXPECT_TRUE(condicion)`: Comprueba que algo es verdadero.
- `EXPECT_FALSE(condicion)`: Comprueba que algo es falso.

---

## 4. Tests Avanzados: Usando Fixtures (Test Fixtures)

A veces necesitas crear objetos complejos antes de cada test. Si tienes 10 tests que prueban el reproductor, no quieres inicializar el reproductor 10 veces en cada bloque. Para eso usamos **Fixtures**.

Un Fixture es una clase que hereda de `::testing::Test`.

```cpp
class PlaylistParserTest : public ::testing::Test {
 protected:
  // SetUp() se ejecuta ANTES de CADA test
  void SetUp() override {
    parser = std::make_unique<iptv::manifest::M3u8Parser>();
  }

  // TearDown() se ejecuta DESPUÉS de CADA test (opcional)
  void TearDown() override {
    parser.reset();
  }

  // Variables compartidas por todos los tests
  std::unique_ptr<iptv::manifest::M3u8Parser> parser;
};
```

Ahora, para usar este Fixture, usamos `TEST_F` (nota la 'F') en lugar de `TEST`:

```cpp
TEST_F(PlaylistParserTest, DevuelveErrorConArchivoVacio) {
    // Ya tenemos acceso a 'parser' porque lo inyectó el SetUp()
    auto result = parser->parse("");
    EXPECT_FALSE(result.hasValue());
}
```

---

## 5. Cómo Ejecutar los Tests

Una vez escrito el código, es hora de compilar y ver el resultado. VSMN-Player usa CMake.
Abre tu terminal (preferiblemente dentro del contenedor Docker de desarrollo) y ejecuta:

```bash
# Entrar a la carpeta de compilación de desarrollo (Debug)
cd build/Debug

# Compilar todo el proyecto y los tests
make -j4

# ¡Ejecutar los tests!
ctest --verbose --output-on-failure
```

Si ves texto verde diciendo `100% tests passed`, ¡Enhorabuena, tu código funciona y estás listo para hacer un Pull Request (PR)!

---

## 6. Los Escudos Ocultos: Sanitizers y CI/CD

No te asustes si tu test pasa en tu ordenador pero falla cuando subes el código a GitHub (GitHub Actions).

Nuestro pipeline ejecuta el código bajo **Sanitizers**:

- **ASan (AddressSanitizer):** Buscará si te has olvidado de liberar memoria (Memory Leaks) o si has accedido a un array fuera de sus límites.
- **UBSan (UndefinedBehaviorSanitizer):** Se quejará si haces divisiones por cero o matemáticas corruptas.
- **TSan (ThreadSanitizer):** Detectará si dos hilos están modificando la misma variable a la vez sin un mutex.

Si el pipeline de GitHub se pone rojo, revisa el *Log* de la acción. El Sanitizer te dirá exactamente en qué línea de tu test ocurrió el problema. ¡Arréglalo, súbelo de nuevo y siéntete orgulloso de estar escribiendo C++ de grado industrial!
