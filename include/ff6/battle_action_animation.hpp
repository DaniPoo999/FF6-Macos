#pragma once
#include "ff6/battle_effects.hpp"
#include "ff6/battle_monster_transition.hpp"

namespace ff6 {
enum class BattleActionAnimationKind {
    magic,
    rage_magic,
    magitek,
    monster_fight,
    monster_special,
    monster_effect,
    character_fight,
    character_special,
    item,
    command_effect,
    status_effect,
    outcome_only,
    escape,
    bushido,
    blitz,
    health,
    monster_transition
};
enum class BattleActionAnimationPhase {
    pre_cast,
    release_pre_cast,
    attack,
    release_attack,
    clear_images,
    returning,
    settle,
    complete,
    loading_pre_cast,
    loading_attack,
    outcomes,
    sliced_death,
    monster_transition
};
struct BattleActionAnimationRequest {
    BattleActionAnimationKind kind = BattleActionAnimationKind::magic;
    std::uint16_t attack = 0;
    bool skip_pre_cast = false;
    bool handoff_outcomes = false;
    bool critical = false;
    bool runic_absorb = false;
    std::bitset<10> reflecting_targets{};
    std::array<std::optional<std::uint8_t>, 10> reflection_targets{};
    std::bitset<10> reflection_hits{};
    // Command callers request the shared return only AFTER their body. Setting
    // it before execution makes jump_step skip the authored forward movement.
    bool command_step_back = false;
    bool lore_pre_cast = false, dance_pre_cast = false, slot_pre_cast = false;
    std::optional<std::uint8_t> covered_target{};
    bool explode_attacker = false;
    bool defer_return = false; // A repeated hit retains its caster until the final outcome.
    std::array<std::optional<std::uint8_t>, 8> projectile_targets{};
    std::bitset<8> projectile_hits{};
    std::optional<std::uint8_t> weapon_death_effect{};
    bool capture = false; // Shared approach/weapon/return, also selected by Thief Knife.
    bool stance_only = false; // Retort arms through the command prelude, without its strike.
    bool monster_entering = false;
    unsigned body_repetitions = 1; // Red Card presents three darts for one resolved attack.
};
const char* battle_action_animation_phase_name(BattleActionAnimationPhase phase);

// A presentation action, not damage resolution or the battle graphics queue.
// Includes character/monster magic, Reflect, Runic, items and physical attacks.
// Explicit image/message-cleanup waits are retained; original conversion work
// overruns, message display and independent damage numerals have separate owners.
class BattleActionAnimation {
  public:
    BattleActionAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                          BattleActionAnimationRequest request, BattleEffectContext context,
                          BattleEffectImageState images = initial_battle_effect_images());
    // One logical presentation boundary, independent of SDL/audio callbacks.
    // The caller owns battle time. This owns actor publication exactly once.
    BattleEffectFrame advance(const BattleActorSceneAssets& assets, BattleActorScene& actors,
                              BattleActorSceneRules& rules,
                              std::optional<BattleEffectRasterTiming> timing = {});
    void publish_suspended(const BattleActorSceneAssets& assets, BattleActorScene& actors,
                           BattleActorSceneRules& rules);
    void update_graphics_random_index(std::uint8_t index) {
        context_.random_index = index;
        if (animation_) animation_->update_graphics_random_index(index);
    }
    // Bounded integration entry for an explicit elapsed picture plan. Only
    // active bodies accept one; loading/cleanup keep their separate owners.
    BattleEffectFrame advance_scheduled(const BattleActorSceneAssets& assets,
                                        BattleActorScene& actors, BattleActorSceneRules& rules,
                                        BattleEffectPictureWork work);
    void compose(RasterScene& scene, std::span<const BattleActorSpriteGroup> actors = {}) const;
    // Resume after the native action-result presenter dispatches its commands.
    // Numerals may continue independently while the caster steps back.
    void resume_return(std::bitset<6> hidden_monsters = {});
    BattleActorPaletteOverrides actor_palettes() const;
    const BattleEffectContext& context() const {
        return context_;
    }
    BattleActionAnimationPhase phase() const {
        return phase_;
    }
    bool complete() const {
        return phase_ == BattleActionAnimationPhase::complete;
    }
    std::uint32_t frames() const {
        return frames_;
    }
    bool loading() const {
        return phase_ == BattleActionAnimationPhase::loading_pre_cast ||
               phase_ == BattleActionAnimationPhase::loading_attack;
    }
    bool resource_wait() const {
        return resource_wait_;
    }
    const auto& raster_events() const {
        return raster_events_;
    }
    const BattleAttackAnimation* body() const {
        return animation_ ? &*animation_ : nullptr;
    }
    std::optional<std::uint32_t> published_sprite_pass() const {
        return visible_sprites_ ? std::optional{visible_sprites_->frames()} : std::nullopt;
    }

  private:
    void publish_picture(const BattleActorSceneAssets&, BattleActorScene&,
                         BattleActorSceneRules&, bool suspended);
    std::optional<BattleMonsterTransition> monster_transition_;
    std::shared_ptr<const BattleEffectLibrary> library_;
    BattleActionAnimationRequest request_;
    BattleEffectContext context_;
    BattleEffectDisplay visible_display_;
    std::vector<std::array<Rgb555, 16>> visible_battlefield_colors_, prepared_battlefield_colors_;
    std::array<std::array<Rgb555, 16>, 4> visible_effect_colors_{};
    BattleActorPaletteOverrides visible_actor_palettes_;
    BattleEffectImageState images_;
    std::optional<BattleAttackAnimation> animation_, prepared_sprites_, visible_sprites_;
    BattleActionAnimationPhase phase_ = BattleActionAnimationPhase::attack;
    std::array<std::uint8_t, 4> return_facing_{};
    unsigned waits_ = 0;
    std::uint16_t pending_definition_ = 0;
    bool resource_wait_ = false;
    bool absorbing_ = false, reflected_ = false, command_prelude_done_ = false;
    bool weapon_death_started_ = false, slot_swirl_ = false, capture_return_ = false;
    bool exploding_self_ = false;
    unsigned completed_bodies_ = 0;
    enum class SliceStage { initialize, visit, copy, fading, clear, finish };
    SliceStage slice_stage_ = SliceStage::initialize;
    unsigned slice_slot_ = 0, slice_age_ = 0;
    std::bitset<6> sliced_targets_;
    std::optional<BattleMonsterBackground> sliced_image_;
    // The copied image and its palette/scroll publish at the next picture.
    // Monster sprite removal uses the ordinary actor preparation path instead.
    std::optional<BattleMonsterBackground> visible_sliced_image_;
    unsigned visible_slice_age_ = 0;
    bool slice_scroll_initialized_ = false, visible_slice_scroll_initialized_ = false;
    std::optional<GameSoundEffect> slice_sound_;
    void advance_sliced_death();
    std::bitset<10> explosion_targets_{}, explosion_hits_{};
    std::array<std::optional<BattleAnimationPoint>, 10> cover_origins_{};
    std::vector<BattleEffectRasterEvent> raster_events_;
    std::uint32_t frames_ = 0;
    bool physical() const;
    void create_body(std::uint16_t definition);
    void start(std::uint16_t definition);
    void begin_return();
    void remember_return_facing();
    void finish_boundary();
    BattleEffectFrame step(const BattleActorSceneAssets& assets, BattleActorScene& actors,
                           BattleActorSceneRules& rules,
                           std::optional<BattleEffectRasterTiming> timing,
                           std::optional<BattleEffectPictureWork> work = {});
};
} // namespace ff6
