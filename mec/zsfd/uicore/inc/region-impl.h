/** @file region-impl.h
 * Definition of internal object for regions
 */

#ifndef __CXX_ZAS_UICORE_REGION_IMPL_H__
#define __CXX_ZAS_UICORE_REGION_IMPL_H__

#include "uicore/region.h"
#include "std/list.h"

namespace zas {
namespace uicore {

class view_impl;

class region_t
{
public:
private:
	listnode_t _ownerlist;
	listnode_t *_rectlist;
	int32_t rect_count;
	uint32_t flags;
	listnode_t head[2];
};

/**
  intersect the rect with the given area

  @param r the rect to do the intersection
  @param left the given area left
  @param top the given area top
  @param width the given area width
  @param height the given area height
  */
void rect_intersect(irect_t* r, int32_t left, int32_t top, int32_t width, int32_t height);

/**
  intersect the rect with the given area

  @param r the rect to do the intersection
  @param clip the given area for clip
  */
static inline void rect_intersect(irect_t* r, irect_t& clip) {
	rect_intersect(r, clip.left, clip.top, clip.width, clip.height);
}

/**
  merge the rect with the given area (integer)

  @param r the rect to do the merge
  @param left the given area left
  @param top the given area top
  @param width the given area width
  @param height the given area height
  */
void rect_merge(irect_t* r, int32_t left, int32_t top, int32_t width, int32_t height);

/**
  merge the rect with the given area (float)

  @param r the rect to do the merge
  @param left the given area left
  @param top the given area top
  @param width the given area width
  @param height the given area height
  */

void rect_merge(rect_t* r, float left, float top, float width, float height);

/**
  merge the rect with the given area (integer)

  @param r the given area to which the r is going to merge
  @param src the rect to do the merge
  */
static inline void rect_merge(irect_t* r, irect_t& src) {
	rect_merge(r, src.left, src.top, src.width, src.height);
}

/**
  merge the rect with the given area

  @param r the given area to which the r is going to merge
  @param src the rect to do the merge
  */
static inline void rect_merge(rect_t* r, rect_t& src) {
	rect_merge(r, src.left, src.top, src.width, src.height);
}

/**
  Get visible dirty rect for a node
  the dirty rect means the maximum rect that
  covers all changes of the rect
  NOTE: if there is no change last time, the dirty
  rect will be the wrap rect of the node

  @param nd the node to be handle
  @param r the rect will be stored here
  @return 0 for success or an error happened
  */
int get_visible_dirtyrect(view_impl* nd, irect_t& r);

}} // end of namespace zas::uicore
#endif /* __CXX_ZAS_UICORE_REGION_IMPL_H__ */
/* EOF */
