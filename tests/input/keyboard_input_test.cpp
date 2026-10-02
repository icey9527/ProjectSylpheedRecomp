#include "input/keyboard_keystroke_driver.h"

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <iostream>
#include <stdexcept>

using namespace rex::input;
using rex::X_RESULT;
using rex::X_STATUS;
using VK = rex::ui::VirtualKey;
using Driver = sylpheed::input::KeyboardKeystrokeDriver;

void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

class PhysicalPad final : public InputDriver {
 public:
  PhysicalPad() : InputDriver(nullptr, 0) {}
  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    DeviceInfo info;
    info.id = static_cast<DeviceId>(1);
    info.name = "Test physical pad";
    out.push_back(info);
  }
  X_RESULT GetDeviceState(DeviceId, X_INPUT_STATE* state) override {
    if (state) {
      *state = {};
      state->packet_number = 100;
      state->gamepad.buttons = X_INPUT_GAMEPAD_X;
    }
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceCapabilities(DeviceId, uint32_t, X_INPUT_CAPABILITIES* caps) override {
    *caps = {};
    caps->type = 1;
    caps->sub_type = 3;
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetDeviceVibration(DeviceId, X_INPUT_VIBRATION*) override {
    ++vibrations;
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceKeystroke(DeviceId, uint32_t, X_INPUT_KEYSTROKE* out) override {
    if (!pending) return X_ERROR_EMPTY;
    *out = {};
    out->virtual_key = uint16_t(VK::kXInputPadY);
    out->flags = X_INPUT_KEYSTROKE_KEYDOWN;
    pending = false;
    return X_ERROR_SUCCESS;
  }
  bool pending = false;
  unsigned vibrations = 0;
};

int main() {
  try {
    rex::cvar::SetFlagByName("mnk_mode", "true");
    rex::cvar::SetFlagByName("mnk_mouse", "false");
    rex::cvar::SetFlagByName("keybind_a", "Return,Space");
    rex::cvar::SetFlagByName("keybind_start", "X");
    rex::cvar::SetFlagByName("keybind_dpad_up", "Up");
    rex::cvar::SetFlagByName("keybind_rstick_up", "Numpad8");
    rex::cvar::SetFlagByName("keybind_rstick_right", "Numpad6");
    auto time = Driver::Clock::time_point{};
    InputSystem input(nullptr);
    auto keyboard_state = std::make_unique<mnk::MnkInputDriver>(nullptr, 0);
    auto* state_driver = keyboard_state.get();
    auto keyboard_events = std::make_unique<Driver>([&] { return time; });
    auto* events = keyboard_events.get();
    auto physical = std::make_unique<PhysicalPad>();
    auto* pad = physical.get();
    input.AddDriver(std::move(physical));
    input.AddDriver(std::move(keyboard_state));
    input.AddDriver(std::move(keyboard_events));
    input.SetDeviceAssignment(std::make_unique<SlotAssignment>());

    auto key = [&](VK vk, bool down) {
      rex::ui::KeyEvent event(nullptr, vk, 1, false, false, false, false, false);
      if (down) { state_driver->OnKeyDown(event); events->OnKeyDown(event); }
      else { state_driver->OnKeyUp(event); events->OnKeyUp(event); }
      Require(!event.is_handled(), "Input adapter must not consume UI events");
    };
    auto next = [&](VK vk, uint16_t flags) {
      X_INPUT_KEYSTROKE stroke{};
      Require(input.GetKeystroke(0, 3, &stroke) == X_ERROR_SUCCESS, "Expected keyboard event");
      Require(stroke.virtual_key == uint16_t(vk) && stroke.flags == flags, "Wrong VK_PAD event/flags");
      Require(stroke.user_index == 0, "Wrong guest user");
    };
    auto empty = [&] {
      X_INPUT_KEYSTROKE stroke{};
      Require(input.GetKeystroke(0, 3, &stroke) == X_ERROR_EMPTY, "Unexpected duplicate/stale event");
    };
    constexpr auto down = X_INPUT_KEYSTROKE_KEYDOWN, up = X_INPUT_KEYSTROKE_KEYUP;
    X_INPUT_CAPABILITIES caps{};
    Require(input.GetCapabilities(0, 1, &caps) == 0 && caps.sub_type == 3, "Physical capabilities changed");
    key(VK::kReturn, true);
    X_INPUT_STATE state{};
    Require(input.GetState(0, &state) == 0, "Controller state unavailable");
    Require(state.gamepad.buttons == (X_INPUT_GAMEPAD_A | X_INPUT_GAMEPAD_X), "SDK state merge changed");
    next(VK::kXInputPadA, down);
    key(VK::kSpace, true); empty();
    key(VK::kReturn, false); empty();
    key(VK::kSpace, false); next(VK::kXInputPadA, up); empty();

    // A complete tap between game polls must not be lost.
    key(VK::kReturn, true); key(VK::kReturn, false);
    next(VK::kXInputPadA, down); next(VK::kXInputPadA, up); empty();
    key(VK::kUp, true); next(VK::kXInputPadDpadUp, down);
    time += std::chrono::milliseconds(399); empty();
    time += std::chrono::milliseconds(1);
    next(VK::kXInputPadDpadUp, down | X_INPUT_KEYSTROKE_REPEAT);
    time += std::chrono::milliseconds(99); empty();
    time += std::chrono::milliseconds(1);
    next(VK::kXInputPadDpadUp, down | X_INPUT_KEYSTROKE_REPEAT);
    key(VK::kUp, false); next(VK::kXInputPadDpadUp, up); empty();

    key(VK::kW, true); next(VK::kXInputPadLThumbUp, down);
    key(VK::kD, true);
    next(VK::kXInputPadLThumbUp, up); next(VK::kXInputPadLThumbUpRight, down);
    key(VK::kW, false);
    next(VK::kXInputPadLThumbUpRight, up); next(VK::kXInputPadLThumbRight, down);
    key(VK::kD, false); next(VK::kXInputPadLThumbRight, up); empty();
    key(VK::kQ, true); next(VK::kXInputPadLTrigger, down);
    key(VK::kQ, false); next(VK::kXInputPadLTrigger, up);
    key(VK::kNumpad8, true); next(VK::kXInputPadRThumbUp, down);
    key(VK::kNumpad8, false); next(VK::kXInputPadRThumbUp, up);

    rex::cvar::SetFlagByName("keybind_dpad_up", "Shift+Up");
    key(VK::kUp, true); empty();
    key(VK::kShift, true); next(VK::kXInputPadDpadUp, down);
    key(VK::kShift, false); next(VK::kXInputPadDpadUp, up);
    key(VK::kUp, false); empty();
    rex::cvar::SetFlagByName("keybind_dpad_up", "Up");

    rex::ui::UISetupEvent focus;
    key(VK::kReturn, true); next(VK::kXInputPadA, down);
    state_driver->OnLostFocus(focus); events->OnLostFocus(focus);
    events->OnLostFocus(focus);
    next(VK::kXInputPadA, up);
    time += std::chrono::seconds(1); empty();
    key(VK::kReturn, true); empty();
    state_driver->OnGotFocus(focus); events->OnGotFocus(focus);
    key(VK::kReturn, true);  // Pending down must be discarded on focus loss.
    state_driver->OnLostFocus(focus); events->OnLostFocus(focus);
    next(VK::kXInputPadA, up); empty();
    state_driver->OnGotFocus(focus); events->OnGotFocus(focus);

    bool active = true;
    input.SetActiveCallback([&] { return active; });
    key(VK::kReturn, true); next(VK::kXInputPadA, down);
    active = false; next(VK::kXInputPadA, up); empty();
    key(VK::kReturn, false); active = true;
    rex::ui::KeyEvent handled(nullptr, VK::kReturn, 1, false, false, false, false, false);
    handled.set_handled(true); events->OnKeyDown(handled); empty();

    pad->pending = true; key(VK::kReturn, true);
    next(VK::kXInputPadY, down); next(VK::kXInputPadA, down);
    key(VK::kReturn, false); next(VK::kXInputPadA, up); empty();
    X_INPUT_VIBRATION vibration{};
    Require(input.SetState(0, &vibration) == 0 && pad->vibrations == 1, "Physical vibration changed");
    X_INPUT_KEYSTROKE stroke{};
    Require(input.GetKeystroke(1, 3, &stroke) == X_ERROR_DEVICE_NOT_CONNECTED, "Extra guest user connected");
    std::vector<DeviceInfo> devices;
    events->EnumerateDevices(devices);
    Require(events->GetDeviceKeystroke(devices[0].id, 3, nullptr) == X_ERROR_BAD_ARGUMENTS, "Null output accepted");
    rex::cvar::SetFlagByName("mnk_mode", "false");
    key(VK::kReturn, true); empty();
    rex::cvar::SetFlagByName("mnk_mode", "true"); empty();
    input.Shutdown();
    std::cout << "Keyboard events, aliases, fast taps, repeat, sticks, modifiers, focus, "
                 "capture, physical pad coexistence and disabled mode passed.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
