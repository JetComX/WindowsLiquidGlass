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
"    if (cornerCoord.x >= 0.0 || cornerCoord.y >= 0.0) {\n" \
"        return sign(coord) * normalize(max(cornerCoord, 0.0));\n" \
"    } else {\n" \
"        float gradX = step(cornerCoord.y, cornerCoord.x);\n" \
"        return sign(coord) * float2(gradX, 1.0 - gradX);\n" \
"    }\n" \
"}\n" \
"float circleMap(float x) {\n" \
"    return 1.0 - sqrt(1.0 - x * x);\n" \
"}\n"

static const char* FullscreenVS = R"(
struct VSOutput { float4 svpos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOutput main(uint id : SV_VertexID) {
    VSOutput o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.svpos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}
)";

static const char* BackgroundPS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer BgCB : register(b0) { float2 screenSize; float time; float padding; };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 uvCoord = svpos.xy / screenSize;
    float3 bgColor = lerp(float3(0.18,0.12,0.28), float3(0.08,0.18,0.35), uvCoord.x*0.7+uvCoord.y*0.3);
    float glowDist = length(float2(uvCoord.x-0.5, uvCoord.y-0.15)) / 0.7;
    bgColor += float3(0.22,0.10,0.03) * exp(-glowDist*2.5) * 0.6;
    return float4(bgColor, 1.0);
}
)";

static const char* BlurH_PS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer BlurCB : register(b0) { float2 texelSize; int kernelRadius; float sigma; };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 coord = svpos.xy * texelSize;
    float sumW = 1.0;
    float4 color = inputTex.SampleLevel(s0, coord, 0);
    for (int i = 1; i <= kernelRadius; i++) {
        float w = exp(-(float(i)*float(i))/(2.0*sigma*sigma));
        sumW += 2.0 * w;
        float2 off = float2(texelSize.x * float(i), 0.0);
        color += inputTex.SampleLevel(s0, coord + off, 0) * w;
        color += inputTex.SampleLevel(s0, coord - off, 0) * w;
    }
    return color / sumW;
}
)";

static const char* BlurV_PS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer BlurCB : register(b0) { float2 texelSize; int kernelRadius; float sigma; };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 coord = svpos.xy * texelSize;
    float sumW = 1.0;
    float4 color = inputTex.SampleLevel(s0, coord, 0);
    for (int i = 1; i <= kernelRadius; i++) {
        float w = exp(-(float(i)*float(i))/(2.0*sigma*sigma));
        sumW += 2.0 * w;
        float2 off = float2(0.0, texelSize.y * float(i));
        color += inputTex.SampleLevel(s0, coord + off, 0) * w;
        color += inputTex.SampleLevel(s0, coord - off, 0) * w;
    }
    return color / sumW;
}
)";

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
    if (-sd >= refractionHeight) {
        float4 c = blurTex.SampleLevel(s0, pc * screenSizeInv, 0);
        float lum = dot(c.rgb, lumVec);
        c.rgb = lerp(float3(lum,lum,lum), c.rgb, saturation);
        c.rgb *= 0.92;
        c.a *= edgeAA;
        return c;
    }
    sd = min(sd, 0.0);
    float d = circleMap(1.0 - (-sd/refractionHeight)) * refractionAmount;
    float gr = min(r * 1.5, min(hs.x, hs.y));
    float2 grad = normalize(gradSdRoundedRect(cc, hs, gr) + depthEffect * normalize(cc));
    float4 c = blurTex.SampleLevel(s0, (pc + d * grad) * screenSizeInv, 0);
    float lum = dot(c.rgb, lumVec);
    c.rgb = lerp(float3(lum,lum,lum), c.rgb, saturation);
    c.rgb *= 0.92;
    c.a *= edgeAA;
    return c;
}
)";

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
    if (-sd >= refractionHeight) {
        float4 c = blurTex.SampleLevel(s0, pc * screenSizeInv, 0);
        float lum = dot(c.rgb, lumVec);
        c.rgb = lerp(float3(lum,lum,lum), c.rgb, saturation);
        c.rgb *= 0.92;
        c.a *= edgeAA;
        return c;
    }
    sd = min(sd, 0.0);
    float d = circleMap(1.0 - (-sd/refractionHeight)) * refractionAmount;
    float gr = min(r * 1.5, min(hs.x, hs.y));
    float2 grad = normalize(gradSdRoundedRect(cc, hs, gr) + depthEffect * normalize(cc));
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
    color.rgb *= 0.85;
    color.a *= edgeAA;
    return color;
}
)";

static const char* HighlightPS = R"(
Texture2D t0 : register(t0); SamplerState s0 : register(s0);
)" SDF_COMMON_HLSL R"(
cbuffer HighlightCB : register(b0) {
    float2 elementPos; float2 elementSize; float4 cornerRadii;
    float4 highlightColor; float angle; float falloff; float highlightWidth; float padding;
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

static const char* ShadowPS = R"(
Texture2D t0 : register(t0); SamplerState s0 : register(s0);
)" SDF_COMMON_HLSL R"(
cbuffer ShadowCB : register(b0) {
    float2 elementPos; float2 elementSize; float4 cornerRadii;
    float2 shadowOffset; float shadowBlur; float padding1;
    float4 shadowColor; float padding2;
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

static const char* ImageCopyPS = R"(
Texture2D inputTex : register(t0); SamplerState s0 : register(s0);
cbuffer ImageCB : register(b0) { float2 imageSize; float2 screenSize; };
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

static const char* DebugSolidPS = R"(
)" SDF_COMMON_HLSL R"(
cbuffer SolidCB : register(b0) { float2 ep; float2 es; float4 cr; float4 sc; };
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 hs = es * 0.5; float2 cc = svpos.xy - ep - hs;
    float sd = sdRoundedRect(cc, hs, radiusAt(cc, cr));
    if (sd > 0.0) discard;
    return sc;
}
)";
