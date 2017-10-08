#ifndef		HAS_LRUCACHE_H_BEEN_INCLUDED
#define		HAS_LRUCACHE_H_BEEN_INCLUDED

// ----------------------------------------------------------------------------

#include	<list>
#include	<glm/glm.hpp>

template <typename ValueT, typename KeyT, int FlushFn(ValueT*), int SIZE>
class LRUCache
{
public:

	void initiailise()
	{
		for (int i = 0; i < SIZE; i++)
		{
			indices.push_front(i);
		}
	}
	
	int findItem(const KeyT& key)
	{
		for (int i = 0; i < SIZE; i++)
		{
			if (slots[i] == key)
			{
				return i;
			}
		}

		return -1;
	}

	int allocSlot(const KeyT& key)
	{
		int index = -1;
		auto iter = begin(indices);
		for (; iter != end(indices); ++iter)
		{
			index = *iter;
			if (slots[index] == key)
			{
				break;
			}
		}

		if (index != -1 && iter != end(indices))
		{
			// found the item, update the LRU list
			if (iter != begin(indices))
			{
				indices.erase(iter);
				indices.push_front(index);
			}

			return index;
		}

		// not found so replace the coldest item
		index = indices.back();
		ValueT* slot = &slots[index];

		if (FlushFn && FlushFn(slot) != 0)
		{
			printf("Error! Could not flush slot\n");
		}

		indices.pop_back();
		indices.push_front(index);
		
		*slot = ValueT();
		return index;
	}

	const ValueT* operator[](const int index) const
	{
		if (index >= 0 && index < SIZE)
		{
			return &slots[index];
		}

		return nullptr;
	}

	ValueT* operator[](const int index) 
	{
		if (index >= 0 && index < SIZE)
		{
			return &slots[index];
		}

		return nullptr;
	}


private:

	ValueT				slots[SIZE];
	std::list<int>		indices;
};

#endif	//	HAS_LRUCACHE_H_BEEN_INCLUDED
