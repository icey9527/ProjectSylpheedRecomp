#pragma once

#include <rex/input/input_system.h>
#include <rex/ui/window_listener.h>

#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>

namespace sylpheed::input {
using rex::X_RESULT;
using rex::X_STATUS;

// Supplies the VK_PAD event stream missing from the v0.10 MnK driver.
// The SDK still owns controller state, capabilities and physical devices.
class KeyboardKeystrokeDriver final : public rex::input::InputDriver,
                                     public rex::ui::WindowInputListener,
                                     public rex::ui::WindowListener {
 public:
  using Clock = std::chrono::steady_clock;
  using Now = std::function<Clock::time_point()>;
  explicit KeyboardKeystrokeDriver(Now now = Clock::now);
  ~KeyboardKeystrokeDriver() override;

  X_STATUS Setup() override;
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override;
  X_RESULT GetDeviceState(rex::input::DeviceId, rex::input::X_INPUT_STATE*) override;
  X_RESULT GetDeviceCapabilities(rex::input::DeviceId, uint32_t,
                                rex::input::X_INPUT_CAPABILITIES*) override;
  X_RESULT SetDeviceVibration(rex::input::DeviceId, rex::input::X_INPUT_VIBRATION*) override;
  X_RESULT GetDeviceKeystroke(rex::input::DeviceId id, uint32_t flags,
                             rex::input::X_INPUT_KEYSTROKE* out) override;

  void OnWindowAvailable(rex::ui::Window* window) override;
  void OnClosing(rex::ui::UIEvent&) override;
  void OnLostFocus(rex::ui::UISetupEvent&) override;
  void OnGotFocus(rex::ui::UISetupEvent&) override;
  void OnKeyDown(rex::ui::KeyEvent& event) override;
  void OnKeyUp(rex::ui::KeyEvent& event) override;
  void OnMouseDown(rex::ui::MouseEvent& event) override;
  void OnMouseUp(rex::ui::MouseEvent& event) override;

 private:
  static bool Enabled();
  bool BindPressed(const char* name) const;
  uint64_t PadKeys() const;
  void ChangeKey(uint16_t key, bool down);
  void UpdatePadKeys(uint64_t mask);
  void ReleaseKeys();
  void DetachWindow();

  std::mutex mutex_;
  std::array<bool, 256> keys_{};
  std::deque<rex::input::X_INPUT_KEYSTROKE> events_;
  uint64_t held_ = 0;
  int repeat_key_ = -1;
  Clock::time_point repeat_at_{};
  Now now_;
  bool focused_ = true;
  unsigned logged_ = 0;
  rex::ui::Window* attached_ = nullptr;
};

std::unique_ptr<rex::input::InputSystem> CreateInputSystem(bool tool_mode);

}  // namespace sylpheed::input
