// Harvest Moon SNES Recomp — one EPX / Scale2x step. Public domain / CC0-1.0.
//
// Doubles the picture and rounds diagonal staircases: each game pixel P
// becomes four, and a quarter takes the colour of its two neighbours when
// they agree with each other (and not with the other side). Flat areas and
// straight edges are untouched, so the pixel art keeps its colours.

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
in vec2 vTexCoord;
out vec4 FragColor;

bool same(vec3 a, vec3 b) { return all(lessThan(abs(a - b), vec3(1.0 / 64.0))); }

void main() {
    vec2 px = vTexCoord * TextureSize;
    vec2 cell = floor(px);
    vec2 q = fract(px);                       // which quarter of the source pixel
    vec2 t = 1.0 / TextureSize;
    vec2 c = (cell + 0.5) * t;
    vec3 P = texture(Texture, c).rgb;
    vec3 A = texture(Texture, c + vec2(0.0, -t.y)).rgb;   // up
    vec3 B = texture(Texture, c + vec2( t.x, 0.0)).rgb;   // right
    vec3 C = texture(Texture, c + vec2(-t.x, 0.0)).rgb;   // left
    vec3 D = texture(Texture, c + vec2(0.0,  t.y)).rgb;   // down
    vec3 o = P;
    bool left = q.x < 0.5, top = q.y < 0.5;
    if (top && left)        { if (same(C, A) && !same(C, D) && !same(A, B)) o = A; }
    else if (top && !left)  { if (same(A, B) && !same(A, C) && !same(B, D)) o = B; }
    else if (!top && left)  { if (same(D, C) && !same(D, B) && !same(C, A)) o = C; }
    else                    { if (same(B, D) && !same(B, A) && !same(D, C)) o = D; }
    FragColor = vec4(o, 1.0);
}
#endif
