layout(location=0) in vec4 position;
layout(location=1) in vec4 colour;

uniform mat4 MVP;
smooth out vec4 vertexColour;

void main()
{
	vertexColour = colour;
	gl_Position = MVP * vec4(position.xyz, 1);
}

