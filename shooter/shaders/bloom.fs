#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float bloomSize;

out vec4 finalColor;

void main()
{
    vec2 texel = 1.0 / resolution;
    vec4 sum = vec4(0);
    int range = int(bloomSize);
    float count = 0.0;

    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            vec2 offset = vec2(x, y) * texel * 2.0;
            vec4 sample = texture(texture0, fragTexCoord + offset);
            float dist = length(vec2(x, y));
            float weight = exp(-dist * dist / (bloomSize * 0.8));
            sum += sample * weight;
            count += weight;
        }
    }

    vec4 source = texture(texture0, fragTexCoord);
    vec4 bloom = sum / count;
    finalColor = mix(source, bloom, 0.35) * colDiffuse;
}
