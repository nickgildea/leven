#ifndef		HAS_CONFIG_H_BEEN_INCLUDED
#define		HAS_CONFIG_H_BEEN_INCLUDED

#include	<string>

struct Config
{
	Config()
		: windowWidth(-1)
		, windowHeight(-1)
		, fullscreen(false)
		, threadpoolCount(6)
		, gridSize(0)
		, noiseSeed(0)
		, useShadows(true)
		, shadowMapSize(2048)
	{
	}

	int			windowWidth, windowHeight;
	bool		fullscreen;

	int			threadpoolCount;

	int			gridSize;
	int			noiseSeed;

	bool		useShadows;
	int			shadowMapSize;
};

bool Config_Load(Config& cfg, const std::string& filepath);

#endif	//	HAS_CONFIG_H_BEEN_INCLUDED