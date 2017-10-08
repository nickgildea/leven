layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

smooth in vec4 vs_vertexWorldPosition[];
smooth in vec4 vs_vertexColour[];
smooth in float vs_vertexDepth[];
smooth in vec4 vs_shadowPosition[];

#if DRAW_MODE == VOXEL_DRAW
flat in int vs_vertexMaterial[];
smooth in vec4 vs_vertexNormal[];
#endif

out vec4 gs_position;
out vec4 gs_colour;
out float gs_depth;
out vec4 gs_shadowPosition;

#if DRAW_MODE == VOXEL_DRAW
out vec4 gs_normal;
flat out int gs_material;
#endif

noperspective out vec3 gs_edgeDistance;

uniform mat4 u_viewportMatrix;
uniform int u_showWireframe;

void main()
{
	vec3 distances[3];
	if (u_showWireframe > 0)
	{
		const vec2 p0 = vec2(u_viewportMatrix * (gl_in[0].gl_Position / gl_in[0].gl_Position.w));
		const vec2 p1 = vec2(u_viewportMatrix * (gl_in[1].gl_Position / gl_in[1].gl_Position.w));
		const vec2 p2 = vec2(u_viewportMatrix * (gl_in[2].gl_Position / gl_in[2].gl_Position.w));

		const float a = length(p1 - p2);
		const float b = length(p2 - p0);
		const float c = length(p1 - p0);
		const float alpha = acos( (b * b + c * c - a * a) / (2.f * b * c));
		const float beta = acos( (a * a + c * c - b * b) / (2.f * a * c));
		const float ha = abs(c * sin(beta));
		const float hb = abs(c * sin(alpha));
		const float hc = abs(b * sin(alpha));

		distances[0] = vec3(ha, 0.f, 0.f);
		distances[1] = vec3(0.f, hb, 0.f);
		distances[2] = vec3(0.f, 0.f, hc);
	}
	else
	{
		distances[0] = vec3(0.f);
		distances[1] = vec3(0.f);
		distances[2] = vec3(0.f);
	}

	for (int i = 0; i < 3; i++)
	{
		gs_position = vs_vertexWorldPosition[i];
		gs_colour = vs_vertexColour[i];
		gs_depth = vs_vertexDepth[i];
		gs_shadowPosition = vs_shadowPosition[i];
#if DRAW_MODE == VOXEL_DRAW
		gs_normal = vs_vertexNormal[i];
		gs_material = vs_vertexMaterial[i];
#endif
		gs_edgeDistance = distances[i];
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}

	EndPrimitive();
}

