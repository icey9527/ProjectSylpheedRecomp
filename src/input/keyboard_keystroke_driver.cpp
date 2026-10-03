#include "keyboard_keystroke_driver.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <string_view>
#include <algorithm>
#include <limits>

// Mouse motion to left-stick conversion scale. Runtime-adjustable from the
// Tools menu slider (mouse_settings.cpp); clamped when read.
REXCVAR_DEFINE_INT32(mouse_look_scale, 8192, "Input",
                     "Mouse motion to left-stick scale; higher is more sensitive");

namespace sylpheed::input {
namespace {
using VK = rex::ui::VirtualKey;
using namespace rex::input;
constexpr DeviceId kDevice = static_cast<DeviceId>(0x53594B45);  // Separate from SDK MnK.
constexpr std::chrono::milliseconds kRepeatDelay{400}, kRepeatInterval{100};
struct Binding { const char* name; VK pad; };
constexpr Binding kButtons[] = {
    {"keybind_a", VK::kXInputPadA}, {"keybind_b", VK::kXInputPadB},
    {"keybind_x", VK::kXInputPadX}, {"keybind_y", VK::kXInputPadY},
    {"keybind_right_shoulder", VK::kXInputPadRShoulder},
    {"keybind_left_shoulder", VK::kXInputPadLShoulder},
    {"keybind_left_trigger", VK::kXInputPadLTrigger},
    {"keybind_right_trigger", VK::kXInputPadRTrigger},
    {"keybind_dpad_up", VK::kXInputPadDpadUp},
    {"keybind_dpad_down", VK::kXInputPadDpadDown},
    {"keybind_dpad_left", VK::kXInputPadDpadLeft},
    {"keybind_dpad_right", VK::kXInputPadDpadRight},
    {"keybind_start", VK::kXInputPadStart}, {"keybind_back", VK::kXInputPadBack},
    {"keybind_lstick_press", VK::kXInputPadLThumbPress},
    {"keybind_rstick_press", VK::kXInputPadRThumbPress},
};
constexpr VK kStickKeys[] = {
    VK::kXInputPadLThumbUp, VK::kXInputPadLThumbDown,
    VK::kXInputPadLThumbLeft, VK::kXInputPadLThumbRight,
    VK::kXInputPadLThumbUpLeft, VK::kXInputPadLThumbUpRight,
    VK::kXInputPadLThumbDownLeft, VK::kXInputPadLThumbDownRight,
    VK::kXInputPadRThumbUp, VK::kXInputPadRThumbDown,
    VK::kXInputPadRThumbLeft, VK::kXInputPadRThumbRight,
    VK::kXInputPadRThumbUpLeft, VK::kXInputPadRThumbUpRight,
    VK::kXInputPadRThumbDownLeft, VK::kXInputPadRThumbDownRight,
};
constexpr size_t kButtonCount = std::size(kButtons);
constexpr size_t kKeyCount = kButtonCount + std::size(kStickKeys);
VK PadKey(size_t index) {
  return index < kButtonCount ? kButtons[index].pad : kStickKeys[index - kButtonCount];
}
// True when the pointer is over a host ImGui window (sensitivity slider, frame
// rates, F3 overlay). Without an ImGui context (unit tests) nothing captures.
bool CapturedByHostUi() {
  return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}
X_INPUT_KEYSTROKE Stroke(size_t index, uint16_t flags) {
  X_INPUT_KEYSTROKE result{};
  result.virtual_key = static_cast<uint16_t>(PadKey(index));
  result.flags = flags;
  return result;  // InputSystem assigns the guest user index.
}
std::string_view Trim(std::string_view text) {
  while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
  return text;
}
uint16_t MouseKey(rex::ui::MouseEvent::Button button) {
  using Button = rex::ui::MouseEvent::Button;
  if (button == Button::kLeft) return static_cast<uint16_t>(VK::kLButton);
  if (button == Button::kRight) return static_cast<uint16_t>(VK::kRButton);
  if (button == Button::kMiddle) return static_cast<uint16_t>(VK::kMButton);
  return 0;
}
}  // namespace

KeyboardKeystrokeDriver::KeyboardKeystrokeDriver(Now now)
    : InputDriver(nullptr, 0), now_(std::move(now)) {}
KeyboardKeystrokeDriver::~KeyboardKeystrokeDriver() {
  // Window callbacks and runtime teardown run on the UI thread, where draining
  // pending callbacks and releasing the capture is safe. From any other thread
  // the SDK may refuse to queue the release, or accept it and never execute it
  // once the loop has exited, which would block a synchronous wait forever.
  // Drop local state there instead; the platform releases the capture together
  // with the window.
  rex::ui::Window* window = nullptr;
  {
    std::lock_guard lock(mutex_);
    window = attached_;
  }
  if (window && !window->app_context().IsInUIThread()) {
    mouse_capture_requested_.store(false, std::memory_order_release);
    mouse_capture_queued_.store(false, std::memory_order_release);
    mouse_capture_active_.store(false, std::memory_order_release);
    mouse_dx_.store(0, std::memory_order_release);
    mouse_dy_.store(0, std::memory_order_release);
    {
      std::lock_guard lock(mutex_);
      attached_ = nullptr;
    }
    REXLOG_WARN("SYLPHEED_INPUT window detached off the UI thread; capture release skipped");
    return;
  }
  DetachWindow();
}
X_STATUS KeyboardKeystrokeDriver::Setup() { return X_STATUS_SUCCESS; }
bool KeyboardKeystrokeDriver::Enabled() {
  return rex::cvar::GetFlagByName("mnk_mode") == "true";
}
void KeyboardKeystrokeDriver::EnumerateDevices(std::vector<DeviceInfo>& out) {
  if (!Enabled()) {
    std::lock_guard lock(mutex_);
    keys_.fill(false);
    events_.clear();
    held_ = 0;
    repeat_key_ = -1;
    return;
  }
  DeviceInfo info;
  info.id = kDevice;
  info.name = "Keyboard controller events";
  info.synthetic = true;
  out.push_back(info);
}
  // Contribute a synthetic state for mouse control, while leaving capabilities and
// vibration ownership to the physical controller/SDK driver.
X_RESULT KeyboardKeystrokeDriver::GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) {
  if (id != kDevice || !Enabled()) return X_ERROR_DEVICE_NOT_CONNECTED;
  if (!out_state) return X_ERROR_BAD_ARGUMENTS;
  if (!focused_.load(std::memory_order_acquire) || !is_active()) {
    mouse_dx_.store(0, std::memory_order_release);
    mouse_dy_.store(0, std::memory_order_release);
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  const auto clamp_axis = [](int32_t value) {
    return static_cast<int16_t>(std::clamp(value,
        int32_t(std::numeric_limits<int16_t>::min()),
        int32_t(std::numeric_limits<int16_t>::max())));
  };
  // SDL reports physical-pixel deltas. A host scale gives ordinary mouse
  // movement useful camera range without enabling the SDK's hidden
  // relative-mouse mode. One poll consumes the accumulated motion so a
  // stopped mouse does not keep moving the game. Four times the initial
  // baseline by default; the Tools menu slider adjusts it live.
  constexpr int32_t kMinMouseScale = 1024, kMaxMouseScale = 65536;
  const int32_t mouse_scale =
      std::clamp(REXCVAR_GET(mouse_look_scale), kMinMouseScale, kMaxMouseScale);
  const int32_t dx = mouse_dx_.exchange(0, std::memory_order_acq_rel);
  const int32_t dy = mouse_dy_.exchange(0, std::memory_order_acq_rel);
  *out_state = {};
  out_state->packet_number = ++packet_number_;
  out_state->gamepad.thumb_lx = clamp_axis(dx * mouse_scale);
  out_state->gamepad.thumb_ly = clamp_axis(-dy * mouse_scale);
  return X_ERROR_SUCCESS;
}
X_RESULT KeyboardKeystrokeDriver::GetDeviceCapabilities(DeviceId, uint32_t,
                                                        X_INPUT_CAPABILITIES*) {
  return X_ERROR_DEVICE_NOT_CONNECTED;
}
X_RESULT KeyboardKeystrokeDriver::SetDeviceVibration(DeviceId, X_INPUT_VIBRATION*) {
  return X_ERROR_DEVICE_NOT_CONNECTED;
}

bool KeyboardKeystrokeDriver::BindPressed(const char* name) const {
  // Match v0.10 MnK's comma alternatives and exact Shift/Ctrl/Alt modifiers.
  const unsigned live = (keys_[static_cast<unsigned>(VK::kShift)] ? 1 : 0) |
                        (keys_[static_cast<unsigned>(VK::kControl)] ? 2 : 0) |
                        (keys_[static_cast<unsigned>(VK::kMenu)] ? 4 : 0);
  const auto binding = rex::cvar::GetFlagByName(name);
  std::string_view rest(binding);
  while (!rest.empty()) {
    const auto comma = rest.find(',');
    auto token = Trim(rest.substr(0, comma));
    rest = comma == rest.npos ? std::string_view{} : rest.substr(comma + 1);
    unsigned wanted = 0;
    for (;;) {
      const auto plus = token.find('+');
      if (plus == token.npos || plus == 0) break;
      const auto head = token.substr(0, plus);
      if (head == "Shift") wanted |= 1;
      else if (head == "Ctrl" || head == "Control") wanted |= 2;
      else if (head == "Alt") wanted |= 4;
      else break;
      token.remove_prefix(plus + 1);
    }
    const auto key = static_cast<unsigned>(rex::ui::ParseVirtualKey(token));
    if (live == wanted && key != 0 && key < keys_.size() && keys_[key]) return true;
  }
  return false;
}
uint64_t KeyboardKeystrokeDriver::PadKeys() const {
  uint64_t mask = 0;
  for (size_t i = 0; i < kButtonCount; ++i) {
    if (BindPressed(kButtons[i].name)) mask |= uint64_t{1} << i;
  }
  const char* sticks[][4] = {
      {"keybind_lstick_up", "keybind_lstick_down", "keybind_lstick_left", "keybind_lstick_right"},
      {"keybind_rstick_up", "keybind_rstick_down", "keybind_rstick_left", "keybind_rstick_right"},
  };
  for (size_t stick = 0; stick < 2; ++stick) {
    const auto& names = sticks[stick];
    const int y = int(BindPressed(names[0])) - int(BindPressed(names[1]));
    const int x = int(BindPressed(names[3])) - int(BindPressed(names[2]));
    int direction = -1;
    if (y > 0) direction = x < 0 ? 4 : x > 0 ? 5 : 0;
    else if (y < 0) direction = x < 0 ? 6 : x > 0 ? 7 : 1;
    else if (x) direction = x < 0 ? 2 : 3;
    if (direction >= 0) mask |= uint64_t{1} << (kButtonCount + stick * 8 + direction);
  }
  return mask;
}
void KeyboardKeystrokeDriver::UpdatePadKeys(uint64_t mask) {
  const auto changed = held_ ^ mask;
  // Release the old direction before pressing a new diagonal, as XInput does.
  for (size_t i = 0; i < kKeyCount; ++i) {
    if ((changed & (uint64_t{1} << i)) && !(mask & (uint64_t{1} << i))) {
      events_.push_back(Stroke(i, X_INPUT_KEYSTROKE_KEYUP));
      if (repeat_key_ == int(i)) repeat_key_ = -1;
    }
  }
  for (size_t i = 0; i < kKeyCount; ++i) {
    if ((changed & (uint64_t{1} << i)) && (mask & (uint64_t{1} << i))) {
      events_.push_back(Stroke(i, X_INPUT_KEYSTROKE_KEYDOWN));
      repeat_key_ = int(i);
      repeat_at_ = now_() + kRepeatDelay;
    }
  }
  held_ = mask;
}
void KeyboardKeystrokeDriver::ReleaseKeys() {
  keys_.fill(false);
  // Drop pending actions, but retain releases across repeated focus-loss events.
  std::erase_if(events_, [](const auto& event) {
    return (unsigned(event.flags) & X_INPUT_KEYSTROKE_KEYUP) == 0;
  });
  UpdatePadKeys(0);
}
void KeyboardKeystrokeDriver::ChangeKey(uint16_t key, bool down) {
  if (key == 0 || key >= keys_.size() || !Enabled()) return;
  std::lock_guard lock(mutex_);
  if (down && (!focused_.load(std::memory_order_acquire) || !is_active())) return;
  keys_[key] = down;
  UpdatePadKeys(PadKeys());
}
X_RESULT KeyboardKeystrokeDriver::GetDeviceKeystroke(DeviceId id, uint32_t,
                                                    X_INPUT_KEYSTROKE* out) {
  if (id != kDevice || !Enabled()) return X_ERROR_DEVICE_NOT_CONNECTED;
  if (!out) return X_ERROR_BAD_ARGUMENTS;
  std::lock_guard lock(mutex_);
  if (held_ && (!focused_.load(std::memory_order_acquire) || !is_active())) ReleaseKeys();
  if (!events_.empty()) {
    *out = events_.front();
    events_.pop_front();
  } else if (repeat_key_ >= 0 && now_() >= repeat_at_) {
    *out = Stroke(repeat_key_, X_INPUT_KEYSTROKE_KEYDOWN | X_INPUT_KEYSTROKE_REPEAT);
    repeat_at_ = now_() + kRepeatInterval;
  } else {
    return X_ERROR_EMPTY;
  }
  if (logged_++ < 16) {
    REXLOG_INFO("SYLPHEED_KEYSTROKE vk={} flags={}", unsigned(out->virtual_key), unsigned(out->flags));
  }
  return X_ERROR_SUCCESS;
}
void KeyboardKeystrokeDriver::OnKeyDown(rex::ui::KeyEvent& event) {
  if (event.virtual_key() == VK::kEscape) {
    QueueMouseCapture(false);
    return;
  }
  if (!event.is_handled()) ChangeKey(static_cast<uint16_t>(event.virtual_key()), true);
}
void KeyboardKeystrokeDriver::OnKeyUp(rex::ui::KeyEvent& event) {
  ChangeKey(static_cast<uint16_t>(event.virtual_key()), false);
}
void KeyboardKeystrokeDriver::OnMouseDown(rex::ui::MouseEvent& event) {
  // ImGui windows (sensitivity slider, frame rates, F3 overlay) keep the
  // system cursor usable. The SDK does not mark their mouse events handled,
  // so check ImGui's capture flag here: interacting with a host dialog must
  // not hide the cursor through the mouse-look capture or steer the ship.
  if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) return;
  ChangeKey(MouseKey(event.button()), true);
  if (event.button() == rex::ui::MouseEvent::Button::kLeft ||
      event.button() == rex::ui::MouseEvent::Button::kRight ||
      event.button() == rex::ui::MouseEvent::Button::kMiddle) {
    QueueMouseCapture(true);
  }
}
void KeyboardKeystrokeDriver::OnMouseMove(rex::ui::MouseEvent& event) {
  if (!Enabled() || CapturedByHostUi() ||
      !focused_.load(std::memory_order_acquire) || !is_active() ||
      (!mouse_capture_active_.load(std::memory_order_acquire) &&
       !mouse_capture_requested_.load(std::memory_order_acquire))) return;
  const auto add_saturated = [](std::atomic<int32_t>& target, float delta) {
    const int32_t rounded = static_cast<int32_t>(delta);
    if (!rounded) return;
    int32_t current = target.load(std::memory_order_relaxed);
    for (;;) {
      const int64_t next64 = int64_t(current) + rounded;
      const int32_t next = static_cast<int32_t>(std::clamp(next64,
          int64_t(std::numeric_limits<int32_t>::min()),
          int64_t(std::numeric_limits<int32_t>::max())));
      if (target.compare_exchange_weak(current, next, std::memory_order_release,
                                       std::memory_order_relaxed)) return;
    }
  };
  add_saturated(mouse_dx_, event.dx());
  add_saturated(mouse_dy_, event.dy());
}
void KeyboardKeystrokeDriver::OnMouseUp(rex::ui::MouseEvent& event) {
  ChangeKey(MouseKey(event.button()), false);
}
void KeyboardKeystrokeDriver::OnLostFocus(rex::ui::UISetupEvent&) {
  {
    std::lock_guard lock(mutex_);
    focused_.store(false, std::memory_order_release);
    ReleaseKeys();
    mouse_dx_.store(0, std::memory_order_release);
    mouse_dy_.store(0, std::memory_order_release);
  }
  QueueMouseCapture(false);
}
void KeyboardKeystrokeDriver::OnGotFocus(rex::ui::UISetupEvent&) {
  std::lock_guard lock(mutex_);
  focused_.store(true, std::memory_order_release);
}
void KeyboardKeystrokeDriver::OnWindowAvailable(rex::ui::Window* window) {
  DetachWindow();
  {
    std::lock_guard lock(mutex_);
    attached_ = window;
  }
  if (window) {
    window->AddInputListener(this, window_z_order());
    window->AddListener(this);
  }
}
void KeyboardKeystrokeDriver::QueueMouseCapture(bool capture) {
  mouse_capture_requested_.store(capture, std::memory_order_release);
  rex::ui::Window* window = nullptr;
  {
    std::lock_guard lock(mutex_);
    window = attached_;
  }
  if (!window) return;
  bool expected = false;
  if (!mouse_capture_queued_.compare_exchange_strong(expected, true,
                                                     std::memory_order_acq_rel)) return;
  window->app_context().CallInUIThreadDeferred([this, window] {
    mouse_capture_queued_.store(false, std::memory_order_release);
    ApplyMouseCaptureOnUIThread(window);
  });
}
void KeyboardKeystrokeDriver::ApplyMouseCaptureOnUIThread(rex::ui::Window* window) {
  if (!window) return;
  const bool should_capture = mouse_capture_requested_.load(std::memory_order_acquire);
  if (should_capture == mouse_captured_on_ui_) return;
  if (!should_capture) {
    window->SetRelativeMouseMode(false);
    if (mouse_captured_on_ui_) window->ReleaseMouse();
    window->SetCursorVisibility(cursor_visibility_before_capture_);
    mouse_captured_on_ui_ = false;
    mouse_capture_active_.store(false, std::memory_order_release);
    mouse_dx_.store(0, std::memory_order_release);
    mouse_dy_.store(0, std::memory_order_release);
    return;
  }
  cursor_visibility_before_capture_ = window->GetCursorVisibility();
  window->SetCursorVisibility(rex::ui::Window::CursorVisibility::kHidden);
  window->CaptureMouse();
  if (!window->SetRelativeMouseMode(true)) {
    window->ReleaseMouse();
    window->SetCursorVisibility(cursor_visibility_before_capture_);
    REXLOG_WARN("SYLPHEED_INPUT relative mouse mode unavailable");
    mouse_capture_active_.store(false, std::memory_order_release);
    return;
  }
  mouse_captured_on_ui_ = true;
  mouse_capture_active_.store(true, std::memory_order_release);
  mouse_dx_.store(0, std::memory_order_release);
  mouse_dy_.store(0, std::memory_order_release);
}
void KeyboardKeystrokeDriver::DetachWindow() {
  rex::ui::Window* window = nullptr;
  {
    std::lock_guard lock(mutex_);
    window = attached_;
    attached_ = nullptr;
  }
  if (!window) return;
  mouse_capture_requested_.store(false, std::memory_order_release);
  window->app_context().CallInUIThreadSynchronous([this, window] {
    // Detach first so no new callback can target this window, then drain a
    // callback that was queued before detachment and release on the UI thread.
    if (mouse_capture_queued_.load(std::memory_order_acquire)) {
      window->app_context().ExecutePendingFunctionsFromUIThread();
    }
    ApplyMouseCaptureOnUIThread(window);
    window->RemoveInputListener(this);
    window->RemoveListener(this);
  });
}
void KeyboardKeystrokeDriver::OnClosing(rex::ui::UIEvent&) { DetachWindow(); }

std::unique_ptr<rex::input::InputSystem> CreateInputSystem(bool tool_mode) {
  auto input = rex::input::CreateDefaultInputSystem(tool_mode);
  if (!tool_mode) {
    auto keyboard = std::make_unique<KeyboardKeystrokeDriver>();
    keyboard->Setup();
    input->AddDriver(std::move(keyboard));
  }
  return input;
}
}  // namespace sylpheed::input
