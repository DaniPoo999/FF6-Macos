#pragma once
#include "ff6/battle_entrance.hpp"
#include "rom-materials/final_battle_schedule.macros.inc"

namespace ff6 {
std::optional<std::uint16_t> next_final_battle(std::uint16_t formation);
BattlePartyContext final_battle_party_context(const BattleState& battle, const BattleParty& party);
void retain_final_battle_state(BattleState& next, const BattleState& previous,
                              const BattlePartyContext& retained);
void resume_final_battle_jumps(BattleState& battle);
struct BattleTierPicture {
    RasterScene terrain;
    std::vector<BattleActorSpriteGroup> actors;
};
BattleTierPicture final_tier_picture(const BattleEntranceAssets& assets, const BattleEntranceSeed& seed);
// Native scene transition; no register streams or original graphics execution.
class BattleFinalTransition {
  public:
    BattleFinalTransition(BattleTierPicture outgoing, BattleTierPicture incoming, bool last);
    std::vector<MusicPlayerCommand> advance();
    RasterScene scene() const;
    bool complete() const { return age_ >= FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_6; }
    unsigned elapsed() const { return age_; }
  private:
    BattleTierPicture outgoing_, incoming_;
    bool last_;
    unsigned age_ = 0;
};
} // namespace ff6
