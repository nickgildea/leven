#ifndef		HAS_POOL_ALLOCATOR_H_BEEN_INCLUDED
#define		HAS_POOL_ALLOCATOR_H_BEEN_INCLUDED

template <typename T>
class PoolAllocator
{
public:

	int initialise(const int maxItems)
	{
		LVN_ALWAYS_ASSERT("PoolAllocator already initialised", !buffer_);

		if (maxItems <= 0)
		{
			return LVN_ERR_INVALID_PARAM_SIZE;
		}

		buffer_ = new T[maxItems];
		for (int i = 0; i < (maxItems - 1); i++)
		{
			// write the freelist to the buffer elements
			T** p = (T**)&buffer_[i];
			*p = &buffer_[i + 1];
		}

		// terminate the list
		T** p = (T**)&buffer_[maxItems - 1];
		*p = (T*)-1;

		firstFree_ = buffer_;
		maxItems_ = maxItems;
		itemsAllocated_ = 0;

		return LVN_SUCCESS;
	}

	void clear()
	{
		delete[] buffer_;
		buffer_ = nullptr;
		firstFree_ = (T*)-1;
		maxItems_ = -1;
		itemsAllocated_ = -1;
	}

	u32	size() const { return itemsAllocated_; }

	T* alloc()
	{
		T* alloced = nullptr;

		if (firstFree_ != (T*)-1)
		{
			LVN_ASSERT(firstFree_ >= &buffer_[0] && firstFree_ <= &buffer_[maxItems_ - 1]);
			alloced = firstFree_;
			firstFree_ = *(T**)alloced;

			itemsAllocated_++;
		}
		
		return alloced;
	}

	int free(T* p)
	{
		if (!p || p < &buffer_[0] || p > &buffer_[maxItems_ - 1])
		{
			return LVN_ERR_INVALID_PARAM_PTR;
		}

		if (itemsAllocated_ == 0)
		{
			// TODO error?
			LVN_ASSERT(false);
			return LVN_SUCCESS;
		}
		
		*(T**)p = firstFree_;
		firstFree_ = (T*)p;

		itemsAllocated_--;

		return LVN_SUCCESS;
	}

private:

	T*				buffer_ = nullptr;
	T*				firstFree_ = nullptr;
	int				maxItems_ = -1;
	int				itemsAllocated_ = -1;
};

class IndexPoolAllocator
{
public:

	void			initialise(const int size);
	void			clear();

	u32				size() const { return allocated_; }

	int				alloc();
	void			free(int* p);

private:

	int*			pool_ = nullptr;
	int				firstFree_ = 0;
	int				size_ = 0;
	int				allocated_ = 0;
};


#endif	//	HAS_POOL_ALLOCATOR_H_BEEN_INCLUDED
