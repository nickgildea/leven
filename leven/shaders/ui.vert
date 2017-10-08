layout(location=0) in vec3 position;
layout(location=1) in vec2 texCoord;

uniform mat4 MVP;

smooth out vec2 vs_texCoord;

void main()
{
	gl_Position = MVP * vec4(position, 1);
	vs_texCoord = texCoord;
}