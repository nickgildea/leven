layout(binding=0) uniform sampler2D Texture;

smooth in vec2 vs_texCoord;

void main()
{
	gl_FragColor = texture2D(Texture, vs_texCoord);
}
