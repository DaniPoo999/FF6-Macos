#include "game_presenter.hpp"
#include "metal_pipeline.hpp"
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <algorithm>
#include <condition_variable>
#include <cmath>
#include <mutex>
#include <thread>

namespace presentation {
namespace {
struct RenderSettings {
    Settings profile;
    Mode requested{}, effective{};
    float current{1}, gain{1};
    bool active{}, edr{};
    int width{}, height{};
    bool operator==(const RenderSettings&) const = default;
};
struct Packet {
    std::shared_ptr<const std::vector<std::uint32_t>> pixels;
    std::uint64_t generation{};
    RenderSettings settings;
};
struct Shared {
    std::mutex mutex;
    std::condition_variable wake;
    Packet pending;
    bool ready{}, stop{};
    std::array<bool,2> busy{};
    std::uint64_t presented{}, coalesced{};
    std::array<double,2048> timings{};
    std::size_t timing_count{}, timing_next{};
    double gpu_p95_ms{};
    std::string error;
};
class MetalBackend final : public Backend {
    SDL_Window* window_;
    MetalPipeline pipeline_;
    SDL_MetalView view_{};
    CAMetalLayer* layer_;
    NSTextField* label_;
    std::shared_ptr<Shared> shared_ = std::make_shared<Shared>();
    std::thread worker_;
    Status status_;
    RenderSettings previous_;
    std::shared_ptr<const std::vector<std::uint32_t>> pixels_;
    std::uint64_t generation_{}, time_{};
    float gain_{1};
    std::string overlay_;

    void render() {
        try {
            @autoreleasepool {
                std::array<MetalFrame,2> frames{pipeline_.make_frame(),pipeline_.make_frame()};
                for (;;) {
                    Packet packet;
                    std::size_t slot = 0;
                    {
                        std::unique_lock lock(shared_->mutex);
                        shared_->wake.wait(lock,[&] {
                            return shared_->stop || (shared_->ready &&
                                (!shared_->busy[0] || !shared_->busy[1]));
                        });
                        if (shared_->stop) return;
                        packet = shared_->pending;
                        shared_->ready = false;
                        if (!packet.settings.active) continue;
                        slot = shared_->busy[0] ? 1 : 0;
                        shared_->busy[slot] = true;
                    }
                    @autoreleasepool {
                        // No AppKit work or game references on this thread. Drawable
                        // acquisition may block; only this presentation worker waits.
                        id<CAMetalDrawable> drawable = [layer_ nextDrawable];
                        if (!drawable) {
                            std::lock_guard lock(shared_->mutex);
                            shared_->busy[slot] = false;
                            // A timeout during a resize/minimize is not device loss.
                            if (!shared_->ready) {
                                shared_->pending = packet;
                                shared_->ready = true;
                            }
                            continue;
                        }
                        {
                            std::lock_guard lock(shared_->mutex);
                            // A resize/mode change can arrive while nextDrawable waits.
                            if (shared_->ready) {
                                packet = shared_->pending;
                                shared_->ready = false;
                                ++shared_->coalesced;
                            }
                            if (shared_->stop) return;
                            if (!packet.settings.active) {
                                shared_->busy[slot] = false;
                                continue;
                            }
                        }
                        auto& frame = frames[slot];
                        pipeline_.upload(frame,*packet.pixels,packet.generation);
                        const auto viewport = fit(static_cast<int>(drawable.texture.width),
                                                   static_cast<int>(drawable.texture.height),packet.settings.profile.aspect_4_3);
                        const float scale = viewport.height/224.0F;
                        const float gain = std::min(packet.settings.gain,target_gain(scale,packet.settings.current,packet.settings.profile));
                        const auto parameters = uniforms(viewport,packet.settings.effective,
                            packet.settings.current,gain,packet.settings.profile);
                        id<MTLCommandBuffer> commands = [pipeline_.queue commandBuffer];
                        if (!commands) throw std::runtime_error("Cannot create presentation commands");
                        pipeline_.encode(commands,frame,drawable.texture,parameters);
                        [commands presentDrawable:drawable];
                        auto shared = shared_;
                        [commands addCompletedHandler:^(id<MTLCommandBuffer> completed) {
                            std::lock_guard lock(shared->mutex);
                            shared->busy[slot] = false;
                            if (completed.status == MTLCommandBufferStatusError) {
                                shared->error = completed.error ? completed.error.localizedDescription.UTF8String : "Metal submission failed";
                                shared->stop = true;
                            } else {
                                ++shared->presented;
                                const double milliseconds = (completed.GPUEndTime-completed.GPUStartTime)*1000;
                                if (milliseconds > 0) {
                                    shared->timings[shared->timing_next] = milliseconds;
                                    shared->timing_next = (shared->timing_next+1)%shared->timings.size();
                                    shared->timing_count = std::min(shared->timing_count+1,shared->timings.size());
                                    if (shared->presented % 32 == 0 || shared->presented == 1) {
                                        auto times = shared->timings;
                                        const auto index = (shared->timing_count-1)*95/100;
                                        std::nth_element(times.begin(),times.begin()+static_cast<std::ptrdiff_t>(index),
                                            times.begin()+static_cast<std::ptrdiff_t>(shared->timing_count));
                                        shared->gpu_p95_ms = times[index];
                                    }
                                }
                            }
                            shared->wake.notify_one();
                        }];
                        [commands commit];
                    }
                }
            }
        } catch (const std::exception& e) {
            std::lock_guard lock(shared_->mutex);
            shared_->error = e.what();
            shared_->stop = true;
        }
    }
  public:
    MetalBackend(SDL_Window* window, const std::filesystem::path& library)
        : window_(window), pipeline_(library) {
        @autoreleasepool {
            view_ = SDL_Metal_CreateView(window);
            if (!view_) throw std::runtime_error(SDL_GetError());
            layer_ = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(view_);
            layer_.device = pipeline_.device;
            // One linear format avoids rebuilding the swapchain during mode changes.
            layer_.pixelFormat = MTLPixelFormatRGBA16Float;
            layer_.framebufferOnly = YES;
            layer_.maximumDrawableCount = 2;
            layer_.displaySyncEnabled = YES;
            layer_.allowsNextDrawableTimeout = YES;
            layer_.wantsExtendedDynamicRangeContent = NO;
            CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
            layer_.colorspace = space;
            CGColorSpaceRelease(space);
            layer_.EDRMetadata = nil; // Application maps to the current headroom.
            label_ = [NSTextField wrappingLabelWithString:@""];
            label_.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
            label_.textColor = [NSColor whiteColor];
            label_.backgroundColor = [NSColor colorWithWhite:0 alpha:0.85];
            label_.drawsBackground = YES;
            label_.hidden = YES;
            [(__bridge NSView*)view_ addSubview:label_];
            status_.metal = true;
            try { worker_ = std::thread([this] { render(); }); }
            catch (...) {
                [label_ removeFromSuperview];
                SDL_Metal_DestroyView(view_);
                view_ = nullptr;
                throw;
            }
        }
    }
    ~MetalBackend() override {
        {
            std::lock_guard lock(shared_->mutex);
            shared_->stop = true;
        }
        shared_->wake.notify_one();
        if (worker_.joinable()) worker_.join();
        @autoreleasepool {
            layer_.wantsExtendedDynamicRangeContent = NO;
            [label_ removeFromSuperview];
            if (view_) SDL_Metal_DestroyView(view_);
        }
    }
    void present(const ff6::FrameBuffer& frame, Settings profile) override {
        const auto requested = profile.mode();
        @autoreleasepool {
            auto* view = (__bridge NSView*)view_;
            NSScreen* screen = view.window.screen;
            status_.requested = requested;
            status_.potential = headroom(static_cast<float>(screen.maximumPotentialExtendedDynamicRangeColorComponentValue));
            status_.current = headroom(static_cast<float>(screen.maximumExtendedDynamicRangeColorComponentValue));
            // SDL's macOS HDR query substitutes potential headroom when current is
            // one. Read NSScreen directly to avoid exceeding the CURRENT limit.
            status_.effective = effective(requested,true,status_.potential,status_.current);
            int width = 0, height = 0;
            SDL_GetWindowSizeInPixels(window_,&width,&height);
            const auto viewport = fit(width,height,profile.aspect_4_3);
            const auto now = SDL_GetTicksNS();
            const float seconds = time_ == 0 ? 0 : static_cast<float>(now-time_)/1e9F;
            time_ = now;
            const float target = status_.effective == Mode::hdr ?
                target_gain(viewport.height/224.0F,status_.current,profile) : 1.0F;
            gain_ = approach_gain(gain_,target,seconds);
            if (std::abs(gain_-target) < 0.0001F) gain_ = target;
            const auto flags = SDL_GetWindowFlags(window_);
            RenderSettings settings{profile,requested,status_.effective,status_.current,gain_,
                              width > 0 && height > 0 && !(flags & (SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN)),
                              requested == Mode::hdr && status_.potential > 1,width,height};
            // Request EDR even before current headroom rises above one.
            if (settings.edr != previous_.edr) layer_.wantsExtendedDynamicRangeContent = settings.edr;
            bool changed = !pixels_ || *pixels_ != frame.rgba;
            if (changed) {
                pixels_ = std::make_shared<const std::vector<std::uint32_t>>(frame.rgba);
                ++generation_;
            }
            // Cache unchanged source textures, not the visible EDR presentation.
            // Once gain settles, deduplicating whole HDR frames leaves Core
            // Animation with an idle drawable even while the game is running.
            // Keep presenting the retained image while HDR is requested on an
            // EDR screen (including warmup); hidden/minimized windows stay idle.
            const bool refresh_hdr = settings.active && settings.edr;
            if (changed || settings != previous_ || refresh_hdr) {
                std::lock_guard lock(shared_->mutex);
                if (shared_->ready) ++shared_->coalesced;
                shared_->pending = {pixels_,generation_,settings};
                shared_->ready = true;
                shared_->wake.notify_one();
            }
            previous_ = settings;
            label_.frame = NSMakeRect(12,std::max(0.0,view.bounds.size.height-68),
                                     std::max(0.0,view.bounds.size.width-24),56);
        }
    }
    void overlay(const std::string& text) override {
        if (text == overlay_) return;
        overlay_ = text;
        @autoreleasepool {
            label_.stringValue = [NSString stringWithUTF8String:text.c_str()];
            label_.hidden = text.empty();
        }
    }
    Status status() const override {
        auto result = status_;
        std::lock_guard lock(shared_->mutex);
        result.presented = shared_->presented;
        result.coalesced = shared_->coalesced;
        result.error = shared_->error;
        result.gpu_p95_ms = shared_->gpu_p95_ms;
        return result;
    }
};
} // namespace
std::unique_ptr<Backend> make_metal_backend(SDL_Window* window, const std::filesystem::path& library) {
    return std::make_unique<MetalBackend>(window,library);
}
} // namespace presentation
