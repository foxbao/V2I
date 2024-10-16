/** @file inc/std/list.h
 * definition of standard list operations
 */

#ifndef __CXX_STD_LIST_H__
#define __CXX_STD_LIST_H__

struct listnode_t
{
	listnode_t *next;
	listnode_t *prev;
};

#define listnode_init(myself)	\
do {	\
	(myself).prev = (myself).next = &(myself);	\
} while (0)

#define listnode_add(head, myself)	\
do {	\
	(myself).prev = (head).prev;	\
	(myself).next = (head).prev->next;	\
	(head).prev->next = &(myself);	\
	(head).prev = &(myself);	\
} while (0)

#define listnode_addprev(curr, myself)	\
do {	\
	(myself).next = &(curr);	\
	(myself).prev = (curr).prev;	\
	(curr).prev = &(myself);	\
	(curr).prev->next = &(myself);	\
} while (0)

#define listnode_insertfirst(head, myself)	\
do {	\
	(myself).next = (head).next;	\
	(myself).prev = &(head);	\
	(head).next->prev = &(myself);	\
	(head).next = &(myself);	\
} while (0)

#define listnode_del(myself)	\
do {	\
	(myself).prev->next = (myself).next;	\
	(myself).next->prev = (myself).prev;	\
	(myself).prev = (myself).next = &(myself);	\
} while (0)

#define listnode_safe_del(myself)	\
do {	\
	if (!listnode_isempty(myself))	\
		listnode_del(myself);	\
} while (0)

#define listnode_issingle(myself)	\
	(((myself).prev == (myself).next) && ((myself).prev == &(myself)))

#define listnode_iscleared(myself)	\
	((myself).prev == NULL && (myself).next == NULL)

#define listnode_clear(myself)	\
do {	\
	(myself).prev = (myself).next = NULL;	\
} while (0)

#define listnode_isempty(myself)	((myself).next == &(myself))
#define LIST_ENTRY(type, member, ptr)	\
	((type *)((size_t)(ptr)-(size_t)(&((type *)0)->member)))
#define list_entry(type, member, ptr)	\
	((type *)((size_t)(ptr)-(size_t)(&((type *)0)->member)))
#define LISTNODE_INITIALIZER(myself) { &(myself), &(myself) }

struct listnode : public listnode_t
{
	listnode() {
		prev = this, next = this;
	}
	bool empty(void) {
		return listnode_isempty(*this) ? true : false;
	}
};

#endif
/* EOF */

