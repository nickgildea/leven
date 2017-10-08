uniform vec3 lightDir;
uniform int u_useTextures;
uniform int u_showWireframe;

layout (binding=1) uniform sampler2D NormalData;
layout (binding=2) uniform sampler2D ColourData;
layout (binding=3) uniform sampler2D LinearDepthData;

layout (location=0) out vec4 FragColour;

uniform float enableLight;
uniform int showNormals;

uniform float screenWidth;
uniform float screenHeight;

// couldn't get this to work properly cameraPosition term seems wrong (I think)
#if 0
uniform vec3 cameraPosition;

float calculateFogAmount(in vec3 colour, in float depth, in vec3 rayDir)
{
	float c = 1.0;
	float b = 0.003;
	float amount = c * exp(-(cameraPosition.y / 1000.0) * b) * (1.0 - exp(-depth * rayDir.y * b)) / rayDir.y;
	return amount;
}
#endif

vec3 calculateViewRay(vec2 st)
{
	float fov = 60.0;
	float aspect = 16.0 / 9.0;
	float t = tan(fov / 2.0);	// TODO make this a uniform
	vec2 ndc = (st * 2.0) - 1.0;
	vec3 viewRay = normalize(vec3(
		ndc.x * t * aspect,
		ndc.y * t,
		1.0));
	
	return viewRay;
}

void main()
{
	vec2 st;
	st.s = gl_FragCoord.x / screenWidth;
	st.t = gl_FragCoord.y / screenHeight;

	vec3 colour = vec3(texture(ColourData, st)).rgb;

	vec3 normal = normalize((2 * texture(NormalData, st).rgb) - vec3(1));
	vec3 L = -lightDir;

	if (u_useTextures > 0)
	{
		// remap colour value
		colour = 0.3 * colour;

		// based on iq's "outdoor lighting"
#if 0
		float sun = clamp(dot(normal, L), 0.0, 1.0);
		float sky = clamp(0.5 + 0.5 * normal.y, 0.0, 1.0);
		float indirect = clamp(dot(normal, normalize(L * vec3(-1.0, 0.0, -1.0))), 0.0, 1.0);

		vec3 light = vec3(0);
		light += 0.5 * sun * vec3(1.0, 0.9, 0.8);
		light += 0.05 * sky * vec3(0.16, 0.2, 0.28);		// TODO ambient occulision factor
		light += 0.1 * indirect * vec3(0.4, 0.28, 0.2);	// TODO ditto

#else
		float diffuse = 0.9 * clamp(dot(L, normal), 0.0, 1.0);
		float ambient = 0.1 * clamp(0.5 + (0.5 * normal.y), 0.0, 1.0);
		float backLight = 0.2 * clamp(0.2 + (0.8 * dot(normalize(vec3(-L.x, 0, L.z)), normal)), 0.0, 1.0);

		vec3 sunColour = vec3(0.7, 0.6, 0.6);
		vec3 skyColour = vec3(0.2, 0.3, 0.5);
		vec3 backLightColour = vec3(0.2);

		vec3 light =  diffuse * sunColour;
		light += ambient * skyColour;
		light += backLight * backLightColour;
#endif

		// fog
		const float depth = texture(LinearDepthData, st).r;
#if 0
		const vec3 viewRay = calculateViewRay(st);
		const float fogAmount = calculateFogAmount(colour, depth, viewRay);
#else
		const float fogStrength = 0.2;
		const float fogAmount = 1.0 - exp(-depth * fogStrength);
#endif

		if (showNormals > 0)
		{
			colour = 0.2 * (0.5f + (0.5f * normal));
		//	colour = normal;
		//	colour = light;
		//	colour = vec3(depth);
		//	colour = vec3(fogAmount);
		//	colour = viewRay;
		}
	//	else 
		{
			colour *= light;
			colour = mix(colour, vec3(0.3, 0.3, 0.5), fogAmount);

			// gamma
			colour = pow(colour, vec3(1.0 / 2.2));
		}
	}
	else
	{
		colour *= clamp(dot(L, normal), 0.7, 1.0);
	}

	FragColour.xyz = colour; 
	FragColour.a = 1.0;
}
