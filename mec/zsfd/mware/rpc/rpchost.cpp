#include "inc/rpchost.h"

#include <unistd.h>
#include <dlfcn.h>

#include "std/list.h"
#include "utils/evloop.h"
#include "utils/buffer.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "mware/pkgconfig.h"

#include <google/protobuf/message.h>

#include "inc/rpcmgr-impl.h"
#include "inc/rpc_pkg_def.h"


namespace zas {
namespace mware {
namespace rpc {

using namespace zas::utils;
#define SYSTME_APPLICATION_ROOTPATH "/zassys/sysapp/"

rpcmethod_node::rpcmethod_node(const uint128_t &uuid,
	pbf_wrapper inparam, pbf_wrapper inout_param, void* hdr)
: _inparam(inparam)
, _inout_param(inout_param)
, _handler(hdr)
{
	memcpy(&_uuid, &uuid, sizeof(uint128_t));
}

rpcclass_node::rpcclass_node(const char* clsname,
	void* factory, pbf_wrapper param)
: _flags(0), _factory_hdr(factory),
	_factory_param(param), _refcnt(1), _method_tree(NULL)
{
	_class_name = clsname;
	listnode_init(_method_list);
}

rpcclass_node::~rpcclass_node()
{
	release_all_method_node();
}

int rpcclass_node::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0)
		delete this;
	return cnt;
}

void rpcclass_node::set_singleton(bool singleton)
{
	if (singleton)
		setflags(rpccls_node_singleton);
	else
		clearflags(rpccls_node_singleton);
}

int rpcclass_node::add_method(const uint128_t& uuid,
	pbf_wrapper inparam,
	pbf_wrapper inout_param,
	rpcmethod_handler hdr)
{
	auto_mutex am(_mut);
	if (!hdr) return -EBADPARM;
	auto* node = find_method_node_unlock(uuid);
	if (node) return -EEXISTS;
	node = new rpcmethod_node(uuid, inparam, inout_param, hdr);
	if (avl_insert(&_method_tree, &node->avlnode,
		rpcmethod_node_compared)) {
		delete node;
		return -ELOGIC;
	}
	listnode_add(_method_list, node->ownerlist);
	return 0;
}

rpcmethod_node*
rpcclass_node::get_method_node(const uint128_t& uuid)
{
	auto_mutex am(_mut);
	return find_method_node_unlock(uuid);
}

rpcmethod_node*
rpcclass_node::find_method_node_unlock(const uint128_t &uuid)
{
	avl_node_t* anode = avl_find(_method_tree, 
		MAKE_FIND_OBJECT(uuid, rpcmethod_node, _uuid, avlnode),
		rpcmethod_node_compared);
	if (!anode) return NULL;

	return AVLNODE_ENTRY(rpcmethod_node, avlnode, anode);
}

void rpcclass_node::release_all_method_node(void)
{
	auto_mutex am(_mut);
	rpcmethod_node* nd = NULL;
	while (!listnode_isempty(_method_list))
	{
		nd = LIST_ENTRY(rpcmethod_node, ownerlist, _method_list.next);
		if (!nd) return;
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_method_tree = NULL;
}

int rpcclass_node::rpcmethod_node_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda = AVLNODE_ENTRY(rpcmethod_node, avlnode, aa);
	auto* ndb = AVLNODE_ENTRY(rpcmethod_node, avlnode, bb);
	int ret = memcmp(&nda->_uuid, &ndb->_uuid, sizeof(uint128_t));
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}


service_pkginfo::service_pkginfo()
: version(0), id(0), accessible(0)
, shared(0), signleton(0), bufsize(0)
, pkgname(0), name(0), executive(0), inst_name(0)
, client_name(0), cli_inst_name(0)
, ipaddr(0), port(0)
{
	validity.all = 0;
}

int rpcclass_instance_node::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpcclass_instance_node::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0 && !_f.keep_instacne) {
		delete this;
	}
	clear_keep_instance();
	return cnt;
}

int rpcclass_instance_node::set_internal_create()
{
	_refcnt = 0;
	return _refcnt;
}

int rpcclass_instance_node::set_keep_instance()
{
	_f.keep_instacne = 1;
	return _flags;
}

int rpcclass_instance_node::clear_keep_instance()
{
	_f.keep_instacne = 0;
	return _flags;
}

rpcinst_node_mgr::rpcinst_node_mgr()
: _instance_tree(NULL), _singleton_tree(NULL), _sinst_class_tree(NULL)
{
	listnode_init(_instance_list);
}

rpcinst_node_mgr::~rpcinst_node_mgr()
{
	release_all_instance();
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_create_instance_node(void** instid,
	rpcclass_node* clsnode, bool renew_inst, void* data, size_t sz)
{
	auto_mutex am(_mut);
	if (!clsnode) return nullptr;

	rpcclass_instance_node* inst_node = nullptr;
	if (*instid) {
		//check exist instance
		if (clsnode->testflags(rpccls_node_singleton)) {
			inst_node = find_singleton_instance_unlocked(*instid);
		} else {
			inst_node = find_instance_unlocked(*instid);
		}
		if (inst_node) return inst_node;

		if (!inst_node && !renew_inst)
			return nullptr;
	}

	if (clsnode->testflags(rpccls_node_singleton)) {
		inst_node = find_singleton_instance_unlocked(clsnode);
		if (inst_node) return inst_node;
	}

	void* (*factory)(void*, pbf_wrapper) = 
		(void* (*)(void*, pbf_wrapper))clsnode->getfatory();	
	if (!factory) return nullptr;

	pbf_wrapper param = clsnode->getparam();
	if (data && sz > 0 && param.get()) {
		auto* inpara = param->get_pbfmessage();
		inpara->ParseFromArray(data, sz);
	}

	*instid = factory(nullptr, param);
	if (!*instid) return nullptr;

	if (clsnode->testflags(rpccls_node_singleton)) {
		inst_node = find_singleton_instance_unlocked(*instid);
	} else {
		inst_node = find_instance_unlocked(*instid);
	}
	if (!inst_node) return nullptr;

	inst_node->set_internal_create();

	return inst_node;
}

int rpcinst_node_mgr::add_instid(void* instid, rpcclass_node* clsnode)
{
	auto_mutex am(_mut);

	if (!clsnode) return -EBADPARM;
	rpcclass_instance_node* inst_node;
	if (clsnode->testflags(rpccls_node_singleton)) {
		inst_node = find_singleton_instance_unlocked(instid);
		if (!inst_node)
			inst_node = find_singleton_instance_class(clsnode);
	} else {
		inst_node = find_instance_unlocked(instid);
	}
	if (inst_node) return -EEXISTS;
	inst_node = new rpcclass_instance_node();
	inst_node->_class_node = clsnode;
	inst_node->_inst_id = instid;
	if (clsnode->testflags(rpccls_node_singleton)) {
		if (avl_insert(&_singleton_tree, &inst_node->avlnode,
			instance_node_compared)){
			delete inst_node;
			return -ELOGIC;
		}
		if (avl_insert(&_sinst_class_tree, &inst_node->avlclsnode,
			instance_class_compared)){
			delete inst_node;
			return -ELOGIC;
		}
	} else {
		if (avl_insert(&_instance_tree, &inst_node->avlnode,
			instance_node_compared)){
			delete inst_node;
			return -ELOGIC;
		}
	}
	listnode_add(_instance_list, inst_node->ownerlist);

	return 0;
}

int rpcinst_node_mgr::remove_instance_node(void* inst_id)
{
	auto_mutex am(_mut);
	if (!inst_id) return -EBADPARM;

	rpcclass_instance_node* inst_node = NULL;
	inst_node = find_instance_unlocked(inst_id);
	if (!inst_node) {
		inst_node = find_singleton_instance_unlocked(inst_id);
	}

	if (!inst_node) return -ENOTFOUND;

	if (inst_node->_class_node->testflags(rpccls_node_singleton)) {
		avl_remove(&_singleton_tree, &inst_node->avlnode);
		avl_remove(&_sinst_class_tree, &inst_node->avlclsnode);
	} else {
		avl_remove(&_instance_tree, &inst_node->avlnode);
	}
	listnode_del(inst_node->ownerlist);
	delete inst_node;
	return 0;
}

int rpcinst_node_mgr::keep_instance_node(void* inst_id)
{
	auto_mutex am(_mut);
	if (!inst_id) return -EBADPARM;

	rpcclass_instance_node* inst_node = NULL;
	inst_node = find_instance_unlocked(inst_id);
	if (!inst_node) {
		inst_node = find_singleton_instance_unlocked(inst_id);
	}
	if (!inst_node) return -ENOTFOUND;
	inst_node->set_keep_instance();
	return 0;
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_instance(void* inst_id)
{
	auto_mutex am(_mut);
	return find_instance_unlocked(inst_id);
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_instance_unlocked(void* inst_id)
{
	if (!inst_id) return NULL;
	avl_node_t* anode = avl_find(_instance_tree, 
		MAKE_FIND_OBJECT(inst_id, rpcclass_instance_node, _inst_id, avlnode),
		instance_node_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcclass_instance_node, avlnode, anode);
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_singleton_instance(void* inst_id)
{
	auto_mutex am(_mut);
	return find_singleton_instance_unlocked(inst_id);
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_singleton_instance_unlocked(void* inst_id)
{
	if (!inst_id) return NULL;
	avl_node_t* anode = avl_find(_singleton_tree, 
		MAKE_FIND_OBJECT(inst_id, rpcclass_instance_node, _inst_id, avlnode),
		instance_node_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcclass_instance_node, avlnode, anode);
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_singleton_instance_class(rpcclass_node* clsnode)
{
	auto_mutex am(_mut);
	return find_singleton_instance_class_unlocked(clsnode);
}

rpcclass_instance_node* 
rpcinst_node_mgr::find_singleton_instance_class_unlocked(rpcclass_node* clsnode)
{
	if (!clsnode) return NULL;
	avl_node_t* anode = avl_find(_sinst_class_tree, 
		MAKE_FIND_OBJECT(clsnode, rpcclass_instance_node, 	\
		_class_node, avlclsnode), instance_class_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcclass_instance_node, avlclsnode, anode);
}

int rpcinst_node_mgr::handle_local_invoke(rpcclass_instance_node* inst_node,
	rpcclass_node* clsnode, uint128_t& method_uuid,
	google::protobuf::Message *inparam,
	google::protobuf::Message *outparam, bool renew_inst)
{
	if (!clsnode || !inst_node) return -EBADPARM;
	rpcmethod_node* methodnd = clsnode->get_method_node(method_uuid);
	if (!methodnd) return reply_type_no_method;

	if (methodnd->_inparam.get()) {
		auto* inproto = methodnd->_inparam->get_pbfmessage();
		inproto->Clear();
		if (inparam) 
			inproto->CopyFrom(*inparam);
	}
	google::protobuf::Message* outproto = nullptr;
	if (methodnd->_inout_param.get()) {	
		outproto = methodnd->_inout_param->get_pbfmessage();
		outproto->Clear();
		if (outparam)
			outproto->CopyFrom(*outparam);
	}

	uint8_t ret = ((uint8_t (*)(void*, pbf_wrapper, pbf_wrapper))
		methodnd->_handler)(
			inst_node->_inst_id, 
			methodnd->_inparam,
			methodnd->_inout_param);
	if (outproto)
		outparam->CopyFrom(*outproto);
	return 0;
}

int rpcinst_node_mgr::handle_invoke(rpcclass_instance_node* inst_node,
	rpcclass_node* clsnode, uint128_t& method_uuid, invoke_reqinfo& rinfo,
	google::protobuf::Message **retmsg, bool renew_inst)
{
	if (!clsnode || !inst_node) return -EBADPARM;
	rpcmethod_node* methodnd = clsnode->get_method_node(method_uuid);
	if (!methodnd) return reply_type_no_method;

	google::protobuf::Message* inproto = nullptr;
	if (methodnd->_inparam.get()) {
		inproto = methodnd->_inparam->get_pbfmessage();
		inproto->Clear();
	}
	if (rinfo.validity.m.inparam && inproto){
		inproto->ParseFromArray((void*)(rinfo.buf + rinfo.inparam),
			rinfo.inparam_sz);
	} 
	
	google::protobuf::Message* outproto = nullptr;
	if (methodnd->_inout_param.get()) {
		outproto = methodnd->_inout_param->get_pbfmessage();
		outproto->Clear();
	}
	if (rinfo.validity.m.inoutparam && outproto){
		outproto->ParseFromArray((void*)(rinfo.buf + rinfo.inoutparam),
			rinfo.inoutparam_sz);
	} 

	uint16_t ret = ((uint16_t (*)(void*, pbf_wrapper, pbf_wrapper))
		methodnd->_handler)(
			inst_node->_inst_id, 
			methodnd->_inparam,
			methodnd->_inout_param);

	*retmsg = outproto;
	return 0;
}

int rpcinst_node_mgr::release_all_instance(void)
{
	auto_mutex am(_mut);
	rpcclass_instance_node *nd;
	while (!listnode_isempty(_instance_list)) {
		nd = LIST_ENTRY(rpcclass_instance_node, ownerlist, _instance_list.next);
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_instance_tree = NULL;
	_sinst_class_tree = NULL;
	_singleton_tree = NULL;
	return 0;
}

int rpcinst_node_mgr::instance_node_compared(avl_node_t* aa, avl_node_t* bb)
{
	rpcclass_instance_node* anode = AVLNODE_ENTRY(rpcclass_instance_node,	\
		avlnode, aa);
	rpcclass_instance_node* bnode = AVLNODE_ENTRY(rpcclass_instance_node,	\
		avlnode, bb);
	
	if (anode->_inst_id > bnode->_inst_id) return 1;
	else if (anode->_inst_id < bnode->_inst_id) return -1;
	else return 0;
}

int rpcinst_node_mgr::instance_class_compared(avl_node_t* aa, avl_node_t* bb)
{
	rpcclass_instance_node* anode = AVLNODE_ENTRY(rpcclass_instance_node,	\
		avlclsnode, aa);
	rpcclass_instance_node* bnode = AVLNODE_ENTRY(rpcclass_instance_node,	\
		avlclsnode, bb);
	
	if (anode->_class_node > bnode->_class_node) return 1;
	else if (anode->_class_node < bnode->_class_node) return -1;
	else return 0;
}

service_visitor::service_visitor(evlclient cli)
: _evlclient(cli), _inst_node_tree(nullptr)
{
	listnode_init(_inst_node_list);
}

service_visitor::~service_visitor()
{
	releaseallnode();
}

int service_visitor::compare_visitor_inst_node(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda =  AVLNODE_ENTRY(visitor_inst_node, avlnode, aa);
	auto* ndb =  AVLNODE_ENTRY(visitor_inst_node, avlnode, bb);
	if (nda->inst_node > ndb->inst_node) return 1;
	else if (nda->inst_node < ndb->inst_node) return -1;
	else return 0;	
}

bool service_visitor::check_inst_node_exist(rpcclass_instance_node *inst_node)
{
	auto_mutex am(_mut);
	if (!inst_node) return false;
	avl_node_t* anode = avl_find(_inst_node_tree, 
		MAKE_FIND_OBJECT(inst_node, visitor_inst_node, inst_node, avlnode),
		compare_visitor_inst_node);
	if (NULL == anode) return false;
	return true;
}

int service_visitor::add_inst_node(rpcclass_instance_node *inst_node)
{
	auto_mutex am(_mut);
	if (!inst_node) return -EBADPARM;
	avl_node_t* anode = avl_find(_inst_node_tree, 
		MAKE_FIND_OBJECT(inst_node, visitor_inst_node, inst_node, avlnode),
		compare_visitor_inst_node);
	if (anode) return -EEXISTS;

	auto* nd = new visitor_inst_node();
	nd->inst_node = inst_node;
	if (avl_insert(&_inst_node_tree, &nd->avlnode, 
		compare_visitor_inst_node)) {
		delete nd; return -ELOGIC;
	}
	nd->inst_node->addref();
	listnode_add(_inst_node_list, nd->ownerlist);
	return true;
}

int service_visitor::releaseallnode(void)
{
	auto_mutex am(_mut);
	visitor_inst_node* nd = NULL;
	while (!listnode_isempty(_inst_node_list))
	{
		nd = LIST_ENTRY(visitor_inst_node, ownerlist, _inst_node_list.next);
		if (!nd) return -ELOGIC;
		nd->inst_node->release();
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_inst_node_tree = NULL;	
	return 0;
}

rpchost_impl::rpchost_impl(const char* pkgname,
	const char* servicename,
	const char* instname,
	const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
: service_info(pkgname, servicename, instname, executive,
	ipaddr, port, version, attributes)
, _class_tree(NULL), _service_visitor_tree(NULL)
, _servcie_dll_handle(NULL), _service_create_instance(NULL)
, _service_destory_instance(NULL), _service_handle(NULL)
{
	listnode_init(_class_list);
	listnode_init(_service_visitor_list);
}

rpchost_impl::~rpchost_impl()
{
	release_all_class_node();
	release_all_vistor();
}

int rpchost_impl::add_instance(const char* instname)
{
	auto_mutex am(_mut);
	if (!instname || !*instname) return -EBADPARM;

	if (!is_default_service())
		return -ENOTAVAIL;

	if (!check_multi_instance())
		return -ENOTALLOWED;

	if (find_inst_unlock(instname)) return -EEXISTS;
	rpchost_impl* inst = new rpchost_impl(_pkg_name.c_str(),
		_service_name.c_str(), instname, _executive.c_str(),
		_ipaddr.c_str(), _port,
		_version, _flags);

	if (add_instance_unlock(inst)) {
		delete inst;
		return -ELOGIC;
	}
	return 0;
}

int rpchost_impl::remove_instance(const char* instname)
{
	auto_mutex am(_mut);
	int ret = remove_instance_unlock(instname);
	if (ret) return ret;
	return 0;
}

rpchost_impl* rpchost_impl::find_instance(const char* instname)
{
	auto_mutex am(_mut);
	if (!instname || !*instname) return NULL;
	auto* svc = find_inst_unlock(instname);
	if (!svc) return NULL;
	return zas_downcast(service_info, rpchost_impl, svc);
}

int rpchost_impl::set_service_id(uint32_t service_id)
{
	_service_id = service_id;
	return 0;
}

int rpchost_impl::set_service_impl_method(void* create, void*destory)
{
	if (!create || !destory) return -EBADPARM;
	_service_create_instance = create;
	_service_destory_instance = destory;
	return 0;
}

service* rpchost_impl::get_service_impl(void)
{
	return _service_handle;
}

int rpchost_impl::load_service_dll()
{
	if (_executive.length() < 1) return -ENOTAVAIL;

	std::string libfile = SYSTME_APPLICATION_ROOTPATH;
	const pkgconfig& p = pkgconfig::getdefault();
	libfile += p.get_package_name();
	libfile += "/" + _executive;

	_servcie_dll_handle = dlopen(libfile.c_str(), RTLD_LAZY);
	if (!_servcie_dll_handle) {
		printf("load service library %s\n", libfile.c_str());
		printf("load failure %s\n", dlerror());
		return -ELOGIC;
	}
	return 0;
}
int rpchost_impl::load_service_impl()
{
	if (!_servcie_dll_handle) return -ENOTAVAIL;
	if (_service_handle) return 0;


	_service_handle = ((service* (*)(void))_service_create_instance)();
	if (!_service_handle) return -ENOTAVAIL;
	_service_handle->register_all();
	_service_handle->on_create(NULL);
	return 0;
}

rpcclass_node*
rpchost_impl::register_interface(const char* clsname,
	void* factory, pbf_wrapper param, uint32_t flags)
{
	auto_mutex am(_mut);
	if (find_class_node_unlock(clsname))
		return NULL;	
	
	auto* node = new rpcclass_node(clsname, factory, param);
	node->setflags(flags);

	if (avl_insert(&_class_tree, &node->avlnode, 
		rpcclass_node_compared)) {
		delete node;
		return NULL;
	}
	listnode_add(_class_list, node->ownerlist);
	return node;
}

rpcclass_node*
rpchost_impl::get_interface(const char *clsname)
{
	auto_mutex am(_mut);
	return find_class_node_unlock(clsname);
}

int rpchost_impl::add_instid(const char* clsname, void* inst_id)
{
	auto_mutex am(_mut);
	rpcclass_node* clsnode = find_class_node_unlock(clsname);
	if (!clsnode) return -EBADPARM;

	return _inst_node_mgr.add_instid(inst_id, clsnode);
}

int rpchost_impl::release(void* inst_id)
{
	auto_mutex am(_mut);

	return _inst_node_mgr.remove_instance_node(inst_id);
}

int rpchost_impl::keep_instance(void* inst_id)
{
	auto_mutex am(_mut);

	return _inst_node_mgr.keep_instance_node(inst_id);
}

int rpchost_impl::release()
{
	return service_info::release();
}

rpcclass_node*
rpchost_impl::find_class_node_unlock(const char* clsname)
{
	if (!clsname || !*clsname) return NULL;
	avl_node_t* anode = avl_find(_class_tree, 
		MAKE_FIND_OBJECT(clsname, rpcclass_node, _class_name, avlnode),
		rpcclass_node_compared);

	if (NULL == anode) return NULL;
	return AVLNODE_ENTRY(rpcclass_node, avlnode, anode);
}

int rpchost_impl::rpcclass_node_compared(avl_node_t* aa, avl_node_t* bb)
{
	auto* nda =  AVLNODE_ENTRY(rpcclass_node, avlnode, aa);
	auto* ndb =  AVLNODE_ENTRY(rpcclass_node, avlnode, bb);
	int ret = strcmp(nda->_class_name.c_str(), ndb->_class_name.c_str());
	if (ret > 0) return 1;
	else if (ret < 0) return -1;
	else return 0;
}

int rpchost_impl::release_all_class_node(void)
{
	auto_mutex am(_mut);
	rpcclass_node* nd = NULL;
	while (!listnode_isempty(_class_list))
	{
		nd = LIST_ENTRY(rpcclass_node, ownerlist, _class_list.next);
		if (!nd) return -ELOGIC;
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_class_tree = NULL;
	return 0;
}

int rpchost_impl::terminate(void)
{
	release_all_vistor();
	request_to_server_unlock(reqtype_service_stop);
	release_all_class_node();
	_f.starting = 0;
	_f.running = 0;
	if (_service_handle) {
		_service_handle->on_destroy(NULL);	
		((void (*)(service* svc))_service_destory_instance)(_service_handle);
	}
	
	_service_handle = NULL;
	if (_servcie_dll_handle)
		dlclose(_servcie_dll_handle);
	_servcie_dll_handle = NULL;
	return 0;
}

int rpchost_impl::stop_service(evlclient sender)
{

	if (listnode_isempty(_service_visitor_list)) return -ELOGIC;

	if (_service_handle)
		_service_handle->on_unbind(NULL, sender);

	int ret = release_service_visitor(sender);
	
	if (!ret && listnode_isempty(_service_visitor_list))
		request_to_server_unlock(reqtype_service_stop);
	return 0;
}

int rpchost_impl::start_service(void)
{
	if (_service_handle)
		_service_handle->on_start(NULL);
	return 0;
}

int rpchost_impl::run()
{
	auto_mutex am(_mut);	
	if (is_starting())
		return -EEXISTS;
	_f.starting = 1;

	load_service_dll();
	load_service_impl();
	int ret = regist_to_server();
	if (ret) {
		_f.starting = 0;
		return ret;
	}
	_f.running = 1;
	return 0;
}

int rpchost_impl::regist_to_server(void)
{
	return request_to_server_unlock(reqtype_service_start);
}

int rpchost_impl::service_status_change_stop()
{
	auto_mutex am(_mut);
	_f.starting = 0;
	_f.running = 0;
	return 0;
}

int rpchost_impl::request_to_server_unlock(uint16_t reqtype)
{
	size_t sz = get_string_size();
	request_server_pkg* svrreq = new(alloca(sizeof(*svrreq) + sz))
		request_server_pkg(sz);
	request_server& rinfo = svrreq->payload();
	load_info(&(rinfo.service_info));
	rinfo.request = reqtype;
	if (reqtype_service_stop == reqtype) {
		service_status_change_stop();
	}
	//get launch cli
	evlclient svrcli = evloop::inst()->getclient(NULL, NULL);

	evpoller poller;
	auto* ev = poller.create_event(evp_evid_package_with_seqid,
		SERVER_REPLY_CTRL, svrreq->header().seqid);
	assert(NULL != ev);
	rpchost_impl* self = this;
	ev->write_inputbuf(&self, sizeof(void*));
	// submit the event
	ev->submit();
	
	size_t sendsz = svrcli->write((void*)svrreq, sizeof(*svrreq) + sz);	
	if (sendsz < sizeof(*svrreq) + sz) {
		return -EBADOBJ;
	}
	if (poller.poll(5000)) {
		return -4;
	}
	ev = poller.get_triggered_event();
	assert(!poller.get_triggered_event());
	int status = -1;
	ev->read_outputbuf(&status, sizeof(status));
	return status;
}

int rpchost_impl::handle_invoke(evlclient sender, void** instid,
	std::string& clsname, uint128_t& method_uuid,
	invoke_reqinfo& rinfo, google::protobuf::Message **retmsg, bool renew_inst)
{
	rpcclass_node* clsnode = get_interface(clsname.c_str());
	if (!clsnode) return reply_type_no_class;

	auto* inst_node = _inst_node_mgr.find_create_instance_node(instid,
		clsnode, renew_inst);
	if (!inst_node) return reply_type_no_instance;
	if (!*instid)
		*instid = inst_node->_inst_id;
	
	service_visitor* visitor = find_and_create_visitor(sender, inst_node);
	if (!visitor) return -ELOGIC;


	return _inst_node_mgr.handle_invoke(inst_node, clsnode, method_uuid,
		rinfo, retmsg, renew_inst);
}

int rpchost_impl::handle_get_instance(evlclient sender, void** instid,
	std::string& clsname, void* data, size_t sz, bool singleton)
{
	rpcclass_node* clsnode = get_interface(clsname.c_str());
	if (!clsnode) return reply_type_no_class;

	if (clsnode->testflags(rpccls_node_singleton) != singleton)
		return reply_type_get_instance_param_err;

	*instid = nullptr;
	auto* inst_node = _inst_node_mgr.find_create_instance_node(instid,
		clsnode, true, data, sz);

	if (!inst_node) 
		return reply_type_no_instance;
	*instid = inst_node->_inst_id;
	service_visitor* visitor = find_and_create_visitor(sender, inst_node);
	if (!visitor) return -ELOGIC;	
	return 0;
}

int rpchost_impl::handle_local_invoke(void** instid,
	std::string& clsname, uint128_t& method_uuid, 
	google::protobuf::Message *inparam,
	google::protobuf::Message *outparam, bool renew_inst)
{
	rpcclass_node* clsnode = get_interface(clsname.c_str());
	if (!clsnode) return reply_type_no_class;

	auto* inst_node = _inst_node_mgr.find_create_instance_node(instid,
		clsnode, renew_inst);
	if (!inst_node) return reply_type_no_instance;
	if (!*instid)
		*instid = inst_node->_inst_id;
	
	evlclient sender;
	service_visitor* visitor = find_and_create_visitor(sender, inst_node);
	if (!visitor) return -ELOGIC;

	return _inst_node_mgr.handle_local_invoke(inst_node, clsnode, method_uuid,
		inparam, outparam, renew_inst);
}

service_visitor* rpchost_impl::find_and_create_visitor(evlclient evlclient,
	rpcclass_instance_node* node)
{
	auto_mutex am(_mut);
	service_visitor* visitor = find_service_visitor_unlocked(evlclient);
	if (visitor) {
		if (!visitor->check_inst_node_exist(node))
			visitor->add_inst_node(node);
	}
	if (visitor) return visitor;
	if (!check_visitor_right_unlock(evlclient)) return nullptr;
	visitor = new service_visitor(evlclient);
	if (avl_insert(&_service_visitor_tree, &visitor->avlnode, 
		service_visitor_compared)) {
		delete visitor;
		return nullptr;
	}
	visitor->add_inst_node(node);
	listnode_add(_service_visitor_list, visitor->ownerlist);
	if (_service_handle)
		_service_handle->on_bind(nullptr, evlclient);
	return visitor;
}

bool rpchost_impl::check_visitor_right_unlock(evlclient evlclient)
{
	// singleton_type_globally is only for service on cloud
	if (singleton_type_globally == _f.singleton)
		return false;
	if (singleton_type_device_only == _f.singleton
		&& 0 == _f.global_accessible) {
		listnode_t *root = nullptr;
		if (1 == _f.is_default)
			root = &_inst_list;
		else
			root = &ownerlist;
		listnode_t *nd = root->next;
		service_info* info = nullptr;
		rpchost_impl* info_impl = nullptr;
		for (; nd != root; nd = nd->next) {
			info = LIST_ENTRY(service_info, ownerlist, nd);
			info_impl = zas_downcast(service_info, rpchost_impl, info);
			if (info_impl->find_service_visitor_unlocked(evlclient))
				return false;
		}
	}
	return true;
}

bool rpchost_impl::check_service_right_unlock(void)
{
	// singleton_type_globally is only for service on cloud
	if (singleton_type_globally == _f.singleton)
		return false;
	if (singleton_type_device_only == _f.singleton
		&& 1 == _f.global_accessible) {
		listnode_t *root = nullptr;
		if (1 == _f.is_default)
			root = &_inst_list;
		else
			root = &ownerlist;
		listnode_t *nd = root->next;
		service_info* info = nullptr;
		for (; nd != root; nd = nd->next) {
			info = LIST_ENTRY(service_info, ownerlist, nd);
			if (0 != info->_service_id) return false;
		}
	}
	return true;
}

int rpchost_impl::remove_service_visitor(evlclient client)
{
	auto_mutex am(_mut);
	service_visitor* visitor = find_service_visitor_unlocked(client);
	if (!visitor) return -ENOTFOUND;
	avl_remove(&_service_visitor_tree, &visitor->avlnode);
	listnode_del(visitor->ownerlist);
	delete visitor;
	return 0;
}

int rpchost_impl::release_service_visitor(evlclient client)
{
	auto_mutex am(_mut);

	if (listnode_isempty(_service_visitor_list)) return -ENOTEXISTS;

	service_visitor* visitor = find_service_visitor_unlocked(client);
	if (!visitor) return -ENOTFOUND;
	avl_remove(&_service_visitor_tree, &visitor->avlnode);
	listnode_del(visitor->ownerlist);
	delete visitor;
	return 0;
}

service_visitor* rpchost_impl::find_service_visitor_unlocked(evlclient client)
{
	avl_node_t* anode = avl_find(_service_visitor_tree, 
		MAKE_FIND_OBJECT(client, service_visitor, _evlclient, avlnode),
		service_visitor_compared);
	if (!anode) return NULL;

	return AVLNODE_ENTRY(service_visitor, avlnode, anode);
}

int rpchost_impl::release_all_vistor(void)
{
	auto_mutex am(_mut);
	service_visitor *nd;
	while (!listnode_isempty(_service_visitor_list)) {
		nd = LIST_ENTRY(service_visitor, ownerlist, _service_visitor_list.next);
		if (_service_handle)
			_service_handle->on_unbind(NULL, nd->_evlclient);
		listnode_del(nd->ownerlist);
		delete nd;
	}
	_service_visitor_tree = NULL;
	return 0;
}

int rpchost_impl::service_visitor_compared(avl_node_t* aa, avl_node_t* bb)
{
	service_visitor* av = AVLNODE_ENTRY(service_visitor, avlnode, aa);
	service_visitor* bv = AVLNODE_ENTRY(service_visitor, avlnode, bb);

	if (av->_evlclient.get() > bv->_evlclient.get()) return 1;
	else if (av->_evlclient.get() < bv->_evlclient.get()) return -1;
	else return 0;
}


rpchost_mgr::rpchost_mgr()
: _active_service_tree(NULL), _refcnt(1)
{
}

rpchost_mgr::~rpchost_mgr()
{
	release_all_nodes();
}

int rpchost_mgr::addref()
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int rpchost_mgr::release()
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) {
		delete this;
	}
	return cnt;
}

rpchost_impl* rpchost_mgr::add_service(const char* pkgname,
	const char* servicename, const char* executive,
	const char* ipaddr, uint32_t port,
	uint32_t version, uint32_t attributes)
{
	auto_mutex am(_mut);
	// check service exist
	if (find_service_unlock(pkgname, servicename))
		return NULL;

	auto* hsvc = new rpchost_impl(pkgname,
		 servicename, NULL, executive, ipaddr, port, version, attributes);

	if (add_service_unlock(hsvc)) {
		delete hsvc; return NULL;
	}
	return hsvc;	
}

int rpchost_mgr::remove_service(const char* pkgname,
	const char* servicename)
{
	auto_mutex am(_mut);
	return remove_service_unlock(pkgname, servicename);
}
rpchost_impl* rpchost_mgr::get_service(const char* pkgname,
	const char* servicename)
{
	auto_mutex am(_mut);

	auto* info = find_service_unlock(pkgname, servicename);
	if (!info) return NULL;

	return zas_downcast(service_info, rpchost_impl, info);
}

int rpchost_mgr::release_all_nodes(void)
{
	auto_mutex am(_mut);
	int ret = release_all_service_unlock();
	_active_service_tree = NULL;
	return ret;
}

int rpchost_mgr::handle_server_reply_package(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue,
	server_reply* reply)
{
	int status;
	// get the package payload
	auto* ev = queue.dequeue();
	assert(NULL == queue.dequeue());
	rpchost_impl* peer = NULL;
	ev->read_inputbuf(&peer, sizeof(void*));
	assert(NULL != peer);
	service_pkginfo_validity_u val;
	status = reply->result;
	if (peer->check_info(reply->service_info, val))
		status = -ELOGIC;

	if (reply->service_info.validity.m.id) {
		peer->set_service_id(reply->service_info.id);
		if (add_active_service(peer)) {
			printf("rpchost_mgr add service to active list error\n");
		}
	}
	// update info of event
	ev->write_outputbuf(&status, sizeof(status));
	return 0;
}

int rpchost_mgr::handle_service_start(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	request_service* req = (request_service*)
		alloca(pkghdr.size);
	assert(NULL != req);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)req, pkghdr.size);
	assert(sz == pkghdr.size);

	service_info tmp;
	service_pkginfo_validity_u val;
	tmp.update_info(req->service_info, val);

	rpchost_impl* host_impl = NULL;
	host_impl = find_service(tmp._pkg_name.c_str(), tmp._service_name.c_str());
	printf("recive %s, %s\n", tmp._pkg_name.c_str(), tmp._service_name.c_str());
	if (host_impl) {
		if (req->service_info.validity.m.inst_name
			&& host_impl->is_default_service()) {
			auto* svc = host_impl->find_instance(tmp._inst_name.c_str());
			if (svc)
				host_impl = svc;
		}
	}

	//frist reply message then handle command;
	size_t replysz = 0;
	if (host_impl)
		replysz = host_impl->get_string_size();	
	service_reply_pkg* svrrly = new(alloca(sizeof(*svrrly) + replysz))
		service_reply_pkg(replysz);
	service_reply& rinfo = svrrly->payload();
	svrrly->header().seqid = pkghdr.seqid;	
	if (host_impl)
		host_impl->load_info(&(rinfo.service_info));
	rinfo.result = host_impl ? 0 : -1;

	sender->write((void*)svrrly, sizeof(*svrrly) + replysz);

	// handle command
	if (host_impl) {
		if (zrpc_svc_req_start == req->request)
			host_impl->run();
		else if (zrpc_svc_start_request == req->request) {
			host_impl->run();
			host_impl->start_service();
		} else if (zrpc_svc_stop_request == req->request){
			host_impl->stop_service(sender);
		} else if (zrpc_svc_terminate_request == req->request){
			host_impl->terminate();
		}
	}
	return 0;
}

int rpchost_mgr::handle_invoke_request(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	invoke_request* reqinfo = (invoke_request*)
		alloca(pkghdr.size);
	assert(NULL != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload

	invoke_reqinfo& rinfo = reqinfo->req_info;
	std::string clsname;
	if (rinfo.validity.m.class_name)
		clsname = rinfo.buf + rinfo.class_name;
	
	bool renew_inst = false;
	if (rinfo.validity.m.renew_inst && (1 == rinfo.renew_inst))
		renew_inst = true;

	std::string method_name;
	if (rinfo.validity.m.method_name)
		method_name = rinfo.buf + rinfo.method_name;

	uint16_t method_id = 0;
	if (rinfo.validity.m.method_id)
		method_id = rinfo.method_id;

	void* inst_id = NULL;
	if (rinfo.validity.m.instid)
		inst_id = reinterpret_cast<void*>(rinfo.instid);
	
	void* data_buf = NULL;
	size_t datasize = 0;
	if (rinfo.validity.m.inparam) {
		data_buf = rinfo.buf + rinfo.inparam;
		datasize= rinfo.inparam_sz;
	}

	int result = -1;
	rpchost_impl* active_service = find_active_service(reqinfo->svc_id);
	if (!active_service) {
		result = reply_type_no_service;
		return reply_invoke(sender, pkghdr,reqinfo->req_action,
			result, inst_id, NULL);
	}
	google::protobuf::Message *retmsg = NULL;
	switch (reqinfo->req_action)
	{
	case req_action_call_method:
		result = active_service->handle_invoke(sender, &inst_id, clsname,
			reqinfo->method_uuid, rinfo, &retmsg, renew_inst);
		break;
	case req_action_get_instance:
		result = active_service->handle_get_instance(sender, &inst_id,
			clsname, data_buf, datasize, false);
		break;
	case req_action_get_singleton:
		result = active_service->handle_get_instance(sender, &inst_id,
			clsname, data_buf, datasize, true);
		break;
	
	default:
		break;
	}
	return reply_invoke(sender, pkghdr,reqinfo->req_action,
		result, inst_id, retmsg);
}

int rpchost_mgr::handle_client_request(evlclient sender,
	const package_header &pkghdr,
	const triggered_pkgevent_queue& queue)
{
	client_request_service* reqinfo = (client_request_service*)
		alloca(pkghdr.size);
	assert(NULL != reqinfo);
	readonlybuffer* rbuf = pkghdr.get_readbuffer();
	assert(NULL != rbuf);
	size_t sz = rbuf->peekdata(0, (void*)reqinfo, pkghdr.size);
	assert(sz == pkghdr.size);
	// get the package payload
	int ret = -1;
	if (cli_req_svc_stop == reqinfo->request) {
		rpchost_impl* active_service = find_active_service(reqinfo->svc_id);
		if (active_service)
			ret = active_service->stop_service(sender);
	}

	service_reply_client_pkg* svcreply = new(alloca(sizeof(*svcreply)))
		service_reply_client_pkg(0);
	service_reply_client& rinfo = svcreply->payload();
	svcreply->header().seqid = pkghdr.seqid;
	rinfo.result = ret;
	sender->write((void*)svcreply, sizeof(*svcreply));
	return 0;
}

int rpchost_mgr::reply_invoke(evlclient sender, const package_header &pkghdr,
	uint32_t req_action, uint32_t reply_type,
	void* instid, google::protobuf::Message *retmsg)
{
	size_t sz = 0;
	if (!reply_type && retmsg) {
		sz += retmsg->ByteSizeLong() + 1;
	}
	invoke_reply_pkg* invokereply = new(alloca(sizeof(*invokereply) + sz))
		invoke_reply_pkg(sz);
	invoke_reply& rinfo = invokereply->payload();

	rinfo.req_action = req_action;
	if (!reply_type) {
		rinfo.reply_type = reply_type_success;
		rinfo.instid = (size_t)instid;
	}
	else
		rinfo.reply_type = reply_type;
		
	rinfo.reply_info.result = 0;
	if (!reply_type && retmsg) {
		rinfo.reply_info.result = retmsg->ByteSizeLong();
		retmsg->SerializeToArray(rinfo.reply_info.buf, rinfo.reply_info.result);
	}
	invokereply->header().seqid = pkghdr.seqid;
	sender->write((void*)invokereply, sizeof(*invokereply) + sz);
	return 0;
}

int rpchost_mgr::add_active_service(rpchost_impl* impl)
{
	auto_mutex am(_mut);

	if (!impl || !impl->_service_id) return -EBADPARM;
	
	rpchost_impl* rhost = find_active_service_unlock(impl->_service_id);
	if (rhost) return -EEXISTS;

	if (!impl->check_service_right_unlock()) return -ENOTALLOWED;

	if (avl_insert(&_active_service_tree, &impl->avlactivenode,
		service_active_compared)){
		return -ELOGIC;
	}
	return 0;
}

int rpchost_mgr::remove_active_service(rpchost_impl* impl)
{
	if (!impl) return -EBADPARM;

	auto_mutex am(_mut);
	avl_remove(&_active_service_tree, &impl->avlactivenode);
	return 0;
}

int rpchost_mgr::remove_active_service(uint32_t service_id)
{
	auto_mutex am(_mut);
	rpchost_impl *impl = find_active_service_unlock(service_id);
	if (!impl) return -EBADPARM;
	avl_remove(&_active_service_tree, &impl->avlactivenode);
	return 0;
}

rpchost_impl* rpchost_mgr::find_active_service_unlock(uint32_t service_id)
{
	avl_node_t* anode = avl_find(_active_service_tree, 
		MAKE_FIND_OBJECT(service_id, rpchost_impl, _service_id, avlactivenode),
		service_active_compared);
	if (!anode) return NULL;

	return AVLNODE_ENTRY(rpchost_impl, avlactivenode, anode);
}

rpchost_impl* rpchost_mgr::find_active_service(uint32_t service_id)
{
	auto_mutex am(_mut);
	return find_active_service_unlock(service_id);
}

int rpchost_mgr::service_active_compared(avl_node_t* aa, avl_node_t* bb)
{
	rpchost_impl *ahost = AVLNODE_ENTRY(rpchost_impl, avlactivenode, aa);
	rpchost_impl *bhost = AVLNODE_ENTRY(rpchost_impl, avlactivenode, bb);
	if (ahost->_service_id > bhost->_service_id) return 1;
	else if (ahost->_service_id < bhost->_service_id) return -1;
	else return 0;
}
rpchost_impl* rpchost_mgr::find_service(const char* pkgname,
	const char* servicename)
{
	auto_mutex am(_mut);
	auto* info = find_service_unlock(pkgname, servicename);
	if (!info) return NULL;
	return zas_downcast(service_info, rpchost_impl, info);	
}

}}}