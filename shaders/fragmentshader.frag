#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D textureY;
uniform sampler2D textureU;
uniform sampler2D textureV;

void main()
{   
    float y = texture(textureY, TexCoord).r;
    float u = texture(textureU, TexCoord).r - 0.5;
    float v = texture(textureV, TexCoord).r - 0.5;
    
    
    float r = y + 1.5748 * v;
    float g = y - 0.187324 * u - 0.468124 * v;
    float b = y + 1.8556 * u;

    FragColor = vec4(r, g, b, 1.0);
}