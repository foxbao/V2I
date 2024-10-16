/** @file mutex.h
 */

#ifndef __CXX_ZAS_UTILS_MUTEX_H__
#define __CXX_ZAS_UTILS_MUTEX_H__

namespace zas {
namespace utils {

class mutex
{
};

class auto_mutex
{
public:
	auto_mutex(mutex& mut) {}
	auto_mutex(mutex* mut) {}
	~auto_mutex() {}
};

#define MUTEX_ENTER(mut)	{ zas::utils::auto_mutex am(mut)
#define MUTEX_EXIT()		}

}}; // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_MUTEX_H__ */
/* EOF */