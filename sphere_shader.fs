#version 330 core

out vec4 FragColor;

in vec2 TextureCoord;

uniform sampler2D ourTexture;

void main()
{

    // vec4 gLowColor = vec4(0.6, 0.38, 0.66, 1.0);
    // vec4 gHighColor = vec4(0.0, 0.15, 0.66, 1.0);
    FragColor = texture(ourTexture, TextureCoord);
    //vec4(1.0, 1.0, 1.0, 1.0); //mix(gLowColor, gHighColor, height);
}