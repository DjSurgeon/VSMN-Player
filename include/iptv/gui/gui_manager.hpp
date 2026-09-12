#pragma once

#include <memory>

#include "iptv/controller/stream_controller.hpp"

namespace iptv::gui {

/**
 * @brief Subsystem responsible for the graphical user interface.
 *
 * Encapsulates the UI layer (e.g. ImGui) to decouple UI logic from playback logic.
 */
class IGuiManager {
 public:
  virtual ~IGuiManager() = default;

  /**
   * @brief Initializes the UI context and binds it to the render window.
   */
  virtual bool initialize() = 0;

  /**
   * @brief Renders a single UI frame.
   *
   * @param controller Reference to the orchestrator to query state and send commands
   *                   (like play/pause).
   */
  virtual void render(controller::StreamController& controller) = 0;
};

/**
 * @brief Factory for creating the default ImGui-based GUI Manager.
 */
std::shared_ptr<IGuiManager> createDefaultGuiManager();

}  // namespace iptv::gui
