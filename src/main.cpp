#include <iostream>
#include <string>

#include "iptv/manifest/playlist_m3u8_parser.hpp"
#include "iptv/network/http_client.hpp"
#include "iptv/network/http_init.hpp"

using namespace iptv::network;
using namespace iptv::manifest;

int main(int argc, char* argv[]) {
  std::cout << "🎬 VSMN-Player CLI Validator\n";
  std::cout << "------------------------------\n";

  if (argc < 2) {
    std::cerr << "Uso: " << argv[0] << " <url_m3u8>\n";
    std::cerr << "Ejemplo: " << argv[0] << " https://test-streams.mux.dev/x36xhzz/x36xhzz.m3u8\n";
    return 1;
  }

  std::string url = argv[1];
  std::cout << "Descargando: " << url << "...\n";

  // Init libcurl and subsystems
  iptv::network::initialize();

  HttpClient client;
  // Bajar lista, max 10 segundos de timeout
  auto response = client.download(url, std::chrono::seconds(10));

  if (!response.isSuccess()) {
    std::cerr << "❌ Error HTTP: " << static_cast<int>(response.getStatusCode()) << "\n";
    iptv::network::shutdown();
    return 1;
  }

  const auto& body = response.getBody();
  std::string content(body.begin(), body.end());

  std::cout << "Descarga OK (" << content.size() << " bytes). Parseando...\n";

  // Parsear
  ParseResult result = M3u8Parser::parse({content});

  if (!result.hasValue()) {
    const auto& err = result.error();
    std::cerr << "❌ Error de parseo (Línea " << err.line_number << "): " << err.message << "\n";
  } else {
    const auto& playlist = result.value();
    std::cout << "✅ Playlist Válida\n";
    std::cout << "Tipo: "
              << (playlist.type == PlaylistType::Master ? "Master (Calidades/Canales)"
                                                        : "Media (Segmentos/Ts)")
              << "\n";
    std::cout << "Ítems: " << playlist.segments.size() << "\n";

    // Imprimir el primer item de ejemplo si hay
    if (!playlist.segments.empty()) {
      std::cout << "\nEjemplo ítem 1:\n";
      std::cout << "  URI: " << playlist.segments[0].uri << "\n";
      std::cout << "  Duración: " << playlist.segments[0].duration.count() << "s\n";
    }
  }

  iptv::network::shutdown();
  return 0;
}