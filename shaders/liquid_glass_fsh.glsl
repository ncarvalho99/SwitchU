// Liquid glass fragment shader for deko3d.
// Based on the OverShifted/LiquidGlass superellipse refraction pass.
#version 460

layout (location = 0) in vec2 fragUV;
layout (location = 1) in vec4 fragColor;

layout (binding = 0) uniform sampler2D tex;

// FsUniforms layout (matches nxui::FsUniforms struct):
//   int  useTexture;      // offset  0
//   float param1..param3; // offset  4, 8, 12
//   float extra[48];      // offset 16
layout (std140, binding = 1) uniform FsUniforms {
    int   useTexture;
    float lg_refractionIntensity;   // overall refraction strength (0..1)
    float lg_blurIntensity;         // blur contribution (0..2)
    float lg_noiseIntensity;        // noise dithering (0..1)
    // extra[0..47] — packed as vec4s for std140 alignment
    vec4  lg_pack0;   // x=glowIntensity, y=saturation, z=opacity, w=roughness
    vec4  lg_pack1;   // x=animSpeed, y=time, z=powerFactor, w=fPower
    vec4  lg_pack2;   // x=refA, y=refB, z=refC, w=refD
    vec4  lg_pack3;   // x=glowWeight, y=glowBias, z=glowEdge0, w=glowEdge1
    vec4  lg_tintColor;      // rgba tint
    vec4  lg_panelRect;      // x, y, width, height in screen pixels
    vec4  lg_screenSize;     // x=screenW, y=screenH, z=shadeAmount, w=0
};

layout (location = 0) out vec4 outColor;

const float M_E = 2.718281828459045;
const float M_PI = 3.14159265359;

// pow() compiles to exp2(n * log2(x)) here — two transcendental ops per call.
// Exponentiation by squaring instead, for any integer n up to 32. powerFactor
// is a uniform, so the loop unrolls and every pixel takes the same path.
float powN(float x, float n) {
    int e = int(n + 0.5);
    if (abs(n - float(e)) > 0.001 || e < 1 || e > 32)
        return pow(x, n);

    float result = 1.0;
    float base = x;
    // 6 iterations covers exponents up to 63; the loop bound is a constant so
    // it unrolls, and e is a uniform so every pixel takes the same path.
    for (int i = 0; i < 6; ++i) {
        if ((e & 1) != 0) result *= base;
        e >>= 1;
        if (e == 0) break;
        base *= base;
    }
    return result;
}

// Ran four pow() per pixel. Two are gone by algebra: q^(n-1) is q^n / q, and
// q^n is already computed for the numerator. The other two go through powN.
float sdSuperellipse(vec2 p, vec2 r, float n) {
    vec2 pa = abs(p);
    vec2 safeR = max(r, vec2(0.00001));
    vec2 q = pa / safeR;

    float qxn = powN(q.x, n);
    float qyn = powN(q.y, n);
    float num = qxn + qyn - 1.0;

    vec2 qSafe = max(q, vec2(0.00001));
    vec2 grad = vec2(
        n * (qxn / qSafe.x) / safeR.x,
        n * (qyn / qSafe.y) / safeR.y
    );
    float den = length(grad) + 0.00001;
    return num / den;
}

float refractionCurve(float x) {
    float a = lg_pack2.x;
    float b = lg_pack2.y;
    float c = lg_pack2.z;
    float d = lg_pack2.w;
    return 1.0 - b * pow(c * M_E, -d * x - a);
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 applySaturation(vec3 color, float saturation) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), color, saturation);
}

vec4 sampleBlurred(vec2 uv, vec2 texelSize, float blurRadius) {
    if (blurRadius <= 0.001) {
        return texture(tex, uv);
    }

    vec2 off1 = texelSize * blurRadius * 1.3846153846;
    vec2 off2 = texelSize * blurRadius * 3.2307692308;

    vec4 color = texture(tex, uv) * 0.2270270270;
    color += texture(tex, uv + vec2(off1.x, 0.0)) * 0.3162162162;
    color += texture(tex, uv - vec2(off1.x, 0.0)) * 0.3162162162;
    color += texture(tex, uv + vec2(off2.x, 0.0)) * 0.0702702703;
    color += texture(tex, uv - vec2(off2.x, 0.0)) * 0.0702702703;
    color += texture(tex, uv + vec2(0.0, off1.y)) * 0.3162162162;
    color += texture(tex, uv - vec2(0.0, off1.y)) * 0.3162162162;
    color += texture(tex, uv + vec2(0.0, off2.y)) * 0.0702702703;
    color += texture(tex, uv - vec2(0.0, off2.y)) * 0.0702702703;
    return color * 0.5;
}

vec3 screenBlend(vec3 base, vec3 tint) {
    return 1.0 - (1.0 - base) * (1.0 - tint);
}

float computeGlow(vec2 uv) {
    return sin(atan(uv.y * 2.0 - 1.0, uv.x * 2.0 - 1.0) - 0.5);
}

void main() {
    float refrIntensity  = lg_refractionIntensity;
    float blurIntensity  = lg_blurIntensity;
    float noiseIntensity = lg_noiseIntensity;
    float glowIntensity  = lg_pack0.x;
    float saturation     = lg_pack0.y;
    float reflectionStrength = lg_pack0.z;
    float roughness      = lg_pack0.w;
    float animSpeed      = lg_pack1.x;
    float time           = lg_pack1.y;
    float powerFactor    = lg_pack1.z;
    float fPower         = lg_pack1.w;
    float glowWeight     = lg_pack3.x;
    float glowBias       = lg_pack3.y;
    float glowEdge0      = lg_pack3.z;
    float glowEdge1      = lg_pack3.w;
    float unavailableShade = clamp(lg_screenSize.z, 0.0, 1.0);

    vec2 panelSize = max(lg_panelRect.zw, vec2(1.0));
    float panelMinSide = max(1.0, min(panelSize.x, panelSize.y));
    vec2 panelAspect = panelSize / panelMinSide;

    vec2 center = vec2(0.5);
    vec2 p = (fragUV - center) * 2.0 * panelAspect;

    float d = sdSuperellipse(p, panelAspect, powerFactor);

    // Antialias the panel outline. d is a signed distance, so fading across
    // one pixel's width in that field — which fwidth gives directly — is all
    // the edge needs. The hard discard on d > 0 made every pixel fully in or
    // fully out, which with no MSAA on the framebuffer is exactly the
    // stair-stepped corners reported on the home icons.
    //
    // This is a re-application: the same change was reverted while isolating
    // a flickering menu, whose actual cause turned out to be a vertex buffer
    // overflow elsewhere. The shader itself already ran on hardware without
    // incident.
    float edgeWidth = max(fwidth(d), 1e-5);
    float coverage = 1.0 - smoothstep(-edgeWidth, edgeWidth, d);
    if (coverage <= 0.0)
        discard;

    float dist = max(-d, 0.0);

    // refractionCurve is a pow per pixel; mix() then scales its effect by
    // refrIntensity, which is 0.035 here. Below a threshold the displacement is
    // under a pixel, so the sample lands on the same texel either way.
    vec2 sampleP = p;
    if (refrIntensity > 0.002) {
        float refBase = refractionCurve(dist);
        float refScale = (abs(fPower - 1.0) < 0.001) ? refBase : pow(refBase, fPower);
        sampleP = p * mix(1.0, refScale, refrIntensity);
    }

    // A sin and a cos per pixel, scaled by roughness * 0.05. The settings
    // overlay uses 0.004, giving a displacement of 2e-4 — far below a texel.
    float waveTime = time * animSpeed;
    if (roughness > 0.02) {
        sampleP += vec2(
            sin((p.y + waveTime) * 14.0),
            cos((p.x - waveTime) * 12.0)
        ) * roughness * 0.05;
    }

    vec2 localUV = (sampleP / panelAspect) * 0.5 + 0.5;

    vec2 panelPos  = lg_panelRect.xy;
    vec2 panelPixelSize = lg_panelRect.zw;
    vec2 screenPx  = panelPos + localUV * panelPixelSize;
    vec2 screenUV  = screenPx / lg_screenSize.xy;

    screenUV = clamp(screenUV, 0.0, 1.0);

    // With blurIntensity 0, which is what every preset ships, blurMix is 0 and
    // the mix returns `original` — but both samples were still being fetched.
    vec4 color = texture(tex, screenUV);
    float blurMix = clamp(blurIntensity / 10.0, 0.0, 1.0);
    if (blurMix > 0.001) {
        vec2 texelSize = 1.0 / max(lg_screenSize.xy, vec2(1.0));
        color = mix(color, sampleBlurred(screenUV, texelSize, blurIntensity), blurMix);
    }

    // rand() costs a sin per pixel and noiseIntensity is 0 in every preset
    // shipped, so the result was being multiplied away. Uniform branch.
    if (noiseIntensity > 0.001) {
        float n = (rand((gl_FragCoord.xy + waveTime * 61.0) * 1e-3) - 0.5) * noiseIntensity;
        color.rgb += vec3(n);
    }

    // computeGlow costs an atan plus a sin per pixel. Its contribution is
    // scaled by glowWeight * glowIntensity, which the settings overlay leaves
    // at 0.14 * 0.14, about 2% — below what is visible, but paid in full over
    // ~875k pixels. Skipped when the combined weight cannot show up.
    float glowMul = 1.0 + glowBias;
    if (glowWeight * glowIntensity > 0.004) {
        float glow = computeGlow(fragUV);
        float glowMask = smoothstep(glowEdge0, glowEdge1, dist);
        glowMul += glow * glowWeight * glowIntensity * glowMask;
    }
    color.rgb *= glowMul;

    float edgeReflection = 1.0 - smoothstep(0.0, 0.32, dist);
    float topReflection = pow(clamp(1.0 - fragUV.y, 0.0, 1.0), 2.2);
    float reflectionMask = edgeReflection * (0.55 + 0.45 * topReflection);
    vec3 reflectionTint = mix(vec3(0.94, 0.97, 1.0), clamp(lg_tintColor.rgb, 0.0, 1.0), 0.06);
    float reflection = clamp(reflectionStrength * (0.08 + glowIntensity * 0.06) * reflectionMask,
                             0.0, 0.42);
    color.rgb = mix(color.rgb, screenBlend(color.rgb, reflectionTint), reflection);

    vec3 bodyTint = mix(vec3(0.96, 0.98, 1.0), clamp(lg_tintColor.rgb, 0.0, 1.0), 0.12);
    color.rgb = mix(color.rgb, color.rgb * bodyTint, clamp(lg_tintColor.a * 0.08, 0.0, 0.08));
    color.rgb = applySaturation(color.rgb, saturation);

    if (unavailableShade > 0.001) {
        float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
        vec3 desaturated = mix(color.rgb, vec3(luma), unavailableShade * 0.18);
        vec3 veilColor = vec3(0.82, 0.84, 0.88);
        color.rgb = mix(color.rgb, desaturated * veilColor, unavailableShade * 0.32);
    }

    // Edge coverage rather than a flat 1.0; fragColor still carries the
    // panel's own opacity and multiplies in below.
    color.a = coverage;

    outColor = color * fragColor;
}
