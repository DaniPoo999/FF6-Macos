#include "builder_icon.hpp"
#include "ff6/io.hpp"
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <type_traits>

namespace ff6 {
namespace {
constexpr auto bitmap_format = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                               kCGBitmapByteOrder32Big;
template <typename T> struct Release {
    void operator()(T value) const { if (value) CFRelease(value); }
};
template <typename T> using Owned = std::unique_ptr<std::remove_pointer_t<T>, Release<T>>;

void fill(CGContextRef context, CGFloat r, CGFloat g, CGFloat b, CGFloat a = 1) {
    CGContextSetRGBFillColor(context, r, g, b, a);
}

void draw_background(CGContextRef context, CGColorSpaceRef colors, std::size_t size) {
    // A source-only design: rounded ink/teal tile, restrained rim and mint glow.
    // Transparent padding lets Finder and the Dock place their own shadows.
    Owned<CGPathRef> shape(CGPathCreateWithRoundedRect(CGRectMake(80, 80, 864, 864),
                                                     190, 190, nullptr));
    CGContextAddPath(context, shape.get());
    CGContextClip(context);
    const CGFloat background[]{0.025, 0.055, 0.11, 1, 0.08, 0.28, 0.29, 1};
    Owned<CGGradientRef> gradient(CGGradientCreateWithColorComponents(colors,
                                                                     background, nullptr, 2));
    CGContextDrawLinearGradient(context, gradient.get(), CGPointMake(512, 80),
                                 CGPointMake(512, 944), 0);
    const CGFloat glow[]{0.43, 0.88, 0.72, 0.32, 0.22, 0.64, 0.57, 0};
    Owned<CGGradientRef> halo(CGGradientCreateWithColorComponents(colors, glow, nullptr, 2));
    CGContextDrawRadialGradient(context, halo.get(), CGPointMake(512, 585), 30,
                                 CGPointMake(512, 585), 430, 0);
    if (size >= 32) {
        CGContextAddPath(context, shape.get());
        CGContextSetRGBStrokeColor(context, 0.66, 0.91, 0.83, 0.32);
        CGContextSetLineWidth(context, 7);
        CGContextStrokePath(context);
        fill(context, 0.015, 0.025, 0.06, 0.45);
        CGContextFillEllipseInRect(context, CGRectMake(298, 182, 428, 70));
    }
}

Owned<CGImageRef> draw_game_icon(const FieldSpriteAssets& sprites, std::size_t size) {
    Owned<CGColorSpaceRef> colors(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    Owned<CGContextRef> bitmap(CGBitmapContextCreate(nullptr, size, size, 8, size * 4,
        colors.get(), bitmap_format));
    if (!bitmap) throw std::runtime_error("Could not allocate application icon");
    auto* context = bitmap.get();
    const auto scale = static_cast<CGFloat>(size) / 1024;
    CGContextScaleCTM(context, scale, scale);
    draw_background(context, colors.get(), size);

    // GameStart selects graphic 0 and palette 2 (Terra). Reuse the extracted
    // standing-pose and tile-address tables, including each piece's flip flags.
    const auto pose = sprites.selection.standing[static_cast<std::size_t>(FieldFacing::down)];
    const auto tiles = sprites.pose(0, pose);
    const auto& palette = sprites.palettes[2];
    // Whole device pixels at ordinary sizes keep the ROM pixel art crisp.
    // The 16px representation is area-filtered from 32px below instead.
    const auto pixel = size <= 64 ? static_cast<CGFloat>(size) / 32
                                 : std::floor(static_cast<CGFloat>(size) * 0.66 / 24);
    const auto left = std::floor((static_cast<CGFloat>(size) - pixel * 16) / 2) / scale;
    const auto top = std::floor(static_cast<CGFloat>(size) * (size <= 64 ? 0.875 : 0.84)) / scale;
    CGContextSetShouldAntialias(context, false);
    for (std::size_t y = 0; y < 24; ++y) {
        const auto flip = y < 16 ? sprites.upper_flip[pose] : sprites.lower_flip[pose];
        for (std::size_t x = 0; x < 16; ++x) {
            const auto source_x = flip ? 15 - x : x;
            const auto index = tiles[(y / 8) * 2 + source_x / 8][(y % 8) * 8 + source_x % 8];
            if (index == 0) continue;
            if (index >= palette.size()) throw std::runtime_error("Invalid application icon palette index");
            const auto rgb = decode_color(palette[index]);
            fill(context, rgb.red / 31.0, rgb.green / 31.0, rgb.blue / 31.0);
            CGContextFillRect(context, CGRectMake(left + static_cast<CGFloat>(x) * pixel / scale,
                top - static_cast<CGFloat>(y + 1) * pixel / scale, pixel / scale, pixel / scale));
        }
    }
    CGContextSetShouldAntialias(context, true);
    if (size >= 128) {
        // Geometric roman numeral, not a bundled typeface or the original logo.
        fill(context, 0.80, 0.90, 0.80);
        const CGPoint v[]{{438, 176}, {454, 176}, {481, 117}, {508, 176},
                          {524, 176}, {488, 99}, {474, 99}};
        CGContextAddLines(context, v, std::size(v));
        CGContextClosePath(context);
        CGContextFillPath(context);
        CGContextFillRect(context, CGRectMake(550, 99, 14, 77));
        CGContextFillRect(context, CGRectMake(537, 99, 40, 5));
        CGContextFillRect(context, CGRectMake(537, 171, 40, 5));
    }
    Owned<CGImageRef> image(CGBitmapContextCreateImage(context));
    if (!image) throw std::runtime_error("Could not render application icon");
    return image;
}

void polygon(CGContextRef context, std::initializer_list<CGPoint> points,
             CGFloat r, CGFloat g, CGFloat b, CGFloat alpha = 1) {
    fill(context, r, g, b, alpha);
    CGContextAddLines(context, points.begin(), points.size());
    CGContextClosePath(context);
    CGContextFillPath(context);
}

Owned<CGImageRef> draw_builder_icon(std::size_t size) {
    Owned<CGColorSpaceRef> colors(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    Owned<CGContextRef> bitmap(CGBitmapContextCreate(nullptr, size, size, 8, size * 4,
        colors.get(), bitmap_format));
    if (!bitmap) throw std::runtime_error("Could not allocate builder icon");
    auto* context = bitmap.get();
    const auto scale = static_cast<CGFloat>(size) / 1024;
    CGContextScaleCTM(context, scale, scale);
    draw_background(context, colors.get(), size);

    // A small fantasy forge: every facet and contour is original geometry.
    // No game artwork, logos, fonts, system-symbol assets or ROM input.
    polygon(context, {{282, 457}, {651, 457}, {651, 423}, {805, 423}, {761, 379},
                      {642, 349}, {612, 278}, {612, 249}, {666, 223}, {666, 186},
                      {300, 186}, {300, 223}, {354, 249}, {354, 278}, {322, 349},
                      {282, 349}}, 0.57, 0.32, 0.14);
    polygon(context, {{282, 457}, {651, 457}, {651, 423}, {805, 423}, {761, 399},
                      {618, 377}, {282, 377}}, 0.94, 0.72, 0.37);
    polygon(context, {{322, 349}, {618, 349}, {580, 272}, {580, 236}, {354, 236},
                      {384, 278}, {366, 327}}, 0.74, 0.48, 0.22);
    polygon(context, {{300, 223}, {666, 223}, {666, 201}, {300, 201}}, 0.92, 0.68, 0.33);
    fill(context, 1.0, 0.87, 0.58);
    CGContextFillRect(context, CGRectMake(282, 447, 369, 10));

    // Tall, six-sided crystal. Facets meet exactly; a bright center spine keeps
    // the silhouette readable when the icon is reduced to a Finder list item.
    polygon(context, {{512, 865}, {674, 731}, {674, 590}, {512, 480},
                      {350, 590}, {350, 731}}, 0.16, 0.56, 0.54);
    polygon(context, {{512, 865}, {465, 714}, {350, 731}}, 0.70, 0.98, 0.85);
    polygon(context, {{512, 865}, {674, 731}, {561, 714}}, 0.36, 0.83, 0.74);
    polygon(context, {{512, 865}, {561, 714}, {465, 714}}, 0.88, 1.0, 0.92);
    polygon(context, {{350, 731}, {465, 714}, {465, 594}, {350, 590}}, 0.24, 0.75, 0.66);
    polygon(context, {{465, 714}, {561, 714}, {561, 594}, {465, 594}}, 0.57, 0.94, 0.78);
    polygon(context, {{561, 714}, {674, 731}, {674, 590}, {561, 594}}, 0.09, 0.43, 0.45);
    polygon(context, {{350, 590}, {465, 594}, {512, 480}}, 0.13, 0.49, 0.48);
    polygon(context, {{465, 594}, {561, 594}, {512, 480}}, 0.39, 0.83, 0.70);
    polygon(context, {{561, 594}, {674, 590}, {512, 480}}, 0.07, 0.33, 0.37);

    if (size >= 128) {
        // A tiny engraved VI echoes the game's icon without reproducing its logo.
        polygon(context, {{425, 322}, {436, 322}, {451, 282}, {466, 322},
                          {477, 322}, {457, 267}, {445, 267}}, 0.20, 0.17, 0.12);
        fill(context, 0.20, 0.17, 0.12);
        CGContextFillRect(context, CGRectMake(494, 267, 10, 55));
        // Two restrained sparks suggest crafting; omit them in small icons.
        for (const auto center : {CGPointMake(733, 685), CGPointMake(293, 557)}) {
            polygon(context, {{center.x, center.y + 26}, {center.x + 6, center.y + 6},
                              {center.x + 26, center.y}, {center.x + 6, center.y - 6},
                              {center.x, center.y - 26}, {center.x - 6, center.y - 6},
                              {center.x - 26, center.y}, {center.x - 6, center.y + 6}},
                    0.98, 0.85, 0.53);
        }
    }
    Owned<CGImageRef> image(CGBitmapContextCreateImage(context));
    if (!image) throw std::runtime_error("Could not render builder icon");
    return image;
}

template <typename Draw> Bytes make_icon(Draw draw) {
    @autoreleasepool {
        // ImageIO writes Apple's icon container, including Retina scale metadata.
        // No iconutil, Xcode, temporary iconset or external image tool is needed.
        struct Representation { std::size_t points, density; };
        constexpr Representation sizes[]{{16, 1}, {16, 2}, {32, 1}, {32, 2}, {128, 1},
                                          {128, 2}, {256, 1}, {256, 2}, {512, 1}, {512, 2}};
        NSMutableData* data = [NSMutableData data];
        Owned<CGImageDestinationRef> destination(CGImageDestinationCreateWithData(
            (__bridge CFMutableDataRef)data, CFSTR("com.apple.icns"), std::size(sizes), nullptr));
        if (!destination) throw std::runtime_error("macOS could not create an ICNS encoder");
        for (const auto [points, density] : sizes) {
            const auto size = points * density;
            auto image = draw(std::max<std::size_t>(32, size));
            if (size == 16) {
                Owned<CGContextRef> small(CGBitmapContextCreate(nullptr, 16, 16, 8, 64,
                    CGImageGetColorSpace(image.get()), bitmap_format));
                if (!small) throw std::runtime_error("Could not allocate small application icon");
                CGContextSetInterpolationQuality(small.get(), kCGInterpolationHigh);
                CGContextDrawImage(small.get(), CGRectMake(0, 0, 16, 16), image.get());
                image.reset(CGBitmapContextCreateImage(small.get()));
            }
            NSDictionary* properties = @{
                (__bridge NSString*)kCGImagePropertyDPIWidth: @(72 * density),
                (__bridge NSString*)kCGImagePropertyDPIHeight: @(72 * density)};
            CGImageDestinationAddImage(destination.get(), image.get(), (__bridge CFDictionaryRef)properties);
        }
        if (!CGImageDestinationFinalize(destination.get()))
            throw std::runtime_error("macOS could not finish the application icon");
        const auto* first = static_cast<const std::uint8_t*>(data.bytes);
        return Bytes(first, first + data.length);
    }
}
} // namespace

Bytes make_game_icon(const FieldSpriteAssets& sprites) {
    return make_icon([&](std::size_t size) { return draw_game_icon(sprites, size); });
}

Bytes make_builder_icon() {
    return make_icon(draw_builder_icon);
}

Bytes make_builder_icon(ByteView artwork) {
    @autoreleasepool {
        NSData* data = [NSData dataWithBytes:artwork.data() length:artwork.size()];
        Owned<CGImageSourceRef> source(CGImageSourceCreateWithData((__bridge CFDataRef)data, nullptr));
        if (!source || CGImageSourceGetCount(source.get()) != 1 ||
            !CFEqual(CGImageSourceGetType(source.get()), CFSTR("public.png")))
            throw std::runtime_error("Builder artwork must be a single PNG image");
        NSDictionary* properties = CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(source.get(), 0, nullptr));
        const auto width = [properties[(__bridge NSString*)kCGImagePropertyPixelWidth] unsignedLongLongValue];
        const auto height = [properties[(__bridge NSString*)kCGImagePropertyPixelHeight] unsignedLongLongValue];
        if (width != height || width < 128 || width > 4096)
            throw std::runtime_error("Builder artwork must be square and 128–4096 pixels wide");
        Owned<CGImageRef> master(CGImageSourceCreateImageAtIndex(source.get(), 0, nullptr));
        if (!master) throw std::runtime_error("Could not decode builder artwork");
        return make_icon([&](std::size_t size) {
            Owned<CGColorSpaceRef> colors(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
            Owned<CGContextRef> bitmap(CGBitmapContextCreate(nullptr, size, size, 8, size * 4,
                colors.get(), bitmap_format));
            if (!bitmap) throw std::runtime_error("Could not allocate builder artwork icon");
            CGContextSetInterpolationQuality(bitmap.get(), kCGInterpolationHigh);
            CGContextDrawImage(bitmap.get(), CGRectMake(0, 0, static_cast<CGFloat>(size),
                                                        static_cast<CGFloat>(size)), master.get());
            Owned<CGImageRef> image(CGBitmapContextCreateImage(bitmap.get()));
            if (!image) throw std::runtime_error("Could not resize builder artwork");
            return image;
        });
    }
}

void install_game_icon(const ContentPack& pack, const std::filesystem::path& app) {
    const auto icon = make_game_icon(load_field_sprites(pack));
    write_new_file(app / "Contents/Resources/GameIcon.icns", icon);
    const auto path = app / "Contents/Info.plist";
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    NSError* error = nil;
    NSMutableDictionary* info = [[NSDictionary dictionaryWithContentsOfURL:url error:&error] mutableCopy];
    if (!info) throw std::runtime_error("Could not read game icon bundle metadata");
    info[@"CFBundleIconFile"] = @"GameIcon.icns";
    if (![info writeToURL:url error:&error])
        throw std::runtime_error(error.localizedDescription.UTF8String);
}
} // namespace ff6
