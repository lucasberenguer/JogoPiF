#version 330

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 screenSize;
uniform int lightCount;
uniform vec4 lights[8];
uniform vec3 tints[8];
uniform vec2 ambientGrade;
uniform int shadowLightMask;
uniform int shadowCount;
uniform vec4 shadowStarts[7];
uniform vec3 shadowShapes[7];
out vec4 finalColor;

float projectedShadow(vec2 position, int lightIndex)
{
    float coverage = 0.0;
    for (int i = 0; i < shadowCount; i++) {
        if (int(shadowShapes[i].z) != lightIndex) continue;
        vec2 delta = position - shadowStarts[i].xy;
        vec2 axis = shadowStarts[i].zw;
        vec2 shape = shadowShapes[i].xy;
        float along = dot(delta, axis);
        if (along < 0.0 || along >= shape.x) continue;
        float across = abs(dot(delta, vec2(-axis.y, axis.x)));
        float progress = along / shape.x;
        float width = shape.y * (1.0 + .32 * progress);
        float fadeIn = smoothstep(0.0, min(shape.x * .24, shape.y * .42), along);
        float fadeOut = 1.0 - smoothstep(.48, 1.0, progress);
        float edge = 1.0 - smoothstep(width * (.86 - .43 * progress), width, across);
        coverage = max(coverage, fadeIn * fadeOut * edge);
    }
    return coverage;
}

void main()
{
    vec4 scene = texture(texture0, fragTexCoord);
    vec2 position = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * screenSize;
    float luma = dot(scene.rgb, vec3(.2126, .7152, .0722));
    float grade = 1.0 - smoothstep(.24, .68, luma);
    vec3 ambient = mix(scene.rgb, vec3(luma), ambientGrade.x * grade) * (1.0 - ambientGrade.y * grade);
    float light = 0.0;
    float shadowShade = 0.0;
    vec3 emission = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        vec2 delta = (position - lights[i].xy) * vec2(1.0, 1.18);
        float falloff = 1.0 - smoothstep(0.0, lights[i].z, length(delta));
        float strength = lights[i].w * falloff;
        if ((shadowLightMask & (1 << i)) != 0 && strength > .004) {
            float coverage = projectedShadow(position, i);
            shadowShade += coverage * strength * .27;
            strength *= 1.0 - .92 * coverage;
        }
        light += strength;
        emission += tints[i] * strength;
    }
    emission *= .85 / max(light, .85);
    float surface = smoothstep(0.0, .18, max(scene.r, max(scene.g, scene.b)));
    vec3 lit = ambient * (1.0 - min(shadowShade, .22)) + (1.0 - ambient) * emission * surface * .55;
    finalColor = vec4(clamp(lit, 0.0, 1.0), scene.a);
}
