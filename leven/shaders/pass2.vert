layout(location=0) in vec3 position;

uniform mat4 modelToWorldMatrix;
uniform mat4 worldToCameraMatrix;
uniform mat4 projectionMatrix;

smooth out vec3 viewray;

void main()
{
	gl_Position = vec4(position, 1);
	mat4 invP = inverse(projectionMatrix);
	viewray = (invP * vec4(position, 1)).xyz;
}

