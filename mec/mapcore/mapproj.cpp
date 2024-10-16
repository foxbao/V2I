#include <set>
#include <math.h>

#include "proj.h"
#include "inc/maputils.h"
#include "utils/timer.h"

namespace zas {
namespace mapcore {
using namespace std;

class proj_impl
{
public:
	proj_impl(const char* from, const char* to)
	: _prj(nullptr) {
		init(from, to);
	}

	~proj_impl() {
		if (nullptr != _prj) {
			proj_destroy(_prj); _prj = nullptr;
		}
	}

	point3d transform(const point3d& src, bool inv)
	{
		if (nullptr == _prj) {
			return {0, 0, 0};
		}
		PJ_COORD s = proj_coord(src.v[0], src.v[1], src.v[2], 0);
		PJ_COORD r = proj_trans(_prj, (inv) ? PJ_INV : PJ_FWD, s);
		return { r.v[0], r.v[1], r.v[2] };
	}

	void transform_point(point3d& src, bool inv) {
		if (nullptr == _prj) {
			src.set(0., 0.);
			return;
		}
		PJ_COORD s = proj_coord(src.v[0], src.v[1], src.v[2], 0);
		PJ_COORD r = proj_trans(_prj, (inv) ? PJ_INV : PJ_FWD, s);
		src.set(r.v[0], r.v[1], r.v[2]);
	}

	bool valid(void) {
		return (nullptr == _prj) ? false : true;
	}

private:
	void grant_context(void)
	{
		if (nullptr != _thdctx) {
			return;
		}
		_thdctx = proj_context_create();
		assert(nullptr != _thdctx);
	}

	void init(const char* from, const char* to)
	{
		if (!from || !*from || !to || !*to) {
			return;
		}	
		grant_context();

		PJ* proj = proj_create_crs_to_crs(_thdctx, from, to, nullptr);
		if (nullptr == proj) {
			return;
		}

		PJ* norm = proj_normalize_for_visualization(_thdctx, proj);
		proj_destroy(proj);
		if (nullptr == norm) {
			return;
		}
		_prj = norm;
	}

private:
	PJ *_prj;
	static __thread PJ_CONTEXT* _thdctx;
};

__thread PJ_CONTEXT* proj_impl::_thdctx = nullptr;

proj::proj() : _data(nullptr) {
}

proj::proj(const char* from , const char* to)
: _data(nullptr)
{
	auto* p = new proj_impl(from, to);
	if (!p || !p->valid()) {
		delete p; return;
	}
	_data = reinterpret_cast<proj*>(p);
}

proj::~proj()
{
	if (nullptr != _data) {
		auto* p = reinterpret_cast<proj_impl*>(_data);
		_data = nullptr;
		delete p;
	}
}

bool proj::valid(void) const
{
	if (nullptr == _data) {
		return false;
	}
	auto* p = reinterpret_cast<proj_impl*>(_data);
	return p->valid();
}

void proj::reset(const char* from, const char* to)
{
	if (nullptr != _data) {
		auto* p = reinterpret_cast<proj_impl*>(_data);
		_data = nullptr;
		delete p;
	}
	if (nullptr == from || nullptr == to) {
		return;
	}
	auto *p = new proj_impl(from, to);
	if (!p || !p->valid()) {
		delete p; return;
	}
	_data = reinterpret_cast<proj*>(p);
}

point3d proj::transform(const point3d& src, bool inv) const
{
	if (!_data || !reinterpret_cast<proj_impl*>(_data)->valid()) {
		return {0, 0, 0};
	}
	auto* p = reinterpret_cast<proj_impl*>(_data);
	return p->transform(src, inv);
}

void proj::transform_point(point3d& src, bool inv) const
{
	if (!_data || !reinterpret_cast<proj_impl*>(_data)->valid()) {
		return;
	}
	auto* p = reinterpret_cast<proj_impl*>(_data);
	p->transform_point(src, inv);
}

point3d proj::inv_transform(const point3d& src) const {
	return transform(src, true);
}

void proj::inv_transform_point(point3d& src) const {
	transform_point(src, true);
}

}} // end of namespace zas::mapcore
/* EOF */
