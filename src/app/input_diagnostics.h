#pragma once

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/window_listener.h>

namespace sylpheed {

// Observes keyboard delivery without swallowing keys or injecting input.
class InputDiagnostics : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& event) override {
    if (remaining_ == 0) return;
    --remaining_;
    const auto vk = static_cast<unsigned>(event.virtual_key());
    REXLOG_INFO("SYLPHEED_INPUT key_down vk={} mnk_mode={}", vk,
                rex::cvar::GetFlagByName("mnk_mode"));
    // GetState/GetKeystroke rebuild SDK device containers without synchronization.
    // Never query them here on the UI thread alongside guest input polling.
  }
 private:
  unsigned remaining_ = 20;
};

}  // namespace sylpheed
