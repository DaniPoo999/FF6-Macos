#pragma once
#include "ff6/field_menu.hpp"
#include "ff6/field_sprites.hpp"

namespace ff6 {
struct ShopDefinition {
    std::uint8_t type = 0, price_rule = 0;
    std::vector<std::uint8_t> items;
};
struct ShopData {
    std::vector<ShopDefinition> shops;
    std::array<std::uint16_t, 256> prices{};
    MenuItemData items;
};
ShopData load_shop_data(const ContentPack& pack);
std::uint16_t shop_price(std::uint16_t base, std::uint8_t rule, std::uint8_t graphics);
std::uint16_t shop_character_mask(const Roster& roster, std::uint16_t initialized);
unsigned shop_equipped(const Roster& roster, std::uint16_t initialized, std::uint8_t item);
unsigned shop_comparison(const MenuItemData& data, const Character& character, std::uint8_t item);
unsigned shop_owned(const Inventory& inventory, std::uint8_t item);
enum class ShopResult { success, unavailable, insufficient_gil, capacity, tool_owned };
ShopResult buy_shop_item(const ShopData& data, const ShopDefinition& shop, const Roster& roster,
                         std::uint16_t initialized, Inventory& inventory, std::uint8_t graphics,
                         unsigned entry, unsigned quantity);
bool sell_shop_item(const ShopData& data, Inventory& inventory, unsigned slot, unsigned quantity);
enum class ShopPage { options, buy, sell, buy_quantity, sell_quantity, closed };
// Event 9B suspends the field. This owner commits only completed purchases/sales;
// the session resumes the same event on a retained-map reload after closing.
class ShopMenu {
  public:
    ShopMenu(const ContentPack& pack, std::uint8_t identity, std::uint8_t graphics,
             FieldMenuValues values, Buttons held_buttons);
    FieldMenuUpdate advance(Buttons buttons);
    RasterScene scene() const;
    const FieldMenuValues& values() const {
        return values_;
    }
    ShopPage page() const {
        return page_;
    }
    bool complete() const {
        return page_ == ShopPage::closed && intensity_ == 0;
    }
    unsigned cursor() const {
        return cursor_;
    }
    unsigned quantity() const {
        return quantity_;
    }
    unsigned option() const { return option_; }

  private:
    MenuLayoutAssets assets_;
    ShopData data_;
    std::array<std::array<std::array<Tile, 8>, 2>, 14> actors_{};
    std::array<std::array<Rgb555, 16>, 14> actor_colors_{};
    FieldMenuValues values_;
    InputState input_;
    std::uint8_t identity_, graphics_, intensity_ = 0;
    ShopPage page_ = ShopPage::options;
    unsigned option_ = 0, cursor_ = 0, scroll_ = 0, quantity_ = 1, wait_ = 0;
    unsigned ticks_ = 0;
    std::uint8_t transaction_item_ = 255;
    std::string message_ = "shopgreetingmsgtext";
    const ShopDefinition& definition() const {
        return data_.shops[identity_];
    }
    unsigned limit() const;
    void return_to_list();
};
} // namespace ff6
