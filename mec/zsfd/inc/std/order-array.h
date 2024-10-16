/** @file order-array.h
 * definition of an ordered array and its implmentation
 */

#ifndef __CXX_ZAS_STD_ORDER_ARRAY_H__
#define __CXX_ZAS_STD_ORDER_ARRAY_H__

#include "zasbsc.h"

namespace zas {

template <typename T> class ordered_array
{
	struct array_item
	{
		int order;
		T object;
	};

public:
	ordered_array(int minitems)
	: _minitems(minitems)
	, _total_items(0)
	, _cur_items(0)
	, _array(NULL)
	, _ordered(0) {
		assert(minitems != 0);
	}

	~ordered_array() {
		if (_array) delete [] _array;
	}

	void add(int orderid, const T& obj)
	{
		if (_cur_items == _total_items)
		{
			array_item* newptr = new array_item [_total_items + _minitems];
			assert(NULL != newptr);

			if (_cur_items) {
				memcpy(newptr, _array, sizeof(array_item) * _cur_items);
				delete [] _array;
			}
			_array = newptr;
			_total_items += _minitems;
		}

		array_item& item = _array[_cur_items++];
		item.order = orderid;
		item.object = obj;

		_ordered = 0;
	}

	T* get(int index)
	{
		int mid;
		int start = 0;
		int end = _cur_items - 1;

		if (!_cur_items) return NULL;
		if (!_ordered) {
			qsort(&_array, _cur_items, sizeof(array_item), compare);
			_ordered = 1;
		}

		// find the item
		while (start <= end)
		{
			mid = (start + end) / 2;
			array_item* node = &_array[mid];

			if (index == node->order)
				return &node->object;
			else if (index > node->order) start = mid + 1;
			else end = mid - 1;
		}
		return NULL;
	}

	int count(void) {
		return _cur_items;
	}

	T* operator[](int idx)
	{
		if (idx >= _cur_items) {
			return NULL;
		}
		return &_array[idx].object;
	}

private:

	static int compare(const void* a, const void* b)
	{
		array_item* aa = (array_item*)a;
		array_item* bb = (array_item*)b;
		if (aa->order == bb->order) return 0;
		else if (aa->order < bb->order) return -1;
		else return 1;
	}
	
private:
	int _minitems;
	int _total_items;
	int _cur_items;
	array_item* _array;
	uint32_t _ordered : 1;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ordered_array);
};

} // end of namespace zas
#endif // __CXX_ZAS_STD_ORDER_ARRAY_H__
/* EOF */

