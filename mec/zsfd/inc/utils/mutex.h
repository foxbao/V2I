/** @file mutex.h
 * Definition of in process mutex and inter-process mutex
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_CMDLINE

#ifndef __CXX_ZAS_UTILS_MUTEX_H__
#define __CXX_ZAS_UTILS_MUTEX_H__

#include <pthread.h>
#include "utils/utils.h"

namespace zas {
namespace utils {

class UTILS_EXPORT mutex
{
public:
	mutex();
	~mutex();

public:

	/**
	 lock the mutex
	*/
	int lock(void);

	/**
	 try to lock the mutex
	 @return true if successfully acquired, or false if already locked
	*/
	bool trylock(void);

	/**
	 unlock the mutex
	*/
	int unlock(void);

private:
	pthread_mutex_t _mut;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(mutex);
};

class auto_mutex
{
public:
	auto_mutex(mutex& mut) {
		mut.lock();
		_mut = &mut;
	}

	auto_mutex(mutex* mut) {
		if (mut) {
			mut->lock();
			_mut = mut;
		} else _mut = NULL;
	}
	
	~auto_mutex() {
		if (_mut) {
			_mut->unlock();
			_mut = NULL;
		}
	}

private:
	mutex* _mut;
};

#define MUTEX_ENTER(mut)	{ zas::utils::auto_mutex am(mut)
#define MUTEX_EXIT()		}

class UTILS_EXPORT semaphore
{
public:
	semaphore(const char* name, int value = 1);
	~semaphore();

public:

	/**
	 lock the semaphore
	*/
	int lock(void);

	/**
	 try to lock the semaphore
	 @return true if successfully acquired, or false if already locked
	*/
	bool trylock(void);

	/**
	 unlock the semaphore
	*/
	int unlock(void);

private:
	void* _sem;
	unsigned long int _tid;
	uint32_t _refcnt;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(semaphore);
};

class auto_semaphore
{
public:
	auto_semaphore(semaphore& sem) {
		sem.lock();
		_sem = &sem;
	}

	auto_semaphore(semaphore* sem) {
		if (sem) {
			sem->lock();
			_sem = sem;
		} else _sem = NULL;
	}
	
	~auto_semaphore() {
		if (_sem) {
			_sem->unlock();
			_sem = NULL;
		}
	}

private:
	semaphore* _sem;
};

#define SEM_ENTER(sem)	{ zas::utils::auto_semaphore as(sem)
#define SEM_EXIT()		}

}}; // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_MUTEX_H__ */
#endif // UTILS_ENABLE_FBLOCK_CMDLINE
/* EOF */