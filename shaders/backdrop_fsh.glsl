// Fragment shader for drawing captured/blurred offscreen backdrop textures.
#version 460

layout (location = 0) in vec2 fragUV;
layout (location = 1) in vec4 fragColor;

// Same corner mask as basic_fsh, carried on the vertex. A backdrop draw always
// samples, so there is no useTexture here. See basic_fsh.glsl for why the
// corners are cut in the fragment stage rather than tessellated.
layout (location = 2) in vec2 fragShapePos;
layout (location = 3) in vec2 fragShapeHalf;
layout (location = 4) in vec2 fragShapeRound;

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec4 outColor;

float sdRoundedBox(vec2 p, vec2 halfExtent, float radius) {
    float r = min(radius, min(halfExtent.x, halfExtent.y));
    vec2 q = abs(p) - (halfExtent - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec4 c = texture(tex, fragUV) * fragColor;

    float radius = fragShapeRound.x;
    if (radius > 0.0) {
        float d = sdRoundedBox(fragShapePos, fragShapeHalf, radius);
        // fwidth is |dFdx| + |dFdy|: 1.0 along a flat edge, 1.41 on the
        // diagonal through a corner, which widened the ramp by 41% exactly
        // where the rounding starts and left a seam there. The gradient length
        // is the real per-pixel step and is continuous across the tangent.
        float w = max(length(vec2(dFdx(d), dFdy(d))), 1e-5);
        float thickness = fragShapeRound.y;
        if (thickness > 0.0) {
            // The stroke sits inside the boundary, so the band is centred on
            // -h. Sub-pixel strokes are widened to one pixel rather than left
            // to fade, which is how the selection ring reads as a line.
            float h = max(thickness, 1.0) * 0.5;
            c.a *= 1.0 - smoothstep(-w, w, abs(d + h) - h);
        } else {
            c.a *= 1.0 - smoothstep(-w, w, d);
        }
    }

    outColor = c;
}
