#include	"compute_program.h"

#include	"compute.h"
#include	"compute_local.h"
#include	"file_utils.h"

#include	<chrono>
#include	<ctime>

int ComputeProgram::build()
{
	cl::Program::Sources sources;
	std::string source;

	// need to insert the headers first, of course...
	std::vector<std::string> headerSource;
	for (const auto& path: headerPaths_)
	{
		if (!LoadTextFile(path, source))
		{
			printf("Error! Unable to load file '%s'\n", path.c_str());
			continue;
		}

		headerSource.push_back(source);
		const std::string& src = headerSource.back();
		sources.push_back(std::make_pair(src.c_str(), src.length()));
	}

	if (!LoadTextFile(filePath_, source))
	{
		printf("Error! Unable to load CL source file '%s'\n", filePath_.c_str());
		return CL_BUILD_ERROR;
	}

	const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	const std::string timestamp = std::ctime(&time);
	const std::string injection = "#define LVN_TIMESTAMP /* " + timestamp + " */\n";

//	sources.push_back(std::make_pair(injection.c_str(), injection.length()));
	sources.push_back(std::make_pair(source.c_str(), source.length()));

	// add the generate source last so it has access to all the on-disk code
	if (!generatedSource_.empty())
	{
		sources.push_back(std::make_pair(generatedSource_.c_str(), generatedSource_.length()));
	}

	auto ctx = GetComputeContext();
	cl_int error = CL_SUCCESS;
	program_ = cl::Program(ctx->context, sources, &error);
	if (!program_() || error != CL_SUCCESS)
	{
		printf("Error! Unable to create program for file '%s': %s (%d)\n",
			filePath_.c_str(), GetCLErrorString(error), error);
		return error;
	}

	std::vector<cl::Device> devices(1, ctx->device);
	error = program_.build(devices, buildOptions_.c_str());
	if (error != CL_SUCCESS)
	{
		printf("Build program failed: %s (%d)\n", GetCLErrorString(error), error);
	}

//	const std::string buildLog = program_.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ctx->device);
//	printf("--------------------------------------------------------------------------------\n"
//		"Build log for '%s':%s\n\n", filePath_.c_str(), buildLog.c_str());

	return error;
}
