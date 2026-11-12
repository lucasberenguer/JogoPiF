#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;

// The attack sheet carries a dark wine backdrop behind the ink and slash.
float foreground(vec3 rgb)
{
    vec3 levels = (rgb * 255.0 - vec3(56, 20, 24)) / vec3(13, 8, 9);
    return smoothstep(0.0, 1.0, max(levels.r, max(levels.g, levels.b)));
}

float nearbyInk(vec2 offset)
{
    vec4 pixel = texture(texture0, fragTexCoord + offset);
    return foreground(pixel.rgb) * smoothstep(0.90, 0.98, pixel.a);
}

void main()
{
    vec4 ink = texture(texture0, fragTexCoord);
    float mask = foreground(ink.rgb);
    if (mask < 1.0 && ink.a > 0.94) {
        vec2 stepUV = 2.0 / vec2(textureSize(texture0, 0));
        vec4 sides = vec4(0.0);
        for (int step = 1; step <= 7; ++step) {
            vec2 offset = float(step) * stepUV;
            sides = max(sides, vec4(nearbyInk(vec2(offset.x, 0)),
                                   nearbyInk(vec2(-offset.x, 0)),
                                   nearbyInk(vec2(0, offset.y)),
                                   nearbyInk(vec2(0, -offset.y))));
        }
        float enclosed = max(min(sides.x, sides.y), min(sides.z, sides.w));
        float outline = ink.r < 27.0 / 255.0 ? max(max(sides.x, sides.y), max(sides.z, sides.w)) : 0.0;
        mask = max(mask, max(enclosed, outline));
    }
    finalColor = vec4(ink.rgb, ink.a * mask) * fragColor * colDiffuse;
}
