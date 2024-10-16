#ifndef __CXX_OSM_PARSER1_KDTREE_H__
#define __CXX_OSM_PARSER1_KDTREE_H__

#include <stack>
#include <math.h>
#include <float.h>

namespace osm_parser1 {
using namespace std;

struct file_memory;

template <typename T> static void choose_split(T** exm_set, int size, int &split)
{
	double tmp1 = 0., tmp2 = 0.;
	for (int i = 0; i < size; ++i) {
		auto x = exm_set[i]->x();
		tmp1 += 1.0 / (double)size * x * x;
		tmp2 += 1.0 / (double)size * x;
    }
	double v1 = tmp1 - tmp2 * tmp2; //compute variance on the x dimension
	
	tmp1 = tmp2 = 0;
	for (int i = 0; i < size; ++i) {
		auto y = exm_set[i]->y();
		tmp1 += 1.0 / (double)size * y * y;
		tmp2 += 1.0 / (double)size * y;
	}
    double v2 = tmp1 - tmp2 * tmp2; //compute variance on the y dimension
	
	split = (v1 > v2) ? 0 : 1; //set the split dimension
	if (split == 0) {
		qsort(exm_set, size, sizeof(T*), T::kdtree_x_compare);
	} else{
		qsort(exm_set, size, sizeof(T*), T::kdtree_y_compare);
	}
}

template <typename T> static T* do_build_kdtree(T **exm_set, int size, file_memory* m)
{
	if (size <= 0) {
		return nullptr;
	}

	int split;
	T** exm_set_left = nullptr;
	T** exm_set_right = nullptr;

	choose_split(exm_set, size, split);

	int size_left = size / 2;
	int size_right = size - size_left - 1;

	if (size_left > 0) {
		exm_set_left = new T* [size_left];
		assert(nullptr != exm_set_left);
		memcpy(exm_set_left, exm_set, sizeof(void*) * size_left);		
	}
	if (size_right > 0) {
		exm_set_right = new T* [size_right];
		assert(nullptr != exm_set_right);
		memcpy(exm_set_right, exm_set
			+ size / 2 + 1, sizeof(void*) * size_right);
	}

	auto* ret = exm_set[size / 2];
	ret->set_split(split);
	
	// build the sub kd-tree
	ret->set_left(m, do_build_kdtree(exm_set_left, size_left, m));
	ret->set_right(m, do_build_kdtree(exm_set_right, size_right, m));

	// free temp memory
	if (exm_set_left) delete [] exm_set_left;
	if (exm_set_right) delete [] exm_set_right;
	return ret;
}

template <typename T> static int build_kdtree(T*& tree, T** table, int count, file_memory* m)
{
	if (count <= 0) {
		return 0;
	}

	// create the exam set table
	auto** exm_set = new T* [count];
	assert(nullptr != exm_set);

	// add all nodes to the array and build the kd-tree
	for (int i = 0; i < count; ++i) {
		exm_set[i] = table[i];
	}

	tree = do_build_kdtree(exm_set, count, m);
	assert(nullptr != tree);

	delete [] exm_set;
	return 0;
}

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_KDTREE_H__