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

//////////////////////////////////////////////////////////////////////////////////////

struct Ray
{
	vec3 origin;
	vec3 dir;
};

struct Surface
{
	bool hit;
	vec3 pos;
	vec3 normal;
	vec4 colour;
};

Ray reflect_ray(Ray ray, Surface sf)
{
	vec3 dir = reflect(ray.dir, sf.normal);
	return Ray(sf.pos + (dir * 0.0001), dir);
}

struct Sphere
{
	vec3 center;
	float radius;
};

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

vec4 background_colour(Ray r, vec3 l)
{
	float sun = max(0, dot(r.dir, l));
	vec3 sky = vec3(0.5, 0.6, 0.7) * max(0, -dot(r.dir, vec3(0,1,0)));
	return vec4(sky + (pow(sun, 256) + 0.2 * pow(sun, 2)) * vec3(2.0, 1.6, 1.0), 1);
}

//////////////////////////////////////////////////////////////////////////////////////

Surface get_surface(Ray r)
{
	const float epsilon = 0.0000001;
	float tnearest = 3.402823466e+38; //float max
	float t = 0;

	Surface sf;
	sf.hit = false;

	Sphere s = Sphere(vec3(0, -0.6, 2), 0.5);
	t = hit_sphere(s, r);
	if (t > epsilon && t < tnearest)
	{
		tnearest = t;
		sf.hit = true;
		sf.pos = r.origin + (r.dir * t);
		sf.normal = normalize(sf.pos - s.center);
		sf.colour = vec4(0.8, 0, 0.5, 1);
	}

	t = hit_plane(r);
	if (t > epsilon && t < tnearest)
	{
		tnearest = t;
		sf.hit = true;
		sf.pos = r.origin + (r.dir * t);
		sf.normal = vec3(0, -1, 0);
		sf.colour = chess_pattern(sf.pos.xz);
	}

	return sf;
}

vec4 shade_surface(Ray r, Surface sf, vec4 inColour, vec3 light, vec4 backgroundColour)
{
	//attenuate
	const float fogStart = 15;
	float dist = distance(r.origin, sf.pos);
	float visibility = (dist > fogStart) ? exp(-(dist - fogStart) * 0.1) : 1.0f;
	//visible threshold
	if (visibility < 0.0001)
	{
		return backgroundColour;
	}

	//Check if surface is visible from light
	float shadow = get_surface(Ray(sf.pos + (light * 0.0001), light)).hit ? 0 : 1;

	float ambient = 0.2;
	float diffuse = max(dot(sf.normal, light), 0);
	float spec = 0.5 * pow(max(dot(r.dir, reflect(light, sf.normal)), 0), 32);

	vec4 surfaceColour = vec4((ambient + (spec + diffuse * shadow)) * sf.colour.xyz, 1);

	//combine with incoming colour
	surfaceColour = mix(surfaceColour, inColour, 0.2);

	return mix(backgroundColour, surfaceColour, visibility);
}

vec4 trace_scene(Ray r)
{
	vec3 light = normalize(vec3(sin(0.001*u.time), -0.5, cos(0.001*u.time)));
	vec4 backgroundColour = background_colour(r, light);

	Surface sf = get_surface(r);

	if (sf.hit)
	{
		Ray r2 = reflect_ray(r, sf);
		Surface sf2 = get_surface(r2);

		vec4 reflectedBackground = background_colour(r2, light);
		vec4 reflectedColour = reflectedBackground;
		if (sf2.hit)
		{
			reflectedColour = shade_surface(r2, sf2, vec4(1,1,1,1), light, reflectedBackground);
		}
		return shade_surface(r,  sf,  reflectedColour, light, backgroundColour);
	}
	else
	{
		return backgroundColour;
	}
}

void main()
{
	Ray r;
	r.origin = ray_origin;
	r.dir = normalize(ray_dir);
	colour = trace_scene(r);
}
