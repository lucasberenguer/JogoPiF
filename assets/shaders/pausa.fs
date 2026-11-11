#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D fogTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform int layer;
out vec4 finalColor;

float band(float value, float start, float end, float feather)
{
    return smoothstep(start, start + feather, value) *
           (1.0 - smoothstep(end - feather, end, value));
}

vec2 leafSway(vec2 uv, vec2 center, vec2 radius, float phase, float speed, float amplitude)
{
    vec2 local = (uv - center) / radius;
    float distanceSquared = dot(local, local);
    if (distanceSquared >= 1.0) return vec2(0.0);
    float weight = 1.0 - smoothstep(0.04, 1.0, distanceSquared);
    float gust = 0.72 + 0.28 * sin(time * 0.48 + phase);
    float sway = sin(time * speed + phase + local.x * 1.7);
    float curl = sin(time * (speed * 1.36) + phase + local.y * 2.0);
    return vec2(sway, curl * 0.35) * weight * gust * amplitude;
}

float mirrorDrift(float x)
{
    float position = 1.0 - abs(2.0 * fract(x * 0.5) - 1.0);
    // Faça a textura voltar suavemente ao tocar as bordas espelhadas.
    if (position < 0.04) return position * smoothstep(0.0, 0.04, position);
    if (position > 0.96) return 1.0 - (1.0-position) * smoothstep(0.0, 0.04, 1.0-position);
    return position;
}

vec4 landscape(vec2 uv)
{
    vec2 plants = vec2(0.0);
    if (uv.x < 0.25) {
        plants += leafSway(uv, vec2(0.066, 0.559), vec2(0.065, 0.066), 0.4, 1.9, 0.0025);
        plants += leafSway(uv, vec2(0.064, 0.835), vec2(0.081, 0.091), 2.1, 1.4, 0.0027);
        plants += leafSway(uv, vec2(0.217, 0.754), vec2(0.032, 0.050), 3.4, 2.4, 0.0022);
    } else if (uv.x > 0.68) {
        plants += leafSway(uv, vec2(0.769, 0.803), vec2(0.082, 0.090), 4.6, 1.65, 0.0024);
        plants += leafSway(uv, vec2(0.929, 0.423), vec2(0.073, 0.104), 1.7, 1.1, 0.0021);
    }
    float riverCenter = 0.635 - 0.64 * (uv.y - 0.74);
    float river = band(uv.y, 0.719, 0.854, 0.023) *
                  (1.0 - smoothstep(0.033, 0.121, abs(uv.x - riverCenter)));
    float falls = band(uv.y, 0.545, 0.736, 0.035) *
                  (1.0 - smoothstep(0.009, 0.031, abs(uv.x - (0.671 - (uv.y - 0.60) * 0.18))));
    float flow = sin(uv.y * 375.0 - time * 3.1 + sin(uv.x * 62.0 + time * 0.7));
    float ripple = sin(uv.y * 522.0 - time * 4.2 + sin(uv.x * 74.0) * 1.8);
    vec2 water = vec2(ripple * river * 0.0012, flow * river * 0.0008 + flow * falls * 0.0011);
    vec4 scene = texture(texture0, uv + plants + water);

    float pulse = 0.5 + 0.5 * sin(time * 1.10 - 1.5708);
    float moonDistance = length((uv - vec2(0.812, 0.209)) / vec2(0.078, 0.134));
    float disk = 1.0 - smoothstep(0.87, 1.06, moonDistance);
    float halo = smoothstep(0.74, 1.03, moonDistance) * (1.0 - smoothstep(1.03, 1.71, moonDistance));
    float sky = smoothstep(0.055, 0.24, scene.r);
    scene.rgb += scene.rgb * disk * sky * mix(-0.025, 0.16, pulse);
    scene.rgb += vec3(0.18, 0.058, 0.034) * halo * sky * mix(0.10, 0.78, pulse);

    float redWater = smoothstep(0.12, 0.48, scene.r - scene.b * 0.55);
    float reflection = river * redWater * redWater;
    scene.rgb += vec3(0.11, 0.038, 0.022) * reflection * (0.56*pulse + 0.31*flow + 0.22*ripple);
    scene.rgb += vec3(0.045, 0.013, 0.010) * falls * redWater * flow;

    float fogBanks = band(uv.y, 0.49, 0.73, 0.10) * band(uv.x, 0.09, 0.99, 0.25);
    float fogValley = band(uv.y, 0.65, 0.92, 0.08) * band(uv.x, 0.14, 0.96, 0.26);
    if (max(fogBanks, fogValley) > 0.0) {
        float banks = texture(fogTexture, vec2(mirrorDrift(uv.x*1.24 - time*0.013), uv.y*2.6 - 1.14)).r;
        float valley = texture(fogTexture, vec2(mirrorDrift(0.36 + uv.x*1.43 - time*0.025), uv.y*2.7 - 1.62)).r;
        float opacity = fogBanks * smoothstep(0.23, 0.70, banks) * (0.18 + 0.035*sin(time*0.69));
        opacity += fogValley * smoothstep(0.22, 0.74, valley) * (0.23 + 0.045*sin(time*0.47 + 1.8));
        float moonlight = pulse * (1.0 - smoothstep(0.0, 0.43, abs(uv.x - 0.70)));
        scene.rgb = mix(scene.rgb, vec3(0.42, 0.18, 0.20) + vec3(0.17, 0.050, 0.028)*moonlight,
                        opacity * (1.0 - 0.52*reflection));
    }
    return scene;
}

vec4 frame(vec2 uv, bool premultiplied)
{
    float left = band(uv.x, 0.137, 0.249, 0.010);
    float right = band(uv.x, 0.750, 0.868, 0.010);
    float drape = smoothstep(0.242, 0.60, uv.y);
    float wave = left * sin(time * 2.05 - uv.y * 16.0) +
                 right * sin(time * 2.05 - uv.y * 16.0 + 1.3);
    float flutter = (left + right) * sin(time * 3.18 - uv.y * 23.0);
    vec2 leaves = leafSway(uv, vec2(0.187, 0.223), vec2(0.072, 0.069), 0.9, 1.7, 0.0026);
    leaves += leafSway(uv, vec2(0.333, 0.242), vec2(0.056, 0.049), 2.6, 1.1, 0.0022);
    leaves += leafSway(uv, vec2(0.663, 0.233), vec2(0.060, 0.054), 3.9, 1.9, 0.0021);
    leaves += leafSway(uv, vec2(0.804, 0.220), vec2(0.069, 0.066), 5.2, 1.4, 0.0028);
    vec2 cloth = vec2(drape * (wave * 0.007 + flutter * 0.0016), drape * wave * 0.0015);
    vec4 art = texture(texture0, uv + cloth + leaves);
    if (premultiplied) {
        // A textura é interpolada em alfa pré-multiplicado. A mistura do raylib usa alfa direto.
        art.rgb /= max(art.a, 1.0 / 255.0);
    } else {
        // Builds antigos usam a camada 1 e ainda enviam a moldura RGB com o quadriculado.
        float low = min(min(art.r, art.g), art.b);
        float high = max(max(art.r, art.g), art.b);
        float alpha = max(1.0 - smoothstep(70.0, 142.0, low*255.0),
                          smoothstep(19.0, 62.0, (high-low)*255.0));
        art.rgb = clamp((art.rgb - (218.0/255.0)*(1.0-alpha))/max(alpha, 1.0/255.0), 0.0, 1.0);
        art.a *= alpha;
    }
    art.a *= 1.0 - smoothstep(0.583, 0.670, uv.y);
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
    else if (layer == 1 || layer == 4) pixel = frame(fragTexCoord, layer == 4);
    else if (layer == 2) pixel = buttonPlate(fragTexCoord);
    else {
        vec4 source = texture(texture0, fragTexCoord);
        float ink = smoothstep(0.32, 0.73, source.g);
        pixel = vec4(mix(vec3(0.82, 0.48, 0.21), vec3(1.0, 0.92, 0.66), ink), source.a * ink);
    }
    finalColor = pixel * colDiffuse * fragColor;
}
