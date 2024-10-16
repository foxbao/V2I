/** @file context2d.h
 * definition of context2d object
 */

#ifndef __CXX_ZAS_HWGRPH_CONTEXT2D__H__
#define __CXX_ZAS_HWGRPH_CONTEXT2D__H__

#include "hwgrph/font.h"
#include "hwgrph/imageloader.h"

namespace zas {
namespace hwgrph {

class HWGRPH_EXPORT color
{
public:

	color() : m_color(0xFFFFLL << (64 - 16)) {}
	color(const color& c) : m_color(c.m_color) {}

	color& operator=(const color& c)
	{
		if (this != &c)
			m_color = c.m_color;
		return *this;
	}

	color(uint64_t clr) : m_color(clr) {}
	color(float r, float g, float b);
	color(float r, float g, float b, float a);

	~color() {}
	uint64_t value(void) { return m_color; }
	uint32_t value32(void);

	float r(void) const {
		return float(m_color & 0xFFFF) / 65535;
	}
	float g(void) const {
		return float((m_color >> 16) & 0xFFFF) / 65535;
	}
	float b(void) const {
		return float((m_color >> 32) & 0xFFFF) / 65535;
	}
	float a(void) const {
		return float((m_color >> 48) & 0xFFFF) / 65535;
	}

private:
	uint64_t m_color;
};

static inline color rgba(float r, float g, float b, float a) {
	return color(r, g, b, a);
}

enum fillstyle_type
{
	fillstyle_type_unknown = -1,
	solid_fill,
	linear_gradient,
	pattern_fill,
};

class HWGRPH_EXPORT fillstyle_base
{
public:
	fillstyle_base();
	virtual ~fillstyle_base();

	int addref(void) {
		return __sync_add_and_fetch(&_refcnt, 1);
	}
	int release(void);

	/**
	  Downcast the "real" fillstyle object
	  this method will also return the type of the object
	  @param type [out] the type of the object
	  @return the object pointer
	 */
	virtual void* getobject(int* type = NULL);

private:
	int _refcnt;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(fillstyle_base);
};

typedef zas::smtptr<fillstyle_base> fillstyle;

class HWGRPH_EXPORT solidfill_object
{
public:
	solidfill_object() = delete;
	~solidfill_object() = delete;

	int addref(void);
	int release(void);	
	operator fillstyle_base*(void);
	/**
	  Set the color of the solidfill
	  @return the solidfill object
	 */
	solidfill_object* setcolor(color c);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(solidfill_object);
};

typedef zas::smtptr<solidfill_object> solidfill;

enum ctx2d_target_type
{
	ctx2d_target_type_unknown = 0,
	render_target_to_screen,
	render_target_to_image,
};

class HWGRPH_EXPORT ctx2d_target_object
{
public:
	ctx2d_target_object() = delete;
	~ctx2d_target_object() = delete;

	int addref(void);
	int release(void);

	/**
	 Get the target type
	 @return the type of the target
	 */
	int gettype(void);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(ctx2d_target_object);
};

typedef zas::smtptr<ctx2d_target_object> ctx2d_target;

class HWGRPH_EXPORT shadow_object
{
public:
	shadow_object() = delete;
	~shadow_object() = delete;

	int addref(void);
	int release(void);

	/**
	  Check if the shadow is enabled
	  @return ture for enabled
	 */
	bool enabled(void);

	/**
	  Enable the shadow for following drawing
	 */
	void enable(void);

	/**
	  Disable the shadow for following drawing
	 */
	void disable(void);

	/**
	  Set the color of shadow
	  @param c the color for shadow
	 */
	void setcolor(color c);

	/**
	  Set the size of the shadow
	  the size is the rate of the
	  (shadow size) / (shape size)
	  @param s the size
	  @return 0 for success or error happened
	 */
	int setsize(float s);

	/**
	  Set the offsetX of the shadow
	  offsetX is the x-shifting for the shadow
	  relative to the shape
	  @param x the x shift
	  @return 0 for success or error happened
	 */
	int setoffsetX(float x);

	/**
	  Set the offsetY of the shadow
	  offsetY is the Y-shifting for the shadow
	  relative to the shape
	  @param y the y shift
	  @return 0 for success or error happened
	 */
	int setoffsetY(float y);

	/**
	  Set the blur level (similar to blur radius)
	  @param blurlevel the blur level
	  @return 0 for success or error occurred
	 */
	int setblur(int blurlevel);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shadow_object);
};

typedef zas::smtptr<shadow_object> shadowstyle;

class HWGRPH_EXPORT context2d
{
public:
	context2d() = delete;
	~context2d() = delete;

public:

	/**
	 Create a context2d
	note: a user shall not create the context
	by itself if it knows what they are doing
	use the system provided API to access a context2d
	@param thdsafe indicate if the context having the
		thread-safe execution to be enabled
	@return the context2d object created
	*/
	static context2d* create(bool thdsafe = false);

	/**
	  Release the context2d object
	 */
	void release(void);

	/**
	  Set the target
	  a new off-screen image will be created according to
	  the width and height parameter if render_target_to_image
	  is speicified
	  @param type the type of the target
	  @param width the image width
	  @param height the image height
	  @return the target in the context
	  	before this call
	 */
	ctx2d_target set_target(ctx2d_target_type type, int width, int height);

	/**
	  Set fillstyle as the solid fill
	  @param sf the solid fill object
	  @return the previous fillstyle
	 */
	fillstyle set_fillstyle(solidfill sf);

	/**
	  Set the shadow object
	  @param sdw the shadow object
	  @return the previous shadow object
	 */
	shadowstyle set_shadowstyle(shadowstyle sdw);

	/**
	  Set the font for current context
	  @param fnt the font object to be set
	  @return the previous font object
	 */
	font set_font(font fnt);

	/**
	  Create a shadow style
	  @return the created shadow style
	 */
	shadowstyle create_shadowstyle(void);

	/**
	  Create a solidfill object
	  @return the solidfill object
	 */
	solidfill create_solidfill(void);

	/**
	  Create a solidfill object
	  @param c specify the color of solidfill
	  @return the solidfill object
	 */
	solidfill create_solidfill(color c);

	/**
	  Clear the screen with specfic color
	*/
	void clear(const color c);

	/**
	  Draw the shadow of the path
	  @return 0 for success or error occurred
	 */
	int shadow(void);

	/**
	  Draw the outline of the path
	  @return 0 for success or error occurred
	 */
	int stroke(void);

	/**
	  Fill the path
	  @return 0 for success or error occurred
	 */
	int fill(void);

	/**
	  Create the path of rect (including round rect)
	  @param left the left position
	  @param top the top position
	  @param width the width of rectangle
	  @param height the height of rectangle
	  @param radius the radius of the round rect
			if radius is zero, the method will create
			a rectangle
	  @return 0 for success or an error occurred
	 */
	int rect(float left, float top, float width,
		float height, float radius = 0);

	// todo: test only
	int drawimage(image img, int left, int top,
		int width, int height);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(context2d);
};

}} // end of namespace zas::hwgrph
#endif // __CXX_ZAS_HWGRPH_CONTEXT2D__H__
/* EOF */
