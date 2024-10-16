/** @file surface.h
 * Definition of the surface and surface manager
 */

#ifndef __CXX_ZAS_UICORE_SURFACE_H__
#define __CXX_ZAS_UICORE_SURFACE_H__

#include "uicore/uiobjectid.h"
#include "uicore/region.h"

namespace zas {
namespace uicore {

class surface_mgr;

zas_interface surface
{
	/**
	  Set the size of the surface
	  @param g the geometry of the size
	 */
	virtual void set_size(igeometry_t g) = 0;

	/**
	  Set the position of the surface
	  @param p the x, y of the surface
	  @param g the width, height of the surface
	           (width: -1, height: -1) means keep
			   the current size unchanged
	 */
	virtual void set_position(iposition_t p, igeometry_t g
		= igeometry_t()) = 0;
};

class UICORE_EXPORT surface_mgr
{
public:
	surface_mgr() = delete;
	~surface_mgr() = delete;

public:
	surface* create_surface(surfaceid id);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(surface_mgr);
};

/**
  Get the singleton object of surface manager
  @return return the singleton instance of surface manager
  */
UICORE_EXPORT surface_mgr* get_surfacemgr(void);

}} // end of namespace zas::uicore
#endif // __CXX_ZAS_UICORE_SURFACE_H__
/* EOF */
