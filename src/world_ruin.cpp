#include "ff6/world_ruin.hpp"
#include "rom-materials/ruin_fire_schedule.macros.inc"
#include "rom-materials/ruin_land_timing.macros.inc"
#include "rom-materials/ruin_first_land_velocity.macros.inc"
#include "rom-materials/ruin_second_land_velocity.macros.inc"
#include "rom-materials/ruin_land_origins.macros.inc"
#include "rom-materials/ruin_final_fade_period.macros.inc"
#include "rom-materials/ruin_spark_gate_mask.macros.inc"
#include "rom-materials/ruin_crack_gate_mask.macros.inc"
#include "rom-materials/ruin_crack_position_mask.macros.inc"
#include "rom-materials/ruin_spark_spread_mask.macros.inc"
#include "rom-materials/ruin_spark_origins.macros.inc"
#include "rom-materials/ruin_object_arithmetic.macros.inc"
#include "rom-materials/ruin_palette_sources.macros.inc"
#include "rom-materials/ruin_object_palette_banks.macros.inc"
#include "rom-materials/ruin_terrain_operand.macros.inc"
#include <algorithm>
#include <map>

namespace ff6 {
namespace {
// Two consecutive, aligned object banks share one validation interval. These
// are source-buffer/record constraints; palette destinations remain ROM inputs.
static_assert(FF6_ROM_VALUE_RUIN_OBJECT_PALETTE_BANKS_1 ==
              FF6_ROM_VALUE_RUIN_OBJECT_PALETTE_BANKS_0 + 32);
static_assert((FF6_ROM_VALUE_RUIN_OBJECT_PALETTE_BANKS_0 - 0x3100) % 64 == 0);
constexpr unsigned ruin_particles_end = FF6_ROM_VALUE_RUIN_PHASE_WAITS_0 +
    FF6_ROM_VALUE_RUIN_PHASE_WAITS_1 + FF6_ROM_VALUE_RUIN_PHASE_WAITS_2;
constexpr unsigned ruin_palette_fade_start = FF6_ROM_VALUE_RUIN_PHASE_WAITS_0 +
    FF6_ROM_VALUE_RUIN_PHASE_WAITS_1;
constexpr unsigned ruin_brightness_fade_start = ruin_particles_end + 2;
constexpr unsigned ruin_complete = ruin_brightness_fade_start + FF6_ROM_VALUE_RUIN_PHASE_WAITS_3;
}

WorldRuinAssets load_world_ruin_assets(const ContentPack& pack) {
    WorldRuinAssets a;
    const auto graphics = decompress_lzss(pack.get("cutscene_assets_ruin_cutscene_bin_lz"));
    if (graphics.size() != 0x24c0) throw std::invalid_argument("Invalid ruin graphics extent");
    auto objects = decode_tiles(ByteView(graphics).subspan(0x16e0), 4);
    // ClearVRAM supplies the transparent trailing pieces of 16x16 objects.
    objects.resize(128);
    auto& terrain = a.terrain;
    terrain.columns = terrain.rows = 64;
    terrain.cells.resize(64 * 64);
    terrain.tiles = decode_tiles(ByteView(graphics).first(0xfe0), 4);
    terrain.lines.resize(224);
    terrain.palettes.resize(1);
    terrain.operand = bool{FF6_ROM_VALUE_RUIN_TERRAIN_OPERAND_0};
    for (auto& line : terrain.lines) line.offset_y = 1;
    // LoadWorldOfRuinGfx writes only the first 28 rows of the top-left
    // screen page. Keep the remaining cleared cells; do not stretch the map.
    for (unsigned i = 0; i < 0x380; ++i) {
        const auto cell = read16(graphics, 0xfe0 + i * 2);
        if ((cell & 1023U) >= terrain.tiles.size() || (cell & 0x1c00U))
            throw std::invalid_argument("Unprepared ruin background tile/palette");
        terrain.cells[(i / 32) * 64 + i % 32] =
            {cell & 1023U, 0, static_cast<std::uint16_t>((cell & 0x2000U) ? 11 : 7),
             (cell & 0x4000U) != 0, (cell & 0x8000U) != 0};
    }
    const auto colors = pack.get("ruin_palettes"), data = pack.get("ruin_animation_data");
    const auto fires = pack.get("ruin_fire_positions"), cracks = pack.get("ruin_crack_positions");
    const auto rng = pack.get("encounter_rng_tbl_bin");
    if (colors.size() != 128 || data.size() != 200 || fires.size() != 20 ||
        cracks.size() != 8 || rng.size() != 256)
        throw std::invalid_argument("Invalid ruin presentation resource extent");
    for (unsigned p = 0; p < 4; ++p)
        for (unsigned c = 0; c < 16; ++c) a.palettes[p][c] = read16(colors, p * 32 + c * 2) & 0x7fff;
    for (unsigned i = 0; i < 10; ++i) a.fire_positions[i] = {fires[i * 2], fires[i * 2 + 1]};
    for (unsigned i = 0; i < 4; ++i) a.crack_positions[i] = {cracks[i * 2], cracks[i * 2 + 1]};
    std::ranges::copy(rng, a.random.begin());
    // Resolve the five reviewed data streams to ordinary resource identities.
    // These offsets describe data, never executable entry points.
    constexpr std::array<unsigned, 5> offsets{0, 0x2c, 0x60, 0x7c, 0x9f};
    std::map<unsigned, std::size_t> groups;
    for (unsigned stream = 0; stream < offsets.size(); ++stream) {
        std::size_t at = offsets[stream];
        for (;;) {
            const auto record = slice(data, at, 3); at += 3;
            const unsigned pointer = read16(record, 0);
            if (pointer < 0x707a || pointer >= 0x7142)
                throw std::invalid_argument("Unresolved ruin sprite group");
            if (!groups.contains(pointer)) {
                std::size_t position = pointer - 0x707a;
                const auto id = a.sprites.size();
                a.sprites.push_back(decode_cutscene_sprite_group(data, position, objects));
                for (const auto& s : a.sprites.back())
                    if (s.palette != ((FF6_ROM_VALUE_RUIN_OBJECT_PALETTE_BANKS_0 - 0x3100) / 32) &&
                        s.palette != ((FF6_ROM_VALUE_RUIN_OBJECT_PALETTE_BANKS_1 - 0x3100) / 32))
                        throw std::invalid_argument("Unprepared ruin sprite palette");
                groups.emplace(pointer, id);
            }
            a.animations[stream].push_back({groups.at(pointer), record[2]});
            if (record[2] >= 254) break;
        }
    }
    return a;
}
WorldRuin::WorldRuin(WorldRuinAssets assets) : assets_(std::move(assets)) {
    for (const auto& animation : assets_.animations)
        if (animation.empty() || animation.back().duration < 254)
            throw std::invalid_argument("Unterminated ruin animation");
    spawn(0, FF6_ROM_VALUE_RUIN_LAND_ORIGINS_0, FF6_ROM_VALUE_RUIN_LAND_ORIGINS_1);
    spawn(1, FF6_ROM_VALUE_RUIN_LAND_ORIGINS_2, FF6_ROM_VALUE_RUIN_LAND_ORIGINS_3);
}
void WorldRuin::spawn(unsigned animation, int x, int y) {
    effects_.push_back({animation, x * 256, y * 256,
                       {0, assets_.animations.at(animation)[0].duration}});
}
std::uint8_t WorldRuin::random() {
    // RuinRandom's two-lookup visual-noise stream. Subpixel task/carry
    // alignment of individual particles remains a later comparison target.
    random_index_ = wrap8(unsigned{random_index_} + 1);
    const auto first = assets_.random[random_index_];
    return wrap8(unsigned{first} + assets_.random[first]);
}
bool WorldRuin::advance() {
    if (complete()) return false;
    // State_01/02/03 own particles; field timers and input remain suspended.
    if (updates_ > 0 && updates_ <= ruin_particles_end) {
        if ((random() & FF6_ROM_VALUE_RUIN_SPARK_GATE_MASK_0) == 0) {
            const int x = FF6_ROM_VALUE_RUIN_SPARK_ORIGINS_0 +
                (random() & FF6_ROM_VALUE_RUIN_SPARK_SPREAD_MASK_0);
            const int y = FF6_ROM_VALUE_RUIN_SPARK_ORIGINS_1 +
                (random() & FF6_ROM_VALUE_RUIN_SPARK_SPREAD_MASK_0);
            spawn(2, x, y);
        }
        if ((random() & FF6_ROM_VALUE_RUIN_CRACK_GATE_MASK_0) == 0) {
            const auto [x, y] = assets_.crack_positions[random() & FF6_ROM_VALUE_RUIN_CRACK_POSITION_MASK_0];
            spawn(3, x, y);
        }
    }
    if (updates_ >= FF6_ROM_VALUE_RUIN_FIRE_SCHEDULE_0 &&
        fires_ < assets_.fire_positions.size() &&
        (updates_ - FF6_ROM_VALUE_RUIN_FIRE_SCHEDULE_0) % FF6_ROM_VALUE_RUIN_FIRE_SCHEDULE_1 == 0) {
        const auto [x, y] = assets_.fire_positions[fires_++];
        spawn(4, x, y);
        if (fires_ == assets_.fire_positions.size())
            colors_[1] = assets_.palettes[(FF6_ROM_VALUE_RUIN_PALETTE_SOURCES_3 - 0x717d) / 32];
    }
    if (updates_ == ruin_palette_fade_start) for (auto& fade : fades_) fade = CutscenePaletteFade{FF6_ROM_VALUE_RUIN_FINAL_FADE_PERIOD_0};
    const CutscenePalette black{};
    for (unsigned p = 0; p < 3; ++p)
        fades_[p].advance(colors_[p], updates_ >= ruin_palette_fade_start ? black :
            assets_.palettes[((p == 0 ? FF6_ROM_VALUE_RUIN_PALETTE_SOURCES_0 :
                p == 1 ? FF6_ROM_VALUE_RUIN_PALETTE_SOURCES_1 :
                         FF6_ROM_VALUE_RUIN_PALETTE_SOURCES_2) - 0x717d) / 32]);
    std::erase_if(effects_, [](const Effect& e) {
        return (e.animation == 2 || e.animation == 3) && e.cursor.wait == 254;
    });
    for (auto& e : effects_) {
        // Both source creators share the extracted initial delay. The land
        // task lifetime includes that delay; velocities retain eight fractional bits.
        if (e.animation < 2 && updates_ >= FF6_ROM_VALUE_RUIN_LAND_TIMING_1 &&
            updates_ < FF6_ROM_VALUE_RUIN_LAND_TIMING_0) {
            e.x += e.animation == 0 ? FF6_ROM_VALUE_RUIN_FIRST_LAND_VELOCITY_0
                                    : FF6_ROM_VALUE_RUIN_SECOND_LAND_VELOCITY_0;
            e.y += e.animation == 0 ? FF6_ROM_VALUE_RUIN_FIRST_LAND_VELOCITY_1
                                    : FF6_ROM_VALUE_RUIN_SECOND_LAND_VELOCITY_1;
        }
        advance_cutscene_animation(e.cursor, assets_.animations[e.animation]);
    }
    ++updates_;
    return true;
}
RasterScene WorldRuin::scene() const {
    RasterScene out;
    out.backdrop = colors_[0][0];
    out.intensity.assign(224, static_cast<std::uint8_t>(complete() ? 0 :
                         updates_ > ruin_brightness_fade_start ? ruin_complete - updates_ : 15));
    out.layers.push_back(assets_.terrain);
    out.layers[0].palettes[0] = colors_[0];
    out.objects.emplace();
    out.objects->lines.resize(224);
    // Publish particles before the land plates, preserving the original task
    // ordering. Blend arithmetic and the terrain operand come from ROM inputs.
    for (unsigned priority = 0; priority < 2; ++priority)
        for (const auto& effect : effects_) {
            if ((effect.animation < 2 ? 1U : 0U) != priority) continue;
            for (const auto& piece : assets_.sprites.at(
                     assets_.animations[effect.animation][effect.cursor.frame].resource)) {
                auto image = piece.image;
                image.x += effect.x / 256;
                image.y += effect.y / 256 - 1;
                image.colors = colors_[piece.palette ==
                    ((FF6_ROM_VALUE_RUIN_OBJECT_PALETTE_BANKS_0 - 0x3100) / 32) ? 1 : 2];
                image.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_RUIN_OBJECT_ARITHMETIC_0),
                             ColorBlend::none};
                out.objects->sprites.push_back(std::move(image));
            }
        }
    return out;
}
} // namespace ff6
