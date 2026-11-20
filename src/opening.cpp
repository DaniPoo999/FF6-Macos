#include "ff6/opening.hpp"
#include "ff6/perspective.hpp"
#include "rom-materials/opening_phase_waits.macros.inc"
#include "rom-materials/opening_landscape_scroll_mask.macros.inc"
#include "rom-materials/opening_town_scroll_mask.macros.inc"
#include "rom-materials/opening_caption_fade_in_checkpoint.macros.inc"
#include "rom-materials/opening_caption_fade_out_checkpoint.macros.inc"
#include "rom-materials/opening_caption_fade_period.macros.inc"
#include "rom-materials/opening_snow_task_limit.macros.inc"
#include "rom-materials/opening_armor_task_priority.macros.inc"
#include "rom-materials/opening_snow_horizontal_mask.macros.inc"
#include "rom-materials/opening_snow_vertical_mask.macros.inc"
#include "rom-materials/opening_town_light_palette.macros.inc"
#include "rom-materials/opening_town_light_mask.macros.inc"
#include "rom-materials/opening_caption_top_band.macros.inc"
#include "rom-materials/opening_caption_band_height.macros.inc"
#include "rom-materials/opening_perspective_center_x.macros.inc"
#include "rom-materials/opening_perspective_prefix.macros.inc"
#include "rom-materials/opening_perspective_band_rows.macros.inc"
#include "rom-materials/opening_caption_cell_origin.macros.inc"
#include "rom-materials/opening_exit_wait.macros.inc"
#include "rom-materials/opening_text_arithmetic.macros.inc"
#include "rom-materials/opening_landscape_arithmetic.macros.inc"
#include <algorithm>
#include <type_traits>
#include "rom-materials/opening_march_delays.macros.inc"
#include "rom-materials/opening_march_durations.macros.inc"
#include "rom-materials/opening_march_rates.macros.inc"
#include "rom-materials/opening_initial_fade_routes.macros.inc"
#include "rom-materials/opening_snow_first_fade_period.macros.inc"
#include "rom-materials/opening_snow_landscape_fade_period.macros.inc"
#include "rom-materials/opening_snow_town_fade_period.macros.inc"
#include "rom-materials/opening_snow_final_fade_period.macros.inc"
#include "rom-materials/opening_armor_fade_period.macros.inc"

namespace ff6 {
namespace {
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
std::int16_t coordinate13(std::uint16_t value) {
    const auto low = value & 8191U;
    return static_cast<std::int16_t>(low < 4096 ? static_cast<int>(low)
                                                : static_cast<int>(low) - 8192);
}
} // namespace
OpeningAnimation::OpeningAnimation(OpeningAssets assets, std::uint8_t random_index)
    : assets_(std::move(assets)) {
    require(assets_.armor_offsets.size() == 5 && !assets_.caption_animation.empty(),
            "Opening requires animation resources");
    for (const auto* layer : {&assets_.town, &assets_.text})
        require(layer->columns == 32 && layer->rows == 32 && layer->cells.size() == 1024 &&
                    layer->palettes.size() == 8 && layer->lines.size() == 224,
                "Invalid opening tiled canvas");
    require(assets_.landscape.lines.size() == 224, "Invalid opening landscape rows");
    require(assets_.armor_offsets.back().duration == 255, "Unterminated armor offset cycle");
    for (unsigned i = 0; i < 4; ++i)
        require(assets_.armor_offsets[i].duration > 0 && assets_.armor_offsets[i].duration < 254,
                "Invalid armor offset duration");
    for (const auto selection : assets_.snow_selection)
        require(selection < assets_.snow.size(), "Invalid snow selection");
    for (const auto& frame : assets_.sprite_frames)
        for (const auto& sprite : frame)
            require(sprite.palette < 8 && sprite.image.x >= 0 && sprite.image.x < 128 &&
                        sprite.image.y >= 0 && sprite.image.y < 256 &&
                        (sprite.image.width == 8 || sprite.image.width == 16) &&
                        sprite.image.height == sprite.image.width,
                    "Invalid opening sprite description");
    for (const auto& part : assets_.armor)
        require(!part.animation.empty(), "Opening armor has no animation");
    for (const auto& frames : assets_.snow)
        require(frames.size() == 1 && frames[0].duration == 254,
                "Opening snow must have one held frame");
    state_.random_index = random_index;
}
std::size_t OpeningAnimation::create(unsigned priority, Task task) {
    require(priority <= 2, "Invalid opening task priority");
    for (std::size_t i = priority * 16; i < slots_.size(); ++i)
        if (std::holds_alternative<std::monostate>(slots_[i].task)) {
            slots_[i].task = std::move(task);
            ++state_.active_tasks;
            return i;
        }
    throw std::runtime_error("Opening task capacity exhausted");
}
void OpeningAnimation::fade(unsigned group, const OpeningPalette& target, std::uint8_t period) {
    require(group < 16, "Invalid opening palette group");
    CutscenePaletteFade task;
    task.period = period;
    const auto i = create(0, Fade{group, target, task});
    slots_[i].particle_x = wrap8(group * 32);
    slots_[i].particle_y = target.particle_fraction;
}
void OpeningAnimation::initialize() {
    create(0, Bob{});
    for (unsigned actor = 0; actor < 3; ++actor)
        for (unsigned part = 0; part < 6; ++part)
            create(FF6_ROM_VALUE_OPENING_ARMOR_TASK_PRIORITY_0, Armor{actor, part});
    fade(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_0, assets_.snow_palette, FF6_ROM_VALUE_OPENING_SNOW_FIRST_FADE_PERIOD_0);
    fade(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_1, assets_.snow_palette, FF6_ROM_VALUE_OPENING_SNOW_LANDSCAPE_FADE_PERIOD_0);
    fade(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_2, assets_.snow_palette, FF6_ROM_VALUE_OPENING_SNOW_TOWN_FADE_PERIOD_0);
    fade(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_3, assets_.snow_palette, FF6_ROM_VALUE_OPENING_SNOW_FINAL_FADE_PERIOD_0);
    fade(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_4, assets_.armor_palette, FF6_ROM_VALUE_OPENING_ARMOR_FADE_PERIOD_0);
    // The three separately authored destinations must form this native span.
    static_assert(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_6 == FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_5 + 1U &&
                  FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_7 == FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_5 + 2U);
    for (unsigned i = 0; i < 3; ++i)
        fade(i + FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_5, assets_.landscape_palettes[i],
             FF6_ROM_VALUE_OPENING_SNOW_LANDSCAPE_FADE_PERIOD_0);
    fade(FF6_ROM_VALUE_OPENING_INITIAL_FADE_ROUTES_8, assets_.town_light, FF6_ROM_VALUE_OPENING_SNOW_TOWN_FADE_PERIOD_0);
    create(0, SnowSpawner{});
    create(0, Scroll{});
    create(0, Move{0, 0, FF6_ROM_VALUE_OPENING_MARCH_DELAYS_0, FF6_ROM_VALUE_OPENING_MARCH_DURATIONS_0, 0, FF6_ROM_VALUE_OPENING_MARCH_RATES_0});
    create(0, Move{1, 0, FF6_ROM_VALUE_OPENING_MARCH_DELAYS_1, FF6_ROM_VALUE_OPENING_MARCH_DURATIONS_1, 0, FF6_ROM_VALUE_OPENING_MARCH_RATES_1});
    create(0, Move{2, 0, FF6_ROM_VALUE_OPENING_MARCH_DELAYS_2, FF6_ROM_VALUE_OPENING_MARCH_DURATIONS_2, 0, FF6_ROM_VALUE_OPENING_MARCH_RATES_2});
    state_.stage = OpeningStage::approach;
}
std::uint8_t OpeningAnimation::random() {
    state_.random_index = wrap8(state_.random_index + 1U);
    return assets_.random[wrap8(unsigned{state_.frame_counter} + state_.random_index)];
}
void OpeningAnimation::draw(std::size_t resource, std::uint8_t x, std::uint8_t y) {
    require(resource < assets_.sprite_frames.size(), "Invalid opening sprite frame");
    for (auto sprite : assets_.sprite_frames[resource]) {
        if (sprites_.size() == 128)
            break;
        sprite.image.x = wrap8(static_cast<unsigned>(sprite.image.x) + x);
        const auto vertical = wrap8(static_cast<unsigned>(sprite.image.y) + y);
        // These 8/16-pixel pieces cannot straddle both edges of a 224-line view.
        // Positions near the byte wrap therefore have one negative native copy.
        sprite.image.y = vertical >= 224 ? static_cast<int>(vertical) - 256 : vertical;
        sprites_.push_back(std::move(sprite));
    }
}
void OpeningAnimation::caption(const OpeningCaption& caption) {
    require(caption.scroll < assets_.caption_scrolls.size(), "Invalid opening caption scroll");
    for (unsigned i = 0; i < 4; ++i)
        live_text_scroll_[i] =
            static_cast<std::uint8_t>(255U - assets_.caption_scrolls[caption.scroll][i]);
    auto next = text_;
    std::size_t origin = caption.cell, column = 0;
    bool finished = false;
    for (const auto glyph : caption.glyphs) {
        if (!glyph) {
            finished = true;
            break;
        }
        if (glyph >= 254) {
            origin += glyph == 254 ? 64 : 32;
            column = 0;
        } else {
            require(origin < next.size() && column < next.size() - origin,
                    "Opening caption exceeds its canvas");
            const unsigned value = glyph | (unsigned{caption.attributes} << 8U);
            next[origin + column++] = {value & 1023U, (value >> 10U) & 7U,
                                       static_cast<std::uint16_t>((value & 8192U) ? 14 : 1),
                                       (value & 16384U) != 0, (value & 32768U) != 0};
        }
    }
    require(finished, "Unterminated opening caption");
    text_ = next;
}
bool OpeningAnimation::run(std::size_t index) {
    auto& slot = slots_[index];
    return std::visit(
        [&](auto& task) -> bool {
            using T = std::decay_t<decltype(task)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return false;
            else if constexpr (std::is_same_v<T, Fade>) {
                task.task.advance(state_.colors[task.group], task.target.colors);
                return !task.task.finished;
            } else if constexpr (std::is_same_v<T, Bob>) {
                if (!task.initialized) {
                    task.initialized = true;
                    task.wait = FF6_ROM_VALUE_OPENING_BOB_INITIAL_WAIT_0;
                }
                if (!task.wait) {
                    ++task.frame;
                    require(task.frame < assets_.armor_offsets.size(), "Invalid armor bob cycle");
                    task.wait = assets_.armor_offsets[task.frame].duration;
                }
                if (task.wait == 255) {
                    task.frame = 0;
                    task.wait = assets_.armor_offsets[0].duration;
                }
                require(task.wait > 0 && task.wait < 255, "Invalid armor bob duration");
                --task.wait;
                state_.bob = assets_.armor_offsets[task.frame].value;
            } else if constexpr (std::is_same_v<T, Armor>) {
                const auto& part = assets_.armor[task.part];
                if (!task.initialized) {
                    task.initialized = true;
                    task.cursor = {0, wrap8(part.animation[0].duration + task.actor * 8U)};
                }
                advance_cutscene_animation(task.cursor, part.animation);
                // The source carries the byte offset addition into the anchor.
                const auto ox = add8(0, part.offset_x, false);
                const auto oy = add8(state_.bob, part.offset_y, false);
                const auto x = add8(ox.value, state_.armor_x[task.actor], ox.carry).value;
                const auto y = add8(oy.value, state_.armor_y[task.actor], oy.carry).value;
                draw(part.animation[task.cursor.frame].resource, x, y);
            } else if constexpr (std::is_same_v<T, SnowSpawner>) {
                if (state_.active_tasks < FF6_ROM_VALUE_OPENING_SNOW_TASK_LIMIT_0) {
                    const auto animation = assets_.snow_selection[state_.frame_counter & 7U];
                    const auto x = random(), y = random();
                    const auto i = create(
                        0, Snow{animation, 0, 0, static_cast<std::uint16_t>(((y & FF6_ROM_VALUE_OPENING_SNOW_HORIZONTAL_MASK_0) + 1U) * 257U),
                                static_cast<std::uint16_t>(((x & FF6_ROM_VALUE_OPENING_SNOW_VERTICAL_MASK_0) + 1U) * 256U + y)});
                    auto& snow = std::get<Snow>(slots_[i].task);
                    snow.x = static_cast<std::uint16_t>((unsigned{x} << 8U) | slots_[i].particle_x);
                    snow.y = static_cast<std::uint16_t>((unsigned{y} << 8U) | slots_[i].particle_y);
                }
            } else if constexpr (std::is_same_v<T, Snow>) {
                task.x = wrap16(unsigned{task.x} + task.dx);
                task.y = wrap16(unsigned{task.y} + task.dy);
                slot.particle_x = wrap8(task.x);
                slot.particle_y = wrap8(task.y);
                require(task.animation < assets_.snow.size(), "Invalid snow animation");
                draw(assets_.snow[task.animation][0].resource,
                     static_cast<std::uint8_t>(task.x >> 8U),
                     static_cast<std::uint8_t>(task.y >> 8U));
            } else if constexpr (std::is_same_v<T, Scroll>) {
                if ((state_.frame_counter & FF6_ROM_VALUE_OPENING_LANDSCAPE_SCROLL_MASK_0) == 0) {
                    state_.center_y = wrap16(state_.center_y + 65535U);
                    state_.landscape_scroll = wrap16(state_.landscape_scroll + 65535U);
                }
                if ((state_.frame_counter & FF6_ROM_VALUE_OPENING_TOWN_SCROLL_MASK_0) == 0)
                    state_.town_scroll = wrap16(state_.town_scroll + 1U);
            } else if constexpr (std::is_same_v<T, Move>) {
                if (task.phase == 0) {
                    task.phase = 1;
                    task.y = static_cast<std::uint16_t>(unsigned{state_.armor_y[task.actor]} << 8U);
                    slot.particle_y = 0;
                }
                if (task.phase == 1) {
                    if (task.wait)
                        --task.wait;
                    else {
                        task.phase = 2;
                        task.wait = task.duration;
                    }
                } else if (task.wait) {
                    task.y = wrap16(unsigned{task.y} + task.rate);
                    state_.armor_y[task.actor] = static_cast<std::uint8_t>(task.y >> 8U);
                    slot.particle_y = wrap8(task.y);
                    --task.wait;
                } else
                    return false;
            } else if constexpr (std::is_same_v<T, Caption>) {
                if (!task.initialized) {
                    task.initialized = true;
                    task.cursor = {0, assets_.caption_animation.front().duration};
                }
                if (task.cursor.wait == FF6_ROM_VALUE_OPENING_CAPTION_FADE_IN_CHECKPOINT_0)
                    fade(0, assets_.font, FF6_ROM_VALUE_OPENING_CAPTION_FADE_PERIOD_0);
                if (task.cursor.wait == FF6_ROM_VALUE_OPENING_CAPTION_FADE_OUT_CHECKPOINT_0)
                    fade(0, assets_.black, FF6_ROM_VALUE_OPENING_CAPTION_FADE_PERIOD_0);
                if (task.cursor.wait == 254)
                    return false;
                advance_cutscene_animation(task.cursor, assets_.caption_animation);
                const auto resource = assets_.caption_animation[task.cursor.frame].resource;
                require(resource < assets_.captions.size(), "Invalid opening caption resource");
                caption(assets_.captions[resource]);
            } else if constexpr (std::is_same_v<T, TownLights>) {
                state_.colors[FF6_ROM_VALUE_OPENING_TOWN_LIGHT_PALETTE_0] = (state_.frame_counter & FF6_ROM_VALUE_OPENING_TOWN_LIGHT_MASK_0) ? assets_.town_dark.colors
                                                               : assets_.town_light.colors;
            } else if constexpr (std::is_same_v<T, Brightness>) {
                state_.intensity = task.level--;
            }
            return true;
        },
        slot.task);
}
void OpeningAnimation::prepare(bool advance_stage) {
    if (state_.fading && !state_.remaining) {
        state_.complete = true;
        return;
    }
    if (!state_.fading) {
        const bool terminal = state_.stage == OpeningStage::finished;
        if (advance_stage) {
            switch (state_.stage) {
            case OpeningStage::approach:
                if (!state_.remaining) {
                    state_.stage = OpeningStage::credits;
                    state_.remaining = FF6_ROM_VALUE_OPENING_PHASE_WAITS_1;
                    create(0, Caption{});
                    create(0, TownLights{});
                }
                live_center_y_ = state_.center_y;
                break;
            case OpeningStage::credits:
                if (!state_.remaining)
                    state_.stage = OpeningStage::ending;
                live_center_y_ = state_.center_y;
                break;
            case OpeningStage::ending:
                state_.stage = OpeningStage::finished;
                break;
            case OpeningStage::finished:
                break;
            default:
                throw std::logic_error("Uninitialized opening stage");
            }
        }
        if (terminal || state_.input.pressed) {
            if (!terminal)
                state_.skipped = true;
            state_.fading = true;
            state_.remaining = FF6_ROM_VALUE_OPENING_EXIT_WAIT_0;
            create(0, Brightness{});
        }
    }
    sprites_.clear();
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (std::holds_alternative<std::monostate>(slots_[i].task))
            continue;
        if (!run(i)) {
            slots_[i].task = std::monostate{};
            --state_.active_tasks;
        }
    }
}
bool OpeningAnimation::advance(bool held_a) {
    const bool initial = !started_;
    if (initial) {
        initialize();
        started_ = true;
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
    published_text_ = text_;
    if (initial) {
        state_.remaining = FF6_ROM_VALUE_OPENING_PHASE_WAITS_0;
        state_.intensity = 15;
    }
    prepare(!initial);
    return true;
}
RasterScene OpeningAnimation::scene() const {
    require(published_.visits && !published_.complete, "Opening has no published frame");
    RasterScene scene;
    scene.intensity.assign(224, published_.intensity);
    scene.backdrop = published_.colors[0][0];
    scene.layers = {assets_.town, assets_.town, assets_.text};
    auto& underlay = scene.layers[0];
    underlay.cells.assign(1024, RasterCell{0, 0, 7});
    underlay.operand = false;
    auto& town = scene.layers[1];
    auto& text = scene.layers[2];
    text.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_OPENING_TEXT_ARITHMETIC_0),
                static_cast<ColorBlend>(FF6_ROM_VALUE_OPENING_TEXT_ARITHMETIC_0)};
    std::ranges::copy(published_text_, text.cells.begin() + FF6_ROM_VALUE_OPENING_CAPTION_CELL_ORIGIN_0);
    for (unsigned group = 0; group < 8; ++group)
        for (unsigned color = 0; color < 16; ++color) {
            underlay.palettes[group][color] = town.palettes[group][color] =
                published_.colors[group][color];
            text.palettes[group][color] = published_.colors[group / 4][(group % 4) * 4 + color % 4];
        }
    scene.affine_layers.push_back(assets_.landscape);
    auto& landscape = scene.affine_layers[0];
    landscape.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_OPENING_LANDSCAPE_ARITHMETIC_0),
                     static_cast<ColorBlend>(FF6_ROM_VALUE_OPENING_LANDSCAPE_ARITHMETIC_0)};
    scene.operand_fallback_rows.assign(224, 0);
    // OpeningFixedColorHDMATbl's semantic gray bands. Their lengths count
    // output rows; the background's one-based source y is a separate origin.
    constexpr std::array<unsigned, 14> lengths{
#include "rom-materials/opening_gray_lengths.inc"
};
    constexpr std::array<unsigned, 14> shades{
#include "rom-materials/opening_gray_shades.inc"
};
    unsigned line = 0;
    for (unsigned band = 0; band < lengths.size(); ++band)
        for (unsigned i = 0; i < lengths[band]; ++i, ++line)
            if (line < 224)
                scene.operand_fallback_rows[line] = static_cast<Rgb555>(shades[band] * 1057U);
    for (unsigned group = 0; group < 16; ++group)
        for (unsigned color = 0; color < 16; ++color)
            landscape.colors[group * 16 + color] = published_.colors[group][color];
    // The original coefficient lanes must cover the same number of rows.
    static_assert(FF6_ROM_VALUE_OPENING_PERSPECTIVE_BAND_ROWS_0 == FF6_ROM_VALUE_OPENING_PERSPECTIVE_BAND_ROWS_2 &&
                  FF6_ROM_VALUE_OPENING_PERSPECTIVE_BAND_ROWS_1 == FF6_ROM_VALUE_OPENING_PERSPECTIVE_BAND_ROWS_3);
    const auto scales = make_perspective_scales({});
    for (unsigned y = 0; y < 224; ++y) {
        // Bands are expressed as scene ownership, never as a transfer program.
        const bool upper = y <
#include "rom-materials/opening_gray_lengths.first.inc"
        ;
        underlay.lines[y].enabled = town.lines[y].enabled = text.lines[y].enabled = upper;
        underlay.lines[y].offset_x = FF6_ROM_VALUE_OPENING_INITIAL_CENTER_Y_0;
        underlay.lines[y].offset_y = 1 + signed16(published_.landscape_scroll);
        town.lines[y].offset_y = 1 + published_.town_scroll;
        const unsigned band = y < FF6_ROM_VALUE_OPENING_CAPTION_TOP_BAND_0 ? 0
            : std::min(3U, 1U + (y - FF6_ROM_VALUE_OPENING_CAPTION_TOP_BAND_0) / FF6_ROM_VALUE_OPENING_CAPTION_BAND_HEIGHT_0);
        text.lines[y].offset_x = live_text_scroll_[band];
        text.lines[y].offset_y = 1;
        if (!upper) {
            // Source pointer displacements determine the coefficient prefix.
            // Tiled bands hide it; the final programmed row remains held.
            const auto row = std::min(y, FF6_ROM_VALUE_OPENING_PERSPECTIVE_BAND_ROWS_0 +
                                        FF6_ROM_VALUE_OPENING_PERSPECTIVE_BAND_ROWS_1 - 1U) -
                             FF6_ROM_VALUE_OPENING_PERSPECTIVE_PREFIX_0;
            landscape.lines[y] = make_perspective_line(
                {FF6_ROM_VALUE_OPENING_PERSPECTIVE_CENTER_X_0, coordinate13(live_center_y_), FF6_ROM_VALUE_OPENING_INITIAL_CENTER_Y_0,
                 coordinate13(published_.landscape_scroll)},
                scales.rows[row], static_cast<std::uint16_t>(y + 1));
        }
    }
    scene.objects = SpriteLayer{};
    scene.objects->lines.resize(224);
    for (const auto& sprite : published_sprites_) {
        auto image = sprite.image;
        image.colors = published_.colors[8 + sprite.palette];
        scene.objects->sprites.push_back(std::move(image));
    }
    return scene;
}
} // namespace ff6
