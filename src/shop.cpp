#include "ff6/shop.hpp"
#include "rom-materials/shop_item_quantity_cap.macros.inc"
#include "rom-materials/shop_quantity_step.macros.inc"
#include "rom-materials/shop_buy_list_rows.macros.inc"
#include "rom-materials/shop_sell_list_rows.macros.inc"
#include "rom-materials/shop_gil_limit_parts.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include <algorithm>
#include "rom-materials/shop_presentation_choices.macros.inc"

namespace ff6 {
static_assert(FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_0 == FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_2 &&
              FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_1 == FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_3);
ShopData load_shop_data(const ContentPack& pack) {
    ShopData data;
    data.items = load_menu_items(pack);
    const auto shops = pack.get("data_c47ac0_shop_prop"), items = pack.get("data_d85000_item_prop");
    if (shops.size() % 9 || items.size() != 256 * 30)
        throw std::invalid_argument("Invalid shop data extent");
    for (unsigned i = 0; i < 256; ++i)
        data.prices[i] = read16(items, i * 30 + 28);
    for (unsigned at = 0; at < shops.size(); at += 9) {
        ShopDefinition shop;
        shop.type = shops[at] & 7U;
        shop.price_rule = (shops[at] >> 3U) & 7U;
        for (unsigned i = 1; i <= 8; ++i)
            if (shops[at + i] != 255)
                shop.items.push_back(shops[at + i]);
        data.shops.push_back(std::move(shop));
    }
    return data;
}
std::uint16_t shop_price(std::uint16_t base, std::uint8_t rule, std::uint8_t graphics) {
    const bool female = graphics == 0 || graphics == 6 || graphics == 8;
    // AdjustShopPrice uses word arithmetic, including truncation before the
    // +50% addition. Discounts use the displayed graphics, not character id.
    switch (rule) {
    case 0:
        return base;
    case 1:
        return wrap16(unsigned{base} + base / 2U);
    case 2:
        return wrap16(unsigned{base} * 2U);
    case 3:
        return base / 2;
    case 4:
        return shop_price(base, female ? 3 : 1, graphics);
    case 5:
        return shop_price(base, female ? 1 : 3, graphics);
    case 6:
        return graphics == 4 ? base / 2 : base;
    default:
        throw std::invalid_argument("Invalid shop price rule");
    }
}
unsigned shop_owned(const Inventory& inventory, std::uint8_t item) {
    const auto found = std::ranges::find(inventory.slots, item, &InventorySlot::item);
    return found == inventory.slots.end() || item == 255 ? 0 : found->quantity;
}
std::uint16_t shop_character_mask(const Roster& roster, std::uint16_t initialized) {
    // C3/C109 and C3/BF69 select the same initialized slots and exclude guest
    // properties. Temporary story availability must not hide their gear/users.
    for (unsigned i = 0; i < roster.characters.size(); ++i) {
        const auto property = roster.characters[i].properties;
        if (!property || *property >= 14)
            initialized &= static_cast<std::uint16_t>(~(1U << i));
    }
    return initialized;
}
unsigned shop_equipped(const Roster& roster, std::uint16_t initialized, std::uint8_t item) {
    unsigned count = 0;
    const auto eligible = shop_character_mask(roster, initialized);
    for (unsigned i = 0; i < 16; ++i) {
        const auto& c = roster.characters[i];
        if (eligible & (1U << i))
            count += static_cast<unsigned>(std::ranges::count(c.equipment, item));
    }
    return count;
}
unsigned shop_comparison(const MenuItemData& data, const Character& c, std::uint8_t item) {
    constexpr std::uint8_t weapon = 1, armor = 2, shield = 3, helmet = 4, relic = 5;
    const auto& selected = data.items[item];
    if (!c.properties || *c.properties >= 14 || !(selected.characters & (1U << *c.properties)))
        return 0;
    const auto& e = c.equipment;
    unsigned prior = 255;
    // C3/BCFD dispatches encoded item types independently of equipment slots.
    // Compare raw power/defense; body armor (type 2) occupies slot 3, while
    // helmets (type 4) occupy slot 2 and shields (type 3) occupy either hand.
    if (selected.type == weapon || selected.type == shield) {
        if (e[0] == item || e[1] == item)
            return 1;
        if (selected.type == weapon) {
            if (e[0] != 255 && data.items[e[0]].type == weapon)
                prior = e[0];
            else if (e[1] != 255)
                prior = data.items[e[1]].type == weapon ? e[1] : e[0];
            // C3/BDE0 goes directly to "better" when the left hand is empty
            // and the right hand is not a weapon, even if it holds a shield.
        } else {
            prior = data.items[e[1]].type == shield ? e[1]
                    : data.items[e[0]].type == shield ? e[0] : 255;
        }
    } else if (selected.type == armor || selected.type == helmet)
        prior = e[selected.type == armor ? 3 : 2];
    else if (selected.type == relic)
        return e[4] == item || e[5] == item ? 1 : 0;
    else
        return 0;
    if (prior == item)
        return 1;
    if (prior == 255)
        return 2;
    return data.items[prior].power == selected.power  ? 4
           : data.items[prior].power < selected.power ? 2
                                                      : 3;
}
ShopResult buy_shop_item(const ShopData& data, const ShopDefinition& shop, const Roster& roster,
                         std::uint16_t initialized, Inventory& inventory, std::uint8_t graphics,
                         unsigned entry, unsigned quantity) {
    if (entry >= shop.items.size() || quantity == 0 || quantity > FF6_ROM_VALUE_SHOP_ITEM_QUANTITY_CAP_0)
        return ShopResult::unavailable;
    const auto item = shop.items[entry];
    if (item == 255)
        return ShopResult::unavailable;
    const auto owned = shop_owned(inventory, item);
    const auto equipped = shop_equipped(roster, initialized, item);
    if (data.items.items[item].type == 0 && owned)
        return ShopResult::tool_owned;
    const auto price =
        std::uint32_t{shop_price(data.prices[item], shop.price_rule, graphics)} * quantity;
    if (price > inventory.gil)
        return ShopResult::insufficient_gil;
    if (owned + equipped + quantity > FF6_ROM_VALUE_SHOP_ITEM_QUANTITY_CAP_0)
        return ShopResult::capacity;
    auto slot = std::ranges::find(inventory.slots, item, &InventorySlot::item);
    if (slot == inventory.slots.end())
        slot = std::ranges::find(inventory.slots, 255, &InventorySlot::item);
    if (slot == inventory.slots.end())
        return ShopResult::capacity;
    *slot = {item, static_cast<std::uint8_t>(owned + quantity)};
    inventory.gil = std::min<std::uint32_t>((FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_0 + FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_1), inventory.gil - price);
    return ShopResult::success;
}
bool sell_shop_item(const ShopData& data, Inventory& inventory, unsigned slot, unsigned quantity) {
    if (slot >= inventory.slots.size() || quantity == 0)
        return false;
    auto& stock = inventory.slots[slot];
    if (stock.item == 255 || quantity > stock.quantity)
        return false;
    const auto price = std::uint32_t{data.prices[stock.item] / 2U} * quantity;
    inventory.gil = std::min<std::uint32_t>((FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_0 + FF6_ROM_VALUE_SHOP_GIL_LIMIT_PARTS_1), (inventory.gil + price) & 0xffffffU);
    stock.quantity = static_cast<std::uint8_t>(stock.quantity - quantity);
    if (!stock.quantity)
        stock.item = 255;
    return true;
}
ShopMenu::ShopMenu(const ContentPack& pack, std::uint8_t identity, std::uint8_t graphics,
                   FieldMenuValues values, Buttons buttons)
    : assets_(load_menu_layouts(pack)), data_(load_shop_data(pack)), values_(std::move(values)),
      identity_(identity), graphics_(graphics) {
    if (identity >= data_.shops.size() || definition().type < 1 || definition().type > 5 ||
        definition().price_rule > 6 || definition().items.empty())
        throw std::invalid_argument("Invalid shop identity or definition");
    input_.previous = buttons;
    const auto sprites = load_field_sprites(pack);
    const auto mapping = pack.get("field_menu_data_shop_character_tiles");
    const auto animation = pack.get("data_d8e8c8_menu_animation_and_letters");
    const auto palettes = pack.get("interface_palettes_battle_char_pal_pal");
    if (mapping.size() != 96)
        throw std::invalid_argument("Invalid shop pose mapping");
    for (unsigned c = 0; c < 14; ++c) {
        const auto anim = read16(animation, 0x4f + c * 2) - 0xe8c8U;
        const auto frame = read16(animation, anim) - 0xe8c8U;
        const unsigned palette = ((slice(animation, frame, 9)[4] >> 1) & 7U) - 2;
        if (palette >= 6)
            throw std::invalid_argument("Invalid shop character palette");
        for (unsigned color = 0; color < 16; ++color)
            actor_colors_[c][color] = read16(palettes, palette * 32 + color * 2) & 32767U;
        // LoadShopCharGfx's data describes two native poses per character.
        for (unsigned pose = 0; pose < 2; ++pose)
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned col = 0; col < 2; ++col) {
                    const unsigned tile =
                        (c % 2) * 8 + pose * 4 + (row / 2) * 2 + (row % 2) * 16 + col;
                    const auto offset = read16(mapping, 32 + mapping[tile] * 2U);
                    if (offset != 65535)
                        actors_[c][pose][row * 2 + col] =
                            sprites.tiles.at(sprites.graphics[c & ~1U] + offset / 32);
                }
    }
}
unsigned ShopMenu::limit() const {
    if (page_ == ShopPage::sell_quantity)
        return values_.inventory.slots[cursor_].quantity;
    const auto item = definition().items[cursor_];
    const auto count = shop_owned(values_.inventory, item) +
                       shop_equipped(values_.roster, values_.initialized_characters, item);
    return count < FF6_ROM_VALUE_SHOP_ITEM_QUANTITY_CAP_0 ? FF6_ROM_VALUE_SHOP_ITEM_QUANTITY_CAP_0 - count : 0;
}
void ShopMenu::return_to_list() {
    page_ = (page_ == ShopPage::buy_quantity || page_ == ShopPage::buy) ? ShopPage::buy
                                                                        : ShopPage::sell;
    message_ = page_ == ShopPage::buy ? "shopbuymsgtext" : "shopsellmsgtext";
}
FieldMenuUpdate ShopMenu::advance(Buttons buttons) {
    ++ticks_;
    FieldMenuUpdate out;
    auto& sounds = out.sounds;
    sample_input(input_, values_.input, InputContext::menu, buttons, 0);
    if (page_ == ShopPage::closed) {
        if (intensity_)
            --intensity_;
        return out;
    }
    if (intensity_ < 15) {
        ++intensity_;
        return out;
    }
    if (wait_) {
        if (!--wait_)
            return_to_list();
        return out;
    }
    const auto press = input_.pressed, repeat = input_.repeating;
    if (page_ == ShopPage::options) {
        if (held(repeat, Button::left) || held(repeat, Button::right)) {
            option_ = (option_ + (held(repeat, Button::right) ? 1 : 2)) % 3;
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        }
        if (held(press, Button::b) || (held(press, Button::a) && option_ == 2)) {
            page_ = ShopPage::closed;
            sounds.push_back(held(press, Button::b) ? FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U : FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        } else if (held(press, Button::a)) {
            page_ = option_ == 0 ? ShopPage::buy : ShopPage::sell;
            cursor_ = scroll_ = 0;
            return_to_list();
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        }
        return out;
    }
    const bool quantity = page_ == ShopPage::buy_quantity || page_ == ShopPage::sell_quantity;
    if (held(press, Button::b)) {
        sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        if (quantity)
            return_to_list();
        else {
            page_ = ShopPage::options;
            message_ = "shopgreetingmsgtext";
        }
        return out;
    }
    if (quantity) {
        auto next = quantity_;
        if (held(repeat, Button::right))
            next = std::min(limit(), next + 1);
        else if (held(repeat, Button::left))
            next = std::max(1U, next - 1);
        else if (held(repeat, Button::up)) {
            if (page_ == ShopPage::sell_quantity)
                next = std::min(limit(), next + FF6_ROM_VALUE_SHOP_QUANTITY_STEP_0);
            else if (next + FF6_ROM_VALUE_SHOP_QUANTITY_STEP_0 <= limit())
                next += FF6_ROM_VALUE_SHOP_QUANTITY_STEP_0; // Buy's upward step does not clamp.
        } else if (held(repeat, Button::down) && next != FF6_ROM_VALUE_SHOP_QUANTITY_STEP_0)
            next = next > FF6_ROM_VALUE_SHOP_QUANTITY_STEP_0 ? next - FF6_ROM_VALUE_SHOP_QUANTITY_STEP_0 : 1;
        if (next != quantity_) {
            const auto price = page_ == ShopPage::buy_quantity
                                   ? shop_price(data_.prices[definition().items[cursor_]],
                                                definition().price_rule, graphics_)
                                   : std::uint16_t{0};
            if (std::uint32_t{price} * next <= values_.inventory.gil)
                quantity_ = next;
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        }
        if (held(press, Button::a)) {
            const bool ok =
                page_ == ShopPage::buy_quantity
                    ? buy_shop_item(data_, definition(), values_.roster, values_.initialized_characters,
                                    values_.inventory, graphics_, cursor_, quantity_) == ShopResult::success
                    : sell_shop_item(data_, values_.inventory, cursor_, quantity_);
            if (!ok)
                throw std::logic_error("Shop quantity admission lost its transaction");
            out.game_sounds.push_back(FF6_ROM_VALUE_SHOP_PRESENTATION_CHOICES_0);
            message_ = "shopbyemsgtext";
            wait_ = FF6_ROM_VALUE_SHOP_PRESENTATION_CHOICES_1;
        }
    } else {
        const unsigned count =
            page_ == ShopPage::buy ? static_cast<unsigned>(definition().items.size()) : 255;
        const unsigned columns = 1;
        const auto old = cursor_;
        if (held(repeat, Button::down))
            cursor_ = std::min(count - 1, cursor_ + columns);
        else if (held(repeat, Button::up))
            cursor_ = cursor_ >= columns ? cursor_ - columns : cursor_;
        else if (columns == 2 && held(repeat, Button::right))
            cursor_ |= 1;
        else if (columns == 2 && held(repeat, Button::left))
            cursor_ &= ~1U;
        if (cursor_ != old)
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        const unsigned rows = page_ == ShopPage::buy ? FF6_ROM_VALUE_SHOP_BUY_LIST_ROWS_0 : FF6_ROM_VALUE_SHOP_SELL_LIST_ROWS_0;
        if (cursor_ / columns < scroll_)
            scroll_ = cursor_ / columns;
        if (cursor_ / columns >= scroll_ + rows)
            scroll_ = cursor_ / columns - rows + 1;
        if (held(press, Button::a)) {
            auto result = ShopResult::success;
            if (page_ == ShopPage::buy) {
                auto trial = values_.inventory;
                result = buy_shop_item(data_, definition(), values_.roster, values_.initialized_characters,
                                       trial, graphics_, cursor_, 1);
            } else if (values_.inventory.slots[cursor_].item == 255 ||
                       !values_.inventory.slots[cursor_].quantity)
                result = ShopResult::unavailable;
            if (result == ShopResult::success) {
                page_ = page_ == ShopPage::buy ? ShopPage::buy_quantity : ShopPage::sell_quantity;
                transaction_item_ = page_ == ShopPage::buy_quantity
                                        ? definition().items[cursor_]
                                        : values_.inventory.slots[cursor_].item;
                quantity_ = 1;
                message_ =
                    page_ == ShopPage::buy_quantity ? "shopbuyqtymsgtext" : "shopsellqtymsgtext";
                sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            } else {
                sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
                if (page_ == ShopPage::buy) {
                    message_ = result == ShopResult::insufficient_gil ? "shopnotenoughgilmsgtext"
                               : result == ShopResult::tool_owned     ? "shoponlyonemsgtext"
                                                                      : "shoptoomanymsgtext";
                    wait_ = FF6_ROM_VALUE_SHOP_PRESENTATION_CHOICES_1;
                }
            }
        }
    }
    return out;
}
} // namespace ff6
