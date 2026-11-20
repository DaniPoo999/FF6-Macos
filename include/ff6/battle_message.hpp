#pragma once
#include "ff6/battle_readout.hpp"
#include "ff6/battle_state.hpp"
#include "ff6/input.hpp"
namespace ff6 {
struct BattleMessageAssets {
    Bytes messages, pointers, item_names;
    std::vector<Glyph> glyphs;
    std::array<std::uint8_t, 256> widths{};
    Bytes attack_names, magic_names, esper_names, bushido_names, command_names;
};
BattleMessageAssets load_battle_message_assets(const ContentPack& pack);
enum class BattleMessageStepKind { glyph, newline, wait, confirm };
struct BattleMessageStep {
    BattleMessageStepKind kind;
    std::uint8_t value = 0;
};
struct BattleMessage {
    Bytes glyphs;
    bool wait_for_confirm = false;
    std::vector<BattleMessageStep> steps{};
    bool attack_name = false;
};
// Spell, esper and other ability names use their original distinct strides.
BattleMessage format_battle_ability_name(const BattleMessageAssets& assets, std::uint8_t attack);
BattleMessage format_battle_command_name(const BattleMessageAssets&, std::uint8_t command);
BattleMessage format_battle_bushido_name(const BattleMessageAssets&, std::uint8_t attack);
BattleMessage format_battle_item_name(const BattleMessageAssets& assets, std::uint8_t item);
BattleMessage format_battle_message(const BattleMessageAssets& assets, std::uint8_t message,
                                   std::array<std::uint32_t, 4> values, const BattleParty& party);
std::uint8_t battle_announcement_message(BattleAnnouncement announcement);
// Attack messages share the original message-speed delay. Their foreground
// surface owns presentation while ordinary menus and combat clocks continue.
class BattleMessagePresentation {
  public:
    BattleMessagePresentation(BattleMessage message, std::uint8_t speed);
    void advance(Buttons buttons);
    bool complete() const {
        return remaining_ == 0 && at_ == steps_.size() && !wait_ && !key_wait_ && !scroll_;
    }
    unsigned scroll() const {
        return scroll_;
    }
    const BattleMessage& message() const { return message_; }
  private:
    BattleMessage message_;
    unsigned remaining_;
    bool confirm_held_ = false, key_wait_ = false, confirmed_text_ = false;
    std::vector<BattleMessageStep> steps_;
    std::size_t at_ = 0;
    unsigned wait_ = 0, scroll_ = 0;
};
void compose_battle_message(RasterScene& scene, const BattleReadoutAssets& assets,
                            const BattleReadoutView& readout,
                            const BattleMessageAssets& text_assets, const BattleMessage& message,
                            bool dialogue = false, unsigned scroll = 0);
} // namespace ff6
