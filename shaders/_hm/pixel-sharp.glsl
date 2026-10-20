// Harvest Moon SNES Recomp — "Pixel Sharp". Public domain / CC0-1.0.
//
// Nearest-neighbour look at ANY window size (1080p, 1440p, 4K...): inside a
// game pixel the colour is flat; only the last fraction of an output pixel at
// each edge is blended, so pixels stay square and the same size everywhere
// instead of some being one screen pixel wider than others. Needs linear
// filtering on its input (filter_linear = true in the preset).

#ifdef VERTEX
in vec2 VertexCoord;
in vec2 TexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(VertexCoord * 2.0 - 1.0, 0.0, 1.0);
}
#endif

#ifdef FRAGMENT
uniform sampler2D Texture;
uniform vec2 TextureSize;
uniform vec2 InputSize;
uniform vec2 OutputSize;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    vec2 texel = vTexCoord * TextureSize;
    vec2 scale = max(OutputSize / InputSize, vec2(1.0));   // screen pixels per game pixel
    vec2 cell = floor(texel);
    vec2 off = fract(texel) - 0.5;                          // -0.5 .. 0.5 inside the pixel
    vec2 flat_half = 0.5 - 0.5 / scale;                     // the flat part of the pixel
    vec2 edge = off - clamp(off, -flat_half, flat_half);    // 0 inside, small at the edges
    vec2 uv = (cell + 0.5 + edge * scale) / TextureSize;
    FragColor = vec4(texture(Texture, uv).rgb, 1.0);
}
#endif
