#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D img;

layout(set = 0, binding = 1) uniform Uniforms
{
	mat4 camera;
	float time;
};

void main()
{
    vec2 coords = (gl_GlobalInvocationID.xy + vec2(0.5));// / vec2(imageSize(img));
    coords = (coords - vec2(0.5)) * vec2(2.0);
    
    vec4 colour = vec4(sin(length(coords) * 100 * time), 0, 0, 1.0);
    imageStore(img, ivec2(gl_GlobalInvocationID.xy), colour);
}
