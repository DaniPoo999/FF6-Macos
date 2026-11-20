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
Mode load_preference(const std::filesystem::path& folder);
void save_preference(const std::filesystem::path& folder, Mode mode);

struct Viewport { float x{}, y{}, width{}, height{}; };
Viewport fit(int width, int height);
float detail(float scale);
float headroom(float value);
Mode effective(Mode requested, bool metal, float potential, float current);
float target_gain(float scale, float current);
float hdr_detail(float scale, float gain);
float approach_gain(float previous, float target, float seconds);
std::vector<std::uint8_t> unpack(std::span<const std::uint32_t> pixels);

// Explicitly matches Metal's constant buffer: one float4 and four scalars.
struct Uniforms {
    Viewport viewport;
    std::uint32_t mode{};
    float gain{1}, headroom{1}, detail{1};
};
static_assert(sizeof(Uniforms) == 32);
using RGB = std::array<float, 3>;
float srgb_to_linear(float signal);
// Independent scalar reference for GPU tests and white-field calibration.
RGB reference_pixel(std::span<const std::uint32_t> source, float x, float y,
                    const Uniforms& uniforms);
} // namespace presentation
