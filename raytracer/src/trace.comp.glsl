#version 450

#define MAX_OBJECTS 5

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D img;

layout(set = 0, binding = 1) uniform Uniforms
{
    // screen to world matrix
	mat4 cameraToWorld;
    // current time
	float time;
};

struct Sphere
{
	vec3  center;
	float radius;
	int   materialIndex;
};

layout(set = 0, binding = 2) uniform Scene
{
	Sphere scene[MAX_OBJECTS];
};

struct Material
{
	vec4 colour;
};

layout(set = 0, binding = 3) uniform Materials
{
	Material materials[MAX_OBJECTS];
};

//////////////////////////////////////////////////////////////////////////////////////

struct Ray
{
	vec3 origin;
	vec3 dir;
};

struct Surface
{
	int  hit;
	int  materialIndex;
	vec3 pos;
	vec3 normal;
	vec2 uv;
	vec4 colour;
};

Ray reflect_ray(Ray ray, Surface sf)
{
	vec3 dir = reflect(ray.dir, sf.normal);
	return Ray(sf.pos + (dir * 0.0001), dir);
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
	return (dsc < 0)
		? -1
		: (-b - sqrt(dsc)) / (2 * a);
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
	
	Surface sf;
	sf.hit = 0;

	for (int i = 0; i < scene.length(); i++)
	{
		Sphere obj = scene[i];
		float t = hit_sphere(obj, r);
		if (t > epsilon && t < tnearest)
		{
			tnearest = t;

			sf.hit = 1;
			sf.materialIndex = obj.materialIndex;
			sf.pos = r.origin + (r.dir * t);
			sf.normal = normalize(sf.pos - obj.center);

			float u = 0.5 + (atan(sf.normal.x, sf.normal.z) / (2 * 3.14159));
			float v = 0.5 - (asin(sf.normal.y) / 3.14159);
			sf.uv = vec2(u, v);
			sf.colour = materials[obj.materialIndex].colour;
		}
	}

	float t = hit_plane(r);
	if (t > epsilon && t < tnearest)
	{
		tnearest = t;
		sf.hit = 1;
		sf.pos = r.origin + (r.dir * t);
		sf.normal = vec3(0, -1, 0);
		sf.colour = chess_pattern(sf.pos.xz);
	}

	return sf;
}

vec4 shade_surface(Ray r, Surface sf, vec4 inColour, vec3 light, vec4 backgroundColour)
{
	//attenuate
	const float fogStart = 25;
	float dist = distance(r.origin, sf.pos);
	float visibility = (dist > fogStart) ? exp(-(dist - fogStart) * 0.1) : 1.0f;
	//visible threshold
	if (visibility < 0.0001)
	{
		return backgroundColour;
	}

	//Check if surface is visible from light
	float shadow = get_surface(Ray(sf.pos + (light * 0.0001), light)).hit == 0 ? 1.0 : 0.0;

	float ambient = 0.2;
	float diffuse = max(dot(sf.normal, light), 0);
	float spec = 0.5 * pow(max(dot(r.dir, reflect(light, sf.normal)), 0), 64);
	
	vec4 surfaceColour = vec4((ambient + ((spec + diffuse) * shadow)) * sf.colour.xyz, 1);

	//combine with incoming colour
	surfaceColour = mix(surfaceColour, inColour, 0.2);

	return mix(backgroundColour, surfaceColour, visibility);
}

vec4 trace_scene(Ray r)
{
	vec3 light = normalize(vec3(sin(0.5*time), -0.5, cos(0.5*time)));
	vec4 backgroundColour = background_colour(r, light);

	Surface sf = get_surface(r);

	if (sf.hit != 0)
	{
		Ray r2 = reflect_ray(r, sf);
		Surface sf2 = get_surface(r2);

		vec4 reflectedBackground = background_colour(r2, light);
		vec4 reflectedColour = reflectedBackground;
		if (sf2.hit != 0)
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

//////////////////////////////////////////////////////////////////////////////////////

void main()
{
    // image plane dimensions
    ivec2 dim = imageSize(img);

    // compute camera dimensions
	float vfov = 3.14159f / 3; //60 degrees (vertical fov)
	float aspectRatio = float(dim.x) / float(dim.y);
	float half_height = tan(vfov * 0.5);
	float half_width = aspectRatio * half_height;
	float focalLength = 1.0f / half_height;

    // image plane position
    vec2 image_pos = gl_GlobalInvocationID.xy / vec2(dim);   // [0,1]
    image_pos = (image_pos - vec2(0.5)) * vec2(2.0);         // [-1,1]
    image_pos = image_pos * (vec2(half_width, half_height)); // scale to camera dimensions
	
	vec3 world_pos = (cameraToWorld * vec4(image_pos, focalLength, 1)).xyz;

	Ray r;
    r.origin = (cameraToWorld * vec4(0, 0, 0, 1)).xyz; // eye position
	r.dir = normalize(world_pos - r.origin);
	
	vec4 colour = trace_scene(r);
    imageStore(img, ivec2(gl_GlobalInvocationID.xy), colour);
}

/*
void main()
{
    vec2 coords = (gl_GlobalInvocationID.xy + vec2(0.5)) / vec2(imageSize(img));
    coords = (coords - vec2(0.5)) * vec2(2.0);

    vec4 colour = vec4(sin((length(coords) * 3.159 * 10) + (4 * time)), 0, 0, 1.0);
    imageStore(img, ivec2(gl_GlobalInvocationID.xy), colour);
}
*/