#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform int layer;
out vec4 finalColor;

float band(float value, float start, float end, float feather)
{
    return smoothstep(start, start + feather, value) *
           (1.0 - smoothstep(end - feather, end, value));
}

vec4 landscape(vec2 uv)
{
    vec4 base = texture(texture0, uv);
    float redWater = smoothstep(0.12, 0.48, base.r - base.b * 0.55);
    float riverCenter = 0.635 - 0.64 * (uv.y - 0.74);
    float river = band(uv.y, 0.719, 0.854, 0.023) *
                  (1.0 - smoothstep(0.033, 0.121, abs(uv.x - riverCenter)));
    float falls = band(uv.y, 0.545, 0.736, 0.035) *
                  (1.0 - smoothstep(0.009, 0.031, abs(uv.x - (0.671 - (uv.y - 0.60) * 0.18))));
    float flow = sin(uv.y * 375.0 - time * 3.1 + sin(uv.x * 62.0 + time * 0.7));
    float ripple = sin(uv.y * 522.0 - time * 4.2 + sin(uv.x * 74.0) * 1.8);
    vec2 offset = vec2(ripple * river * 0.0018, flow * river * 0.0013 + flow * falls * 0.0017);
    vec4 scene = mix(base, texture(texture0, uv + offset), max(river, falls) * 0.64);

    float moonDistance = length((uv - vec2(0.812, 0.206)) / vec2(0.082, 0.141));
    float moon = 1.0 - smoothstep(0.80, 1.28, moonDistance);
    float pulse = 0.5 + 0.5 * sin(time * 1.22);
    scene.rgb += base.rgb * moon * redWater * (pulse * 0.075 - 0.018);
    float reflection = river * redWater * redWater * (0.58 + 0.42 * pulse);
    scene.rgb += vec3(0.11, 0.026, 0.015) * reflection * (0.31 * flow + 0.22 * ripple);
    scene.rgb += vec3(0.045, 0.013, 0.010) * falls * redWater * flow;
    return scene;
}

vec4 frame(vec2 uv)
{
    float left = band(uv.x, 0.140, 0.251, 0.020);
    float right = band(uv.x, 0.751, 0.869, 0.020);
    float drape = smoothstep(0.225, 0.61, uv.y);
    float wave = left * sin(time * 2.05 - uv.y * 16.0) +
                 right * sin(time * 2.05 - uv.y * 16.0 + 1.3);
    float flutter = (left + right) * sin(time * 3.18 - uv.y * 23.0);
    uv.x += drape * (wave * 0.007 + flutter * 0.0016);
    vec4 art = texture(texture0, uv);
    // A referência tem um quadriculado cinza já incorporado à imagem.
    float high = max(max(art.r, art.g), art.b);
    float low = min(min(art.r, art.g), art.b);
    float checker = smoothstep(0.44, 0.67, low) * (1.0 - smoothstep(0.045, 0.18, high - low));
    art.rgb = mix(art.rgb, art.rgb * 0.64, checker);
    art.a *= (1.0 - checker) * (1.0 - smoothstep(0.583, 0.670, uv.y));
    return art;
}

vec4 buttonPlate(vec2 uv)
{
    vec4 art = texture(texture0, uv);
    // Reaproveita a moldura de Configurações e encobre apenas a área do rótulo.
    float interior = band(uv.x, 0.125, 0.873, 0.020) * band(uv.y, 0.288, 0.723, 0.032);
    vec3 fill = vec3(0.235, 0.021, 0.066);
    fill += vec3(0.087, 0.013, 0.024) * (1.0 - abs(uv.y - 0.49) * 4.0);
    art.rgb = mix(art.rgb, fill, interior);
    return art;
}

void main()
{
    vec4 pixel;
    if (layer == 0) pixel = landscape(fragTexCoord);
    else if (layer == 1) pixel = frame(fragTexCoord);
    else if (layer == 2) pixel = buttonPlate(fragTexCoord);
    else {
        vec4 source = texture(texture0, fragTexCoord);
        float ink = smoothstep(0.32, 0.73, source.g);
        pixel = vec4(mix(vec3(0.82, 0.48, 0.21), vec3(1.0, 0.92, 0.66), ink), source.a * ink);
    }
    finalColor = pixel * colDiffuse * fragColor;
}
