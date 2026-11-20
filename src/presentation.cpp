#include "presentation.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace presentation {
Mode next(Mode mode) {
    return mode == Mode::off ? Mode::sdr : mode == Mode::sdr ? Mode::hdr : Mode::off;
}
Mode parse(std::string_view text) {
    if (text == "off") return Mode::off;
    if (text == "sdr") return Mode::sdr;
    if (text == "hdr") return Mode::hdr;
    throw std::invalid_argument("CRT mode must be off, sdr or hdr");
}
const char* name(Mode mode) {
    return mode == Mode::off ? "off" : mode == Mode::sdr ? "sdr" : "hdr";
}
Mode load_preference(const std::filesystem::path& folder) {
    std::error_code error;
    const auto length = std::filesystem::file_size(folder/"presentation.cfg",error);
    if (error || length > 64) return Mode::off;
    std::ifstream input(folder / "presentation.cfg");
    std::string header, value, extra;
    if (!std::getline(input, header) || header != "FFVI presentation 1" ||
        !std::getline(input, value) || std::getline(input, extra)) return Mode::off;
    try { return parse(value); } catch (const std::invalid_argument&) { return Mode::off; }
}
void save_preference(const std::filesystem::path& folder, Mode mode) {
    std::filesystem::create_directories(folder);
    static std::atomic<unsigned> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary = folder / ("presentation." + std::to_string(stamp) + "." +
                                      std::to_string(sequence++) + ".tmp");
    try {
        std::ofstream out(temporary, std::ios::binary);
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out << "FFVI presentation 1\n" << name(mode) << '\n';
        out.close();
        std::filesystem::rename(temporary, folder / "presentation.cfg");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
Viewport fit(int width, int height) {
    if (width <= 0 || height <= 0) return {};
    const double scale = std::min(static_cast<double>(width) / 256.0,
                                  static_cast<double>(height) / 224.0);
    const float w = static_cast<float>(256.0*scale), h = static_cast<float>(224.0*scale);
    return {std::max(0.0F,(static_cast<float>(width)-w)*0.5F),
            std::max(0.0F,(static_cast<float>(height)-h)*0.5F),w,h};
}
float detail(float scale) {
    const float t = std::clamp((scale - 1.5F) / 1.5F, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}
float headroom(float value) {
    return std::isfinite(value) ? std::clamp(value, 1.0F, 64.0F) : 1.0F;
}
Mode effective(Mode requested, bool metal, float potential, float current) {
    if (!metal) return Mode::off;
    if (requested == Mode::hdr && (headroom(potential) <= 1 || headroom(current) <= 1))
        return Mode::sdr;
    return requested;
}
float target_gain(float scale, float current) {
    const float d = detail(scale);
    // Integral of the white beam is .9 * (1 + 1 + .6 + 0) / 4 = .585.
    // Each channel occupies one of four aperture-grille columns.
    const float coverage = (1.0F - d + d * 0.585F) * (1.0F - 0.75F * d);
    return std::min(1.0F / coverage, headroom(current));
}
float hdr_detail(float scale, float gain) {
    // Invert (1-.415*d)*(1-.75*d) = 1/gain. When a display has less
    // headroom, soften the gaps instead of making the entire image darker.
    if (gain >= target_gain(scale,64)) return detail(scale);
    const float available = (1.165F-std::sqrt(1.357225F-1.245F*(1-1/headroom(gain))))/.6225F;
    return std::min(detail(scale),std::clamp(available,0.0F,1.0F));
}
float approach_gain(float previous, float target, float seconds) {
    if (target <= previous) return target;
    return previous + (target - previous) * (1.0F - std::exp(-std::max(seconds, 0.0F) / 0.25F));
}
std::vector<std::uint8_t> unpack(std::span<const std::uint32_t> pixels) {
    std::vector<std::uint8_t> bytes(pixels.size() * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i)
        for (unsigned c = 0; c < 4; ++c)
            bytes[i * 4 + c] = static_cast<std::uint8_t>(pixels[i] >> (24U - c * 8U));
    return bytes;
}
float srgb_to_linear(float signal) {
    return signal <= 0.04045F ? signal / 12.92F : std::pow((signal + 0.055F) / 1.055F, 2.4F);
}
namespace {
float channel(std::span<const std::uint32_t> source, int x, int y, unsigned c) {
    if (x < 0 || x >= 256 || y < 0 || y >= 224) return 0;
    return static_cast<float>((source[static_cast<std::size_t>(y * 256 + x)] >>
                               (24U - c * 8U)) & 255U) / 255.0F;
}
double integral(double distance, double signal) {
    const double radius = (0.45 + 0.45 * signal) * 0.5;
    const double t = std::min(std::abs(distance) / radius, 1.0);
    const double value = radius * (t + (-1.0 + 0.6 * signal) * t*t*t +
                                   (0.5 - 0.45 * signal) * t*t*t*t);
    return distance < 0 ? -value : value;
}
}
RGB reference_pixel(std::span<const std::uint32_t> source, float x, float y,
                    const Uniforms& u) {
    if (source.size() != 256U * 224U) throw std::invalid_argument("Expected a 256x224 frame");
    const auto v = u.viewport;
    if (v.width <= 0 || v.height <= 0 || x < v.x || y < v.y ||
        x >= v.x + v.width || y >= v.y + v.height) return {};
    const float sx = (x - v.x) * 256.0F / v.width;
    const float sy = (y - v.y) * 224.0F / v.height;
    const int ix = static_cast<int>(std::floor(sx)), iy = static_cast<int>(std::floor(sy));
    RGB result{};
    for (unsigned c = 0; c < 3; ++c) {
        if (u.mode == 0) {
            result[c] = srgb_to_linear(channel(source, ix, iy, c));
            continue;
        }
        const double f = std::clamp((static_cast<double>(sx - std::floor(sx)) - .5) * 1.3 + .5, 0.0, 1.0);
        const double blend = f*f*(3-2*f);
        const double footprint = 224.0 / static_cast<double>(v.height);
        double beam = 0;
        for (int row = iy - 1; row <= iy + 1; ++row) {
            const double left = std::pow(channel(source, ix, row, c), 2.22);
            const double right = std::pow(channel(source, ix + 1, row, c), 2.22);
            const double signal = left + (right-left)*blend;
            const double distance = static_cast<double>(sy) - (static_cast<double>(row) + .5);
            const double energy = (0.45+0.45*signal)*(0.5+0.15*signal);
            beam += signal * (.585/energy) * (integral(distance + footprint*.5, signal) -
                              integral(distance - footprint*.5, signal)) / footprint;
        }
        const double base = std::pow(channel(source, ix, iy, c), 2.22);
        const unsigned stripe = static_cast<unsigned>(std::floor(x - v.x)) % 4U;
        const double scan_detail = u.detail * (u.mode == 1 ? .25 : 1.0);
        const double mask_detail = u.detail * (u.mode == 1 ? .22 : 1.0);
        const double mask = (1.0 - mask_detail) + mask_detail * (stripe == c ? 1.0 : 0.0);
        result[c] = static_cast<float>((base + (beam-base)*scan_detail) * mask);
    }
    if (u.mode == 2) {
        const float peak = std::max({result[0], result[1], result[2]});
        const float gain = std::min(u.gain, u.headroom / std::max(peak, 0.00001F));
        for (auto& c : result) c *= gain;
    } else if (u.mode == 1) {
        const float coverage = (1-.415F*.25F*u.detail)*(1-.75F*.22F*u.detail);
        const float peak = std::max({result[0], result[1], result[2]});
        const float gain = 1/(coverage+(1-coverage)*peak);
        for (auto& c : result) c = srgb_to_linear(std::pow(std::clamp(c*gain, 0.0F, 1.0F), 1.0F/2.2F));
    }
    return result;
}
} // namespace presentation
