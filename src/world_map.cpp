#include "ff6/world_map.hpp"
#include "rom-materials/encounter_counter_cap.macros.inc"
#include "rom-materials/encounter_index_step.macros.inc"
#include "rom-materials/formation_index_step.macros.inc"
#include "rom-materials/random_battle_cutoffs.macros.inc"
#include "rom-materials/world_walk_clock_step.macros.inc"
#include "rom-materials/world_walk_clock_threshold.macros.inc"
#include "rom-materials/world_walk_phase_mask.macros.inc"
#include "rom-materials/world_minimap_palette_copy.macros.inc"
#include "rom-materials/world_chocobo_first_tile_origin.macros.inc"
#include "rom-materials/world_chocobo_second_tile_origin.macros.inc"
#include <algorithm>

namespace ff6 {
DoomGazeLocations initialize_doom_gaze_locations(std::uint8_t& random_index,
    const std::array<std::uint8_t, 256>& random) {
    random_index = wrap8(unsigned{random_index} + 3U);
    DoomGazeLocations out;
    // EE/86A6 increments X between each cell's X and Y lookup, but not after
    // Y. Adjacent cells intentionally share a lookup. Only the first two of
    // the four generated positions are checked by EE/6F1D.
    for (unsigned i = 0; i < out.size(); ++i) {
        const auto index = wrap8(unsigned{random_index} + i);
        out[i] = {static_cast<std::uint8_t>(random[index] & 63U),
                  static_cast<std::uint8_t>(random[wrap8(unsigned{index} + 1U)] >> 2U)};
    }
    return out;
}
bool encounters_doom_gaze(const WorldState& world, const DoomGazeLocations& locations,
                          Buttons buttons, bool defeated) {
    if (world.map != 1 || !world.vehicle || world.airship_transition || defeated ||
        !held(buttons, Button::a)) return false;
    const FieldTilePosition cell{wrap8(world.x >> 6U), wrap8(world.y >> 6U)};
    return std::ranges::find(locations, cell) != locations.end();
}
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
unsigned wrapped(int n, unsigned mask) {
    return static_cast<unsigned>(n) & mask;
}
void modify_terrain(Bytes& terrain, ByteView data, ByteView pointers, unsigned map,
                    const FieldFlags& flags) {
    require(pointers.size() == 30, "Invalid world modification directory");
    const auto base = read24(pointers, 0);
    const auto begin = read24(pointers, map * 3) - base;
    const auto end = read24(pointers, map * 3 + 3) - base;
    require(begin <= end && end <= data.size() && (end - begin) % 4 == 0,
            "Invalid world modification range");
    for (auto p = begin; p < end; p += 4) {
        const auto flag = read16(data, p) & 32767U;
        require(flag < flags.story.size(), "Invalid world modification flag");
        if (!flags.story[flag])
            continue;
        const auto at = read16(data, p + 2);
        require(at + 3U <= data.size(), "Truncated world modification");
        const unsigned x = data[at], y = data[at + 1], width = data[at + 2] >> 4U,
                       height = data[at + 2] & 15U;
        require(width && height && at + 3U + width * height <= data.size() && x + width <= 256 &&
                    y + height <= 256,
                "Invalid world modification rectangle");
        for (unsigned row = 0; row < height; ++row)
            std::copy_n(data.begin() + at + 3 + row * width, width,
                        terrain.begin() + (y + row) * 256 + x);
    }
}
} // namespace
WorldAssets load_world_assets(const ContentPack& pack, std::uint8_t map, const FieldFlags& flags) {
    require(map < 3, "Invalid world identity");
    const auto prefix = std::string("world_graphics_world_") + std::to_string(map + 1);
    const auto graphics = decompress_lzss(pack.get(prefix + "_gfx_4bpp_lz"));
    WorldAssets out;
    out.terrain = decompress_lzss(pack.get(prefix + "_tilemap_bin_lz"));
    require(out.terrain.size() == (map == 2 ? 16384U : 65536U) && graphics.size() == 0x2480,
            "Invalid world terrain/graphics extent");
    if (map < 2) modify_terrain(out.terrain, pack.get("data_cef600_world_modifications"),
                   pack.get("ptr_table_eeb260_world_and_vehicle_events"), map, flags);
    const auto properties = pack.get("world_tile_properties");
    require(properties.size() == 1024, "Invalid world terrain properties");
    out.tiles.resize(256);
    for (unsigned i = 0; i < 256; ++i) {
        std::copy_n(graphics.begin() + i * 4, 4, out.cells[i].begin());
        if (map < 2) out.properties[i] = read16(properties, map * 512 + i * 2);
        const unsigned palette = (graphics[0x2400 + i / 2] >> ((i % 2) * 4)) & 15U;
        for (unsigned pixel = 0; pixel < 64; ++pixel)
            out.tiles[i][pixel] = static_cast<std::uint8_t>(
                palette * 16 + ((graphics[1024 + i * 32 + pixel / 2] >> ((pixel % 2) * 4)) & 15U));
    }
    // The underwater palette is one compressed image containing both owners.
    const auto trench_palette = map == 2 ? decompress_lzss(pack.get(prefix + "_pal_pal_lz"))
                                        : Bytes{};
    const auto colors = map == 2 ? slice(trench_palette, 0, 256)
                                : pack.get(prefix + "_bg_pal_pal"),
               object = map == 2 ? slice(trench_palette, 256, 256)
                                : pack.get(prefix + "_sprite_pal_pal");
    require(colors.size() == 256 && object.size() == 256, "Invalid world palette extent");
    for (unsigned i = 0; i < 128; ++i)
        out.colors[i] = read16(colors, i * 2) & 32767U;
    for (unsigned i = 0; i < 128; ++i)
        out.object_colors[i / 16][i % 16] = read16(object, i * 2) & 32767U;
    if (!map && flags.story[0x9e])
        // FixMinimap's selected addresses normalize to color indices. Palette
        // width is format metadata; all four selected colors come from the ROM.
        for (auto i : {unsigned{FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_COPY_1},
                       unsigned{FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_COPY_2},
                       unsigned{FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_COPY_3}})
            out.object_colors[i / 16][i % 16] =
                out.object_colors[FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_COPY_0 / 16]
                                 [FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_COPY_0 % 16];
    if (map < 2) out.minimap = decode_tiles(decompress_lzss(pack.get("world_graphics_minimap_" +
                                                        std::to_string(map + 1) + "_gfx_4bpp_lz")),
                               4);
    out.misc =
        decode_tiles(decompress_lzss(pack.get("world_graphics_world_misc_sprite_gfx_4bpp_lz")), 4);
    const auto esper_colors = pack.get("world_graphics_world_esper_terra_pal_pal");
    require(esper_colors.size() == 32, "Invalid world Esper palette");
    for (unsigned i = 0; i < 16; ++i)
        out.esper_colors[i] = read16(esper_colors, i * 2) & 32767U;
    const auto directions = pack.get("world_airship_direction_frames"),
               projection = pack.get("world_parked_airship_projection");
    require(directions.size() == out.airship_direction_frames.size() &&
            projection.size() == out.parked_airship_projection.size(), "Invalid airship sprite tables");
    std::ranges::copy(directions, out.airship_direction_frames.begin());
    std::ranges::copy(projection, out.parked_airship_projection.begin());
    const auto vehicle_events = pack.get("ptr_table_eeb260_world_and_vehicle_events");
    out.airship_ground_event = read24(vehicle_events, 15);
    out.airship_deck_event = read24(vehicle_events, 9);
    out.phoenix_cave_event = read24(vehicle_events, 18);
    out.kefka_tower_event = read24(vehicle_events, 21);
    out.gogo_cave_event = read24(vehicle_events, 24);
    out.doom_gaze_event = read24(vehicle_events, 27);
    const auto airship_songs = pack.get("world_airship_songs"),
               transitions = pack.get("world_airship_transition_steps");
    require(airship_songs.size() == out.airship_songs.size() &&
            transitions.size() == out.airship_transition_steps.size(), "Invalid airship travel tables");
    std::ranges::copy(airship_songs, out.airship_songs.begin());
    std::ranges::copy(transitions, out.airship_transition_steps.begin());
    out.approach_tiles = decode_tiles(decompress_lzss(pack.get("world_graphics_vector_approach_gfx_4bpp_lz")), 4);
    out.approach_cells = decompress_lzss(pack.get("world_graphics_vector_approach_tiles_scr_lz"));
    const auto approach_colors = pack.get("world_graphics_vector_approach_pal_pal");
    require(out.approach_cells.size() == 2048 && approach_colors.size() == 32,
            "Invalid Vector approach assets");
    for (unsigned i = 0; i < 16; ++i) out.approach_colors[i] = read16(approach_colors, i * 2) & 32767U;
    out.airship_tiles = decode_tiles(decompress_lzss(pack.get(
        map ? "world_graphics_airship_2_gfx_4bpp_lz" : "world_graphics_airship_1_gfx_4bpp_lz")), 4);
    // Daryl's memory uses the Falcon over the earlier world's terrain. The
    // vehicle F4 command changes the sprite owner without changing the map.
    out.falcon_tiles = map ? out.airship_tiles : decode_tiles(decompress_lzss(
        pack.get("world_graphics_airship_2_gfx_4bpp_lz")), 4);
    const auto falcon_colors = pack.get("world_graphics_world_2_sprite_pal_pal");
    for (unsigned i = 0; i < 16; ++i)
        out.falcon_colors[i] = read16(falcon_colors, i * 2) & 32767U;
    out.characters = load_field_sprites(pack);
    const auto frames = pack.get("world_sprite_frames"),
               walk = pack.get("world_character_walk_frames");
    require(frames.size() == 0x14ae && walk.size() == 16, "Invalid world character frames");
    out.frames.assign(frames.begin(), frames.end());
    const auto smoke = pack.get("world_airship_smoke_parameters");
    require(smoke.size() == 32768 * 6, "Invalid smoking airship parameters");
    out.airship_smoke_parameters.assign(smoke.begin(), smoke.end());
    // AirshipCrash uses the low bits of existing terrain graphics as jitter.
    // This is already a data resource; no additional executable is packaged.
    const auto crash_noise = slice(pack.get("map_graphics_map_gfx_4bpp"), 0x2500, 1024);
    out.crash_jitter.assign(crash_noise.begin(), crash_noise.end());
    const auto splashes = pack.get("world_falcon_splash_frames"),
               pigeon = pack.get("world_pigeon_frames");
    require(splashes.size() == out.falcon_splash_frames.size() &&
            pigeon.size() == out.pigeon_frames.size(), "Invalid Falcon presentation tables");
    std::ranges::copy(splashes, out.falcon_splash_frames.begin());
    std::ranges::copy(pigeon, out.pigeon_frames.begin());
    std::ranges::copy(walk, out.walk_frames.begin());
    out.entrances = load_field_entrances(pack, map);
    const auto triggers = pack.get("data_c40000_event_triggers");
    const auto begin = read16(triggers, map * 2), end = read16(triggers, map * 2 + 2);
    require(begin <= end && end <= triggers.size() && (end - begin) % 5 == 0,
            "Invalid world triggers");
    for (unsigned at = begin; at < end; at += 5)
        out.triggers.push_back({{triggers[at], triggers[at + 1]}, read24(triggers, at + 2)});
    out.battles = load_field_battle_data(pack);
    const auto groups = pack.get("encounter_world_battle_group_bin"),
               rates = pack.get("data_cf5800_battle_groups"),
               rules = pack.get("world_encounter_rules"), songs = pack.get("world_foot_songs");
    require(groups.size() == 512 && rates.size() == 256 && rules.size() == 64 && songs.size() == 4,
            "Invalid world encounter/song tables");
    std::ranges::copy(groups, out.groups.begin());
    std::copy_n(rates.begin(), 128, out.rates.begin());
    std::ranges::copy(rules, out.encounter_rules.begin());
    std::ranges::copy(songs, out.songs.begin());
    for (unsigned i = 16; i < 32; ++i)
        require(out.encounter_rules[i] < 4, "Invalid terrain encounter selector");
    const auto zoom = pack.get("world_battle_zoom");
    require(zoom.size() == 68, "Invalid world battle transition");
    std::ranges::copy(zoom, out.battle_zoom.begin());
    const auto sine = pack.get("data_effef1_world_sine"),
               particles = pack.get("world_castle_particles"),
               choco = pack.get("world_chocobo_songs");
    require(sine.size() >= 270 && particles.size() == 36 && choco.size() == 4,
            "Invalid world animation data");
    out.sine.assign(sine.begin(), sine.end());
    out.castle_particles.assign(particles.begin(), particles.end());
    std::ranges::copy(choco, out.chocobo_songs.begin());
    const auto trench = pack.get("world_trench_songs");
    require(trench.size() == 2, "Invalid underwater songs");
    std::ranges::copy(trench, out.trench_songs.begin());
    out.animation =
        decode_tiles(decompress_lzss(pack.get("world_graphics_world_anim_sprite_gfx_4bpp_lz")), 4);
    out.chocobo_tiles.resize(512);
    const auto choco1 = decode_tiles(
                   decompress_lzss(pack.get("world_graphics_world_choco_1_gfx_4bpp_lz")), 4),
               choco2 = decode_tiles(
                   decompress_lzss(pack.get("world_graphics_world_choco_2_gfx_4bpp_lz")), 4);
    // Each source occupies the remainder of its tile bank. Extract its chosen
    // placement and derive the available capacity from the bank format.
    require(choco1.size() <= 256U - FF6_ROM_VALUE_WORLD_CHOCOBO_FIRST_TILE_ORIGIN_0 &&
            choco2.size() <= 512U - FF6_ROM_VALUE_WORLD_CHOCOBO_SECOND_TILE_ORIGIN_0,
            "Invalid chocobo tile extent");
    std::ranges::copy(choco1, out.chocobo_tiles.begin() + FF6_ROM_VALUE_WORLD_CHOCOBO_FIRST_TILE_ORIGIN_0);
    std::ranges::copy(choco2, out.chocobo_tiles.begin() + FF6_ROM_VALUE_WORLD_CHOCOBO_SECOND_TILE_ORIGIN_0);
    out.sky_tiles =
        decode_tiles(decompress_lzss(pack.get("world_graphics_world_backdrop_gfx_4bpp_lz")), 4);
    out.sky_cells = decompress_lzss(pack.get("world_graphics_world_backdrop_tiles_scr_lz"));
    const auto dismount = pack.get("world_chocobo_dismount_frames");
    require(dismount.size() == 5 && out.sky_cells.size() == 4096,
            "Invalid vehicle presentation data");
    out.dismount_frames.assign(dismount.begin(), dismount.end());
    return out;
}
std::uint16_t world_tile_properties(const WorldAssets& a, const WorldState& w, int dx, int dy) {
    require(a.terrain.size() == 65536, "Incomplete world terrain");
    return a.properties[a.terrain[wrapped(int(w.y / 16) + dy, 255) * 256 +
                                  wrapped(int(w.x / 16) + dx, 255)]];
}
bool advance_world_walk(const WorldAssets& a, WorldState& w, Buttons buttons) {
    bool started = false;
    if (!w.remaining) {
        w.dx = w.dy = 0;
        const auto here = world_tile_properties(a, w);
        w.forest = (here & 32U) != 0;
        std::optional<unsigned> direction;
        if (held(buttons, Button::right))
            direction = 1;
        else if (held(buttons, Button::left))
            direction = 3;
        else if (held(buttons, Button::down))
            direction = 2;
        else if (held(buttons, Button::up))
            direction = 0;
        if (direction) {
            w.facing = static_cast<std::uint8_t>(*direction);
            constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
            const auto target = world_tile_properties(a, w, dx[*direction], dy[*direction]);
            if (!(target & 16U)) {
                w.dx = static_cast<std::int8_t>(dx[*direction]);
                w.dy = static_cast<std::int8_t>(dy[*direction]);
                w.remaining = 16;
                started = true;
                // Entering forest remains opaque until the tile boundary;
                // leaving it restores the lower body immediately.
                if (!(target & 32U))
                    w.forest = false;
            }
        }
    }
    if (w.remaining) {
        w.x = static_cast<std::uint16_t>(wrapped(int(w.x) + w.dx, 4095));
        w.y = static_cast<std::uint16_t>(wrapped(int(w.y) + w.dy, 4095));
        --w.remaining;
        w.walk_clock = static_cast<std::uint8_t>(w.walk_clock + FF6_ROM_VALUE_WORLD_WALK_CLOCK_STEP_0);
        if (w.walk_clock >= FF6_ROM_VALUE_WORLD_WALK_CLOCK_THRESHOLD_0) {
            w.walk_clock = 0;
            w.walk_phase = (w.walk_phase + 1U) & unsigned{FF6_ROM_VALUE_WORLD_WALK_PHASE_MASK_0};
        }
    }
    ++w.clock;
    return started;
}
std::optional<FieldBattleRequest> select_world_encounter(const WorldAssets& a, const WorldState& w,
                                                         FieldBattleRandom& rng,
                                                         std::uint16_t& counter,
                                                         std::uint8_t equipment, bool music,
                                                         const std::bitset<512>& history) {
    require(w.map < 2, "Invalid encounter world");
    const auto prop = world_tile_properties(a, w);
    if (!(prop & 64U))
        return {};
    const auto terrain = (prop >> 8U) & 7U;
    require(a.encounter_rules[16 + terrain] < 4 && a.encounter_rules[24 + terrain] < 4,
            "Invalid terrain encounter selector");
    // The original sector remains the reload position until a menu or battle
    // return. Preserve this observable quirk independently of the live camera.
    const auto sector =
        unsigned{w.map} * 64 + (w.encounter_origin.y / 32) * 8 + w.encounter_origin.x / 32;
    const auto rate_id = (a.rates[sector] >> (a.encounter_rules[16 + terrain] * 2U)) & 3U;
    if (rate_id == 3)
        return {};
    const auto rate = read16(a.encounter_rules, 32 + ((equipment & 3U) * 4 + rate_id) * 2);
    if (!rate)
        return {};
    counter =
        unsigned{counter} + rate > 65535 ? FF6_ROM_VALUE_ENCOUNTER_COUNTER_CAP_0
                                       : static_cast<std::uint16_t>(counter + rate);
    rng.encounter_index = wrap8(unsigned{rng.encounter_index} + 1);
    if (!rng.encounter_index)
        rng.encounter_offset = wrap8(unsigned{rng.encounter_offset} + FF6_ROM_VALUE_ENCOUNTER_INDEX_STEP_0);
    if (wrap8(unsigned{a.battles.random[rng.encounter_index]} + rng.encounter_offset) >=
        (counter >> 8U))
        return {};
    counter = 0;
    const auto group = a.groups[sector * 4 + a.encounter_rules[24 + terrain]];
    // Old native saves lack this history. Unlike the original unbounded scan,
    // an empty history produces no encounter; never invent discovered battles.
    if (group == 255 && history.none())
        return {};
    rng.formation_index = wrap8(unsigned{rng.formation_index} + 1);
    if (!rng.formation_index)
        rng.formation_offset = wrap8(unsigned{rng.formation_offset} + FF6_ROM_VALUE_FORMATION_INDEX_STEP_0);
    const auto value =
        wrap8(unsigned{a.battles.random[rng.formation_index]} + rng.formation_offset);
    FieldBattleRequest result;
    if (group == 255) {
        // GetVeldtBattle advances through 64 packs, then scans cyclically from
        // a random bit. This deliberately is not uniform among available bits.
        unsigned pack = (unsigned{rng.veldt} + 1) & 63U;
        for (unsigned remaining = 64; remaining; --remaining, pack = (pack + 1) & 63U) {
            bool available = false;
            for (unsigned bit = 0; bit < 8; ++bit)
                available |= history[pack * 8 + bit];
            if (!available)
                continue;
            rng.veldt = static_cast<std::uint8_t>(pack);
            for (unsigned step = 0; step < 8; ++step) {
                const auto bit = (unsigned{value} + step) & 7U;
                if (history[pack * 8 + bit]) {
                    result.formation = static_cast<std::uint16_t>(pack * 8 + bit);
                    break;
                }
            }
            break;
        }
        result.on_veldt = true;
    } else {
        result.formation = a.battles.random_groups[group][
            value < FF6_ROM_VALUE_RANDOM_BATTLE_CUTOFFS_0 ? 0
            : value < FF6_ROM_VALUE_RANDOM_BATTLE_CUTOFFS_1 ? 1
            : value < FF6_ROM_VALUE_RANDOM_BATTLE_CUTOFFS_2 ? 2 : 3];
    }
    result.background = a.encounter_rules[w.map * 8 + terrain];
    result.continue_music = music || result.on_veldt;
    result.field_return.fade_in = true;
    return result;
}
} // namespace ff6
