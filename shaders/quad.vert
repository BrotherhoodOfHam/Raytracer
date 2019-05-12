/*
	Generates rays for a fullscreen quad
*/

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0)
uniform Uniforms
{
	mat4 camera;
	float time;
	uint framewidth;
	uint frameheight;
} u;

layout(location = 0) out vec4 ray_origin;
layout(location = 1) out vec3 ray_dir;

void main() 
{
	vec2 quad[] = vec2[]
	(
		vec2(-1.0, -1.0),
		vec2(1.0, -1.0),
		vec2(-1.0, 1.0),
		vec2(1.0, 1.0)
	);
	vec2 vertex = quad[gl_VertexIndex];

	//Distance from eye to image plane
	float vfov = 3.14159f / 3; //60 degrees (vertical fov)
	float aspectRatio = float(u.framewidth) / float(u.frameheight);
	float half_height = tan(vfov * 0.5);
	float half_width = aspectRatio * half_height;
	float focalLength = 1.0f / half_height;

	//Screen scale
	vec2 scale = vec2(half_width, half_height);

	//Eye position
	vec4 eye_pos = u.camera * vec4(0, 0, -focalLength, 1);
	//Ray origin
	ray_origin = u.camera * vec4(vertex * scale, 0, 1);
	//ray_origin.y = -ray_origin.y;
	//Ray direction
	ray_dir = normalize((ray_origin - eye_pos).xyz);

    gl_Position = vec4(vertex, 0, 1);
}