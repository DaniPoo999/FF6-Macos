// Native procedural CRT. Equations and research provenance: docs/CRT.md.
#include <metal_stdlib>
using namespace metal;
struct Vertex { float4 position [[position]]; };
struct Uniforms {
    float4 viewport; uint mode; float gain; float headroom; float detail;
    float curvature; float sharpness; float scanlines; float phosphor_size;
    float phosphor_strength; float glow; float brightness; float reserved;
};
constant uint crt_mode [[function_constant(0)]];
constant bool full_detail [[function_constant(1)]];
vertex Vertex fullscreen(uint id [[vertex_id]]) {
    const float2 positions[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    return {float4(positions[id],0,1)};
}
float3 decode_srgb(float3 c) {
    return select(pow((c+.055f)/1.055f,float3(2.4f)),c/12.92f,c<=.04045f);
}
fragment half4 prepare_source(Vertex in [[stage_in]], texture2d<float> source [[texture(0)]]) {
    return half4(half3(pow(source.read(uint2(in.position.xy)).rgb,float3(2.22f))),1);
}
float3 read_edge(texture2d<half> source, int2 p) {
    return float3(source.read(uint2(clamp(p,int2(0),int2(255,223)))).rgb);
}
fragment half4 diffuse_source(Vertex in [[stage_in]], texture2d<half> source [[texture(0)]],
                              constant int2& direction [[buffer(0)]]) {
    const int2 p=int2(in.position.xy);
    const float3 light=(read_edge(source,p-direction*2)+read_edge(source,p+direction*2)+
                       4*(read_edge(source,p-direction)+read_edge(source,p+direction))+6*read_edge(source,p))/16;
    return half4(half3(light),1);
}
struct SurfaceSample { float2 position; float2 footprint; };
SurfaceSample surface(float2 p, float2 viewport, float angle) {
    if (angle<.0001f) return {(p+1)*float2(128,112),float2(256,224)/viewport};
    const float inverse_radius=sin(angle), eye=2, aspect=viewport.y/viewport.x;
    // A rectangular image lies on a convex sphere, with its center closest to
    // the viewer. Fit the short-axis extremum using one scale: no overscan and
    // no independent axis stretching. Aspect is the physical viewport's ratio.
    const float sag=2*pow(sin(angle*aspect*.5f),2.0f)/inverse_radius;
    const float edge=eye*sin(angle*aspect)/(inverse_radius*aspect*(eye+sag));
    const float2 q=p*float2(1,aspect)*edge;
    const float squared=dot(q,q), distance=sqrt(squared);
    // Rationalized near intersection of a view ray with the sphere. This form
    // avoids cancellation as curvature approaches zero.
    const float k=inverse_radius*inverse_radius+2*inverse_radius/eye;
    const float root=sqrt(max(1-squared*k,1e-6f));
    const float denominator=1+eye*inverse_radius+root;
    const float ray=(2+eye*inverse_radius)/denominator;
    const float sine=clamp(distance*ray*inverse_radius,0.0f,.999999f);
    const float factor=distance<1e-5f ? inverse_radius/angle : asin(sine)/(angle*distance);
    const float radial=ray*inverse_radius*(1+squared*k/(root*denominator))/
                       (angle*sqrt(1-sine*sine));
    const float2 direction=distance<1e-5f ? float2(0) : q/distance;
    const float delta=radial-factor;
    // Full Jacobian: both source coordinates vary along both output axes.
    // Include cross derivatives when filtering curved beams and phosphors.
    const float cross=delta*direction.x*direction.y;
    const float2 diagonal=factor+delta*direction*direction;
    const float2 footprint=edge*float2(256,224)*
        float2((abs(diagonal.x)+abs(cross))/viewport.x,
               (abs(diagonal.y)+abs(cross))/viewport.y);
    return {(p*(factor*edge)+1)*float2(128,112),footprint};
}
float3 beam_integral(float distance,float3 signal) {
    const float3 radius=(.45f+.45f*signal)*.5f;
    const float3 t=min(abs(distance)/radius,1.0f);
    const float3 value=radius*(t+(-1+.6f*signal)*t*t*t+(.5f-.45f*signal)*t*t*t*t);
    return distance<0 ? -value : value;
}
float3 pulse_integral(float3 p) { return floor(p)*.25f+clamp(fract(p),0.0f,.25f); }
fragment half4 present_frame(Vertex in [[stage_in]], constant Uniforms& u [[buffer(0)]],
                            texture2d<float> raw [[texture(0)]], texture2d<half> linear [[texture(1)]],
                            texture2d<half> diffuse [[texture(2)]]) {
    const float2 local=in.position.xy-u.viewport.xy;
    if (any(local<0) || any(local>=u.viewport.zw)) return half4(0,0,0,1);
    const float2 p=local*2/u.viewport.zw-1;
    if (crt_mode==0) return half4(half3(decode_srgb(raw.read(uint2(local*float2(256,224)/u.viewport.zw)).rgb)),1);
    const auto projected=surface(p,u.viewport.zw,u.curvature);
    const float2 source=projected.position;
    const float2 footprint=max(projected.footprint,1e-5f);
    const float2 edge=clamp(min(source,float2(256,224)-source)/footprint+.5f,0.0f,1.0f);
    const float border=edge.x*edge.y;
    if (border==0) return half4(0,0,0,1);
    const int2 pixel=int2(floor(source-float2(.5f,0)));
    const float sigma=.65f-.4f*u.sharpness;
    const float4 distances=float(pixel.x)+float4(-.5f,.5f,1.5f,2.5f)-source.x;
    float4 weights=exp(-.5f*distances*distances/(sigma*sigma));
    weights/=dot(weights,float4(1));
    float3 beam=0, base=0;
    for (int row=-1;row<=1;++row) {
        const float distance=source.y-(float(pixel.y+row)+.5f);
        if (row!=0 && abs(distance)>=.45f+footprint.y*.5f) continue;
        float3 signal=0;
        for (int tap=0;tap<4;++tap) signal+=read_edge(linear,pixel+int2(tap-1,row))*weights[tap];
        if (row==0) base=signal;
        beam+=signal*.585f/((.45f+.45f*signal)*(.5f+.15f*signal))*
            (beam_integral(distance+footprint.y*.5f,signal)-beam_integral(distance-footprint.y*.5f,signal))/footprint.y;
    }
    const float scan=u.detail*u.scanlines*(crt_mode==1 ? .25f : 1.0f);
    const float triads=480/u.phosphor_size, width=footprint.x*triads/256;
    // Integrate each phosphor over this pixel's footprint; fade unresolved color
    // stripes towards their mean rather than letting them alias into broad bands.
    const float mask=u.detail*u.phosphor_strength*(crt_mode==1 ? .22f : 1.0f)*smoothstep(2.0f,4.0f,1/width);
    const float3 phase=source.x*triads/256-float3(.5f,1.5f,2.5f)/3+.125f;
    const float3 grille=(pulse_integral(phase+width*.5f)-pulse_integral(phase-width*.5f))/width;
    const float coverage=(1-.415f*scan)*(1-.75f*mask);
    float3 light=mix(base,beam,scan)*mix(float3(1),grille,mask);
    constexpr sampler bilinear(coord::normalized,address::clamp_to_edge,filter::linear);
    light=mix(light,float3(diffuse.sample(bilinear,source/float2(256,224)).rgb)*coverage,u.glow);
    if (crt_mode==2) {
        const float peak=max(light.r,max(light.g,light.b));
        light*=min(u.brightness/coverage,u.headroom/max(peak,.00001f));
    } else {
        const float peak=max(light.r,max(light.g,light.b));
        light*=u.brightness/(coverage+(1-coverage)*peak*u.brightness);
        light=decode_srgb(pow(clamp(light,0.0f,1.0f),float3(1/2.2f)));
    }
    return half4(half3(light*border),1);
}
