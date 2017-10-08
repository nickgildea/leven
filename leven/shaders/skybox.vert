layout(location=0) in vec4 position;
layout(location=1) in vec4 normal;
layout(location=2) in vec4 colour;

uniform mat4 MVP;

void main()
{
	vec3 p = position.xyz;
	gl_Position = MVP * vec4(p, 1);
}

