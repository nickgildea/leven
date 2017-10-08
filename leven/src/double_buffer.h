#ifndef		HAS_DOUBLE_BUFFER_H_BEEN_INCLUDED
#define		HAS_DOUBLE_BUFFER_H_BEEN_INCLUDED

#include	<atomic>

template <typename T>
class DoubleBuffer
{
public:

	T* current()
	{
		// explicitly copy the old buffer rather than returning a reference
		return &buffer_[counter_ & 1];
	}

	T* next()
	{
		return &buffer_[(counter_ + 1) & 1];
	}

	void increment()
	{
		counter_++;
	}

	void clear()
	{
		buffer_[0] = T();
		buffer_[1] = T();
		counter_ = 0;
	}

private:

	std::atomic<int> counter_ = 0;
	T                buffer_[2];
};

#endif		HAS_DOUBLE_BUFFER_H_BEEN_INCLUDED
