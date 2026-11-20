#include "ff6/battle_damage_numbers.hpp"
#include "rom-materials/battle_damage_number_mass_cutoff.macros.inc"
#include "rom-materials/battle_damage_number_overlap_wait.macros.inc"
#include "rom-materials/battle_damage_number_lifetime.macros.inc"
#include "rom-materials/battle_damage_number_clip_bottom.macros.inc"
#include "rom-materials/battle_damage_number_miss_glyph.macros.inc"
#include "rom-materials/battle_damage_number_digit_glyphs.macros.inc"
#include "rom-materials/battle_damage_number_palettes.macros.inc"
#include "rom-materials/battle_damage_number_priority.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
RasterSprite image(const BattleDamageNumberAssets& assets, BattleDamageNumber request) {
    RasterSprite out;
    out.height = 8;
    out.depth = 3 + 2 * FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_PRIORITY_0;
    out.colors = assets.colors[request.kind == BattleDamageNumberKind::healing ? 1 : 0];
    std::vector<Tile> glyphs;
    if (request.kind == BattleDamageNumberKind::miss)
        glyphs.assign(assets.miss.begin(), assets.miss.end());
    else {
        unsigned divisor = 1000;
        while (divisor > 1 && request.amount < divisor) divisor /= 10;
        for (; divisor; divisor /= 10)
            glyphs.push_back(assets.digits[(request.amount / divisor) % 10]);
    }
    out.width = static_cast<std::uint16_t>(glyphs.size() * 8);
    out.pixels.resize(out.width * out.height);
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < out.width; ++x)
            out.pixels[y * out.width + x] = glyphs[x / 8][y * 8 + x % 8];
    return out;
}
bool empty(const std::array<std::optional<RasterSprite>, 10>& sprites) {
    return std::ranges::none_of(sprites, [](const auto& item) { return item.has_value(); });
}
} // namespace
BattleDamageNumberAssets load_battle_damage_number_assets(const ContentPack& pack) {
    BattleDamageNumberAssets out;
    const auto graphics = decompress_lzss(pack.get("menu_graphics_status_gfx_4bpp_lz"));
    const auto palette = pack.get("battle_character_status_palette");
    const auto bounce = pack.get("battle_damage_number_bounce");
    require(graphics.size() == 8192 && palette.size() == 32 && bounce.size() == 65,
            "Invalid damage number resources");
    const auto tiles = decode_tiles(graphics, 4);
    // Source-selected glyph locations use private ROM input; tile dimensions,
    // decimal radix and the two compact palette records are resource formats.
    std::copy_n(tiles.begin() + FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_MISS_GLYPH_0, 2, out.miss.begin());
    std::copy_n(tiles.begin() + FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_DIGIT_GLYPHS_0, 10, out.digits.begin());
    for (unsigned i = FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_MISS_GLYPH_0;
         i < FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_DIGIT_GLYPHS_0 + 10; ++i)
        require(std::ranges::all_of(tiles[i], [](auto color) { return color == 0 || color >= 12; }),
                "Damage number uses an unowned palette color");
    for (unsigned bank = 0; bank < 2; ++bank)
        for (unsigned i = 0; i < 4; ++i) {
            const auto color = read16(palette, bank * 8 + i * 2);
            require(color <= 32767, "Invalid damage number color");
            out.colors[bank][12 + i] = color;
        }
    std::copy_n(bounce.begin(), 64, out.bounce.begin());
    return out;
}
std::vector<BattleDamageNumber>
merge_battle_damage_numbers(std::span<const BattleDamageNumber> requests) {
    std::vector<BattleDamageNumber> result;
    for (const auto request : requests) {
        require(request.target < 10 && request.amount <= 9999, "Invalid damage contribution");
        auto current = std::ranges::find(result, request.target, &BattleDamageNumber::target);
        if (current == result.end())
            result.push_back(request);
        else if (current->kind == request.kind && request.kind != BattleDamageNumberKind::miss)
            current->amount = static_cast<std::uint16_t>(
                std::min(9999U, unsigned{current->amount} + request.amount));
        else if (request.kind == BattleDamageNumberKind::damage ||
                 current->kind == BattleDamageNumberKind::miss)
            *current = request;
    }
    return result;
}
BattleDamageNumbers::BattleDamageNumbers(const BattleDamageNumberAssets& assets,
                                         std::span<const BattleDamageNumber> requests)
    : assets_(assets) {
    submit(requests);
}
void BattleDamageNumbers::submit(std::span<const BattleDamageNumber> requests) {
    require(dispatch_complete(), "Damage dispatch is still active");
    const bool mass = requests.size() >= FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_MASS_CUTOFF_0;
    require(!mass || complete(), "Mass damage requires drained single numerals");
    numbers_.clear();
    simultaneous_ = mass;
    ticks_ = request_ = overlap_waits_ = 0;
    dispatch_ = requests.empty() ? Dispatch::done : Dispatch::clear;
    require(requests.size() <= 10, "Too many damage number targets");
    std::bitset<10> targets;
    for (const auto request : requests) {
        require(request.target < 10 && !targets[request.target] && request.amount <= 9999 &&
                    (request.kind == BattleDamageNumberKind::damage ||
                     request.kind == BattleDamageNumberKind::healing ||
                     request.kind == BattleDamageNumberKind::miss),
                "Invalid or duplicate damage number");
        targets[request.target] = true;
        numbers_.push_back({request, image(assets_, request)});
    }
    // ExecDmg emits the individual commands in descending participant order.
    std::ranges::sort(numbers_, [](const auto& a, const auto& b) {
        return a.request.target > b.request.target;
    });
}
void BattleDamageNumbers::advance(const BattleSceneAnchors& anchors, std::bitset<10> visible,
                                  const std::array<BattleMonsterStyleMask, 6>& masks) {
    if (complete()) return;
    visible_ = std::move(prepared_);
    prepared_ = {};
    visible_singles_ = std::move(prepared_singles_);
    prepared_singles_ = {};
    ++ticks_;
    if (!simultaneous_) {
        // DMG_NUMERALS_SINGLE rotates four owners. It waits eight extra pictures only
        // when another live owner displays a number for this same target.
        if (dispatch_ == Dispatch::clear) {
            singles_[next_single_].reset();
            dispatch_ = Dispatch::prepare;
        } else if (dispatch_ == Dispatch::prepare) {
            const auto target = numbers_[request_].request.target;
            const bool overlap = std::ranges::any_of(singles_, [target](const auto& n) {
                return n && n->active && n->request.target == target;
            });
            if (overlap) {
                overlap_waits_ = FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_OVERLAP_WAIT_0;
                dispatch_ = Dispatch::overlap_wait;
            }
            else { singles_[next_single_] = numbers_[request_]; singles_[next_single_]->active = true; dispatch_ = Dispatch::publish; }
        } else if (dispatch_ == Dispatch::overlap_wait && --overlap_waits_ == 0) {
            singles_[next_single_] = numbers_[request_];
            singles_[next_single_]->active = true;
            dispatch_ = Dispatch::publish;
        } else if (dispatch_ == Dispatch::publish) {
            next_single_ = (next_single_ + 1) % singles_.size();
            dispatch_ = ++request_ == numbers_.size() ? Dispatch::done : Dispatch::clear;
        }
        for (unsigned i = 0; i < singles_.size(); ++i) {
            if (!singles_[i]) continue;
            auto& n = *singles_[i];
            const auto target = n.request.target;
            if (!visible[target] || n.age == FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_LIFETIME_0) {
                singles_[i].reset();
                continue;
            }
            auto sprite = n.image;
            const auto& anchor = anchors[target];
            const unsigned miss = n.request.kind == BattleDamageNumberKind::miss ? 8U : 0U;
            const auto x = (unsigned{anchor.center_x} - sprite.width / 2U + miss) & 511U;
            const auto y = wrap8(unsigned{anchor.bottom_y} - assets_.bounce[n.age++]);
            sprite.x = x >= 256 ? static_cast<int>(x) - 512 : static_cast<int>(x);
            sprite.y = y;
            if (target >= 4) {
                const auto mask = masks[target - 4];
                const unsigned palette =
                    (n.request.kind == BattleDamageNumberKind::healing
                         ? unsigned{FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_PALETTES_3}
                         : unsigned{FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_PALETTES_2}) & mask.palette;
                require(palette == 4 || palette == 5, "Unowned masked damage number palette");
                sprite.colors = assets_.colors[palette - 4];
                sprite.depth = static_cast<std::uint16_t>(3 + 2 * mask.priority);
            }
            if (y < FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_CLIP_BOTTOM_0)
                prepared_singles_[i] = PublishedNumber{target, std::move(sprite)};
        }
        // Convenience view for readout/probe callers; composition below keeps
        // every independently bouncing owner, including two on one target.
        visible_ = {};
        for (const auto& n : visible_singles_)
            if (n && !visible_[n->target]) visible_[n->target] = n->image;
        return;
    }
    for (unsigned i = 0; i < numbers_.size(); ++i) {
        auto& number = numbers_[i];
        // Single commands have two setup waits before preparing the image and
        // one publication wait afterward. The simultaneous upload has 3 waits.
        const unsigned start = simultaneous_ ? 4 : 2 + i * 3;
        if (ticks_ == start) number.active = true;
        if (!number.active) continue;
        const auto target = number.request.target;
        if (!visible[target] || number.age == FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_LIFETIME_0) {
            number.active = false;
            continue;
        }
        auto sprite = number.image;
        const auto& anchor = anchors[target];
        // The original Miss image starts in the second tile of its buffer.
        // Single commands subtract 8; mass setup leaves that offset at zero.
        const unsigned miss_offset = number.request.kind == BattleDamageNumberKind::miss
                                         ? (simultaneous_ ? 16 : 8) : 0;
        const auto x = (unsigned{anchor.center_x} - sprite.width / 2U + miss_offset) & 511U;
        const auto y = wrap8(unsigned{anchor.bottom_y} - assets_.bounce[number.age]);
        sprite.x = x >= 256 ? static_cast<int>(x) - 512 : static_cast<int>(x);
        sprite.y = simultaneous_ && y >= 224 ? int{y} - 256 : int{y};
        if (!simultaneous_ && target >= 4) {
            const auto mask = masks[target - 4];
            const unsigned palette =
                (number.request.kind == BattleDamageNumberKind::healing
                     ? unsigned{FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_PALETTES_1}
                     : unsigned{FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_PALETTES_0}) & mask.palette;
            require(palette == 4 || palette == 5, "Unowned masked damage number palette");
            sprite.colors = assets_.colors[palette - 4];
            sprite.depth = static_cast<std::uint16_t>(3 + 2 * mask.priority);
        }
        ++number.age;
        if (simultaneous_ || y < FF6_ROM_VALUE_BATTLE_DAMAGE_NUMBER_CLIP_BOTTOM_0)
            prepared_[target] = std::move(sprite);
    }
}
bool BattleDamageNumbers::complete() const {
    if (!simultaneous_)
        return dispatch_ == Dispatch::done &&
            std::ranges::none_of(singles_, [](const auto& n) { return n.has_value(); }) &&
            std::ranges::none_of(prepared_singles_, [](const auto& n) { return n.has_value(); }) &&
            std::ranges::none_of(visible_singles_, [](const auto& n) { return n.has_value(); });
    return (numbers_.empty() || ticks_ >= (simultaneous_ ? 4U : 2U + (numbers_.size() - 1) * 3U)) &&
           std::ranges::none_of(numbers_, [](const auto& n) { return n.active; }) &&
           empty(prepared_) && empty(visible_);
}
bool BattleDamageNumbers::dispatch_complete() const {
    return simultaneous_ ? complete() : dispatch_ == Dispatch::done;
}
void BattleDamageNumbers::add_to(std::vector<BattleActorSpriteGroup>& actors) const {
    if (!simultaneous_) {
        for (auto& actor : actors) {
            auto position = actor.sprites.begin();
            for (const auto& n : visible_singles_)
                if (n && n->target == actor.participant)
                    position = actor.sprites.insert(position, {n->image, true}) + 1;
        }
        return;
    }
    // Numerals precede their own participant, retaining actor-to-actor ordering.
    for (auto& actor : actors)
        if (const auto& sprite = visible_.at(actor.participant))
            actor.sprites.insert(actor.sprites.begin(), {*sprite, true});
}
} // namespace ff6
