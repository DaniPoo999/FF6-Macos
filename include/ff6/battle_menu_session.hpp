#pragma once
#include "ff6/battle_pause.hpp"
#include "rom-materials/battle_row_movement_parameters.macros.inc"
#include "rom-materials/battle_defeat_message_waits.macros.inc"
#include "rom-materials/battle_slot_fade_steps.macros.inc"
#include "ff6/battle_dance.hpp"
#include "ff6/battle_final.hpp"
#include "ff6/battle_action_animation.hpp"
#include "ff6/battle_blitz.hpp"
#include "ff6/battle_slot.hpp"
#include "ff6/battle_command_selection.hpp"
#include "ff6/battle_cursors.hpp"
#include "ff6/battle_damage_numbers.hpp"
#include "ff6/battle_entrance.hpp"
#include "ff6/battle_event.hpp"
#include "ff6/battle_execution.hpp"
#include "ff6/battle_magitek.hpp"
#include "ff6/battle_menu_surfaces.hpp"
#include "ff6/battle_monster_death.hpp"
#include "ff6/battle_physical.hpp"
#include "ff6/battle_victory.hpp"
#include <deque>

namespace ff6 {
struct BattleMenuSessionAssets {
    BattleMenuSurfaceAssets surfaces;
    BattleMenuTransitionAssets transitions;
    BattleCursorAssets cursors;
    std::array<std::uint8_t, 256> random{};
    std::array<bool, 256> magic_supported{};
    std::array<bool, 256> items_supported{};
    BattleBlitzCodes blitz{};
    BattleSlotData slot{};
    std::array<bool, 256> tools_supported{};
    std::array<bool, 256> fixed_equipment{};
};
BattleMenuSessionAssets load_battle_menu_session_assets(const ContentPack& pack);
enum class BattleMenuPhase {
    idle,
    wait,
    open_command,
    command,
    prepare_magitek,
    magitek,
    target,
    close_list,
    close_command,
    finish,
    stance,
    prepare_choices,
    choices,
    blitz,
    bushido,
    esper,
    slot,
    close_slot,
    equipment
};
struct BattleMenuContext {
    std::bitset<4> enemy_character_proxy;
    BattleSceneAnchors anchors;
    BattleMonsterPositions positions;
    BattleCharacterViewData character_view;
    std::array<BattleCharacterPose, 4> poses;
    std::array<std::uint32_t, 4> shown_status{};
    std::array<std::uint8_t, 4> graphics{255, 255, 255, 255};
    std::array<std::optional<std::uint8_t>, 4> text_rows;
    BattleTargetEligibility eligible, visible;
    std::uint8_t clock = 0;
    bool magitek = false, hide_character_indicator = false, joker_doom = true;
};
// Build the displayed enemy-character alias without changing combat identity.
void prepare_battle_menu_targets(BattleMenuContext& context, std::bitset<4> proxy,
                                BattleApproach approach);
// One native frame owner for the command/Magitek/target menu slice. Prepared
// canvases, published canvases, window tasks and input tasks are independent.
// The caller services battle time AFTER advance(), then supplies its requests
// on the next frame. No attack execution is performed here.
class BattleMenuSession {
  public:
    BattleMenuSession(BattleMenuSessionAssets assets, const BattleReadoutView& readout,
                      GameMenuState options = {}, InputState input = {});
    std::vector<MusicPlayerCommand> advance(BattleState& battle, const BattleMenuContext& context,
                                            Buttons first, Buttons second = 0,
                                            std::optional<std::uint8_t> display_clock = {});
    void advance_suspended(Buttons first, Buttons second, std::uint8_t display_clock);
    std::vector<TileLayer> layers() const;
    BattleCursorSprites cursor_sprites() const;
    std::optional<RasterSprite> roulette_sprite(std::uint8_t target,
        const BattleSceneAnchors& anchors, const BattleMonsterPositions& positions) const {
        return make_battle_roulette_sprite(target, anchors, positions, assets_.cursors);
    }
    void update_counters(const BattleReadoutView& readout);
    // Publish outcome HP independently of active command/list windows.
    void publish_action_readout(const BattleReadoutView& readout);
    void acknowledge_command();
    void close_for_victory();
    std::uint8_t graphics_random_index() const {
        return graphics_random_;
    }
    void set_graphics_random_index(std::uint8_t value) {
        graphics_random_ = value;
    }
    BattleMenuPhase phase() const {
        return phase_;
    }
    std::optional<std::uint8_t> active_character() const {
        return active_;
    }
    bool wait_for_menu() const {
        return wait_for_menu_;
    }
    bool escape_held() const {
        return held(input_.held, Button::l) && held(input_.held, Button::r);
    }
    bool complete() const {
        return confirmed_ && !active_;
    }
    const std::optional<BattlePlayerCommand>& confirmed() const {
        return confirmed_;
    }
    const std::string& notice() const {
        return notice_;
    }
    const BattleMenuOrder& order() const {
        return order_;
    }
    const BattleMenuTransition& transition() const {
        return transition_;
    }
    const BattleCursorPublication& cursors() const {
        return cursors_;
    }
    const InputState& input() const {
        return input_;
    }
    const BattleTargetCursor& targets() const {
        return targets_;
    }
    const BattleSlotState& slot_reels() const { return slot_; }
    BattleListCursor list_cursor() const;
    unsigned list_scroll() const {
        return active_ ? (choice_kind_ == 2 ? magic_scroll_[*active_] :
                          choice_kind_ == 12 ? lore_scroll_[*active_] :
                          choice_kind_ == 8 ? throw_scroll_[*active_] :
                          choice_kind_ == 16 ? rage_scroll_[*active_] :
                          choice_kind_ == 19 ? dance_scroll_[*active_] : item_scroll_[*active_]) : 0;
    }
    std::uint8_t list_command() const {
        return choice_kind_;
    }
    std::uint8_t command_cursor() const {
        return active_ ? commands_[*active_] : 0;
    }
    unsigned text_publications_left() const {
        return text_rows_left_;
    }
    const BattleDisplayTransfers& transfers() const {
        return transfers_;
    }

  private:
    enum class WindowTask {
        style,
        open_command,
        wait_open,
        auxiliary,
        open_list,
        open_slot,
        restore_slot,
        open_stance,
        close_stance,
        open_esper,
        close_esper,
        open_equipment,
        close_equipment,
        open_bushido,
        close_bushido,
        close_list,
        close_command,
        wait_close,
        reset_style,
        finish
    };
    BattleMenuSessionAssets assets_;
    BattleMenuSurfaces work_, published_;
    BattleMenuTransition transition_, visible_transition_;
    BattleCursorPublication cursors_;
    GameMenuState options_;
    InputState input_;
    InputSettings input_settings_;
    BattleMenuOrder order_;
    std::optional<std::uint8_t> active_;
    BattleCommandLayout layout_ = BattleCommandLayout::window;
    std::array<std::uint8_t, 4> commands_{};
    std::array<BattleListCursor, 4> lists_{}, magic_cursors_{}, item_cursors_{}, rage_cursors_{};
    std::array<std::uint8_t, 4> magic_scroll_{}, item_scroll_{}, rage_scroll_{};
    std::array<BattleListCursor, 4> throw_cursors_{}, lore_cursors_{}, dance_cursors_{};
    std::array<std::uint8_t, 4> throw_scroll_{}, lore_scroll_{}, dance_scroll_{};
    std::uint8_t choice_kind_ = 29;
    std::vector<BattleItemStack> tool_list_;
    std::optional<unsigned> selected_item_;
    std::optional<unsigned> selected_hand_;
    unsigned equipment_cursor_ = 0;
    BattleItemSource item_source_ = BattleItemSource::inventory;
    void equipment_input(BattleState&, const BattleMenuContext&);
    void refresh_equipment_text(const BattleState&);
    bool reopening_choices_ = false;
    void stage_choices(const BattleState& battle);
    void choice_input(BattleState& battle, const BattleMenuContext& context);
    BattleMenuPhase phase_ = BattleMenuPhase::idle, after_wait_ = BattleMenuPhase::idle,
                    target_return_ = BattleMenuPhase::command;
    std::deque<WindowTask> windows_;
    BattleMenuCycle cycle_ = BattleMenuCycle::consumed;
    BattleTargetCursor targets_;
    BattleBlitzInput blitz_;
    BattleSlotState slot_;
    unsigned slot_fade_ = FF6_ROM_VALUE_BATTLE_SLOT_FADE_STEPS_0;
    std::vector<BattleTextBinding> slot_previous_list_;
    std::optional<BattleCommandDraft> draft_;
    std::optional<BattlePlayerCommand> confirmed_;
    std::optional<BattleHandCursor> hand_;
    std::uint8_t attack_ = 0, graphics_random_ = 0, list_row_ = 0;
    BattleTextCanvas text_destination_ = BattleTextCanvas::readout;
    unsigned text_rows_left_ = 0, style_rows_left_ = 0;
    std::optional<std::uint8_t> pending_list_row_;
    BattleMenuSounds sounds_;
    BattleDisplayTransfers transfers_;
    bool defend_choice_ = false;
    std::uint8_t bushido_charge_ = 0;
    bool active_sound_ = false, indicator_ = false, closing_ = false, fast_window_ = false,
         wait_for_menu_ = false;
    std::string notice_;
    void publish_readout_region(const TileLayer& fresh, BattleReadoutRectangle region);
    void publish(std::uint8_t clock, bool active = true);
    void sample_buttons(Buttons first, Buttons second);
    void window_step(const BattleState& battle, const BattleMenuContext& context);
    void input_step(BattleState& battle, const BattleMenuContext& context);
    void wait(BattleMenuPhase next);
    void begin_target(std::uint8_t policy, const BattleMenuContext& context);
    void stage_magitek(const BattleMenuContext& context);
    void add_sounds(BattleMenuSounds sounds);
    BattleCommandList command_list(const BattleState& battle) const;
    void start_text(BattleTextCanvas destination);
};
// Owns the continuing battle presentation and command menus. connect_combat
// enables shared action arbitration, enemy turns and victory; the default is
// retained only for isolated presentation studies.
struct BattleChangeRequest { std::uint16_t formation = 0; std::uint8_t transition = 0; bool restore = false; };
class BattleMenuPreview {
  public:
    BattleMenuPreview(const ContentPack& pack, const BattleEntrance& entrance,
                      GameMenuState options, InputState input);
    std::vector<MusicPlayerCommand> advance(BattleState& battle, const BattleParty& party,
                                            Buttons buttons, Buttons second = 0, bool audio_ready = true);
    BattlePauseDecision pause_decision(bool audio_ready = true) const;
    bool paused() const { return paused_; }
    void connect_combat(const BattleFormationEntry& formation,
                        const BattlePreparation& preparation,
                        const std::array<std::uint8_t, 20>& global_variables = {},
                        bool continue_field_music = false, bool gau_recruited = false);
    std::array<std::uint8_t, 20> global_script_variables() const;
    std::uint8_t cursed_shield_battles() const;
    const std::optional<BattleChangeRequest>& battle_change() const { return battle_change_; }
    void replace_monsters(const BattleEntrance&, const BattleFormationEntry&,
                          const BattlePreparation&, BattleState&);
    void begin_victory(BattleRewards rewards);
    std::optional<std::uint8_t> scripted_event_identity() const {
        return event_ ? std::optional{event_->identity()} : std::nullopt;
    }
    bool scripted_event_active() const {
        return event_.has_value();
    }
    bool victory_complete() const {
        return victory_ && victory_->complete();
    }
    BattleTierPicture final_tier_picture() const { return ff6::final_tier_picture(assets_, seed_); }
    unsigned completed_actions() const {
        return completed_actions_;
    }
    bool escape_complete() const {
        return escape_ready_;
    }
    bool defeat_complete() const {
        return defeat_ && defeat_message_started_ && !message_;
    }
    bool ending() const { return victory_ready_ || escape_ready_ || defeat_ || leap_pending_; }
    bool escape_held() const { return menus_.escape_held() && !seed_.actors.animation.run_animation_blocked &&
                                      !event_ && !ending(); }
    bool victory_ready() const {
        return victory_ready_;
    }
    const BattleMenuSession& menus() const {
        return menus_;
    }
    const RasterScene& visible() const {
        return visible_;
    }
    const BattleActorScene& actor_scene() const { return seed_.actors; }
    const BattleActorSceneRules& actor_rules() const { return seed_.rules; }
    std::uint8_t background_identity() const { return assets_.background.identity; }
    std::bitset<4> enemy_character_proxy() const { return seed_.enemy_character_proxy; }
    std::uint8_t clock() const {
        return wrap8(seed_.actors.clock);
    }
    const std::optional<BattleAttackResult>& action_result() const {
        return action_result_;
    }
    const std::string& animation_notice() const {
        return animation_notice_;
    }
    const BattleDamageNumbers* damage_numbers() const {
        return numbers_ ? &*numbers_ : nullptr;
    }
    const BattleMonsterDeath* monster_death() const {
        return death_ ? &*death_ : nullptr;
    }
    bool animation_active() const {
        return row_motion_.has_value() || (animation_ && !animation_->complete()) ||
               (numbers_ && !numbers_->complete());
    }
    bool leap_complete() const { return leap_complete_; }
    bool action_complete() const {
        return action_result_ && !animation_active() && result_frames_ >= 8;
    }
    bool command_execution_active() const {
        return execution_context_.executing.has_value();
    }

  private:
    struct RowMotion {
        std::uint8_t character;
        int increment;
        unsigned waits = FF6_ROM_VALUE_BATTLE_ROW_MOVEMENT_PARAMETERS_0;
    };
    std::optional<RowMotion> row_motion_;
    void advance_background();
    void compose_picture();
    BattleEntranceAssets assets_;
    BattleEntranceSeed seed_;
    BattleGaugePublication gauges_;
    std::optional<std::bitset<4>> menu_characters_;
    std::bitset<4> event_arrivals_, event_departures_;
    BattleMenuSession menus_;
    bool joker_doom_ = true;
    BattleMagitekData magitek_;
    BattleMagicData magic_;
    BattleDanceData dances_;
    std::array<BattleBackground, 8> dance_backgrounds_;
    struct DanceTransition { unsigned age = 0; std::uint8_t dance = 0; };
    std::optional<DanceTransition> dance_transition_;
    MonsterStatsData rage_monsters_;
    BattleItemEffectData items_;
    std::bitset<6> background_monsters_;
    std::shared_ptr<const BattleEventAssets> event_assets_;
    std::optional<BattleScriptedEvent> event_;
    BattleEffectDisplay display_; // Survives task cleanup and scripted battle exit.
    BattleEffectContext effect_context(const BattleState& battle) const;
    std::array<std::array<std::optional<std::uint16_t>, 18>, 2> transitions_{};
    std::bitset<6> pending_hide_, pending_show_;
    void queue_retaliations(BattleState& battle, bool physical, std::optional<std::uint8_t> interceptor = {});
    BattlePhysicalData physical_;
    BattlePhysicalEnvironment physical_environment_;
    PartyData equipment_party_;
    EquipmentData equipment_data_;
    void refresh_equipment(BattleState&);
    std::array<std::uint8_t, 6> special_animations_{};
    std::vector<BattleDamageNumber> numeral_requests_;
    std::optional<BattleChangeRequest> battle_change_;
    bool live_ = false, victory_ready_ = false, escape_ready_ = false, immediate_event_ = false;
    bool immediate_attack_ = false;
    bool defeat_ = false, defeat_message_started_ = false;
    bool leap_pending_ = false, leap_complete_ = false;
    unsigned defeat_waits_ = FF6_ROM_VALUE_BATTLE_DEFEAT_MESSAGE_WAITS_0;
    unsigned completed_actions_ = 0;
    bool victory_animation_ = true;
    BattleVictoryMusic victory_music_;
    bool continue_field_music_ = false, formation_continues_music_ = false;
    std::uint8_t message_speed_ = 0;
    std::shared_ptr<const BattleVictoryAssets> victory_assets_;
    std::optional<BattleRewards> rewards_;
    std::optional<BattleVictory> victory_;
    std::optional<BattleMessagePresentation> message_;
    std::optional<BattleMessagePresentation> attack_name_;
    std::optional<BattleAutomaticRoulette> automatic_roulette_;
    std::uint8_t message_window_uses_ = 0;
    void open_message(std::optional<BattleMessagePresentation>& destination, BattleMessage message);
    std::vector<BattleMagicMessage> pending_messages_;
    BattleExecutionData execution_data_;
    BattleQueueContext queue_context_;
    BattleExecutionContext execution_context_;
    std::optional<BattleQueuedCommand> executing_command_;
    // Retain the outer command across delegated effects/repeated hits.
    std::optional<BattleQueuedCommand> mimic_command_;
    std::optional<GameSoundEffect> script_sound_;
    BattleCommandData commands_;
    std::optional<std::uint8_t> pending_attacker_;
    std::optional<BattleAttackResult> action_result_;
    std::shared_ptr<const BattleEffectLibrary> effects_;
    std::optional<BattleActionAnimation> animation_;
    bool status_animation_ = false;
    std::bitset<4> departing_characters_, jumping_characters_;
    std::bitset<6> jumping_monsters_;
    BattleDamageNumberAssets number_assets_;
    std::optional<BattleDamageNumbers> numbers_;
    bool numbers_started_ = false;
    BattleMonsterDeathPalette death_palette_;
    std::optional<BattleMonsterDeath> death_;
    std::optional<unsigned> outcome_waits_;
    std::string animation_notice_;
    unsigned result_frames_ = 0;
    bool hide_character_indicator_ = false;
    RasterScene visible_;
    void advance_selected_action(BattleState& battle, const BattleParty& party);
    void finish_selected_action(BattleState& battle);
    void execute_selected_attack(BattleState&, const BattleParty&, std::uint8_t,
                                  BattleExecutionRequest);
    unsigned repeated_hit_ = 0, repeated_count_ = 1;
    std::optional<BattleEffectContext> repeated_context_;
    struct WeaponStep {
        BattleQueuedCommand command;
        std::uint8_t hand = 0;
        unsigned remaining = 0;
        bool spell = false, same_target = false;
    };
    std::deque<WeaponStep> weapon_sequence_;
    struct SketchContinuation {
        std::uint8_t source = 0;
        BattleQueuedCommand command;
        bool executing = false;
    };
    std::optional<SketchContinuation> sketch_;
    std::optional<BattleQueuedCommand> item_continuation_;
    bool paused_ = false, flashback_ = false;
    std::uint8_t display_clock_ = 0, picture_clock_ = 0;
    unsigned picture_dance_age_ = 0;
    BattleBackgroundMotion background_motion_;
};
} // namespace ff6
