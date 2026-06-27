#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

#define     MAX_LIGHTS              16
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 diffuseSum = vec3(0.0);
    vec3 specularSum = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec4 tint = colDiffuse * fragColor;

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 lightDir = vec3(0.0);
            float attenuation = 1.0;

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                lightDir = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT)
            {
                vec3 diff = lights[i].position - fragPosition;
                float dist = length(diff);
                lightDir = diff / max(dist, 0.001);
                attenuation = 1.0 / (1.0 + 0.5 * dist + 0.1 * dist * dist);
            }

            float NdotL = max(dot(normal, lightDir), 0.0);
            diffuseSum += lights[i].color.rgb * NdotL * attenuation * 1.5;

            float specCo = 0.0;
            if (NdotL > 0.0)
                specCo = pow(max(0.0, dot(viewD, reflect(-lightDir, normal))), 16.0);
            specularSum += lights[i].color.rgb * specCo * attenuation;
        }
    }

    vec3 final = texelColor.rgb * tint.rgb * diffuseSum + specularSum;
    final += texelColor.rgb * tint.rgb * ambient.rgb * 0.05;

    finalColor = vec4(final, texelColor.a * tint.a);
    finalColor = pow(finalColor, vec4(1.0 / 2.2));
}
