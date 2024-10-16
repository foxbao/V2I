#include "qnx-poller.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include <map>
#include <vector>
#include "utils/timer.h"
#include <errno.h>

#ifdef QNX_PLATEFORM

namespace zas {
namespace utils {

struct qnx_timer_info {
	FILE* _fp = nullptr;
	int _fd = -1;
	int _id;
	int _flags;
	epoll_data_t data;
};
static qnx_timer_info _s_qnx_timer_info;

struct qnx_event_info
{
	FILE* _fp = nullptr;
	int _fd = -1;
	int _count = 0;
	int _flags = 0;
	int _bwrite = false;
	epoll_data_t data;
};

struct qnx_epoll_info
{
	FILE* _fp = nullptr;
	int _fd = -1;
	std::map<int, std::shared_ptr<qnx_event_info>> _events;
	std::shared_ptr<poller_items> _items = nullptr;
	std::vector<std::shared_ptr<epoll_event>> _left_events;
	long _tickcount = -1;
	int _timeout = 0;
};

static qnx_epoll_info _s_qnx_epoll_info;


poller_items::poller_items()
: _items(POLL_ITEM_MAX_COUNT_INIT)
, _event_list(POLL_ITEM_MAX_COUNT_INIT)
, _fd_tree(nullptr)
{
	_freelist.clear();
}

poller_items::~poller_items()
{
	release_all();
}

event_poller_node* poller_items::get_event_node(int i, pollfd** item)
{
	auto_mutex am(_mut);
	int cnt = _event_list.getsize();
	if (i >= cnt) {
		return nullptr;
	}
	auto* pollitem = &_items.buffer()[i];
	if (pollitem->fd < 0) {
		return nullptr;
	}
	auto& eventnode = _event_list.buffer()[i];
	if (item) *item = pollitem;

	return &eventnode;
}

int event_poller_avlnode::compare(avl_node_t* a, avl_node_t* b)
{
	auto* aa = AVLNODE_ENTRY(event_poller_avlnode, avlnode, a);
	auto* bb = AVLNODE_ENTRY(event_poller_avlnode, avlnode, b);
	if (aa->key > bb->key) {
		return 1;
	} else if (aa->key < bb->key) {
		return -1;
	} else return 0;
}

int poller_items::additem(int fd, int events, epoll_data_t &data)
{
	auto_mutex am(_mut);
	if (fd < 0) {
		return -1;
	}

	int ret = -4;
	pollfd* item = nullptr;
	event_poller_node* epn = nullptr;

	if (!_freelist.empty()) {
		ret = _freelist.back();
		_freelist.pop_back();
		assert(ret < _items.getsize());
		item = &_items.buffer()[ret];
		epn = &_event_list.buffer()[ret];
	}
	else {
		// allocate a new one
		item = _items.new_object();
		epn = _event_list.new_object();
		if (epn) {
			// before doing anything else, we make
			// sure the avl_entry is allocated
			epn->avl_entry = new event_poller_avlnode;
			if (nullptr == epn->avl_entry) {
				_event_list.remove(1);
				if (item) _items.remove(1);
				return -2;
			}
		}

		ret = _items.getsize();
		assert(ret == _event_list.getsize());
		--ret; // the last one
	}

	if (nullptr == item || nullptr == epn) {
		return -3;
	}


	epn->fd = fd;
	epn->data.ptr = data.ptr;
	item->fd = fd;
	item->events = events;

	epn->avl_entry->index = ret;
	epn->avl_entry->key = (size_t)epn->fd;

	int val = avl_insert(&_fd_tree,
		&epn->avl_entry->avlnode, event_poller_avlnode::compare);
	assert(val == 0);
	return ret;
}

int poller_items::release(int fd)
{
	auto_mutex am(_mut);
	int id = get_id_by_fd(fd);
	if (id < 0) return id;

	release_byid(id);
	return id;
}

void poller_items::release_byid(int id)
{
	// clear and add item to freelist
	auto* epn = &_event_list.buffer()[id];
	avl_remove(&_fd_tree, &epn->avl_entry->avlnode);
	epn->fd = -1;
	auto& item = _items.buffer()[id];
	item.fd = -1;
	item.events = 0;
	item.revents = 0;
	_freelist.push_back(id);
}

int poller_items::get_id_by_fd(int fd)
{
	auto_mutex am(_mut);
	if (fd < 0) {
		return -1;
	}

	event_poller_avlnode dummy;
	dummy.key = fd;

	avl_node_t* avl = avl_find(_fd_tree, &dummy.avlnode,
		event_poller_avlnode::compare);
	if (nullptr == avl) {
		return -2;
	}

	auto* node = AVLNODE_ENTRY(event_poller_avlnode, avlnode, avl);
	return node->index;
}

void poller_items::release_all(void)
{
	int i, cnt = _items.getsize();
	for (i = 0; i < cnt; ++i) {
		int fd = _items.buffer()[i].fd;
		if (fd >= 0) close(fd);
	}
	// release socket tree
	for (auto* node = avl_first(_fd_tree); node;) {
		avl_remove(&_fd_tree, node);
		auto* avl = AVLNODE_ENTRY(event_poller_avlnode, avlnode, node);
		delete avl;
	}
}

bool poller_items::empty_slot(int slot)
{
	int cnt = _items.getsize();
	assert(cnt == _event_list.getsize());
	
	if (slot >= cnt) {
		return true;
	}

	pollfd& item = _items.buffer()[slot];
	if (item.fd < 0) {
		return true;
	}
	return false;
}

int timerfd_create(int id, int flags)
{
	if (_s_qnx_timer_info._fd >= 0) {
		return -ENOTAVAIL;
	}
	FILE *fp = tmpfile();
	if (nullptr == fp) {
		return -ELOGIC;
	}
	if (fp->_fileno <= 0) {
		fclose(fp);
		return -ELOGIC;
	}
	_s_qnx_timer_info._fp = fp;
	_s_qnx_timer_info._fd = fp->_fileno;
	_s_qnx_timer_info._id = id;
	_s_qnx_timer_info._flags = flags;
	_s_qnx_timer_info.data.ptr = nullptr;
	return fp->_fileno;
}

int timerfd_settime(int fd, int flags, 
	const struct itimerspec *__utmr,
	struct itimerspec *__otmr)
{
	return 0;
}

int epoll_create1 (int __flags)
{
	if (_s_qnx_epoll_info._fd >= 0) {
		return -EEXISTS;
	}
	_s_qnx_epoll_info._fp = tmpfile();
	if (nullptr == _s_qnx_epoll_info._fp) {
		return -ELOGIC;
	}
	_s_qnx_epoll_info._fd = _s_qnx_epoll_info._fp->_fileno;
	if (_s_qnx_epoll_info._fd <= 0) {
		fclose(_s_qnx_epoll_info._fp);
		return -ELOGIC;
	}
	_s_qnx_epoll_info._items = std::make_shared<poller_items>();
	_s_qnx_epoll_info._timeout = TIMER_MIN_INTERVAL_MS;
	_s_qnx_epoll_info._left_events.clear();	
	return _s_qnx_epoll_info._fd;
}

int epoll_ctl(int __epfd, int __op, int __fd, struct epoll_event *evt)
{
	if (__fd < 0) {
		return -EBADPARM;
	}

	if (_s_qnx_epoll_info._fd != __epfd) {
		return -ENOTAVAIL;
	}

	if (_s_qnx_timer_info._fd == __fd) {
		// timer fd
		_s_qnx_timer_info.data.ptr = evt->data.ptr;
		return 0;
	}

	if (_s_qnx_epoll_info._events.count(__fd) > 0) {
		// event fd
		_s_qnx_epoll_info._events[__fd]->data.ptr = evt->data.ptr;
		return 0;
	}
	
	int index = _s_qnx_epoll_info._items->get_id_by_fd(__fd);
	if (EPOLL_CTL_ADD == __op) {
		if (index >= 0) {
			return -EEXISTS;
		}
		_s_qnx_epoll_info._items->additem(__fd, evt->events, evt->data);
	}
	else if (EPOLL_CTL_DEL == __op) {
		if (index < 0) {
			return 0;
		}
		_s_qnx_epoll_info._items->release(__fd);
	}
	else {
		return -ENOTHANDLED;
	}
	return 0;
}

int eventfd(unsigned int __count, int __flags)
{
	FILE *fp = tmpfile();
	if (nullptr == fp) {
		return -ELOGIC;
	}

	if (fp->_fileno <= 0) {
		fclose(fp);
		return -ELOGIC;
	}

	auto evt_item = std::make_shared<qnx_event_info>();
	evt_item->_fp = fp;
	evt_item->_fd = fp->_fileno;
	evt_item->_count = __count;
	evt_item->_flags = __flags;
	_s_qnx_epoll_info._events[fp->_fileno] = evt_item;
	return fp->_fileno;
}

int checkset_timeout_before_poll(void)
{
	if (_s_qnx_epoll_info._tickcount == -1) {
		_s_qnx_epoll_info._tickcount = gettick_millisecond();
		_s_qnx_epoll_info._timeout = TIMER_MIN_INTERVAL_MS;
	} 
	auto deltatm = gettick_millisecond() - _s_qnx_epoll_info._tickcount;
	_s_qnx_epoll_info._tickcount = gettick_millisecond();
	if (deltatm > _s_qnx_epoll_info._timeout) {
		// timeout reset
		_s_qnx_epoll_info._timeout = TIMER_MIN_INTERVAL_MS - 
			(deltatm - _s_qnx_epoll_info._timeout) % TIMER_MIN_INTERVAL_MS;
		// timeout action
		if (nullptr == _s_qnx_timer_info.data.ptr) {
			return 0;
		}
		return 1;
	}
	else {
		_s_qnx_epoll_info._timeout -= deltatm;
	}
	return 0;
}

int checkset_timeout_after_poll(void)
{
	if (_s_qnx_epoll_info._tickcount == -1) {
		_s_qnx_epoll_info._tickcount = gettick_millisecond();
		_s_qnx_epoll_info._timeout = TIMER_MIN_INTERVAL_MS;
	} 
	auto deltatm = gettick_millisecond() - _s_qnx_epoll_info._tickcount;
	_s_qnx_epoll_info._tickcount = gettick_millisecond();
	if (deltatm > _s_qnx_epoll_info._timeout) {
		_s_qnx_epoll_info._timeout = TIMER_MIN_INTERVAL_MS - 
			(deltatm - _s_qnx_epoll_info._timeout) % TIMER_MIN_INTERVAL_MS;
	}
	else {
		_s_qnx_epoll_info._timeout -= deltatm;
		_s_qnx_epoll_info._timeout += TIMER_MIN_INTERVAL_MS;
	}
	return 0;
}

int epoll_wait (int __epfd, struct epoll_event *__events,
		int __maxevents, int __timeout)
{
	if (_s_qnx_epoll_info._fd < 0) {
		return -ENOTALLOWED;
	}
	auto items = _s_qnx_epoll_info._items;
	if (items->count() <= 0) {
		_s_qnx_epoll_info._left_events.clear();
	}

	// 提取剩余的event事件
	if (_s_qnx_epoll_info._left_events.size() > 0) {
		int eventcnt = 0;
		while (!_s_qnx_epoll_info._left_events.empty()) {
			auto shr_event = _s_qnx_epoll_info._left_events.back();
			_s_qnx_epoll_info._left_events.pop_back();
			__events[eventcnt].events = shr_event->events;
			__events[eventcnt].data.ptr = shr_event->data.ptr;
			eventcnt++;
			if (eventcnt >= __maxevents) {
				return __maxevents;
			}
		}
		return eventcnt;
	}

	if (checkset_timeout_before_poll()) {
		__events[0].events = POLLIN;
		__events[0].data.ptr = _s_qnx_timer_info.data.ptr;
		return 1;
	}

	// eventfd事件处理
	int evcnt = 0;
	for (const auto& eventfd:_s_qnx_epoll_info._events) {
		if (eventfd.second->_bwrite) {
			__events[evcnt].events = POLLIN;
			__events[evcnt].data.ptr = eventfd.second->data.ptr;
			evcnt++;
			eventfd.second->_bwrite = false;
		}
	}
	if (evcnt > 0) {
		return evcnt;
	}

	int icnt = items->count();
	auto pollfd = items->get_pollset();
	int ret = poll(pollfd, icnt, _s_qnx_epoll_info._timeout);

	if (0 == ret) {	// timeout
		checkset_timeout_after_poll();
		__events[0].events = POLLIN;
		__events[0].data.ptr = _s_qnx_timer_info.data.ptr;
		return 1;
	}
	else if (ret > 0) {	// select
		int eventcnt = 0;
		for (int i = 0; i < icnt; i++) {
			if (pollfd[i].fd >= 0 && pollfd[i].revents != 0) {
				if (eventcnt < __maxevents) {
					int index = items->get_id_by_fd(pollfd[i].fd);
					if (index < 0) {
						continue;
					}
					auto node = items->get_event_node(index, nullptr);
					if (nullptr == node) {
						continue;
					}
					__events[eventcnt].events = pollfd[i].revents;
					__events[eventcnt].data.ptr = node->data.ptr;
				}
				else {
					int index = items->get_id_by_fd(pollfd[i].fd);
					if (index < 0) {
						continue;
					}
					auto node = items->get_event_node(index, nullptr);
					if (nullptr == node) {
						continue;
					}
					auto shr_event = std::make_shared<epoll_event>();
					shr_event->events = pollfd[i].revents;
					shr_event->data.ptr = node->data.ptr;
					_s_qnx_epoll_info._left_events.push_back(shr_event);
				}
				eventcnt++;
			}
		}
		if (eventcnt > ret) {
			printf("ERROR: poll select error\n");
		}
		if (eventcnt > __maxevents) {
			return __maxevents;
		}
		return eventcnt;
	}
	else {	// error
		printf("poll error %d\n", errno);
		return ret;
	}
	return 0;
}

bool is_internal_fd(int __fd)
{
	if (_s_qnx_epoll_info._fd == __fd) {
		return true;
	}
	if (_s_qnx_timer_info._fd == __fd) {
		return true;
	}
	if (_s_qnx_epoll_info._events.count(__fd) > 0) {
		return true;
	}
	return false;
}

ssize_t __read (int __fd, void *__buf, size_t __nbytes)
{
	if (is_internal_fd(__fd)) {
		return 8;
	}
	return ::read(__fd, __buf, __nbytes);
}

ssize_t __write (int __fd, const void *__buf, size_t __n)
{
	if (_s_qnx_epoll_info._fd == __fd) {
		return __n;
	}
	if (_s_qnx_timer_info._fd == __fd) {
		return __n;
	}
	if (_s_qnx_epoll_info._events.count(__fd) > 0) {
		_s_qnx_epoll_info._events[__fd]->_bwrite = true;
		return __n;
	}
	return ::write(__fd, __buf, __n);
}

int __close (int __fd)
{
	if (_s_qnx_epoll_info._events.count(__fd) > 0) {
		fclose(_s_qnx_epoll_info._events[__fd]->_fp);
		_s_qnx_epoll_info._events.erase(__fd);
		return 0;
	}
	if (_s_qnx_timer_info._fd == __fd) {
		fclose(_s_qnx_timer_info._fp);
		_s_qnx_timer_info._fd = -1;
		_s_qnx_timer_info._fp = nullptr;
		_s_qnx_timer_info.data.ptr = nullptr;
		_s_qnx_timer_info._id = -1;
		return 0;
	}
	if (_s_qnx_epoll_info._fd == __fd) {
		_s_qnx_epoll_info._items = nullptr;
		_s_qnx_epoll_info._left_events.clear();
		while(_s_qnx_epoll_info._events.size() > 0) {
			auto it = _s_qnx_epoll_info._events.begin();
			fclose(it->second->_fp);
			_s_qnx_epoll_info._events.erase(it);
		}
		fclose(_s_qnx_epoll_info._fp);
		_s_qnx_epoll_info._fd = -1;
		_s_qnx_epoll_info._fp = nullptr;
	}
	return ::close(__fd);
}

}}

#endif /* QNX_PLATEFORM */