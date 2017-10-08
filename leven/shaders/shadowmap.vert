layout(location=0) in vec4 position;
layout(location=1) in vec4 normal;
layout(location=2) in vec4 colour;

uniform mat4 modelToWorldMatrix;
uniform mat4 worldToCameraMatrix;
uniform mat4 projectionMatrix;

void main()
{
	mat4 modelView = worldToCameraMatrix * modelToWorldMatrix;
	mat4 mvp = projectionMatrix * modelView;

	gl_Position = mvp * vec4(position.xyz, 1);
}

