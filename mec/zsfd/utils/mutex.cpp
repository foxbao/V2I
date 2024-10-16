/** @file mutex.cpp
 * implementation of in process mutex and inter-process mutex
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_CMDLINE

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <semaphore.h>

#include "utils/mutex.h"
#include "utils/thread.h"
#include "utils/cmdline.h"

namespace zas {
namespace utils {

mutex::mutex()
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&_mut, &attr);
	pthread_mutexattr_destroy(&attr);
}

mutex::~mutex()
{
	pthread_mutex_destroy(&_mut);
}

/**
  lock the mutex
  */
int mutex::lock(void) {
	return pthread_mutex_lock(&_mut);
}

/**
  unlock the mutex
  */
int mutex::unlock(void) {
	return pthread_mutex_unlock(&_mut);
}

/**
 try to lock the mutex
 @return true if successfully acquired, or false if already locked
*/
bool mutex::trylock(void)
{
	int ret = pthread_mutex_trylock(&_mut);
	return (!ret) ? true : false;
}

semaphore::semaphore(const char* name, int value)
: _sem(NULL)
, _tid(0), _refcnt(0) {
	if (NULL == name) return;
	size_t len = strlen(name);
	if (len > 230) return;

	// check if there is '/' in the name
	for (const char* t = name; *t; ++t) {
		if (*t == '/') return;
	}

	char* buf = (char*)alloca(len + 16);
	strcpy(buf, "/zas-");
	memcpy(buf + 5, name, len + 1);
	_sem = (void*)sem_open(buf, O_CREAT, 0666, value);
}

semaphore::~semaphore()
{
	if (_sem) {
		sem_close((sem_t*)_sem);
		_sem = NULL;
	}
}

int semaphore::lock(void)
{
	if (!_sem) return -1;
	unsigned long int ctid = gettid();
	if (ctid != _tid) {
		int ret = sem_wait((sem_t*)_sem);
		if (ret) return ret;

		// we are in lock status
		_tid = ctid;
		_refcnt = 1;
		return 0;
	}

	// we are the same thread
	++_refcnt;
	return 0;
}

bool semaphore::trylock(void)
{
	if (!_sem) return false;
	if (sem_trywait((sem_t*)_sem) && errno == EAGAIN)
		return false;
	return true;
}

int semaphore::unlock(void)
{
	if (!_sem) return -1;

	unsigned long int ctid = gettid();
	if (ctid == _tid) {
		if (--_refcnt <= 0) {
			_tid = 0;
			return sem_post((sem_t*)_sem);
		}
	}
	return 0;
}

static void do_reset_semaphore(void)
{
	DIR *db;
	struct dirent *p;

	db = opendir("/dev/shm");
	if (NULL == db) return;

	while (NULL != (p=readdir(db)))
	{
		if(!strcmp(p->d_name,".") || !strcmp(p->d_name,".."))
			continue;

		if (!strncmp(p->d_name, "sem.zas-", 8)) {
			p->d_name[3] = '/';
			sem_unlink(&p->d_name[3]);
			p->d_name[3] = '.';
		}
	}
	closedir(db);
}

void reset_semaphore(void)
{
	cmdline_option opts[] = {
		{ "reset", no_argument, NULL, 'r' },
		{ 0, 0, NULL, 0},
	};

	int i, c;
	auto* cmd = cmdline::inst();
	cmd->reset();
	while ((c = cmd->getopt_long(NULL, opts, &i)) != -1)
	{
		if (c == 'r') {
			do_reset_semaphore();
			return;
		}
	}
}

}}; // end of zas::utils
#endif // UTILS_ENABLE_FBLOCK_CMDLINE
/* EOF */
