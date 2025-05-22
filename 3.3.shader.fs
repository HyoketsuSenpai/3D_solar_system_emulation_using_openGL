#version 330 core


uniform vec3 lightPos;
uniform sampler2D ourTexture;

out vec4 FragColor;
out vec3 Normal;

in vec3 ourColor;
//in vec3 ourPosition;
in vec3 bNormal;
in vec3 FragPos;
in vec2 TextureCoord;

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

    vec3 result = (ambient + diffuse) * vec3(tex.xyz);
    FragColor = vec4(result, 1.0);

    Normal = norm;
}