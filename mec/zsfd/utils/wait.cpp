/** @file wait.cpp
 * implementation of the wait object
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_WAIT

#include <errno.h>
#include <sys/wait.h>
#include "utils/wait.h"

namespace zas {
namespace utils {

waitobject::waitobject()
{
	pthread_cond_init(&cond, NULL);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mut, &attr);
	pthread_mutexattr_destroy(&attr);
}

waitobject::~waitobject()
{
	pthread_mutex_destroy(&mut);
	pthread_cond_destroy(&cond);
}

waitobject& waitobject::lock(void)
{
	pthread_mutex_lock(&mut);
	return *this;
}

waitobject& waitobject::unlock(void)
{
	pthread_mutex_unlock(&mut);
	return *this;
}

bool waitobject::wait(unsigned int msec)
{
	if (infinite == msec)
	{
		pthread_cond_wait(&cond, &mut);
		return true;
	}
	else
	{
		struct timespec timeout;

		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += msec / 1000;
		timeout.tv_nsec += (msec % 1000) * 1000000;
		timeout.tv_sec += timeout.tv_nsec / 1000000000;
		timeout.tv_nsec %= 1000000000;
		if (ETIMEDOUT == pthread_cond_timedwait(&cond, &mut, &timeout))
			return false;
		else return true;
	}
}

waitobject& waitobject::notify(void)
{
	pthread_cond_signal(&cond);
	return *this;
}

waitobject& waitobject::broadcast(void)
{
	pthread_cond_broadcast(&cond);
	return *this;
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_WAIT
/* EOF */
