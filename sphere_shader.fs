#version 330 core

out vec4 FragColor;

in float height;

void main()
{
    vec4 gLowColor = vec4(0.6, 0.38, 0.66, 1.0);
    vec4 gHighColor = vec4(0.0, 0.15, 0.66, 1.0);
    FragColor = vec4(1.0, 1.0, 1.0, 1.0); //mix(gLowColor, gHighColor, height);
}