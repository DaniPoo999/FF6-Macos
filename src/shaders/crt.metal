// Independently structured native CRT equations; see docs/CRT.md and THIRD_PARTY.md.
#include <metal_stdlib>
using namespace metal;
struct Vertex { float4 position [[position]]; };
struct Uniforms { float4 viewport; uint mode; float gain; float headroom; float detail; };
constant uint crt_mode [[function_constant(0)]];
constant bool full_detail [[function_constant(1)]];
vertex Vertex fullscreen(uint id [[vertex_id]]) {
    const float2 positions[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    return {float4(positions[id], 0, 1)};
}
float3 decode_srgb(float3 c) {
    return select(pow((c + .055f) / 1.055f, float3(2.4f)), c / 12.92f, c <= .04045f);
}
fragment half4 prepare_source(Vertex in [[stage_in]], texture2d<float> source [[texture(0)]]) {
    // Rec.709/sRGB primaries already use D65. The neutral default needs no gamut
    // conversion or approximate temperature tint. This is signal gamma, not sRGB decoding.
    return half4(half3(pow(source.read(uint2(in.position.xy)).rgb, float3(2.22f))), 1);
}
float3 read_black(texture2d<half> source, int2 p) {
    if (any(p < int2(0)) || p.x >= 256 || p.y >= 224) return 0;
    return float3(source.read(uint2(p)).rgb);
}
float3 beam_integral(float distance, float3 signal) {
    const float3 radius = (.45f + .45f * signal) * .5f;
    const float3 t = min(abs(distance) / radius, 1.0f);
    const float3 value = radius * (t + (-1.0f + .6f*signal)*t*t*t +
                                   (.5f - .45f*signal)*t*t*t*t);
    return distance < 0 ? -value : value;
}
float beam_integral(float distance, float signal) {
    const float radius = (.45f + .45f * signal) * .5f;
    const float t = min(abs(distance) / radius, 1.0f);
    const float value = radius * (t + (-1.0f + .6f*signal)*t*t*t +
                                  (.5f - .45f*signal)*t*t*t*t);
    return distance < 0 ? -value : value;
}
float3 beam_compensation(float3 signal) {
    // Equalize integrated beam energy at every input level. Narrow dark beams
    // otherwise lose considerably more light than the white-field calibration.
    return .585f / ((.45f + .45f*signal) * (.5f + .15f*signal));
}
float beam_compensation(float signal) {
    return .585f / ((.45f + .45f*signal) * (.5f + .15f*signal));
}
fragment half4 present_frame(Vertex in [[stage_in]], constant Uniforms& u [[buffer(0)]],
                            texture2d<float> raw [[texture(0)]],
                            texture2d<half> linear [[texture(1)]]) {
    const float2 local = in.position.xy - u.viewport.xy;
    if (any(local < 0) || any(local >= u.viewport.zw)) return half4(0,0,0,1);
    const float2 source = local * float2(256,224) / u.viewport.zw;
    const int2 pixel = int2(floor(source));
    if (crt_mode == 0) return half4(half3(decode_srgb(raw.read(uint2(pixel)).rgb)), 1);
    const float offset = clamp((fract(source.x)-.5f)*1.3f+.5f, 0.0f, 1.0f);
    const float interpolation = offset*offset*(3.0f-2.0f*offset);
    const float footprint = 224.0f/u.viewport.w;
    const uint stripe = uint(floor(local.x)) % 4;
    if (full_detail && crt_mode == 2) {
        // At >=3x scale the far neighboring row cannot intersect this pixel.
        // An opaque grille column emits one channel, or no light at all.
        if (stripe == 3) return half4(0,0,0,1);
        float light = 0;
        const int neighbor = source.y - (float(pixel.y)+.5f) < 0 ? -1 : 1;
        for (int sample = 0; sample < 2; ++sample) {
            const int row = sample == 0 ? 0 : neighbor;
            const float distance = source.y - (float(pixel.y+row)+.5f);
            if (abs(distance) >= .45f+footprint*.5f) continue;
            const float a = read_black(linear,pixel+int2(0,row))[stripe];
            const float b = read_black(linear,pixel+int2(1,row))[stripe];
            const float signal = mix(a,b,interpolation);
            light += signal*beam_compensation(signal)*(beam_integral(distance+footprint*.5f,signal)-
                             beam_integral(distance-footprint*.5f,signal))/footprint;
        }
        light *= min(u.gain,u.headroom/max(light,.00001f));
        return half4(half3(float3(stripe==0,stripe==1,stripe==2)*light),1);
    }
    float3 light = 0;
    const int neighbor = source.y - (float(pixel.y)+.5f) < 0 ? -1 : 1;
    for (int sample = 0; sample < (full_detail ? 2 : 3); ++sample) {
        const int row = full_detail ? (sample == 0 ? 0 : neighbor) : sample-1;
        const float distance = source.y - (float(pixel.y+row)+.5f);
        // A beam's largest radius is .45 source pixels. Most neighboring rows
        // cannot intersect this output pixel, regardless of their RGB values.
        if (abs(distance) >= .45f+footprint*.5f) continue;
        const float3 a = read_black(linear, pixel + int2(0,row));
        const float3 b = read_black(linear, pixel + int2(1,row));
        const float3 signal = mix(a,b,interpolation);
        light += signal * beam_compensation(signal) * (beam_integral(distance + footprint*.5f,signal) -
                           beam_integral(distance - footprint*.5f,signal)) / footprint;
    }
    // SDR has no extra luminance to spend on opaque phosphor gaps. Keep the
    // geometry but soften its modulation; HDR retains the full grille.
    const float scan_detail = u.detail * (crt_mode == 1 ? .25f : 1.0f);
    const float mask_detail = u.detail * (crt_mode == 1 ? .22f : 1.0f);
    light = mix(read_black(linear,pixel),light,scan_detail);
    const float3 mask = float3(stripe == 0, stripe == 1, stripe == 2);
    light *= mix(float3(1),mask,mask_detail);
    if (crt_mode == 2) {
        const float peak = max(light.r,max(light.g,light.b));
        light *= min(u.gain,u.headroom/max(peak,.00001f));
    } else {
        const float coverage = (1.0f-.415f*scan_detail)*(1.0f-.75f*mask_detail);
        const float peak = max(light.r,max(light.g,light.b));
        // A common RGB gain restores midtones with a smooth SDR shoulder.
        // It preserves black and hue, and never clips channels independently.
        light /= coverage + (1.0f-coverage)*peak;
        // Match the preset's gamma-2.2 SDR signal, then deliver LINEAR sRGB to
        // ColorSync. This is not an Adobe RGB gamut and must not be encoded twice.
        light = decode_srgb(pow(clamp(light,0.0f,1.0f),float3(1.0f/2.2f)));
    }
    return half4(half3(light),1);
}
