/*
	Raytracer
*/

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 ray_origin;
layout(location = 1) in vec3 ray_dir;
layout(location = 0) out vec4 colour;

layout(binding = 0)
uniform Uniforms
{
	mat4 camera;
	float time;
	uint framewidth;
	uint frameheight;
} u;

struct Ray
{
	vec3 origin;
	vec3 dir;
};

struct Surface
{
	vec3 pos;
	vec3 normal;
	vec3 colour;
};

struct Sphere
{
	vec3 center;
	float radius;
};

Ray reflect_ray(Ray ray, Surface s)
{
	return Ray(s.pos, reflect(ray.dir, s.normal));
}

float hit_sphere(Sphere s, Ray ray)
{
	vec3 oc = ray.origin - s.center;
	float a = dot(ray.dir, ray.dir);
	float b = 2.0f * dot(oc, ray.dir);
	float c = dot(oc, oc) - (s.radius * s.radius);
	//discriminant = b^2 - 4ac
	float dsc = b*b - (4 * a * c);
	//solve quadratic formula
	return (dsc < 0) ? -1 : (-b - sqrt(dsc)) / (2 * a);
}

float hit_plane(Ray r)
{
	vec3 c = vec3(0, 0, 0);
	vec3 n = vec3(0, -1, 0);
	return dot(c - r.origin, n) / dot(r.dir, n);
}

vec4 chess_pattern(vec2 pos)
{
	int a = int(round(pos.x)) + int(round(pos.y));
	return (a % 2 == 0) ?
		vec4(1, 0.2, 0, 1) :
		vec4(1, 1, 1, 1);
}

vec3 background_colour(Ray r, vec3 l)
{
	float sun = max(0, dot(r.dir, l));
	vec3 sky = vec3(0.5, 0.6, 0.7) * max(0, -dot(r.dir, vec3(0,1,0)));
	return sky + (pow(sun, 256) + 0.2 * pow(sun, 2)) * vec3(2.0, 1.6, 1.0);
}

//////////////////////////////////////////////////////////////////////////////////////
vec4 compute_ray2(Ray r)
{
	const float epsilon = 0.00000001;
	vec3 light = normalize(vec3(1, -1, -1));
	vec4 backgroundColour = vec4(background_colour(r, light), 1);

	float t = hit_plane(r);
	if (t > epsilon)
	{
		const float fogStart = 15;
		vec3 p = r.origin + (r.dir * t);

		float dist = distance(r.origin, p);
		float visibility = (dist > fogStart) ? exp(-(dist - fogStart) * 0.1) : 1.0f;

		if (visibility > epsilon)
		{
			return mix(backgroundColour, chess_pattern(p.xz), visibility);
		}
	}
	//no hit
	return backgroundColour;
}

vec4 compute_ray3(Ray r)
{
	const float epsilon = 0.00000001;
	vec3 light = normalize(vec3(1, -1, -1));
	vec4 backgroundColour = vec4(background_colour(r, light), 1);

	Sphere s = Sphere(vec3(0, -0.6, 2), 0.5);
	float t = hit_sphere(s, r);
	if (t > epsilon)
	{
		vec3 p = r.origin + (r.dir * t);
		vec3 n = normalize(p - s.center);
		//modulate colour
		float f = 0.5 * (1 + sin(u.time / 1000));
		vec3 colour = vec3(0, 1 - f, f);
		
		//lighting
		float ambient = 0.2;
		float diffuse = max(dot(n, light), 0);
		float spec = 0.5 * pow(max(dot(r.dir, reflect(light, n)), 0), 32);
		return vec4((ambient + spec + diffuse) * colour, 1);
	}
	//no hit
	return backgroundColour;
}
//////////////////////////////////////////////////////////////////////////////////////

vec4 compute_ray(Ray r)
{
	const float epsilon = 0.00000001;
	vec3 light = normalize(vec3(1, -1, -1));
	vec4 backgroundColour = vec4(background_colour(r, light), 1);

	Sphere s = Sphere(vec3(0, -0.6, 2), 0.5);
	float t = hit_sphere(s, r);
	if (t > epsilon)
	{
		vec3 p = r.origin + (r.dir * t);
		vec3 n = normalize(p - s.center);
		//modulate colour
		float f = 0.5 * (1 + sin(u.time / 1000));
		vec3 colour = vec3(0, 1 - f, f);
		vec3 colour2 = compute_ray2(Ray(p, reflect(r.dir, n))).xyz;
		colour = mix(colour, colour2, 0.2);
		
		//lighting
		float ambient = 0.2;
		float diffuse = max(dot(n, light), 0);
		float spec = 0.5 * pow(max(dot(r.dir, reflect(light, n)), 0), 32);
		return vec4((ambient + spec + diffuse) * colour, 1);
	}

	t = hit_plane(r);
	if (t > epsilon)
	{
		const float fogStart = 15;
		vec3 p = r.origin + (r.dir * t);
		vec3 n = vec3(0, -1, 0);
		float dist = distance(r.origin, p);
		float visibility = (dist > fogStart) ? exp(-(dist - fogStart) * 0.1) : 1.0f;

		vec4 colour2 = compute_ray3(Ray(p, reflect(r.dir, n)));

		if (visibility > epsilon)
		{
			vec4 colour = chess_pattern(p.xz);
			colour = mix(colour, colour2, 0.4);
			return mix(backgroundColour, colour, visibility);
		}
	}

	//no hit
	return backgroundColour;
}

void main()
{
	/*
	for (uint i = 0; i < 4; i++)
	{
		uint a = i % 2;
		uint b = i / 2;
	}
	*/
	Ray r;
	r.origin = ray_origin;
	r.dir = normalize(ray_dir);
	colour = compute_ray(r);
}
