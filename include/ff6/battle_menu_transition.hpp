#pragma once
#include "ff6/content.hpp"
#include <array>
#include <optional>

namespace ff6 {
enum class BattleWindowCanvas {
    closed,
    command,
    slot,
    list,
    magic,
    equipment,
    esper,
    row_def,
    bushido
};
enum class BattleWindowPlane { menu, foreground };
enum class BattleTextBank { command, row_def, character_status, list, esper, equipment };
enum class BattleTextCanvas { readout, command, auxiliary, list, esper, equipment };
enum class BattleWindowMotion { opening, closing };
struct BattleWindowBinding {
    std::optional<BattleWindowCanvas> canvas;
    std::int16_t offset_x = 0, offset_y = 0;
    bool operator==(const BattleWindowBinding&) const = default;
};
// Native text surfaces and their per-line sampling offsets. During transitions,
// a source bank temporarily owns the previous visible surface bindings.
struct BattleTextBinding {
    BattleTextCanvas canvas = BattleTextCanvas::readout;
    std::int16_t offset_x = 0, offset_y = 0;
    bool operator==(const BattleTextBinding&) const = default;
};
struct BattleWindowLayout {
    std::uint16_t top = 0;
    std::uint8_t steps = 0;
    BattleWindowPlane plane = BattleWindowPlane::menu;
    BattleWindowBinding binding;
    bool operator==(const BattleWindowLayout&) const = default;
};
struct BattleTextLayout {
    BattleTextBank bank = BattleTextBank::command;
    std::uint8_t source_line = 0, rows = 0;
    std::uint16_t top = 0;
    bool operator==(const BattleTextLayout&) const = default;
};
struct BattleMenuTransitionAssets {
    std::array<BattleWindowLayout, 28> windows;
    std::array<BattleTextLayout, 11> text;
};
BattleMenuTransitionAssets load_battle_menu_transition_assets(const ContentPack& pack);
struct BattleWindowTravel {
    BattleWindowLayout layout;
    std::int16_t first = 0, second = 0;
    std::uint8_t remaining = 0;
    bool operator==(const BattleWindowTravel&) const = default;
};
struct BattleTextTravel {
    BattleTextLayout layout;
    std::int16_t first = 0, second = 0;
    std::uint8_t remaining = 0;
    bool deferred = false, backed_up = false;
    std::vector<BattleTextBinding> backup;
    bool operator==(const BattleTextTravel&) const = default;
};
struct BattleMenuTransition {
    std::array<BattleWindowBinding, 224> windows{}, foreground{};
    std::array<BattleTextBinding, 224> text{};
    std::array<std::vector<BattleTextBinding>, 6> text_banks;
    std::optional<BattleWindowTravel> window_travel;
    std::optional<BattleTextTravel> text_travel;
    BattleMenuTransition();
    bool busy() const {
        return window_travel.has_value() || text_travel.has_value();
    }
    bool operator==(const BattleMenuTransition&) const = default;
};
// Start only after the caller has prepared the relevant text surfaces. Closing
// may defer text by one visit; border travel is independent. Invalid extents or
// an already active transition fail before changing any retained state.
void begin_battle_menu_transition(BattleMenuTransition& state, BattleWindowLayout window,
                                  BattleTextLayout text, BattleWindowMotion motion,
                                  bool defer_text = false);
// Pending tile publication freezes both tracks, including a deferred text visit.
// Input/window queue advancement belongs to the caller and must wait for !busy().
void advance_battle_menu_transition(BattleMenuTransition& state, bool publication_pending);
} // namespace ff6
