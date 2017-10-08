layout (location=1) out vec3 NormalData;
layout (location=2) out vec3 ColourData;
layout (location=3) out float LinearDepthData;

void main()
{
	ColourData = vec3(0.5, 0.6, 0.7);
	NormalData = vec3(1);
	LinearDepthData = 0.f;
}

