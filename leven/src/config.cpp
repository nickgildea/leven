#include "config.h"

#include "log.h"

#include <fstream>
#include <sstream>
#include <vector>

bool Config_Load(Config& cfg, const std::string& filepath)
{
	std::ifstream stream(filepath);
	if (!stream.is_open())
	{
		return false;
	}

	std::string line;
	while (std::getline(stream, line))
	{
		std::stringstream ss(line);

		std::string key;
		ss >> key;

		if (key[0] == '#')
		{
			// comments! 
			continue;
		}

		if (_stricmp(key.c_str(), "WindowWidth") == 0)
		{
			ss >> cfg.windowWidth;

			// set the height incase its not specified
			if (cfg.windowHeight == -1)
			{
				cfg.windowHeight = (9 * cfg.windowWidth) / 16;
			}
		}
		else if (_stricmp(key.c_str(), "WindowHeight") == 0)
		{
			ss >> cfg.windowHeight;

			// set the width incase its not specified
			if (cfg.windowWidth == -1)
			{
				cfg.windowWidth = (16 * cfg.windowHeight) / 9;
			}
		}
		else if (_stricmp(key.c_str(), "ThreadpoolCount") == 0)
		{
			ss >> cfg.threadpoolCount;
		}
		else if (_stricmp(key.c_str(), "NoiseSeed") == 0)
		{
			ss >> cfg.noiseSeed;
		}
		else if (_stricmp(key.c_str(), "UseShadows") == 0)
		{
			std::string value;
			ss >> value;
			cfg.useShadows = _stricmp(value.c_str(), "True") == 0;
		}
		else if (_stricmp(key.c_str(), "ShadowMapSize") == 0)
		{
			ss >> cfg.shadowMapSize;
		}
		else if (_stricmp(key.c_str(), "FullScreen") == 0)
		{
			std::string value;
			ss >> value;
			cfg.fullscreen = _stricmp(value.c_str(), "True") == 0;
		}
		else
		{
			LogPrintf("Unknown key for line '%s'\n", line.c_str());			
		}
	}

	return true;
}
