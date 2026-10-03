#include "input/keyboard_keystroke_driver.h"

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/ui/surface.h>
#include <rex/ui/window.h>
#include <iostream>
#include <stdexcept>
#include <thread>

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

// Minimal stand-ins so the mouse capture path runs against the real SDK
// WindowedAppContext queue and Window state instead of a null window.
class TestAppContext final : public rex::ui::WindowedAppContext {
 public:
  void NotifyUILoopOfPendingFunctions() override {}
  void PlatformQuitFromUIThread() override {}
};

class FakeWindow final : public rex::ui::Window {
 public:
  explicit FakeWindow(rex::ui::WindowedAppContext& context)
      : Window(context, "capture test", 1280, 720) {}
  bool SetRelativeMouseMode(bool enable) override {
    relative_mouse_mode_ = enable;
    return enable;
  }
  bool relative_mouse_mode() const { return relative_mouse_mode_; }
  ~FakeWindow() override { EnterDestructor(); }

 protected:
  bool OpenImpl() override { return true; }
  void RequestCloseImpl() override {}
  std::unique_ptr<rex::ui::Surface> CreateSurfaceImpl(rex::ui::Surface::TypeFlags) override {
    return nullptr;
  }
  void RequestPaintImpl() override {}

 private:
  bool relative_mouse_mode_ = false;
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

    // Visible-cursor mouse input is exposed as a transient left-stick state,
    // then consumed by the next guest poll.
    rex::ui::MouseEvent mouse_down(nullptr, rex::ui::MouseEvent::Button::kLeft,
                                   100, 100, 0, 0, 0.0f, 0.0f);
    events->OnMouseDown(mouse_down);
    events->OnMouseUp(mouse_down);
    rex::ui::MouseEvent mouse_move(nullptr, rex::ui::MouseEvent::Button::kNone,
                                   100, 100, 0, 0, 3.0f, -2.0f);
    events->OnMouseMove(mouse_move);
    Require(input.GetState(0, &state) == 0, "Mouse state unavailable");
    Require(state.gamepad.thumb_lx > 0 && state.gamepad.thumb_ly > 0,
            "Mouse motion did not map to left stick");
    Require(input.GetState(0, &state) == 0 && state.gamepad.thumb_lx == 0 &&
                state.gamepad.thumb_ly == 0,
            "Mouse motion was not consumed");

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

    // Mouse capture lifecycle through the real SDK window queue. The driver
    // is held directly here: the null-window checks above cannot reach the
    // capture path because QueueMouseCapture leaves early without a window.
    TestAppContext capture_context;
    FakeWindow capture_window(capture_context);
    auto capture_driver = std::make_unique<Driver>([&] { return time; });
    auto* capture = capture_driver.get();
    capture->OnWindowAvailable(&capture_window);

    rex::ui::MouseEvent left_down(&capture_window, rex::ui::MouseEvent::Button::kLeft, 100, 100,
                                  0, 0, 0.0f, 0.0f);
    capture->OnMouseDown(left_down);
    Require(!capture_window.IsMouseCaptureRequested(),
            "Mouse capture applied before the UI thread ran the queue");
    capture_context.ExecutePendingFunctionsFromUIThread();
    Require(capture_window.IsMouseCaptureRequested(), "Deferred mouse capture was not applied");
    Require(capture_window.GetCursorVisibility() == rex::ui::Window::CursorVisibility::kHidden,
            "Cursor not hidden while captured");
    Require(capture_window.relative_mouse_mode(), "Relative mouse mode not enabled");

    // Escape releases the capture and restores the previous cursor visibility.
    rex::ui::KeyEvent escape(&capture_window, VK::kEscape, 1, false, false, false, false, false);
    capture->OnKeyDown(escape);
    capture_context.ExecutePendingFunctionsFromUIThread();
    Require(!capture_window.IsMouseCaptureRequested(), "Escape did not release the mouse capture");
    Require(capture_window.GetCursorVisibility() == rex::ui::Window::CursorVisibility::kVisible,
            "Cursor visibility not restored after release");
    Require(!capture_window.relative_mouse_mode(), "Relative mouse mode not disabled on release");

    // Focus loss releases the capture too, and regaining focus re-arms input.
    rex::ui::UISetupEvent capture_focus;
    capture->OnLostFocus(capture_focus);
    capture_context.ExecutePendingFunctionsFromUIThread();
    Require(!capture_window.IsMouseCaptureRequested(),
            "Focus loss did not release the mouse capture");
    capture->OnGotFocus(capture_focus);
    capture->OnMouseDown(left_down);
    capture_context.ExecutePendingFunctionsFromUIThread();
    Require(capture_window.IsMouseCaptureRequested(), "Re-capture after focus return failed");

    // Closing drains a queued release and detaches the window; afterwards the
    // driver can no longer reach it and a second close stays a no-op. The
    // listener lists themselves are private to the SDK window, so detachment
    // is observed through the driver no longer touching the window. Motion
    // synthesis for guest polls is covered by the null-window checks above.
    capture->OnLostFocus(capture_focus);
    rex::ui::UIEvent closing(&capture_window);
    capture->OnClosing(closing);
    Require(!capture_window.IsMouseCaptureRequested(), "Close did not release the pending capture");
    capture->OnMouseDown(left_down);
    capture_context.ExecutePendingFunctionsFromUIThread();
    Require(!capture_window.IsMouseCaptureRequested(), "Detached driver re-captured the window");
    capture->OnClosing(closing);
    Require(!capture_window.relative_mouse_mode(), "Second close re-enabled relative mouse mode");
    capture_driver.reset();

    // Destroying on the UI thread with the window still attached releases the
    // capture through the queue, matching the runtime shutdown path.
    auto ui_thread = std::make_unique<Driver>([&] { return time; });
    ui_thread->OnWindowAvailable(&capture_window);
    ui_thread->OnMouseDown(left_down);
    capture_context.ExecutePendingFunctionsFromUIThread();
    Require(capture_window.IsMouseCaptureRequested(), "Capture before UI-thread destruction failed");
    ui_thread.reset();
    Require(!capture_window.IsMouseCaptureRequested(),
            "UI-thread destruction did not release the capture");
    Require(!capture_window.relative_mouse_mode(),
            "UI-thread destruction left relative mouse mode on");

    // Destroying the driver off the UI thread must not block on the queue and
    // must leave the window untouched (the platform releases capture with it).
    auto off_thread = std::make_unique<Driver>([&] { return time; });
    off_thread->OnWindowAvailable(&capture_window);
    std::thread destroyer([&off_thread] { off_thread.reset(); });
    destroyer.join();
    Require(!capture_window.IsMouseCaptureRequested() && !capture_window.relative_mouse_mode(),
            "Off-UI-thread destruction left the window captured");

    input.Shutdown();
    std::cout << "Keyboard events, aliases, fast taps, repeat, sticks, modifiers, focus, "
                 "capture, physical pad coexistence, capture lifecycle and disabled mode "
                 "passed.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
