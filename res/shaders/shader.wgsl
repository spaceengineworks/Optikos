struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) color: vec4<f32>,
    @location(2) tex_coord: vec2<f32>,
    @location(3) size: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) v_TexCoord: vec2<f32>,
    @location(1) v_Size: vec4<f32>,
    @location(2) v_Position: vec2<f32>,
    @location(3) fsColor: vec4<f32>,
};

struct Uniforms {
    uScreenSize: vec2<f32>,
    uHasTexture: i32,
    _padding: i32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var uTexture: texture_2d<f32>;
@group(0) @binding(2) var uSampler: sampler;

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    output.fsColor = input.color;
    
    let ndc = (input.position / uniforms.uScreenSize) * 2.0 - vec2<f32>(1.0, 1.0);
    output.gl_Position = vec4<f32>(ndc.x, -ndc.y, 0.0, 1.0);

    output.v_TexCoord = input.tex_coord;
    output.v_Position = input.position;
    output.v_Size = input.size;
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    if (input.v_Position.x < input.v_Size.x || input.v_Position.x > input.v_Size.y ||
        input.v_Position.y < input.v_Size.z || input.v_Position.y > input.v_Size.w) {
        return vec4<f32>(0.0, 0.0, 0.0, 0.0);
    }

    if (uniforms.uHasTexture == 2) {
        return textureSampleLevel(uTexture, uSampler, input.v_TexCoord, 0.0);
    }

    if (uniforms.uHasTexture == 1) {
        let sampledAlpha = textureSampleLevel(uTexture, uSampler, input.v_TexCoord, 0.0).a;
        return vec4<f32>(input.fsColor.rgb, input.fsColor.a * sampledAlpha);
    }

    return input.fsColor;
}
