#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace presentation {
enum class Mode : std::uint32_t { off, sdr, hdr };
Mode next(Mode mode);
Mode parse(std::string_view text);
const char* name(Mode mode);
struct Settings {
    bool enabled{true};
    Mode shader{Mode::hdr};
    bool curvature{true};
    bool aspect_4_3{true};
    float curvature_strength{0.25F}, sharpness{0.6F}, scanlines{0.85F};
    float phosphor_size{1}, phosphor_strength{0.85F}, glow{0.04F}, brightness{1};
    Mode mode() const { return enabled ? shader : Mode::off; }
    bool operator==(const Settings&) const = default;
};
Settings sanitized(Settings settings);
Settings load_settings(const std::filesystem::path& folder);
void save_settings(const std::filesystem::path& folder, Settings settings);
Mode load_preference(const std::filesystem::path& folder);
void save_preference(const std::filesystem::path& folder, Mode mode);

struct Viewport { float x{}, y{}, width{}, height{}; };
Viewport fit(int width, int height, bool aspect_4_3 = false);
float detail(float scale);
float headroom(float value);
Mode effective(Mode requested, bool metal, float potential, float current);
float target_gain(float scale, float current, Settings settings = {});
float hdr_detail(float scale, float gain, Settings settings = {});
float approach_gain(float previous, float target, float seconds);
std::vector<std::uint8_t> unpack(std::span<const std::uint32_t> pixels);

// Explicitly matches Metal's constant buffer (all offsets are 32-bit scalars).
struct Uniforms {
    Viewport viewport;
    std::uint32_t mode{};
    float gain{1}, headroom{1}, detail{1};
    float curvature{}, sharpness{0.6F}, scanlines{0.85F}, phosphor_size{1};
    float phosphor_strength{0.85F}, glow{0.04F}, brightness{1}, reserved{};
};
static_assert(sizeof(Uniforms) == 64);
Uniforms uniforms(Viewport viewport, Mode mode, float current, float gain, Settings settings);
using RGB = std::array<float, 3>;
float srgb_to_linear(float signal);
// Independent scalar reference for GPU tests and white-field calibration.
RGB reference_pixel(std::span<const std::uint32_t> source, float x, float y,
                    const Uniforms& uniforms);
} // namespace presentation
