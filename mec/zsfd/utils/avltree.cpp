#include "utils/avltree.h"

namespace zas {
namespace utils {

static const int  avl_child2balance[]	= {-1, 1};
const int  avl_balance2child[]	= {0, 0, 1};

// Perform a rotation to restore balance at the subtree given by depth.
//
// This routine is used by both insertion and deletion. The return value
// indicates:
//	 0 : subtree did not change height
//	!0 : subtree was reduced in height
//
// The code is written as if handling left rotations, right rotations are
// symmetric and handled by swapping values of variables right/left[_heavy]
//
// On input balance is the "new" balance at "node". This value is either
// -2 or +2.

static int avl_rotation(avl_node_t **tree, avl_node_t *node, int balance)
{
	int left = !(balance < 0);	// when balance = -2, left will be 0
	int right = 1 - left;
	int left_heavy = balance >> 1;
	int right_heavy = -left_heavy;
	avl_node_t *parent = AVL_XPARENT(node);
	avl_node_t *child = node->avl_child[left];
	avl_node_t *cright;
	avl_node_t *gchild;
	avl_node_t *gright;
	avl_node_t *gleft;
	int which_child = AVL_XCHILD(node);
	int child_bal = AVL_XBALANCE(child);

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

		cright = child->avl_child[right];
		node->avl_child[left] = cright;
		if (cright != NULL) {
			AVL_SETPARENT(cright, node);
			AVL_SETCHILD(cright, left);
		}

		//
		// move node to be child's right child
		//

		child->avl_child[right] = node;
		AVL_SETBALANCE(node, -child_bal);
		AVL_SETCHILD(node, right);
		AVL_SETPARENT(node, child);

		//
		// update the pointer into this subtree
		//

		AVL_SETBALANCE(child, child_bal);
		AVL_SETCHILD(child, which_child);
		AVL_SETPARENT(child, parent);
		if (parent != NULL)
			parent->avl_child[which_child] = child;
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

	gchild = child->avl_child[right];
	gleft = gchild->avl_child[left];
	gright = gchild->avl_child[right];

	//
	// move gright to left child of node and
	//
	// move gleft to right child of node
	//

	node->avl_child[left] = gright;
	if (gright != NULL) {
		AVL_SETPARENT(gright, node);
		AVL_SETCHILD(gright, left);
	}

	child->avl_child[right] = gleft;
	if (gleft != NULL) {
		AVL_SETPARENT(gleft, child);
		AVL_SETCHILD(gleft, right);
	}

	//
	// move child to left child of gchild and
	//
	// move node to right child of gchild and
	//
	// fixup parent of all this to point to gchild
	//
	balance = AVL_XBALANCE(gchild);
	gchild->avl_child[left] = child;
	AVL_SETBALANCE(child, (balance == right_heavy ? left_heavy : 0));
	AVL_SETPARENT(child, gchild);
	AVL_SETCHILD(child, left);

	gchild->avl_child[right] = node;
	AVL_SETBALANCE(node, (balance == left_heavy ? right_heavy : 0));
	AVL_SETPARENT(node, gchild);
	AVL_SETCHILD(node, right);

	AVL_SETBALANCE(gchild, 0);
	AVL_SETPARENT(gchild, parent);
	AVL_SETCHILD(gchild, which_child);
	if (parent != NULL)
		parent->avl_child[which_child] = gchild;
	else
		*tree = gchild;
	
	return (1);	// the new tree is always shorter
}

void
avl_remove(avl_node_t **tree, avl_node_t *pDelete)
{
	avl_node_t tmp;	
	avl_node_t *node;
	avl_node_t *parent;

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
	
	
	if (pDelete->avl_child[0] != NULL && pDelete->avl_child[1] != NULL)
	{
		//choose node to swap from whichever side is taller
		old_balance = AVL_XBALANCE(pDelete);
		left = avl_balance2child[old_balance + 1];
		right = 1 - left;

		// get to the previous value'd node
		// (down 1 left, as far as possible right)
		
		for (node = pDelete->avl_child[left];
				node->avl_child[right] != NULL;
				node = node->avl_child[right]);


	    // create a temp placeholder for 'node'
	    // move 'node' to pDelete's spot in the tree
		tmp = *node;

		*node = *pDelete;
		if (node->avl_child[left] == node)
			node->avl_child[left] = &tmp;

		parent = AVL_XPARENT(node);
		if (parent != NULL)
			parent->avl_child[AVL_XCHILD(node)] = node;
		else
			*tree = node;
		
		AVL_SETPARENT(node->avl_child[left], node);
		AVL_SETPARENT(node->avl_child[right], node);

		// Put tmp where node used to be (just temporary).
		// It always has a parent and at most 1 child.
		pDelete = &tmp;
		parent = AVL_XPARENT(pDelete);
		parent->avl_child[AVL_XCHILD(pDelete)] = pDelete;
		which_child = (pDelete->avl_child[1] != 0);
		if (pDelete->avl_child[which_child] != NULL)
		AVL_SETPARENT(pDelete->avl_child[which_child], pDelete);
	}

	// Here we know "pDelete" is at least partially a leaf node. It can
	// be easily removed from the tree.

	parent = AVL_XPARENT(pDelete);
	which_child = AVL_XCHILD(pDelete);

	if (pDelete->avl_child[0] != NULL)
		node = pDelete->avl_child[0];
	else
	node = pDelete->avl_child[1];

	// Connect parent directly to node (leaving out pDelete).

	if (node != NULL)
	{
		AVL_SETPARENT(node, parent);
		AVL_SETCHILD(node, which_child);
	}

	if (parent == NULL) {
		*tree = node;
		return;
	}

	parent->avl_child[which_child] = node;

	// Since the subtree is now shorter, begin adjusting parent balances
	// and performing any needed rotations.

	do {

		// Move up the tree and adjust the balance
		// Capture the parent and which_child values for the next
		// iteration before any rotations occur.

		node = parent;
		old_balance = AVL_XBALANCE(node);
		new_balance = old_balance - avl_child2balance[which_child];
		parent = AVL_XPARENT(node);
		which_child = AVL_XCHILD(node);

		// If a node was in perfect balance but isn't anymore then
		// we can stop, since the height didn't change above this point
		// due to a deletion.

		if (old_balance == 0) {
			AVL_SETBALANCE(node, new_balance);
			break;
		}

		// If the new balance is zero, we don't need to rotate
		// else
		// need a rotation to fix the balance.
		// If the rotation doesn't change the height
		// of the sub-tree we have finished adjusting.

		if (new_balance == 0)
			AVL_SETBALANCE(node, new_balance);
		else if (!avl_rotation(tree, node, new_balance))
			break;
	} while (parent != NULL);
}

int avl_insert(avl_node_t **tree,
			avl_node_t *new_data, avl_compare_f cmp)
{
	avl_node_t *node;
	avl_node_t *parent = NULL;
	int old_balance;
	int new_balance;
	int which_child = 0;

	// find the position to insert node
	for (node = *tree; NULL != node;
			node = node->avl_child[which_child])
	{
		parent = node;
		old_balance = cmp(new_data, node);
		if (!old_balance) return 1;
		which_child = avl_balance2child[1 + old_balance];
	}

	node = new_data;
	node->avl_child[0] = NULL;
	node->avl_child[1] = NULL;

	AVL_SETCHILD(node, which_child);
    AVL_SETBALANCE(node, 0);
    AVL_SETPARENT(node, parent);

	if (parent != NULL) {
		parent->avl_child[which_child] = node;
	} else *tree = node;

	// Now, back up the tree modifying the balance of all nodes above the
	// insertion point. If we get to a highly unbalanced ancestor, we
	// need to do a rotation.  If we back out of the tree we are done.
	// If we brought any subtree into perfect balance (0), we are also done.

	for (;;) {
		node = parent;
		if (node == NULL)
			return 0;

	// Compute the new balance

	old_balance = AVL_XBALANCE(node);
	new_balance = old_balance + avl_child2balance[which_child];

	//If we introduced equal balance, then we are done immediately

	if (new_balance == 0) {
		AVL_SETBALANCE(node, 0);
		return 0;
	}

	// If both old and new are not zero we went
	// from -1 to -2 balance, do a rotation.

	if (old_balance != 0)
		break;
	
	AVL_SETBALANCE(node, new_balance);
	parent = AVL_XPARENT(node);
	which_child = AVL_XCHILD(node);
	}

	// perform a rotation to fix the tree and return
	(void) avl_rotation(tree, node, new_balance);
	return 0;
}

avl_node_t* avl_find(avl_node_t *tree, avl_node_t *val, avl_compare_f cmp)
{
	register int child;
	register avl_node_t *node;

	for (node = tree; NULL != node;
		node = node->avl_child[child])
	{
		child = cmp(val, node);
		if (!child) return node;
		child = avl_balance2child[1 + child];
	}
	return NULL;
}

// Return the lowest valued node in a tree or NULL.
// (leftmost child from root of tree)

avl_node_t *avl_first(avl_node_t *tree)
{
	avl_node_t *prev = NULL;

	for (;tree != NULL; tree = tree->avl_child[0])
		prev = tree;

	if (NULL != prev) return prev;
	return NULL;
}

avl_node_t *avl_next(avl_node_t *prev)
{
	if (NULL == prev)
		return NULL;

	if (prev->avl_child[1])
		return avl_first(prev->avl_child[1]);
	else
	{
		avl_node_t *parent = prev->avl_parent;
		for (; parent && parent->avl_child[1] == prev;
			prev = parent, parent = prev->avl_parent);
		return parent;
	}
}


};	// end of namespace utils
};	// end of namespace zas
/* EOF */
