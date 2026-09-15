#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "iptv/abr/abr_manager.hpp"
#include "iptv/common/concurrent_queue.hpp"
#include "iptv/network/network_component.hpp"

namespace iptv::player {

/**
 * @brief Orquestador principal que une la capa de red con la cola de reproducción.
 */
class PlaybackOrchestrator {
 public:
  explicit PlaybackOrchestrator(std::unique_ptr<network::NetworkComponent> network_component);
  ~PlaybackOrchestrator();

  PlaybackOrchestrator(const PlaybackOrchestrator&) = delete;
  PlaybackOrchestrator& operator=(const PlaybackOrchestrator&) = delete;
  PlaybackOrchestrator(PlaybackOrchestrator&&) = delete;
  PlaybackOrchestrator& operator=(PlaybackOrchestrator&&) = delete;

  /**
   * @brief Inicia el hilo de red que descarga el manifiesto y los segmentos.
   * @param master_playlist_url URL del stream HLS a reproducir.
   */
  void start(const std::string& master_playlist_url);

  /**
   * @brief Detiene el hilo de red inyectando un stop_token y vacía la cola.
   */
  void stop();

  /**
   * @brief Acceso a la cola concurrente para el Demuxer/Decodificador.
   */
  iptv::ConcurrentQueue<network::MediaSegmentBundle>& getQueue() { return segment_queue_; }

 private:
  void downloadLoop(std::stop_token stop_token, const std::string& master_url);

  std::unique_ptr<network::NetworkComponent> network_;
  abr::AbrManager abr_manager_;
  iptv::ConcurrentQueue<network::MediaSegmentBundle> segment_queue_;
  std::jthread worker_thread_;
};

}  // namespace iptv::player
