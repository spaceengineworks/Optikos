#version 450

layout(location = 0) in vec4 fsColor;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in vec2 v_Position;
layout(location = 3) in vec4 v_Size;

layout(location = 0) out vec4 color;

layout(push_constant) uniform PushBlock {
    vec2 uScreenSize;
    int uHasTexture;
} push;

// layout(binding = 0) uniform sampler2D uTexture;

void main()
{
    if (v_Position.x < v_Size.x || v_Position.x > v_Size.y ||
        v_Position.y < v_Size.z || v_Position.y > v_Size.w)
    {
        color = vec4(0.0);
        return;
    }

    if (push.uHasTexture == 2)
    {
        // color = texture(uTexture, v_TexCoord);
    }
    else if (push.uHasTexture == 1)
    {
        // float sampledAlpha = texture(uTexture, v_TexCoord).r;
        // color = vec4(fsColor.rgb, fsColor.a * sampledAlpha);
    }
    else
    {
        color = fsColor;
    }
}