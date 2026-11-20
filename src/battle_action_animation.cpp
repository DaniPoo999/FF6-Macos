#include "ff6/battle_action_animation.hpp"
#include "rom-materials/sliced_death_sound.macros.inc"
#include "rom-materials/cover_horizontal_offset.macros.inc"
#include "rom-materials/attack_normal_return_ticks.macros.inc"
#include "rom-materials/attack_armor_return_ticks.macros.inc"
#include "rom-materials/attack_normal_return_step.macros.inc"
#include "rom-materials/attack_armor_return_step.macros.inc"
#include "rom-materials/sliced_death_amplitude.macros.inc"
#include "rom-materials/sliced_death_phase_step.macros.inc"
#include "rom-materials/sliced_death_frames.macros.inc"
#include "rom-materials/sliced_death_band_rows.macros.inc"
#include "rom-materials/battlefield_horizontal_window.macros.inc"
#include "rom-materials/final_battle_viewport.macros.inc"
#include "rom-materials/sliced_death_gray_decrement.macros.inc"
#include "rom-materials/battle_animation_blend_presets.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
// AnimType09 selects this arithmetic. The native scene reverses the additive
// base/operand ownership; that representation is separate from the ROM preset.
constexpr auto sliced_blend = composite_blend(
    (FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_37 & 0x80U) != 0,
    (FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_37 & 0x40U) != 0,
    (FF6_ROM_VALUE_BATTLE_ANIMATION_BLEND_PRESETS_36 & 2U) == 0).covered;
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
void face_character_target(BattleEffectContext& context) {
    auto& caster = context.characters[context.attacker];
    for (unsigned target = 0; target < 10; ++target)
        if (context.targets[target]) {
            if (target < 4)
                break;
            const bool right = (context.anchors[target].center_x & 511U) >=
                               (context.anchors[context.attacker].center_x & 511U);
            if (right != ((caster.facing & 1U) != 0)) {
                caster.facing = right ? 1 : 0;
                caster.appearance.flip_x = !caster.appearance.flip_x;
            }
            break;
        }
    caster.disable_float = true;
}
} // namespace
const char* battle_action_animation_phase_name(BattleActionAnimationPhase phase) {
    switch (phase) {
    case BattleActionAnimationPhase::monster_transition:
        return "Monster transition";
    case BattleActionAnimationPhase::sliced_death:
        return "Slicing dissolve";
    case BattleActionAnimationPhase::outcomes:
        return "Action outcomes";
    case BattleActionAnimationPhase::loading_pre_cast:
        return "Loading pre-cast images";
    case BattleActionAnimationPhase::loading_attack:
        return "Loading attack images";
    case BattleActionAnimationPhase::pre_cast:
        return "Pre-cast";
    case BattleActionAnimationPhase::release_pre_cast:
        return "Pre-cast cleanup";
    case BattleActionAnimationPhase::attack:
        return "Attack";
    case BattleActionAnimationPhase::release_attack:
        return "Attack cleanup";
    case BattleActionAnimationPhase::clear_images:
        return "Clearing effects";
    case BattleActionAnimationPhase::returning:
        return "Return";
    case BattleActionAnimationPhase::settle:
        return "Publishing final pose";
    case BattleActionAnimationPhase::complete:
        return "Sequence finished";
    }
    throw std::invalid_argument("Invalid action animation phase");
}
BattleActionAnimation::BattleActionAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                                             BattleActionAnimationRequest request,
                                             BattleEffectContext context,
                                             BattleEffectImageState images)
    : library_(std::move(library)), request_(request), context_(std::move(context)),
      visible_display_(context_.display), visible_battlefield_colors_(context_.battlefield_colors),
      prepared_battlefield_colors_(context_.battlefield_colors),
      images_(std::move(images)) {
    require(library_ && context_.attacker < 10 && context_.targets.any(),
            "Continuous animation requires an attacker and targets");
    require(request.body_repetitions >= 1 && request.body_repetitions <= 3 &&
                (request.body_repetitions == 1 || request.kind == BattleActionAnimationKind::character_fight),
            "Invalid repeated weapon presentation");
    if (request.kind == BattleActionAnimationKind::monster_transition) {
        monster_transition_.emplace(*library_, request.attack, request.monster_entering, context_);
        phase_ = BattleActionAnimationPhase::monster_transition;
        return;
    }
    if (request.explode_attacker) {
        // C2/3FFC publishes a caster explosion before the selected target
        // picture. The caster's death belongs to the later common outcome.
        exploding_self_ = true;
        explosion_targets_ = context_.targets;
        explosion_hits_ = context_.hits;
        context_.targets.reset();
        context_.targets.set(context_.attacker);
        context_.hits = context_.targets;
    }
    if (request.kind == BattleActionAnimationKind::outcome_only) {
        // The original Mimic graphics entry returns without starting a body.
        // Periodic status damage still owns numerals and the ordinary HP wait.
        // An empty weapon follow-up may also inherit an outstanding step.
        remember_return_facing();
        phase_ = BattleActionAnimationPhase::outcomes;
        return;
    }
    if (request.kind == BattleActionAnimationKind::status_effect ||
        request.kind == BattleActionAnimationKind::escape) {
        start(request.attack);
        return;
    }
    if (request.kind == BattleActionAnimationKind::command_effect) {
        if (context_.attacker < 4) {
            face_character_target(context_);
            remember_return_facing();
        }
        start(request.attack);
        return;
    }
    if (request.lore_pre_cast || request.dance_pre_cast || request.slot_pre_cast) {
        require(context_.attacker < 4, "Command prelude requires a character");
        face_character_target(context_);
        remember_return_facing();
        phase_ = BattleActionAnimationPhase::pre_cast;
        // LoreCmdAnim selects BATTLE_CMD_ANIM::LORE, whose table entry
        // points to definition 287. Definition 303 is Run Away and moves the
        // caster off-screen; it is not the Lore preparation gesture.
        start(request.slot_pre_cast ? 282 : request.dance_pre_cast ? 293 : 287);
        return;
    }
    if (physical()) {
        const bool character = context_.attacker < 4;
        require(character == (request.kind == BattleActionAnimationKind::character_fight ||
                              request.kind == BattleActionAnimationKind::character_special),
                "Physical animation attacker kind mismatch");
        if (request.covered_target) {
            const auto protected_slot = *request.covered_target;
            require(protected_slot < 10 && context_.targets.count() == 1,
                    "Cover requires one protector and a valid protected actor");
            const auto& anchor = context_.anchors[protected_slot];
            const bool right = protected_slot < 4
                ? (context_.characters[protected_slot].facing & 1U) != 0
                : ((context_.monsters[protected_slot - 4].flip ^
                    context_.monster_facing[protected_slot - 4]) & 1U) == 0;
            const auto x = protected_slot < 4 ? context_.characters[protected_slot].x
                                              : anchor.center_x;
            const auto y = protected_slot < 4 ? context_.characters[protected_slot].y
                                              : anchor.bottom_y;
            // InitCoverAnim places the protector at its selected offset toward the
            // incoming attack, then PopObjPos restores it after the impact.
            for (unsigned slot = 0; slot < 10; ++slot) {
                if (!context_.targets[slot]) continue;
                auto& px = slot < 4 ? context_.characters[slot].x : context_.monsters[slot - 4].x;
                auto& py = slot < 4 ? context_.characters[slot].y : context_.monsters[slot - 4].y;
                cover_origins_[slot] = BattleAnimationPoint{px, py};
                px = wrap16(unsigned{x} + (right ? unsigned{FF6_ROM_VALUE_COVER_HORIZONTAL_OFFSET_0} :
                                  65536U - unsigned{FF6_ROM_VALUE_COVER_HORIZONTAL_OFFSET_0}));
                py = y;
                context_.anchors[slot] = anchor;
                context_.anchors[slot].center_x = wrap16(unsigned{anchor.center_x} +
                                                         (right ? unsigned{FF6_ROM_VALUE_COVER_HORIZONTAL_OFFSET_0} :
                                  65536U - unsigned{FF6_ROM_VALUE_COVER_HORIZONTAL_OFFSET_0}));
            }
        }
        if (character) {
            face_character_target(context_);
            // Fight's caller requests the shared return even though it has no
            // casting prelude; the authored weapon script owns forward motion.
            remember_return_facing();
            // C1/BEFF increments the step flag; opcode 88 moves only at one.
            // Preserve its meaning without treating the counter as emulated state.
            if (request.capture) {
                context_.needs_step_back[context_.attacker] = true;
                context_.suppress_fight_step = true;
                phase_ = BattleActionAnimationPhase::pre_cast;
                start(386); // Capture-to saves the original position before approaching.
                return;
            }
            context_.suppress_fight_step = context_.needs_step_back[context_.attacker];
            context_.needs_step_back[context_.attacker] = true;
        }
        start(request.attack);
        return;
    }
    if (request.runic_absorb) {
        absorbing_ = true;
        remember_return_facing();
        phase_ = BattleActionAnimationPhase::pre_cast;
        start(275);
        return;
    }
    if (request.kind == BattleActionAnimationKind::monster_effect) {
        require(context_.attacker >= 4, "Monster effect requires a monster attacker");
        start(request.attack);
        return;
    }
    require(context_.attacker < 4, "Magic sequence requires a character attacker");
    require((context_.hits & ~context_.targets).none(), "Hit participant is not an action target");
    require(request.kind == BattleActionAnimationKind::health  ? request.attack == 302
            : request.kind == BattleActionAnimationKind::rage_magic ? request.attack < 256
            : request.kind == BattleActionAnimationKind::magic
                ? request.attack < 81 || (request.attack >= 240 && request.attack <= 253)
            : request.kind == BattleActionAnimationKind::blitz
                ? request.attack >= 93 && request.attack <= 100
            : request.kind == BattleActionAnimationKind::bushido
                ? request.attack >= 85 && request.attack <= 92
            : request.kind == BattleActionAnimationKind::item
                ? request.attack < library_->definitions.size()
                : request.attack >= 131 && request.attack <= 138,
            "Unsupported continuous character attack presentation");
    // The original caller faces a monster target before saving the direction
    // of the forward step. A changed facing later must not reverse that return.
    face_character_target(context_);
    remember_return_facing();
    if (request.stance_only && request.skip_pre_cast) {
        phase_ = BattleActionAnimationPhase::outcomes;
        return;
    }
    const bool magic_pre =
        (request.kind == BattleActionAnimationKind::magic ||
         request.kind == BattleActionAnimationKind::rage_magic) &&
        request.attack < 81 && !request.skip_pre_cast;
    const bool armor_pre =
        request.kind == BattleActionAnimationKind::magitek && request.attack >= 134;
    // Health steps forward with arms raised, then runs its own body (C1/BB5F).
    const bool item_pre = request.kind == BattleActionAnimationKind::item ||
                          request.kind == BattleActionAnimationKind::health;
    const bool blitz_pre = request.kind == BattleActionAnimationKind::blitz;
    // C1/B98D runs the command gesture once across repeated hits.
    const bool bushido_pre = request.kind == BattleActionAnimationKind::bushido && !request.skip_pre_cast;
    if (magic_pre || armor_pre || item_pre || blitz_pre || bushido_pre) {
        phase_ = BattleActionAnimationPhase::pre_cast;
        start(bushido_pre           ? 290
              : blitz_pre           ? 291
              : item_pre            ? 282
              : armor_pre           ? 256
              : request.attack < 24 ? 284
              : request.attack < 54 ? 285
                                    : 286);
    } else
        start(request.attack);
}
bool BattleActionAnimation::physical() const {
    return request_.kind == BattleActionAnimationKind::monster_fight ||
           request_.kind == BattleActionAnimationKind::monster_special ||
           request_.kind == BattleActionAnimationKind::character_fight ||
           request_.kind == BattleActionAnimationKind::character_special;
}
void BattleActionAnimation::create_body(std::uint16_t definition) {
    context_.reflecting_targets = (phase_ == BattleActionAnimationPhase::attack ||
                                   phase_ == BattleActionAnimationPhase::loading_attack) &&
                                          !reflected_
                                      ? request_.reflecting_targets
                                      : std::bitset<10>{};
    if ((phase_ == BattleActionAnimationPhase::attack ||
         phase_ == BattleActionAnimationPhase::loading_attack) &&
        std::ranges::any_of(request_.projectile_targets, [](auto target) { return target.has_value(); })) {
        // C1/A98F builds one body with independent logical projectile owners.
        // They may repeat a recipient; collapsing to a participant mask loses
        // projectiles and their authored staggering. No reflection block runs.
        context_.projectile_volley = true;
        context_.targets.reset();
        context_.hits.reset();
        context_.reflection_targets = {};
        for (unsigned i = 0; i < request_.projectile_targets.size(); ++i) {
            context_.reflection_targets[i] = request_.projectile_targets[i];
            context_.targets[i] = request_.projectile_targets[i].has_value();
            context_.hits[i] = request_.projectile_hits[i];
        }
    }
    const bool command_prelude = (request_.lore_pre_cast || request_.dance_pre_cast || request_.capture) &&
        (phase_ == BattleActionAnimationPhase::pre_cast || phase_ == BattleActionAnimationPhase::loading_pre_cast);
    if (physical() && !command_prelude && !capture_return_) {
        require(definition < 256, "Invalid physical animation identity");
        animation_.emplace(library_,
                           BattlePhysicalAttackPresentation{
                               static_cast<std::uint8_t>(definition),
                               request_.kind == BattleActionAnimationKind::monster_special ||
                                   request_.kind == BattleActionAnimationKind::character_special,
                               request_.critical},
                           context_, images_);
    } else
        animation_.emplace(library_, definition, context_, images_);
}
void BattleActionAnimation::start(std::uint16_t definition) {
    create_body(definition);
    pending_definition_ = definition;
    waits_ = animation_->loading_waits() + animation_->preparation_waits();
    phase_ = phase_ == BattleActionAnimationPhase::pre_cast
                 ? BattleActionAnimationPhase::loading_pre_cast
                 : BattleActionAnimationPhase::loading_attack;
    // No prepared sprite from the previous program may acquire the new image.
    // The release passes have already drained that preceding publication.
    prepared_sprites_ = animation_;
    visible_sprites_ = animation_;
}
void BattleActionAnimation::remember_return_facing() {
    // MagicFront (C1/9B19) changes the target-facing direction every hit,
    // but preserves the direction of an outstanding forward step. Dragon Horn
    // can face a different target on its next landing; the final return still
    // belongs to the first step, not the latest target.
    for (unsigned i = 0; i < 4; ++i) {
        if (!context_.needs_step_back[i])
            context_.step_back_facing[i] = context_.characters[i].facing;
        return_facing_[i] = context_.step_back_facing[i];
    }
}
void BattleActionAnimation::begin_return() {
    if (request_.capture && !capture_return_) {
        // Capture-from owns the entire retreat. Never append Fight's ordinary
        // eight-pixel return, including across dual-wield/Offering repeats.
        capture_return_ = true;
        phase_ = BattleActionAnimationPhase::attack;
        start(387);
        return;
    }
    if (request_.defer_return ||
        (request_.kind == BattleActionAnimationKind::outcome_only &&
         std::ranges::none_of(context_.needs_step_back, [](bool pending) { return pending; }))) {
        phase_ = BattleActionAnimationPhase::complete;
        return;
    }
    if ((physical() && context_.attacker >= 4) ||
        request_.kind == BattleActionAnimationKind::monster_effect ||
        request_.kind == BattleActionAnimationKind::monster_transition ||
        request_.kind == BattleActionAnimationKind::escape) {
        phase_ = BattleActionAnimationPhase::settle;
        waits_ = 2;
        return;
    }
    phase_ = BattleActionAnimationPhase::returning;
    // Quadra's final script restores the facing saved before its first jump.
    // A later random target may have faced the caster the other way meanwhile.
    if (request_.kind == BattleActionAnimationKind::bushido && context_.saved_attacker_direction)
        return_facing_[context_.attacker] = context_.saved_attacker_direction->first;
    waits_ = context_.magitek ? FF6_ROM_VALUE_ATTACK_ARMOR_RETURN_TICKS_0 : FF6_ROM_VALUE_ATTACK_NORMAL_RETURN_TICKS_0;
    for (unsigned i = 0; i < 4; ++i)
        if (context_.needs_step_back[i] && context_.characters[i].facing == return_facing_[i])
            context_.characters[i].appearance.flip_x = !context_.characters[i].appearance.flip_x;
}
void BattleActionAnimation::resume_return(std::bitset<6> hidden_monsters) {
    require(phase_ == BattleActionAnimationPhase::outcomes, "Action is not awaiting outcomes");
    context_.shown_monsters &= ~hidden_monsters;
    begin_return();
}
void BattleActionAnimation::finish_boundary() {
    if (phase_ == BattleActionAnimationPhase::pre_cast ||
        phase_ == BattleActionAnimationPhase::attack) {
        if (!animation_->complete())
            return;
        images_ = animation_->retained_images();
        const bool pre = phase_ == BattleActionAnimationPhase::pre_cast;
        phase_ = pre ? BattleActionAnimationPhase::release_pre_cast
                     : BattleActionAnimationPhase::release_attack;
        // DeinitAnimType: wait, clear 1216-byte foreground (two waits), clear
        // 1024-byte overlay (one), wait. Native resources need no DMA model.
        waits_ = physical() && !pre ? 1 : 5;
        return;
    }
    if (waits_ && --waits_)
        return;
    switch (phase_) {
    case BattleActionAnimationPhase::loading_pre_cast:
    case BattleActionAnimationPhase::loading_attack:
        // Actor images, anchors and working colors have continued to advance
        // during the load. Initialize from that live state, not the preflight.
        create_body(pending_definition_);
        context_ = animation_->context();
        prepared_sprites_ = visible_sprites_ = animation_;
        phase_ = phase_ == BattleActionAnimationPhase::loading_pre_cast
                     ? BattleActionAnimationPhase::pre_cast
                     : BattleActionAnimationPhase::attack;
        break;
    case BattleActionAnimationPhase::release_pre_cast:
        if (request_.stance_only) {
            context_.needs_step_back[context_.attacker] = true;
            phase_ = BattleActionAnimationPhase::outcomes;
            break;
        }
        if ((request_.lore_pre_cast || request_.dance_pre_cast) && !command_prelude_done_) {
            command_prelude_done_ = true;
            // Lore and Dance enter MagicCmdAnim after their command gesture.
            context_.needs_step_back[context_.attacker] = true;
            if (request_.runic_absorb) {
                absorbing_ = true;
                phase_ = BattleActionAnimationPhase::pre_cast;
                start(275);
                break;
            }
            if (!physical() && request_.attack < 81 && !request_.skip_pre_cast) {
                phase_ = BattleActionAnimationPhase::pre_cast;
                start(request_.attack < 24 ? 284 : request_.attack < 54 ? 285 : 286);
                break;
            }
        }
        if (absorbing_) {
            absorbing_ = false;
            if (context_.attacker < 4) {
                face_character_target(context_);
                remember_return_facing();
            }
            if ((request_.kind == BattleActionAnimationKind::magic ||
                 request_.kind == BattleActionAnimationKind::rage_magic) &&
                request_.attack < 81 && !request_.skip_pre_cast) {
                phase_ = BattleActionAnimationPhase::pre_cast;
                start(request_.attack < 24 ? 284 : request_.attack < 54 ? 285 : 286);
                break;
            }
        }
        if (request_.slot_pre_cast && !slot_swirl_ && request_.attack < 81) {
            // C1/B900 steps first, then MagicCmdAnim runs the summon prelude.
            // Retain step ownership so the second prelude cannot move twice.
            slot_swirl_ = true;
            context_.needs_step_back[context_.attacker] = true;
            phase_ = BattleActionAnimationPhase::pre_cast;
            start(request_.attack < 24 ? 284 : request_.attack < 54 ? 285 : 286);
            break;
        }
        if (request_.lore_pre_cast || request_.dance_pre_cast || request_.kind == BattleActionAnimationKind::magic ||
            request_.kind == BattleActionAnimationKind::rage_magic ||
            request_.kind == BattleActionAnimationKind::item ||
            request_.kind == BattleActionAnimationKind::bushido ||
            request_.kind == BattleActionAnimationKind::blitz ||
            request_.kind == BattleActionAnimationKind::health)
            context_.needs_step_back[context_.attacker] = true;
        phase_ = BattleActionAnimationPhase::attack;
        start(request_.attack);
        break;
    case BattleActionAnimationPhase::release_attack:
        if (++completed_bodies_ < request_.body_repetitions) {
            context_.suppress_fight_step = true;
            phase_ = BattleActionAnimationPhase::attack;
            start(request_.attack);
            break;
        }
        if (capture_return_) {
            context_.needs_step_back[context_.attacker] = false;
            context_.characters[context_.attacker].disable_float = false;
            phase_ = BattleActionAnimationPhase::settle;
            waits_ = 2;
            break;
        }
        if (exploding_self_) {
            exploding_self_ = false;
            context_.targets = explosion_targets_;
            context_.hits = explosion_hits_;
            phase_ = BattleActionAnimationPhase::attack;
            start(request_.attack);
            break;
        }
        if (!reflected_ && std::ranges::any_of(request_.reflection_targets,
                                               [](auto source) { return source.has_value(); })) {
            reflected_ = true;
            context_.targets.reset();
            context_.reflecting_targets.reset();
            context_.hits = request_.reflection_hits;
            context_.reflection_targets = request_.reflection_targets;
            for (unsigned i = 0; i < 10; ++i)
                context_.targets[i] = request_.reflection_targets[i].has_value();
            phase_ = BattleActionAnimationPhase::attack;
            start(request_.attack);
            break;
        }
        if (request_.weapon_death_effect && !weapon_death_started_) {
            weapon_death_started_ = true;
            request_.attack = *request_.weapon_death_effect;
            request_.kind = BattleActionAnimationKind::command_effect;
            phase_ = BattleActionAnimationPhase::attack;
            start(request_.attack);
            break;
        }
        // DeinitAnimType invokes this shared ending for Cleave, Odin and
        // Raiden. It precedes ordinary outcome dispatch and hides each sliced
        // monster, so the later generic death presenter must not fade it again.
        // Fight uses a weapon-property identity, not the spell-definition table.
        if (animation_ && !physical()) {
            const auto kind = library_->definitions[request_.attack].initialization;
            if (kind == 0x0b || kind == 0x45 || kind == 0x6b) {
                for (unsigned i = 0; i < 6; ++i)
                    sliced_targets_[i] = context_.hits[i + 4] && context_.shown_monsters[i];
                if (sliced_targets_.any()) {
                    phase_ = BattleActionAnimationPhase::sliced_death;
                    slice_stage_ = SliceStage::initialize;
                    waits_ = 1;
                    animation_.reset(); prepared_sprites_.reset(); visible_sprites_.reset();
                    break;
                }
            }
        }
        phase_ = BattleActionAnimationPhase::clear_images;
        waits_ = physical() ? 1 : 4; // MagicCmdAnim clears two 2048-byte frame regions.
        break;
    case BattleActionAnimationPhase::sliced_death:
        advance_sliced_death();
        break;
    case BattleActionAnimationPhase::clear_images:
        for (unsigned slot = 0; slot < cover_origins_.size(); ++slot)
            if (const auto origin = cover_origins_[slot]) {
                if (slot < 4) {
                    context_.characters[slot].x = origin->x;
                    context_.characters[slot].y = origin->y;
                } else {
                    context_.monsters[slot - 4].x = origin->x;
                    context_.monsters[slot - 4].y = origin->y;
                }
                cover_origins_[slot].reset();
            }
        // ToolsCmdAnim and C1/BBE1 Runic request the return after ExecAnim.
        // Steal instead returns inside its vector-jump script (C1/BB87).
        if (request_.kind == BattleActionAnimationKind::command_effect &&
            request_.command_step_back && context_.attacker < 4)
            context_.needs_step_back[context_.attacker] = true;
        if (request_.kind == BattleActionAnimationKind::magitek && request_.attack >= 134)
            context_.needs_step_back[context_.attacker] = true;
        if (request_.handoff_outcomes)
            phase_ = BattleActionAnimationPhase::outcomes;
        else
            begin_return();
        break;
    case BattleActionAnimationPhase::returning:
        for (unsigned i = 0; i < 4; ++i) {
            auto& pose = context_.characters[i];
            if (context_.needs_step_back[i]) {
                if (pose.facing == return_facing_[i])
                    pose.appearance.flip_x = !pose.appearance.flip_x;
                pose.action = 0;
                context_.control.armor_actions[i] = 0;
            }
            pose.disable_float = false;
        }
        context_.needs_step_back.fill(false);
        phase_ = BattleActionAnimationPhase::settle;
        // The standalone presentation caller drains actor preparation and then
        // publication. These two ticks are not extra original graphics commands.
        waits_ = 2;
        break;
    case BattleActionAnimationPhase::settle:
        phase_ = BattleActionAnimationPhase::complete;
        break;
    default:
        break;
    }
}
BattleEffectFrame BattleActionAnimation::step(const BattleActorSceneAssets& assets,
                                              BattleActorScene& actors,
                                              BattleActorSceneRules& rules,
                                              std::optional<BattleEffectRasterTiming> timing,
                                              std::optional<BattleEffectPictureWork> work) {
    require((!timing && !work) || phase_ == BattleActionAnimationPhase::pre_cast ||
                phase_ == BattleActionAnimationPhase::attack,
            "Raster deadlines require an active animation body");
    require(!timing || !work, "Cannot mix row deadlines and elapsed picture work");
    if (complete())
        return {{}, true, {}};
    // Screen brightness and mosaic publish before this boundary executes tasks.
    visible_display_ = context_.display;
    // OdinDeath's palette writes and post-battlefield scroll changes affect
    // the next picture. Its sprite mask is sampled by UpdateSprites and then
    // published one boundary later by the common actor pipeline below.
    visible_sliced_image_ = (slice_stage_ == SliceStage::fading || slice_stage_ == SliceStage::copy)
                               ? sliced_image_ : std::nullopt;
    // TfrBG1Tiles publishes the first sixteen rows, then the remaining three.
    // A preceding target leaves a complementary one-pixel band offset, so an
    // edge of this copied image can show before the ordinary sprite is hidden.
    if (visible_sliced_image_ && slice_stage_ == SliceStage::copy && waits_ == 3)
        for (unsigned cell = 16 * 32; cell < 19 * 32; ++cell)
            visible_sliced_image_->image.cells[cell].tile =
                static_cast<std::uint32_t>(visible_sliced_image_->image.tiles.size() - 1);
    visible_slice_age_ = slice_age_;
    visible_slice_scroll_initialized_ = slice_scroll_initialized_;
    // BattleNMI first publishes the preceding UpdateBattleBG snapshot; that
    // routine copies working terrain colors only at the end of the update.
    // Script writes therefore pass through both preparation and publication.
    // Effect/actor palettes below have a direct publication owner instead.
    visible_battlefield_colors_ = prepared_battlefield_colors_;
    prepared_battlefield_colors_ = context_.battlefield_colors;
    // BattleNMI publishes the previous description, prepares actors/echoes
    // from the completed script state, and only then lets ExecAnimScript
    // resume. Preparing after the new commands samples the next position too
    // early and publishes actor palette flashes ahead of terrain colors.
    apply_battle_effect_actor_state(context_, actors, rules);
    publish_battle_actor_scene(actors, assets, rules.hide_characters);
    visible_sprites_ = prepared_sprites_;
    if (animation_) {
        for (unsigned i = 0; i < visible_effect_colors_.size(); ++i)
            visible_effect_colors_[i] = i == 3 ? animation_->secondary_foreground_colors()
                : animation_->effect_colors(static_cast<BattleEffectLayer>(i));
        // A target's temporary palette selection travels with its sprite
        // description, while the selected palette's colors publish separately.
        visible_actor_palettes_ = animation_->actor_palettes(
            visible_sprites_ ? &*visible_sprites_ : nullptr);
    } else visible_actor_palettes_ = {};
    prepare_battle_actor_scene(assets, rules, actors);
    prepared_sprites_ = animation_;
    update_battle_character_status(assets.graphics, rules.appearance, actors.characters);
    if (!actors.animation.fixed_draw_order)
        stage_battle_draw_order(actors.order, actors.anchors);
    context_.presentation_tick = actors.clock;
    context_.character_colors = battle_character_colors(actors, assets);
    for (unsigned i = 0; i < 4; ++i)
        context_.shown_character_status[i] = actors.characters.control[i].shown_status;
    context_.monster_colors = battle_monster_colors(actors, assets);
    context_.published_character_frames = actors.publication.character_frames;
    context_.published_status_tiles = actors.publication.status_tiles;
    context_.anchors = actors.anchors;
    context_.draw_order = actors.order.staged;
    context_.control.force_character_frame = actors.animation.force_character_frame;
    BattleEffectFrame result;
    resource_wait_ = false;
    raster_events_.clear();
    if (phase_ == BattleActionAnimationPhase::attack && !animation_)
        start(request_.attack);
    if (phase_ == BattleActionAnimationPhase::pre_cast ||
        phase_ == BattleActionAnimationPhase::attack) {
        animation_->update_character_frames(*context_.published_character_frames);
        if (context_.published_status_tiles)
            animation_->update_status_tiles(*context_.published_status_tiles);
        animation_->update_forced_character_frame(context_.control.force_character_frame);
        animation_->update_character_colors(context_.character_colors);
        animation_->update_monster_colors(context_.monster_colors);
        animation_->update_draw_order(context_.draw_order);
        animation_->update_anchors(context_.anchors);
        animation_->update_presentation_tick(context_.presentation_tick);
        result = work ? animation_->advance_scheduled(std::move(*work))
                      : animation_->advance_presentation(16384, std::move(timing));
        // Isolated effect callers advance their own clock. The continuous
        // caller has already advanced the shared actor clock at preparation.
        animation_->update_presentation_tick(actors.clock);
        resource_wait_ = result.resource_wait;
        raster_events_ = animation_->raster_events();
        context_ = animation_->context();
    }
    apply_battle_effect_actor_state(context_, actors, rules);
    apply_battle_actor_appearance_changes(assets, rules, result.appearance_changes, actors);
    if (phase_ == BattleActionAnimationPhase::returning) {
        // STEP_BACK changes positions AFTER its wait, unlike body commands.
        for (unsigned i = 0; i < 4; ++i)
            if (context_.needs_step_back[i]) {
                auto& pose = context_.characters[i];
                const int step = (return_facing_[i] ? -1 : 1) * (context_.magitek ? int{FF6_ROM_VALUE_ATTACK_ARMOR_RETURN_STEP_0}
                                                                        : int{FF6_ROM_VALUE_ATTACK_NORMAL_RETURN_STEP_0});
                pose.animation_x = wrap16(static_cast<std::uint32_t>(int{pose.animation_x} + step));
                pose.frame_override = 0;
                pose.action = 4;
                if (context_.magitek)
                    context_.control.armor_actions[i] = 1;
            }
    }
    // Ordinary actor preparation owns the next palette round-robin update and
    // consumes forced-image requests. Do not replay either at a phase handoff.
    context_.character_colors = battle_character_colors(actors, assets);
    for (unsigned i = 0; i < 4; ++i)
        context_.shown_character_status[i] = actors.characters.control[i].shown_status;
    context_.monster_colors = battle_monster_colors(actors, assets);
    context_.control.force_character_frame = actors.animation.force_character_frame;
    context_.anchors = actors.anchors;
    context_.published_character_frames = actors.publication.character_frames;
    context_.published_status_tiles = actors.publication.status_tiles;
    if (phase_ == BattleActionAnimationPhase::monster_transition) {
        result.volume = monster_transition_->advance(context_);
        if (monster_transition_->complete()) {
            if (request_.handoff_outcomes) phase_ = BattleActionAnimationPhase::outcomes;
            else begin_return();
        }
    } else finish_boundary();
    if (slice_sound_) { result.sound = slice_sound_; slice_sound_.reset(); }
    apply_battle_effect_actor_state(context_, actors, rules);
    ++frames_;
    result.complete = complete();
    return result;
}
BattleEffectFrame BattleActionAnimation::advance(const BattleActorSceneAssets& assets,
                                                 BattleActorScene& actors,
                                                 BattleActorSceneRules& rules,
                                                 std::optional<BattleEffectRasterTiming> timing) {
    auto next = *this;
    auto next_actors = actors;
    auto next_rules = rules;
    const auto frame = next.step(assets, next_actors, next_rules, std::move(timing));
    *this = std::move(next);
    actors = std::move(next_actors);
    rules = next_rules;
    return frame;
}
BattleEffectFrame BattleActionAnimation::advance_scheduled(const BattleActorSceneAssets& assets,
                                                           BattleActorScene& actors,
                                                           BattleActorSceneRules& rules,
                                                           BattleEffectPictureWork work) {
    auto next = *this;
    auto next_actors = actors;
    auto next_rules = rules;
    const auto frame = next.step(assets, next_actors, next_rules, {}, std::move(work));
    *this = std::move(next);
    actors = std::move(next_actors);
    rules = next_rules;
    return frame;
}
BattleActorPaletteOverrides BattleActionAnimation::actor_palettes() const {
    return animation_ && !loading() ? visible_actor_palettes_ : BattleActorPaletteOverrides{};
}
void BattleActionAnimation::compose(RasterScene& scene,
                                    std::span<const BattleActorSpriteGroup> actors) const {
    if (animation_ && !loading())
        animation_->compose(scene, actors, visible_sprites_ ? &*visible_sprites_ : nullptr,
                            &visible_display_, visible_battlefield_colors_, visible_effect_colors_);
    else {
        apply_battle_effect_display(visible_display_, scene);
        if (!visible_battlefield_colors_.empty()) {
            require(!scene.layers.empty() &&
                        scene.layers.front().palettes.size() == visible_battlefield_colors_.size(),
                    "Published battlefield palette has the wrong extent");
            scene.layers.front().palettes = visible_battlefield_colors_;
        }
        if (!scene.objects) {
            scene.objects.emplace();
            scene.objects->lines.resize(scene.height);
        }
        for (const auto& group : actors) {
            for (const auto& sprite : group.sprites) {
                auto image = sprite.image;
                if (visible_sliced_image_ && sprite.color_math) image.mix = {sliced_blend, ColorBlend::none};
                scene.objects->sprites.push_back(std::move(image));
            }
        }
        if (monster_transition_) monster_transition_->compose(scene);
        if (visible_sliced_image_) {
            auto layer = visible_sliced_image_->image;
            layer.visible = false;
            layer.operand = true;
            layer.lines.resize(scene.height);
            const auto shift = scale_battle_effect_sine(*library_,
                static_cast<std::uint8_t>((visible_slice_age_ ? visible_slice_age_ - 1 : 0) * FF6_ROM_VALUE_SLICED_DEATH_PHASE_STEP_0),
                FF6_ROM_VALUE_SLICED_DEATH_AMPLITUDE_0);
            for (unsigned y = 0; y < scene.height; ++y) {
                auto& line = layer.lines[y];
                line.enabled = y >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 &&
                    y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1);
                line.left = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0;
                line.right = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1;
                // UpdateOdinDeathHDMA complements the signed word for the
                // second band. Even at zero displacement it is -1, not zero.
                line.offset_x = !visible_slice_scroll_initialized_ ? 0
                                    : (y % (2U * FF6_ROM_VALUE_SLICED_DEATH_BAND_ROWS_0) < FF6_ROM_VALUE_SLICED_DEATH_BAND_ROWS_0) ? shift : -shift - 1;
            }
            for (auto& bank : layer.palettes)
                for (auto& color : bank)
                    color = blend_color(color, static_cast<Rgb555>(unsigned{FF6_ROM_VALUE_SLICED_DEATH_GRAY_DECREMENT_0} * std::min(31U, visible_slice_age_)), ColorBlend::subtract);
            scene.layers.front().mix = {sliced_blend, ColorBlend::none};
            scene.layers.push_back(std::move(layer));
        }
    }
}

void BattleActionAnimation::advance_sliced_death() {
    // C1/AF57: ascending slots, one visit wait even for an empty slot, two
    // image publication waits, 32 sine/fade pictures, then two clear waits.
    if (slice_stage_ == SliceStage::initialize) {
        slice_slot_ = 0; slice_stage_ = SliceStage::visit; waits_ = 1;
    } else if (slice_stage_ == SliceStage::visit) {
        if (sliced_targets_[slice_slot_]) {
            require(context_.monster_images != nullptr, "Slicing dissolve needs monster images");
            BattleMonsterBackgroundRequest copy;
            copy.selected[slice_slot_] = true;
            copy.shown = copy.displayed = copy.visible = context_.shown_monsters;
            copy.positions = context_.monsters;
            copy.widths = context_.monster_widths; copy.heights = context_.monster_heights;
            copy.facing = context_.monster_facing; copy.colors = context_.monster_colors;
            for (unsigned i = 0; i < 6; ++i) copy.palette_slots[i] = context_.monster_styles[i].palette;
            sliced_image_ = copy_battle_monsters_to_background(*context_.monster_images, copy);
            slice_age_ = 0;
            slice_stage_ = SliceStage::copy; waits_ = 3;
        } else {
            ++slice_slot_;
            slice_stage_ = slice_slot_ == 6 ? SliceStage::finish : SliceStage::visit;
            waits_ = slice_slot_ == 6 ? 3 : 1;
        }
    } else if (slice_stage_ == SliceStage::copy) {
        context_.control.monster_visible[slice_slot_] = false;
        slice_sound_ = GameSoundEffect{FF6_ROM_VALUE_SLICED_DEATH_SOUND_0, 128};
        slice_stage_ = SliceStage::fading; slice_age_ = 0; waits_ = 1;
    } else if (slice_stage_ == SliceStage::fading) {
        slice_scroll_initialized_ = true;
        if (++slice_age_ == FF6_ROM_VALUE_SLICED_DEATH_FRAMES_0) {
            context_.shown_monsters.reset(slice_slot_);
            context_.control.monster_visible[slice_slot_] = false;
            sliced_image_.reset(); slice_stage_ = SliceStage::clear; waits_ = 2;
        } else waits_ = 1;
    } else if (slice_stage_ == SliceStage::clear) {
        ++slice_slot_;
        slice_stage_ = slice_slot_ == 6 ? SliceStage::finish : SliceStage::visit;
        waits_ = slice_slot_ == 6 ? 3 : 1;
    } else {
        phase_ = BattleActionAnimationPhase::clear_images; waits_ = 4;
    }
}
} // namespace ff6
