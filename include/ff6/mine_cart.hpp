#pragma once
#include "ff6/field_battle.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
struct MineCartSegment {
    std::uint8_t pitch = 0, yaw = 0, scenery = 0, bend = 0, command = 0;
};
struct MineCartStamp {
    std::uint8_t x = 0, y = 0;
    std::uint16_t image = 0;
};
struct MineCartAssets {
    std::array<std::array<std::uint8_t, 32>, 13> rise{}, pitch{};
    std::array<std::array<std::uint8_t, 32>, 3> yaw{};
    std::array<std::array<std::uint8_t, 32>, 5> scenery{};
    std::array<std::uint8_t, 64> perspective{};
    std::array<std::uint8_t, 32> bend{};
    std::array<std::array<MineCartStamp, 12>, 20> layouts{};
    std::vector<MineCartSegment> course;
    // Authored sizes, largest first; mirrored atlas entries share this interface.
    std::array<std::array<Bytes, 12>, 29> images;
    std::array<Rgb555, 256> colors{};
    Bytes battle_mosaic;
    std::uint8_t song = 0;
};
MineCartAssets decode_mine_cart_assets(ByteView course, ByteView graphics, ByteView directory,
                                       ByteView palette, ByteView mosaic, ByteView songs);
MineCartAssets load_mine_cart_assets(const ContentPack& pack);

// The course is a data language. The suspended field and combat retain their
// own state; this owner contains only tunnel samples, progress and presentation.
class MineCartRide {
  public:
    explicit MineCartRide(MineCartAssets assets);
    void advance(); // One 60 Hz display interval; course/drawing advance at 15 Hz.
    void resume_after_battle();
    RasterScene scene() const;
    bool complete() const { return complete_; }
    bool battle_ready() const;
    std::optional<std::uint8_t> battle_group() const { return battle_; }
    std::size_t next_segment() const { return next_; }
    std::uint8_t sample() const { return sample_; }
    std::uint8_t song() const { return assets_.song; }
    bool take_battle_sound();

  private:
    MineCartAssets assets_;
    std::array<std::uint8_t, 64> rise_{}, pitch_{}, yaw_{}, scenery_{};
    std::array<std::uint8_t, 128 * 128> picture_{};
    std::size_t next_ = 0, transition_ = 0;
    std::uint8_t sample_ = 0, interval_ = 0, brightness_ = 0;
    bool fading_ = false, complete_ = false, battle_sound_ = false;
    std::optional<std::uint8_t> battle_;
    void load_segment(unsigned half);
    void draw_tunnel();
};
// TrainCmd_e0/e1 read the current general field RNG entry, then increment it.
// This deliberately does not consume the ordinary encounter formation stream.
FieldBattleRequest select_mine_cart_battle(const FieldBattleData& data, std::uint8_t group,
                                           std::uint8_t& field_random);
} // namespace ff6
