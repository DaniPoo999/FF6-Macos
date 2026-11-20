#pragma once
#include "ff6/battle_actor_background.hpp"
#include "ff6/battle_actor_scene.hpp"
#include "ff6/battle_effect_raster.hpp"
#include "ff6/battle_raster_timeline.hpp"
#include "ff6/music_player.hpp"
#include "rom-materials/battle_tornado_initial_phase.macros.inc"
#include "rom-materials/battle_character_orbit_radius.macros.inc"
#include "rom-materials/battle_character_orbit_padding.macros.inc"
#include "rom-materials/battle_effect_default_palette.macros.inc"
#include "rom-materials/battle_effect_default_priority.macros.inc"
#include "rom-materials/battle_effect_initial_scroll.macros.inc"
#include "rom-materials/effect_steering_initial.macros.inc"
#include "rom-materials/effect_initial_draw_count.macros.inc"
#include "rom-materials/effect_alternate_draw_count.macros.inc"
#include <memory>

namespace ff6 {
enum class BattleEffectLayer : std::uint8_t { sprite, foreground, overlay, extra };
struct BattleEffectSelection {
    std::uint16_t script = 0;
    // Primary layers: true starts the script, false loads graphics only.
    // Fourth field: true creates attacker extra tasks; false creates an esper.
    bool runs = true;
};
struct BattleEffectDefinition {
    std::array<std::optional<BattleEffectSelection>, 4> layers;
    std::array<std::uint8_t, 3> palettes{};
    std::uint8_t sound = 0, initialization = 0, target_delay = 0;
};
struct BattleEffectGraphics {
    std::uint16_t tile_group = 0, first_frame = 0;
    std::uint8_t frames = 0, width = 0, height = 0;
    bool two_bit = false;
};
struct BattleEffectPiece {
    std::uint8_t x = 0, y = 0, tile = 0;
    bool flip_x = false, flip_y = false;
    bool operator==(const BattleEffectPiece&) const = default;
};
struct BattleEffectSharedFrame {
    std::uint16_t script = 0;
    std::uint8_t frame = 0;
};
struct BattleEffectGradientData {
    std::array<std::uint8_t, 32> ramp{};
    std::array<std::uint8_t, 16> channels{}; // RGB component selection, bits 0..2.
};
BattleEffectGradientData decode_battle_effect_gradients(ByteView data);
struct BattleEffectGradient {
    std::uint8_t phase = 0, intensity = 0;
    bool published = false;
    std::array<Rgb555, 151> rows{};
    BattleEffectRaster::Colors colors{};
    // Actual shared-writer order; distinct from display row order.
    std::vector<BattleGradientWrite> writes;
    void update(const BattleEffectGradientData& data, std::uint8_t command, std::uint8_t parameter);
    void clear();
};
std::array<BattleEffectSharedFrame, 7> decode_battle_effect_shared_frames(ByteView data);
struct BattlePhysicalAttackPresentation {
    std::uint8_t property = 0;
    bool special = false, critical = false;
};
struct BattleBlockPresentationData {
    struct Selection { std::uint8_t script = 0, palette = 0, sound = 0; };
    std::array<Selection, 7> selections{};
    std::array<std::array<Rgb555, 8>, 4> palettes{};
    std::vector<Tile> tiles, status_tiles;
};
struct BattleMiscMonsterAnimation {
    std::uint16_t effect = 0;
    bool all_characters = false;
};
// The small data programs choose an existing animation and its target scope.
std::array<BattleMiscMonsterAnimation, 14> decode_battle_misc_monster_animations(ByteView);
struct BattleEffectLibrary {
    std::array<BattleMiscMonsterAnimation, 14> monster_misc{};
    std::optional<std::array<std::uint8_t, 64>> monster_flash_delays;
    std::optional<std::array<std::uint16_t, 56>> monster_dissolve_masks;
    std::array<std::uint8_t, 17> screen_shake{};
    std::array<std::uint8_t, 5> event_esper_images{};
    std::optional<BattleBlockPresentationData> blocks;
    Bytes programs, frame_data;
    std::optional<std::array<std::int16_t, 8>> fight_step_y;
    std::vector<std::array<Rgb555, 16>> event_palettes;
    std::optional<std::array<std::array<std::array<Rgb555, 16>, 3>, 2>> transition_lighting;
    std::array<std::uint8_t, 256> sine{}, random{};
    std::array<std::uint16_t, 256> sine16{};
    std::array<std::uint8_t, 1024> angles{};
    std::array<std::uint8_t, 16> character_walking{}, character_jumping{};
    std::array<std::uint16_t, 1024> distances{};
    std::vector<std::uint16_t> entries, frame_offsets, tiles_two, tiles_three;
    std::vector<Tile> pixels_two, pixels_three;
    std::vector<std::array<Rgb555, 8>> palettes;
    // Index FF reads a reviewed, separate ROM padding span after the palette
    // table. Intervening indices are not implicitly valid palette resources.
    std::optional<std::array<Rgb555, 8>> tail_palette;
    std::optional<std::array<Rgb555, 16>> tigerbreak_palette;
    std::optional<std::array<std::uint8_t, 54>> curved_scroll_repeats;
    std::optional<std::array<std::uint8_t, 8>> rainbow_palettes;
    std::optional<std::array<std::uint8_t, 8>> random_sprite_palettes;
    std::vector<BattleEffectDefinition> definitions;
    std::vector<std::array<std::uint8_t, 8>> weapon_properties;
    std::vector<BattleEffectGraphics> graphics;
    std::optional<std::array<BattleEffectSharedFrame, 7>> shared_frames;
    std::array<MonsterGraphic, 30> summons;
    std::vector<Tile> affine_tiles;
    Bytes affine_layouts;
    std::vector<MonsterGraphic> sketch_monsters;
    std::array<std::optional<std::array<Rgb555, 16>>, 30> summon_following_colors;
    std::optional<BattleEffectGradientData> gradients;
    std::optional<std::array<std::uint8_t, 7>> target_copies;
};
struct BattleEffectVector {
    std::uint8_t angle = 0, horizontal_distance = 0;
    std::uint16_t length = 0;
};
BattleEffectVector measure_battle_effect_vector(const BattleEffectLibrary& library, std::uint8_t x0,
                                                std::uint8_t y0, std::uint8_t x1, std::uint8_t y1);
int scale_battle_effect_sine(const BattleEffectLibrary& library, std::uint8_t phase,
                             std::uint16_t radius);
int scale_battle_effect_sine8(const BattleEffectLibrary& library, std::uint8_t phase,
                              std::uint8_t radius);
BattleEffectLibrary load_battle_effects(const ContentPack& pack);
const std::array<Rgb555, 8>& battle_effect_palette(const BattleEffectLibrary& library,
                                                  std::uint8_t identity);
// Public decoders also accept synthetic inputs. Shared by sprite/background
// effects; a coordinate FF is legal unless followed by the FF terminator.
BattleEffectDefinition decode_battle_effect_definition(ByteView record);
BattleEffectGraphics decode_battle_effect_graphics(ByteView record);
std::vector<BattleEffectPiece> decode_battle_effect_frame(ByteView record);
std::vector<Tile> load_battle_effect_tiles(const BattleEffectLibrary& library,
                                           BattleEffectGraphics graphics, BattleEffectLayer layer);
// Source-loop work before the upload wait, in processor cycles;
// useful for validating the preparation bound without conflating display time.
std::uint64_t battle_effect_image_preparation_work(const BattleEffectLibrary& library,
    BattleEffectGraphics graphics, BattleEffectLayer layer);
// Whole presentation intervals necessarily consumed by image conversion,
// before its explicit publication waits. This conservative source-loop bound
// excludes display service and layout construction; it is not a complete load
// duration. Already-decoded native images still need this visible preparation.
unsigned battle_effect_image_preparation_waits(const BattleEffectLibrary& library,
    BattleEffectGraphics graphics, BattleEffectLayer layer);
// Shared loader bound also includes task clearing and frame construction.
// Repeated hits skip conversion only; explicit waits split preparation stages.
unsigned battle_effect_preparation_waits(const BattleEffectLibrary& library,
    const BattleEffectDefinition& definition, bool repeated, bool message_cleanup);
struct BattleEffectImage {
    BattleEffectGraphics geometry;
    std::vector<Tile> tiles;
    std::vector<std::vector<BattleEffectPiece>> frames;
    // Shared frames can use different geometry from the primary animation.
    std::vector<BattleEffectGraphics> frame_geometry;
    std::array<Rgb555, 16> colors{}, base_colors{};
    // Palette selection belongs to loaded 16x16 frame cells, including flips.
    // Empty entries use the primary foreground bank.
    std::vector<std::bitset<256>> secondary_palette_cells;
    bool character_frames = false;
    bool adjacent_status_frames = false;
    // Empty means a contiguous owned image. Block/weapon/status composition
    // has a gap which must reject if a malformed frame attempts to sample it.
    std::vector<bool> owned_tiles;
};
// Loaded image resources and their authored origins survive command/pre-cast
// boundaries. Visible frames and script tasks have a shorter lifetime. A caller
// without a previous owner must leave origins absent, not guess screen center.
struct BattleEffectImageState {
    std::array<std::optional<BattleEffectImage>, 4> images;
    std::array<std::optional<BattleAnimationPoint>, 2> origins;
    std::array<bool, 2> flipped{};
    std::uint8_t gradient_phase = 0, gradient_intensity = 0;
    std::optional<std::array<BattleAnimationPoint, 151>> overlay_projection;
};
// Fresh battle entry only: InitHWRegs clears authored origins and counters;
// the separate line initializer clears all battlefield projection rows.
BattleEffectImageState initial_battle_effect_images();
BattleEffectImage load_battle_effect_image(const BattleEffectLibrary& library, std::uint16_t script,
                                           BattleEffectLayer layer, std::uint8_t palette);
// Palette offsets have their own retained state. Relative commands copy the
// first selected component's new offset to all selected components.
struct BattleEffectColorOffset {
    std::array<std::uint8_t, 3> channels{};
    bool subtract = false;
    void update(std::uint8_t parameter, bool relative, bool subtraction);
    Rgb555 apply(Rgb555 color) const;
};
// Presentation survives body cleanup and loading the next data program.
struct BattleEffectDisplay {
    std::uint8_t screen_shake = 0; // Event-controlled terrain shake; 0x80 is horizontal only.
    std::uint8_t brightness = 15, mosaic_size = 1;
    std::bitset<3> mosaic_layers; // Foreground effect, terrain, overlay.
};
struct BattleEffectContext {
    std::uint8_t attacker = 0;
    std::optional<std::uint16_t> sketch_monster; // Absent for a failed Sketch.
    std::bitset<4> revealed_characters; // Event SHOW_CHAR, independent of combat admission.
    std::optional<std::pair<std::uint8_t, bool>> saved_attacker_direction;
    // Command-level save/restore can span separate authored animation bodies.
    std::array<std::optional<BattleAnimationPoint>, 4> saved_character_positions{};
    std::uint8_t repeated_hit = 0; // Authored hit-index branches (animation command C5).
    std::array<std::uint8_t, 3> dice{}; // Combat-owned faces, zero for an absent die.
    bool weapon_left_hand = false;
    bool vertical_sound_pan = false; // Dance/Sketch admit the authored attacker-Y sound cues.
    bool suppress_fight_step = false; // A prior strike already moved this caster forward.
    bool jump_again = false; // Dragon Horn's next landing remains in the same action.
    // Blocks retain their selected presentation independently of the miss flag.
    // Cover movement still requires its separate caller.
    std::array<std::uint8_t, 4> block_animations{};
    std::bitset<10> targets, hits;
    // Initial targets displaying the shared reflection block. Rebound tasks
    // are keyed by reflector, target the mapped recipient and draw beside it.
    std::bitset<10> reflecting_targets{};
    std::array<std::optional<std::uint8_t>, 10> reflection_targets{};
    // Volley tasks share mapped recipients with rebounds, but every projectile
    // originates at the caster rather than at its logical thread owner.
    bool projectile_volley = false;
    BattleEffectDisplay display;
    // InitMsgWindow counts uses for the lifetime of the battle. A nonzero
    // count makes LoadAnimProp clear its retained foreground/text surface.
    std::uint8_t message_window_uses = 0;
    BattleSceneAnchors anchors;
    std::array<BattleCharacterPose, 4> characters;
    std::optional<std::array<bool, 4>> character_back_rows, character_hand_swaps;
    std::array<std::int16_t, 4> character_row_distances{};
    std::array<std::optional<std::uint8_t>, 4> character_identities{};
    std::array<BattleMonsterPosition, 6> monsters;
    std::array<std::uint8_t, 6> monster_widths{}, monster_heights{}, monster_facing{};
    BattleAnimationControl control;
    std::array<BattleDrawTarget, 10> draw_order{};
    std::array<BattleMonsterStyle, 6> monster_styles{};
    std::array<bool, 4> needs_step_back{};
    // Retained across repeated hits until the common action return (C1/9B19).
    std::array<std::uint8_t, 4> step_back_facing{};
    std::uint8_t random_index = 0;
    // Logical battle presentation time, shared across casting/body/return.
    // Isolated passes advance once; live callers supply their actor clock.
    std::uint16_t presentation_tick = 0;
    std::uint8_t battle_type = 0;
    bool magitek = false;
    // Displayed statuses are sampled separately from newly resolved battle effects.
    std::array<std::uint32_t, 4> shown_character_status{};
    // Working character banks, including status colors and armor palettes.
    // Construction snapshots these colors once, as the original CopyPal does.
    BattleCharacterPaletteBanks character_colors{};
    BattleMonsterPaletteBanks monster_colors{};
    std::vector<std::array<Rgb555, 16>> battlefield_colors;
    std::shared_ptr<const BattleMonsterBackgroundSource> monster_images;
    std::optional<BattlePublishedCharacterFrames> published_character_frames;
    std::optional<BattlePublishedStatusTiles> published_status_tiles;
    // Authored scroll selections, retained for Specter's observable palette
    // alias. They identify native effects; they are not hardware registers.
    std::array<std::uint8_t, 3> scroll_programs{
        static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_EFFECT_INITIAL_SCROLL_0 & 0xffU),
        static_cast<std::uint8_t>((FF6_ROM_VALUE_BATTLE_EFFECT_INITIAL_SCROLL_0 >> 8U) & 0xffU),
        static_cast<std::uint8_t>((FF6_ROM_VALUE_BATTLE_EFFECT_INITIAL_SCROLL_0 >> 16U) & 0xffU)};
    // Independent original visibility gates; entering monsters are not an
    // additional filter for an explicit target copy.
    std::bitset<6> shown_monsters, visible_monsters{63}, background_monsters;
    bool freeze_character_palettes = false, hide_characters = false;
    bool hide_character_indicator = false, menus_disabled = false;
};
void apply_battle_effect_actor_state(const BattleEffectContext& context, BattleActorScene& actors,
                                     BattleActorSceneRules& rules);
void apply_battle_effect_display(const BattleEffectDisplay& context, RasterScene& scene);
struct BattleEffectSpriteSelection {
    BattleAnimationPoint position;
    std::uint8_t priority = FF6_ROM_VALUE_BATTLE_EFFECT_DEFAULT_PRIORITY_0,
                 palette = FF6_ROM_VALUE_BATTLE_EFFECT_DEFAULT_PALETTE_0;
    bool flipped = false, supplemental_image = false;
    bool character_image = false;
};
struct BattleCharacterOrbit {
    std::uint8_t phase = 0, radius = FF6_ROM_VALUE_BATTLE_CHARACTER_ORBIT_RADIUS_0, limit = 0;
};
struct BattleEffectTask {
    BattleAnimationTask script;
    BattleEffectLayer layer = BattleEffectLayer::sprite;
    std::uint8_t copy = 0, frame_offset = 0, priority = FF6_ROM_VALUE_BATTLE_EFFECT_DEFAULT_PRIORITY_0;
    // Retained at task creation, independently of later image/frame selection.
    // Frame height is in 16-pixel pieces; target height is in 8-pixel tiles.
    std::uint8_t frame_height = 0, target_height = 0;
    bool behind_actor = false, sprite_visible = false;
    std::uint8_t palette = FF6_ROM_VALUE_BATTLE_EFFECT_DEFAULT_PALETTE_0;
    bool supplemental_image = false;
    bool character_image = false;
    // Frame selection commits these values separately from working task state.
    // Resource waits can expose pending movement/flags before the next frame;
    // sprite preparation must retain the previous selection throughout them.
    BattleEffectSpriteSelection selected_sprite;
    // Polar and projectile commands reuse radius/phase. Byte-radius writes
    // preserve the high byte; projectile writes replace the full word.
    std::uint16_t radius = 0, vector_length = 0;
    std::uint16_t seize_stretch = 0;
    std::uint8_t motion_phase = 0, vector_angle = 0, vector_dx = 0, frame_counter = 1;
    std::uint8_t bounce_phase = 0, wave_phase = 0;
    std::optional<std::size_t> return_to;
    // Integer line stepping has an error remainder which survives the return
    // journey. It must not be replaced with floating point interpolation. This
    // owner replaces the original projectile scratch values with line state.
    bool character_line = false;
    BattleCharacterLine line;
    std::optional<BattleAnimationPoint> monster_line_origin;
    BattleAnimationPoint monster_line_displacement;
    std::optional<BattleCharacterOrbit> character_orbit;
};
struct BattleEffectFrame {
    // PlayAnimSfx replaces one pending cue; UpdateSfx consumes the last value.
    // Keep requests separately for diagnostics, never dispatch all of them.
    std::optional<GameSoundEffect> sound;
    bool complete = false;
    std::vector<GameSoundEffect> sound_requests;
    bool resource_wait = false;
    // An implicit picture boundary inside a command, not an explicit task wait.
    bool raster_wait = false;
    std::vector<BattleActorAppearanceChange> appearance_changes{};
    std::optional<SystemSoundEffect> system_sound{};
    std::optional<PlaybackVolumeRamp> volume{};
};
// Explicit caller work in the picture plan's clock domain. Each entry belongs
// to the next gradient command reached by the native data-language interpreter.
// No observed clock or default CPU-to-elapsed conversion is supplied here.
struct BattleGradientWriterPlan {
    BattleRasterTick start = 0;
    std::vector<BattleRasterTick> stores;
    BattleRasterTick tail = 0;
};
struct BattleEffectPictureWork {
    BattleRasterPicturePlan picture;
    std::vector<BattleGradientWriterPlan> gradients;
};
// A synchronization floor within the active picture, not a CPU-cycle estimate.
// Row writes before a barrier can still overrun while preparing a large effect.
enum class BattleEffectRasterOperation { barrier, gradient, mask, affine };
struct BattleEffectRasterEvent {
    BattleEffectRasterOperation operation;
    std::uint16_t earliest_scanline = 0;
    bool operator==(const BattleEffectRasterEvent&) const = default;
};
// Complete write deadlines for one presentation interval. Omission selects
// the boundary-normalized caller, retaining proven after-battlefield barriers;
// it does not claim original work timing. Partial observations reject.
struct BattleEffectRasterTiming {
    std::vector<BattleRasterDeadline> gradient, mask;
};
// Native effect state: a repeating displacement curve or a retained per-line
// image projection. These are game algorithms, not device programs/registers.
enum class BattleScrollPattern { fixed, wave32, wave64, full };
struct BattleScrollAxis {
    std::uint8_t amplitude = 0, phase = 0, speed = 0;
    std::array<std::uint16_t, 64> offsets{};
    // D6 specifies a displacement modulo 256. This is equivalent to the
    // original retained high byte only on a verified repeating source plane.
    bool byte_displacement = false;
};
struct BattleEffectScroll {
    BattleScrollPattern pattern = BattleScrollPattern::fixed;
    bool projection_owned = false;
    std::array<BattleScrollAxis, 2> axes;
    std::array<BattleAnimationPoint, 151> projection{};
};
struct BattleTornadoMotion {
    bool initialized = false;
    BattleAnimationPoint origin;
    std::uint8_t horizontal_phase = FF6_ROM_VALUE_BATTLE_TORNADO_INITIAL_PHASE_0, vertical_phase = 0;
    std::uint8_t radius = 0, row_phase = 0;
};
struct BattleEffectMask {
    bool configured = false, published = false;
    std::uint8_t x = 0, y = 0, radius = 0;
    std::uint8_t final_x = 0, final_y = 0, final_radius = 0, speed = 0, phase = 0;
    std::uint8_t shape = 0;
    std::array<BattleMaskSpan, 151> rows{};
};
struct BattleEffectTriangle {
    bool configured = false;
    std::uint8_t x = 0, y = 0, size = 0, phase = 0;
};
struct BattleEffectCompositing {
    ColorBlend operation = ColorBlend::none;
    // Data-language layer bits: foreground 1, battlefield 2, overlay 4,
    // objects 16. Operand images remain independent of visible mask coverage.
    std::uint8_t affected_layers = 0, operand_layers = 0;
    bool fixed_color = false;
};
// Scripts scale and rotate authored images about a moving center. Values wrap
// as words in the data language; projection gives the renderer ordinary fixed
// point source coordinates. There is no register stream or device state here.
struct BattleEffectAffine {
    bool enabled = false, flip_x = false, flip_y = false;
    std::uint16_t scale_x = 256, scale_y = 256, center_x = 0, center_y = 0;
    std::uint16_t cross_x = 0, cross_y = 0;
};
AffineLine project_battle_effect_affine(const BattleEffectAffine& transform,
                                        BattleAnimationPoint origin, unsigned row);
AffineLayer make_battle_summon_affine(const MonsterGraphic& source, bool folded);
// Executes original animation DATA with semantic tasks, images and cues. This
// is independent of damage/RNG, the audio callback and display refresh.
class BattleAttackAnimation {
  public:
    const BattleDisplayTransfers& effect_image_transfers() const {
        return effect_image_transfers_;
    }
    BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                          std::uint16_t definition, BattleEffectContext context,
                          BattleEffectImageState retained = {});
    BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                          BattlePhysicalAttackPresentation request, BattleEffectContext context,
                          BattleEffectImageState retained = {});
    // Standalone event tasks use their own bounded data resource, with absolute
    // script pointers normalized at the resource boundary. No padded ROM image.
    BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                          std::shared_ptr<const Bytes> event_programs, BattleEffectContext context);
    void begin_event_batch(BattleEffectContext context);
    void add_event_task(unsigned slot, BattleAnimationTask task);
    void inherit_event_tasks(const BattleAttackAnimation& source);
    std::vector<MusicPlayerCommand> take_event_audio();
    void hold_completed_frame() { hold_completed_frame_ = true; }
    BattleEffectImageState retained_images() const;
    BattleEffectFrame advance(std::size_t command_budget = 16384);
    // Resume through one presentation wait. Unlike the isolated pass adapter,
    // this suspends a task during resource loading, without ticking other tasks.
    BattleEffectFrame advance_presentation(std::size_t command_budget = 16384,
                                           std::optional<BattleEffectRasterTiming> timing = {});
    // Executes the real task caller through one picture, retaining a command
    // interrupted by that boundary. Plans are mandatory on subsequent calls;
    // no incomplete elapsed schedule is silently replaced with normalized time.
    // Scheduled mask writers and the native elapsed-plan producer remain open.
    BattleEffectFrame advance_scheduled(BattleEffectPictureWork work,
                                        std::size_t command_budget = 16384);
    const BattleEffectRaster& raster_picture() const {
        return raster_picture_;
    }
    unsigned loading_waits() const {
        return loading_waits_;
    }
    unsigned preparation_waits() const { return preparation_waits_; }
    // Diagnostic lower bound only; excludes unmodeled owners/interruptions.
    unsigned minimum_update_work() const { return update_work_floor_; }
    const auto& raster_events() const {
        return raster_events_;
    }
    // Sprite preparation and the separately staged battlefield palette may
    // precede the current effect image. Isolated-body diagnostics may omit them.
    // Effect colors can publish independently in sprite/foreground/overlay order.
    void compose(RasterScene& scene, std::span<const BattleActorSpriteGroup> actors = {},
                 const BattleAttackAnimation* prepared_sprites = nullptr,
                 const BattleEffectDisplay* visible_display = nullptr,
                 std::span<const std::array<Rgb555, 16>> visible_battlefield_colors = {},
                 std::span<const std::array<Rgb555, 16>> visible_effect_colors = {}) const;
    // The actor owner publishes this independently of effect task execution.
    void update_draw_order(const std::array<BattleDrawTarget, 10>& order);
    // Live actor coordinates used by depth decisions. Existing task targets
    // remain the snapshots selected by their original script commands.
    void update_anchors(const BattleSceneAnchors& anchors);
    BattleActorPaletteOverrides actor_palettes(const BattleAttackAnimation* published_selection = nullptr) const;
    // Commit script-owned fields while retaining ordinary actor frame counters
    // and the already-published sprite/image buffers.
    void apply_actor_state(BattleActorScene& actors, BattleActorSceneRules& rules) const;
    // Ordinary actor updates may replace working colors between script passes;
    // they must not replace the animation's retained base or relative offsets.
    void update_character_colors(const BattleCharacterPaletteBanks& colors) {
        context_.character_colors = colors;
    }
    void update_monster_colors(const BattleMonsterPaletteBanks& colors) {
        context_.monster_colors = colors;
    }
    void update_presentation_tick(std::uint16_t tick) {
        context_.presentation_tick = tick;
    }
    void update_character_frames(const BattlePublishedCharacterFrames& frames) {
        context_.published_character_frames = frames;
    }
    void update_status_tiles(const BattlePublishedStatusTiles& tiles) {
        context_.published_status_tiles = tiles;
    }
    void update_forced_character_frame(std::optional<std::uint8_t> frame) {
        context_.control.force_character_frame = frame;
    }
    const std::optional<BattleMonsterBackground>& monster_background() const {
        return monster_background_;
    }
    std::bitset<6> background_hidden_monsters() const {
        return hidden_background_monsters_;
    }
    const std::optional<std::array<Rgb555, 16>>& copied_background_colors() const {
        return copied_background_colors_;
    }
    const std::array<BattleEffectScroll, 3>& scrolling() const {
        return scrolling_;
    }
    // Image destinations are one-shot. Each quadrant retains its own frame
    // until a later publication replaces it, including an explicit blank.
    const auto& background_images() const {
        return background_images_;
    }
    const auto& background_destinations() const {
        return background_destinations_;
    }
    const BattleEffectMask& masking() const {
        return mask_;
    }
    const BattleEffectTriangle& triangle() const {
        return triangle_;
    }
    const BattleEffectGradient& gradient() const {
        return gradient_;
    }
    const std::optional<BattleEffectAffine>& affine() const {
        return affine_;
    }
    BattleEffectCompositing compositing() const {
        return {blend_, blended_layers_, operand_layers_, fixed_color_};
    }
    const BattleEffectImage* image(BattleEffectLayer layer) const {
        const auto& image = images_.at(static_cast<std::size_t>(layer));
        return image ? &*image : nullptr;
    }
    const std::array<Rgb555, 16>& secondary_foreground_colors() const { return secondary_colors_; }
    const std::array<Rgb555, 16>& effect_colors(BattleEffectLayer layer, bool base = false) const;
    const BattleEffectContext& context() const {
        return context_;
    }
    bool complete() const {
        return complete_;
    }
    std::uint32_t frames() const {
        return frames_;
    }
    std::uint8_t sprite_task_count(bool monster) const {
        return alternate_sprite_count_ && (context_.presentation_tick & 1U)
                   ? FF6_ROM_VALUE_EFFECT_ALTERNATE_DRAW_COUNT_0 : sprite_task_counts_[monster ? 1 : 0];
    }
    const std::array<std::optional<BattleEffectTask>, 85>& tasks() const {
        return tasks_;
    }

  private:
    std::shared_ptr<const BattleEffectLibrary> library_;
    BattleEffectDefinition definition_;
    std::shared_ptr<const Bytes> event_programs_;
    std::bitset<85> event_task_sources_;
    std::uint8_t thread_launch_spacing_ = 1;
    std::vector<MusicPlayerCommand> event_audio_;
    BattleAttackAnimation(std::shared_ptr<const BattleEffectLibrary> library,
                          BattleEffectDefinition definition, BattleEffectContext context,
                          BattleEffectImageState retained, bool physical);
    BattleEffectContext context_;
    std::array<std::optional<BattleEffectImage>, 4> images_;
    // Palette loading is independent of image loading. Invisible planes still
    // own their selected colors and can receive authored palette changes.
    std::array<std::array<Rgb555, 16>, 3> imageless_base_colors_{}, imageless_colors_{};
    std::array<Rgb555, 16> secondary_base_colors_{}, secondary_colors_{};
    std::optional<std::uint8_t> affine_radius_;
    std::optional<BattleEffectAffine> affine_;
    std::shared_ptr<const AffineLayer> affine_image_;
    std::optional<BattleEffectImage> supplemental_image_;
    std::optional<std::size_t> loaded_summon_;
    bool sketch_layout_ = false;
    std::optional<std::uint8_t> sketch_frame_;
    bool foreground_frames_cleared_ = false;
    std::uint8_t foreground_low_rows_ = 0;
    std::uint8_t steering_limit_ = FF6_ROM_VALUE_EFFECT_STEERING_INITIAL_0, steering_ticks_ = 0;
    bool battlefield_layout_ = false, battlefield_pixels_ = false;
    std::optional<BattleMonsterBackground> monster_background_;
    std::optional<std::array<Rgb555, 16>> copied_background_base_, copied_background_colors_;
    std::bitset<6> hidden_background_monsters_;
    std::array<std::optional<BattleEffectTask>, 85> tasks_;
    // Event-animation position saves belong to characters, not task copies.
    std::array<std::uint8_t, 7> palette_counters_{}; // Effects, monsters, second foreground bank.
    std::array<BattleEffectColorOffset, 6> color_offsets_;
    BattleCharacterPaletteBanks base_character_colors_;
    BattleMonsterPaletteBanks base_monster_colors_;
    std::vector<std::array<Rgb555, 16>> base_battlefield_colors_;
    std::array<BattleEffectScroll, 3> scrolling_;
    // Mind Blast uses deterministic per-row delays and a growing displacement
    // added to the freshly calculated wave. These are not gameplay RNG draws.
    std::array<std::uint8_t, 32> disintegration_delays_{};
    std::array<std::uint16_t, 32> disintegration_displacements_{};
    BattleTornadoMotion tornado_;
    BattleEffectMask mask_;
    BattleEffectTriangle triangle_;
    BattleEffectGradient gradient_;
    std::uint8_t monster_palette_shift_ = 0;
    std::array<bool, 2> masked_backgrounds_{};
    bool overlay_high_priority_ = true;
    std::uint8_t visible_layers_ = 0x17;
    std::optional<unsigned> entrance_lines_, unfolding_lines_;
    std::bitset<10> recolored_targets_;
    std::array<bool, 3> paused_{}, hold_image_{};
    bool detached_character_vector_ = false; // Keep effect sprites stationary as their caster approaches.
    std::array<std::optional<std::uint8_t>, 2> background_frames_;
    std::array<std::array<std::optional<std::uint8_t>, 4>, 2> background_images_;
    std::array<std::optional<std::uint8_t>, 2> pending_backgrounds_;
    BattleDisplayTransfers effect_image_transfers_;
    std::array<std::uint8_t, 2> background_destinations_{};
    std::array<BattleAnimationPoint, 2> background_positions_{};
    std::array<std::optional<BattleAnimationPoint>, 2> background_anchors_;
    std::array<BattleAnimationPoint, 2> background_views_{};
    std::array<bool, 2> background_flipped_{};
    std::array<std::uint8_t, 2> sprite_task_counts_{
        FF6_ROM_VALUE_EFFECT_INITIAL_DRAW_COUNT_0, FF6_ROM_VALUE_EFFECT_INITIAL_DRAW_COUNT_0};
    bool alternate_sprite_count_ = false;
    bool esper_behind_actors_ = false;
    std::uint8_t operand_layers_ = 0, blended_layers_ = 0;
    ColorBlend blend_ = ColorBlend::none;
    bool fixed_color_ = false;
    bool complete_ = false, hold_completed_frame_ = false;
    std::uint32_t frames_ = 0;
    bool pending_system_sound_ = false;
    void publish_system_sound(BattleEffectFrame& frame);
    std::optional<GameSoundEffect> pending_sound_;
    std::vector<GameSoundEffect> sound_requests_;
    unsigned loading_waits_ = 0;
    unsigned preparation_waits_ = 0;
    bool initializing_ = true;
    bool presentation_timing_ = false, pass_started_ = false, pass_active_ = false;
    std::size_t next_task_ = 0;
    std::optional<BattleEffectLayer> resumed_layer_;
    unsigned resource_waits_ = 0;
    std::vector<BattleActorAppearanceChange> appearance_changes_;
    bool supplemental_pending_ = false;
    std::uint16_t raster_floor_ = 0;
    std::vector<BattleEffectRasterEvent> raster_events_;
    BattleEffectRaster raster_picture_;
    // Direct projections can change after the battlefield has been sampled.
    // Prepared waves, image/mask publication and blend roles have other owners.
    std::optional<std::array<BattleEffectScroll, 3>> picture_scrolling_;
    BattleEffectRaster::Mask mask_publication_rows_{};
    std::optional<BattleEffectRasterTiming> raster_timing_;
    std::optional<BattleRasterTimeline> raster_timeline_;
    std::optional<BattleEffectPictureWork> picture_work_;
    std::optional<BattleEffectGradient> pending_gradient_;
    std::size_t next_gradient_plan_ = 0;
    std::size_t gradient_writes_ = 0, mask_writes_ = 0;
    bool mask_pending_ = false;
    // A late mask rebuild can cross a display boundary before its result is
    // ready. Resume the command without revisiting earlier tasks or cues.
    bool mask_build_pending_ = false;
    std::optional<std::array<std::uint8_t, 2>> gradient_build_pending_;
    std::optional<std::uint8_t> wave_build_pending_;
    unsigned update_work_floor_ = 0;
    void accrue_update_work(unsigned minimum_source_cycles, unsigned slow_bytes = 0);
    bool update_crosses_picture(unsigned minimum_source_cycles, unsigned slow_bytes = 0);
    bool mask_crosses_picture();
    void begin_gradient(std::uint8_t operation, std::uint8_t parameter, bool resuming = false);
    BattleEffectFrame step(std::size_t budget, bool presentation);
    void publish_backgrounds();
    void finish_supplemental_load();
    void raster_event(BattleEffectRasterOperation operation);
    void publish_gradient();
    bool resume_gradient();
    void publish_mask();
    bool command(std::size_t task, BattleEffectLayer execution_layer);
    bool palette_command(std::uint8_t command, std::uint8_t parameter);
    void set_blend(std::uint8_t operation, std::uint8_t affected, std::uint8_t operand);
    void initialize(std::uint8_t identity);
    enum class TargetBackgroundCopy { monster_only, general, death, attacker };
    void copy_target_background(TargetBackgroundCopy kind);
    void load_summon(std::size_t identity, BattleEffectLayer layer);
    void load_monster_image(const MonsterGraphic&, BattleEffectLayer);
    void initialize_affine(std::size_t identity, bool folded);
    void initialize_authored_affine(unsigned graphics_offset, unsigned layout_offset);
    void load_supplemental_summon(std::size_t identity);
    void prioritize_from(std::uint8_t participant, bool reset = true);
    std::uint8_t random();
    void set_vector(BattleEffectTask& task, std::uint8_t x0, std::uint8_t y0, std::uint8_t x1,
                    std::uint8_t y1);
    void move_vector(BattleEffectTask& task) const;
    void move_boomerang(BattleEffectTask& task) const;
    bool move_character_jump(BattleEffectTask& task, std::uint8_t speed, bool backwards);
    BattleAnimationPoint background_origin(unsigned layer) const;
    void init_scroll_wave(std::uint8_t selection, std::uint8_t axes);
    void update_scroll_wave(std::uint8_t selection);
    void select_scroll(std::uint8_t selection);
    void update_entrance_scroll(std::uint8_t operation);
    void update_unfolding_scroll(std::uint8_t operation);
    void initialize_curved_overlay(bool horizontal);
    void reset_tornado();
    void move_tornado(BattleEffectTask& task, std::uint8_t horizontal, std::uint8_t vertical);
    void prioritize_tornado(const BattleEffectTask& task);
    void advance_curved_overlay();
    void update_mask();
    void move_mask(std::uint8_t x, std::uint8_t y, std::uint8_t radius);
    void select_mask_shape(std::uint8_t shape);
    void update_triangle(bool spatial);
    void open_mask(std::uint8_t inset);
};
} // namespace ff6
