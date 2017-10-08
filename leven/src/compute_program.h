#ifndef		HAS_COMPUTE_PROGRAM_H_BEEN_INCLUDED
#define		HAS_COMPUTE_PROGRAM_H_BEEN_INCLUDED

#include	<string>
#include	<vector>
#include	<CL/cl.hpp>

class ComputeProgram
{
public:

	void initialise(const std::string& filePath, const std::string& buildOptions)
	{
		filePath_ = filePath;
		buildOptions_ = buildOptions;

		headerPaths_.clear();
		generatedSource_ = "";
		program_ = cl::Program();
	}

	void addHeader(const std::string& headerPath)
	{
		headerPaths_.push_back(headerPath);
	}
	
	void setGeneratedSource(const std::string& generatedSource)
	{
		generatedSource_ = generatedSource;
	}

	int build();

	cl::Program get() const
	{
		// use the value semantics rather than returning a ref
		return program_;
	}
	
private:

	std::string					filePath_;
	std::string					buildOptions_;
	std::vector<std::string>	headerPaths_;
	std::string					generatedSource_;
	cl::Program					program_;
};

#endif	//	HAS_COMPUTE_PROGRAM_H_BEEN_INCLUDED

