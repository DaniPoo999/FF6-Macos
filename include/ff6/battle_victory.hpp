#pragma once
#include "ff6/battle_actor_scene.hpp"
#include "ff6/battle_message.hpp"
#include "ff6/battle_rewards.hpp"
#include "ff6/music_player.hpp"
#include <memory>
#include "ff6/input.hpp"
namespace ff6 {
struct BattleVictoryAssets {
    BattleMessageAssets messages;
    BattleCharacterAnimationContent characters;
};
BattleVictoryAssets load_battle_victory_assets(const ContentPack& pack);
struct BattleVictoryMusic {
    bool fanfare = true;
    bool fade_on_return = true;
};
BattleVictoryMusic battle_victory_music(bool victory_enabled, bool formation_continues_music,
                                       bool field_continues_music);
class BattleVictory {
  public:
    BattleVictory(std::shared_ptr<const BattleVictoryAssets> assets, const BattleRewards& rewards,
                  const BattleParty& party, const BattleActorScene& actors,
                  const BattleActorSceneRules& rules, std::uint8_t message_speed,
                  bool animate, BattleVictoryMusic music);
    std::vector<MusicPlayerCommand> advance(const BattleActorSceneAssets& assets,
                                            BattleActorScene& actors, BattleActorSceneRules& rules, Buttons buttons);
    void compose(RasterScene& scene, const BattleReadoutAssets& assets,
                 const BattleReadoutView& readout) const;
    bool complete() const { return finished_; }
  private:
    std::shared_ptr<const BattleVictoryAssets> assets_;
    BattleCharacterAnimation characters_;
    std::vector<BattleMessage> messages_;
    std::size_t message_ = 0;
    // C1/0117 explicitly visits WaitFrame twice before the fanfare.
    // The constructor always initializes message_wait_ from the ROM table.
    unsigned waits_ = 2, message_wait_ = 0;
    bool confirm_held_ = false, settled_ = false;
    BattleVictoryMusic music_;
    bool animate_ = true, started_ = false, messages_started_ = false, finished_ = false;
};
} // namespace ff6
