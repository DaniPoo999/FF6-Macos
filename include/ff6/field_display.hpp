#pragma once
#include "ff6/field_animation.hpp"

namespace ff6 {
// Ordinary field backgrounds sample from screen origin before windows, object
// composition and color math. Apply before dialogue adds its independent layers.
void apply_field_mosaic(RasterScene& scene, std::uint8_t block_size);
// Events 54–57 share one retained fixed-color effect. Intensities have three
// fractional bits; reissuing either ramp command releases the active effect
// instead of replacing its channels, operation or speed.
struct FieldColorEffect {
    std::uint8_t channels = 0, counter = 0, rate = 0, target = 0;
    bool subtract = false, increasing = false;
    void flash(std::uint8_t colors);
    void ramp(std::uint8_t control, bool subtraction);
    void apply(RasterScene& scene);
};
// Decaying vertical shake, shared by all field scenes. Offsets are presentation
// only: camera following, collision and object positions remain authoritative.
struct FieldShake {
    std::uint8_t control = 0, amplitude = 0;
    std::array<std::uint8_t, 4> offsets{};
    void advance(bool odd_frame, const std::array<std::uint8_t, 256>& random,
                 std::uint8_t& random_index);
};
using FieldWindowSpans = std::array<std::array<std::uint16_t, 2>, 208>;
// Shared field aperture: the map selects which layers it reveals. Circle and
// pyramid producers supply half-open intervals, not hardware window programs.
struct FieldWindowMask {
    bool active = false;
    FieldWindowSpans spans{};
    void apply(RasterScene& scene, std::uint8_t mode) const;
};
// Event 63 / UpdateFlashlight: a retained radius, moving by two pixels per
// field display. Rows use the original integer circle construction; the scene
// consumes semantic clipping spans, never a hardware window program.
struct FieldFlashlight {
    bool active = false;
    std::uint8_t radius = 0, target = 0;
    FieldWindowSpans spans{};
    void advance(std::uint8_t center_x, std::uint8_t center_y);
    // Requires all three background layers in source order.
    void apply(RasterScene& scene, std::uint8_t mode) const;
};
using FieldWindowPoint = std::array<std::uint8_t, 2>;
FieldWindowSpans field_triangle_spans(std::array<FieldWindowPoint, 3> points);
std::array<std::uint8_t, 256> load_field_window_sine(const ContentPack& pack);
// A6/A7 retain phase across disable/re-enable. The anchor is a direct object
// identity, not a party-slot alias, and need not currently have a visible sprite.
struct FieldPyramid {
    std::optional<std::uint8_t> object;
    std::uint16_t phase = 0;
    FieldWindowSpans spans{};
    void advance(std::uint8_t center_x, std::uint8_t center_y,
                 const std::array<std::uint8_t, 256>& sine);
};
struct FieldPaletteState {
    std::array<Rgb555, 128> base, pending;
};
// Inclusive normalized color indices, wrapping from 127 to 0. Event decoding
// removes the high-bit alias introduced by the original eight-bit byte offset.
void modify_field_background_palette(FieldPaletteState& palette, std::uint8_t control,
                                     std::uint8_t first, std::uint8_t last);
void modify_field_sprite_palettes(std::array<std::array<Rgb555, 16>, 8>& colors,
                                  const std::array<std::array<Rgb555, 16>, 8>& base,
                                  std::uint8_t control, std::uint8_t first, std::uint8_t last);
// Event 61 derives a channel-filtered average from the original color. Repeated
// filtering must not average an already filtered presentation palette.
Rgb555 filter_field_color(Rgb555 original, std::uint8_t channels);
// Ordinary field framing, at logical scroll origins. Special windows, waves,
// objects and dialogue still need their own native scene producers.
RasterScene make_field_display_scene(const FieldMap& map,
                                     const std::array<FieldOrigin, 3>& logical_origins,
                                     std::uint8_t intensity, const FieldMapView* view = nullptr,
                                     std::array<bool, 3> include_inactive = {});
class FieldDisplay {
  public:
    // Phase is retained across map loads in the original. Startup must supply it;
    // initial palette ownership can include already applied window preferences.
    FieldDisplay(FieldMap map, FieldAnimationAssets animation, bool third_phase,
                 std::optional<FieldPaletteState> palette = std::nullopt);
    const FieldMap& map() const {
        return map_;
    }
    const FieldPaletteState& palette() const {
        return palette_;
    }
    const FieldAnimationState& animation_state() const {
        return animation_.state();
    }
    bool third_phase() const {
        return third_phase_;
    }
    bool displaying() const {
        return displaying_;
    }
    void advance_palette();
    void set_background_rate(std::uint8_t group, std::uint8_t rate) {
        animation_.set_background_rate(group, rate);
    }
    void load_palette(std::uint8_t destination, const std::array<Rgb555, 16>& colors);
    void modify_palette(std::uint8_t control, std::uint8_t first, std::uint8_t last);
    void filter_palette(std::uint8_t channels, std::uint8_t first, std::uint8_t last);
    void restore_color_math();
    void refresh_text_color(Rgb555 font_color);
    void set_window_colors(const std::array<Rgb555, 7>& colors);
    // Commits pending colors for the next visible frame and advances field phase.
    // Intensity belongs to the separate game fade owner, not this presentation.
    void commit(std::uint8_t intensity);
    void commit(std::uint8_t intensity, const FieldMapView& view);
    RasterScene scene(const std::array<FieldOrigin, 3>& logical_origins,
                      std::array<bool, 3> include_inactive = {}) const;
    // Caller has consumed the visible frame before this updates graphics for the
    // following frame. Palette updates may occur on either side of this boundary.
    void finish();

  private:
    FieldMap map_;
    FieldComposition original_composition_;
    FieldAnimation animation_;
    FieldPaletteState palette_;
    std::optional<FieldMapView> view_;
    bool third_phase_, displaying_ = false;
    std::uint8_t intensity_ = 0;
};
} // namespace ff6
