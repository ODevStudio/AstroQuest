#include "input/controller.h"

#include "core/vr/vr_runtime.h"
#include "input/input_handler.h"

namespace Input {

void GameControllers::ProcessSDLGamepadEvent(const SDL_Event& event) {
    const u8 gamepad = GetGamepadIndexFromJoystickId(event.gbutton.which);
    if (gamepad >= 4) {
        return;
    }
    auto* const controller = controllers[gamepad];
    const bool button_event = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                              event.type == SDL_EVENT_GAMEPAD_BUTTON_UP;
    const bool input_down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    if (gamepad == 0 && button_event && event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
        Core::Vr::Runtime::Instance().NotePadButton(Core::Vr::Runtime::PadButton::Home, input_down);
    }
    if (button_event && event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD) {
        controller->Button(Libraries::Pad::OrbisPadButtonDataOffset::TouchPad, input_down);
        return;
    }
    switch (event.type) {
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        switch (static_cast<SDL_SensorType>(event.gsensor.sensor)) {
        case SDL_SENSOR_GYRO:
            controller->UpdateGyro(event.gsensor.data);
            if (gamepad == 0) {
                const float* gyro = event.gsensor.data;
                Core::Vr::Runtime::Instance().UpdatePadGyro({gyro[0], gyro[1], gyro[2]});
            }
            break;
        case SDL_SENSOR_ACCEL:
            controller->UpdateAcceleration(event.gsensor.data);
            if (gamepad == 0) {
                const float* accel = event.gsensor.data;
                Core::Vr::Runtime::Instance().UpdatePadAcceleration({accel[0], accel[1], accel[2]});
            }
            break;
        default:
            break;
        }
        return;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        controller->SetTouchpadState(event.gtouchpad.finger,
                                    event.type != SDL_EVENT_GAMEPAD_TOUCHPAD_UP,
                                    event.gtouchpad.x, event.gtouchpad.y);
        return;
    default:
        break;
    }
    if (UpdatePressedKeys(InputBinding::GetInputEventFromSDLEvent(event))) {
        ActivateOutputsFromInputs();
    }
}

}
