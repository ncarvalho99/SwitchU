// Fragment shader for 2D rendering — deko3d / uam
// Compiled by: uam -s frag -o basic_fsh.dksh basic_fsh.glsl
#version 460

layout (location = 0) in vec2 fragUV;
layout (location = 1) in vec4 fragColor;

layout (binding = 0) uniform sampler2D tex;

// Rounded corners are cut here rather than approximated by a triangle fan.
// The fan quantised every corner into eight straight segments, which is what
// made icon and card edges look stepped, and widening it into an antialiased
// skirt cost hundreds of vertices per shape and faulted the GPU. A signed
// distance field gives exact per-pixel coverage for six vertices and one draw.
//
// roundUvMin/roundUvScale remap fragUV onto 0..1 across the shape, so one mask
// serves quads whose UVs span a whole texture and quads that sample an
// arbitrary sub-rect of a screen-space capture.
layout (std140, binding = 1) uniform FsUniforms {
    int   useTexture;    // 0 = color only, 1 = texture × color
    float roundRadius;   // pixels; 0 disables the mask
    vec2  roundSize;     // shape extent in pixels
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
    vec4 c = (useTexture != 0) ? texture(tex, fragUV) * fragColor : fragColor;

    if (roundRadius > 0.0) {
        vec2 t = (fragUV - roundUvMin) * roundUvScale;
        vec2 halfExtent = roundSize * 0.5;
        float d = sdRoundedBox((t - vec2(0.5)) * roundSize, halfExtent, roundRadius);
        // fwidth is how far the field moves across one pixel, so the ramp stays
        // one pixel wide however large the shape is drawn.
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
