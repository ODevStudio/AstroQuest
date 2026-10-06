#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace Core::Vr {

struct OpenXrControllerProfile {
    const char* name;
    const char* path;
    const char* square;
    const char* circle;
    const char* triangle;
    const char* options;
};

constexpr auto OpenXrControllerProfiles(bool sense) {
    const OpenXrControllerProfile touch{
        "Touch",
        "/interaction_profiles/oculus/touch_controller",
        "/user/hand/right/input/b/click",
        "/user/hand/left/input/x/click",
        "/user/hand/left/input/y/click",
        "/user/hand/left/input/menu/click",
    };
    const OpenXrControllerProfile index{
        "Index",
        "/interaction_profiles/valve/index_controller",
        "/user/hand/right/input/b/click",
        "/user/hand/left/input/a/click",
        "/user/hand/left/input/b/click",
        "/user/hand/left/input/trackpad/force",
    };
    const OpenXrControllerProfile psvr2{
        "PSVR2 Sense",
        "/interaction_profiles/oculus/touch_controller",
        "/user/hand/left/input/x/click",
        "/user/hand/right/input/b/click",
        "/user/hand/left/input/y/click",
        "/user/hand/left/input/menu/click",
    };
    return std::array{sense ? psvr2 : touch, index};
}

inline bool IsPsvr2Headset(std::string_view name) {
    for (const std::string_view model :
         {"playstation vr2", "playstation_vr2", "psvr2", "ps vr2"}) {
        const auto match =
            std::ranges::search(name, model, [](unsigned char actual, char expected) {
                return std::tolower(actual) == expected;
            });
        if (!match.empty()) {
            return true;
        }
    }
    return false;
}

}
