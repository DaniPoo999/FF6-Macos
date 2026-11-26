#include "metal_pipeline.hpp"
#include <stdexcept>

namespace presentation {
namespace {
id<MTLRenderPipelineState> pipeline(id<MTLDevice> device, id<MTLLibrary> library,
                                  NSString* fragment, int mode = -1, bool full = false) {
    MTLRenderPipelineDescriptor* description = [MTLRenderPipelineDescriptor new];
    description.vertexFunction = [library newFunctionWithName:@"fullscreen"];
    NSError* error = nil;
    if (mode < 0) description.fragmentFunction = [library newFunctionWithName:fragment];
    else {
        auto* values = [MTLFunctionConstantValues new];
        auto selected = static_cast<std::uint32_t>(mode);
        [values setConstantValue:&selected type:MTLDataTypeUInt atIndex:0];
        [values setConstantValue:&full type:MTLDataTypeBool atIndex:1];
        description.fragmentFunction = [library newFunctionWithName:fragment constantValues:values error:&error];
    }
    description.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
    if (!description.vertexFunction || !description.fragmentFunction)
        throw std::runtime_error("CRT shader library is missing a required function");
    id<MTLRenderPipelineState> result = [device newRenderPipelineStateWithDescriptor:description error:&error];
    if (!result) throw std::runtime_error(error ? error.localizedDescription.UTF8String : "Metal pipeline failed");
    return result;
}
id<MTLRenderCommandEncoder> encoder(id<MTLCommandBuffer> commands, id<MTLTexture> target) {
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> result = [commands renderCommandEncoderWithDescriptor:pass];
    if (!result) throw std::runtime_error("Cannot encode CRT render pass");
    return result;
}
}
MetalPipeline::MetalPipeline(const std::filesystem::path& path) {
    device = MTLCreateSystemDefaultDevice();
    if (!device) throw std::runtime_error("Metal is unavailable");
    queue = [device newCommandQueue];
    if (!queue) throw std::runtime_error("Cannot create Metal command queue");
    NSError* error = nil;
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    id<MTLLibrary> library = [device newLibraryWithURL:url error:&error];
    if (!library) throw std::runtime_error(error ? error.localizedDescription.UTF8String : "Cannot load CRT shaders");
    prepare_ = pipeline(device, library, @"prepare_source");
    diffuse_ = pipeline(device, library, @"diffuse_source");
    for (unsigned mode=0;mode<3;++mode)
        for (unsigned full=0;full<2;++full)
            present_[mode][full] = pipeline(device,library,@"present_frame",static_cast<int>(mode),full!=0);
}
MetalFrame MetalPipeline::make_frame() const {
    MTLTextureDescriptor* raw = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                width:256 height:224 mipmapped:NO];
    raw.storageMode = MTLStorageModeShared;
    raw.usage = MTLTextureUsageShaderRead;
    MTLTextureDescriptor* linear = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                   width:256 height:224 mipmapped:NO];
    linear.storageMode = MTLStorageModePrivate;
    linear.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    MetalFrame result{[device newTextureWithDescriptor:raw], [device newTextureWithDescriptor:linear],
                      [device newTextureWithDescriptor:linear], [device newTextureWithDescriptor:linear]};
    if (!result.raw || !result.linear || !result.blur || !result.diffuse) throw std::runtime_error("Cannot allocate CRT source textures");
    return result;
}
void MetalPipeline::upload(MetalFrame& frame, std::span<const std::uint32_t> pixels,
                           std::uint64_t generation) const {
    if (pixels.size() != 256U*224U) throw std::invalid_argument("Expected a 256x224 frame");
    if (frame.generation == generation) return;
    const auto bytes = unpack(pixels);
    [frame.raw replaceRegion:MTLRegionMake2D(0,0,256,224) mipmapLevel:0
                  withBytes:bytes.data() bytesPerRow:256*4];
    frame.generation = generation;
    frame.prepared = false;
    frame.diffused = false;
}
void MetalPipeline::encode(id<MTLCommandBuffer> commands, MetalFrame& frame,
                           id<MTLTexture> output, const Uniforms& uniforms) const {
    if (uniforms.mode != 0 && !frame.prepared) {
        auto prepare = encoder(commands, frame.linear);
        [prepare setRenderPipelineState:prepare_];
        [prepare setFragmentTexture:frame.raw atIndex:0];
        [prepare drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [prepare endEncoding];
        frame.prepared = true;
    }
    if (uniforms.mode != 0 && !frame.diffused) {
        for (unsigned axis=0;axis<2;++axis) {
            auto blur = encoder(commands,axis==0 ? frame.blur : frame.diffuse);
            const std::array<int,2> direction{axis==0 ? 1 : 0,axis==1 ? 1 : 0};
            [blur setRenderPipelineState:diffuse_];
            [blur setFragmentTexture:axis==0 ? frame.linear : frame.blur atIndex:0];
            [blur setFragmentBytes:direction.data() length:sizeof(direction) atIndex:0];
            [blur drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [blur endEncoding];
        }
        frame.diffused = true;
    }
    auto present = encoder(commands, output);
    [present setRenderPipelineState:present_.at(uniforms.mode)[uniforms.detail==1 ? 1 : 0]];
    [present setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [present setFragmentTexture:frame.raw atIndex:0];
    [present setFragmentTexture:frame.linear atIndex:1];
    [present setFragmentTexture:frame.diffuse atIndex:2];
    [present drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [present endEncoding];
}
} // namespace presentation
