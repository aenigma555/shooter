#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;

out vec4 finalColor;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    float fresnel = 1.0 - max(dot(normal, viewDir), 0.0);
    fresnel = pow(fresnel, 3.0);
    vec3 baseColor = texelColor.rgb * colDiffuse.rgb;
    vec3 glowColor = baseColor + vec3(1.0) * fresnel * 0.6;
    glowColor *= 1.5;
    finalColor = vec4(glowColor, texelColor.a * colDiffuse.a);
}
