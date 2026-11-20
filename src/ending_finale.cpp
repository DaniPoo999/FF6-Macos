#include "ff6/ending_finale.hpp"
#include <algorithm>
#include "rom-materials/ending_final_star_extent.macros.inc"
#include "rom-materials/ending_final_star_group_size.macros.inc"
#include "rom-materials/ending_final_star_group_mask.macros.inc"
#include "rom-materials/ending_airship_hull_tile_origin.macros.inc"
#include "rom-materials/ending_airship_misc_tile_origin.macros.inc"
#include "rom-materials/ending_final_tile_runs.macros.inc"
#include "rom-materials/ending_final_graphics_first_extent.macros.inc"
#include "rom-materials/ending_final_graphics_second_extent.macros.inc"
#include "rom-materials/ending_final_tile_extent.macros.inc"
#include "rom-materials/ending_final_text_palette.macros.inc"
#include "rom-materials/ending_final_star_palette.macros.inc"
#include "rom-materials/ending_final_window_left.macros.inc"
#include "rom-materials/ending_final_window_right.macros.inc"
#include "rom-materials/ending_final_window_span.macros.inc"
#include "rom-materials/ending_final_window_cycle_origin.macros.inc"
#include "rom-materials/ending_final_window_height.macros.inc"
#include "rom-materials/ending_final_background_x.macros.inc"
#include "rom-materials/ending_final_blank_tile.macros.inc"
#include "rom-materials/ending_final_background_palette.macros.inc"
#include "rom-materials/ending_final_foreground_palette.macros.inc"
#include "rom-materials/ending_final_layer_priority.macros.inc"
#include "rom-materials/ending_final_star_priority.macros.inc"
#include "rom-materials/ending_final_scroll_period.macros.inc"
#include "rom-materials/ending_final_reveal_duration.macros.inc"
#include "rom-materials/ending_final_second_cue.macros.inc"
#include "rom-materials/ending_final_text_fade.macros.inc"
#include "rom-materials/ending_final_silence.macros.inc"
#include "rom-materials/ending_final_star_fade.macros.inc"
#include "rom-materials/ending_final_music_mute.macros.inc"
#include "rom-materials/ending_final_music_resume.macros.inc"
#include "rom-materials/ending_final_ripple_period.macros.inc"
#include "rom-materials/ending_final_ripple_initial.macros.inc"
#include "rom-materials/ending_final_ripple_cycle.macros.inc"
#include "rom-materials/ending_final_star_limits.macros.inc"
#include "rom-materials/ending_final_star_tiles.macros.inc"
#include "rom-materials/ending_airship_camera_x.macros.inc"
#include "rom-materials/ending_airship_camera_y.macros.inc"
#include "rom-materials/ending_airship_depth.macros.inc"
#include "rom-materials/ending_airship_clip_left.macros.inc"
#include "rom-materials/ending_airship_clip_right.macros.inc"
#include "rom-materials/ending_airship_palette_routes.macros.inc"
#include "rom-materials/ending_airship_haze_blue.macros.inc"
#include "rom-materials/ending_airship_haze_red_green.macros.inc"
#include "rom-materials/ending_airship_haze_blue_step.macros.inc"
#include "rom-materials/ending_airship_haze_red_green_step.macros.inc"
#include "rom-materials/ending_airship_haze_blend.macros.inc"
#include "rom-materials/ending_airship_haze_backdrop.macros.inc"
#include "rom-materials/ending_airship_fade_x.macros.inc"
#include "rom-materials/ending_airship_rotor_step.macros.inc"
#include "rom-materials/ending_airship_rotor_slow.macros.inc"
#include "rom-materials/ending_airship_rotor_fast.macros.inc"
#include "rom-materials/ending_airship_bob_end.macros.inc"
#include "rom-materials/ending_airship_bob_up.macros.inc"
#include "rom-materials/ending_airship_bob_down.macros.inc"
#include "rom-materials/ending_airship_horizontal_base.macros.inc"
#include "rom-materials/ending_airship_horizontal_total.macros.inc"
#include "rom-materials/ending_airship_vertical_acceleration.macros.inc"
#include "rom-materials/ending_airship_drift_mask.macros.inc"
#include "rom-materials/ending_airship_drift_base.macros.inc"
#include "rom-materials/ending_airship_period_mask.macros.inc"
#include "rom-materials/ending_airship_spread_mask.macros.inc"
#include "rom-materials/ending_airship_trail_y.macros.inc"
#include "rom-materials/ending_airship_angle.macros.inc"

namespace ff6 {
namespace {
void require(bool value, const char* why) { if (!value) throw std::invalid_argument(why); }
int signed_byte(std::uint8_t value) { return value < 128 ? int(value) : int(value) - 256; }
CutscenePalette faded(const CutscenePalette& colors, unsigned fraction, unsigned denominator) {
    CutscenePalette out{};
    for (unsigned i = 0; i < 16; ++i)
        for (unsigned shift : {0U, 5U, 10U})
            out[i] |= static_cast<Rgb555>((((colors[i] >> shift) & 31U) * fraction / denominator) << shift);
    return out;
}
}
EndingAirshipAssets load_ending_airship_assets(const ContentPack& pack) {
    EndingAirshipAssets out;
    // C3/D57F uses the same eight land blocks and indexed tile palette mapping
    // as the staff-credit landscape. Only this scene's colors/camera differ.
    out.terrain = load_ending_flight_assets(pack, EndingFlightScene::landscape).terrain;
    const auto palette = pack.get("world_graphics_ending_airship_pal_pal");
    for (unsigned i = 0; i < 128; ++i) out.terrain.colors[i] = read16(palette, i * 2) & 32767U;
    out.terrain.colors[0] = 0;
    const auto colors = pack.get("world_graphics_world_2_sprite_pal_pal");
    std::array<CutscenePalette, 8> object{};
    for (unsigned i = 0; i < 128; ++i) object[i / 16][i % 16] = read16(colors, i * 2) & 32767U;
    object[FF6_ROM_VALUE_ENDING_AIRSHIP_PALETTE_ROUTES_1] = object[FF6_ROM_VALUE_ENDING_AIRSHIP_PALETTE_ROUTES_0];
    object[FF6_ROM_VALUE_ENDING_AIRSHIP_PALETTE_ROUTES_3] = object[FF6_ROM_VALUE_ENDING_AIRSHIP_PALETTE_ROUTES_2];
    const auto hull = decode_tiles(decompress_lzss(pack.get("world_graphics_airship_2_gfx_4bpp_lz")), 4);
    const auto misc = decode_tiles(decompress_lzss(pack.get("world_graphics_world_misc_sprite_gfx_4bpp_lz")), 4);
    const auto frames = pack.get("world_sprite_frames"), trails = pack.get("world_falcon_splash_frames");
    std::ranges::copy(slice(trails, 0, FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_PHASES_0), out.trail_frames.begin());
    for (unsigned image = 0; image < out.images.size(); ++image) {
        if (image != FF6_ROM_VALUE_ENDING_AIRSHIP_FRAMES_0 && image != FF6_ROM_VALUE_ENDING_AIRSHIP_FRAMES_0 + FF6_ROM_VALUE_ENDING_AIRSHIP_ROTOR_STEP_0 && std::find(out.trail_frames.begin(), out.trail_frames.end(), image) == out.trail_frames.end()) continue;
        if (!image) continue;
        const unsigned at = 216 + read16(frames, image * 2), count = slice(frames, at, 1)[0];
        for (unsigned i = 0; i < count; ++i) {
            const auto piece = slice(frames, at + 1 + i * 4, 4);
            const unsigned flags = piece[3], tile = piece[2] + (flags & 1U) * 256;
            // Resource destinations come from the ending loader. The upper tile
            // bit selects the encoded sprite bank; it is not a scene choice.
            require((tile >= FF6_ROM_VALUE_ENDING_AIRSHIP_HULL_TILE_ORIGIN_0 && tile < 256)
                || tile >= FF6_ROM_VALUE_ENDING_AIRSHIP_MISC_TILE_ORIGIN_0, "Unowned final-airship sprite tile");
            const auto& pixels = tile < 256
                ? hull.at(tile - FF6_ROM_VALUE_ENDING_AIRSHIP_HULL_TILE_ORIGIN_0)
                : misc.at(tile - FF6_ROM_VALUE_ENDING_AIRSHIP_MISC_TILE_ORIGIN_0);
            RasterSprite sprite;
            sprite.x = signed_byte(piece[0]); sprite.y = signed_byte(piece[1]);
            sprite.width = sprite.height = 8; sprite.depth = 4;
            sprite.pixels.assign(pixels.begin(), pixels.end()); sprite.colors = object[(flags >> 1) & 7U];
            sprite.flip_x = (flags & 64U) != 0; sprite.flip_y = (flags & 128U) != 0;
            // World OBJ palettes 0..3 do not participate in the fixed fog blend.
            sprite.allow_color_effect = ((flags >> 1) & 7U) >= 4;
            out.images[image].push_back(std::move(sprite));
        }
    }
    std::ranges::copy(slice(pack.get("encounter_rng_tbl_bin"), 0, 256), out.random.begin());
    const auto sine = pack.get("data_effef1_world_sine");
    // The sine table uses degrees; the perpendicular component is a quarter turn.
    out.travel_x = slice(sine, FF6_ROM_VALUE_ENDING_AIRSHIP_ANGLE_0, 1)[0];
    out.travel_y = slice(sine, FF6_ROM_VALUE_ENDING_AIRSHIP_ANGLE_0 + 90, 1)[0];
    return out;
}
EndingAirship::EndingAirship(EndingAirshipAssets assets, std::uint8_t random)
    : assets_(std::move(assets)), random_(random) {
    require(assets_.terrain.cells.size() == 16384, "Incomplete final-airship landscape");
}
bool EndingAirship::advance() {
    if (finished_) return false;
    ++updates_;
    // EE/552F–570D: bob, accelerate to the upper left, increase ground speed.
    // ASL5 expresses the ramp in fixed-point steps, including its initial speed.
    if (updates_ >= FF6_ROM_VALUE_ENDING_AIRSHIP_SPEED_CURVE_1 && updates_ < FF6_ROM_VALUE_ENDING_AIRSHIP_SPEED_CURVE_0)
        speed_ = FF6_ROM_VALUE_ENDING_AIRSHIP_SPEED_CURVE_2 * 32 + (updates_ - FF6_ROM_VALUE_ENDING_AIRSHIP_SPEED_CURVE_1) * 32;
    if (!--rotor_wait_) {
        rotor_ ^= FF6_ROM_VALUE_ENDING_AIRSHIP_ROTOR_STEP_0;
        rotor_wait_ = updates_ >= FF6_ROM_VALUE_ENDING_AIRSHIP_SPEED_CURVE_1
            ? FF6_ROM_VALUE_ENDING_AIRSHIP_ROTOR_FAST_0 : FF6_ROM_VALUE_ENDING_AIRSHIP_ROTOR_SLOW_0;
    }
    picture_x_ = x_ / 256; picture_y_ = y_ / 256; frame_ = FF6_ROM_VALUE_ENDING_AIRSHIP_FRAMES_0 + rotor_;
    x_ = static_cast<std::uint16_t>(int(x_) + vx_);
    y_ = static_cast<std::uint16_t>(int(y_) + vy_);
    vx_ -= FF6_ROM_VALUE_ENDING_AIRSHIP_HORIZONTAL_BASE_0;
    if (updates_ < FF6_ROM_VALUE_ENDING_AIRSHIP_BOB_END_0)
        vy_ += y_ < FF6_ROM_VALUE_ENDING_AIRSHIP_ORIGIN_1
            ? FF6_ROM_VALUE_ENDING_AIRSHIP_BOB_UP_0 : -FF6_ROM_VALUE_ENDING_AIRSHIP_BOB_DOWN_0;
    else {
        // The earlier base acceleration has already been applied.
        vx_ -= FF6_ROM_VALUE_ENDING_AIRSHIP_HORIZONTAL_TOTAL_0 - FF6_ROM_VALUE_ENDING_AIRSHIP_HORIZONTAL_BASE_0;
        vy_ -= FF6_ROM_VALUE_ENDING_AIRSHIP_VERTICAL_ACCELERATION_0;
    }
    for (unsigned i = 0; i < FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_LAYOUT_0; ++i) {
        auto& trail = trail_[first_ + i];
        if (trail.phase && !--trail.wait) { trail.phase = (trail.phase + 1) % FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_PHASES_0; trail.wait = trail.period; }
        trail.x = (trail.x + drift_) & 65535U;
        published_trail_[i] = trail;
    }
    ++random_;
    for (unsigned i = 0; i < FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_LAYOUT_0; ++i) {
        auto& trail = trail_[first_ + i];
        if (trail.phase) continue;
        const auto noise = [&](unsigned offset) { return assets_.random[(unsigned(random_) + offset) & 255U]; };
        drift_ = (noise(1) & FF6_ROM_VALUE_ENDING_AIRSHIP_DRIFT_MASK_0) + FF6_ROM_VALUE_ENDING_AIRSHIP_DRIFT_BASE_0;
        trail.period = trail.wait = (noise(2) & FF6_ROM_VALUE_ENDING_AIRSHIP_PERIOD_MASK_0) + 1;
        trail.y = (noise(3) & FF6_ROM_VALUE_ENDING_AIRSHIP_SPREAD_MASK_0) + FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_Y_0;
        // InitHWRegs clears the shared emitter bias before this entry.
        trail.x = x_ / 256; trail.phase = 1; break;
    }
    first_ = FF6_ROM_VALUE_ENDING_AIRSHIP_TRAIL_LAYOUT_0 - first_;
    camera_x_ = (camera_x_ - assets_.travel_x * speed_ / 256) & 0xfffffU;
    camera_y_ = (camera_y_ + assets_.travel_y * speed_ / 256) & 0xfffffU;
    const unsigned target = x_ < FF6_ROM_VALUE_ENDING_AIRSHIP_FADE_X_0 ? 0 : 15;
    if (brightness_ < target) ++brightness_;
    else if (brightness_ > target) --brightness_;
    finished_ = !target && !brightness_;
    return true;
}
RasterScene EndingAirship::scene() const {
    RasterScene out;
    auto terrain = assets_.terrain;
    const int x = int(camera_x_ / 256), y = int(camera_y_ / 256);
    const PerspectiveView view{static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                              static_cast<std::int16_t>(x - FF6_ROM_VALUE_ENDING_AIRSHIP_CAMERA_X_0),
                              static_cast<std::int16_t>(y - FF6_ROM_VALUE_ENDING_AIRSHIP_CAMERA_Y_0)};
    const auto scales = make_perspective_scales({224, FF6_ROM_VALUE_ENDING_AIRSHIP_DEPTH_0,
                                                 FF6_ROM_VALUE_ENDING_AIRSHIP_DEPTH_1, FF6_ROM_VALUE_ENDING_AIRSHIP_DEPTH_2});
    for (unsigned row = 0; row < 224; ++row)
        terrain.lines[row] = make_perspective_line(view, scales.rows[row], static_cast<std::uint16_t>(row + 1));
    out.affine_layers.push_back(std::move(terrain));
    out.intensity.assign(224, static_cast<std::uint8_t>(brightness_));
    out.objects.emplace(); out.objects->lines.resize(224);
    for (auto& line : out.objects->lines) {
        line.left = FF6_ROM_VALUE_ENDING_AIRSHIP_CLIP_LEFT_0;
        line.right = FF6_ROM_VALUE_ENDING_AIRSHIP_CLIP_RIGHT_0 + 1; // Inclusive source edge, half-open native interval.
    }
    auto draw = [&](unsigned frame, unsigned px, unsigned py) {
        for (auto sprite : assets_.images.at(frame)) {
            sprite.x = int((unsigned(sprite.x) + px) & 255U);
            sprite.y = int((unsigned(sprite.y) + py) & 255U) - 1;
            if (sprite.y >= 224) sprite.y -= 256;
            out.objects->sprites.push_back(std::move(sprite));
        }
    };
    for (const auto& trail : published_trail_) draw(assets_.trail_frames[trail.phase], trail.x, trail.y);
    draw(frame_, picture_x_, picture_y_);
    // EE/1930's sunset haze is a scene color ramp, not a display-device model.
    out.color_effect_rows.resize(224);
    for (unsigned row = 0; row < FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_RED_GREEN_0 / FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_RED_GREEN_STEP_0; ++row) {
        const unsigned blue = (FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_BLUE_0 - row * FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_BLUE_STEP_0) / 256;
        const unsigned rg = (FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_RED_GREEN_0 - row * FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_RED_GREEN_STEP_0) / 256;
        out.color_effect_rows[row] = RasterColorEffect{static_cast<Rgb555>(rg + (rg << 5) + (blue << 10)),
            static_cast<ColorBlend>(FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_BLEND_0), FF6_ROM_VALUE_ENDING_AIRSHIP_HAZE_BACKDROP_0 != 0};
    }
    return out;
}
TheEndAssets load_the_end_assets(const ContentPack& pack) {
    TheEndAssets out;
    auto graphics = pack.get("cutscene_assets_the_end_1_4bpp");
    out.tiles = decode_tiles(slice(graphics, 0, FF6_ROM_VALUE_ENDING_FINAL_GRAPHICS_FIRST_EXTENT_0), 4);
    const auto tail = decode_tiles(slice(pack.get("cutscene_assets_the_end_2_4bpp"), 0, FF6_ROM_VALUE_ENDING_FINAL_GRAPHICS_SECOND_EXTENT_0), 4);
    out.tiles.insert(out.tiles.end(), tail.begin(), tail.end());
    const auto colors = pack.get("cutscene_assets_the_end_pal_pal");
    for (unsigned i = 0; i < 128; ++i) out.palettes[i / 16][i % 16] = read16(colors, i * 2) & 32767U;
    std::ranges::copy(slice(pack.get("encounter_rng_tbl_bin"), 0, 256), out.random.begin());
    return out;
}
TheEnd::TheEnd(TheEndAssets assets, std::uint8_t random) : assets_(std::move(assets)), random_(random) {
    // Both original star traversals select the same extent in four-byte sprite
    // records. Reserved storage is a format limit; the authored active extent
    // also controls which records are drawn.
    static_assert(FF6_ROM_VALUE_ENDING_FINAL_STAR_EXTENT_0 % 4 == 0);
    static_assert(FF6_ROM_VALUE_ENDING_FINAL_STAR_EXTENT_0 / 4 > 0 &&
                  FF6_ROM_VALUE_ENDING_FINAL_STAR_EXTENT_0 / 4 <= 128);
    require(assets_.tiles.size() >= FF6_ROM_VALUE_ENDING_FINAL_TILE_EXTENT_0, "Incomplete final-card graphics");
    for (unsigned i = FF6_ROM_VALUE_ENDING_FINAL_STAR_EXTENT_0 / 4; i; --i) initialize(stars_[i - 1]);
    random_ = 0; // TheEnd explicitly resets the respawn seed after initial setup.
}
void TheEnd::initialize(Star& star) {
    const unsigned first = assets_.random[random_++], second = assets_.random[first], third = assets_.random[second];
    star = {};
    star.vx = first & FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_0;
    star.vy = std::max(1U, second & FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_1);
    star.x = (first & FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_2) + FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_3;
    star.y = (second & FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_4) + FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_5;
    star.size = third & FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_6;
    star.growth = std::max(1U, third & FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_7);
    if ((star.vx | star.vy) >= FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_8 &&
        star.growth < FF6_ROM_VALUE_ENDING_FINAL_STAR_INITIAL_9) { star.growth *= 2; star.size /= 2; }
}
void TheEnd::move(Star& star) {
    const int x = int(star.x) + (star.x < 128 ? -int(star.vx) : int(star.vx));
    if (x < 0 || x >= 256) { star.valid = false; return; }
    star.x = static_cast<unsigned>(x);
    const int y = int(star.y) + (star.y < FF6_ROM_VALUE_ENDING_FINAL_STAR_LIMITS_0 ? -int(star.vy) : int(star.vy));
    if (y < 0 || y >= 224) { star.valid = false; return; }
    star.y = static_cast<unsigned>(y);
    star.size = std::min(255U, star.size + star.growth);
    if (star.size < FF6_ROM_VALUE_ENDING_FINAL_STAR_LIMITS_1) return;
    star.tile = star.size < FF6_ROM_VALUE_ENDING_FINAL_STAR_LIMITS_2 ? FF6_ROM_VALUE_ENDING_FINAL_STAR_TILES_2
        : star.size < FF6_ROM_VALUE_ENDING_FINAL_STAR_LIMITS_3 ? FF6_ROM_VALUE_ENDING_FINAL_STAR_TILES_1 : FF6_ROM_VALUE_ENDING_FINAL_STAR_TILES_0;
    const unsigned acceleration = star.size < FF6_ROM_VALUE_ENDING_FINAL_STAR_LIMITS_2 ? 0
        : star.size < FF6_ROM_VALUE_ENDING_FINAL_STAR_LIMITS_3 ? 1 : 2;
    if (acceleration != star.acceleration) {
        star.acceleration = acceleration;
        // LSR then ADC retains carry: add ceil(speed/2), not floor(speed/2).
        star.vx += (star.vx + 1) / 2; star.vy += (star.vy + 1) / 2;
    }
}
std::vector<MusicPlayerCommand> TheEnd::advance(std::optional<std::uint8_t> cue, bool song_finished) {
    std::vector<MusicPlayerCommand> audio;
    // The source selects one rotating group per update. Keep that authored
    // schedule in ROM data, independent of the native Star record layout.
    const unsigned group = (updates_ % (FF6_ROM_VALUE_ENDING_FINAL_STAR_GROUP_MASK_0 + 1))
        * FF6_ROM_VALUE_ENDING_FINAL_STAR_GROUP_SIZE_0;
    for (unsigned i = group; i < group + FF6_ROM_VALUE_ENDING_FINAL_STAR_GROUP_SIZE_0; ++i) move(stars_[i]);
    if (phase_ != TheEndPhase::reveal)
        for (unsigned i = FF6_ROM_VALUE_ENDING_FINAL_STAR_EXTENT_0 / 4; i; --i) if (!stars_[i - 1].valid) initialize(stars_[i - 1]);
    ++updates_; ++age_;
    auto next = [&](TheEndPhase phase) { phase_ = phase; age_ = 0; };
    switch (phase_) {
    case TheEndPhase::first_cue: if (cue && *cue != 0) next(TheEndPhase::reveal); break;
    case TheEndPhase::reveal:
        if (age_ % FF6_ROM_VALUE_ENDING_FINAL_SCROLL_PERIOD_0 == 0)
            scroll_ = FF6_ROM_VALUE_ENDING_FINAL_SCROLL_ORIGIN_0 - 1 + age_ / FF6_ROM_VALUE_ENDING_FINAL_SCROLL_PERIOD_0;
        if (age_ == FF6_ROM_VALUE_ENDING_FINAL_REVEAL_DURATION_0) next(TheEndPhase::second_cue);
        break;
    case TheEndPhase::second_cue: if (cue == FF6_ROM_VALUE_ENDING_FINAL_SECOND_CUE_0) next(TheEndPhase::lettering); break;
    case TheEndPhase::lettering:
        text_fade_ = age_;
        if (age_ == FF6_ROM_VALUE_ENDING_FINAL_TEXT_FADE_0) next(TheEndPhase::song_end);
        break;
    case TheEndPhase::song_end:
        if (song_finished) {
            // E5/F8D1 and F902 call ExecSound twice, explicitly invalidating
            // its current-song cache between them. Both must reach loading;
            // ordinary GameSong duplicate suppression would leave Prelude mute.
            audio.push_back(SongLoad{FF6_ROM_VALUE_ENDING_FINAL_MUSIC_MUTE_0 & 255, FF6_ROM_VALUE_ENDING_FINAL_MUSIC_MUTE_0 >> 8}); next(TheEndPhase::silence);
        }
        break;
    case TheEndPhase::silence:
        if (age_ == FF6_ROM_VALUE_ENDING_FINAL_SILENCE_0) { audio.push_back(SongLoad{FF6_ROM_VALUE_ENDING_FINAL_MUSIC_RESUME_0 & 255, FF6_ROM_VALUE_ENDING_FINAL_MUSIC_RESUME_0 >> 8}); next(TheEndPhase::stars); }
        break;
    case TheEndPhase::stars:
        star_fade_ = age_;
        if (age_ == FF6_ROM_VALUE_ENDING_FINAL_STAR_FADE_0) next(TheEndPhase::hold);
        break;
    case TheEndPhase::hold: age_ = 0; break;
    }
    return audio;
}
RasterScene TheEnd::scene() const {
    RasterScene out; out.intensity.assign(224, 15);
    constexpr std::array<int, FF6_ROM_VALUE_ENDING_FINAL_RIPPLE_CYCLE_0> wave{
#include "rom-materials/ending_ripple.inc"
    };
    for (unsigned background = 0; background < 2; ++background) {
        TileLayer layer;
        layer.columns = layer.rows = 32; layer.tiles = assets_.tiles;
        layer.palettes.assign(assets_.palettes.begin(), assets_.palettes.end());
        layer.palettes[FF6_ROM_VALUE_ENDING_FINAL_TEXT_PALETTE_0] = faded(assets_.palettes[FF6_ROM_VALUE_ENDING_FINAL_TEXT_PALETTE_0], text_fade_, FF6_ROM_VALUE_ENDING_FINAL_TEXT_FADE_0);
        layer.cells.resize(1024, RasterCell{FF6_ROM_VALUE_ENDING_FINAL_BLANK_TILE_0, FF6_ROM_VALUE_ENDING_FINAL_BACKGROUND_PALETTE_0, static_cast<std::uint16_t>(FF6_ROM_VALUE_ENDING_FINAL_LAYER_PRIORITY_0 + (background ? 1 : 2))});
        // Row geometry is selected from the original tilemap operands. The
        // private layout also drives complete native-fragment regeneration;
        // extracting only the counts would leave the unrolled columns behind.
        for (unsigned row = 0; row < (background ? FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_4 : FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_0); ++row)
            for (unsigned x = 0; x < (background ? FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_5 : FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_2); ++x) {
                const unsigned tile = background ? FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_6 + row * FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_7 + x
                    : row < FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_1 ? FF6_ROM_VALUE_ENDING_FINAL_BLANK_TILE_0
                    : (row - FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_1) * FF6_ROM_VALUE_ENDING_FINAL_TILE_RUNS_3 + x;
                layer.cells[row * 32 + x] = {
                    tile, static_cast<std::uint32_t>(background ? FF6_ROM_VALUE_ENDING_FINAL_FOREGROUND_PALETTE_0
                                                               : FF6_ROM_VALUE_ENDING_FINAL_BACKGROUND_PALETTE_0),
                    static_cast<std::uint16_t>(FF6_ROM_VALUE_ENDING_FINAL_LAYER_PRIORITY_0 + (background ? 1 : 2))};
            }
        layer.lines.resize(224);
        for (unsigned y = 0; y < 224; ++y) {
            auto& line = layer.lines[y]; line.enabled = y >= FF6_ROM_VALUE_ENDING_FINAL_WINDOW_CYCLE_ORIGIN_0 && y < FF6_ROM_VALUE_ENDING_FINAL_WINDOW_CYCLE_ORIGIN_0 + FF6_ROM_VALUE_ENDING_FINAL_WINDOW_HEIGHT_0;
            line.left = FF6_ROM_VALUE_ENDING_FINAL_WINDOW_LEFT_0; line.right = FF6_ROM_VALUE_ENDING_FINAL_WINDOW_RIGHT_0 + 1;
            line.offset_x = background ? FF6_ROM_VALUE_ENDING_FINAL_BACKGROUND_X_1 : FF6_ROM_VALUE_ENDING_FINAL_BACKGROUND_X_0;
            line.offset_y = 1 + (background ? wave[((updates_ ? updates_ - FF6_ROM_VALUE_ENDING_FINAL_RIPPLE_INITIAL_0 : 0) / FF6_ROM_VALUE_ENDING_FINAL_RIPPLE_PERIOD_0 + y + FF6_ROM_VALUE_ENDING_FINAL_WINDOW_CYCLE_ORIGIN_1) % FF6_ROM_VALUE_ENDING_FINAL_RIPPLE_CYCLE_0] : int(scroll_));
        }
        out.layers.push_back(std::move(layer));
    }
    out.objects.emplace(); out.objects->lines.resize(224);
    const auto colors = faded(assets_.palettes[FF6_ROM_VALUE_ENDING_FINAL_STAR_PALETTE_0], star_fade_, FF6_ROM_VALUE_ENDING_FINAL_STAR_FADE_0);
    for (const auto& star : std::span(stars_).first(FF6_ROM_VALUE_ENDING_FINAL_STAR_EXTENT_0 / 4)) {
        RasterSprite sprite; sprite.x = int(star.x); sprite.y = int(star.y) - 1;
        sprite.width = sprite.height = 8; sprite.depth = FF6_ROM_VALUE_ENDING_FINAL_STAR_PRIORITY_0 + 1; sprite.colors = colors;
        const auto& tile = assets_.tiles[star.tile]; sprite.pixels.assign(tile.begin(), tile.end());
        // The shared sprite layer supports a single interval per row. Resolve
        // this scene's inverted rectangle into its native sprite texels instead.
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                if (sprite.y + y >= FF6_ROM_VALUE_ENDING_FINAL_WINDOW_CYCLE_ORIGIN_0 && sprite.y + y < FF6_ROM_VALUE_ENDING_FINAL_WINDOW_CYCLE_ORIGIN_0 + FF6_ROM_VALUE_ENDING_FINAL_WINDOW_HEIGHT_0 &&
                    sprite.x + x >= FF6_ROM_VALUE_ENDING_FINAL_WINDOW_LEFT_0 && sprite.x + x < FF6_ROM_VALUE_ENDING_FINAL_WINDOW_LEFT_0 + FF6_ROM_VALUE_ENDING_FINAL_WINDOW_SPAN_0 + 1)
                    sprite.pixels[std::size_t(y * 8 + x)] = 0;
        out.objects->sprites.push_back(std::move(sprite));
    }
    return out;
}
} // namespace ff6
