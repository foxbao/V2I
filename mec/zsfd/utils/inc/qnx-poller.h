#ifndef __CXX_UTILS_EVENT_POLLER_H__
#define __CXX_UTILS_EVENT_POLLER_H__

#ifdef QNX_PLATEFORM

#include "utils/utils.h"
#include <sys/poll.h>
#include "utils/gbuf.h"
#include <time.h>
#include <vector>
#include "utils/avltree.h"
#include "utils/mutex.h"

namespace zas {
namespace utils {

#define POLL_ITEM_MAX_COUNT_INIT (16)
#define EPOLLERR POLLERR
#define EPOLLHUP POLLHUP
#define EPOLLPRI POLLPRI
#define EPOLLRDHUP POLLRDHUP
#define EPOLLIN POLLIN
#define EPOLLOUT POLLOUT

#define EPOLL_CTL_ADD (1)
#define EPOLL_CTL_DEL (2)
#define EPOLLET (0)
#define EPOLL_CLOEXEC (02000000)

#define TFD_CLOEXEC (02000000)
#define TFD_NONBLOCK (00004000)
#define TIMER_MIN_INTERVAL_MS (100)

#define EFD_CLOEXEC (02000000)
#define EFD_NONBLOCK (00004000)

typedef union epoll_data
{
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

struct epoll_event
{
  uint32_t events;
  epoll_data_t data;
} ;

typedef zas::utils::gbuf_<pollfd> event_pollitem_set;

struct event_poller_avlnode
{
	// todo: remove the node in dtor
	avl_node_t avlnode;
	size_t key;
	int index;
	static int compare(avl_node_t*, avl_node_t*);
};


struct event_poller_node
{
	event_poller_avlnode* avl_entry;
	int fd;
	epoll_data_t data;
};

typedef zas::utils::gbuf_<event_poller_node> eventpoller_set;

class poller_items
{
public:
	poller_items();
	~poller_items();

	event_poller_node* get_event_node(int i, pollfd** item);
	
	// add a new sock or fd item
	int additem(int fd,	int events, epoll_data_t &data);

	// release a fd
	int release(int fd);

public:

	int count(void) {
		auto_mutex am(_mut);
		return _items.getsize();
	}

	pollfd* get_pollset(void) {
		auto_mutex am(_mut);
		return _items.buffer();
	}

	int get_id_by_fd(int fd);

private:
	void release_all(void);
	bool empty_slot(int slot);

	void release_byid(int id);

private:
	// -1 means nullptr
	std::vector<short> _freelist;

	// all poll items
	event_pollitem_set _items;
	eventpoller_set	_event_list;
	// all poller fd will be
	// added to this tree
	avl_node_t* _fd_tree;
	mutex	_mut;
};

int timerfd_create(int id, int flags);
int timerfd_settime(int fd, int flags, 
	const struct itimerspec *__utmr,
	struct itimerspec *__otmr); 

int epoll_create1 (int __flags);
int epoll_ctl (int __epfd, int __op, int __fd, struct epoll_event *__event);
int epoll_wait (int __epfd, struct epoll_event *__events,
		int __maxevents, int __timeout);

int eventfd(unsigned int __count, int __flags);

ssize_t __read (int __fd, void *__buf, size_t __nbytes);
ssize_t __write (int __fd, const void *__buf, size_t __n);
int __close (int __fd);

}} /* zas::utils*/


#endif /* QNX_PLATEFORM */
#endif /* __CXX_UTILS_EVENT_POLLER_H__ */