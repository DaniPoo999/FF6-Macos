#pragma once
#include "ff6/battle_actor_scene.hpp"
#include <bitset>
#include <span>

namespace ff6 {
struct BattleDamageNumberAssets {
    std::array<Tile, 10> digits{};
    std::array<Tile, 2> miss{};
    std::array<std::array<Rgb555, 16>, 2> colors{};
    std::array<std::uint8_t, 64> bounce{};
};
BattleDamageNumberAssets load_battle_damage_number_assets(const ContentPack& pack);
enum class BattleDamageNumberKind { damage, healing, miss };
struct BattleDamageNumber {
    std::uint8_t target = 0;
    BattleDamageNumberKind kind = BattleDamageNumberKind::damage;
    std::uint16_t amount = 0;
};
// Several reflected rays and self-drain can contribute to one actor. The
// original display chooses accumulated damage before healing before MISS.
std::vector<BattleDamageNumber>
merge_battle_damage_numbers(std::span<const BattleDamageNumber> requests);
// Damage display is independent of HP mutation. In particular, overkill and
// healing at maximum HP still show the calculated amount, including zero.
// One batch supports the ordinary single-target commands or the >=5-target
// simultaneous command. A new batch may follow dispatch while earlier single
// numerals still bounce; the original has four rotating single-number owners.
class BattleDamageNumbers {
  public:
    BattleDamageNumbers(const BattleDamageNumberAssets& assets,
                        std::span<const BattleDamageNumber> requests);
    void submit(std::span<const BattleDamageNumber> requests);
    void advance(const BattleSceneAnchors& anchors, std::bitset<10> visible,
                 const std::array<BattleMonsterStyleMask, 6>& masks = {});
    void add_to(std::vector<BattleActorSpriteGroup>& actors) const;
    const auto& sprites() const { return visible_; }
    bool complete() const;
    // Single commands return after their uploads while numerals keep bouncing;
    // the simultaneous command waits for all of its numerals to finish.
    bool dispatch_complete() const;
  private:
    struct Number {
        BattleDamageNumber request;
        RasterSprite image;
        unsigned age = 0;
        bool active = false;
    };
    BattleDamageNumberAssets assets_;
    std::vector<Number> numbers_;
    std::array<std::optional<RasterSprite>, 10> prepared_{}, visible_{};
    std::array<std::optional<Number>, 4> singles_{};
    struct PublishedNumber { std::uint8_t target; RasterSprite image; };
    std::array<std::optional<PublishedNumber>, 4> prepared_singles_{}, visible_singles_{};
    enum class Dispatch { clear, prepare, overlap_wait, publish, done };
    Dispatch dispatch_ = Dispatch::done;
    unsigned next_single_ = 0, request_ = 0, overlap_waits_ = 0;
    unsigned ticks_ = 0;
    bool simultaneous_ = false;
};
} // namespace ff6
