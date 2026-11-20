#pragma once
#include "ff6/game_menu_state.hpp"
#include "ff6/menu_canvas.hpp"
#include "ff6/menu_items.hpp"
#include "ff6/menu_skills.hpp"
#include "ff6/name_menu.hpp"
#include "ff6/party_menu.hpp"
#include <bitset>

namespace ff6 {
struct FieldMenuValues {
    Roster roster;
    // Read-only snapshot of event bits 2E0..2EF. Availability and active-party
    // membership are independent; neither selects the equipment/shop roster.
    std::uint16_t initialized_characters = 0;
    Inventory inventory;
    GameMenuState preferences;
    InputSettings input;
    WindowPalettes window_colors{};
    std::uint8_t window_skin = 0;
    Rgb555 font_color = 32767;
    std::uint32_t steps = 0;
    std::bitset<28> rare_items;
    FieldItemPermissions permissions;
};
enum class FieldMenuPage {
    main,
    character,
    order,
    order_move,
    items,
    item_move,
    item_options,
    item_target,
    item_details,
    rare_items,
    equipment,
    equip_slot,
    equip_list,
    party_equipment,
    status,
    gogo_commands,
    skills,
    skill_list,
    esper_details,
    magic_target,
    configuration,
    command_arrange,
    command_select,
    command_move,
    controllers,
    naming,
    save,
    closed
};
struct FieldMenuUpdate {
    std::vector<std::uint8_t> sounds, game_sounds;
};
// This owner temporarily owns the persistent menu-editable values while field
// simulation is suspended. Close returns them to the field in one transaction.
class FieldMenu {
  public:
    FieldMenu(const ContentPack& pack, FieldMenuValues values, Buttons held_buttons = 0,
              std::optional<std::uint8_t> inspected_character = {});
    FieldMenuUpdate advance(Buttons buttons);
    RasterScene scene();
    const FieldMenuValues& values() const {
        return values_;
    }
    FieldMenuPage page() const {
        return page_;
    }
    bool complete() const {
        return page_ == FieldMenuPage::closed && !transitioning_;
    }
    const std::optional<FieldItemResult>& item_handoff() const {
        return item_handoff_;
    }
    // Read-only navigation state for the inspector and controller replays.
    unsigned cursor() const {
        return page_ == FieldMenuPage::main ? main_.cursor() : cursor_;
    }
    unsigned target_slot() const {
        return character_slot_;
    }
    unsigned equipment_slot() const { return equip_slot_; }
    std::optional<std::uint8_t> equipment_candidate() const {
        return page_ == FieldMenuPage::equip_list && cursor_ < candidates_.size()
                   ? std::optional<std::uint8_t>{values_.inventory.slots[candidates_[cursor_]].item}
                   : std::nullopt;
    }
    std::uint8_t selected_character() const;
    void finish_save(std::optional<GameMenuState> saved = {});
    // Timer 0 replaces the ordinary main-menu clock. Expiry closes through the
    // normal fade/return owner, preserving any inventory/equipment changes.
    void event_timer(std::optional<std::uint16_t> frames, bool expired);

  private:
    MenuLayoutAssets assets_;
    MenuItemData items_;
    PartyData party_;
    EquipmentData equipment_;
    MenuSkillData skills_;
    NameMenuAssets name_assets_;
    std::optional<NameMenu> naming_;
    FieldMenuValues values_;
    std::optional<std::uint16_t> timer_frames_;
    std::optional<std::uint8_t> inspected_character_;
    MainMenuNavigation main_;
    CharacterMenuNavigation characters_;
    InputState input_;
    FieldMenuPage page_ = FieldMenuPage::main, next_page_ = FieldMenuPage::main;
    FieldMenuPage previous_page_ = FieldMenuPage::main;
    CharacterMenuKind kind_ = CharacterMenuKind::status;
    bool transitioning_ = false, fade_in_ = true, relics_ = false, removing_ = false;
    std::uint8_t intensity_ = 0;
    unsigned invalid_ticks_ = 0, ticks_ = 0, character_slot_ = 0;
    unsigned cursor_ = 0, scroll_ = 0, selected_ = 0, option_ = 0, equip_slot_ = 0;
    unsigned description_ticks_ = 0;
    unsigned detail_page_ = 0, reequip_ticks_ = 0, esper_error_ticks_ = 0;
    unsigned detail_scroll_ = 0, order_scroll_ = 0, order_target_ = 0, row_slide_ = 0;
    unsigned swap_ticks_ = 0;
    unsigned config_scroll_ = 0;
    std::optional<std::uint8_t> esper_owner_;
    unsigned skill_ = 0, caster_slot_ = 0, target_slot_ = 0, color_index_ = 0;
    unsigned saved_skill_cursor_ = 0, saved_skill_scroll_ = 0;
    bool magic_costs_ = false, multi_target_ = false;
    std::vector<std::uint8_t> skill_list_;
    unsigned saved_item_cursor_ = 0, saved_item_scroll_ = 0;
    std::vector<std::uint8_t> candidates_;
    std::array<std::uint8_t, 2> old_relics_{};
    std::optional<FieldItemResult> item_handoff_;
    void transition(FieldMenuPage target);
    void enter_page();
    void sidebar();
    void move_list(unsigned size, unsigned height, Buttons repeat, FieldMenuUpdate& out);
    void move_character(Buttons repeat, FieldMenuUpdate& out);
    void swap_party_order();
    void reject(FieldMenuUpdate& out);
    void begin_item(FieldMenuUpdate& out);
    void advance_items(Buttons press, Buttons repeat, FieldMenuUpdate& out);
    void advance_equipment(Buttons press, Buttons repeat, FieldMenuUpdate& out);
    void advance_status(Buttons press, Buttons repeat, FieldMenuUpdate& out);
    void advance_skills(Buttons press, Buttons repeat, FieldMenuUpdate& out);
    void advance_configuration(Buttons press, Buttons repeat, FieldMenuUpdate& out);
    void change_character(Buttons press, FieldMenuUpdate& out);
    bool check_reequip();
    void prepare_skill_list();
    RasterScene item_scene();
    RasterScene equipment_scene();
    RasterScene status_scene();
    RasterScene skill_scene();
    RasterScene configuration_scene();
    RasterScene configuration_page_scene(unsigned page);
    MainMenuView party_view();
    void draw_party_status(MenuCanvas& canvas, int shift_x);
};
} // namespace ff6
