#include "inc/rpcevent.h"
#include <google/protobuf/message.h>

namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;
static mutex rcpevent_mut;
#define RPC_EVENT_PREFIX "___rpc_event_"

void rpc_event_trigger_notify(void* owner, void* data, size_t sz)
{
	auto* event_node = reinterpret_cast<rpcevent_node*>(owner);
	rpcevent_impl::instance()->on_trigger(event_node, data, sz);
}

rpcevent_impl* rpcevent_impl::instance(void)
{
	static rpcevent_impl* _inst = NULL;
	if (_inst) return _inst;

	auto_mutex am(rcpevent_mut);
	if (_inst) return _inst;
	_inst = new rpcevent_impl();
	assert(NULL != _inst);
	return _inst;
}

void rpcevent_impl::destory(void)
{

}

rpcevent_impl::rpcevent_impl()
: _event_tree(NULL), _event_host_tree(NULL)
{
	listnode_init(_event_list);
	listnode_init(_event_host_list);
}

rpcevent_impl::~rpcevent_impl()
{

}

int rpcevent_impl::event_host_node_compared(avl_node_t* a, avl_node_t* b)
{
	rpcevent_host_node* anode = AVLNODE_ENTRY(rpcevent_host_node, avlnode, a);
	rpcevent_host_node* bnode = AVLNODE_ENTRY(rpcevent_host_node, avlnode, b);
	int ret = anode->name.compare(bnode->name);
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int rpcevent_impl::event_node_compared(avl_node_t* a, avl_node_t* b)
{
	rpcevent_node* anode = AVLNODE_ENTRY(rpcevent_node, avlnode, a);
	rpcevent_node* bnode = AVLNODE_ENTRY(rpcevent_node, avlnode, b);
	int ret = anode->name.compare(bnode->name);
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

rpcevent_node* rpcevent_impl::add_event(const char* ename,
	rpcevent *event, pbf_wrapper event_data)
{
	auto_mutex am(_mut);
	auto* node = find_event_node_unlock(ename);
	if (!node) {
		node = new rpcevent_node();
		node->name = ename;
		listnode_init(node->eventroot);
		//get datapool element
		std::string elename = RPC_EVENT_PREFIX;
		elename += ename;
		node->element = datapool::inst()->get_element(
			elename.c_str(), true);
		if (!node->element.get()) {
			node->element = datapool::inst()->create_element(
				elename.c_str(), true, false);
			if (!node->element.get()) {
				delete node; return NULL;
			}
		}
		if (avl_insert(&_event_tree, &node->avlnode,
			event_node_compared)) {
			delete node; return NULL;
		}
		auto* enode = new rpcevent_data_node();
		enode->event = event;
		enode->event_data = event_data;
		listnode_add(node->eventroot, enode->ownerlist);
		listnode_add(_event_list, node->ownerlist);
		node->element->add_listener(rpc_event_trigger_notify, node);
		return node;
	}
	listnode_t* nd = node->eventroot.next;
	for (; nd != &(node->eventroot); nd = nd->next) {
		auto* eventnode = list_entry(rpcevent_data_node, ownerlist, nd);
		if (eventnode->event == event) {
			eventnode->event_data = event_data;
			return node;
		}
	}
	auto* enode = new rpcevent_data_node();
	enode->event = event;
	enode->event_data = event_data;
	listnode_add(node->eventroot, enode->ownerlist);	
	return node;
}

void rpcevent_impl::remove_event(const char *ename, rpcevent *event)
{
	auto_mutex am(_mut);
	auto* node = find_event_node_unlock(ename);
	if (!node) return;
	listnode_t* nd = node->eventroot.next;
	for (; nd != &(node->eventroot); nd = nd->next) {
		auto* eventnode = list_entry(rpcevent_data_node, ownerlist, nd);
		if (eventnode->event == event){
			listnode_del(eventnode->ownerlist);
			delete eventnode;
			break;
		}
	}
	if (listnode_isempty(node->eventroot)) {
		node->element->remove_listener(rpc_event_trigger_notify, node);
		avl_remove(&_event_tree, &node->avlnode);
		listnode_del(node->ownerlist);
		delete node;
	}
	return;	
}

void rpcevent_impl::on_trigger(rpcevent_node* node, void* data, size_t sz)
{
	auto_mutex am(_mut);
	listnode_t* nd = node->eventroot.next;
	for (; nd != &(node->eventroot); nd = nd->next) {
		auto* eventnode = list_entry(rpcevent_data_node, ownerlist, nd);
		if (!eventnode->event_data.get() || !data || 0 == sz) {
			eventnode->event->on_triggered(nullptr);
		} else {
			auto* event_data = eventnode->event_data->get_pbfmessage();
			event_data->ParseFromArray(data, sz);
			eventnode->event->on_triggered(eventnode->event_data);
		}
	}
}

void rpcevent_impl::trigger_event(const char *ename, void* buff, size_t sz)
{
	auto* hnode = find_and_create_event_host_node(ename);
	if (!hnode) return ;
	if (!buff) {
		hnode->element->notify(NULL);
		return;
	}
	hnode->element->notify(buff, sz);
}

void rpcevent_impl::force_trigger_last_event(const char *ename)
{
	auto* hnode = find_event_node(ename);
	if (!hnode) return ;
	std::string sdata;
	size_t sz = hnode->element->getdata(sdata);
	if (0 == sz) return;
	on_trigger(hnode, (void*)sdata.c_str(), sdata.length());
}

rpcevent_node* rpcevent_impl::find_event_node(const char* ename)
{
	auto_mutex am(_mut);
	return find_event_node_unlock(ename);
}

rpcevent_node* rpcevent_impl::find_event_node_unlock(const char* ename)
{
	rpcevent_node node;
	node.name = ename;
	avl_node_t* anode = avl_find(_event_tree, &node.avlnode,
		event_node_compared);
	if (NULL == anode)
		return NULL;
	return AVLNODE_ENTRY(rpcevent_node, avlnode, anode);
}

rpcevent_host_node* rpcevent_impl::find_and_create_event_host_node(
	const char* ename)
{
	rpcevent_host_node node;
	node.name = ename;
	avl_node_t* anode = avl_find(_event_host_tree, &node.avlnode,
		event_host_node_compared);
	if (NULL == anode){
		auto* node = new rpcevent_host_node();
		node->name = ename;
		std::string elename = RPC_EVENT_PREFIX;
		elename += ename;
		node->element = datapool::inst()->get_element(
			elename.c_str(), true);
		if (!node->element.get()) {
			node->element = datapool::inst()->create_element(
				elename.c_str(), true, false);
			if (!node->element.get()) {
				delete node; return NULL;
			}
		}
		if (avl_insert(&_event_host_tree, &node->avlnode,
			event_host_node_compared)) {
			delete node; return NULL;
		}
		listnode_add(_event_host_list, node->ownerlist);
		return node;
	}
	return AVLNODE_ENTRY(rpcevent_host_node, avlnode, anode);
}

void rpcevent_impl::release_all_event_node(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_event_list)) {
		listnode_t *nd = _event_list.next;
		auto* enode = list_entry(rpcevent_node, ownerlist, nd);
		while (!listnode_isempty(enode->eventroot)) {
			listnode_t *ned = enode->eventroot.next;
			auto* event_node = list_entry(rpcevent_data_node, ownerlist, ned);
			listnode_del(event_node->ownerlist);
			delete event_node;
		}
		enode->element->remove_listener(rpc_event_trigger_notify, enode);
		avl_remove(&_event_tree, &enode->avlnode);
		listnode_del(enode->ownerlist);
		delete enode;
	}
}

void rpcevent_impl::release_all_event_host_node(void)
{
	auto_mutex am(_mut);
	while (!listnode_isempty(_event_host_list)) {
		listnode_t *nd = _event_host_list.next;
		auto* enode = list_entry(rpcevent_host_node, ownerlist, nd);
		avl_remove(&_event_host_tree, &enode->avlnode);
		listnode_del(enode->ownerlist);
		delete enode;
	}
}

void trigger_event(const char *event_name, google::protobuf::Message* data)
{
	if (nullptr == data) {
		rpcevent_impl::instance()->trigger_event(event_name, nullptr, 0);
		return;
	}
	size_t sz = data->ByteSizeLong();
	void* buff = alloca(sz);
	data->SerializeToArray(buff, sz);
	rpcevent_impl::instance()->trigger_event(event_name, buff, sz);
}

rpcevent::rpcevent()
{
}

rpcevent::~rpcevent()
{
}

void rpcevent::on_triggered(pbf_wrapper event_data)
{

}

void rpcevent::force_trigger_last_event(const char *event_name)
{
	rpcevent_impl::instance()->force_trigger_last_event(event_name);
}

void rpcevent::start_listen(const char *event_name, pbf_wrapper event_data)
{
	rpcevent_impl::instance()->add_event(event_name, this, event_data);
}

void rpcevent::end_listen(const char *event_name)
{
	rpcevent_impl::instance()->remove_event(event_name, this);
}

void rpcevent(const char *event_name, google::protobuf::Message* data)
{
	if (!data) {
		rpcevent_impl::instance()->trigger_event(event_name, nullptr, 0);
		return;
	}
	size_t sz = data->ByteSizeLong();
	void* buff = alloca(sz);
	data->SerializeToArray(buff, sz);
	rpcevent_impl::instance()->trigger_event(event_name, buff, sz);
}

}}}
