/** @file listree.h
 * definition of standard "location independent list and avltree operations
 */

#ifndef __CXX_ZAS_HWGRPH_LI_LIST_AVLTREE_H__
#define __CXX_ZAS_HWGRPH_LI_LIST_AVLTREE_H__

#include "std/zasbsc.h"

namespace zas {
namespace hwgrph {

#define li_offset(T, base, self, elemsz)	\
	(T)((((size_t)(self)) - ((size_t)(base))) / ((size_t)(elemsz)))

#define li_addr(T, base, offset, elemsz)	\
	(T*)(((size_t)(base)) + ((size_t)(offset)) * ((size_t)(elemsz)))

template <typename T> struct li_listnode_
{
	static const T _nullptr = (const T)(-1);
	T _next, _prev;
	li_listnode_(void *base, size_t elemsz) {
		init(base, elemsz);
	}

	li_listnode_* next(void* base, size_t elemsz) {
		return li_addr(li_listnode_, base, _next, elemsz);
	}

	li_listnode_* prev(void* base, size_t elemsz) {
		return li_addr(li_listnode_, base, _prev, elemsz);
	}

	bool empty(void* base, size_t elemsz) {
		return (li_addr(li_listnode_, base, _next, elemsz)
			== this) ? true : false;
	}

	void add(li_listnode_<T>& head, void* base, size_t elemsz)
	{
		_prev = head._prev;
		_next = head.prev(base, elemsz)->_next;
		T myself_offset = li_offset(T, base, this, elemsz);
		head.prev(base, elemsz)->_next = myself_offset;
		head._prev = myself_offset;
	}

	void del(void) {
		prev()->_next = _next;
		next()->_prev = _prev;		
	}

	void del(void* base, size_t elemsz) {
		prev()->_next = _next;
		next()->_prev = _prev;
		init(base, elemsz);
	}

	bool single(void* base, size_t elemsz) {
		return (_prev == _next && li_addr(li_listnode_, base, _prev, elemsz)
			== this) ? true : false;
	}

	bool cleared(void) {
		return (_prev == _nullptr && _next == _nullptr)
		? true : false;
	}

	bool clear(void) {
		_prev = _nullptr;
		_next = _nullptr;
	}

	void init(void* base, size_t elemsz) {
		_next = li_offset(T, base, this, elemsz);
		_prev = li_offset(T, base, this, elemsz);
	}
} PACKED;

#define li_list_entry(type, member, ptr)	\
	((type *)((size_t)(ptr)-(size_t)(&((type *)0)->member)))
#define li_avlnode_entry(type, member, ptr) li_list_entry(type, member, ptr)

#define li_make_find_object(val, type, mumber, avl_mumber, avlnode_type)	\
	(avlnode_type*)(((size_t)&(val)) -	\
	((size_t)(&(((type*)0)->mumber))) +	\
	((size_t)(&(((type*)0)->avl_mumber))))

static const int  avl_child2balance[]	= {-1, 1};
static const int  avl_balance2child[]	= {0, 0, 1};

template <typename T> class li_avlnode_
{
public:
	static const T _nullptr = (const T)(-1);
	void avl_remove(T* tree, li_avlnode_<T>* tmp_ptr,
		void* base, size_t elemsz)
	{
		T node, parent, pdel;
		li_avlnode_* parent_ptr, *node_ptr;
		li_avlnode_* pdel_ptr = this;
		T tmp = li_offset(T, base, tmp_ptr, elemsz);

		int left;
		int right;
		int which_child;
		int old_balance;
		int new_balance;

		// * Deletion is easiest with a node that has at most 1 child.
		// * We swap a node with 2 children with a sequentially valued
		// * neighbor node. That node will have at most 1 child. Note this
		// * has no effect on the ordering of the remaining nodes.
		// *
		// * As an optimization, we choose the greater neighbor if the tree
		// * is right heavy, otherwise the left neighbor. This reduces the
		// * number of rotations needed.
		
		
		if (pdel_ptr->_avl_child[0] != _nullptr && pdel_ptr->_avl_child[1] != _nullptr)
		{
			//choose node to swap from whichever side is taller
			old_balance = pdel_ptr->_avl_balance;
			left = avl_balance2child[old_balance + 1];
			right = 1 - left;

			// get to the previous value'd node
			// (down 1 left, as far as possible right)
			
			node = pdel_ptr->_avl_child[left];
			node_ptr = li_addr(T, base, node, elemsz);
			for (; node_ptr->_avl_child[right] != _nullptr;
				node = node_ptr->_avl_child[right],
				node_ptr = li_addr(T, base, node, elemsz));


			// create a temp placeholder for 'node'
			// move 'node' to pDelete's spot in the tree
			*tmp_ptr = *node_ptr;

			*node_ptr = *pdel_ptr;
			if (node_ptr->_avl_child[left] == node)
				node_ptr->_avl_child[left] = tmp;

			parent = node_ptr->avl_parent;
			if (parent != _nullptr) {
				parent_ptr = li_addr(T, base, parent, elemsz);
				parent_ptr->_avl_child[node_ptr->_avl_child_index] = node;
			}
			else
				*tree = node;

			li_avlnode_* node_left_chd_ptr =
				li_addr(li_avlnode_, base, node_ptr->_avl_child[left], elemsz);
			node_left_chd_ptr->_avl_parent = node;
			li_avlnode_* node_right_chd_ptr =
				li_addr(li_avlnode_, base, node_ptr->_avl_child[right], elemsz);
			node_right_chd_ptr->_avl_parent = node;

			// Put tmp where node used to be (just temporary).
			// It always has a parent and at most 1 child.
			pdel_ptr = &tmp;
			pdel = li_offset(T, base, pdel_ptr, elemsz);

			parent = pdel->_avl_parent;
			parent_ptr = li_addr(li_avlnode_, base, parent, elemsz);
			parent_ptr->_avl_child[pdel_ptr->avl_child_index] = pdel;
			which_child = (pdel_ptr->_avl_child[1] != 0);
			if (pdel_ptr->_avl_child[which_child] != _nullptr) {
				li_avlnode_* pdel_child_ptr =
					li_addr(li_avlnode_, base, pdel_ptr->_avl_child[which_child], elemsz);
				pdel_child_ptr->_avl_parent = pdel;
			}
		}

		// Here we know "pDelete" is at least partially a leaf node. It can
		// be easily removed from the tree.
		parent = pdel_ptr->_avl_parent;
		which_child = pdel_ptr->_avl_child_index;

		if (pdel_ptr->_avl_child[0] != _nullptr)
			node = pdel_ptr->_avl_child[0];
		else {
			node = pdel_ptr->_avl_child[1];
		}

		// Connect parent directly to node (leaving out pDelete).

		if (node != _nullptr)
		{
			node_ptr = li_addr(T, base, node, elemsz);
			node_ptr->_avl_parent = parent;
			node_ptr->_avl_child_index = uint16_t(which_child);
		}

		if (parent == _nullptr) {
			*tree = node;
			return;
		}

		parent_ptr = li_addr(li_avlnode_, base, parent, elemsz);
		parent_ptr->_avl_child[which_child] = node;

		// Since the subtree is now shorter, begin adjusting parent balances
		// and performing any needed rotations.

		do {

			// Move up the tree and adjust the balance
			// Capture the parent and which_child values for the next
			// iteration before any rotations occur.

			node = parent;
			node_ptr = li_addr(T, base, node, elemsz);
			old_balance = node_ptr->_avl_balance;
			new_balance = old_balance - avl_child2balance[which_child];
			parent = node_ptr->_avl_parent;
			which_child = node_ptr->_avl_child_index;

			// If a node was in perfect balance but isn't anymore then
			// we can stop, since the height didn't change above this point
			// due to a deletion.

			if (old_balance == 0) {
				node_ptr->avl_balance = int16_t(new_balance);
				break;
			}

			// If the new balance is zero, we don't need to rotate
			// else
			// need a rotation to fix the balance.
			// If the rotation doesn't change the height
			// of the sub-tree we have finished adjusting.

			if (new_balance == 0) {
				node_ptr->_avl_balance = int16_t(new_balance);
			}
			else if (!node_ptr->avl_rotation(base, tree, new_balance, elemsz))
				break;
		} while (parent != _nullptr);
	}

	int avl_insert(T *tree, int (*cmp)(li_avlnode_<T>*, li_avlnode_<T>*),
		void* base, size_t elemsz)
	{
		if (!base || !elemsz)
			return -1;

		li_avlnode_<T> *new_data = this;
		T node;
		T parent = _nullptr;
		li_avlnode_* node_ptr, *parent_ptr;

		int old_balance;
		int new_balance;
		int which_child = 0;

		// find the position to insert node
		node = *tree;
		node_ptr = li_addr(li_avlnode_, base, node, elemsz);
		while (_nullptr != node)
		{
			parent = node;
			old_balance = cmp(new_data, node_ptr);
			if (!old_balance) {
				return 1;
			}
			which_child = avl_balance2child[1 + old_balance];
			node = node_ptr->_avl_child[which_child];
			node_ptr = li_addr(li_avlnode_, base, node, elemsz);
		}

		node_ptr = new_data;
		node = li_offset(T, base, node_ptr, elemsz);
		node_ptr->_avl_child[0] = _nullptr;
		node_ptr->_avl_child[1] = _nullptr;

		node_ptr->_avl_child_index = uint16_t(which_child);
		node_ptr->_avl_balance = 0;
		node_ptr->_avl_parent = parent;

		if (parent != _nullptr) {
			parent_ptr = li_addr(li_avlnode_, base, parent, elemsz);
			parent_ptr->_avl_child[which_child] = node;
		} else *tree = node;

		// Now, back up the tree modifying the balance of all nodes above the
		// insertion point. If we get to a highly unbalanced ancestor, we
		// need to do a rotation.  If we back out of the tree we are done.
		// If we brought any subtree into perfect balance (0), we are also done.

		for (;;) {
			node = parent;
			node_ptr = li_addr(li_avlnode_, base, node, elemsz);
			if (node == _nullptr)
				return 0;

			// Compute the new balance
			old_balance = node_ptr->_avl_balance;
			new_balance = old_balance + avl_child2balance[which_child];

			//If we introduced equal balance, then we are done immediately
			if (new_balance == 0) {
				node_ptr->_avl_balance = 0;
				return 0;
			}

			// If both old and new are not zero we went
			// from -1 to -2 balance, do a rotation.

			if (old_balance != 0)
				break;
			
			node_ptr->_avl_balance = int16_t(new_balance);
			parent = node_ptr->_avl_parent;
			which_child = node_ptr->_avl_child_index;
		}

		// perform a rotation to fix the tree and return
		node_ptr->avl_rotation(base, tree, new_balance, elemsz);
		return 0;
	}

private:
	int avl_rotation(void* base, T* tree, int balance, size_t elemsz)
	{
		int left = !(balance < 0);	// when balance = -2, left will be 0
		int right = 1 - left;
		int left_heavy = balance >> 1;
		int right_heavy = -left_heavy;

		T parent = this->_avl_parent;
		T child = this->_avl_child[left];
		li_avlnode_* child_ptr = li_addr(li_avlnode_, base, child, elemsz);

		T cright, gchild, gright, gleft;
		
		int which_child = this->_avl_child_index;
		int child_bal = child_ptr->_avl_balance;

		// BEGIN CSTYLED
		//
		// case 1 : node is overly left heavy, the left child is balanced or
		// also left heavy. This requires the following rotation.
		//
		//                   (node bal:-2)
		/*                    /           \										*/
		/*                   /             \									*/
		//              (child bal:0 or -1)
		/*              /    \													*/
		/*             /      \													*/
		//                     cright
		//
		// becomes:
		//
		//              (child bal:1 or 0)
		/*              /        \												*/
		/*             /          \												*/
		//                        (node bal:-1 or 0)
		/*                         /     \										*/
		/*                        /       \										*/
		//                     cright
		//
		// we detect this situation by noting that child's balance is not
		// right_heavy.
		//
		// END CSTYLED

		if (child_bal != right_heavy) {

			//
			// compute new balance of nodes
			//
			// If child used to be left heavy (now balanced) we reduced
			// the height of this sub-tree -- used in "return...;" below
			//
			
			child_bal += right_heavy; // adjust towards right

			//
			// move "cright" to be node's left child
			//

			cright = child_ptr->_avl_child[right];
			this->_avl_child[left] = cright;
			T this_offset = li_offset(T, base, this, elemsz);
			if (cright != _nullptr) {
				li_avlnode_<T>* cright_ptr = li_addr(li_avlnode_, base, cright, elemsz);
				cright_ptr->_avl_parent = this_offset;
				cright_ptr->_avl_child_index = (uint16_t)left;
			}

			//
			// move node to be child's right child
			//

			child_ptr->_avl_child[right] = this_offset;
			this->_avl_balance = int16_t(-child_bal);
			this->_avl_child_index = uint16_t(right);
			this->_avl_parent = child;

			//
			// update the pointer into this subtree
			//

			child_ptr->_avl_balance = int16_t(child_bal);
			child_ptr->_avl_child_index = uint16_t(which_child);
			child_ptr->_avl_parent = parent;
			if (parent != _nullptr) {
				li_avlnode_* parent_ptr = li_addr(li_avlnode_, base, parent, elemsz);
				parent_ptr->_avl_child[which_child] = child;
			}
			else
				*tree = child;
			return (child_bal == 0);
		}

		// BEGIN CSTYLED */
		//
		// case 2 : When node is left heavy, but child is right heavy we use
		// a different rotation.
		//
		//                   (node b:-2)
		/*                    /   \												*/
		/*                   /     \											*/
		/*                  /       \											*/
		//             (child b:+1)
		/*              /     \													*/
		/*             /       \												*/
		//                   (gchild b: != 0)
		/*                     /  \												*/
		/*                    /    \											*/
		//                 gleft   gright
		//
		// becomes:
		//
		//              (gchild b:0)
		/*              /       \												*/
		/*             /         \												*/
		/*            /           \												*/
		//        (child b:?)   (node b:?)
		/*         /  \          /   \											*/
		/*        /    \        /     \											*/
		//            gleft   gright
		//
		// computing the new balances is more complicated. As an example:
		//	 if gchild was right_heavy, then child is now left heavy
		//		else it is balanced
		//
		// END CSTYLED

		gchild = child_ptr->_avl_child[right];
		li_avlnode_* gchild_ptr = li_addr(li_avlnode_, base, gchild, elemsz);
		gleft = gchild_ptr->_avl_child[left];
		gright = gchild_ptr->_avl_child[right];

		//
		// move gright to left child of node and
		//
		// move gleft to right child of node
		//

		this->_avl_child[left] = gright;
		if (gright != _nullptr)
		{
			li_avlnode_* gright_ptr = li_addr(li_avlnode_, base, gright, elemsz);
			gright_ptr->_avl_parent = li_offset(T, base, this, elemsz);
			gright_ptr->_avl_child_index = uint16_t(left);
		}

		child_ptr->_avl_child[right] = gleft;
		if (gleft != _nullptr)
		{
			li_avlnode_* gleft_ptr = li_addr(li_avlnode_, base, gleft, elemsz);
			gleft_ptr->_avl_parent = child;
			gleft_ptr->_avl_child_index = uint16_t(right);
		}

		//
		// move child to left child of gchild and
		//
		// move node to right child of gchild and
		//
		// fixup parent of all this to point to gchild
		//
		balance = gchild_ptr->_avl_balance;
		gchild_ptr->_avl_child[left] = child;
		child_ptr->_avl_balance = int16_t(balance == right_heavy ? left_heavy : 0);
		child_ptr->_avl_parent = gchild;
		child_ptr->_avl_child_index = uint16_t(left);

		gchild_ptr->_avl_child[right] = li_offset(T, base, this, elemsz);
		this->_avl_balance = int16_t(balance == left_heavy ? right_heavy : 0);
		this->_avl_parent = gchild;
		this->_avl_child_index = uint16_t(right);

		gchild_ptr->_avl_balance = 0;
		gchild_ptr->_avl_parent = parent;
		gchild_ptr->_avl_child_index = uint16_t(which_child);
		if (parent != _nullptr) {
			li_avlnode_* parent_ptr = li_addr(li_avlnode_, base, parent, elemsz);
			parent_ptr->_avl_child[which_child] = gchild;
		}
		else
			*tree = gchild;

		return (1);	// the new tree is always shorter
	}

public:
	int16_t _avl_balance;
	uint16_t _avl_child_index;
	T _avl_child[2];
	T _avl_parent;
} PACKED;

template<typename T> static inline
li_avlnode_<T>* li_avl_find(T tree, li_avlnode_<T>* val,
	int (*cmp)(li_avlnode_<T>*, li_avlnode_<T>*),
	void* base, size_t elemsz)
{
	int child;
	T node = tree;

	while (li_avlnode_<T>::_nullptr != node)
	{
		li_avlnode_<T>* node_ptr = li_addr(li_avlnode_<T>, base, node, elemsz);
		child = cmp(val, node_ptr);
		if (!child) return node_ptr;
		child = avl_balance2child[1 + child];
		node = node_ptr->_avl_child[child];
	}
	return NULL;
}

template<typename T> static inline
li_avlnode_<T>* li_avl_first(T tree, void* base, size_t elemsz)
{
	li_avlnode_<T> *prev = NULL;

	while (tree != li_avlnode_<T>::_nullptr)
	{
		prev = tree;
		li_avlnode_<T>* tree_ptr = li_addr(li_avlnode_<T>, base, tree, elemsz);
		tree = tree_ptr->_avl_child[0];
	}

	if (li_avlnode_<T>::_nullptr == prev) return NULL;
	li_avlnode_<T>* ret = li_addr(li_avlnode_<T>, base, prev, elemsz);
	return ret;
}

template<typename T> static inline
li_avlnode_<T>* li_avl_next(T prev, void* base, size_t elemsz)
{
	if (li_avlnode_<T>::_nullptr == prev)
		return NULL;

	li_avlnode_<T>* prev_ptr = li_addr(li_avlnode_<T>, base, prev, elemsz);
	if (prev_ptr->_avl_child[1])
		return avl_first(prev_ptr->_avl_child[1]);
	else
	{
		T parent = prev_ptr->_avl_parent;
		li_avlnode_<T>* parent_ptr = li_addr(li_avlnode_<T>, base, parent, elemsz);

		while (li_avlnode_<T>::_nullptr != parent &&
			parent_ptr->_avl_child[1] == prev)
		{
			prev = parent;
			parent = parent_ptr->avl_parent;
		}
		li_avlnode_<T>* ret = li_addr(li_avlnode_<T>, base, parent, elemsz);
		return ret;
	}
}

template<typename T> static inline
li_avlnode_<T>* li_avl_next(li_avlnode_<T> *prev_ptr, void* base, size_t elemsz)
{
	T prev = li_offset(T, base, prev_ptr, elemsz);
	return avl_next(prev, base, elemsz);
}

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_LI_LIST_AVLTREE_H__
/* EOF */

