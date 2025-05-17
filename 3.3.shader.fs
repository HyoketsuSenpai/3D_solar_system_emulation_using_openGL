#version 330 core


uniform vec3 lightPos;

out vec4 FragColor;
out vec3 Normal;

in vec3 ourColor;
in vec3 ourPosition;
in vec3 bNormal;
in vec3 FragPos;

void main()
{
    float ambientStrength = 0.5;
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(bNormal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 result = (ambient + diffuse) * ourColor;
    FragColor = vec4(result, 1.0);
}