/** @file region.cpp
 * implementation of region operations
 */

#include "inc/region-impl.h"
#include "inc/view-impl.h"

namespace zas {
namespace uicore {

// this function is not locked
static int _subtract_rect(int x1, int y1, int a1, int b1,
	int x2, int y2, int a2, int b2, void *w,
	void (*proc)(int, int, int, int, void*))
{

#define FIX(x,y,a,b)    \
	do {    \
	register unsigned short tmp;  \
	if ((x) > (a)) { tmp = (x); (x) = (a); (a) = tmp; }  \
	if ((y) > (b)) { tmp = (y); (y) = (b); (b) = tmp; }  \
	}while (0)

#define IN_BOX(x,y,a,b,c,d)	\
	((x) >= (a) && (x) <= (c) && (y) >= (b) && (y) <= (d))

#define INBOX(x,y)      IN_BOX(x,y,x1,y1,a1,b1)

#define FIRST           1
#define SECOND          2
#define THIRD           4
#define FOURTH          8

	unsigned char count = 0,map = 0;

	FIX(x1, y1, a1, b1);
	FIX(x2, y2, a2, b2);

	if (x1 == x2 && y1 == y2 && a1 == a2 && b1 == b2)
	{
		proc(0, 0, -1, -1, w);
		return 1;
	}

	if (INBOX(x2,y2))    { count++; map |= FIRST;  }
	if (INBOX(a2,y2))    { count++; map |= SECOND; }
	if (INBOX(x2,b2))    { count++; map |= THIRD;  }
	if (INBOX(a2,b2))    { count++; map |= FOURTH; }

	if (count == 4)      /* ??????? */
	{
		proc(0, 0, -1, -1, w);
		return 1;
	}

	else if (count == 0)
	{
		int y, b;

		/* ??????????? */
		if (x1 > a2 || y1 > b2 || a1 < x2 || b1 < y2)
		{
			proc(x2,y2,a2,b2,w);
			return 1;
		}
		/* ?4?? */
		if (x2 <= a2 && y2 <= y1 - 1)
			proc(x2, y2, a2, y1 - 1, w);

		y = (y2 > y1) ? y2 : y1;
		b = (b2 > b1) ? b1 : b2;
		if (x2 <= x1 - 1 && y <= b)
			proc(x2, y, x1 - 1, b, w);

		if (a1 + 1 <= a2 && y <= b)
			proc(a1 + 1, y, a2, b, w);

		if (x2 <= a2 && b1 + 1 <= b2)
			proc(x2, b1 + 1, a2, b2, w);
		return 1;
	}
	else if (count == 1)
	{
		if (map & FIRST)
		{
			proc(a1 + 1, y2, a2, b1, w);
			proc(x2 , b1 + 1, a2, b2, w);
		}
		else if (map & SECOND)
		{
			proc(x2, y2, x1 - 1, b1, w);
			proc(x2, b1 + 1, a2, b2, w);
		}
		else if (map & THIRD)
		{
			proc(x2, y2, a2, y1 - 1, w);
			proc(a1 + 1, y1, a2, b2, w);
		}
		else if (map & FOURTH)
		{
			proc(x2, y2, a2, y1 - 1, w);
			proc(x2, y1, x1 - 1, b2, w);
		}
		else return 0;
	}
	else if (count == 2)
	{
		if (map & FIRST)
		{
			if (map & SECOND) proc(x2, b1 + 1, a2, b2, w);
			if (map & THIRD)  proc(a1 + 1, y2, a2, b2, w);
		}
		if (map & SECOND)
			if (map & FOURTH) proc(x2, y2, x1 - 1, b2, w);
		if (map & THIRD)
			if (map & FOURTH) proc(x2, y2, a2, y1 - 1, w);
	}
	return 0;

#undef FIX
#undef IN_BOX
#undef INBOX
#undef FIRST
#undef SECOND
#undef THIRD
#undef FOURTH
}

void rect_intersect(irect_t* r, int32_t left, int32_t top, int32_t width, int32_t height)
{
	int rc, rd, c, d;
	if (r->isnull()) return;

	rc = r->left + r->width - 1;
	rd = r->top + r->height - 1;
	c = left + width - 1;
	d = top + height - 1;

	if (left > rc || c < r->left)
	{
		r->clear();
		return;
	}
	if (top > rd || d < r->top)
	{
		r->clear();
		return;
	}

	if (r->left < left) r->left = left;
	if (r->top < top) r->top = top;
	if (rc > c) rc = c;
	if (rd > d) rd = d;

	r->width = rc - r->left + 1;
	r->height = rd - r->top + 1;
}

void rect_merge(rect_t* r, float left, float top, float width, float height)
{
	if (width < 0 || height < 0)
		return;

	float rr = r->left + r->width;
	float rb = r->top + r->height;

	if (r->left > left) r->left = left;
	if (r->top > top) r->top = top;

	float mr = left + width;
	float mb = top + height;

	if (rr < mr) rr = mr;
	if (rb < mb) rb = mb;

	r->width = rr - r->left;
	r->height = rb - r->top;
}

void rect_merge(irect_t* r, int32_t left, int32_t top, int32_t width, int32_t height)
{
	if (width < 0 || height < 0)
		return;

	int32_t rr = r->left + r->width;
	int32_t rb = r->top + r->height;

	if (r->left > left) r->left = left;
	if (r->top > top) r->top = top;

	int32_t mr = left + width;
	int32_t mb = top + height;

	if (rr < mr) rr = mr;
	if (rb < mb) rb = mb;

	r->width = rr - r->left;
	r->height = rb - r->top;
}

int get_visible_dirtyrect(view_impl* nd, irect_t& r)
{
	if (NULL == nd) {
		return 1;
	}

	for (view_impl* p = nd; p; p = p->parents)
	{
		// get the latest submitted node generic info
		generic_info_t* ginfo = p->ginfo[nd->flags.readonly_ginfo_pos];

		// see if the node is visible
		if (!ginfo || !ginfo->flags.visible) {
			goto zero_wraprect;
		}

		// get the wraprect
		if (p == nd) {
			ginfo_get_wraprect(ginfo, r);

			// check if we have a previous ginfo
			unsigned int prevpos = (nd->flags.readonly_ginfo_pos + 2) % 3;
			generic_info_t* prev_ginfo = p->ginfo[prevpos];
			if (prev_ginfo) {
				irect_t pr;
				ginfo_get_wraprect(prev_ginfo, pr);
				rect_merge(&pr, r);
			}
		}
		else { // clip with parents
			irect_t clip;
			ginfo_get_clientrect(ginfo, clip);
			rect_intersect(&r, clip);
		}
		
		if (!r.width || !r.height) {
			goto zero_wraprect;
		}
	}
	return 0;

	zero_wraprect: r.set(0, 0, -1, -1);
	return 0;
}

}} // end of namespace zas::uicore
/* EOF */
