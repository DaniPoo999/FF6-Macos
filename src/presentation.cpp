#include "presentation.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include <limits>
#include <locale>

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
Settings sanitized(Settings s) {
    if (s.shader != Mode::sdr && s.shader != Mode::hdr)
        throw std::invalid_argument("Expected an SDR or HDR shader");
    const auto bounded = [](float v, float lo, float hi, float fallback) {
        return std::isfinite(v) ? std::clamp(v,lo,hi) : fallback;
    };
    s.curvature_strength = bounded(s.curvature_strength,0,1,Settings{}.curvature_strength);
    s.sharpness = bounded(s.sharpness,0,1,.6F);
    s.scanlines = bounded(s.scanlines,0,1,.85F);
    s.phosphor_size = bounded(s.phosphor_size,.5F,2,1);
    s.phosphor_strength = bounded(s.phosphor_strength,0,1,.85F);
    s.glow = bounded(s.glow,0,.15F,.04F);
    s.brightness = bounded(s.brightness,.5F,1.5F,1);
    return s;
}
Settings load_settings(const std::filesystem::path& folder) {
    std::error_code error;
    const auto length = std::filesystem::file_size(folder/"presentation.cfg",error);
    if (error || length > 512) return {};
    std::ifstream input(folder / "presentation.cfg");
    input.imbue(std::locale::classic());
    std::string header, value, enabled, extra;
    if (!std::getline(input, header) || !std::getline(input, value)) return {};
    try {
        const auto shader = parse(value);
        if (header == "FFVI presentation 1" && !std::getline(input, extra))
            return {shader != Mode::off, shader == Mode::off ? Mode::sdr : shader};
        if (shader == Mode::off || !std::getline(input,enabled) ||
            (enabled != "on" && enabled != "off")) return {};
        Settings s{enabled == "on",shader};
        if (header == "FFVI presentation 2" && !std::getline(input,extra)) return s;
        if (header != "FFVI presentation 3") return {};
        unsigned curved{}, aspect{};
        if (!(input >> curved >> aspect >> s.curvature_strength >> s.sharpness >> s.scanlines >>
              s.phosphor_size >> s.phosphor_strength >> s.glow >> s.brightness) ||
            curved > 1 || aspect > 1 || (input >> extra)) return {};
        s.curvature = curved != 0; s.aspect_4_3 = aspect != 0;
        return sanitized(s);
    } catch (const std::invalid_argument&) {}
    return {};
}
Mode load_preference(const std::filesystem::path& folder) { return load_settings(folder).mode(); }
void save_preference(const std::filesystem::path& folder, Mode mode) {
    auto settings = load_settings(folder);
    settings.enabled = mode != Mode::off;
    if (settings.enabled) settings.shader = mode;
    save_settings(folder, settings);
}
void save_settings(const std::filesystem::path& folder, Settings settings) {
    settings = sanitized(settings);
    std::filesystem::create_directories(folder);
    static std::atomic<unsigned> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary = folder / ("presentation." + std::to_string(stamp) + "." +
                                      std::to_string(sequence++) + ".tmp");
    try {
        std::ofstream out(temporary, std::ios::binary);
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<float>::max_digits10);
        out << "FFVI presentation 3\n" << name(settings.shader) << '\n'
            << (settings.enabled ? "on" : "off") << '\n'
            << settings.curvature << ' ' << settings.aspect_4_3 << '\n'
            << settings.curvature_strength << ' ' << settings.sharpness << ' ' << settings.scanlines << ' '
            << settings.phosphor_size << ' ' << settings.phosphor_strength << ' '
            << settings.glow << ' ' << settings.brightness << '\n';
        out.close();
        std::filesystem::rename(temporary, folder / "presentation.cfg");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
Viewport fit(int width, int height, bool aspect_4_3) {
    if (width <= 0 || height <= 0) return {};
    const double source_width = aspect_4_3 ? 224.0*4.0/3.0 : 256.0;
    const double scale = std::min(static_cast<double>(width) / source_width,
                                  static_cast<double>(height) / 224.0);
    const float w = static_cast<float>(source_width*scale), h = static_cast<float>(224.0*scale);
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
float target_gain(float scale, float current, Settings s) {
    const float d = detail(scale);
    const float coverage = (1-.415F*d*s.scanlines)*(1-.75F*d*s.phosphor_strength);
    return std::min(1/coverage,headroom(current));
}
float hdr_detail(float scale, float gain, Settings s) {
    // Reduce modulation, not the mean luminance, when EDR headroom is limited.
    float lo = 0, hi = detail(scale);
    for (unsigned i=0;i<24;++i) {
        const float d = (lo+hi)*.5F;
        if ((1-.415F*d*s.scanlines)*(1-.75F*d*s.phosphor_strength)*gain < 1) hi=d;
        else lo=d;
    }
    return lo;
}
Uniforms uniforms(Viewport v, Mode mode, float current, float gain, Settings s) {
    s = sanitized(s);
    return {v,static_cast<std::uint32_t>(mode),gain,headroom(current),
            mode == Mode::hdr ? hdr_detail(v.height/224,gain,s) : detail(v.height/224),
            s.curvature ? s.curvature_strength*.4F : 0, s.sharpness,s.scanlines,s.phosphor_size,
            s.phosphor_strength,s.glow,s.brightness,0};
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
namespace {
// Invert a centered spherical projection in physical screen proportions.
// One common scale fits the entire surface without stretching its center.
std::array<double,2> surface(double x, double y, double angle, double aspect) {
    if (angle < .0001) return {x,y};
    const double r=1/std::sin(angle), eye=2;
    const double edge=eye*r*std::sin(angle*aspect)/
                      (aspect*(eye+r*(1-std::cos(angle*aspect))));
    const double distance=std::hypot(x,y*aspect);
    if (distance < 1e-12) return {0,0};
    const double q=distance*edge;
    const double theta=std::asin(std::clamp(q*(eye/r+1)/std::sqrt(eye*eye+q*q),-1.0,1.0))-
                       std::atan(q/eye);
    const double scale=theta/(angle*distance);
    return {x*scale,y*scale};
}
double pulse_integral(double x) {
    return std::floor(x)*.25+std::clamp(x-std::floor(x),0.0,.25);
}
double smooth(double x) { x=std::clamp(x,0.0,1.0); return x*x*(3-2*x); }
}
RGB reference_pixel(std::span<const std::uint32_t> source, float x, float y,
                    const Uniforms& u) {
    if (source.size() != 256U * 224U) throw std::invalid_argument("Expected a 256x224 frame");
    const auto v=u.viewport;
    if (v.width<=0 || v.height<=0 || x<v.x || y<v.y || x>=v.x+v.width || y>=v.y+v.height) return {};
    const double nx=(x-v.x)*2.0/v.width-1, ny=(y-v.y)*2.0/v.height-1;
    const double aspect=v.height/v.width;
    const auto pos=surface(nx,ny,u.mode ? u.curvature : 0,aspect);
    const double sx=(pos[0]+1)*128, sy=(pos[1]+1)*112;
    const int ix=static_cast<int>(std::floor(sx-.5)), iy=static_cast<int>(std::floor(sy));
    RGB result{};
    if (u.mode==0) {
        for (unsigned c=0;c<3;++c) result[c]=srgb_to_linear(channel(source,static_cast<int>(sx),iy,c));
        return result;
    }
    const auto dx=surface(nx+1.0/v.width,ny,u.curvature,aspect), mx=surface(nx-1.0/v.width,ny,u.curvature,aspect);
    const auto dy=surface(nx,ny+1.0/v.height,u.curvature,aspect), my=surface(nx,ny-1.0/v.height,u.curvature,aspect);
    const double fx=std::max((std::abs(dx[0]-mx[0])+std::abs(dy[0]-my[0]))*128,1e-5);
    const double fy=std::max((std::abs(dx[1]-mx[1])+std::abs(dy[1]-my[1]))*112,1e-5);
    const double border=std::clamp(std::min(sx,256-sx)/fx+.5,0.0,1.0)*
                        std::clamp(std::min(sy,224-sy)/fy+.5,0.0,1.0);
    if (border==0) return {};
    const double sigma=.65-.4*u.sharpness;
    double weights[4], sum=0;
    for (int i=0;i<4;++i) { const double d=(ix+i-1+.5)-sx; weights[i]=std::exp(-.5*d*d/(sigma*sigma)); sum+=weights[i]; }
    const double scan=u.detail*u.scanlines*(u.mode==1 ? .25 : 1);
    const double period=480/u.phosphor_size;
    const double footprint=fx*period/256;
    const double mask=u.detail*u.phosphor_strength*(u.mode==1 ? .22 : 1)*smooth((1/footprint-2)/2);
    const double coverage=(1-.415*scan)*(1-.75*mask);
    const auto linear=[&](int px,int py,unsigned c) { return std::pow(channel(source,std::clamp(px,0,255),std::clamp(py,0,223),c),2.22); };
    for (unsigned c=0;c<3;++c) {
        double beam=0, base=0;
        for (int row=iy-1;row<=iy+1;++row) {
            double signal=0;
            for (int i=0;i<4;++i) signal+=linear(ix+i-1,row,c)*weights[i]/sum;
            if (row==iy) base=signal;
            const double distance=sy-(row+.5);
            beam+=signal*.585/((.45+.45*signal)*(.5+.15*signal))*
                 (integral(distance+fy*.5,signal)-integral(distance-fy*.5,signal))/fy;
        }
        // Separable 5-tap binomial diffusion, sampled bilinearly like the GPU.
        const int bx=static_cast<int>(std::floor(sx-.5)), by=static_cast<int>(std::floor(sy-.5));
        const double tx=sx-.5-bx, ty=sy-.5-by;
        const double kernel[5]={1.0/16,4.0/16,6.0/16,4.0/16,1.0/16};
        double blurred=0;
        for (int oy=0;oy<2;++oy) for (int ox=0;ox<2;++ox) {
            // Clamp the bilinear sample before accessing the finite blur texture.
            const int xx=std::clamp(bx+ox,0,255), yy=std::clamp(by+oy,0,223);
            double tap=0;
            for(int j=-2;j<=2;++j) for(int i=-2;i<=2;++i)
                tap+=kernel[i+2]*kernel[j+2]*linear(xx+i,yy+j,c);
            blurred+=tap*(ox ? tx : 1-tx)*(oy ? ty : 1-ty);
        }
        const double phase=sx*period/256-(c+.5)/3+.125;
        const double grille=(pulse_integral(phase+footprint*.5)-pulse_integral(phase-footprint*.5))/footprint;
        const double light=(base+(beam-base)*scan)*(1-mask+mask*grille);
        result[c]=static_cast<float>(light*(1-u.glow)+blurred*coverage*u.glow);
    }
    const float peak=std::max({result[0],result[1],result[2]});
    if (u.mode==2) {
        const float gain=std::min(static_cast<float>(u.brightness/coverage),u.headroom/std::max(peak,.00001F));
        for(auto& c:result) c*=gain*static_cast<float>(border);
    } else {
        const double gain=u.brightness/(coverage+(1-coverage)*peak*u.brightness);
        for(auto& c:result) c=srgb_to_linear(std::pow(std::clamp(static_cast<float>(c*gain),0.0F,1.0F),1/2.2F))*static_cast<float>(border);
    }
    return result;
}
} // namespace presentation
