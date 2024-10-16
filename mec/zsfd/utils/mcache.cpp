/** @file mcache.cpp
 * implementation of the memory pool for fixed length data allocation
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_MEMCACHE) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))

#include <assert.h>
#include "std/list.h"
#include "utils/mutex.h"
#include "utils/mcache.h"
#include "utils/avltree.h"

namespace zas {
namespace utils {

struct slab_t;
struct slab_freelist_node_t;
class memcache_impl;

// global mutex for avltree operation for slabs
mutex slab_avltree_mut;

struct slab_freelist_node_t
{
	struct slab_freelist_node_t *next;
};

struct slab_t
{
	listnode_t ownerlist;
	avl_node_t avlnode;

	memcache_impl *mcache;
	void* buffer;
	uint32_t  first;
	slab_freelist_node_t freelist;

	// items that has been allocated
	uint32_t  used;
};

enum {
	mcache_name_max_len = 16,
	mcache_min_node_size = 16,
	mcache_node_alignment = 4,
	page_size = 4096,
	slab_min_items = 8,
	slab_max_items = 256,
	slab_good_pages = 8,
	slab_heavy_pages = 16,
};

class memcache_impl
{
public:
	memcache_impl(memcache* mc, const char* name,
		size_t sz, size_t maxitems, uint32_t flags)
	: _mcache(mc)
	{
		init_mcache(name, sz, maxitems, flags);
	}

	~memcache_impl()
	{

	}

public:

	void* alloc(void)
	{
		void* ret;
		slab_t *slab;
		listnode_t* node;
		slab_t *empty_slab = NULL;

		// we need lock for allocation
		auto_mutex am(_mut);

		if (listnode_isempty(_slabs_partial))
		{
			// see if there is any node in empty list
			if (listnode_isempty(_slabs_empty))
				mcache_expand();

			if (listnode_isempty(_slabs_empty))
				return NULL;

			node = _slabs_empty.next;
			slab = list_entry(slab_t, ownerlist, node);
			empty_slab = slab;
		}
		else
		{
			node = _slabs_partial.next;
			slab = list_entry(slab_t, ownerlist, node);
		}

		// allocate a node from slab
		ret = mcache_slab_alloc(slab);
		if (ret)
		{
			// re-arrange the slab to its list
			if (empty_slab)
				mcache_move_to_partial_list(slab);
			else if (mcache_is_empty_slab(slab))
				mcache_move_to_full_list(slab);
		}

		// initialize the object
		if (_mcache) _mcache->ctor(ret, _size);
		return ret;		
	}

	static void free(void *p)
	{
		slab_t *slab;
		slab_t dummyslab;
		avl_node_t* node;

		if (NULL == p)
			return;

		// find the slab for this pointer
		dummyslab.buffer = p;

		// need lock
		slab_avltree_mut.lock();
		node = avl_find(slab_avltree_header, &dummyslab.avlnode, slab_avltree_compare);
		slab_avltree_mut.unlock();
		
		if (NULL == node)
		{
			// todo: dbg_output1("kmem_cache_free: pointer (%x) not found in slab.\n", (uint)p);
			return;
		}

		slab = AVLNODE_ENTRY(slab_t, avlnode, node);
		assert(NULL != slab->mcache);
		memcache_impl* mcache = slab->mcache;

		// call the dtor
		mcache->_mcache->dtor(p, mcache->_size);

		mcache->_mut.lock();
		mcache->mcache_release_node(slab, p);
		mcache->_mut.unlock();		
	}

private:

	void init_mcache(const char *name, size_t sz, size_t maxitems, uint32_t flags)
	{	
		assert(NULL != name);
		size_t len = strlen(name);
		if (len > mcache_name_max_len - 1)
		{
			memcpy(_name, name, mcache_name_max_len - 1);
			_name[mcache_name_max_len - 1] = '\0';
		}
		else strcpy(_name, name);

		// init list nodes
		listnode_init(_ownerlist);
		listnode_init(_slabs_partial);
		listnode_init(_slabs_empty);
		listnode_init(_slabs_full);

		sz = (sz < mcache_min_node_size) ? mcache_min_node_size : sz;
		_size = (sz + mcache_node_alignment - 1) & ~(mcache_node_alignment - 1);
		_coloroff = calc_slab_pages(_size, &(_pages), &(_objects), flags);

		if (!maxitems) return;

		// if user specify the max items for one slab
		// the "maxitem" is used for user to specify a smaller slab
		// so if the maxitem is larger than the one calculated by system
		// then we'll continue using the one calculated by system
		if (maxitems > _objects)
			return;

		// calculate the coloroff
		if (flags & mcacheflg_cache_compact) _coloroff = (sizeof(slab_t) + 15) & ~15;
		else _coloroff = calc_slab_coloroff_with_max_items(_size, maxitems);

		_objects = maxitems;
		_pages = (_size * _objects + _coloroff + page_size - 1) / page_size;
	}


	// the return of this function indicates
	// color-off
	// the sz shall already align with "mcache_node_alignment"
	uint32_t calc_slab_pages(size_t sz, uint32_t* pages, uint32_t* objs, uint32_t flags)
	{
		uint32_t items;
		uint32_t coloroff = (flags & mcacheflg_cache_compact)
			? ((sizeof(slab_t) + 15) & ~15)
			: calc_slab_coloroff(sz);

		// firstly, determine the pages
		// criteria:
		// 1) at least contain SLAB_MIN_ITEMS
		// 2) contain no more than SLAB_MAX_ITEMS
		// 3) try to control pages less than SLAB_GOOD_PAGES

		items = slab_max_items;
		if (sz * items + coloroff <= slab_good_pages * page_size)
			goto calc_pages_ok;

		// try 2/3 of max item
		items = (2 * slab_max_items + slab_min_items) / 3;
		if (sz * items + coloroff <= slab_good_pages * page_size)
			goto calc_pages_ok;
		
		// try 1/2 of max item
		items = (slab_max_items + slab_min_items) / 2;
		if (sz * items + coloroff <= slab_heavy_pages * page_size)
			goto calc_pages_ok;

		// try 1/3 of max item
		items = (slab_max_items + 2 * slab_min_items) / 3;
		if (sz * items + coloroff <= slab_heavy_pages * page_size)
			goto calc_pages_ok;

		// use min item
		items = slab_min_items;

	calc_pages_ok:

		assert(NULL != pages && NULL != objs);
		*pages = ((items * sz + coloroff + page_size - 1) & ~(page_size - 1)) / page_size;
		*objs = (*pages * page_size - coloroff) / sz;
		return coloroff;
	}

	uint32_t calc_slab_coloroff(size_t sz)
	{
		const uint coloroff = (sizeof(slab_t) + 15) & ~15;
		if (sz < coloroff) return coloroff;
		if (sz < page_size) {
			if ((page_size % sz) < coloroff)
				return 0;
			else return coloroff;
		}
		else {
			if ((sz % page_size) == 0)
				return 0;
			else return coloroff;
		}
	}

	// the sz shall already align with mcache_NODE_ALIGNMENT
	uint32_t calc_slab_coloroff_with_max_items(size_t sz, size_t maxitems)
	{
		const uint32_t total_sz = sz * maxitems;
		const uint32_t coloroff = (sizeof(slab_t) + 15) & ~15;
		const uint32_t pgs_without_coloroff = (total_sz + page_size - 1) / page_size;
		const uint32_t pgs_with_coloroff = (total_sz + coloroff + page_size - 1) / page_size;

		if (pgs_with_coloroff == pgs_without_coloroff)
			return coloroff;
		else return 0;
	}

	// note: this function is not locked
	void mcache_expand(void)
	{
		int ret;
		void* buffer;
		slab_t *slab;

		// allocate the buffer for pages
		buffer = (void*)malloc(page_size * _pages);
		if (NULL == buffer) return;

		// allocate the slab structure if necessary
		if (!_coloroff)
		{
			slab = (slab_t *)malloc(sizeof(slab_t));
			if (NULL == slab)
			{
				free(buffer);
				return;
			}
		}
		else slab = (slab_t *)buffer;

		// initialize the slab
		slab->mcache = this;
		slab->buffer = (void*)((size_t)buffer + _coloroff);
		slab->first = 0;
		slab->freelist.next = NULL;
		slab->used = 0;

		listnode_add(_slabs_empty, slab->ownerlist);
		
		slab_avltree_mut.lock();
		ret = avl_insert(&slab_avltree_header, &(slab->avlnode),
			memcache_impl::slab_avltree_insert_cmp);
		slab_avltree_mut.unlock();

		if (ret) bug("slab: fail to insert slab: %x\n", slab->buffer);
	}

	static int slab_avltree_insert_cmp(avl_node_t* a, avl_node_t* b)
	{
		size_t f1, f2, s1, s2;
		slab_t *first = AVLNODE_ENTRY(slab_t, avlnode, a);
		slab_t *second = AVLNODE_ENTRY(slab_t, avlnode, b);

		f1 = ((size_t)first->buffer) & ~(page_size - 1);
		f2 = f1 + first->mcache->_pages * page_size;
		s1 = ((size_t)second->buffer) & ~(page_size - 1);
		s2 = s1 + second->mcache->_pages * page_size;
		if (f2 <= s1) return -1;
		else if (f1 >= s2) return 1;
		else return 0;
	}

	static int slab_avltree_compare(avl_node_t* a, avl_node_t* b)
	{
		memcache_impl* cache;
		slab_t *first = AVLNODE_ENTRY(slab_t, avlnode, a);
		slab_t *second = AVLNODE_ENTRY(slab_t, avlnode, b);

		cache = second->mcache;
		if ((size_t)first->buffer < (size_t)second->buffer)
			return -1;
		else if ((size_t)first->buffer >= (size_t)second->buffer + cache->_pages * page_size)
			return 1;
		else return 0;
	}

	// this function is not locked
	// this function does not deal with slab status (like partial -> full)
	void* mcache_slab_alloc(slab_t *slab)
	{
		void* ret;

		assert(NULL != slab);
		if (slab->first >= _objects)
		{
			slab_freelist_node_t *node;

			// see if there is any node in freelist
			if (NULL == slab->freelist.next)
				return NULL;

			// remove node from freelist
			node = slab->freelist.next;
			slab->freelist.next = node->next;
			ret = (void*)node;
		}
		else
		{
			uint idx = slab->first++;
			ret = (void*)(((size_t)slab->buffer) + idx * _size);
		}

		slab->used++;
		return ret;
	}

	// this function is not locked
	void mcache_move_to_full_list(slab_t *slab)
	{
		assert(NULL != slab);
		listnode_del(slab->ownerlist);
		listnode_add(_slabs_full, slab->ownerlist);
	}

	// this function is not locked
	void mcache_move_to_partial_list(slab_t *slab)
	{
		assert(NULL != slab);
		listnode_del(slab->ownerlist);
		listnode_add(_slabs_partial, slab->ownerlist);
	}

	// this function is not locked
	void mcache_move_to_empty_list(slab_t *slab)
	{
		assert(NULL != slab);
		listnode_del(slab->ownerlist);
		listnode_add(_slabs_empty, slab->ownerlist);
	}

	// this function is not locked
	bool mcache_is_empty_slab(slab_t *slab)
	{
		assert(NULL != slab);
		return (slab->used >= _objects) ? true : false;
	}

	// this function is not locked
	void mcache_release_node(slab_t *slab, void* p)
	{
		slab_freelist_node_t *node = (slab_freelist_node_t*)p;
	
		node->next = slab->freelist.next;
		slab->freelist.next = node;

		// if the slab is full before relaseing this node
		// we need to put it to partial list
		if (slab->used == _objects)
		{
			--slab->used;
			mcache_move_to_partial_list(slab);
			return;
		}

		--slab->used;
		// see if this become an empty slab
		if (!slab->used)
			mcache_move_to_empty_list(slab);
	}

private:

	static listnode_t mcache_list;
	static avl_node_t* slab_avltree_header;

	char _name[mcache_name_max_len];

	listnode_t _ownerlist;
	listnode_t _slabs_partial;
	listnode_t _slabs_empty;
	listnode_t _slabs_full;

	mutex _mut;

	uint32_t _pages;			// number of pages in one slab
	uint32_t _objects;			// number of objects in one slab
	uint32_t _size;				// size of the object
	uint32_t _coloroff;			// color off for slab

	memcache* _mcache;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(memcache_impl);
};

// static data initialization
listnode_t memcache_impl::mcache_list = LISTNODE_INITIALIZER( \
	memcache_impl::mcache_list);
avl_node_t* memcache_impl::slab_avltree_header = NULL;

memcache::memcache(const char* name, size_t sz,
	size_t maxitems, uint32_t flags)
{
	_impl = new memcache_impl(this, name, sz, maxitems, flags);
	assert(NULL != _impl);
}

memcache::~memcache()
{
	if (_impl) delete _impl;
}

void* memcache::alloc(void)
{
	if (!_impl) return NULL;
	return _impl->alloc();
}

void memcache::free(void* p) {
	memcache_impl::free(p);
}

void memcache::ctor(void* buf, size_t sz) {}
void memcache::dtor(void* buf, size_t sz) {}

}}; // end of namespace zas::utils
#endif // (defined(UTILS_ENABLE_FBLOCK_MEMCACHE) && defined(UTILS_ENABLE_FBLOCK_CMDLINE))
/* EOF */
