#pragma once
#include "ff6/content.hpp"
#include <array>
namespace ff6 {
struct Esper {
    std::array<std::uint8_t, 5> spells{}, rates{};
    std::uint8_t bonus = 255;
};
using EsperData = std::array<Esper, 27>;
EsperData load_espers(const ContentPack&);
} // namespace ff6
