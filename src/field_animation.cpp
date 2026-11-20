#include "ff6/field_animation.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}
unsigned steps(std::uint8_t count) {
    return count == 0 ? 256 : count;
}
void valid_tile(const Tile& tile, unsigned limit) {
    require(std::ranges::all_of(tile, [limit](auto value) { return value < limit; }),
            "Invalid animation pixel index");
}
Rgb555 pulse_color(Rgb555 color, std::uint8_t scale) {
    unsigned result = 0;
    for (unsigned shift : {0U, 5U, 10U})
        result |= ((((unsigned{color} >> shift) & 31U) * scale) >> 8U) << shift;
    return static_cast<Rgb555>(result);
}
} // namespace

FieldAnimationAssets load_field_animation(const ContentPack& pack,
                                          const FieldMapDefinition& definition) {
    require(definition.background_animation < 20 && definition.background3_animation <= 6 &&
                definition.palette_animation <= 10,
            "Invalid field animation identity");
    FieldAnimationAssets assets;
    assets.backgrounds_enabled =
        definition.background_animation != 0 || definition.background3_animation != 0;
    const auto table = pack.get("field_background_animation_tables");
    const auto raw = pack.get("map_animation_map_anim_4bpp");
    require(table.size() == 1616 && raw.size() == 32768, "Invalid background animation data");
    const auto offset = read16(table, std::size_t{definition.background_animation} * 2) + 42U;
    for (std::size_t group = 0; group < 8; ++group) {
        auto& output = assets.backgrounds[group];
        const auto record = slice(table, offset + group * 10, 10);
        output.speed = read16(record, 0);
        for (std::size_t frame = 0; frame < 4; ++frame) {
            const auto tiles = decode_tiles(slice(raw, read16(record, 2 + frame * 2), 128), 4);
            std::ranges::copy(tiles, output.frames[frame].begin());
        }
    }
    if (definition.background3_animation != 0) {
        const auto identity = std::size_t{definition.background3_animation} - 1;
        // Relative property groups follow the BG12 pool; logical group boundaries
        // are storage metadata, not limits on the original consumer's windows.
        const auto properties = table.subspan(1482);
        const auto record = slice(properties, 14U + read16(properties, identity * 2), 20);
        const auto pointers = pack.get("map_animation_map_anim_gfx_bg3_ptr_bin");
        const auto compressed = pack.get("map_animation_map_anim_gfx_bg3_2bpp_lz");
        require(pointers.size() == 21 && read24(pointers, 0) == 0 &&
                    read24(pointers, 18) == compressed.size(),
                "Invalid third-background animation extent");
        const auto start = read24(pointers, identity * 3), end = read24(pointers, identity * 3 + 3);
        require(start < end, "Unordered third-background animation pointers");
        const auto decoded = decompress_lzss(slice(compressed, start, end - start), 4096);
        require(decoded.size() == 4096, "Invalid third-background animation stream");
        ThirdBackgroundAnimation third;
        third.speed = read16(record, 0);
        const auto size = read16(record, 2);
        require(size != 0 && size <= 4096 && size % 32 == 0,
                "Invalid third-background animation window");
        for (std::size_t frame = 0; frame < 8; ++frame)
            third.frames[frame] =
                decode_tiles(slice(decoded, read16(record, 4 + frame * 2), size), 2);
        assets.third = std::move(third);
    }
    const auto curve = pack.get("field_palette_pulse_curve");
    require(curve.size() == 18, "Invalid palette pulse curve");
    assets.pulse_curve.assign(curve.begin(), curve.end());
    if (definition.palette_animation != 0) {
        const auto records = pack.get("map_animation_map_pal_anim_prop_bin");
        const auto colors = pack.get("map_animation_map_pal_anim_pal");
        require(records.size() == 120 && colors.size() == 576, "Invalid palette animation data");
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const auto row = slice(
                records, (std::size_t{definition.palette_animation} - 1) * 12 + channel * 6, 6);
            auto& output = assets.palettes[channel];
            const auto kind = (row[0] >> 4U) & 7U;
            // Disabled and reserved types are documented original no-ops. Preserve
            // them as explicit inactivity; don't interpret their unused operands.
            if ((row[0] & 128U) != 0 || kind >= 4)
                continue;
            constexpr std::array kinds{PaletteAnimationKind::counter, PaletteAnimationKind::cycle,
                                       PaletteAnimationKind::colors, PaletteAnimationKind::pulse};
            output.kind = kinds[kind];
            output.count = row[0] & 15U;
            output.speed = row[1];
            if (kind == 0)
                continue;
            require(row[2] % 2 == 0 && row[3] % 2 == 0, "Unaligned palette animation range");
            output.first_color = row[2] / 2;
            output.color_count = static_cast<std::uint16_t>(unsigned{row[3]} / 2 + 1);
            require(unsigned{output.first_color} + output.color_count <= 128,
                    "Palette animation exceeds field colors");
            if (kind == 2) {
                for (unsigned frame = 0; frame < steps(output.count); ++frame) {
                    const auto window = slice(colors, read16(row, 4) + frame * 32U,
                                              std::size_t{output.color_count} * 2);
                    std::vector<Rgb555> values;
                    for (std::size_t i = 0; i < output.color_count; ++i)
                        values.push_back(read16(window, i * 2) & 0x7fffU);
                    output.frames.push_back(std::move(values));
                }
            }
        }
    }
    return assets;
}

bool advance_palette_clock(PaletteAnimationClock& clock, std::uint8_t speed, std::uint8_t count) {
    clock.tick = wrap8(unsigned{clock.tick} + 1U);
    if (clock.tick != speed)
        return false;
    clock.tick = 0;
    clock.index = wrap8(unsigned{clock.index} + 1U);
    if (clock.index != count)
        return false;
    clock.index = 0;
    return true;
}

FieldAnimation::FieldAnimation(FieldAnimationAssets assets) : assets_(std::move(assets)) {
    for (const auto& group : assets_.backgrounds)
        for (const auto& frame : group.frames)
            for (const auto& tile : frame)
                valid_tile(tile, 16);
    if (assets_.third) {
        const auto count = assets_.third->frames[0].size();
        require(count != 0 && count <= 256 && count % 2 == 0,
                "Invalid third-background animation tile count");
        for (const auto& frame : assets_.third->frames) {
            require(frame.size() == count, "Inconsistent third-background animation windows");
            for (const auto& tile : frame)
                valid_tile(tile, 4);
        }
    }
    for (const auto& channel : assets_.palettes) {
        switch (channel.kind) {
        case PaletteAnimationKind::inactive:
        case PaletteAnimationKind::counter:
            break;
        case PaletteAnimationKind::cycle:
        case PaletteAnimationKind::colors:
        case PaletteAnimationKind::pulse:
            require(channel.color_count != 0 && channel.first_color < 128 &&
                        unsigned{channel.first_color} + channel.color_count <= 128,
                    "Invalid palette animation window");
            if (channel.kind == PaletteAnimationKind::cycle)
                require(channel.color_count >= 2, "Palette cycle needs at least two colors");
            if (channel.kind == PaletteAnimationKind::pulse)
                require(steps(channel.count) <= assets_.pulse_curve.size(),
                        "Palette pulse exceeds curve");
            if (channel.kind == PaletteAnimationKind::colors) {
                require(channel.frames.size() == steps(channel.count), "Invalid palette frames");
                for (const auto& frame : channel.frames) {
                    require(frame.size() == channel.color_count, "Invalid palette frame extent");
                    for (auto color : frame)
                        require(color <= 0x7fff, "Invalid animation color");
                }
            }
            break;
        default:
            throw std::invalid_argument("Unknown native palette animation kind");
        }
    }
}
void FieldAnimation::set_background_rate(std::uint8_t group, std::uint8_t rate) {
    if (group >= assets_.backgrounds.size())
        throw std::invalid_argument("Invalid background animation group");
    const unsigned magnitude = unsigned{rate & 127U} * 16U;
    assets_.backgrounds[group].speed = wrap16((rate & 128U) ? 65536U - magnitude : magnitude);
}
void FieldAnimation::advance_backgrounds(FieldMap& map, BackgroundAnimationPhase phase) {
    switch (phase) {
    case BackgroundAnimationPhase::first_and_second:
        if (assets_.backgrounds_enabled)
            for (std::size_t group = 0; group < 8; ++group) {
                auto& counter = state_.backgrounds[group];
                const auto& clip = assets_.backgrounds[group];
                counter = wrap16(unsigned{counter} + clip.speed);
                const auto& frame = clip.frames[(counter >> 9U) & 3U];
                for (std::size_t tile = 0; tile < 4; ++tile)
                    map.characters[640 + group * 4 + tile] = frame[tile];
            }
        break;
    case BackgroundAnimationPhase::third:
        if (assets_.third) {
            state_.third = wrap16(unsigned{state_.third} + assets_.third->speed);
            const auto& frame = assets_.third->frames[(state_.third >> 9U) & 7U];
            std::ranges::copy(frame, map.background3.begin());
            // Two consecutive 2bpp characters also form one 4bpp character.
            // Update only this window; aliases beyond it retain their prior owner.
            for (std::size_t tile = 0; tile < frame.size() / 2; ++tile) {
                Tile alias;
                for (std::size_t pixel = 0; pixel < 64; ++pixel)
                    alias[pixel] = static_cast<std::uint8_t>(frame[tile * 2][pixel] |
                                                             (frame[tile * 2 + 1][pixel] << 2U));
                map.characters[768 + tile] = alias;
            }
        }
        break;
    default:
        throw std::invalid_argument("Unknown background animation phase");
    }
}
void FieldAnimation::advance_palette(const std::array<Rgb555, 128>& base,
                                     std::array<Rgb555, 128>& destination) {
    // Preflight before any counter/color mutation, including overlapping channels.
    for (auto color : base)
        require(color <= 0x7fff, "Invalid base field color");
    for (auto color : destination)
        require(color <= 0x7fff, "Invalid destination field color");
    const auto source = base;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& channel = assets_.palettes[i];
        if (channel.kind == PaletteAnimationKind::inactive)
            continue;
        auto& clock = state_.palettes[i];
        const auto wrapped = advance_palette_clock(clock, channel.speed, channel.count);
        if (channel.kind == PaletteAnimationKind::counter)
            continue;
        const auto first = destination.begin() + channel.first_color;
        switch (channel.kind) {
        case PaletteAnimationKind::cycle:
            if (wrapped)
                std::rotate(first, first + 1, first + channel.color_count);
            break;
        case PaletteAnimationKind::colors:
            std::ranges::copy(channel.frames[clock.index], first);
            break;
        case PaletteAnimationKind::pulse:
            for (std::size_t color = 0; color < channel.color_count; ++color) {
                const auto index = channel.first_color + color;
                destination[index] = pulse_color(source[index], assets_.pulse_curve[clock.index]);
            }
            break;
        default:
            break; // Explicit counter-only mode; inactive channels were skipped.
        }
    }
}
} // namespace ff6
