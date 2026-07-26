#pragma once
// Liquid Glass HLSL Shaders - clean version (no capsule/smoothness)

#define SDF_COMMON_HLSL \
"float radiusAt(float2 coord, float4 radii) {\n" \
"    if (coord.x >= 0.0) {\n" \
"        if (coord.y <= 0.0) return radii.y;\n" \
"        else return radii.z;\n" \
"    } else {\n" \
"        if (coord.y <= 0.0) return radii.x;\n" \
"        else return radii.w;\n" \
"    }\n" \
"}\n" \
"float sdRoundedRect(float2 coord, float2 halfSize, float radius) {\n" \
"    float2 cornerCoord = abs(coord) - (halfSize - float2(radius, radius));\n" \
"    float outside = length(max(cornerCoord, 0.0)) - radius;\n" \
"    float inside = min(max(cornerCoord.x, cornerCoord.y), 0.0);\n" \
"    return outside + inside;\n" \
"}\n" \
"float2 gradSdRoundedRect(float2 coord, float2 halfSize, float radius) {\n" \
"    float2 cornerCoord = abs(coord) - (halfSize - float2(radius, radius));\n" \
"    float sx = coord.x >= 0.0 ? 1.0 : -1.0;\n" \
"    float sy = coord.y >= 0.0 ? 1.0 : -1.0;\n" \
"    if (cornerCoord.x > 0.0 || cornerCoord.y > 0.0) {\n" \
"        float2 c = max(cornerCoord, 0.0);\n" \
"        float len = length(c);\n" \
"        if (len > 0.0)\n" \
"            return float2(sx, sy) * (c / len);\n" \
"    }\n" \
"    float gradX = step(cornerCoord.y, cornerCoord.x);\n" \
"    return float2(sx, sy) * float2(gradX, 1.0 - gradX);\n" \
"}\n" \
"float circleMap(float x) {\n" \
"    return 1.0 - sqrt(1.0 - x * x);\n" \
"}\n"

// Fullscreen triangle VS — 3 vertices from SV_VertexID, no vertex buffer
static const char* FullscreenVS = R"(
struct VSOutput { float4 svpos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOutput main(uint id : SV_VertexID) {
    VSOutput o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.svpos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}
)";

// 15-tap separable Gaussian blur — horizontal pass. Weights precomputed on CPU
static const char* BlurH_PS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer BlurCB : register(b0) { float2 texelSize : packoffset(c0.x); int kernelRadius : packoffset(c0.z); float sigma : packoffset(c0.w); float4 weightPack0 : packoffset(c1); float4 weightPack1 : packoffset(c2); float4 weightPack2 : packoffset(c3); float4 weightPack3 : packoffset(c4); };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 coord = svpos.xy * texelSize;
    float w[16] = { weightPack0.x, weightPack0.y, weightPack0.z, weightPack0.w,
                    weightPack1.x, weightPack1.y, weightPack1.z, weightPack1.w,
                    weightPack2.x, weightPack2.y, weightPack2.z, weightPack2.w,
                    weightPack3.x, weightPack3.y, weightPack3.z, weightPack3.w };
    float4 color = inputTex.SampleLevel(s0, coord, 0) * w[0];
    for (int i = 1; i <= kernelRadius; i++) {
        float2 off = float2(texelSize.x * float(i), 0.0);
        color += inputTex.SampleLevel(s0, coord + off, 0) * w[i];
        color += inputTex.SampleLevel(s0, coord - off, 0) * w[i];
    }
    return color;
}
)";

// 15-tap separable Gaussian blur — vertical pass
static const char* BlurV_PS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer BlurCB : register(b0) { float2 texelSize : packoffset(c0.x); int kernelRadius : packoffset(c0.z); float sigma : packoffset(c0.w); float4 weightPack0 : packoffset(c1); float4 weightPack1 : packoffset(c2); float4 weightPack2 : packoffset(c3); float4 weightPack3 : packoffset(c4); };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 coord = svpos.xy * texelSize;
    float w[16] = { weightPack0.x, weightPack0.y, weightPack0.z, weightPack0.w,
                    weightPack1.x, weightPack1.y, weightPack1.z, weightPack1.w,
                    weightPack2.x, weightPack2.y, weightPack2.z, weightPack2.w,
                    weightPack3.x, weightPack3.y, weightPack3.z, weightPack3.w };
    float4 color = inputTex.SampleLevel(s0, coord, 0) * w[0];
    for (int i = 1; i <= kernelRadius; i++) {
        float2 off = float2(0.0, texelSize.y * float(i));
        color += inputTex.SampleLevel(s0, coord + off, 0) * w[i];
        color += inputTex.SampleLevel(s0, coord - off, 0) * w[i];
    }
    return color;
}
)";

// Glass body with SDF rounded-rect refraction + saturation boost (no dispersion)
static const char* GlassRefractionPS = R"(
Texture2D blurTex : register(t0); SamplerState s0 : register(s0);
)" SDF_COMMON_HLSL R"(
cbuffer GlassCB : register(b0) {
    float2 elementPos      : packoffset(c0.x);
    float2 elementSize     : packoffset(c0.z);
    float4 cornerRadii     : packoffset(c1);
    float2 screenSizeInv   : packoffset(c2.x);
    float refractionHeight : packoffset(c2.z);
    float refractionAmount : packoffset(c2.w);
    float depthEffect      : packoffset(c3.x);
    float saturation       : packoffset(c3.y);
    float dispersion       : packoffset(c3.z);
};
static const float3 lumVec = float3(0.213, 0.715, 0.072);
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 pc = svpos.xy;
    float2 hs = elementSize * 0.5;
    float2 cc = pc - elementPos - hs;
    float r = radiusAt(cc, cornerRadii);
    float sd = sdRoundedRect(cc, hs, r);
    float edgeAA = 1.0 - smoothstep(-1.0, 1.0, sd);
    if (edgeAA < 0.001) discard;
    sd = min(sd, 0.0);
    float t = saturate(1.0 - (-sd / refractionHeight));
    float d = circleMap(t) * refractionAmount;
    float gr = min(r * 1.5, min(hs.x, hs.y));
    float2 ccN = cc / (length(cc) + 1e-6);
    float2 grad = normalize(gradSdRoundedRect(cc, hs, gr) + depthEffect * ccN);
    float4 c = blurTex.SampleLevel(s0, (pc + d * grad) * screenSizeInv, 0);
    float lum = dot(c.rgb, lumVec);
    c.rgb = lerp(float3(lum,lum,lum), c.rgb, saturation);
    c.rgb = saturate(c.rgb * 0.92);
    c.a *= edgeAA;
    return c;
}
)";

// Glass body with chromatic dispersion — 7-sample RGB split from (cc.x*cc.y)/(hs.x*hs.y)
static const char* GlassDispersionPS = R"(
Texture2D blurTex : register(t0); SamplerState s0 : register(s0);
)" SDF_COMMON_HLSL R"(
cbuffer GlassCB : register(b0) {
    float2 elementPos      : packoffset(c0.x);
    float2 elementSize     : packoffset(c0.z);
    float4 cornerRadii     : packoffset(c1);
    float2 screenSizeInv   : packoffset(c2.x);
    float refractionHeight : packoffset(c2.z);
    float refractionAmount : packoffset(c2.w);
    float depthEffect      : packoffset(c3.x);
    float saturation       : packoffset(c3.y);
    float dispersion       : packoffset(c3.z);
};
static const float3 lumVec = float3(0.213, 0.715, 0.072);
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 pc = svpos.xy;
    float2 hs = elementSize * 0.5;
    float2 cc = pc - elementPos - hs;
    float r = radiusAt(cc, cornerRadii);
    float sd = sdRoundedRect(cc, hs, r);
    float edgeAA = 1.0 - smoothstep(-1.0, 1.0, sd);
    if (edgeAA < 0.001) discard;
    sd = min(sd, 0.0);
    float t = saturate(1.0 - (-sd / refractionHeight));
    float d = circleMap(t) * refractionAmount;
    float gr = min(r * 1.5, min(hs.x, hs.y));
    float2 ccN = cc / (length(cc) + 1e-6);
    float2 grad = normalize(gradSdRoundedRect(cc, hs, gr) + depthEffect * ccN);
    float2 rp = pc + d * grad;
    float2 ruv = rp * screenSizeInv;
    float disp = (cc.x * cc.y) / (hs.x * hs.y);
    float2 doff = d * grad * disp * screenSizeInv * dispersion;
    float4 color = float4(0,0,0,0);
    float4 s;
    s = blurTex.SampleLevel(s0, ruv + doff, 0); color.r+=s.r/3.5; color.a+=s.a/7.0;
    s = blurTex.SampleLevel(s0, ruv + doff*(2.0/3.0), 0); color.r+=s.r/3.5; color.g+=s.g/7.0; color.a+=s.a/7.0;
    s = blurTex.SampleLevel(s0, ruv + doff*(1.0/3.0), 0); color.r+=s.r/3.5; color.g+=s.g/3.5; color.a+=s.a/7.0;
    s = blurTex.SampleLevel(s0, ruv, 0); color.g+=s.g/3.5; color.a+=s.a/7.0;
    s = blurTex.SampleLevel(s0, ruv - doff*(1.0/3.0), 0); color.g+=s.g/3.5; color.b+=s.b/3.0; color.a+=s.a/7.0;
    s = blurTex.SampleLevel(s0, ruv - doff*(2.0/3.0), 0); color.b+=s.b/3.0; color.a+=s.a/7.0;
    s = blurTex.SampleLevel(s0, ruv - doff, 0); color.r+=s.r/7.0; color.b+=s.b/3.0; color.a+=s.a/7.0;
    float lum = dot(color.rgb, lumVec);
    color.rgb = lerp(float3(lum,lum,lum), color.rgb, saturation);
    color.rgb = saturate(color.rgb * 0.92);
    color.a *= edgeAA;
    return color;
}
)";

// Edge highlight (unused — kept for future re-implementation)
static const char* HighlightPS = R"(
Texture2D t0 : register(t0); SamplerState s0 : register(s0);
)" SDF_COMMON_HLSL R"(
cbuffer HighlightCB : register(b0) {
    float2 elementPos     : packoffset(c0.x);
    float2 elementSize    : packoffset(c0.z);
    float4 cornerRadii    : packoffset(c1);
    float4 highlightColor : packoffset(c2);
    float angle           : packoffset(c3.x);
    float falloff         : packoffset(c3.y);
    float highlightWidth  : packoffset(c3.z);
    float padding         : packoffset(c3.w);
};
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 pc = svpos.xy;
    float2 hs = elementSize * 0.5;
    float2 cc = pc - elementPos - hs;
    float r = radiusAt(cc, cornerRadii);
    float sd = sdRoundedRect(cc, hs, r);
    if (sd > highlightWidth || sd < -highlightWidth * 2.0) discard;
    float gr = min(r * 1.5, min(hs.x, hs.y));
    float2 grad = gradSdRoundedRect(cc, hs, gr);
    float2 ld = float2(cos(angle), sin(angle));
    float intensity = pow(abs(dot(grad, ld)), falloff);
    float ef = 1.0 - smoothstep(-highlightWidth * 0.5, highlightWidth, sd);
    return highlightColor * intensity * ef;
}
)";

// SDF drop shadow — offset {0,6}, blur 20px, alpha 0.30
static const char* ShadowPS = R"(
Texture2D t0 : register(t0); SamplerState s0 : register(s0);
)" SDF_COMMON_HLSL R"(
cbuffer ShadowCB : register(b0) {
    float2 elementPos   : packoffset(c0.x);
    float2 elementSize  : packoffset(c0.z);
    float4 cornerRadii  : packoffset(c1);
    float2 shadowOffset : packoffset(c2.x);
    float shadowBlur    : packoffset(c2.z);
    float padding1      : packoffset(c2.w);
    float4 shadowColor  : packoffset(c3);
    float padding2      : packoffset(c4.x);
};
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 pc = svpos.xy + shadowOffset;
    float2 hs = elementSize * 0.5;
    float2 cc = pc - elementPos - hs;
    float r = radiusAt(cc, cornerRadii);
    float sd = sdRoundedRect(cc, hs, r);
    float alpha = 1.0 - smoothstep(-shadowBlur, shadowBlur, sd);
    return shadowColor * alpha;
}
)";

// Background image scaling — cover-fill mode (aspect-ratio crop)
static const char* ImageCopyPS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer ImageCB : register(b0) { float2 imageSize : packoffset(c0.x); float2 screenSize : packoffset(c0.z); };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float scale = max(screenSize.x/imageSize.x, screenSize.y/imageSize.y);
    float2 visUV = screenSize / (imageSize * scale);
    float2 off = (1.0 - visUV) * 0.5;
    return inputTex.SampleLevel(s0, (svpos.xy/screenSize)*visUV + off, 0);
}
)";

// Zero-CB texture copy (1:1 passthrough via vertex uv)
static const char* PassthroughPS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    return inputTex.SampleLevel(s0, uv, 0);
}
)";

