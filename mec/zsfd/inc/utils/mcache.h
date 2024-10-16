/** @file mcache.h
 * Definition of the memory pool for fixed length data allocation
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_MEMCACHE) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#ifndef __CXX_ZAS_UTILS_MEMCACHE_H__
#define __CXX_ZAS_UTILS_MEMCACHE_H__

#include "utils/utils.h"

namespace zas {
namespace utils {

class memcache_impl;

// flags
// this means the slab structure
// will be contained in the data area
enum {
	mcacheflg_cache_compact = 1,
};

class UTILS_EXPORT memcache
{
	friend class memcache_impl;
public:
	/**
	  create the memory cache (pool)

	  @param name the name of the memory cache (pool)
	  		at most 16 charactors
	  @param sz the fixed size of memory object
	  @param maxitems the max items of memory object in this pool
	  @param flags: mcacheflg_cache_compact
	  */
	memcache(const char* name, size_t sz,
		size_t maxitems = 0, uint32_t flags = 0);
	virtual ~memcache();

public:

	/**
	  allocate a chunk for memory cache pool

	  @return the pointer to the allocated chunk, NULL for fail
	  */
	void* alloc(void);

	/**
	  free a chunk for memory cache pool

	  @param p the pointer to be freed
	  @return none
	  */
	void free(void* p);

protected:

	/**
	  initialize the object allocated
	  this function will be called every time when a new
	  object is allocated
	  Note: override this method to implement your constructor
	  if necessary

	  @param buf the pointer to the object
	  @param sz the size of the object
	  */
	virtual void ctor(void* buf, size_t sz);

	/**
	  destroy the object allocated
	  this function will be called every time before the
	  allocated object be freed
	  Note: override this method to implement your destructor
	  if necessary

	  @param buf the pointer to the object
	  @param sz the size of the object
	  */
	virtual void dtor(void* buf, size_t sz);

private:
	memcache_impl* _impl;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(memcache);
};

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_MEMCACHE_H__ */
#endif // (defined(UTILS_ENABLE_FBLOCK_MEMCACHE) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
