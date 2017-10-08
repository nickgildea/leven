#ifndef		__GLSL_PROGRAM_H__
#define		__GLSL_PROGRAM_H__

#include "sdl_wrapper.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <hash_map>

class GLSLProgram
{
public:

	GLSLProgram();
	~GLSLProgram();

	bool initialise();
	void prependLine(const std::string& line);

	bool compileShader(const GLenum type, const std::string& filePath);
	bool link();

	GLuint getId() const;
	bool getUniform(const std::string& name, glm::mat4& uniform);
	bool getSubroutineIndex(const std::string& name, GLuint& uniform);

	bool setUniform(const std::string& name, const glm::mat4& uniform);
	bool setUniform(const std::string& name, const glm::mat3& uniform);
	bool setUniform(const std::string& name, const glm::vec4& uniform);
	bool setUniform(const std::string& name, const glm::vec3& uniform);

	// NB: overloading deliberately avoided here to prevent problems with literals
	// being converted, e.g. with an integer setUniform func setUniform("bob", 1.f)
	// will call the variant, which can be very confusing (although it does generate a warning)
	bool setUniformFloat(const std::string& name, const float uniform);
	bool setUniformInt(const std::string& name, const GLuint uniform);

private:

	void printShaderInfo(GLuint shader) const;
	void printProgramInfo(GLuint program) const;

	GLuint					program_;
	std::vector<GLuint>		shaders_;
	std::string				header_;
	std::string				filePath_;

	// Hash the locations of the uniforms to prevent glGet calls during frames
	typedef std::hash_map<std::string, GLint> UniformLocations;
	UniformLocations		uniformLocations_;

	const GLint getUniformLocation(const std::string& name);
};

struct GLSLProgramView
{
	const GLSLProgram& renderProgram_;

	GLSLProgramView(const GLSLProgram* const p)
		: renderProgram_(*p)
	{
		glUseProgram(renderProgram_.getId());
//		printf("use(%d)\n", renderProgram_.getId());
	}

	~GLSLProgramView()
	{
		glUseProgram(0);
//		printf("use(0)\n");
	}
};

#endif	//	__GLSL_PROGRAM_H__

