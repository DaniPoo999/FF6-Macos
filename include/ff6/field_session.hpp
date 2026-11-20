#pragma once
#include "ff6/battle_entrance.hpp"
#include "ff6/battle_formation.hpp"
#include "ff6/battle_menu_session.hpp"
#include "ff6/battle_party.hpp"
#include "ff6/battle_state.hpp"
#include "ff6/battle_submission.hpp"
#include "ff6/field_map_entry.hpp"
#include "ff6/field_menu.hpp"
#include "ff6/music_player.hpp"
#include "ff6/mine_cart.hpp"
#include "ff6/floating_continent.hpp"
#include "ff6/world_ruin.hpp"
#include "ff6/ending_book.hpp"
#include "ff6/ending_characters.hpp"
#include "ff6/ending_flight.hpp"
#include "ff6/ending_finale.hpp"
#include "ff6/name_menu.hpp"
#include "ff6/opening_presentation.hpp"
#include "ff6/save_menu.hpp"
#include "ff6/shop.hpp"
#include "ff6/colosseum_menu.hpp"
#include "ff6/party_selection.hpp"
#include "ff6/final_order.hpp"
#include "ff6/title_presentation.hpp"

namespace ff6 {
// Current ordinary-field profile: display/audio at 60 Hz. This is explicit
// development pacing, not a claim to recover original interrupt/slowdown timing.
inline constexpr unsigned field_session_rate = 60;
inline constexpr unsigned field_session_samples = music_sample_rate / field_session_rate;
static_assert(music_sample_rate % field_session_rate == 0);
struct FieldSessionTick {
    bool complete = false, input_sampled = false;
    std::vector<MusicPlayerCommand> audio;
};
// Host-independent continuation and scene replacement. A completed display tick
// owns one field_session_samples audio interval. Song waits retain the event
// visit while display/audio progress. Work-budget continuations never tick.
class FieldSession {
  public:
    explicit FieldSession(FieldLoop scene, OriginalSram storage = {})
        : scene_(std::move(scene)), storage_(std::move(storage)) {}
    // Explicit development replay profile; ordinary F7 always runs live combat.
    void use_single_action_study() { single_action_study_ = true; }
    void set_save_folder(std::filesystem::path folder, std::optional<unsigned> loaded_slot = {}) {
        if (loaded_slot && *loaded_slot >= 3)
            throw std::invalid_argument("Invalid loaded save slot");
        save_store_.emplace(std::move(folder));
        restart_slot_ = loaded_slot;
    }
    std::uint64_t game_over_restarts() const {
        return game_over_restarts_;
    }
    const std::optional<SaveMenu>& save_menu() const {
        return save_menu_;
    }
    FieldSessionTick advance(const ContentPack& pack, Buttons buttons, std::size_t budget = 1024,
                             bool audio_ready = false, Buttons second = 0,
                             bool music_finished = false,
                             std::optional<std::uint8_t> music_position = std::nullopt);
    const std::optional<WorldState>& world() const { return scene_.state().world; }
    const WorldAssets* world_assets() const { return world_assets_ ? &*world_assets_ : nullptr; }
    const FieldLoop& scene() const {
        return scene_;
    }
    const std::optional<CanonicalFrame>& visible() const {
        return visible_;
    }
    std::uint64_t ticks() const {
        return ticks_;
    }
    std::uint64_t maps_loaded() const {
        return maps_loaded_;
    }
    bool stopped() const {
        return stopped_;
    }
    const std::string& notice() const {
        return notice_;
    }
    FieldLoopBoundary boundary() const {
        return boundary_;
    }
    const std::optional<TitlePresentation>& title() const {
        return title_;
    }
    std::optional<bool> last_title_skipped() const {
        return last_title_skipped_;
    }
    const std::optional<OpeningPresentation>& credits() const {
        return credits_;
    }
    const std::optional<MineCartRide>& mine_cart() const { return mine_cart_; }
    const std::optional<FloatingContinentRise>& continent_rise() const { return continent_rise_; }
    const std::optional<NameMenu>& name_menu() const { return name_menu_; }
    const std::optional<FinalOrderMenu>& final_order_menu() const { return final_order_menu_; }
    const std::optional<TheEnd>& the_end() const { return the_end_; }
    const std::optional<PartySelectionMenu>& party_menu() const { return party_menu_; }
    const std::optional<ColosseumMenu>& colosseum_menu() const { return colosseum_menu_; }
    const std::optional<ShopMenu>& shop_menu() const {
        return shop_menu_;
    }
    const std::optional<FieldMenu>& field_menu() const {
        return field_menu_;
    }
    std::optional<bool> last_credits_skipped() const {
        return last_credits_skipped_;
    }
    const OriginalSram& original_sram() const {
        return storage_;
    }
    std::uint64_t new_game_entries() const {
        return new_game_entries_;
    }
    const std::optional<FieldBattleTransition>& battle_transition() const {
        return battle_transition_;
    }
    const std::optional<FieldBattleEntry>& battle_entry() const {
        return battle_entry_;
    }
    const std::optional<BattleFormationEntry>& battle_formation() const {
        return battle_formation_;
    }
    const std::optional<BattleParty>& battle_party() const {
        return battle_party_;
    }
    const BattleCharacters* battle_characters() const {
        return battle_preparation_ ? &battle_preparation_->characters : nullptr;
    }
    const std::optional<BattlePreparation>& battle_preparation() const {
        return battle_preparation_;
    }
    // Entry snapshots are immutable evidence; battle_state owns subsequent changes.
    const std::optional<BattleStatusState>& battle_status() const {
        return battle_status_;
    }
    const std::optional<BattleEntrance>& battle_entrance() const {
        return battle_entrance_;
    }
    const std::optional<BattleMenuPreview>& battle_menus() const {
        return battle_menus_;
    }
    const std::optional<BattleState>& battle_state() const {
        return battle_state_;
    }

  private:
    FieldLoop scene_;
    std::optional<WorldAssets> world_assets_;
    std::uint8_t world_brightness_ = 0, world_battle_zoom_ = 0;
    DoomGazeLocations doom_gaze_locations_{};
    bool doom_gaze_encounter_ = false;
    std::optional<FieldMapLoadPlan> world_exit_;
    std::optional<WorldScriptState> world_script_;
    void enter_world(const ContentPack& pack, const FieldMapLoadPlan& plan);
    FieldSessionTick advance_world(const ContentPack& pack, Buttons buttons);
    void reload_world(const ContentPack& pack);
    void begin_world_event(const ContentPack& pack, FieldEventRunner& events, std::uint32_t entry);
    RasterScene world_scene() const;
    void clear_battle();
    OriginalSram storage_;
    std::uint64_t new_game_entries_ = 0;
    std::optional<CanonicalFrame> visible_;
    std::uint64_t observed_frames_ = 0, ticks_ = 0, maps_loaded_ = 1;
    Buttons sampled_ = 0, sampled_second_ = 0;
    bool pending_tick_ = false, loading_ = false, stopped_ = false;
    FieldLoopBoundary boundary_ = FieldLoopBoundary::startup_visit;
    std::string notice_;
    std::optional<TitlePresentation> title_;
    std::optional<OpeningPresentation> credits_;
    std::optional<MineCartRide> mine_cart_;
    std::optional<FloatingContinentRise> continent_rise_;
    std::optional<WorldRuin> world_ruin_;
    std::optional<EndingBook> ending_book_;
    std::optional<EndingCharacters> ending_characters_;
    std::optional<EndingFlight> ending_flight_;
    std::optional<EndingAirship> ending_airship_;
    std::optional<TheEnd> the_end_;
    FieldSessionTick advance_mine_cart(const ContentPack& pack);
    std::optional<NameMenu> name_menu_;
    std::optional<NameMenuAssets> name_assets_;
    InputState name_input_;
    std::optional<FieldMenu> field_menu_;
    std::optional<ShopMenu> shop_menu_;
    std::optional<ColosseumMenu> colosseum_menu_;
    std::optional<FinalOrderMenu> final_order_menu_;
    std::optional<std::array<std::optional<std::uint8_t>, 12>> final_order_;
    std::optional<PartySelectionMenu> party_menu_;
    std::optional<SaveStore> save_store_;
    std::optional<unsigned> restart_slot_;
    std::uint64_t game_over_restarts_ = 0;
    std::optional<SaveMenu> save_menu_;
    std::optional<GameMenuState> saved_preferences_;
    std::shared_ptr<const FieldLoop> loaded_scene_;
    SavedGame menu_save_snapshot() const;
    FieldSessionTick advance_save_menu(const ContentPack& pack, Buttons buttons);
    FieldSessionTick restart_after_game_over(const ContentPack& pack);
    std::uint8_t menu_fade_ = 0;
    std::uint8_t named_character_ = 0;
    std::uint64_t cutscene_request_ = 0;
    std::optional<bool> last_title_skipped_;
    std::optional<bool> last_credits_skipped_;
    std::optional<FieldBattleTransition> battle_transition_;
    std::optional<FieldBattleEntry> battle_entry_;
    std::optional<BattleFormationEntry> battle_formation_;
    std::optional<BattleParty> battle_party_;
    std::optional<BattlePreparation> battle_preparation_;
    std::optional<BattleStatusState> battle_status_;
    std::optional<BattleState> battle_state_;
    BattleEventTimerDisplay battle_timer_display_;
    std::optional<BattleFinalTransition> final_transition_;
    std::optional<BattleEntrance> battle_entrance_;
    std::optional<BattleMenuPreview> battle_menus_;
    BattleSubmissionData battle_submission_data_;
    void append_field_audio(std::vector<MusicPlayerCommand>& target);
    void enter_battle(const ContentPack& pack);
    void change_battle(const ContentPack& pack);
    void advance_final_tier(const ContentPack& pack);
    void return_from_battle(const ContentPack& pack);
    void return_from_name_menu(const ContentPack& pack);
    void return_from_event_menu(const ContentPack& pack, bool fade_in);
    void enter_field_menu(const ContentPack& pack, Buttons buttons);
    void return_from_field_menu(const ContentPack& pack);
    bool rewards_applied_ = false, single_action_study_ = false;
};
// Explicit one-character test entry; all content comes from this project's
// extracted package. This is not original new-game initialization or a save.
FieldSession make_development_field_session(const ContentPack& pack, std::uint16_t map);

// Integer host-time accumulator. Catch-up is bounded by the caller but debt is
// retained. Inactive time and >=250ms suspension gaps rebase without game ticks.
class FieldPacer {
  public:
    void update(std::uint64_t now_ns, bool active);
    bool due() const {
        return fraction_ >= 1000000000U;
    }
    void consume();

  private:
    std::optional<std::uint64_t> previous_;
    std::uint64_t fraction_ = 0;
};
} // namespace ff6
