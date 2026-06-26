#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aSize;

layout(push_constant) uniform PushBlock {
    vec2 uScreenSize;
    int uHasTexture;
} push;

layout(location = 0) out vec4 fsColor;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out vec2 v_Position;
layout(location = 3) out vec4 v_Size;

void main()
{
    fsColor = aColor;

    vec2 ndc = (aPosition / push.uScreenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, ndc.y, 0.0, 1.0);

    v_TexCoord = aTexCoord;
    v_Position  = aPosition;
    v_Size      = aSize;
}