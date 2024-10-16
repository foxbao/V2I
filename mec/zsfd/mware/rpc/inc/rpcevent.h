
#ifndef __CXX_ZAS_RPC_EVENT_H__
#define __CXX_ZAS_RPC_EVENT_H__

#include <string>
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/evloop.h"
#include "utils/datapool.h"
#include "utils/mutex.h"
#include "utils/wait.h"

#include "rpc/rpcmgr.h"

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;

struct rpcevent_node
{
	avl_node_t avlnode;
	listnode_t ownerlist;
	listnode_t eventroot;
	datapool_element element;
	std::string name;
};
struct rpcevent_data_node
{
	listnode_t ownerlist;
	rpcevent*	event;
	pbf_wrapper	event_data;
};

struct rpcevent_host_node
{
	avl_node_t avlnode;
	listnode_t ownerlist;
	datapool_element element;
	std::string name;
};

class rpcevent_impl
{
public:
	rpcevent_impl();
	~rpcevent_impl();

	static rpcevent_impl* instance();
	static void destory(void);

public:
	rpcevent_node* add_event(const char* ename,
		rpcevent *event, pbf_wrapper event_data);
	void remove_event(const char *ename, rpcevent *event);
	void trigger_event(const char *ename, void* data, size_t sz);
	void on_trigger(rpcevent_node* node, void* data, size_t sz);
	void force_trigger_last_event(const char* ename);

private:
	static int event_node_compared(avl_node_t* a, avl_node_t* b);
	static int event_host_node_compared(avl_node_t* a, avl_node_t* b);

	rpcevent_node* find_event_node_unlock(const char* ename);
	rpcevent_node* find_event_node(const char* ename);
	rpcevent_host_node* find_and_create_event_host_node(const char* ename);
	
	void release_all_event_node(void);
	void release_all_event_host_node(void);

private:
	avl_node_t *_event_tree;
	listnode_t _event_list;
	avl_node_t *_event_host_tree;
	listnode_t _event_host_list;
	mutex	_mut;
};

}}} // end of namespace zas::mware::rpc

#endif /* __CXX_ZAS_ZRPC_HOST_H__
/* EOF */
