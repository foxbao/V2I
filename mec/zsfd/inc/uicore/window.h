/** @file window.h
 * Definition of the window
 */

#ifndef __CXX_ZAS_UICORE_WINDOW_H__
#define __CXX_ZAS_UICORE_WINDOW_H__

#include "uicore/region.h"

namespace zas {
namespace uicore {

class UICORE_EXPORT window
{
public:
	/**
	  Show the window
	  @param sync block the call until the show
	  			operation is finished
	  @return 0 for success
	  */
	int show(bool sync = true);

	/**
	  Close the window and release its resource
	  @return 0 for success
	  */
	int close(bool sync = true);

	/**
	  Set the size of the window
	  @param g the geometry size of the window
	  	(width: -1, height: -1) means no change
		of the size
	  */
	void set_size(igeometry_t g);

	/**
	  Set the position of thw window
	  @param p the position of the window
	  @param g the geometry size of the window
	  	(width: -1, height: -1) means no change
		of the size
	  */
	void set_position(iposition_t p, igeometry_t g);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(window);
};

/**
  Create a window with specified parents
  @param parents the parents for the window, could be NULL
  @return the created window
  */
UICORE_EXPORT window* create_window(window* parents = NULL);

}} // end of namespace zas::uicore
#endif // __CXX_ZAS_UICORE_WINDOW_H__
/* EOF */
