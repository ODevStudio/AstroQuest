#pragma once

#include <cstdint>

namespace Input {

constexpr bool IsPsvr2SenseGamepad(std::uint16_t vendor, std::uint16_t product) {
    return vendor == 0x054c && (product == 0x0e45 || product == 0x0e46);
}

constexpr int GamepadPriority(std::uint16_t vendor, std::uint16_t product) {
    if (vendor == 0x054c && (product == 0x0ce6 || product == 0x0df2)) {
        return 0;
    }
    return vendor == 0x054c ? 1 : 2;
}

}
