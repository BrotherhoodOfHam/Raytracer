/*
	Raytracer
*/

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 ray_origin;
layout(location = 1) in vec3 ray_dir;
layout(location = 0) out vec4 colour;

layout(binding = 0)
uniform Uniforms
{
	mat4 camera;
	float time;
} u;

struct Ray
{
	vec3 origin;
	vec3 dir;
};

struct Sphere
{
	vec3 center;
	float radius;
};

float intersect_sphere(Sphere s, Ray ray)
{
	vec3 oc = ray.origin - s.center;
	float a = dot(ray.dir, ray.dir);
	float b = 2.0f * dot(oc, ray.dir);
	float c = dot(oc, oc) - (s.radius * s.radius);

	//discriminant = b^2 - 4ac
	float dsc = b*b - (4 * a * c);
	if (dsc < 0)
	{
		return -1;
	}

	//solve quadratic formula
	return (-b - sqrt(dsc)) / (2 * a);
}

float intersect_plane(Ray r)
{
	vec3 c = vec3(0,0,0);
	vec3 n = vec3(0, 1, 0);
	return dot(c - r.origin, n) / dot(r.dir, n);
}

vec4 chessPattern(vec2 pos)
{
	int a = int(round(pos.x)) + int(round(pos.y));
	return (a % 2 == 0) ?
		vec4(1, 0.2, 0, 1) :
		vec4(1, 1, 1, 1);
}

vec4 compute_ray(Ray r)
{
	const float epsilon = 0.00000001;
	vec4 backgroundColour = vec4(0.05, 0.05, 0.05, 1.0);

	Sphere s;
	s.center = (vec4(0, -0.6, 2, 1)).xyz;
	s.radius = 0.5;

	float t = intersect_sphere(s, r);
	if (t > epsilon)
	{
		vec3 p = r.origin + (r.dir * t);
		vec3 n = normalize(p - s.center);
		vec3 l = normalize(vec3(1, -1, -1));
		float vis = dot(n, l);

		//modulate colour
		float f = 0.5 * (1 + sin(u.time / 1000));
		
		return vec4(vis * vec3(0, 1 - f, f), 1);
	}

	t = intersect_plane(r);
	if (t > epsilon)
	{
		const float fogStart = 15;
		vec3 p = r.origin + (r.dir * t);

		float dist = distance(r.origin, p);
		float visibility = (dist > fogStart) ? exp(-(dist - fogStart) * 0.1) : 1.0f;

		if (visibility > epsilon)
		{
			return mix(backgroundColour, chessPattern(p.xz), visibility);
		}
	}

	//no hit
	return backgroundColour;
}

void main()
{
	Ray r;
	r.origin = ray_origin.xyz;
	r.dir = normalize(ray_dir);
	colour = compute_ray(r);
}
