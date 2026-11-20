#pragma once
#include "ff6/battle_actor_scene.hpp"
#include "ff6/battle_effects.hpp"
#include "ff6/battle_monster_transition.hpp"
#include "ff6/battle_fade.hpp"
#include "ff6/battle_scene.hpp"
#include "ff6/music_player.hpp"

namespace ff6 {
struct BattleEntranceData {
    std::array<bool, 17> characters_first{};
    std::array<std::optional<std::uint16_t>, 18> monster_properties{};
    std::array<std::optional<std::uint8_t>, 8> songs{};
};
BattleEntranceData load_battle_entrance_data(const ContentPack& pack);
std::optional<std::uint8_t> select_battle_song(const BattleEntranceData& data,
                                               const BattleFormation& formation,
                                               bool continue_field_music,
                                               std::optional<std::uint8_t> scripted_song = {});
struct BattleEntranceAssets {
    BattleActorSceneAssets actors;
    BattleBackground background;
    BattleReadoutAssets readouts;
    BattleCharacterAnimationContent character_programs;
    std::shared_ptr<const BattleEffectLibrary> effects;
    std::optional<std::uint16_t> monster_effect;
    bool flashing_monsters = false;
};
struct BattleEntranceSeed {
    std::bitset<4> enemy_character_proxy;
    BattleActorScene actors;
    BattleActorSceneRules rules;
    BattleReadoutView readout;
    std::array<BattleAnimationParticipant, 4> characters;
    BattleApproach approach = BattleApproach::normal;
    bool characters_first = false;
    bool hide_names = false;
    std::optional<std::uint8_t> song;
};
enum class BattleEntrancePhase {
    start,
    music_ready,
    first_stage,
    character_commands,
    monster_commands,
    cleanup_publish,
    cleanup_finish,
    next_stage,
    final_fade,
    complete
};
struct BattleEntranceTick {
    bool interval = false, image_changed = false, scene_updated = false;
    bool battle_time_requested = false;
    std::vector<MusicPlayerCommand> audio;
};
// A retained native caller. Commands consume bounded work; only explicit frame
// waits publish a scene. The host's callback size and refresh rate cannot tick it.
class BattleEntrance {
  public:
    BattleEntrance(BattleEntranceAssets assets, BattleEntranceSeed seed);
    BattleEntranceTick advance(bool audio_ready, std::size_t command_budget = 1024);
    void settle_final_tier(std::uint16_t clock);
    void update_gauges(const BattleParty& party, const BattleState& battle);
    const BattleEntranceAssets& scene_assets() const {
        return assets_;
    }
    const BattleEntranceSeed& scene_seed() const {
        return seed_;
    }
    const BattleGaugePublication& gauges() const {
        return gauges_;
    }
    bool complete() const {
        return phase_ == BattleEntrancePhase::complete;
    }
    BattleEntrancePhase phase() const {
        return phase_;
    }
    const BattleActorScene& actors() const {
        return seed_.actors;
    }
    const BattleEntryFade& fade() const {
        return fade_;
    }
    std::uint32_t frames() const {
        return frames_;
    }
    const std::optional<RasterScene>& visible() const {
        return visible_;
    }
    std::uint32_t character_passes() const {
        return character_passes_;
    }
    std::uint32_t battle_time_requests() const {
        return battle_time_requests_;
    }
    std::uint32_t monster_passes() const {
        return monster_passes_;
    }

  private:
    BattleEntranceAssets assets_;
    BattleEntranceSeed seed_;
    BattleGaugePublication gauges_;
    std::array<BattleCounterReadout, 4> counters_{};
    BattleEntryFade fade_;
    BattleEntrancePhase phase_ = BattleEntrancePhase::start;
    std::optional<RasterScene> visible_;
    std::optional<BattleCharacterAnimation> characters_;
    std::optional<BattleAttackAnimation> monsters_;
    std::optional<BattleMonsterTransition> special_monsters_;
    BattleEffectContext special_context_;
    bool second_stage_ = false;
    std::uint32_t frames_ = 0, character_passes_ = 0, monster_passes_ = 0,
                  battle_time_requests_ = 0;
    void publish_frame(BattleEntranceTick& tick);
    void start_stage();
    void reset_priorities();
    void finish_commands();
};
BattleEntrance make_battle_entrance(const ContentPack& pack, const BattleFormationEntry& formation,
                                    const BattleParty& party, const BattlePreparation& preparation,
                                    const BattleState& battle,
                                    const BattleSceneAppearance& appearance,
                                    bool continue_field_music,
                                    std::span<const std::optional<Tile>> retained_terrain = {});
} // namespace ff6
