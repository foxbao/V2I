#include "mytest-internal.h"

namespace mytest {
namespace skeleton {

using namespace zas::mware::rpc;

void* myIF1_factory(void *destroy, pbf_wrapper param)
{
	if (destroy) {
		myIF1::destroy_instance((myIF1*)destroy);
		return nullptr;
	}
	return (void *)myIF1::create_instance();
}

static uint16_t myIF1_method1_handler(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)
{
	assert(nullptr != obj);

	auto* inp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF1_method1_inparam,	\
		inparam->get_pbfmessage());
	auto* inoutp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF1_method1_inout_param,	\
		inout_param->get_pbfmessage());

	::mytest::test_struct2 val2(inout_param, inoutp->mutable_val2());
	inoutp->set____retval(((myIF1*)obj)->method1(inp->val1(), val2));
    ::mytest_pbf::test_struct2* test = inoutp->mutable_val2();
	int sz = test->var_3_size();
	for (int i ; i < sz; i++) {
		printf("protobuf content is %d\n", test->var_3(i));

	}

	return 1;
}

static uint16_t myIF1_add_listener_handler(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)
{
	assert(nullptr != obj);

	auto* inp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF1_add_listener_inparam,	\
		inparam->get_pbfmessage());
	auto* inoutp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF1_add_listener_inout_param,	\
		inout_param->get_pbfmessage());

	myIF2_listener listener((void*)inp->observable_id(),
		inp->client_name().c_str(),
		inp->inst_name().c_str());
	
	inoutp->set____retval(((myIF1*)obj)->add_listener(inp->val1(), listener));

	return 1;
}

void import_myIF1(void)
{
	pbf_wrapper param;
	rpchost host = rpcmgr::inst()->get_service("rpc.service2", nullptr);
	rpcinterface rif = host->register_interface("mytest.myIF1", (void*)myIF1_factory, param);
	pbf_wrapper inparam, inout_param;
	inparam = 
		new ::mytest::struct_wrapper::___myIF1_method1_inparam_wrapper;
	inout_param = 
		new ::mytest::struct_wrapper::___myIF1_method1_inout_param_wrapper;
	uint128_t u1 = {0xF716DBF3, 0x19F, 0xB9BE, {0x65, 0xCC, 0xDE, 0x49, 0x18, 0x1C, 0x1A, 0xC9}};
	rif->add_method(u1, inparam, inout_param, (void*)myIF1_method1_handler);

	pbf_wrapper inparaml, inout_paraml;
	inparaml = 
		new ::mytest::struct_wrapper::___myIF1_add_listener_inparam_wrapper;
	inout_paraml = 
		new ::mytest::struct_wrapper::___myIF1_add_listener_inout_param_wrapper;
	uint128_t u2 = {0xF716DBF4, 0x19F, 0xB9BE, {0x65, 0xCC, 0xDE, 0x49, 0x18, 0x1C, 0x1A, 0xC9}};
	rif->add_method(u2, inparaml, inout_paraml,
		(void*)myIF1_add_listener_handler);
}

void* myIF3_singleton_factory(void *destroy, pbf_wrapper param)
{
	if (destroy) {
		myIF3_singleton::destroy_instance((myIF3_singleton*)destroy);
		return nullptr;
	}
	return (void *)myIF3_singleton::create_instance();
}

static uint16_t myIF3_singleton_method1_handler(void* obj,
	pbf_wrapper inparam, pbf_wrapper inout_param)
{
	assert(nullptr != obj);

	auto* inp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF3_method1_inparam,	\
		inparam->get_pbfmessage());
	auto* inoutp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF3_method1_inout_param,	\
		inout_param->get_pbfmessage());
	
	int val2 = inoutp->val2();
	::mytest::test_struct2 ret = ((myIF3_singleton*)obj)->method1(
		inp->val1(), val2);
	ret.copyto(*(inoutp->mutable____retval()));
	inoutp->set_val2(val2);
	return 1;
}

static uint16_t myIF3_singleton_method2_handler(void* obj,
	pbf_wrapper inparam, pbf_wrapper inout_param)
{
	assert(nullptr != obj);

	auto* inoutp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF3_method2_inout_param,	\
		inout_param->get_pbfmessage());

	myIF1 *value = (myIF1*)inoutp->val1();
	
	inoutp->set____retval((size_t)((myIF3_singleton*)obj)->
		method2(*value));

	return 1;
}

void import_myIF3_singleton(void)
{
	rpchost host = rpcmgr::inst()->get_service("rpc.service2", nullptr);
	pbf_wrapper tmp;
	rpcinterface rif = host->register_interface("mytest.myIF3_singleton", (void*)myIF3_singleton_factory, tmp);
	rif->set_singleton(true);
	pbf_wrapper inparam, inout_param;
	inparam = 
		new ::mytest::struct_wrapper::___myIF3_method1_inparam_wrapper;
	inout_param = 
		new ::mytest::struct_wrapper::___myIF3_method1_inout_param_wrapper;
	uint128_t u1 = {0xE716DBF3, 0x19F, 0xB9BE, {0x65, 0xCC, 0xDE, 0x49, 0x18, 0x1C, 0x1A, 0xA9}};;
	rif->add_method(u1, inparam, inout_param,
		(void*)myIF3_singleton_method1_handler);

	pbf_wrapper inparaml, inout_paraml;
	inout_paraml = 
		new ::mytest::struct_wrapper::___myIF3_method2_inout_param_wrapper;
	uint128_t u2 = {0xE05CF8F2, 0x36CF, 0xB8F5, {0xDB, 0xE4, 0xBE, 0x74, 0x00, 0x00, 0x00, 0x20}};
	rif->add_method(u2, inparaml, inout_paraml,
		(void*)myIF3_singleton_method2_handler);
}

myIF2_listener::myIF2_listener()
: _inst_id(NULL)
{
}

myIF2_listener::myIF2_listener(void* inst_id, const char* cliname,
	const char* inst_name)
: _inst_id(inst_id)
, _cliname(cliname)
, _inst_name(inst_name)
{
	assert(0 != _inst_id);
	rpcobservable::inst()->add_client_instance(cliname, inst_name, inst_id);
}

myIF2_listener::myIF2_listener(const myIF2_listener& c)
{
	if (this == &c) {
		return;
	}
	if (_inst_id) {
		rpcobservable::inst()->release_client_instance(_inst_id);
	}
	_inst_id = c._inst_id;
	_cliname = c._cliname;
	_inst_name = c._inst_name;
	rpcobservable::inst()->add_client_instance(_cliname.c_str(),
		_inst_name.c_str(), _inst_id);
}

const myIF2_listener& myIF2_listener::operator=(const myIF2_listener& c)
{
	if (this == &c) {
		return *this;
	}
	if (_inst_id) {
		rpcobservable::inst()->release_client_instance(_inst_id);
	}
	_inst_id = c._inst_id;
	_cliname = c._cliname;
	_inst_name = c._inst_name;
	rpcobservable::inst()->add_client_instance(_cliname.c_str(),
		_inst_name.c_str(), _inst_id);

	return *this;
}

myIF2_listener::~myIF2_listener()
{
	if (_inst_id) {
		rpcobservable::inst()->release_client_instance(_inst_id);
		_inst_id = nullptr;
	}
}

int32_t myIF2_listener::on_method1(int32_t val1, ::mytest::test_struct1& val2)
{
	if (!_inst_id) return -ENOTAVAIL;
	::mytest_pbf::___myIF2_listener_on_method1_inparam inparam;
	::mytest_pbf::___myIF2_listener_on_method1_inout_param inout_param;
	uint128_t uid = {0xB60D8870, 0xA242, 0x1A49, {0xC6, 0x95, 0xA2, 0x3F, 0x06, 0x86, 0x3C, 0xA0}};

	inparam.set_val1(val1);
	val2.copyto(*inout_param.mutable_var2());
	rpcobservable::inst()->invoke_observable_method(_inst_id, 
		"myIF2_listener.on_method1", uid,
		&inparam, &inout_param, 1);

	return inout_param.___retval();
}

void rpc_event_test_with_param::trigger(test_struct2 &testdata){
	// how to change to protobuf
	::mytest_pbf::___rpc_event_test_with_param inparam;
	testdata.copyto(*inparam.mutable_val2());
	trigger_event("mytest.rpc_event_test_with_param", &inparam);
}

}} // end of namespace mytest::skeleton
/* EOF */
