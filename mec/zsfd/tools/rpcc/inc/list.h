#ifndef INCLUDE_TLIST_H_
#define INCLUDE_TLIST_H_

#include <stdio.h>

#define LIST_ENTRY(type,member,ptr)	\
	((type *)((unsigned long)(ptr)-(unsigned long)(&((type *)1)->member) + 1))

class list_item
{
public:

	void AddTo(list_item& head)
	{
		prev = (head).prev;
		next = (head).prev->next;
		(head).prev->next = this;
		(head).prev = this;
	}

	void AddToPrev(list_item &curr)
	{
		next = &curr;
		prev = curr.prev;
		curr.prev = this;
		prev->next = this;
	}

	void InsertFirst(list_item& head)
	{
		next = head.next;
		prev = &head;
		head.next->prev = this;
		head.next = this;
	}

	void Delete(void)
	{
		prev->next = next;
		next->prev = prev;
		prev = next = this;
	}

	list_item *getnext(void) const {
		return next;
	}

	list_item *GetPrev(void) const {
		return prev;
	}

	bool IsCleared(void)
	{
		return (prev == NULL && next == NULL)
			? true : false;
	}

	void Clear(void)  {
		prev = next = NULL;
	}


	bool is_empty(void) {
		return (next == this) ? true : false;
	}

public:

	// Initialize this node
	list_item() {
		next = this;
		prev = this;
	}

private:

	list_item *next;
	list_item *prev;

private:
	list_item(const list_item&);
	list_item& operator =(const list_item&);
};

#endif /* INCLUDE_TLIST_H_ */
