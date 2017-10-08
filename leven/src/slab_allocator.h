#ifndef		HAS_SLAB_ALLOCATOR_BEEN_INCLUDED
#define		HAS_SLAB_ALLOCATOR_BEEN_INCLUDED

#include	<vector>

template <typename T, int COUNT_PER_SLAB>
class SlabAllocator
{
public:

	static const int COUNT_PER_SLAB = COUNT_PER_SLAB;

	SlabAllocator()
		: count_(COUNT_PER_SLAB)	// cause a slab to be allocated on first alloc() call
	{

	}

	~SlabAllocator()
	{
		for (T* p: slabs_)
		{
			delete[] p;
		}
	}

	void clear()
	{
		for (T* p : slabs_)
		{
			delete[] p;
		}

		slabs_.clear();
		count_ = COUNT_PER_SLAB;
	}

	T* alloc()
	{
		if (count_ == COUNT_PER_SLAB)
		{
			count_ = 0;
			slabs_.push_back(new T[COUNT_PER_SLAB]);
		}

		T* p = slabs_.back();
		return &p[count_++];
	}

	size_t size() const
	{
		return slabs_.empty() ? 0 : count_ + (slabs_.size() - 1) * COUNT_PER_SLAB;
	}

private:

	// make non-copyable
	SlabAllocator(const SlabAllocator&);
	SlabAllocator& operator=(const SlabAllocator&);

	std::vector<T*>		slabs_;
	int					count_;
};

#endif	//	HAS_SLAB_ALLOCATOR_BEEN_INCLUDED
