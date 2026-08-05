// Fragment shader for drawing captured/blurred offscreen backdrop textures.
#version 460

layout (location = 0) in vec2 fragUV;
layout (location = 1) in vec4 fragColor;

layout (binding = 0) uniform sampler2D tex;

// Same block and same corner mask as basic_fsh, so the renderer can push one
// uniform layout for both programs. useTexture is ignored here: a backdrop
// draw always samples. See basic_fsh.glsl for why the corners are cut in the
// fragment stage rather than tessellated.
layout (std140, binding = 1) uniform FsUniforms {
    int   useTexture;
    float roundRadius;
    vec2  roundSize;
    vec2  roundUvMin;
    vec2  roundUvScale;
    float roundThickness; // >0 strokes a band inside the edge instead of filling
};

layout (location = 0) out vec4 outColor;

float sdRoundedBox(vec2 p, vec2 halfExtent, float radius) {
    float r = min(radius, min(halfExtent.x, halfExtent.y));
    vec2 q = abs(p) - (halfExtent - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec4 c = texture(tex, fragUV) * fragColor;

    if (roundRadius > 0.0) {
        vec2 t = (fragUV - roundUvMin) * roundUvScale;
        vec2 halfExtent = roundSize * 0.5;
        float d = sdRoundedBox((t - vec2(0.5)) * roundSize, halfExtent, roundRadius);
        float w = max(fwidth(d), 1e-5);
        if (roundThickness > 0.0) {
            // The stroke sits inside the boundary, so the band is centred on
            // -h. Sub-pixel strokes are widened to one pixel rather than left
            // to fade out, which is how the selection ring reads as a line.
            float h = max(roundThickness, 1.0) * 0.5;
            c.a *= 1.0 - smoothstep(-w, w, abs(d + h) - h);
        } else {
            c.a *= 1.0 - smoothstep(-w, w, d);
        }
    }

    outColor = c;
}
