#pragma once
#import <Metal/Metal.h>
#include "presentation.hpp"
#include <filesystem>
#include <span>

// Objective-C++ only. The game core never includes this header.
namespace presentation {
struct MetalFrame {
    id<MTLTexture> raw, linear, blur, diffuse;
    std::uint64_t generation{};
    bool prepared{}, diffused{};
};
class MetalPipeline {
  public:
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    explicit MetalPipeline(const std::filesystem::path& library);
    MetalFrame make_frame() const;
    void upload(MetalFrame& frame, std::span<const std::uint32_t> pixels,
                std::uint64_t generation) const;
    void encode(id<MTLCommandBuffer> commands, MetalFrame& frame,
                id<MTLTexture> output, const Uniforms& uniforms) const;
  private:
    id<MTLRenderPipelineState> prepare_, diffuse_;
    std::array<std::array<id<MTLRenderPipelineState>,2>,3> present_;
};
} // namespace presentation
