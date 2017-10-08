#ifndef		HAS_OPTIONS_H_BEEN_INCLUDED
#define		HAS_OPTIONS_H_BEEN_INCLUDED

class Options
{
public:

	static Options& get()
	{
		static Options instance;
		return instance;
	}

	float meshMaxError_ = 5.f;
	float meshMaxEdgeLen_ = 2.5f;
	float meshMinCosAngle_ = 0.7f;

private:

	Options()
	{
	}
};

#endif		HAS_OPTIONS_H_BEEN_INCLUDED
