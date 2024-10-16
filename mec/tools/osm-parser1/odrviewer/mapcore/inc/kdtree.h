/** @file kdtree.cpp
 */

#ifndef __CXX_ZAS_MAPCORE_KDTREE_BASE_H__
#define __CXX_ZAS_MAPCORE_KDTREE_BASE_H__

#include <stack>
#include <math.h>
#include <float.h>

namespace zas {
namespace mapcore {

using namespace std;

template <typename T> static T*
kdtree2d_search(T* kdtree, double x, double y, double &dist)
{
    double d = DBL_MAX;
    T* nearest;
	T* s = kdtree;
	stack<T*> search_path;
        
    while(s != nullptr) {
		search_path.push(s);
		if (!s->get_split()) {
			s = (x <= s->x()) ? s->left : s->right;
		} else {
			s = (y <= s->y()) ? s->left : s->right;
		}
	}

	// get the last node in search path as the "nearest"
	nearest = search_path.top();
	search_path.pop();
	if (!nearest->KNN_selected()) {
		d = nearest->distance(x, y);     
	}

    // re-walk the search path
    T* back;
	while (search_path.size()) {
		// get the last node in search path for back
        back = search_path.top();    
        search_path.pop();

		double tmp = (back->KNN_selected())
			? DBL_MAX : back->distance(x, y);
		
		// if the back is a leaf
		if (!back->left && !back->right) {	
			if (d > tmp) {
				nearest = back; d = tmp;
			}
			continue;
		}

		int split = back->get_split();
		if (split == 0) {
			// if the circle with <y, x> as center point and d as radius be intersected
			// with the other sub space, move to the other sub space for searching
			if (fabs(back->x() - x) < d) {
				if (d > tmp) {
					nearest = back; d = tmp;
				}

				// if the <y,x> is located at left of <back>, move to right
				// sub space for searching
				s = (x <= back->x()) ? back->right : back->left;
				if (s) search_path.push(s);
			}
		} else {
			if (fabs(back->y() - y) < d) {
				if (d > tmp) {
					nearest = back; d = tmp;
				}  
				// see comments above
				s = (y <= back->y()) ? back->right : back->left;
				if (s) search_path.push(s);
			}
		}
	}
	dist = d;
	nearest->KNN_select();
	return nearest;
}

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

template <typename T> static T* do_build_kdtree(T **exm_set, int size)
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
	ret->left = do_build_kdtree(exm_set_left, size_left);
	ret->right = do_build_kdtree(exm_set_right, size_right);

	// free temp memory
	if (exm_set_left) delete [] exm_set_left;
	if (exm_set_right) delete [] exm_set_right;
	return ret;
}

template <typename T> static int build_kdtree(T*& tree, T** table, int count)
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

	tree = do_build_kdtree(exm_set, count);
	assert(nullptr != tree);

	delete [] exm_set;
	return 0;
}

}} // end of namespace zas::utils
#endif // __CXX_ZAS_MAPCORE_KDTREE_BASE_H__
/* EOF */
