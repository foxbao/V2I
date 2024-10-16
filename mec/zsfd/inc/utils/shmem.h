/** @file shmem.h
 * Definition of in process share memory
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_SHRMEM) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#ifndef __CXX_ZAS_UTILS_SHARE_MEMORY_H__
#define __CXX_ZAS_UTILS_SHARE_MEMORY_H__

#include "utils/utils.h"

namespace zas {
namespace utils {

enum shmem_access
{
	shmem_read = 1,
	shmem_write = 2,
	shmem_rdwr = shmem_read | shmem_write,
	sheme_private = 4,
};

class UTILS_EXPORT shared_memory
{
public:
	shared_memory() = delete;
	~shared_memory() = delete;

	/**
	  Releae the object
	 */
	void release(void);

	/**
	  Create a share memory object
	  @param name the share memory object name
	  @param sz the size of share memory
	  @param flags the flags
	  @param addr the address specified for mapping
		NULL means let kernel choose the address
	  @return the share memory object
	 */
	static shared_memory* create(const char* name,
		size_t sz, uint32_t flags = shmem_rdwr,
		void* addr = NULL);

	/**
	  Get the address of the shared memory object
	  @return the address
	 */
	void* getaddr(void);

	/**
	  Get the size of the shared memory object
	  @return the size
	 */
	size_t getsize(void);

	/**
	  Check if current process is the creator of
	  the shared memory object
	  @return true for creator
	 */
	bool is_creator(void);

	/**
	  Release and reset the shared memory object
	  specified by name
	  @return 0 for success
	 */
	static int reset(const char* name);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(shared_memory);
};

}}; // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_SHARE_MEMORY_H__ */
#endif // (defined(UTILS_ENABLE_FBLOCK_SHRMEM) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */