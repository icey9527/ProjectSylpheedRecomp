#pragma once

#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>
#include <atomic>
#include <memory>

namespace sylpheed {

// Observes keyboard delivery without swallowing keys or injecting input.
class InputDiagnostics : public rex::ui::WindowInputListener {
 public:
  explicit InputDiagnostics(rex::input::InputSystem* input) : input_(input) {}
  ~InputDiagnostics() override { alive_->store(false); }
  void OnKeyDown(rex::ui::KeyEvent& event) override {
    if (remaining_ == 0) return;
    --remaining_;
    const auto vk = static_cast<unsigned>(event.virtual_key());
    REXLOG_INFO("SYLPHEED_INPUT key_down vk={} mnk_mode={}", vk,
                rex::cvar::GetFlagByName("mnk_mode"));
    // Read after the same event has reached the SDK keyboard driver.
    event.target()->app_context().CallInUIThreadDeferred([alive = alive_, input = input_, vk] {
      if (!alive->load()) return;
      for (unsigned user = 0; user < 4; ++user) {
        rex::input::X_INPUT_STATE state{};
        const auto result = input->GetState(user, &state);
        REXLOG_INFO("SYLPHEED_INPUT sample vk={} user={} result={} buttons={} lx={} ly={}",
                    vk, user, result, static_cast<unsigned>(state.gamepad.buttons),
                    static_cast<int>(state.gamepad.thumb_lx),
                    static_cast<int>(state.gamepad.thumb_ly));
      }
    });
  }
 private:
  rex::input::InputSystem* input_;
  unsigned remaining_ = 20;
  std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

}  // namespace sylpheed
