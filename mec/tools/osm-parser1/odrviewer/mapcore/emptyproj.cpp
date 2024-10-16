#include <set>
#include <math.h>

#include "inc/maputils.h"

namespace zas {
namespace mapcore {
using namespace std;

proj::proj() : _data(nullptr) {
}

proj::proj(const char* from , const char* to)
: _data(nullptr)
{
}

proj::~proj()
{
}

bool proj::valid(void) const
{
	return false;
}

void proj::reset(const char* from, const char* to)
{
}

point3d proj::transform(const point3d& src, bool inv) const
{
	point3d v3d;
	return v3d;
}

void proj::transform_point(point3d& src, bool inv) const
{
}

point3d proj::inv_transform(const point3d& src) const {
	point3d v3d;
	return v3d;
}

void proj::inv_transform_point(point3d& src) const {

}

}} // end of namespace zas::mapcore
/* EOF */
