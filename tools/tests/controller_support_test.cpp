#include <array>
#include <cstdio>
#include <string_view>

#include "core/vr/openxr_controller_profiles.h"
#include "input/controller_priority.h"

using Core::Vr::IsPsvr2Headset;
using Core::Vr::OpenXrControllerProfiles;
using Input::GamepadPriority;

bool Equal(const char* actual, std::string_view expected) {
    if (actual == expected) {
        return true;
    }
    std::fprintf(stderr, "Expected %.*s, got %s\n", static_cast<int>(expected.size()),
                 expected.data(), actual);
    return false;
}

int main() {
    constexpr auto standard = OpenXrControllerProfiles(false);
    constexpr auto psvr2 = OpenXrControllerProfiles(true);
    static_assert(standard.size() == 2 && psvr2.size() == 2);
    const auto& touch = standard[0];
    const auto& index = standard[1];
    const auto& sense = psvr2[0];
    if (!Equal(touch.path, "/interaction_profiles/oculus/touch_controller") ||
        !Equal(touch.square, "/user/hand/right/input/b/click") ||
        !Equal(touch.circle, "/user/hand/left/input/x/click") ||
        !Equal(touch.triangle, "/user/hand/left/input/y/click") ||
        !Equal(touch.options, "/user/hand/left/input/menu/click") ||
        !Equal(index.path, "/interaction_profiles/valve/index_controller") ||
        !Equal(index.square, "/user/hand/right/input/b/click") ||
        !Equal(index.circle, "/user/hand/left/input/a/click") ||
        !Equal(index.triangle, "/user/hand/left/input/b/click") ||
        !Equal(index.options, "/user/hand/left/input/trackpad/force") ||
        !Equal(sense.path, "/interaction_profiles/oculus/touch_controller") ||
        !Equal(sense.square, "/user/hand/left/input/x/click") ||
        !Equal(sense.circle, "/user/hand/right/input/b/click") ||
        !Equal(sense.triangle, "/user/hand/left/input/y/click") ||
        !Equal(sense.options, "/user/hand/left/input/menu/click") ||
        !Equal(psvr2[1].path, index.path)) {
        return 1;
    }
    for (const auto name : {"PlayStation VR2", "SteamVR/PSVR2", "PS VR2", "playstation vr2",
                            "SteamVR/OpenXR : playstation_vr2"}) {
        if (!IsPsvr2Headset(name)) {
            std::fprintf(stderr, "Did not recognize %s\n", name);
            return 1;
        }
    }
    for (const auto name : {"Valve Index", "Meta Quest 3", "PlayStation VR", "", "PSVR",
                            "SteamVR/OpenXR : lighthouse"}) {
        if (IsPsvr2Headset(name)) {
            std::fprintf(stderr, "Incorrect Sense layout for %s\n", name);
            return 1;
        }
    }
    for (const auto product : std::array{0x0ce6, 0x0df2}) {
        const int ps5 = GamepadPriority(0x054c, product);
        if (ps5 != 0 || ps5 >= GamepadPriority(0x054c, 0x05c4) ||
            ps5 >= GamepadPriority(0x045e, 0x028e) || ps5 >= GamepadPriority(0x054c, 0x0e45) ||
            ps5 >= GamepadPriority(0x054c, 0x0e46) || GamepadPriority(0x045e, product) == ps5) {
            std::fputs("PS5 gamepad priority check failed\n", stderr);
            return 1;
        }
    }
    std::puts("Touch, Index, PSVR2 Sense bindings and PS5 gamepad priority checks passed");
    return 0;
}
