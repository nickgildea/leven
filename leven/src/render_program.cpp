#include "render_program.h"

#include "log.h"
#include "file_utils.h"

GLSLProgram::GLSLProgram()
	: program_(0)
{
}

GLSLProgram::~GLSLProgram()
{
	if (program_ > 0)
	{
		glDeleteProgram(program_);
	}
}

bool GLSLProgram::initialise()
{
	program_ = glCreateProgram();
	prependLine("#version 420");
	return true;
}

void GLSLProgram::printShaderInfo(GLuint shader) const
{
	int maxLength = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

	static char buffer[2048];
	int length = 0;
	glGetShaderInfoLog(shader, maxLength, &length, buffer);

	LogPrintf("  shader (%s): %s\n", filePath_.c_str(), buffer);
}

void GLSLProgram::printProgramInfo(GLuint program) const
{
	int maxLength = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

	static char buffer[2048];
	int length = 0;
	glGetProgramInfoLog(program, maxLength, &length, buffer);

	LogPrintf("  program (%s): %s\n", filePath_.c_str(), buffer);
}

void GLSLProgram::prependLine(const std::string& line)
{
	header_ += line;
	header_ += "\n";
}

bool GLSLProgram::compileShader(const GLenum type, const std::string& filePath)
{
	std::string data;
	if (!LoadTextFile(filePath.c_str(), data))
	{
		return false;
	}

	filePath_ = filePath;

	const GLchar* inputCode[] = { header_.c_str(), data.c_str() };

	const GLuint shader = glCreateShader(type);
	glShaderSource(shader, 2, inputCode, NULL);
	glCompileShader(shader);

	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	printShaderInfo(shader);
	if (status == GL_FALSE)
	{
		return false;
	}

	glAttachShader(program_, shader);
	shaders_.push_back(shader);

	return true;
}
	

bool GLSLProgram::link()
{
	if (shaders_.empty())
	{
		return true;
	}

	glLinkProgram(program_);
	if (GLenum err = glGetError() != GL_NO_ERROR)
	{
		printf("GLSLProgram: err=%d\n", err);
		return false;
	}

	printProgramInfo(program_);

	GLint status = 0;
	glGetProgramiv(program_, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		return false;
	}

	for (size_t i = 0; i < shaders_.size(); i++)
	{
		glDetachShader(program_, shaders_[i]);
		glDeleteShader(shaders_[i]);
	}

	shaders_.clear();

	return true;
}

GLuint GLSLProgram::getId() const
{
	return program_;
}

const GLint GLSLProgram::getUniformLocation(const std::string& name)
{
	const auto iter = uniformLocations_.find(name);	
	if (iter == end(uniformLocations_))
	{
		const GLint location = glGetUniformLocation(program_, name.c_str());
		uniformLocations_[name] = location;
	}

	return uniformLocations_[name];
}

bool GLSLProgram::getSubroutineIndex(const std::string& name, GLuint& uniform)
{
	// TODO hardcoded to fragment shaders
	uniform = glGetSubroutineIndex(program_, GL_FRAGMENT_SHADER, name.c_str());
	return uniform != GL_INVALID_INDEX;
}


bool GLSLProgram::setUniform(const std::string& name, const glm::mat4& uniform)
{
	const GLint location = getUniformLocation(name);
	if (location == -1)
	{
		return false;
	}

	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(uniform));
	return true;
}

bool GLSLProgram::setUniform(const std::string& name, const glm::mat3& uniform)
{
	const GLint location = getUniformLocation(name);
	if (location == -1)
	{
		return false;
	}

	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(uniform));
	return true;
}

bool GLSLProgram::setUniform(const std::string& name, const glm::vec4& uniform)
{
	const GLint location = getUniformLocation(name);
	if (location == -1)
	{
		return false;
	}

	glUniform4fv(location, 1, glm::value_ptr(uniform));
	return true;
}

bool GLSLProgram::setUniform(const std::string& name, const glm::vec3& uniform)
{
	const GLint location = getUniformLocation(name);
	if (location == -1)
	{
		return false;
	}

	glUniform3fv(location, 1, glm::value_ptr(uniform));
	return true;
}

bool GLSLProgram::setUniformFloat(const std::string& name, const float uniform)
{
	const GLint location = getUniformLocation(name);
	if (location == -1)
	{
		return false;
	}

	glUniform1f(location, uniform);
	return true;
}


bool GLSLProgram::setUniformInt(const std::string& name, const GLuint uniform)
{
	const GLint location = getUniformLocation(name);
	if (location == -1)
	{
		return false;
	}

	glUniform1i(location, uniform);
	return true;
}

