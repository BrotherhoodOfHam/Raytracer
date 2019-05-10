/*
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 uv;

void main() 
{
    float x = float(((uint(gl_VertexIndex) + 2u) / 3u) % 2u); 
    float y = float(((uint(gl_VertexIndex) + 1u) / 3u) % 2u); 

    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);
    uv = vec2(x, y);
}

*/
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 eye_pos;
layout(location = 1) out vec4 ray_origin;
layout(location = 2) out vec3 ray_dir;

void main() 
{
	vec2 quad[] = vec2[]
	(
		vec2(-1.0, -1.0),
		vec2(1.0, -1.0),
		vec2(-1.0, 1.0),
		vec2(1.0, 1.0)
	);
	
	//Distance from eye to image plane
	float fov = 3.14159f / 2;
    float focalLength = 1.0f / tan(fov / 2);
	float aspectRatio = 1.0f;
	
	//Eye position
	eye_pos = vec4(0, 0, -focalLength, 1);
	//Ray origin
	ray_origin = vec4(quad[gl_VertexIndex], 0, 1);
	//Ray direction
	ray_dir = normalize((ray_origin - eye_pos).xyz);

    gl_Position = vec4(quad[gl_VertexIndex], 0, 1);
}