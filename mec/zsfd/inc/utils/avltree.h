#ifndef	__CXX_ZAS_UTILS_AVL_H__
#define	__CXX_ZAS_UTILS_AVL_H__

#include <stdio.h>
#include "utils/utils.h"

namespace zas {
namespace utils {

typedef struct avl_node
{
	short avl_balance;
	unsigned short avl_child_index;
	struct avl_node *avl_child[2];
	struct avl_node *avl_parent;
}
avl_node_t;


#define	AVL_XPARENT(n)		((n)->avl_parent)
#define	AVL_XBALANCE(n)		((n)->avl_balance)
#define	AVL_XCHILD(n)		((n)->avl_child_index)
#define	AVL_SETPARENT(n, p)	((n)->avl_parent = (p))
#define	AVL_SETCHILD(n, c)	((n)->avl_child_index = (unsigned short)(c))
#define	AVL_SETBALANCE(n, b)	((n)->avl_balance = (short)(b))

extern const int  avl_balance2child[];
typedef int (*avl_compare_f)(avl_node_t*, avl_node_t*);

#define INIT_AVL_NODE(n)	\
	memset(&(n), 0, sizeof(avl_node_t))

#define DEF_AVL_FIND_FUNC(name)	\
	static avl_node_t* name(avl_node_t *tree,	\
		avl_node_t *val, AVLCOMPARE cmp)	\
	{	\
		int child;	\
		avl_node_t *node;	\
		for (node = tree; NULL != node;	\
			node = node->avl_child[child])	\
		{	\
			int diff = cmp(val, node);	\
			if (!diff) return node;	\
			child = avl_balance2child[1 + diff];	\
		}	\
      return NULL;	\
	}

#define MAKE_FIND_OBJECT(val, type, mumber, avl_mumber)	\
	(avl_node_t*)(((size_t)&(val)) -	\
	((size_t)(&(((type*)0)->mumber))) +	\
	((size_t)(&(((type*)0)->avl_mumber))))

#define AVLNODE_ENTRY(type,member,ptr)	\
((type *)((unsigned long)(ptr)-(unsigned long)(&((type *)0)->member)))

UTILS_EXPORT int avl_insert(avl_node_t **tree, avl_node_t *new_data, avl_compare_f cmp);
UTILS_EXPORT void avl_remove(avl_node_t **tree, avl_node_t *pDelete);
UTILS_EXPORT avl_node_t* avl_find(avl_node_t *tree, avl_node_t *val, avl_compare_f cmp);
UTILS_EXPORT avl_node_t *avl_first(avl_node_t *tree);
UTILS_EXPORT avl_node_t *avl_next(avl_node_t *prev);

};	// end of namespace utils
};	// end of namespace zas
#endif // __CXX_ZAS_UTILS_AVL_H__
/* EOF */
