#include "ff6/title_animation.hpp"
#include "rom-materials/title_caption_palette_destination.macros.inc"
#include "rom-materials/title_lightning_palette_destination.macros.inc"
#include "rom-materials/title_town_left_palette_destination.macros.inc"
#include "rom-materials/title_unused_caption_palette_destination.macros.inc"
#include "rom-materials/title_initial_palette_transfers.macros.inc"
#include "rom-materials/title_flames_palette_destination.macros.inc"
#include "rom-materials/title_logo_palette_destination.macros.inc"
#include "rom-materials/title_town_middle_palette_destination.macros.inc"
#include "rom-materials/title_town_right_palette_destination.macros.inc"
#include "rom-materials/title_town_lights_palette_destination.macros.inc"
#include "rom-materials/title_town_lower_palette_destination.macros.inc"
#include "rom-materials/title_fade_palette_sources.macros.inc"
#include "rom-materials/title_wave_row_bounds.macros.inc"
#include "rom-materials/title_wave_phase_bias.macros.inc"
#include "rom-materials/title_expansion_frame_mask.macros.inc"
#include "rom-materials/opening_town_light_mask.macros.inc"
#include "rom-materials/title_expansion_hidden_offset.macros.inc"
#include "rom-materials/title_wave_gains.macros.inc"
#include "rom-materials/title_wave_phase_stride.macros.inc"
#include "rom-materials/title_initial_hidden_band.macros.inc"
#include "rom-materials/title_initial_task_priority.macros.inc"
#include "rom-materials/title_lightning_task_priority.macros.inc"
#include "rom-materials/title_cloud_initial_y.macros.inc"
#include "rom-materials/title_expansion_rows.macros.inc"
#include "rom-materials/title_foreground_task_priority.macros.inc"
#include "rom-materials/title_lightning_initial_palette.macros.inc"
#include "rom-materials/title_row_sample_lengths.macros.inc"
#include <algorithm>
#include <type_traits>
#include "rom-materials/title_terminal_wait.macros.inc"
#include "rom-materials/title_extinguish_fade_period.macros.inc"
#include "rom-materials/title_town_fade_period.macros.inc"
#include "rom-materials/title_logo_fade_period.macros.inc"
#include "rom-materials/title_town_scroll_mask.macros.inc"
#include "rom-materials/title_town_vertical_offset.macros.inc"
#include "rom-materials/title_logo_expanded_height.macros.inc"
#include "rom-materials/title_lightning_y_range.macros.inc"

namespace ff6 {
namespace {
// Selected palette destinations come from the ROM. The buffer base and
// RGB555 record size describe its format, not a copied palette selection.
constexpr unsigned caption_palette = (FF6_ROM_VALUE_TITLE_CAPTION_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned lightning_palette = (FF6_ROM_VALUE_TITLE_LIGHTNING_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned town_left_palette = (FF6_ROM_VALUE_TITLE_TOWN_LEFT_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned unused_caption_palette = (FF6_ROM_VALUE_TITLE_UNUSED_CAPTION_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned flames_palette = (FF6_ROM_VALUE_TITLE_FLAMES_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned logo_palette = (FF6_ROM_VALUE_TITLE_LOGO_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned town_middle_palette = FF6_ROM_VALUE_TITLE_TOWN_MIDDLE_PALETTE_DESTINATION_0;
constexpr unsigned town_right_palette = FF6_ROM_VALUE_TITLE_TOWN_RIGHT_PALETTE_DESTINATION_0;
constexpr unsigned town_lights_palette = (FF6_ROM_VALUE_TITLE_TOWN_LIGHTS_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
constexpr unsigned town_lower_palette = (FF6_ROM_VALUE_TITLE_TOWN_LOWER_PALETTE_DESTINATION_0 - 0x3000U) / 32U;
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
CutscenePalette colors(ByteView data) {
    require(data.size() == 32, "Invalid title animation palette extent");
    CutscenePalette result{};
    for (unsigned i = 0; i < 16; ++i)
        result[i] = read16(data, i * 2) & 0x7fffU;
    return result;
}
std::int16_t signed_word(std::uint16_t value) {
    return static_cast<std::int16_t>(value < 32768 ? static_cast<int>(value)
                                                   : static_cast<int>(value) - 65536);
}
std::int16_t wave_offset(std::int16_t sine, unsigned scale) {
    const auto product = static_cast<int>(sine) * static_cast<int>(scale);
    return static_cast<std::int16_t>(product >= 0 ? product / 256 : -((-product + 255) / 256));
}
std::vector<RasterSprite> sprite_group(ByteView data, std::size_t& cursor,
                                       const std::vector<Tile>& tiles) {
    const unsigned count = slice(data, cursor++, 1)[0];
    require(count <= 128, "Invalid lightning sprite count");
    std::vector<RasterSprite> result;
    for (unsigned i = 0; i < count; ++i) {
        const auto item = slice(data, cursor, 4);
        cursor += 4;
        require(((item[3] >> 1U) & 7U) == lightning_palette - 8U, "Lightning uses an unowned sprite palette");
        RasterSprite sprite;
        sprite.x = item[0] & 127U;
        sprite.y = item[1];
        sprite.width = (item[0] & 128U) ? 32 : 16;
        sprite.height = sprite.width;
        constexpr std::array<std::uint16_t, 4> depths{2, 5, 8, 12};
        sprite.depth = depths[(item[3] >> 4U) & 3U];
        sprite.flip_x = (item[3] & 64U) != 0;
        sprite.flip_y = (item[3] & 128U) != 0;
        const unsigned first = item[2] + (item[3] & 1U) * 256U;
        sprite.pixels.resize(unsigned{sprite.width} * sprite.height);
        for (unsigned y = 0; y < sprite.height; ++y)
            for (unsigned x = 0; x < sprite.width; ++x) {
                const auto tile = (first & ~15U) + ((first + x / 8) & 15U) + (y / 8) * 16;
                require(tile < tiles.size(), "Lightning exceeds its typed graphics pool");
                sprite.pixels[y * sprite.width + x] = tiles[tile][(y % 8) * 8 + x % 8];
            }
        result.push_back(std::move(sprite));
    }
    return result;
}
} // namespace

TitleAnimationAssets load_title_animation_assets(const ContentPack& pack) {
    TitleAnimationAssets assets;
    assets.base = load_title_assets(pack);
    assets.town_dark = colors(pack.get("title_town_dark_palette"));
    const auto palette_data = pack.get("title_lightning_palettes");
    const auto sprites = pack.get("title_lightning_sprites");
    require(palette_data.size() == 187 && sprites.size() == 145,
            "Invalid title animation data extent");
    // Normalize legacy data pointers only here, with a complete bounded lookup.
    assets.lightning_colors[0] = assets.base.palettes[(FF6_ROM_VALUE_TITLE_LIGHTNING_INITIAL_PALETTE_0 - 0x7b43U) / 32U];
    for (unsigned i = 0; i < 5; ++i)
        assets.lightning_colors[i + 1] = colors(palette_data.subspan(27 + i * 32, 32));
    for (unsigned i = 0; i < 9; ++i) {
        const unsigned pointer = read16(palette_data, i * 3);
        require(pointer == FF6_ROM_VALUE_TITLE_LIGHTNING_INITIAL_PALETTE_0 ||
                    (pointer >= 0x5659 && pointer <= 0x56d9 && (pointer - 0x5659) % 32 == 0),
                "Unresolved title palette animation reference");
        assets.palette_animation.push_back(
            {pointer == FF6_ROM_VALUE_TITLE_LIGHTNING_INITIAL_PALETTE_0 ? 0U : (pointer - 0x5659) / 32 + 1, palette_data[i * 3 + 2]});
    }
    std::vector<std::size_t> frame_offsets;
    std::size_t cursor = 56;
    while (cursor < sprites.size()) {
        frame_offsets.push_back(cursor);
        assets.lightning_frames.push_back(sprite_group(sprites, cursor, assets.base.sprite_tiles));
    }
    for (unsigned i = 0; i < 4; ++i) {
        const auto pointer = read16(sprites, i * 2);
        require(pointer == 0 || pointer == 0x574e || pointer == 0x5766,
                "Unresolved lightning selection");
        if (pointer)
            assets.lightning_selection[i] = pointer == 0x574e ? 0 : 1;
    }
    for (unsigned animation = 0; animation < 2; ++animation)
        for (unsigned i = 0; i < 8; ++i) {
            const auto offset = 8 + animation * 24 + i * 3;
            const unsigned pointer = read16(sprites, offset);
            require(pointer >= 0x5746, "Invalid lightning frame pointer");
            const auto it = std::ranges::find(frame_offsets, pointer - 0x5746);
            require(it != frame_offsets.end(), "Unresolved lightning frame reference");
            assets.lightning_animations[animation].push_back(
                {static_cast<std::size_t>(it - frame_offsets.begin()), sprites[offset + 2]});
        }
    const auto data = decompress_lzss(pack.get("cutscene_assets_title_opening_bin_lz"));
    for (unsigned i = 0; i < 1024; ++i) {
        const unsigned word = read16(data, 0x23c0 + i * 2);
        assets.cloud_rows.push_back({word & 1023U, (word >> 10U) & 7U,
                                     static_cast<std::uint16_t>((word & 8192U) ? 11 : 7),
                                     (word & 16384U) != 0, (word & 32768U) != 0});
    }
    const auto sine = pack.get("data_c2fc6d_btlgfx_lookup_tables"),
               rng = pack.get("encounter_rng_tbl_bin");
    require(sine.size() == 768 && rng.size() == 256, "Invalid title arithmetic tables");
    for (unsigned i = 0; i < 256; ++i) {
        const int value = sine[512 + i];
        assets.sine[i] = static_cast<std::int16_t>(value < 128 ? value : value - 256);
        assets.random[i] = rng[i];
    }
    return assets;
}
TitleAnimation::TitleAnimation(TitleAnimationAssets assets, const TitleSplashState& splash,
                               std::uint8_t random_index, CutsceneInput input)
    : assets_(std::move(assets)) {
    require(splash.phase == SplashPhase::complete, "Title requires completed splash ownership");
    require(assets_.cloud_rows.size() == 1024 && assets_.base.backgrounds[0].cells.size() == 4096,
            "Invalid title cloud/layout ownership");
    auto validate = [](const auto& frames, std::size_t count) {
        require(!frames.empty(), "Empty title animation");
        for (const auto& frame : frames) {
            require(frame.resource < count && frame.duration > 0 && frame.duration != 255,
                    "Invalid title animation resource or duration");
        }
        require(frames.back().duration == 254, "Unterminated title animation");
    };
    validate(assets_.palette_animation, assets_.lightning_colors.size());
    for (const auto& frames : assets_.lightning_animations)
        validate(frames, assets_.lightning_frames.size());
    for (const auto selection : assets_.lightning_selection)
        require(!selection || *selection < assets_.lightning_animations.size(),
                "Invalid lightning selection");
    require(assets_.base.first_legal_group_size <= assets_.base.legal_text.size(),
            "Invalid legal sprite groups");
    state_.frame_counter = splash.frame_counter;
    state_.random_index = random_index;
    state_.input = input;
    for (unsigned row = 0; row < 224; ++row)
        state_.rows[0][row] = {
            static_cast<std::int16_t>(row >= FF6_ROM_VALUE_TITLE_INITIAL_HIDDEN_BAND_0 && row <= FF6_ROM_VALUE_TITLE_INITIAL_HIDDEN_BAND_0 + FF6_ROM_VALUE_TITLE_INITIAL_HIDDEN_BAND_1 - 1 ? FF6_ROM_VALUE_TITLE_INITIAL_HIDDEN_BAND_2 : 0),
            FF6_ROM_VALUE_TITLE_TOWN_VERTICAL_OFFSET_0};
}
void TitleAnimation::create(unsigned priority, Task task) {
    require(priority <= 2, "Invalid title task priority");
    for (std::size_t i = priority * 16; i < tasks_.size(); ++i)
        if (std::holds_alternative<std::monostate>(tasks_[i])) {
            tasks_[i] = std::move(task);
            return;
        }
    throw std::runtime_error("Title task capacity exhausted");
}
void TitleAnimation::fade(unsigned group, const CutscenePalette& target, std::uint8_t period) {
    CutscenePaletteFade task;
    task.period = period;
    create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Fade{group, target, task});
}
void TitleAnimation::advance_stage() {
    const auto& palettes = assets_.base.palettes;
    switch (state_.stage) {
    case TitleStage::initialize: {
        // ROM-selected source pointers resolve only to the extracted palette
        // resources. The neutral pointer and contiguous source origin describe
        // that resource layout; no source CPU code is executed.
        const auto initial_palette_source = [&](unsigned pointer) -> const CutscenePalette& {
            return pointer == 0x7465U ? assets_.base.black : palettes[(pointer - 0x7b43U) / 32U];
        };
        state_.colors[(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_0 - 0x3000U) / 32U] =
            initial_palette_source(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_1);
        state_.colors[(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_2 - 0x3000U) / 32U] =
            initial_palette_source(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_3);
        state_.colors[(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_6 - 0x3000U) / 32U] =
            initial_palette_source(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_7);
        static_assert(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_9 == FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_5 &&
                      FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_11 == FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_5 &&
                      FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_13 == FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_5 &&
                      FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_15 == FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_5 &&
                      FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_17 == FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_5);
        for (unsigned group : {
                 (FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_4 - 0x3000U) / 32U,
                 (FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_10 - 0x3000U) / 32U,
                 (FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_12 - 0x3000U) / 32U,
                 (FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_16 - 0x3000U) / 32U,
                 (FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_8 - 0x3000U) / 32U,
                 (FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_14 - 0x3000U) / 32U})
            state_.colors[group] = initial_palette_source(FF6_ROM_VALUE_TITLE_INITIAL_PALETTE_TRANSFERS_5);
        fade(town_left_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_0 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
        fade(town_middle_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_1 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
        fade(town_right_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_2 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
        fade(town_lights_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_3 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
        fade(town_lower_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_4 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
        create(FF6_ROM_VALUE_TITLE_INITIAL_TASK_PRIORITY_0, Cloud{});
        state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_1;
        state_.stage = TitleStage::silent_town;
        for (unsigned group : {town_middle_palette, town_right_palette, town_left_palette})
            create(FF6_ROM_VALUE_TITLE_INITIAL_TASK_PRIORITY_0, PaletteCycle{group});
        break;
    }
    case TitleStage::silent_town:
        if (!state_.remaining) {
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_2;
            state_.stage = TitleStage::town;
            sounds_.push_back(TitleSoundRequest::restart_theme);
        }
        break;
    case TitleStage::town:
        if (!state_.remaining) {
            state_.row_effects = true;
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_3;
            state_.stage = TitleStage::reveal_delay;
        }
        break;
    case TitleStage::reveal_delay:
        if (!state_.remaining) {
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Wave{1});
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Wave{2});
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Expand{});
            fade(flames_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_5 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_LOGO_FADE_PERIOD_0);
            fade(logo_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_6 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_SPLASH_FADE_PERIOD_0);
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_4;
            state_.stage = TitleStage::logo;
        }
        break;
    case TitleStage::logo:
        if (!state_.remaining) {
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_5;
            state_.stage = TitleStage::legal_delay;
        }
        break;
    case TitleStage::legal_delay:
        if (!state_.remaining) {
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Legal{0});
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Legal{1});
            fade(caption_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_7 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_6;
            state_.stage = TitleStage::legal;
        }
        break;
    case TitleStage::legal:
        if (!state_.remaining) {
            // The unused English group still consumes an ordered task slot.
            fade(unused_caption_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_8 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_LOGO_FADE_PERIOD_0);
            fade(caption_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_9 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_TOWN_FADE_PERIOD_0);
            fade(flames_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_10 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_EXTINGUISH_FADE_PERIOD_0);
            fade(logo_palette, palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_11 - 0x7b43U) / 32U], FF6_ROM_VALUE_TITLE_EXTINGUISH_FADE_PERIOD_0);
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_7;
            state_.stage = TitleStage::darken;
        }
        break;
    case TitleStage::darken:
        if (!state_.remaining) {
            state_.hide_legal = true;
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, TownLights{});
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_8;
            state_.stage = TitleStage::stop_effects;
        }
        break;
    case TitleStage::stop_effects:
        if (!state_.remaining) {
            state_.row_effects = false;
            state_.hide_legal = false;
            state_.stop_waves = true;
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_9;
            state_.stage = TitleStage::scroll_town;
        }
        break;
    case TitleStage::scroll_town:
        if (!state_.remaining) {
            state_.remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_10;
            state_.stage = TitleStage::hold_town;
        } else if ((state_.frame_counter & FF6_ROM_VALUE_TITLE_TOWN_SCROLL_MASK_0) == 0)
            state_.town_y = wrap16(state_.town_y + 1U);
        break;
    case TitleStage::hold_town:
        if (!state_.remaining)
            state_.stage = TitleStage::terminate;
        break;
    case TitleStage::terminate:
        state_.stage = TitleStage::finished;
        state_.skipped = false;
        break;
    case TitleStage::finished:
        break;
    }
}
std::pair<std::uint8_t, bool> TitleAnimation::random() {
    state_.random_index = wrap8(state_.random_index + 1U);
    const unsigned sum = unsigned{state_.frame_counter} + state_.random_index;
    return {assets_.random[wrap8(sum)], sum > 255};
}
void TitleAnimation::animate(Cursor& cursor, const std::vector<TitleAnimationFrame>& frames) {
    advance_cutscene_animation(cursor, frames);
}
bool TitleAnimation::run(Task& task) {
    return std::visit(
        [&](auto& current) -> bool {
            using T = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return false;
            else if constexpr (std::is_same_v<T, Fade>) {
                current.task.advance(state_.colors[current.group], current.target);
                return !current.task.finished;
            } else if constexpr (std::is_same_v<T, Cloud>) {
                if (!current.initialized) {
                    state_.town_y = FF6_ROM_VALUE_TITLE_CLOUD_INITIAL_Y_0;
                    current.initialized = true;
                }
                if ((state_.frame_counter & FF6_ROM_VALUE_TITLE_TOWN_SCROLL_MASK_0) == 0) {
                    if (state_.town_y == FF6_ROM_VALUE_TITLE_TOWN_VERTICAL_OFFSET_0)
                        return false;
                    if (state_.town_y < 256 && (state_.town_y & 7U) == 0) {
                        const unsigned row = (state_.town_y - 8U) / 8U;
                        std::copy_n(assets_.cloud_rows.begin() + row * 32, 32,
                                    assets_.base.backgrounds[0].cells.begin() + row * 64);
                    }
                    ++state_.town_y;
                }
            } else if constexpr (std::is_same_v<T, PaletteCycle>) {
                if (current.phase == 0) {
                    current.phase = 1;
                    current.cursor = {0, assets_.palette_animation.front().duration};
                } else if (current.phase == 2) {
                    current.wait = random().first;
                    current.phase = 1;
                    current.cursor = {0, assets_.palette_animation.front().duration};
                } else if (current.wait) {
                    if (current.wait == 1) {
                        create(FF6_ROM_VALUE_TITLE_LIGHTNING_TASK_PRIORITY_0, Lightning{});
                        state_.lightning_count = wrap8(state_.lightning_count + 1U);
                    }
                    --current.wait;
                } else {
                    if (current.cursor.wait == 254)
                        current.phase = 2;
                    animate(current.cursor, assets_.palette_animation);
                    state_.colors[current.group] =
                        assets_.lightning_colors[assets_.palette_animation[current.cursor.frame]
                                                     .resource];
                }
            } else if constexpr (std::is_same_v<T, Lightning>) {
                if (!current.initialized) {
                    const auto selected = assets_.lightning_selection[random().first & 3U];
                    if (!selected)
                        return false;
                    current.initialized = true;
                    current.animation = *selected;
                    current.x = random().first;
                    const auto [value, carry] = random();
                    // The original ADC preserves Rand's overflow carry through AND.
                    current.y = wrap8((value & FF6_ROM_VALUE_TITLE_LIGHTNING_Y_RANGE_0) +
                                      FF6_ROM_VALUE_TITLE_LIGHTNING_Y_RANGE_1 +
                                      (carry ? 1U : 0U));
                    current.cursor = {
                        0, assets_.lightning_animations[current.animation].front().duration};
                }
                if (current.cursor.wait == 254)
                    return false;
                const auto& animation = assets_.lightning_animations[current.animation];
                animate(current.cursor, animation);
                for (auto sprite :
                     assets_.lightning_frames[animation[current.cursor.frame].resource]) {
                    sprite.x = wrap8(static_cast<unsigned>(sprite.x) + current.x);
                    sprite.y = wrap8(static_cast<unsigned>(sprite.y) + current.y);
                    sprites_.push_back({std::move(sprite), lightning_palette});
                }
            } else if constexpr (std::is_same_v<T, Wave>) {
                if (state_.stop_waves)
                    return false;
                // The three ROM stores select one row band in typed native layers.
                static_assert(FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_0 == FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_1 &&
                              FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_0 == FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_2);
                static_assert(FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_3 == FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_4);
                for (unsigned row = 0; row < FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_3 / 4U; ++row) {
                    const auto phase =
                        wrap8(current.phase + (current.layer == 1 ? 0U : FF6_ROM_VALUE_TITLE_WAVE_PHASE_BIAS_0) + row * FF6_ROM_VALUE_TITLE_WAVE_PHASE_STRIDE_0);
                    const auto offset =
                        wave_offset(assets_.sine[phase], current.layer == 1 ? FF6_ROM_VALUE_TITLE_WAVE_GAINS_0 : FF6_ROM_VALUE_TITLE_WAVE_GAINS_1);
                    state_.rows[current.layer][FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_0 / 4U + row].x = offset;
                    if (current.layer == 2)
                        state_.rows[current.layer][FF6_ROM_VALUE_TITLE_WAVE_ROW_BOUNDS_0 / 4U + row].y = offset;
                }
                current.phase = wrap8(current.phase + 1U);
            } else if constexpr (std::is_same_v<T, Expand>) {
                if (state_.frame_counter & FF6_ROM_VALUE_TITLE_EXPANSION_FRAME_MASK_0) {
                    if (current.height == FF6_ROM_VALUE_TITLE_LOGO_EXPANDED_HEIGHT_0)
                        return false;
                    const unsigned step =
                        (FF6_ROM_VALUE_TITLE_LOGO_EXPANDED_HEIGHT_0 << 8U) / current.height;
                    std::uint16_t accumulated = 0;
                    for (unsigned i = 0; i < FF6_ROM_VALUE_TITLE_EXPANSION_ROWS_0; ++i) {
                        // The subtraction is byte-wide before the 16-bit negation.
                        const auto difference = wrap8((accumulated >> 8U) + 256U - i);
                        state_.rows[0][FF6_ROM_VALUE_TITLE_EXPANSION_ROWS_0 + FF6_ROM_VALUE_TITLE_EXPANSION_ROWS_1 - i].y =
                            i > current.height ? FF6_ROM_VALUE_TITLE_EXPANSION_HIDDEN_OFFSET_0 : signed_word(wrap16(65536U - difference));
                        accumulated = wrap16(accumulated + step);
                    }
                    ++current.height;
                }
            } else if constexpr (std::is_same_v<T, Legal>) {
                if (state_.hide_legal)
                    return false;
                const auto split = assets_.base.first_legal_group_size;
                const auto begin = current.group == 0 ? 0 : split;
                const auto end = current.group == 0 ? split : assets_.base.legal_text.size();
                for (auto i = begin; i < end; ++i) {
                    sprites_.push_back({assets_.base.legal_text[i], caption_palette});
                }
            } else if constexpr (std::is_same_v<T, TownLights>) {
                state_.colors[town_lights_palette] =
                    (state_.frame_counter & FF6_ROM_VALUE_OPENING_TOWN_LIGHT_MASK_0) ? assets_.town_dark : assets_.base.palettes[(FF6_ROM_VALUE_TITLE_FADE_PALETTE_SOURCES_12 - 0x7b43U) / 32U];
            } else if constexpr (std::is_same_v<T, Brightness>) {
                state_.intensity = current.level--;
            }
            return true;
        },
        task);
}
bool TitleAnimation::prepare() {
    if (state_.complete)
        return false;
    if (state_.fading && !state_.remaining) {
        state_.complete = true;
        sounds_.push_back(TitleSoundRequest::stop);
        return false;
    }
    if (!state_.fading) {
        const bool terminal = state_.stage == TitleStage::finished;
        if (!terminal)
            advance_stage();
        if (terminal || state_.input.pressed) {
            if (!terminal)
                state_.skipped = true;
            state_.fading = true;
            state_.remaining = FF6_ROM_VALUE_TITLE_TERMINAL_WAIT_0;
            create(FF6_ROM_VALUE_TITLE_FOREGROUND_TASK_PRIORITY_0, Brightness{});
        }
    }
    sprites_.clear();
    for (auto& task : tasks_)
        if (!run(task))
            task = std::monostate{};
    require(sprites_.size() <= 128, "Title sprite capacity exceeded");
    return true;
}
bool TitleAnimation::advance(bool held_a) {
    if (!prepared_) {
        prepared_ = true;
        prepare();
    }
    if (state_.complete) {
        published_.complete = true;
        return false;
    }
    if (state_.remaining)
        --state_.remaining;
    state_.frame_counter = wrap8(state_.frame_counter + 1U);
    ++state_.visits;
    state_.input.sample(held_a);
    published_ = state_;
    published_sprites_ = sprites_;
    published_layout_ = assets_.base.backgrounds[0].cells;
    // Palettes/objects/layout are committed at the boundary. Tasks then prepare
    // the next visit; their wave/expansion rows are live during this interval.
    // Keep those native rows separate from the buffered frame description.
    prepare();
    return true;
}
std::vector<TitleSoundRequest> TitleAnimation::take_sound_requests() {
    auto result = std::move(sounds_);
    sounds_.clear();
    return result;
}
RasterScene TitleAnimation::scene() const {
    require(published_.visits && !published_.complete, "Title has no published frame");
    RasterScene scene;
    scene.intensity.assign(224, published_.intensity);
    scene.backdrop = published_.colors[0][0];
    scene.layers.assign(assets_.base.backgrounds.begin(), assets_.base.backgrounds.end());
    scene.layers[0].cells = published_layout_;
    for (unsigned layer = 0; layer < 3; ++layer) {
        auto& target = scene.layers[layer];
        require(target.palettes.size() == 8 && target.lines.size() == 224,
                "Invalid title layer ownership");
        for (unsigned group = 0; group < 8; ++group)
            for (unsigned color = 0; color < 16; ++color)
                target.palettes[group][color] =
                    layer == 2 ? published_.colors[group / 4][(group % 4) * 4 + color % 4]
                               : published_.colors[group][color];
        for (unsigned row = 0; row < 224; ++row) {
            const auto offset = state_.rows[layer][std::min(row, FF6_ROM_VALUE_TITLE_ROW_SAMPLE_LENGTHS_0 + FF6_ROM_VALUE_TITLE_ROW_SAMPLE_LENGTHS_1 - 1U)];
            target.lines[row].offset_x = published_.row_effects ? offset.x : 0;
            target.lines[row].offset_y =
                1 + (published_.row_effects ? offset.y : (layer == 0 ? published_.town_y : 0));
        }
    }
    scene.objects = SpriteLayer{};
    scene.objects->lines.resize(224);
    // Tasks choose sprites before later tasks finish their palette updates. All
    // sprites observe the final published palette, not the earlier task's copy.
    for (const auto& sprite : published_sprites_) {
        auto image = sprite.image;
        image.colors = published_.colors[sprite.palette];
        scene.objects->sprites.push_back(std::move(image));
    }
    return scene;
}
} // namespace ff6
