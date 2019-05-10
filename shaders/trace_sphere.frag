#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 eye_pos;
layout(location = 1) in vec4 ray_origin;
layout(location = 2) in vec3 ray_dir;

layout(location = 0) out vec4 colour;

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

	return (-b + sqrt(dsc)) / (2 * a);
}

float intersect_plane(Ray r)
{
	vec3 n = vec3(0, 1, 0);

	return dot(r.dir, n);
}

vec4 compute_ray(Ray r)
{
	Sphere s;
	s.center = vec3(0, 0, 2);
	s.radius = 0.5;

	float t = intersect_sphere(s, r);
	if (t >= 0.0)
	{
		vec3 p = r.origin + (r.dir * t);
		vec3 n = normalize(p - s.center);
		vec3 l = normalize(vec3(1, -1, 1));
		float v = dot(n, l);
		return vec4(0,0,v,1);
	}

	t = intersect_plane(r);
	if (t >= 0.0)
	{
		vec3 p = r.origin + (r.dir * t);

		//return vec4(0.5, 0.6, 0.0, 1.0);
		return vec4(0.5 + mod(p.xz, 0.1), 0.0, 1.0);
	}

	//no hit
	return vec4(abs(ray_origin.xyz), 1.0);
}

void main()
{
	Ray r;
	r.origin = ray_origin.xyz;
	r.dir = normalize(ray_dir);
	colour = compute_ray(r);
}
