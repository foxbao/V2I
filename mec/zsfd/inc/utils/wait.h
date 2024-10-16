/** @file wait.h
 * Definition of in process wait object and inter-process wait object
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_WAIT

#ifndef __CXX_ZAS_UTILS_WAIT_H__
#define __CXX_ZAS_UTILS_WAIT_H__

#include "utils/utils.h"
#include <pthread.h>

namespace zas {
namespace utils {

const uint32_t infinite = (uint32_t)-1;

class UTILS_EXPORT waitobject
{
public:
	explicit waitobject();
	virtual ~waitobject();

	waitobject& lock(void);
	waitobject& unlock(void);
	bool wait(uint32_t msec = infinite);
	waitobject& notify(void);
	waitobject& broadcast(void);

private:
	pthread_mutex_t mut;
	pthread_cond_t cond;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(waitobject);
};

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_WAIT_H__ */
#endif // UTILS_ENABLE_FBLOCK_WAIT
/* EOF */
