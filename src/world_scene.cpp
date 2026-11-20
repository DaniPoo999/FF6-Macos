#include "rom-materials/world_castle_particle_strip.macros.inc"
#include "ff6/world_map.hpp"
#include "rom-materials/world_onfoot_camera_x_bias.macros.inc"
#include "rom-materials/world_onfoot_camera_y_bias.macros.inc"
#include "rom-materials/world_airship_primary_palette.macros.inc"
#include "rom-materials/world_falcon_primary_palette.macros.inc"
#include "rom-materials/world_dismount_character_palette.macros.inc"
#include "rom-materials/world_ship_palette_choices.macros.inc"
#include "rom-materials/world_airship_tile_address.macros.inc"
#include "rom-materials/world_minimap_tile_address.macros.inc"
#include "rom-materials/world_misc_tile_address.macros.inc"
#include "rom-materials/world_character_tile_address.macros.inc"
#include "rom-materials/world_minimap_position_local_tile.macros.inc"
#include "rom-materials/world_minimap_direction_local_tile.macros.inc"
#include "rom-materials/world_rider_copy_layout.macros.inc"
#include "rom-materials/world_flight_sprite_origin.macros.inc"
#include "rom-materials/world_judgment_copy_destination.macros.inc"
#include "rom-materials/world_judgment_copy_count.macros.inc"
#include "rom-materials/world_judgment_source_color.macros.inc"
#include "rom-materials/world_auxiliary_object_palette.macros.inc"
#include "rom-materials/world_smoke_source_palette.macros.inc"
#include "rom-materials/world_travel_character_palette.macros.inc"
#include "rom-materials/world_airship_overlay_palette.macros.inc"
#include "rom-materials/world_character_frame_base.macros.inc"
#include "rom-materials/world_character_forest_frame_shift.macros.inc"
#include "rom-materials/world_character_screen_origin.macros.inc"
#include "rom-materials/world_airship_ground_screen_x.macros.inc"
#include "rom-materials/falcon_wake_height.macros.inc"
#include "rom-materials/world_fog_vertical_origin.macros.inc"
#include "rom-materials/world_fog_horizontal_origin.macros.inc"
#include "rom-materials/world_fog_column_count.macros.inc"
#include "rom-materials/world_fog_row_layout.macros.inc"
#include "rom-materials/world_fog_sprite_styles.macros.inc"
#include "rom-materials/world_castle_sprite_y.macros.inc"
#include "rom-materials/world_castle_motion_step.macros.inc"
#include "rom-materials/world_castle_sprite_x.macros.inc"
#include "rom-materials/world_castle_image_base.macros.inc"
#include "rom-materials/world_travel_altitude_gain.macros.inc"
#include "rom-materials/world_ship_frame_base.macros.inc"
#include "rom-materials/world_ship_frame_period.macros.inc"
#include "rom-materials/world_ship_sprite_origin.macros.inc"
#include "rom-materials/world_travel_arrow_frames.macros.inc"
#include "rom-materials/world_travel_arrow_frame_bases.macros.inc"
#include "rom-materials/world_travel_arrow_x.macros.inc"
#include "rom-materials/world_travel_arrow_phase_period.macros.inc"
#include "rom-materials/world_parked_vertical_bounds.macros.inc"
#include "rom-materials/world_parked_horizontal_bias.macros.inc"
#include "rom-materials/world_parked_projection_factor.macros.inc"
#include "rom-materials/world_parked_projection_divisor.macros.inc"
#include "rom-materials/world_parked_horizontal_extent.macros.inc"
#include "rom-materials/world_parked_sprite_origin.macros.inc"
#include "rom-materials/world_parked_sprite_y_bias.macros.inc"
#include "rom-materials/world_parked_frame_choices.macros.inc"
#include "rom-materials/world_parked_frame_depth.macros.inc"
#include "rom-materials/world_water_tile_origins.macros.inc"
#include "rom-materials/world_water_scroll_rate.macros.inc"
#include "rom-materials/world_water_palette_cycle.macros.inc"
#include "rom-materials/world_minimap_origin.macros.inc"
#include "rom-materials/world_minimap_marker_origin.macros.inc"
#include "rom-materials/world_minimap_marker_tiles.macros.inc"
#include "rom-materials/world_minimap_palette.macros.inc"
#include "rom-materials/world_cinematic_projection_numerators.macros.inc"
#include "rom-materials/world_cinematic_projection_origin.macros.inc"
#include "rom-materials/world_cinematic_projection_step.macros.inc"
#include "rom-materials/world_onfoot_projection_origin.macros.inc"
#include "rom-materials/world_onfoot_projection_step.macros.inc"
#include "rom-materials/world_onfoot_projection_center.macros.inc"
#include "rom-materials/world_falcon_projection_curve.macros.inc"
#include "rom-materials/world_crash_projection_origin.macros.inc"
#include "rom-materials/world_vehicle_projection_numerator.macros.inc"
#include "rom-materials/world_vehicle_projection_denominator.macros.inc"
#include "rom-materials/world_altitude_projection_rate.macros.inc"
#include "rom-materials/world_projection_ends.macros.inc"
#include "rom-materials/world_panorama_angle_rate.macros.inc"
#include "rom-materials/world_panorama_vertical_origin.macros.inc"
#include "rom-materials/world_chocobo_blend_deadline.macros.inc"
#include "rom-materials/world_projection_first_span.macros.inc"
#include "rom-materials/world_viewport_edges.macros.inc"
#include "rom-materials/world_vehicle_camera_scale.macros.inc"
#include "rom-materials/world_vehicle_ground_origin.macros.inc"
#include "rom-materials/chocobo_riding_origin.macros.inc"
#include "rom-materials/chocobo_dismount_prepare_wait.macros.inc"
#include "rom-materials/chocobo_dismount_prepare_frame.macros.inc"
#include "rom-materials/chocobo_dismount_linear.macros.inc"
#include "rom-materials/chocobo_dismount_origin_x.macros.inc"
#include "rom-materials/chocobo_dismount_cycle_mask.macros.inc"
#include "rom-materials/chocobo_dismount_pose_schedule.macros.inc"
#include "rom-materials/chocobo_dismount_actor_motion.source.macros.inc"
#include "rom-materials/chocobo_dismount_flip_frames.macros.inc"
#include "rom-materials/world_explosion_circle_centers.macros.inc"
#include "rom-materials/world_explosion_light.macros.inc"
#include "rom-materials/world_scene_vehicle_arithmetic.macros.inc"
#include "rom-materials/world_scene_underwater_arithmetic.macros.inc"
#include "rom-materials/world_scene_chocobo_near_arithmetic.macros.inc"
#include "rom-materials/world_scene_judgment_arithmetic.macros.inc"
#include "rom-materials/world_scene_forest_arithmetic.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
// Sprite atlas addresses are ROM-selected. Normalize them through the source
// transfer/tile formats; the native renderer never reads a video-memory bus.
constexpr int airship_tile_origin = (FF6_ROM_VALUE_WORLD_AIRSHIP_TILE_ADDRESS_0 - 0x2000) / 32;
constexpr int minimap_tile_origin = (FF6_ROM_VALUE_WORLD_MINIMAP_TILE_ADDRESS_0 - 0x2000) / 32;
constexpr int misc_tile_origin = (FF6_ROM_VALUE_WORLD_MISC_TILE_ADDRESS_0 - 0x2000) / 32;
constexpr int character_tile_origin = (FF6_ROM_VALUE_WORLD_CHARACTER_TILE_ADDRESS_0 - 0x6000) / 16;
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
int signed_byte(unsigned n) {
    return n < 128 ? int(n) : int(n) - 256;
}
unsigned wrapped(int n, unsigned mask) {
    return static_cast<unsigned>(n) & mask;
}
// MoveVehicle derives this small panorama lean from turning and forward speed.
// Negative rotation uses the source's one's-complement magnitude.
int chocobo_lean(const WorldChocobo& c) {
    const int magnitude = c.turn < 0 ? -int(c.turn) - 1 : int(c.turn);
    const int size = ((magnitude / 8 * (c.speed / 16) / 256) * FF6_ROM_VALUE_WORLD_VEHICLE_CAMERA_SCALE_0) / 256;
    return c.turn < 0 ? size : -size;
}
unsigned visible_ground_rows(int lean) {
    return (FF6_ROM_VALUE_WORLD_VEHICLE_GROUND_ORIGIN_0 + 0U) + static_cast<unsigned>((lean < 0 ? -lean - 1 : lean) / 2);
}
RasterSprite tile_sprite(const Tile& tile, const std::array<Rgb555, 16>& colors, int x, int y,
                         unsigned depth = 3) {
    RasterSprite s;
    s.x = x;
    s.y = y;
    s.width = s.height = 8;
    s.pixels.assign(tile.begin(), tile.end());
    s.colors = colors;
    s.depth = static_cast<std::uint16_t>(depth);
    return s;
}
void draw_vehicle_frame(const WorldAssets& a, const WorldPresentation& presentation,
                        unsigned image, int px, int py, RasterScene& scene) {
    const auto* falcon = presentation.falcon ? &*presentation.falcon : nullptr;
    if (!image) return;
    const unsigned at = 216U + read16(a.frames, image * 2);
    const auto count = a.frames.at(at);
    require(at + 1 + count * 4 <= a.frames.size(), "Truncated airship frame");
    for (unsigned i = 0; i < count; ++i) {
        const unsigned p = at + 1 + i * 4, flags = a.frames[p + 3];
        const unsigned tile = a.frames[p + 2] + (flags & 1U) * 256;
        const auto& ship_tiles = presentation.falcon_graphics ? a.falcon_tiles : a.airship_tiles;
        // InitVehicle places the animated world tiles before the hull
        // graphics, and the minimap between hull and miscellaneous
        // images. Falcon's water masks use animated tile 4; they are
        // not hull tiles with a negative offset.
        const auto& pixels = tile < airship_tile_origin ? a.animation.at(tile)
                           : tile < minimap_tile_origin ? ship_tiles.at(tile - airship_tile_origin)
                           : tile < misc_tile_origin ? a.minimap.at(tile - minimap_tile_origin)
                           : a.misc.at(tile - misc_tile_origin);
        // UpdateSpriteAnim_10 copies the ship's palette to the smoke
        // palette slot. The source frame's palette identity is retained.
        const unsigned palette_id = (flags >> 1) & 7U;
        auto colors = (presentation.smoke || falcon) && palette_id == FF6_ROM_VALUE_WORLD_AUXILIARY_OBJECT_PALETTE_0 ? a.object_colors[FF6_ROM_VALUE_WORLD_SMOKE_SOURCE_PALETTE_0]
                           : falcon && palette_id == FF6_ROM_VALUE_WORLD_AIRSHIP_OVERLAY_PALETTE_0 ? a.object_colors[FF6_ROM_VALUE_WORLD_AIRSHIP_PRIMARY_PALETTE_0]
                           : presentation.falcon_graphics && palette_id == FF6_ROM_VALUE_WORLD_FALCON_PRIMARY_PALETTE_0 ? a.falcon_colors
                           : a.object_colors[palette_id];
        if (presentation.judgment && palette_id == FF6_ROM_VALUE_WORLD_AUXILIARY_OBJECT_PALETTE_0)
            std::copy_n(a.object_colors[FF6_ROM_VALUE_WORLD_JUDGMENT_SOURCE_COLOR_0 / 16].begin() + FF6_ROM_VALUE_WORLD_JUDGMENT_SOURCE_COLOR_0 % 16, FF6_ROM_VALUE_WORLD_JUDGMENT_COPY_COUNT_0, colors.begin() + FF6_ROM_VALUE_WORLD_JUDGMENT_COPY_DESTINATION_0);
        auto piece = tile_sprite(pixels, colors,
            px + signed_byte(a.frames[p]), py + signed_byte(a.frames[p + 1]), 4);
        piece.flip_x = (flags & 64U) != 0; piece.flip_y = (flags & 128U) != 0;
        scene.objects->sprites.push_back(std::move(piece));
    }
}
} // namespace
RasterScene compose_world_scene(const WorldAssets& a, const WorldState& w, std::uint8_t graphics,
                                std::uint8_t palette, std::uint8_t brightness,
                                std::uint16_t camera_scale, const WorldPresentation& presentation) {
    require(w.x < 4096 && w.y < 4096 && w.facing < 4 && w.walk_phase < 4 && palette < 8,
            "Invalid world presentation state");
    RasterScene scene;
    scene.intensity.assign(224, brightness);
    scene.backdrop = a.colors[0];
    AffineLayer layer;
    const auto* transition = w.airship_transition ? &*w.airship_transition : nullptr;
    const auto* crash = presentation.crash ? &*presentation.crash : nullptr;
    const auto* falcon = presentation.falcon ? &*presentation.falcon : nullptr;
    using AirshipPhase = WorldAirshipTransition::Phase;
    const bool transition_camera = transition && (transition->phase == AirshipPhase::lift_camera ||
                                                   transition->phase == AirshipPhase::land_camera);
    const bool transition_ground = transition && (transition->phase == AirshipPhase::lift_ground ||
                                                   transition->phase == AirshipPhase::land_ground);
    const bool vehicle_view = w.chocobo.has_value() || w.vehicle.has_value();
    const unsigned span = vehicle_view ? 64 : 32;
    const unsigned map_side = w.map == 2 ? 128 : 256;
    require(a.terrain.size() == map_side * map_side, "Invalid terrain extent");
    layer.columns = layer.rows = static_cast<std::uint16_t>(span * 2);
    layer.cells.resize(span * span * 4);
    layer.tiles = a.tiles;
    layer.colors = a.colors;
    const int origin_x = int(w.x / 16) - int(span / 2), origin_y = int(w.y / 16) - int(span / 2);
    for (unsigned y = 0; y < span; ++y)
        for (unsigned x = 0; x < span; ++x) {
            const auto id =
                a.terrain[wrapped(origin_y + int(y), map_side - 1) * map_side +
                          wrapped(origin_x + int(x), map_side - 1)];
            const auto& c = a.cells[id];
            layer.cells[y * span * 4 + x * 2] = c[0];
            layer.cells[y * span * 4 + x * 2 + 1] = c[1];
            layer.cells[y * span * 4 + span * 2 + x * 2] = c[2];
            layer.cells[y * span * 4 + span * 2 + x * 2 + 1] = c[3];
        }
    if (presentation.ruin || presentation.judgment) {
        layer.lines.resize(224);
        const unsigned angle = w.vehicle ? w.vehicle->heading / 256U : 0;
        const int sine = int(a.sine.at(angle % 180)) * (angle >= 180 ? -1 : 1);
        const int cosine = int(a.sine.at(angle % 180 + 90)) * (angle >= 90 && angle < 270 ? -1 : 1);
        const int cx = w.vehicle ? w.vehicle->center_x : FF6_ROM_VALUE_WORLD_PROJECTION_CENTER_0;
        const int cy = w.vehicle ? w.vehicle->center_y : FF6_ROM_VALUE_WORLD_PROJECTION_CENTER_1;
        for (unsigned y = 0; y < 224; ++y) {
            const int scale = (presentation.judgment ? FF6_ROM_VALUE_WORLD_CINEMATIC_PROJECTION_NUMERATORS_1 : FF6_ROM_VALUE_WORLD_CINEMATIC_PROJECTION_NUMERATORS_0) / ((FF6_ROM_VALUE_WORLD_CINEMATIC_PROJECTION_ORIGIN_0 + y * FF6_ROM_VALUE_WORLD_CINEMATIC_PROJECTION_STEP_0) >> 8);
            const int ax = scale * cosine / 256, ay = scale * sine / 256;
            auto& line = layer.lines[y];
            line.source_x = (int(w.x) - origin_x * 16) * 256 - cx * ax + (int(y) + 1 - cy) * ay;
            line.source_y = (int(w.y) - origin_y * 16) * 256 + cx * ay + (int(y) + 1 - cy) * ax;
            line.step_x = ax; line.step_y = -ay;
        }
        // The cinematic owns the full opaque world plane, with no vehicle,
        // sky, minimap or fog. Its circular flash adds a fixed light color.
        for (auto& tile : layer.tiles)
            for (auto& pixel : tile) if (!pixel) pixel = 255;
        layer.colors[255] = layer.colors[0];
        if (presentation.ruin && presentation.ruin->palette_scale != 255)
            for (auto& color : layer.colors) {
                unsigned result = 0;
                for (unsigned shift : {0U, 5U, 10U})
                    result |= (((color >> shift) & 31U) * presentation.ruin->palette_scale / 256) << shift;
                color = static_cast<Rgb555>(result);
            }
        if (presentation.judgment) {
            const auto& shot = *presentation.judgment;
            scene.operand_fallback = shot.light;
            auto beam = layer;
            beam.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_JUDGMENT_ARITHMETIC_0), static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_JUDGMENT_ARITHMETIC_0)};
            for (unsigned row = 0; row < 224; ++row) {
                auto& line = beam.lines[row];
                line.enabled = shot.edges[row][0] <= shot.edges[row][1];
                line.left = line.enabled ? shot.edges[row][0] : 0;
                line.right = line.enabled ? shot.edges[row][1] + 1U : 0;
            }
            scene.affine_layers.push_back(std::move(beam));
            scene.affine_layers.push_back(std::move(layer));
            scene.objects.emplace(); scene.objects->lines.resize(224);
            for (const auto& p : shot.picture)
                draw_vehicle_frame(a, presentation, p.frame,
                    p.x < 0x8000 ? int(p.x) : int(p.x) - 65536, p.y, scene);
            return scene;
        }
        const auto& ruin = *presentation.ruin;
        scene.operand_fallback = FF6_ROM_VALUE_WORLD_EXPLOSION_LIGHT_0;
        const auto widths = world_explosion_widths(ruin.radius);
        for (const auto [x, y] : {
                 std::pair{FF6_ROM_VALUE_WORLD_EXPLOSION_CIRCLE_CENTERS_0,
                           FF6_ROM_VALUE_WORLD_EXPLOSION_CIRCLE_CENTERS_1},
                 std::pair{FF6_ROM_VALUE_WORLD_EXPLOSION_CIRCLE_CENTERS_2,
                           FF6_ROM_VALUE_WORLD_EXPLOSION_CIRCLE_CENTERS_3}}) {
            auto flash = layer;
            flash.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_VEHICLE_ARITHMETIC_0), static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_VEHICLE_ARITHMETIC_0)};
            for (unsigned row = 0; row < 224; ++row) {
                const unsigned distance = static_cast<unsigned>(std::abs(int(row) - y));
                auto& line = flash.lines[row];
                line.enabled = distance < ruin.radius;
                const int half = widths[distance];
                line.left = static_cast<std::uint16_t>(std::max(0, x - half));
                line.right = static_cast<std::uint16_t>(std::min(256, x + half + 1));
            }
            scene.affine_layers.push_back(std::move(flash));
        }
        scene.affine_layers.push_back(std::move(layer));
        return scene;
    }
    // Native perspective rows recovered from WorldMain/UpdateMode7Vars.
    // The camera center follows the world pixel with ROM-selected axis offsets.
    // Both on-foot WorldNMI paths select these same operands.
    layer.lines.resize(224);
    const unsigned block = w.poison_flash / 16U + 1U;
    for (unsigned y = 0; y < 224; ++y) {
        const unsigned row = y - y % block;
        const int scale = camera_scale / ((FF6_ROM_VALUE_WORLD_ONFOOT_PROJECTION_ORIGIN_0 + row * (FF6_ROM_VALUE_WORLD_ONFOOT_PROJECTION_STEP_0 + 0U)) >> 8U);
        auto& line = layer.lines[y];
        line.left = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_0;
        line.right = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_1 + 1;
        line.step_x = scale;
        line.step_y = 0;
        line.sample_width = static_cast<std::uint8_t>(block);
        line.source_x = (int(w.x) - origin_x * 16 + FF6_ROM_VALUE_WORLD_ONFOOT_CAMERA_X_BIAS_0) * 256 - FF6_ROM_VALUE_WORLD_ONFOOT_PROJECTION_CENTER_0 * scale;
        // The authored camera anchor is quantized to quarter-texel precision
        // before the row advance. Combining both products first visibly shifts
        // a handful of terrain rows by one source pixel.
        line.source_y = (int(w.y) - origin_y * 16 - FF6_ROM_VALUE_WORLD_ONFOOT_CAMERA_Y_BIAS_0) * 256 - ((FF6_ROM_VALUE_WORLD_ONFOOT_PROJECTION_CENTER_1 * scale + 63) / 64) * 64 +
                        (int(row) + 1) * scale;
    }
    if (vehicle_view) {
        const unsigned angle = (w.vehicle ? w.vehicle->heading : w.chocobo->heading) / 256;
        const int lean = w.vehicle ? world_vehicle_camera_lean(*w.vehicle)
                                   : chocobo_lean(*w.chocobo);
        const unsigned rows = (crash || falcon) ? 224 : transition_camera ? std::min<unsigned>(224, transition->ground_rows) : w.vehicle ? static_cast<unsigned>(world_vehicle_ground_rows(*w.vehicle))
                                        : visible_ground_rows(lean);
        require(rows > 0 && rows <= 224, "Unsupported vehicle camera extent");
        const auto picture = w.vehicle ? w.vehicle->picture : std::nullopt;
        const unsigned horizon = 224 - (picture ? picture->ground_rows : rows);
        const auto slope = [](unsigned size, unsigned column) {
            return size ? ((column + 1) * (size + 1)) / 32 : 0U;
        };
        // Resolve the authored panorama's tile-column offsets into ordinary
        // indexed pixels. This is a scene transform, not an offset-register bus.
        if (w.map != 2 && !presentation.approach && !crash && !falcon) {
            // The panorama and its tilt use the preceding prepared picture,
            // while the ground transform follows the current vehicle movement.
            const auto previous = picture;
            const int sky_lean = previous ? previous->lean : lean;
            const unsigned sky_rows = previous ? previous->ground_rows : rows;
            const unsigned sky_angle = previous ? previous->sky_heading / 256U : angle;
            const unsigned sky_tilt = static_cast<unsigned>(sky_lean < 0 ? -sky_lean : sky_lean);
            AffineLayer sky;
            sky.columns = 32;
            sky.rows = 28;
            sky.tiles.resize(32 * 28);
            sky.cells.resize(32 * 28);
            sky.colors = a.colors;
            sky.lines.resize(224);
            sky.depth = 2;
            sky.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_VEHICLE_ARITHMETIC_0), ColorBlend::none};
            const int sky_x = 511 - int(((sky_angle + (sky_angle * FF6_ROM_VALUE_WORLD_PANORAMA_ANGLE_RATE_0 / 256)) * 2) & 511U);
            for (unsigned y = 0; y < 224; ++y) {
                sky.lines[y].enabled = y < horizon;
                sky.lines[y].left = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_0;
                sky.lines[y].right = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_1 + 1;
                sky.lines[y].source_y = int(y) * 256;
                const int skew = int(slope(sky_tilt, y / 8));
                for (unsigned x = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_0; x < FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_1 + 1; ++x) {
                    const int row_scroll = sky_x + (sky_lean < 0 ? skew : -skew);
                    // The slope strips travel with the panorama's fine scroll;
                    // fixing their boundaries to screen x creates false seams.
                    const int column_shift =
                        int(slope(sky_tilt, (x + wrapped(row_scroll, 7)) / 8 - 1));
                    const unsigned sx = wrapped(int(x) + row_scroll, 511);
                    const unsigned sy = wrapped(int(y) + 1 + int(sky_rows) - FF6_ROM_VALUE_WORLD_PANORAMA_VERTICAL_ORIGIN_0 +
                                                    (sky_lean < 0   ? -column_shift
                                                     : sky_lean > 0 ? column_shift - sky_lean
                                                                : 0),
                                                255);
                    const auto cell =
                        read16(a.sky_cells, ((sx / 256) * 1024 + (sy / 8) * 32 + (sx / 8) % 32) * 2);
                    const auto px = (cell & 16384U) ? 7 - sx % 8 : sx % 8;
                    const auto py = (cell & 32768U) ? 7 - sy % 8 : sy % 8;
                    const auto color = a.sky_tiles.at(cell & 1023U)[py * 8 + px];
                    const unsigned tile = (y / 8) * 32 + x / 8;
                    sky.cells[tile] = tile;
                    sky.tiles[tile][(y % 8) * 8 + x % 8] =
                        color ? static_cast<std::uint8_t>(((cell >> 10U) & 7U) * 16 + color) : 0;
                }
            }
            scene.affine_layers.push_back(std::move(sky));
        }
        const int sine = int(a.sine.at(angle % 180)) * (angle >= 180 ? -1 : 1);
        const int cosine = int(a.sine.at(angle % 180 + 90)) * (angle >= 90 && angle < 270 ? -1 : 1);
        const unsigned altitude = w.vehicle ? w.vehicle->altitude : 0;
        const unsigned denominator = falcon ? (FF6_ROM_VALUE_WORLD_FALCON_PROJECTION_CURVE_1 + 0U) : crash ? (FF6_ROM_VALUE_WORLD_CRASH_PROJECTION_ORIGIN_0 + 0U) : transition_camera ? transition->denominator : presentation.approach ? (FF6_ROM_VALUE_WORLD_VEHICLE_PROJECTION_DENOMINATOR_0 + 0U) : (FF6_ROM_VALUE_WORLD_VEHICLE_PROJECTION_DENOMINATOR_0 + 0U) + (altitude / 256) * FF6_ROM_VALUE_WORLD_ALTITUDE_PROJECTION_RATE_0;
        const unsigned denominator_step = (((crash || falcon) ? (FF6_ROM_VALUE_WORLD_PROJECTION_ENDS_0 + 0U) : transition_camera ? transition->end_scale : presentation.approach ? (FF6_ROM_VALUE_WORLD_PROJECTION_ENDS_0 + 0U) : (FF6_ROM_VALUE_WORLD_PROJECTION_ENDS_1 + 0U)) - denominator) / rows;
        const int center_x = w.vehicle ? w.vehicle->center_x : FF6_ROM_VALUE_WORLD_PROJECTION_CENTER_0;
        const int center_y = transition_camera ? transition->center_y : w.vehicle ? w.vehicle->center_y : FF6_ROM_VALUE_WORLD_PROJECTION_CENTER_1;
        const int camera_x = picture ? picture->x : w.x;
        const int camera_y = picture ? picture->y : w.y;
        for (unsigned y = 0; y < 224; ++y) {
            auto& line = layer.lines[y];
            line.enabled = y >= horizon;
            // The authored perspective has two spans (EE/0569, EE/3A80).
            // Its first 124 rows retain the prepared vertical origin; the
            // lower span uses the new origin. Both use the new scale curve.
            // Express that visible seam directly, without a transfer scheduler.
            const unsigned curve_start = y < FF6_ROM_VALUE_WORLD_PROJECTION_FIRST_SPAN_0 ? horizon : 224 - rows;
            const unsigned row = y >= curve_start ? y - curve_start : 0;
            const int scale = int((falcon ? (FF6_ROM_VALUE_WORLD_FALCON_PROJECTION_CURVE_0 + 0U) : crash ? crash->scale : transition_camera ? transition->numerator : (FF6_ROM_VALUE_WORLD_VEHICLE_PROJECTION_NUMERATOR_0 + 0U) + (presentation.approach ? 0U : altitude)) / ((denominator + row * denominator_step) >> 8U));
            const int ax = scale * cosine / 256, ay = scale * sine / 256;
            line.step_x = ax;
            line.step_y = -ay;
            // The anchor products AND the row displacement have quarter-texel
            // precision. Keeping the row's low six bits moves rotated texture
            // edges by a pixel when the horizontal product carries into them.
            const auto anchor = [](int value) { return value - int(wrapped(value, 63)); };
            line.source_x = (camera_x - origin_x * 16) * 256 + anchor(-center_x * ax) +
                            anchor(-center_y * ay) + anchor((int(y) + 1) * ay);
            line.source_y = (camera_y - origin_y * 16) * 256 + anchor(center_x * ay) +
                            anchor(-center_y * ax) + anchor((int(y) + 1) * ax);
        }
    }
    // Water graphics and colors belong to the published picture, alongside
    // its camera origin and sprites (VehicleNMI, EE/A509).
    const auto vehicle_picture = w.vehicle ? w.vehicle->picture : std::nullopt;
    const auto picture_clock = vehicle_picture ? vehicle_picture->clock : w.clock;
    // Each water row advances on its own fractional phase through a 16-pixel ring.
    constexpr std::array<unsigned, 16> phases{
#include "rom-materials/world_water_phases.inc"
    };
    for (unsigned row = 0; w.map != 2 && row < 16; ++row) {
        const unsigned first = row < 8 ? FF6_ROM_VALUE_WORLD_WATER_TILE_ORIGINS_0 : FF6_ROM_VALUE_WORLD_WATER_TILE_ORIGINS_1, py = row % 8;
        const unsigned shift = ((std::uint64_t{picture_clock} * FF6_ROM_VALUE_WORLD_WATER_SCROLL_RATE_0 + phases[row]) / 256) % 16;
        for (unsigned x = 0; x < 16; ++x) {
            const auto src = (x + shift) % 16;
            layer.tiles[first + x / 8][py * 8 + x % 8] = a.tiles[first + src / 8][py * 8 + src % 8];
        }
    }
    if (!w.map) {
        const unsigned phase = (picture_clock / FF6_ROM_VALUE_WORLD_WATER_PALETTE_CYCLE_2) % FF6_ROM_VALUE_WORLD_WATER_PALETTE_CYCLE_1;
        for (unsigned i = 0; i < FF6_ROM_VALUE_WORLD_WATER_PALETTE_CYCLE_1; ++i)
            layer.colors[FF6_ROM_VALUE_WORLD_WATER_PALETTE_CYCLE_0 + i] = a.colors[FF6_ROM_VALUE_WORLD_WATER_PALETTE_CYCLE_0 + (i + phase) % FF6_ROM_VALUE_WORLD_WATER_PALETTE_CYCLE_1];
    }
    layer.depth = vehicle_view ? 2 : 1;
    layer.operand = !vehicle_view;
    if (w.map == 2) {
        // Underwater terrain subtracts the authored fog colors; it does not
        // inherit the chocobo/airship lower-screen half-add minimap split.
        layer.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_UNDERWATER_ARITHMETIC_0), ColorBlend::none};
    } else if (w.chocobo) {
        auto near = layer;
        near.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_CHOCOBO_NEAR_ARITHMETIC_0), ColorBlend::none};
        layer.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_VEHICLE_ARITHMETIC_0), ColorBlend::none};
        for (unsigned y = 0; y < 224; ++y) {
            near.lines[y].enabled &= y >= FF6_ROM_VALUE_WORLD_CHOCOBO_BLEND_DEADLINE_0;
            layer.lines[y].enabled &= y < FF6_ROM_VALUE_WORLD_CHOCOBO_BLEND_DEADLINE_0;
        }
        scene.affine_layers.push_back(std::move(near));
    } else if (w.vehicle) {
        // AirshipIRQ only changes the projection. Unlike ChocoIRQ it never
        // halves the lower picture's color addition (EE/A6B7 vs EE/A6CF).
        layer.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_VEHICLE_ARITHMETIC_0), ColorBlend::none};
    }
    scene.affine_layers.push_back(std::move(layer));
    scene.objects.emplace();
    scene.objects->lines.resize(224);
    for (auto& line : scene.objects->lines) {
        line.left = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_0;
        line.right = FF6_ROM_VALUE_WORLD_VIEWPORT_EDGES_1 + 1;
    }
    if (presentation.actor_visible && !presentation.castle && !vehicle_view && !presentation.ship && !transition_ground) {
        const auto pose = presentation.pose.value_or(a.walk_frames[w.facing * 4 + w.walk_phase]);
        const auto tiles = a.characters.pose(graphics, pose);
        const unsigned frame = FF6_ROM_VALUE_WORLD_CHARACTER_FRAME_BASE_0 + (a.characters.upper_flip[pose] ? 2 : 0) +
                               (a.characters.lower_flip[pose] ? 1 : 0) + (w.forest ? FF6_ROM_VALUE_WORLD_CHARACTER_FOREST_FRAME_SHIFT_0 : 0);
        const unsigned at = 0xd8 + read16(a.frames, frame * 2), count = a.frames.at(at);
        require(at + 1 + count * 4 <= a.frames.size(), "Invalid world sprite frame");
        for (unsigned i = 0; i < count; ++i) {
            const auto p = at + 1 + i * 4;
            const auto tile = a.frames[p + 2], flags = a.frames[p + 3];
            require(tile >= character_tile_origin && tile < character_tile_origin + 6, "Invalid character piece");
            auto sprite =
                tile_sprite(tiles[tile - character_tile_origin], a.characters.palettes[palette],
                            FF6_ROM_VALUE_WORLD_CHARACTER_SCREEN_ORIGIN_0 + signed_byte(a.frames[p]), FF6_ROM_VALUE_WORLD_CHARACTER_SCREEN_ORIGIN_0 + signed_byte(a.frames[p + 1]));
            sprite.flip_x = (flags & 64U) != 0;
            sprite.flip_y = (flags & 128U) != 0;
            if (w.forest && i >= 4)
                sprite.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_WORLD_SCENE_FOREST_ARITHMETIC_0), ColorBlend::none};
            scene.objects->sprites.push_back(std::move(sprite));
        }
    }
    if (w.chocobo) {
        const auto& c = *w.chocobo;
        auto tiles = a.chocobo_tiles;
        const auto base = a.characters.graphics.at(graphics);
        // InitChoco selects two equal, tile-aligned rider transfers. Their
        // full intervals and addresses come from the ROM, including the copy
        // count; byte and word addresses normalize to decoded 4bpp tiles.
        const unsigned rider_source = FF6_ROM_VALUE_WORLD_RIDER_COPY_LAYOUT_1 / 32;
        const unsigned rider_second_source = (FF6_ROM_VALUE_WORLD_RIDER_COPY_LAYOUT_1 +
                                              FF6_ROM_VALUE_WORLD_RIDER_COPY_LAYOUT_2) / 32;
        for (unsigned i = 0; i < FF6_ROM_VALUE_WORLD_RIDER_COPY_LAYOUT_0; ++i) {
            tiles[FF6_ROM_VALUE_WORLD_RIDER_COPY_LAYOUT_3 + i] =
                a.characters.tiles.at(base + rider_source + i);
            tiles[FF6_ROM_VALUE_WORLD_RIDER_COPY_LAYOUT_4 + i] =
                a.characters.tiles.at(base + rider_second_source + i);
        }
        auto colors = a.object_colors;
        colors[FF6_ROM_VALUE_WORLD_AUXILIARY_OBJECT_PALETTE_0] = a.characters.palettes.at(palette);
        const auto draw = [&](unsigned frame, int x, int y) {
            const unsigned at = 216U + read16(a.frames, frame * 2);
            const unsigned count = a.frames.at(at);
            require(at + 1 + count * 4 <= a.frames.size(), "Truncated chocobo frame");
            for (unsigned i = 0; i < count; ++i) {
                const auto p = at + 1 + i * 4;
                const auto flags = a.frames[p + 3];
                const unsigned tile = a.frames[p + 2] + (flags & 1U) * 256;
                auto piece =
                    tile_sprite(tiles.at(tile), colors[(flags >> 1U) & 7U],
                                x + signed_byte(a.frames[p]), y + signed_byte(a.frames[p + 1]), 4);
                piece.flip_x = (flags & 64U) != 0;
                piece.flip_y = (flags & 128U) != 0;
                scene.objects->sprites.push_back(std::move(piece));
            }
        };
        if (!c.dismount)
            draw(c.frame, FF6_ROM_VALUE_CHOCOBO_RIDING_ORIGIN_0,
                 FF6_ROM_VALUE_CHOCOBO_RIDING_ORIGIN_1);
        else {
            const unsigned age = *c.dismount;
            // Original pose/motion choices are supplied data. Fractional width
            // and explicit per-visit increments remain native arithmetic.
            const unsigned phase = age < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_PREPARE_WAIT_0 ? 0
                : age < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_0
                    ? FF6_ROM_VALUE_CHOCOBO_DISMOUNT_PREPARE_FRAME_0
                    : (age - (FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_0 - 1)) %
                      (FF6_ROM_VALUE_CHOCOBO_DISMOUNT_CYCLE_MASK_0 + 1);
            draw(a.dismount_frames.at(phase), FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ORIGIN_X_0 +
                 int(age < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_0 ? 0 :
                     (age - (FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_0 - 1)) *
                     FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_1 / 256),
                 FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_2 +
                 int(age < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_0 ? 0 :
                     (age - (FF6_ROM_VALUE_CHOCOBO_DISMOUNT_LINEAR_0 - 1)) * 2));
            const auto pose = static_cast<std::uint8_t>(
                age < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_POSE_SCHEDULE_0
                    ? FF6_ROM_VALUE_CHOCOBO_DISMOUNT_POSE_SCHEDULE_2
                    : age < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_POSE_SCHEDULE_1
                        ? FF6_ROM_VALUE_CHOCOBO_DISMOUNT_POSE_SCHEDULE_3
                        : FF6_ROM_VALUE_CHOCOBO_DISMOUNT_POSE_SCHEDULE_4);
            const auto actor = a.characters.pose(graphics, pose);
            for (unsigned i = 0; i < 6; ++i)
                tiles[character_tile_origin + i] = actor[i];
            colors[FF6_ROM_VALUE_WORLD_DISMOUNT_CHARACTER_PALETTE_0] = a.characters.palettes.at(palette);
            int actor_y = FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ACTOR_MOTION_SOURCE_0;
            int velocity = FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ACTOR_MOTION_SOURCE_1 < 32768
                ? FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ACTOR_MOTION_SOURCE_1
                : FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ACTOR_MOTION_SOURCE_1 - 65536;
            int actor_x = FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ACTOR_MOTION_SOURCE_2;
            for (unsigned t = 0; t <= age &&
                    actor_y < FF6_ROM_VALUE_CHOCOBO_DISMOUNT_ACTOR_MOTION_SOURCE_3; ++t) {
                actor_y += velocity++;
                --actor_x;
            }
            draw(FF6_ROM_VALUE_CHOCOBO_DISMOUNT_FLIP_FRAMES_0 +
                     (a.characters.upper_flip[pose] ? 2 : 0) +
                     (a.characters.lower_flip[pose] ? 1 : 0),
                 actor_x, actor_y);
        }
    }
    if (presentation.castle) {
        const auto& castle = *presentation.castle;
        const unsigned age = castle.frame;
        const auto draw = [&](unsigned frame, int x, int y) {
            const unsigned at = 0xd8U + read16(a.frames, frame * 2);
            const auto count = a.frames.at(at);
            require(at + 1 + count * 4 <= a.frames.size(), "Invalid animation frame");
            for (unsigned i = 0; i < count; ++i) {
                const auto p = at + 1 + i * 4;
                const unsigned flags = a.frames[p + 3], tile = a.frames[p + 2] + (flags & 1U) * 256;
                require(tile >= misc_tile_origin && tile - misc_tile_origin < a.misc.size(), "Invalid world animation tile");
                auto piece =
                    tile_sprite(a.misc[tile - misc_tile_origin], a.object_colors[(flags >> 1U) & 7U],
                                x + signed_byte(a.frames[p]), y + signed_byte(a.frames[p + 1]));
                piece.flip_x = (flags & 64U) != 0;
                piece.flip_y = (flags & 128U) != 0;
                scene.objects->sprites.push_back(std::move(piece));
            }
        };
        const auto oscillation = a.sine.at((age + 1) & 255U) & (FF6_ROM_VALUE_WORLD_CASTLE_PARTICLE_STRIP_1 + 0U);
        // WorldCmd_fd/fe define emitter membership through their record span.
        // The subsequent UpdateSpriteAnim_03 replaces the first record with the
        // parked airship (EE/47A9 hides it here). Remaining emitters precede the
        // castle in original sprite selection order.
        for (unsigned i = 1; i < (castle.finished ? 0U : FF6_ROM_VALUE_WORLD_CASTLE_PARTICLE_STRIP_0); ++i) {
            const auto p = i * 6;
            unsigned phase = a.castle_particles[p + 2], fraction = a.castle_particles[p + 3],
                     jitter = a.castle_particles[p + 5];
            for (unsigned t = 0; t < age; ++t) {
                fraction += a.castle_particles[p + 4];
                if (fraction >= 256) {
                    fraction -= 256;
                    phase = (phase + 1) & (FF6_ROM_VALUE_WORLD_CASTLE_PARTICLE_STRIP_1 + 0U);
                    jitter = a.sine.at((t + 1) & 255U) & (FF6_ROM_VALUE_WORLD_CASTLE_PARTICLE_STRIP_2 + 0U);
                }
            }
            draw(FF6_ROM_VALUE_WORLD_CASTLE_PARTICLE_STRIP_3 + phase, int(a.castle_particles[p]) + int(jitter), a.castle_particles[p + 1]);
        }
        const unsigned height = castle.emerging ? FF6_ROM_VALUE_WORLD_CASTLE_SPRITE_Y_1 - (age + (256U / FF6_ROM_VALUE_WORLD_CASTLE_MOTION_STEP_0 - 1U)) / (256U / FF6_ROM_VALUE_WORLD_CASTLE_MOTION_STEP_0) : FF6_ROM_VALUE_WORLD_CASTLE_SPRITE_Y_0 + age / (256U / FF6_ROM_VALUE_WORLD_CASTLE_MOTION_STEP_0);
        draw(FF6_ROM_VALUE_WORLD_CASTLE_IMAGE_BASE_0 + (height - FF6_ROM_VALUE_WORLD_CASTLE_SPRITE_Y_0) / 8, FF6_ROM_VALUE_WORLD_CASTLE_SPRITE_X_0 + int(oscillation == FF6_ROM_VALUE_WORLD_CASTLE_PARTICLE_STRIP_1 ? 1 : oscillation),
             static_cast<int>(height));
    }
    if (presentation.ship || presentation.pigeon || (w.vehicle && (w.vehicle->esper ||
            (vehicle_picture ? vehicle_picture->arrows : w.vehicle->arrows)))) {
        const auto draw = [&](unsigned frame, int x, int y) {
            // Frame zero means hidden; its directory entry aliases the first
            // airship frame and must not be decoded (DrawVehicleSprites).
            if (!frame)
                return;
            const unsigned at = 216U + read16(a.frames, frame * 2);
            const auto count = a.frames.at(at);
            require(at + 1 + count * 4 <= a.frames.size(), "Truncated travel sprite frame");
            for (unsigned i = 0; i < count; ++i) {
                const auto p = at + 1 + i * 4;
                const unsigned flags = a.frames[p + 3], tile = a.frames[p + 2] + (flags & 1U) * 256;
                require(tile >= misc_tile_origin, "Invalid travel sprite tile");
                const unsigned palette_id = (flags >> 1U) & 7U;
                // UpdateSpriteAnim_05 (EE/4B57) gives the ship field palette 4
                // at both object slots 0 and 4. Its frame attributes alone
                // would select unrelated world colors. Keep that ownership
                // local to this presentation, so ordinary travel is unchanged.
                const auto& colors = presentation.ship && (palette_id == (FF6_ROM_VALUE_WORLD_SHIP_PALETTE_CHOICES_0 & 255U) ||
                                                                  palette_id == (FF6_ROM_VALUE_WORLD_SHIP_PALETTE_CHOICES_0 >> 8U))
                                         ? a.characters.palettes[FF6_ROM_VALUE_WORLD_TRAVEL_CHARACTER_PALETTE_0]
                                     : presentation.pigeon && palette_id == FF6_ROM_VALUE_WORLD_AUXILIARY_OBJECT_PALETTE_0
                                         ? a.characters.palettes[FF6_ROM_VALUE_WORLD_TRAVEL_CHARACTER_PALETTE_0]
                                     : w.vehicle && w.vehicle->esper && palette_id == FF6_ROM_VALUE_WORLD_AUXILIARY_OBJECT_PALETTE_0
                                         ? a.esper_colors : a.object_colors[palette_id];
                auto piece = tile_sprite(a.misc.at(tile - misc_tile_origin), colors,
                    x + signed_byte(a.frames[p]), y + signed_byte(a.frames[p + 1]), 4);
                piece.flip_x = (flags & 64U) != 0;
                piece.flip_y = (flags & 128U) != 0;
                scene.objects->sprites.push_back(std::move(piece));
            }
        };
        if (presentation.pigeon && w.vehicle && presentation.actor_visible)
            draw(a.pigeon_frames.at(presentation.pigeon_phase), FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0,
                 FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0 - int((w.vehicle->altitude / 256U) * (FF6_ROM_VALUE_WORLD_TRAVEL_ALTITUDE_GAIN_0 + 0U) / 256U));
        if (presentation.ship && presentation.actor_visible)
            draw(FF6_ROM_VALUE_WORLD_SHIP_FRAME_BASE_0 + w.facing * 2 + ((w.walk_clock / FF6_ROM_VALUE_WORLD_SHIP_FRAME_PERIOD_0) & 1U), FF6_ROM_VALUE_WORLD_SHIP_SPRITE_ORIGIN_0, FF6_ROM_VALUE_WORLD_SHIP_SPRITE_ORIGIN_1);
        if (w.vehicle && w.vehicle->esper && presentation.actor_visible)
            draw(vehicle_picture ? vehicle_picture->sprite_frame : w.vehicle->sprite_frame, FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0,
                 FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0 - int(((vehicle_picture ? vehicle_picture->altitude : w.vehicle->altitude) / 256U) *
                           (FF6_ROM_VALUE_WORLD_TRAVEL_ALTITUDE_GAIN_0 + 0U) / 256U));
        if (w.vehicle && (vehicle_picture ? vehicle_picture->arrows : w.vehicle->arrows)) {
            const auto& v = *w.vehicle;
            const int lean = vehicle_picture ? vehicle_picture->lean : world_vehicle_camera_lean(v);
            const int ground = (vehicle_picture ? vehicle_picture->ground_rows : world_vehicle_ground_rows(v)) -
                               (lean < 0 ? -lean - 1 : lean) / 2;
            const int y = (255 - ground) & 255;
            const unsigned phase = (vehicle_picture ? vehicle_picture->arrow_clock : v.arrow_clock) / FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_PHASE_PERIOD_0;
            const bool left = vehicle_picture ? vehicle_picture->left : v.left;
            draw(left ? FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_FRAMES_0 : FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_FRAMES_1, left ? FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_X_0 : FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_X_1, y);
            if (phase) draw((left ? FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_FRAME_BASES_0 : FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_FRAME_BASES_1) + phase, left ? FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_X_1 : FF6_ROM_VALUE_WORLD_TRAVEL_ARROW_X_0, y);
        }
    }
    if ((w.vehicle && w.map != 2 && !w.vehicle->esper && presentation.actor_visible && !crash && !presentation.pigeon) ||
        (!vehicle_view && (presentation.parked_airship || transition_ground))) {
        unsigned frame = 0;
        int x = FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0, y = FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0;
        const bool flying = w.vehicle.has_value();
        if (falcon) {
            frame = falcon->ship_frame;
            x = falcon->picture_x; y = falcon->picture_y;
        } else if (transition_camera || transition_ground) {
            frame = transition->sprite_frame;
            x = transition_ground ? FF6_ROM_VALUE_WORLD_AIRSHIP_GROUND_SCREEN_X_0 : FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0;
            y = transition->phase == AirshipPhase::land_ground
                ? (transition->sprite_y >= 0 ? transition->sprite_y / 256 : -((-transition->sprite_y + 255) / 256))
                : transition->sprite_y;
        } else if (flying) {
            frame = vehicle_picture ? vehicle_picture->sprite_frame : w.vehicle->sprite_frame;
            y -= static_cast<int>((w.vehicle->altitude / 256U) * (FF6_ROM_VALUE_WORLD_TRAVEL_ALTITUDE_GAIN_0 + 0U) / 256U);
            if (presentation.smoke) {
                const auto& smoke = *presentation.smoke;
                frame = smoke.rotor + 1U;
                x = FF6_ROM_VALUE_WORLD_FLIGHT_SPRITE_ORIGIN_0 + smoke.jitter_x;
                y = smoke.height / 256 + smoke.jitter_y;
            }
        } else {
            const auto at = *presentation.parked_airship;
            const int dy = int(at.y) * 16 - int(w.y);
            if (dy >= -FF6_ROM_VALUE_WORLD_PARKED_VERTICAL_BOUNDS_0 && dy < FF6_ROM_VALUE_WORLD_PARKED_VERTICAL_BOUNDS_1) {
                const unsigned depth = a.parked_airship_projection[static_cast<unsigned>(dy + FF6_ROM_VALUE_WORLD_PARKED_VERTICAL_BOUNDS_0)];
                const int dx = int(at.x) * 16 - int(w.x) - FF6_ROM_VALUE_WORLD_PARKED_HORIZONTAL_BIAS_0;
                const unsigned distance = static_cast<unsigned>(std::abs(dx));
                const unsigned projected = distance + (((distance * (FF6_ROM_VALUE_WORLD_PARKED_PROJECTION_FACTOR_0 + 0U) * depth) / 256U) / (FF6_ROM_VALUE_WORLD_PARKED_PROJECTION_DIVISOR_0 + 0U) & 255U);
                if (projected < FF6_ROM_VALUE_WORLD_PARKED_HORIZONTAL_EXTENT_0) {
                    x = FF6_ROM_VALUE_WORLD_PARKED_SPRITE_ORIGIN_0 + (dx < 0 ? -static_cast<int>(projected) : static_cast<int>(projected));
                    y = static_cast<int>(depth) - FF6_ROM_VALUE_WORLD_PARKED_SPRITE_Y_BIAS_0;
                    frame = (w.map ? FF6_ROM_VALUE_WORLD_PARKED_FRAME_CHOICES_0 + FF6_ROM_VALUE_WORLD_PARKED_FRAME_CHOICES_1 + 0U : FF6_ROM_VALUE_WORLD_PARKED_FRAME_CHOICES_0 + 0U) + depth / FF6_ROM_VALUE_WORLD_PARKED_FRAME_DEPTH_0;
                }
            }
        }
        const auto draw = [&](unsigned image, int px, int py) {
            draw_vehicle_frame(a, presentation, image, px, py, scene);
        };
        if (falcon) {
            for (const auto& splash : falcon->picture)
                draw(splash.frame, splash.x, splash.y);
            draw(falcon->wake_frame, FF6_ROM_VALUE_FALCON_INITIAL_POSITION_0 / 256, FF6_ROM_VALUE_FALCON_WAKE_HEIGHT_0);
        }
        if (presentation.smoke)
            for (const auto& particle : presentation.smoke->picture)
                draw(particle.frame, particle.x / 256, particle.y);
        draw(frame, x, y);
    }
    if (w.minimap && w.map != 2 && !transition) {
        require(a.minimap.size() >= 64 && a.misc.size() > FF6_ROM_VALUE_WORLD_MINIMAP_POSITION_LOCAL_TILE_0, "Missing minimap graphics");
        const auto& marker_tile = w.chocobo ? a.chocobo_tiles.at(FF6_ROM_VALUE_WORLD_MINIMAP_MARKER_TILES_0) : a.misc[FF6_ROM_VALUE_WORLD_MINIMAP_POSITION_LOCAL_TILE_0];
        const int marker_x = vehicle_picture ? vehicle_picture->x : w.x;
        const int marker_y = vehicle_picture ? vehicle_picture->y : w.y;
        auto marker =
            tile_sprite(marker_tile, a.object_colors[FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_0], FF6_ROM_VALUE_WORLD_MINIMAP_MARKER_ORIGIN_0 + marker_x / 64, FF6_ROM_VALUE_WORLD_MINIMAP_MARKER_ORIGIN_1 + marker_y / 64, 4);
        scene.objects->sprites.insert(scene.objects->sprites.begin(), std::move(marker));
        if (vehicle_view) {
            const unsigned angle = (vehicle_picture ? vehicle_picture->sky_heading
                                   : w.chocobo ? w.chocobo->heading : w.vehicle->heading) / 256;
            const int dx = int(a.sine.at(angle % 180) / 64) * (angle < 180 ? -1 : 1);
            const int dy =
                int(a.sine.at(angle % 180 + 90) / 64) * (angle < 90 || angle >= 270 ? -1 : 1);
            const auto& direction_tile = w.chocobo ? a.chocobo_tiles.at(FF6_ROM_VALUE_WORLD_MINIMAP_MARKER_TILES_1) : a.misc.at(FF6_ROM_VALUE_WORLD_MINIMAP_DIRECTION_LOCAL_TILE_0);
            auto direction = tile_sprite(direction_tile, a.object_colors[FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_0],
                                         FF6_ROM_VALUE_WORLD_MINIMAP_MARKER_ORIGIN_0 + 1 + marker_x / 64 + dx, FF6_ROM_VALUE_WORLD_MINIMAP_MARKER_ORIGIN_1 + 1 + marker_y / 64 + dy, 4);
            scene.objects->sprites.insert(scene.objects->sprites.begin() + 1, std::move(direction));
        }
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x) {
                auto piece = tile_sprite(
                    a.minimap[(y / 4) * 32 + ((y / 2) % 2) * 8 + (y % 2) * 16 + x],
                    a.object_colors[FF6_ROM_VALUE_WORLD_MINIMAP_PALETTE_0], FF6_ROM_VALUE_WORLD_MINIMAP_ORIGIN_0 + int(x) * 8, FF6_ROM_VALUE_WORLD_MINIMAP_ORIGIN_1 + int(y) * 8, vehicle_view ? 1 : 4);
                piece.mix = {ColorBlend::average, ColorBlend::none};
                scene.objects->sprites.push_back(std::move(piece));
            }
    }
    if (vehicle_view && !presentation.approach && !crash && !falcon) {
        // InitVehicleSprites/UpdateGradientSprites: five rows of authored
        // 16-pixel fog pieces. The lower rows are behind terrain on the visible
        // plane, but supply its additive operand. This also preserves the
        // original half-add minimap overlay below picture row 156.
        scene.objects->operand = true;
        const int lean = vehicle_picture ? vehicle_picture->lean
                         : w.vehicle ? world_vehicle_camera_lean(*w.vehicle)
                                   : chocobo_lean(*w.chocobo);
        const int size = lean < 0 ? -lean : lean;
        const int rows = vehicle_picture ? vehicle_picture->ground_rows
                         : w.vehicle ? world_vehicle_ground_rows(*w.vehicle)
                                   : static_cast<int>(visible_ground_rows(lean));
        const auto slope = [size](unsigned column) {
            return size ? int((column + 1) * unsigned(size + 1) / 32) : 0;
        };
        int y = FF6_ROM_VALUE_WORLD_FOG_VERTICAL_ORIGIN_0 - rows + std::max(0, lean), previous = slope(0);
        for (unsigned column = 0; column < FF6_ROM_VALUE_WORLD_FOG_COLUMN_COUNT_0; ++column) {
            unsigned variant = 0;
            const int next = slope(column * 2 + 1);
            if (next != previous) {
                previous = next;
                variant = lean < 0 ? FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_5 + 1 : FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_5;
                if (lean < 0)
                    ++y;
            }
            for (unsigned row = 0; row < FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_0; ++row) {
                const unsigned tile = (row < FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_2 ? row * FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_3 : FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_4) + variant;
                for (unsigned py = 0; py < 2; ++py)
                    for (unsigned px = 0; px < 2; ++px) {
                        auto piece = tile_sprite(a.animation.at(tile + py * 16 + px),
                                                 a.object_colors[row < FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_1 ? FF6_ROM_VALUE_WORLD_FOG_SPRITE_STYLES_0 : FF6_ROM_VALUE_WORLD_FOG_SPRITE_STYLES_1],
                                                 FF6_ROM_VALUE_WORLD_FOG_HORIZONTAL_ORIGIN_0 + int(column * 16 + px * 8),
                                                 y + int(row * 16 + py * 8), row < FF6_ROM_VALUE_WORLD_FOG_ROW_LAYOUT_1 ? FF6_ROM_VALUE_WORLD_FOG_SPRITE_STYLES_2 * 2 + 1 : FF6_ROM_VALUE_WORLD_FOG_SPRITE_STYLES_3 * 2 + 1);
                        scene.objects->sprites.push_back(std::move(piece));
                    }
            }
            if (variant && lean > 0)
                --y;
            const int after = slope(column * 2 + 2);
            if (after != previous) {
                previous = after;
                y += lean < 0 ? 1 : -1;
            }
        }
    }
    if (presentation.approach) compose_world_approach(a, *presentation.approach, scene);
    return scene;
}
} // namespace ff6
