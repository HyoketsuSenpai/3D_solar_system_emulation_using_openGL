#version 330 core

in vec3 bNormal;
in vec3 FragPos;
in vec2 TextureCoord;

out vec4 FragColor;

uniform sampler2D ourTexture;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    vec4 tex = texture(ourTexture, TextureCoord);

    float ambientStrength = 0.2;
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(bNormal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.3;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = 0.0;
    if(diff > 0.0)
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * tex.rgb;
    FragColor = vec4(result, 1.0);
}