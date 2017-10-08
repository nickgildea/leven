// ---------------------------------------------------------------------------

#include "cl/hg_sdf.glsl"

//#define NOISE_SCALE (1.f / 96.f)
#define NOISE_SCALE (1.f)

float BasicFractal(
	read_only image2d_t permTexture,
	const int octaves,
	const float frequency,
	const float lacunarity,
	const float persistence,
	float2 position)
{
	float2 p = position * NOISE_SCALE;
	float noise = 0.f;

	float amplitude = 1.f;
	p *= frequency;

	for (int i = 0; i < octaves; i++)
	{
		noise += snoise2(p, permTexture) * amplitude;
		p *= lacunarity;	
		amplitude *= persistence;	
	}

	// move into (0, 1) range
#if 1
	return noise;
#else
	const float remapped = 0.5f + (0.5f * noise);
	return remapped;
#endif
}

// ---------------------------------------------------------------------------

#define RIDGED_MULTI_H (1.f)

float RidgedMultiFractal(
	read_only image2d_t permTexture,
	const int octaves,
	const float lacunarity,
	const float gain,
	const float offset,
	float2 position)
{
	float2 p = position * NOISE_SCALE;
	
	float signal = snoise2(p, permTexture);
	signal = fabs(signal);
	signal = offset - signal;
	signal *= signal;

	float noise = signal;
	float weight = 1.f;
	float frequency = 1.f;

	for (int i = 0; i < octaves; i++)
	{
		p *= lacunarity;

		weight = signal * gain;
		weight = clamp(weight, 0.f, 1.f);

		signal = snoise2(p, permTexture);
		signal = fabs(signal);
		signal = offset - signal;
		signal *= weight;

		const float exponent = pow(frequency, -1.f * RIDGED_MULTI_H);
		frequency *= lacunarity;

		noise += signal * exponent;
	}

	noise *= (1.f / octaves);
	return noise; 
}

// ---------------------------------------------------------------------------

float2 RotateY(const float2 v, const float angle)
{
	float2 result = v;
	const float s = sin(radians(angle));
	const float c = cos(radians(angle));
	result.x =  v.x * c +  v.y * s;
	result.y = -v.x * s +  v.y * c;

	return result;
}

// ---------------------------------------------------------------------------

// polynomial smooth min (k = 0.1);
float smin( float a, float b, float k )
{
    float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
    return mix( b, a, h ) - k*h*(1.0-h);
}

float Cube4(const float3 world_pos, const float3 dimensions)
{
	const float3 d = fabs(world_pos) - dimensions;
	const float m = max(d.x, max(d.y, d.z));
	return min(m, length(max(d.xyz, (float3)(0.f))));
}

float2 octahedron(float3 p, float r) {
  float3 o = (fabs(p)/sqrt(3.f));
  float s = (o.x+o.y+o.z);
  return (float2)((s-(r*(2.f/sqrt(3.f)))),1.f);
}
 
float3 opTwist(float3 p) {
  float t = 3.f;
  float c = cos((t*p.y));
  float s = sin((t*p.y));
  float2 mp = { c * p.x - s * p.z, s * p.x + c * p.z };
  float3 q = (float3)(mp.x, mp.y, p.y);
  return q;
}
 
float paniq(float3 p) {
  p = opTwist(p);
  float2 o = octahedron(p,0.45f);
  float m = min(o.x, o.y);
  return max(m, (0.6f-length(p)));
}

float decocube(float4 p) {
	p = p * p;
    float r = (0.8f*0.8f);
    float a = ((p.x+p.y)-r);
    float b = ((p.y+p.z)-r);
    float c = ((p.z+p.x)-r);
    float4 u = (p-1.f);
    float q = ((a*a)+(u.z*u.z));
    float s = ((b*b)+(u.x*u.x));
    float t = ((c*c)+(u.y*u.y));
    return ((q*s*t)-0.04f);
}

float teardrop(float4 p)
{
	float x4 = p.x * p.x * p.x * p.x;
	float x5 = p.x * x4;
	float y2 = p.y * p.y;
	float z2 = p.z * p.z;

	return (0.5f * (x5 + x4)) - y2 - z2;
}

float tangle(float3 p)
{
	float a = p.x * p.x * p.x * p.x;
	float b = -5.f * (p.x * p.x);
	float c = p.y * p.y * p.y * p.y;
	float d = -5.f * (p.y * p.y);
	float e = p.z * p.z * p.z * p.z;
	float f = -5.f * (p.z * p.z);
	float g = 11.8f;
	return a + b + c + d + e + f + g;
}

float Sphere(const float4 world_pos, const float4 origin, const float radius)
{
	return length((world_pos - origin).xyz) - radius;
}


float Cuboid(const float4 world_pos, const float4 origin, const float4 dimensions)
{
	const float4 local_pos = world_pos - origin;
	const float4 pos = local_pos;

	const float4 d = fabs(pos) - dimensions;
	const float m = max(d.x, max(d.y, d.z));
	return min(m, length(max(d, (float4)(0.f))));
}


float TestObject(const float4 p)
{
	float4 sDim = { 7.f, 2.f, 20.f, 0.f };
	float density = FLT_MAX;

	for (int i = 0; i < 10; i++)
	{
		float2 r = RotateY(p.xz, 0 * 15.f);
		float d = Cuboid((float4)(r.x, p.y, r.y, 0.f), (float4)(i * 14.f, i * 4.f, 0.f, 0.f), sDim);
		density = min(density, d);
	}

	float base = Cuboid(p, (float4)(0.f, 100.f, 0.f, 0.f), (float4)(100, 20, 100, 0));
	density = min(density, base);

	return density;
}


float Terrain(const float4 position, read_only image2d_t permTexture)
{
	float2 p = position.xz * (1.f / 2000.f);

	float ridged = 0.8f * RidgedMultiFractal(permTexture, 7, 2.114352f, /*gain=*/1.5241f, /*offset=*/1.f, p.xy);
	ridged = clamp(ridged, 0.f, 1.f); 

	float billow = 0.6f * BasicFractal(permTexture, 4, 0.24f, 1.8754f, 0.433f, (float2)(-4.33f, 7.98f) * p.xy);
	billow = (0.5f * billow) + 0.5f;

	float noise = billow * ridged;

	float b2 = 0.6f * BasicFractal(permTexture, 2, 0.63f, 2.2f, 0.15f, p.xy);
	b2 = (b2 * 0.5f) + 0.5f;
	noise += b2;

//	return 0.1;
	return noise;
}

float DensityFunc(const float4 position, read_only image2d_t permTexture)
{
#if 0 
	float3 p = (float3)(position.x, position.y, position.z);
	p -= (float3)(0, 100.f, 0.f);

//	return paniq(p / 64.f) * 64.f;


//	pMod3(&p, 30);
	{
		float2 xz = p.xz;
		pR(&xz, PI / 4.6343f);
		p.xz = xz;
	}

	float size = 40;
	float box = fBox(p, (float3)(size, size, size / 2));
//	p -= (float3)(0, 0, 40);
	p -= (float3)(0, 10, 0);
	float box2 = fBox(p, (float3)(size, size - 10, size));
//	float sphere = fSphere(p, 22);

//	float d = fOpUnionRound(box, sphere, 3.3);
//	float d = fOpUnionRound(box, box2, 4);
//	float d = min(box, box2);
//	float d = fOpIntersectionStairs(box, box2, 6, 10);
	float d = fOpUnionStairs(box, box2, 1, 2.5);
	return d;

	float t = fTorus(p, 8.f, 54.f);
	float c = fCapsule(p, 9.f, 16.f);
	p -= (float3)(0, 20.f, 20.f);
	float cone = fCone(p, 13.f, 19.f);
	p -= (float3)(0, 0, -50.f);
	float h = fHexagonCircumcircle(p, (float2)(8.f, 13.f));
	return min(t, min(h, min(cone, c)));

//	return Cuboid(position, (float4)(100.f, 500.f, 100.f, 0.f), (float4)(50.f, 0.f, 50.f, 0.f));
#else
	float noise = Terrain(position, permTexture);
	return position.y - (MAX_TERRAIN_HEIGHT * noise);
#endif
}



