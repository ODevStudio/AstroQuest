#include <cstdio>
#include <cstdlib>
#include <thread>

#define LOG_DEBUG(...) ((void)0)
#define LOG_CRITICAL(...) ((void)0)

#include "input/controller.cpp"
#include "input/controller_events.cpp"
#include "input/input_handler.cpp"
#include "core/vr/vr_runtime.cpp"

using Buttons = Libraries::Pad::OrbisPadButtonDataOffset;
constexpr std::array<int, 6> NeutralAxes{128, 128, 128, 128, 0, 0};

void assert_fail_impl() { std::abort(); }
[[noreturn]] void unreachable_impl() { std::abort(); }

namespace Libraries::Kernel {
u64 PS4_SYSV_ABI sceKernelGetProcessTime() { return SDL_GetTicksNS() / 1000; }
}

namespace Libraries::SystemService {
void PushSystemServiceEvent(const OrbisSystemServiceEvent&) {}
}

namespace Core::Vr {
HostLink& HostLink::Instance() {
    static HostLink link;
    return link;
}
bool HostLink::Start() { return false; }
}

namespace Common::FS {
const std::filesystem::path& GetUserPath(PathType) {
    static const auto path = std::filesystem::temp_directory_path() / "astroquest-controller-test";
    return path;
}
}

namespace Overlay {
void ShowVolume() { std::abort(); }
}

namespace ImGuiEmuSettings {
void OpenInGameSettingsDialog() { std::abort(); }
}

namespace Input {
void ApplyMouseInputBlockers() {}
void SetMouseToJoystick(int) {}
void SetMouseParams(float, float, float) {}
void SetMouseGyroRollMode(bool) {}
}

UserSettingsImpl::UserSettingsImpl() = default;
UserSettingsImpl::~UserSettingsImpl() = default;
std::shared_ptr<UserSettingsImpl> UserSettingsImpl::GetInstance() {
    static const auto settings = std::make_shared<UserSettingsImpl>();
    return settings;
}

User* UserManager::GetUserByPlayerIndex(s32 index) {
    const auto user = std::ranges::find(m_users.user, index, &User::player_index);
    return user == m_users.user.end() ? nullptr : &*user;
}
User* UserManager::GetUserByID(s32 id) {
    const auto user = std::ranges::find(m_users.user, id, &User::user_id);
    return user == m_users.user.end() ? nullptr : &*user;
}
void UserManager::LoginUser(User* user, s32) { user->logged_in = true; }

EmulatorSettingsImpl::EmulatorSettingsImpl() = default;
EmulatorSettingsImpl::~EmulatorSettingsImpl() = default;
std::shared_ptr<EmulatorSettingsImpl> EmulatorSettingsImpl::GetInstance() {
    static const auto settings = std::make_shared<EmulatorSettingsImpl>();
    return settings;
}

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError());
        std::exit(1);
    }
}

Input::GameControllers& Controllers() { return Input::ControllerOutput::controllers; }
Input::GameController& Primary() { return *Controllers()[0]; }

Input::State ReadState() {
    Input::State state;
    bool connected;
    int count;
    Primary().ReadState(&state, &connected, &count);
    return state;
}

SDL_JoystickID AttachGamepad(Uint16 product, Uint16 vendor = 0x054c) {
    SDL_VirtualJoystickDesc descriptor{};
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    descriptor.naxes = SDL_GAMEPAD_AXIS_COUNT;
    descriptor.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    descriptor.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    descriptor.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    descriptor.vendor_id = vendor;
    descriptor.product_id = product;
    descriptor.name = "Controller handover test";
    const auto id = SDL_AttachVirtualJoystick(&descriptor);
    Check(id != 0, "attach virtual gamepad");
    return id;
}

void ButtonEvent(SDL_JoystickID id, SDL_GamepadButton button, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.which = id;
    event.gbutton.button = button;
    event.gbutton.down = down;
    Controllers().ProcessSDLGamepadEvent(event);
}

void TouchEvent(SDL_JoystickID id, int finger) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN;
    event.gtouchpad.which = id;
    event.gtouchpad.finger = finger;
    event.gtouchpad.x = 0.3f;
    event.gtouchpad.y = 0.4f;
    Controllers().ProcessSDLGamepadEvent(event);
}

void SensorEvent(SDL_JoystickID id, SDL_SensorType sensor, float y) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_SENSOR_UPDATE;
    event.gsensor.which = id;
    event.gsensor.sensor = sensor;
    event.gsensor.data[1] = y;
    Controllers().ProcessSDLGamepadEvent(event);
}

void KeyboardEvent(bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = SDLK_SPACE;
    event.key.down = down;
    if (Input::UpdatePressedKeys(Input::InputBinding::GetInputEventFromSDLEvent(event))) {
        Input::ActivateOutputsFromInputs();
    }
}

void Bind(Input::InputID input, SDL_GamepadButton button) {
    auto& outputs = Input::output_arrays[0].data;
    const auto output = std::ranges::find(outputs, Input::ControllerOutput(button));
    Check(output != outputs.end(), "find output");
    Input::connections.emplace_back(Input::InputBinding(input), &*output);
}

void CheckReset(Buttons expected = Buttons::None) {
    const auto state = ReadState();
    if (state.buttonsState != expected || state.axes != NeutralAxes) {
        std::fprintf(stderr, "Reset state: buttons=%x expected=%x axes=%d,%d,%d,%d,%d,%d pad=%u\n",
                     static_cast<unsigned>(state.buttonsState), static_cast<unsigned>(expected),
                     state.axes[0], state.axes[1], state.axes[2], state.axes[3], state.axes[4],
                     state.axes[5], SDL_GetGamepadID(Primary().m_sdl_gamepad));
    }
    Check(state.buttonsState == expected && state.axes == NeutralAxes, "buttons and axes reset");
    Check(!state.touchpad[0].state && !state.touchpad[1].state, "both touch contacts reset");
    Check(state.angularVelocity.x == 0 && state.angularVelocity.y == 0 &&
          state.angularVelocity.z == 0 && state.acceleration.y == Input::State{}.acceleration.y,
          "sensor state reset");
    Check(Primary().gyro_buf[1] == 0 && Primary().accel_buf[1] == 9.81f &&
          Primary().gyro_poll_rate == 0 && Primary().accel_poll_rate == 0, "sensor buffers reset");
    Check(Primary().GetLastOrientation().w == 1 && Primary().GetLastUpdate() ==
          std::chrono::steady_clock::time_point{}, "orientation history reset");
    Check(Primary().last_touch_down_timestamp == 0 && Primary().GetTouchCount() == 0 &&
          Primary().GetSecondaryTouchCount() == 0 && Primary().GetPreviousTouchNum() == 0 &&
          !Primary().WasSecondaryTouchReset(), "touch bookkeeping reset");
    Check(!Core::Vr::Runtime::Instance().PadMotionKnown(), "VR sensor attitude reset");
}

void CheckEmptyQueue() {
    std::array<Input::State, 64> states;
    bool connected;
    int count;
    Check(Primary().ReadStates(states.data(), states.size(), &connected, &count) == 0,
          "outgoing state queue cleared");
}

void TestHandover() {
    auto& runtime = Core::Vr::Runtime::Instance();
    Controllers().TryOpenSDLControllers();
    const Core::Vr::DeviceState old_pose{.pose = {.position = {1, 2, 3}}, .tracked = true};
    Check(Primary().ApplyRemoteState(Buttons::Cross, {220, 128, 128, 128, 0, 0}, true, 0.3f, 0.4f),
          "remote source initially active");
    Primary().ApplyRemotePose(&old_pose);
    const auto sense_left = AttachGamepad(0x0e45);
    const auto sense_right = AttachGamepad(0x0e46);
    Controllers().TryOpenSDLControllers();
    Check(!Primary().HasPhysicalController() && ReadState().buttonsState == Buttons::Cross,
          "Sense halves leave the combined remote source active");
    Check(Controllers().GetGamepadIndexFromJoystickId(sense_left) >= 4 &&
          Controllers().GetGamepadIndexFromJoystickId(sense_right) >= 4, "Sense events unassigned");

    const auto ds4 = AttachGamepad(0x05c4);
    Controllers().TryOpenSDLControllers();
    CheckReset();
    CheckEmptyQueue();
    ButtonEvent(ds4, SDL_GAMEPAD_BUTTON_SOUTH, true);
    TouchEvent(ds4, 0);
    TouchEvent(ds4, 1);
    SensorEvent(ds4, SDL_SENSOR_ACCEL, 9.81f);
    SensorEvent(ds4, SDL_SENSOR_GYRO, 2.0f);
    Primary().Gyro(0);
    Primary().Acceleration(0);
    Primary().gyro_poll_rate = 100;
    Primary().accel_poll_rate = 100;
    Primary().SetTouchCount(5);
    Primary().SetSecondaryTouchCount(6);
    Primary().SetPreviousTouchNum(2);
    auto orientation = Libraries::Pad::OrbisFQuaternion{1, 0, 0, 0};
    Primary().SetLastOrientation(orientation);
    Primary().SetLastUpdate(std::chrono::steady_clock::now());
    Check(ReadState().touchpad[1].state && runtime.PadMotionKnown(), "outgoing device state seeded");

    const auto ds5 = AttachGamepad(0x0ce6);
    Controllers().TryOpenSDLControllers();
    Check(SDL_GetGamepadID(Primary().m_sdl_gamepad) == ds5, "DualSense hot-plug promotion");
    CheckReset();
    CheckEmptyQueue();
    auto* const joystick = SDL_GetGamepadJoystick(Primary().m_sdl_gamepad);
    Check(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, 20000), "move right stick");
    SDL_UpdateJoysticks();
    Primary().Gyro(0);
    Check(ReadState().touchpad[0].state, "right-stick emulation works after replacement");
    Check(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, 0), "release right stick");
    SDL_UpdateJoysticks();
    Primary().Gyro(0);

    ButtonEvent(ds5, SDL_GAMEPAD_BUTTON_SOUTH, true);
    SensorEvent(ds5, SDL_SENSOR_ACCEL, 9.81f);
    SensorEvent(ds5, SDL_SENSOR_GYRO, 0);
    const auto physical_sequence = runtime.GetPad().sequence;
    std::thread late_remote([&] {
        for (int i = 0; i < 1000; ++i) {
            Check(!Primary().ApplyRemoteState(Buttons::None, NeutralAxes, false, 0.5f, 0.5f),
                  "physical source rejects late remote cleanup");
            Primary().ApplyRemotePose(nullptr);
            Primary().ApplyRemotePose(&old_pose);
        }
    });
    late_remote.join();
    Check(ReadState().buttonsState == Buttons::Cross && runtime.GetPad().sequence == physical_sequence,
          "held physical input and pose survive late remote writes");
    ButtonEvent(ds4, SDL_GAMEPAD_BUTTON_EAST, true);
    TouchEvent(ds4, 1);
    SensorEvent(ds4, SDL_SENSOR_GYRO, 10);
    Check(ReadState().buttonsState == Buttons::Cross && !ReadState().touchpad[1].state &&
          runtime.GetPad().sequence == physical_sequence, "unused gamepad events ignored");

    KeyboardEvent(true);
    Check(ReadState().buttonsState == (Buttons::Cross | Buttons::Triangle), "keyboard binding active");
    Check(SDL_DetachVirtualJoystick(ds5), "detach DualSense");
    Controllers().TryOpenSDLControllers();
    Check(SDL_GetGamepadID(Primary().m_sdl_gamepad) == ds4, "remaining gamepad takes over");
    CheckReset(Buttons::Triangle);
    KeyboardEvent(false);
    const auto edge = AttachGamepad(0x0df2);
    Controllers().TryOpenSDLControllers();
    Check(SDL_GetGamepadID(Primary().m_sdl_gamepad) == edge, "DualSense Edge hot-plug promotion");
    CheckReset();
    Check(SDL_DetachVirtualJoystick(edge) && SDL_DetachVirtualJoystick(ds4), "detach physical pads");
    Controllers().TryOpenSDLControllers();
    CheckReset();
    Check(Primary().ApplyRemoteState(Buttons::Cross, NeutralAxes, false, 0.5f, 0.5f),
          "remote fallback resumes with the previously held button");
    Check(ReadState().buttonsState == Buttons::Cross, "fallback state applied after reset");
    Check(SDL_DetachVirtualJoystick(sense_left) && SDL_DetachVirtualJoystick(sense_right),
          "detach Sense halves");
}

int main(int argc, char** argv) {
    for (const auto hint : {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_XINPUT_ENABLED,
                            SDL_HINT_JOYSTICK_RAWINPUT, SDL_HINT_JOYSTICK_DIRECTINPUT,
                            SDL_HINT_JOYSTICK_WGI, SDL_HINT_JOYSTICK_GAMEINPUT}) {
        SDL_SetHint(hint, "0");
    }
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    Check(SDL_Init(SDL_INIT_GAMEPAD), "initialize SDL");
    int count;
    auto* const initial = SDL_GetGamepads(&count);
    SDL_free(initial);
    Check(count == 0, "physical devices must be disabled");
    Check(!std::filesystem::exists(Common::FS::GetUserPath(Common::FS::PathType::UserDir)),
          "test configuration path must not exist");
    const bool flat = argc > 1 && std::string_view(argv[1]) == "--flat";
    const bool scripted = argc > 1 && std::string_view(argv[1]) == "--script";
    _putenv_s("SHADPS4_VR", flat ? "0" : "1");
    _putenv_s("SHADPS4_VR_ONE_PLAYER", "1");
    _putenv_s("SHADPS4_STICK_TOUCHPAD", "1");
    _putenv_s("SHADPS4_INPUT_SCRIPT", scripted ? "test-script" : "");
    Core::Vr::Runtime::Instance().Configure(true, !flat);
    EmulatorSettings.SetMotionControlsEnabled(false);
    User user;
    user.user_id = 1;
    user.player_index = 1;
    UserManagement.GetUsers().user.push_back(user);
    Bind({Input::InputType::Controller, SDL_GAMEPAD_BUTTON_SOUTH, 1}, SDL_GAMEPAD_BUTTON_SOUTH);
    Bind({Input::InputType::Controller, SDL_GAMEPAD_BUTTON_EAST, 1}, SDL_GAMEPAD_BUTTON_EAST);
    Bind({Input::InputType::KeyboardMouse, SDLK_SPACE, 1}, SDL_GAMEPAD_BUTTON_NORTH);
    if (flat) {
        const auto sense = AttachGamepad(0x0e45);
        Controllers().TryOpenSDLControllers();
        Check(SDL_GetGamepadID(Primary().m_sdl_gamepad) == sense, "flat-mode selection unchanged");
        Check(SDL_DetachVirtualJoystick(sense), "detach flat-mode gamepad");
    } else if (scripted) {
        const auto ds5 = AttachGamepad(0x0ce6);
        Controllers().TryOpenSDLControllers();
        Check(!Primary().HasPhysicalController() && Primary().ApplyRemoteState(
              Buttons::Square, NeutralAxes, false, 0.5f, 0.5f), "script retains input priority");
        Check(SDL_DetachVirtualJoystick(ds5), "detach scripted-mode gamepad");
    } else {
        TestHandover();
    }
    Controllers().TryOpenSDLControllers();
    SDL_Quit();
    std::puts("PASS: controller ownership, full reset, event routing and fallback");
}
