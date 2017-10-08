#ifndef		HAS_TIMER_H_BEEN_INCLUDED
#define		HAS_TIMER_H_BEEN_INCLUDED

#include	<chrono>
#include	<string>
#include	<stdio.h>

class Timer
{
public:

	Timer()
		: enabled_(true)
	{

	}

	typedef std::chrono::high_resolution_clock Clock;

	void start()
	{
		start_ = Clock::now();
	}

	Clock::duration end()
	{
		return Clock::now() - start_;
	}

	static unsigned int ElapsedTimeMS(Timer& timer)
	{
		auto elapsedTime = timer.end();
		return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count();
	}

	unsigned int elapsedMilli()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(end()).count();
	}

	unsigned int elapsedMicro()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(end()).count();
	}

	void printElapsed(const std::string& str)
	{
		if (enabled_)
		{
			printf("  %s: %d ms\n", str.c_str(), elapsedMilli());
			start();
		}
	}

	void enable() { enabled_ = true; }
	void disable() { enabled_ = false; }

private:

	bool enabled_;
	Clock::time_point start_;
};

#endif	//	HAS_TIMER_H_BEEN_INCLUDED