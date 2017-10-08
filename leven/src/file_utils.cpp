#include "file_utils.h"

#include <stdio.h>
#include <fstream>
#include <sstream>

bool LoadTextFile(const std::string& path, std::string& data)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
	{
		return false;
	}

	std::stringstream fileData;
	fileData << file.rdbuf();
	file.close();

	data = fileData.str();

	return true; 
}

