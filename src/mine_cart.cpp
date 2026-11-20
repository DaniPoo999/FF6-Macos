#include "ff6/mine_cart.hpp"
#include "rom-materials/mine_cart_encounter_e0.macros.inc"
#include "rom-materials/mine_cart_encounter_e1.macros.inc"
#include "rom-materials/mine_cart_encounter_e2.macros.inc"
#include "rom-materials/mine_cart_encounter_threshold.macros.inc"
#include "rom-materials/mine_cart_battle_background.macros.inc"
#include "rom-materials/mine_cart_tunnel_vertical.macros.inc"
#include "rom-materials/mine_cart_tunnel_horizontal.macros.inc"
#include "rom-materials/mine_cart_update_period.macros.inc"
#include "rom-materials/mine_cart_texture_left.macros.inc"
#include "rom-materials/mine_cart_texture_width.macros.inc"
#include "rom-materials/mine_cart_texture_rows.macros.inc"
#include "rom-materials/mine_cart_visible_rows.macros.inc"
#include "rom-materials/mine_cart_mirror_destination.macros.inc"
#include "rom-materials/mine_cart_mirror_count.macros.inc"
#include "rom-materials/approach_viewport.macros.inc"
#include "rom-materials/mine_cart_camera_projection.macros.inc"
#include "rom-materials/mine_cart_sample_window.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
constexpr std::array<unsigned, 12> sizes{
#include "rom-materials/mine_cart_image_dimensions.inc"
};
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
int signed_byte(unsigned v) { return v < 128 ? int(v) : int(v) - 256; }
// Multiplication truncates the positive magnitude before restoring the sign.
// Using a signed right shift here would move negative coordinates one texel.
int scaled(int value, unsigned scale) {
    const int magnitude = value < 0 ? -value : value;
    const int result = magnitude * int(scale) / 256;
    return value < 0 ? -result : result;
}
} // namespace
MineCartAssets decode_mine_cart_assets(ByteView course, ByteView graphics, ByteView directory,
                                       ByteView palette, ByteView mosaic, ByteView songs) {
    require(course.size() == 2404 && directory.size() == 696 && palette.size() == 512 &&
                mosaic.size() == 41 && songs.size() == 2,
            "Invalid mine-cart content extent");
    MineCartAssets a;
    std::size_t cursor = 0;
    const auto rows = [&](auto& values) {
        for (auto& row : values) {
            std::copy_n(course.begin() + static_cast<std::ptrdiff_t>(cursor), row.size(), row.begin());
            cursor += row.size();
        }
    };
    rows(a.rise); rows(a.pitch);
    std::copy_n(course.begin() + static_cast<std::ptrdiff_t>(cursor), 64, a.perspective.begin());
    cursor += 64;
    std::copy_n(course.begin() + static_cast<std::ptrdiff_t>(cursor), 32, a.bend.begin());
    cursor += 32;
    rows(a.yaw); rows(a.scenery);
    for (const auto& row : a.scenery)
        for (auto layout : row) require(layout < 20, "Invalid mine-cart scenery identity");
    for (auto& layout : a.layouts)
        for (auto& stamp : layout) {
            stamp = {course[cursor], course[cursor + 1], read16(course, cursor + 2)};
            require(stamp.image % 24 == 0 && stamp.image / 24 < 29,
                    "Invalid mine-cart image identity");
            cursor += 4;
        }
    for (; cursor < course.size(); cursor += 5) {
        MineCartSegment s{course[cursor], course[cursor + 1], course[cursor + 2],
                          course[cursor + 3], course[cursor + 4]};
        require(s.pitch < 13 && s.yaw < 3 && s.scenery < 5 && s.bend == 0,
                "Invalid mine-cart course reference");
        require(s.command == 0 || s.command == 0xe0 || s.command == 0xe1 ||
                    s.command == 0xe2 || s.command == 0xff,
                "Unsupported mine-cart course operation");
        a.course.push_back(s);
    }
    // InitTrainGfx: each size has one palette prefix followed by high-nibble
    // first pixels. Zero remains transparent even when the prefix is nonzero.
    std::vector<Bytes> decoded;
    std::vector<std::size_t> offsets;
    cursor = 0;
    std::size_t decoded_offset = 0;
    for (unsigned tile = 0; tile < 29; ++tile)
        for (auto size : sizes) {
            const auto pixels = size * size;
            require(cursor + 1 + (pixels + 1) / 2 <= graphics.size(),
                    "Truncated mine-cart image");
            const auto prefix = graphics[cursor++];
            require((prefix & 15U) == 0, "Invalid mine-cart palette prefix");
            Bytes image(pixels);
            for (unsigned p = 0; p < pixels; ++p) {
                const auto color = (graphics[cursor + p / 2] >> (p % 2 ? 0 : 4)) & 15U;
                image[p] = color ? static_cast<std::uint8_t>(prefix | color) : 0;
            }
            cursor += (pixels + 1) / 2;
            offsets.push_back(decoded_offset);
            decoded_offset += pixels;
            decoded.push_back(std::move(image));
        }
    require(cursor == graphics.size(), "Unexpected mine-cart image tail");
    for (unsigned tile = 0; tile < FF6_ROM_VALUE_MINE_CART_MIRROR_COUNT_0; ++tile)
        for (unsigned size = 0; size < sizes.size(); ++size) {
            auto image = decoded[tile * 12 + size];
            for (unsigned y = 0; y < sizes[size]; ++y)
                std::reverse(image.begin() + y * sizes[size], image.begin() + (y + 1) * sizes[size]);
            // EE/9A9D selects a destination inside the decoded image directory.
            // Replace those placeholders; appending loses the right tunnel wall.
            decoded[(FF6_ROM_VALUE_MINE_CART_MIRROR_DESTINATION_0 + tile) * 12 + size] = std::move(image);
        }
    for (unsigned tile = 0; tile < 29; ++tile)
        for (unsigned size = 0; size < sizes.size(); ++size) {
            const unsigned pointer = read16(directory, tile * 24 + size * 2);
            require(pointer >= 0x2000, "Invalid mine-cart image reference");
            const auto found = std::ranges::find(offsets, pointer - 0x2000U);
            require(found != offsets.end(), "Mine-cart image reference is not an image boundary");
            const auto index = static_cast<std::size_t>(found - offsets.begin());
            require(decoded[index].size() == sizes[size] * sizes[size],
                    "Mine-cart image reference has the wrong size");
            a.images[tile][size] = decoded[index];
        }
    for (unsigned i = 0; i < 256; ++i) a.colors[i] = read16(palette, i * 2) & 32767U;
    a.battle_mosaic.assign(mosaic.begin(), mosaic.end());
    require(a.battle_mosaic.back() == 255, "Mine-cart transition has no terminator");
    a.song = songs[0];
    return a;
}
MineCartAssets load_mine_cart_assets(const ContentPack& pack) {
    return decode_mine_cart_assets(pack.get("mine_cart_course_data"),
        decompress_lzss(pack.get("world_graphics_magitek_train_gfx_cgx_lz")),
        pack.get("world_graphics_magitek_train_tiles_bin"),
        pack.get("world_graphics_magitek_train_pal_pal"), pack.get("mine_cart_battle_mosaic"),
        pack.get("mine_cart_songs"));
}
MineCartRide::MineCartRide(MineCartAssets assets) : assets_(std::move(assets)) {
    load_segment(0);
    load_segment(32);
}
void MineCartRide::load_segment(unsigned half) {
    require(next_ < assets_.course.size() && (half == 0 || half == 32),
            "Mine-cart course exhausted without its terminator");
    const auto s = assets_.course[next_++];
    std::copy(assets_.rise[s.pitch].begin(), assets_.rise[s.pitch].end(), rise_.begin() + half);
    std::copy(assets_.pitch[s.pitch].begin(), assets_.pitch[s.pitch].end(), pitch_.begin() + half);
    std::copy(assets_.yaw[s.yaw].begin(), assets_.yaw[s.yaw].end(), yaw_.begin() + half);
    std::copy(assets_.scenery[s.scenery].begin(), assets_.scenery[s.scenery].end(), scenery_.begin() + half);
}
bool MineCartRide::battle_ready() const {
    return battle_ && transition_ == assets_.battle_mosaic.size();
}
bool MineCartRide::take_battle_sound() {
    const bool result = battle_sound_;
    battle_sound_ = false;
    return result;
}
void MineCartRide::advance() {
    if (complete_ || battle_ready()) return;
    if (battle_) { ++transition_; return; }
    const bool update = interval_ == 0;
    interval_ = static_cast<std::uint8_t>((interval_ + 1U) % FF6_ROM_VALUE_MINE_CART_UPDATE_PERIOD_0);
    if (!update) return;
    if (fading_) {
        if (brightness_) --brightness_;
        else { complete_ = true; return; }
    } else if (brightness_ < 15) ++brightness_;
    if (sample_ == 0 || sample_ == 32) {
        const auto command = assets_.course.at(next_).command;
        load_segment(32U - sample_);
        if (command >= 0xe0 && command <= 0xe2) {
            battle_ = command == 0xe0 ? FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E0_0
                : command == 0xe1 ? FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E1_0
                : FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E2_0;
            transition_ = 0;
            battle_sound_ = true;
        } else if (command == 255) fading_ = true;
    }
    draw_tunnel();
    sample_ = static_cast<std::uint8_t>((sample_ + 1U) & 63U);
}
void MineCartRide::resume_after_battle() {
    require(battle_ready(), "Mine-cart battle has not been admitted");
    // MagitekTrain rehydrates the two neighboring course segments, retaining
    // the current sample. The encounter command itself must not fire again.
    --next_;
    load_segment(sample_ & 32U);
    load_segment(32U - (sample_ & 32U));
    brightness_ = interval_ = 0;
    battle_.reset();
    transition_ = 0;
}
void MineCartRide::draw_tunnel() {
    picture_.fill(0);
    std::array<std::uint8_t, 64> centers_x{}, centers_y{};
    const auto start_height = wrap8(unsigned{pitch_[sample_]} + FF6_ROM_VALUE_MINE_CART_TUNNEL_VERTICAL_1);
    const int tilt = signed_byte(wrap8(unsigned{FF6_ROM_VALUE_MINE_CART_TUNNEL_VERTICAL_0} - start_height));
    auto rise_origin = rise_[sample_];
    const int bend = signed_byte(yaw_[sample_]);
    std::uint8_t bend_sign = bend < 0 ? 1 : 0;
    for (unsigned i = 0; i < FF6_ROM_VALUE_MINE_CART_SAMPLE_WINDOW_0; ++i) {
        const auto at = (unsigned{sample_} + i) & 63U;
        const auto delta = wrap8(unsigned{rise_[at]} - rise_origin);
        centers_y[at] = wrap8(static_cast<unsigned>(int(start_height) +
            scaled(tilt, assets_.perspective[63 - i]) - int(delta)));
        const auto following = (at + 1U) & 63U;
        if (following == 0 || following == 32)
            rise_origin = wrap8(unsigned{rise_[following]} - delta);
        // The first negative curvature sample changes the retained sign for
        // the remaining samples too (EE/23F0). Do not independently re-sign
        // each product: that bends the tunnel in the opposite direction.
        const int curvature = signed_byte(assets_.bend[i]);
        if (curvature < 0) bend_sign = wrap8(unsigned{bend_sign} - 1U);
        const auto amount = scaled(bend < 0 ? -bend : bend, static_cast<unsigned>(
            curvature < 0 ? -curvature : curvature));
        centers_x[at] = wrap8(static_cast<unsigned>(FF6_ROM_VALUE_MINE_CART_TUNNEL_HORIZONTAL_0 + (bend_sign ? -amount : amount)));
    }
    constexpr std::array<unsigned, 22> image_sizes{
#include "rom-materials/mine_cart_depth_groups.inc"
    };
    constexpr unsigned layer_count = static_cast<unsigned>(image_sizes.size());
    for (unsigned layer = 0; layer < layer_count; ++layer) {
        const auto at = (unsigned{sample_} + FF6_ROM_VALUE_MINE_CART_SAMPLE_WINDOW_1 - layer) & 63U;
        const auto scale = assets_.perspective[layer_count - 1U - layer];
        const auto size_id = image_sizes[layer], size = sizes[size_id];
        for (const auto& stamp : assets_.layouts[scenery_[at]]) {
            if (!stamp.x && !stamp.y) continue;
            const int x = int(centers_x[at]) + scaled(signed_byte(wrap8(unsigned{stamp.x} - centers_x[at])), scale);
            const int y = int(centers_y[at]) + scaled(signed_byte(wrap8(unsigned{stamp.y} - centers_y[at])), scale);
            const auto& image = assets_.images[stamp.image / 24][size_id];
            require(image.size() == size * size, "Missing mine-cart depth image");
            for (unsigned row = 0; row < size; ++row)
                for (unsigned col = 0; col < size; ++col) {
                    const int px = x - int((size - 1) / 2) + int(col), py = y - int(row);
                    const auto color = image[row * size + col];
                    // Authored tunnel viewport, before the camera enlarges it.
                    if (color && px >= FF6_ROM_VALUE_MINE_CART_TEXTURE_LEFT_0 &&
                        px < FF6_ROM_VALUE_MINE_CART_TEXTURE_LEFT_0 + FF6_ROM_VALUE_MINE_CART_TEXTURE_WIDTH_0 &&
                        py >= FF6_ROM_VALUE_MINE_CART_TEXTURE_ROWS_1 && py < FF6_ROM_VALUE_MINE_CART_TEXTURE_ROWS_0)
                        picture_[static_cast<unsigned>(py) * 128U + static_cast<unsigned>(px)] = color;
                }
        }
    }
}
RasterScene MineCartRide::scene() const {
    RasterScene result;
    result.intensity.assign(224, brightness_);
    AffineLayer image;
    image.columns = image.rows = 128;
    image.colors = assets_.colors;
    image.cells.assign(picture_.begin(), picture_.end());
    image.tiles.resize(256);
    for (unsigned i = 0; i < 256; ++i) image.tiles[i].fill(static_cast<std::uint8_t>(i));
    image.lines.resize(224);
    const unsigned block = battle_ && transition_
        ? (assets_.battle_mosaic[transition_ - 1] >> 4U) + 1U : 1U;
    for (unsigned y = 0; y < 224; ++y) {
        auto& line = image.lines[y];
        line.enabled = y >= FF6_ROM_VALUE_MINE_CART_VISIBLE_ROWS_1 && y < FF6_ROM_VALUE_MINE_CART_VISIBLE_ROWS_0;
        line.left = FF6_ROM_VALUE_APPROACH_VIEWPORT_0; line.right = FF6_ROM_VALUE_APPROACH_VIEWPORT_1 + 1;
        // Extract static camera sampling from its source coordinates and
        // cosine sample; fixed-point truncation belongs to that projection.
        line.step_x = FF6_ROM_VALUE_MINE_CART_CAMERA_PROJECTION_0;
        line.source_x = FF6_ROM_VALUE_MINE_CART_CAMERA_PROJECTION_1;
        line.source_y = FF6_ROM_VALUE_MINE_CART_CAMERA_PROJECTION_2 +
            int(y - y % block + 1) * FF6_ROM_VALUE_MINE_CART_CAMERA_PROJECTION_0;
        line.sample_width = static_cast<std::uint8_t>(block);
    }
    result.affine_layers.push_back(std::move(image));
    return result;
}
FieldBattleRequest select_mine_cart_battle(const FieldBattleData& data, std::uint8_t group,
                                           std::uint8_t& field_random) {
    require(group == FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E0_0 ||
                group == FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E1_0 ||
                group == FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E2_0, "Invalid mine-cart encounter group");
    // TrainCmd_e0/e1 share their authored probability; the final command
    // uses its first formation and owns its music policy independently.
    unsigned choice = 0;
    if (group != FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E2_0) {
        choice = data.random[field_random] >= FF6_ROM_VALUE_MINE_CART_ENCOUNTER_THRESHOLD_0 ? 1U : 0U;
        field_random = wrap8(unsigned{field_random} + 1);
    }
    FieldBattleRequest result;
    result.formation = data.event_groups[group][choice];
    result.background = FF6_ROM_VALUE_MINE_CART_BATTLE_BACKGROUND_0;
    result.continue_music = group != FF6_ROM_VALUE_MINE_CART_ENCOUNTER_E2_0;
    result.mosaic = result.sound = false; // The tunnel owns its transition.
    return result;
}
} // namespace ff6
