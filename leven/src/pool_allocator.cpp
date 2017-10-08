#include	"pool_allocator.h"

void IndexPoolAllocator::initialise(const int size)
{
	LVN_ASSERT(!pool_);
	clear();

	pool_ = new int[size];
	for (int i = 0; i < (size - 1); i++)
	{
		pool_[i] = i + 1;
	}

	pool_[size - 1] = -1;
	firstFree_ = 0;
	size_ = size;
	allocated_ = 0;
}

void IndexPoolAllocator::clear()
{
	delete[] pool_;
	pool_ = nullptr;
	firstFree_ = -1;
	size_ = -1;
	allocated_ = -1;
}

int IndexPoolAllocator::alloc()
{
	int allocIndex = -1;

	if (firstFree_ != -1)
	{
		LVN_ASSERT(firstFree_ >= 0 && firstFree_ < size_);
		allocIndex = firstFree_;
		firstFree_ = pool_[allocIndex];
		pool_[allocIndex] = -1;

		allocated_++;
	}
	
	return allocIndex;
}

void IndexPoolAllocator::free(int* p)
{
	if (!p || *p < 0 || *p >= size_)
	{
		LVN_ASSERT(false);
		return;
	}

	pool_[*p] = firstFree_;
	firstFree_ = *p;
	*p = -1;

	allocated_--;
}
