#include "mytest-internal.h"

namespace mytest {
namespace proxy {

const char *myIF1::myIF1_clsname_ = "mytest.myIF1";
rpcproxy_static_data myIF1::myIF1_static_data_ = {
	NULL,
	NULL,
	"zas.system",
	"rpc.service2",
	NULL,
	myIF1_clsname_,
};

int32_t myIF1::method1(int val1, test_struct2& val2)
{
	::mytest_pbf::___myIF1_method1_inparam inparam;
	::mytest_pbf::___myIF1_method1_inout_param inout_param;
	uint128_t uid = {0xF716DBF3, 0x19F, 0xB9BE, {0x65, 0xCC, 0xDE, 0x49, 0x18, 0x1C, 0x1A, 0xC9}}; // todo: TBD

	inparam.set_val1(val1);
	val2.copyto(*inout_param.mutable_val2());

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &myIF1_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;
	val2.copyfrom(*inout_param.mutable_val2());

	return inout_param.___retval();
}

int32_t myIF1::add_listener(int val1, myIF2_listener &listener)
{
	::mytest_pbf::___myIF1_add_listener_inparam inparam;
	::mytest_pbf::___myIF1_add_listener_inout_param inout_param;
	uint128_t uid = {0xF716DBF4, 0x19F, 0xB9BE, {0x65, 0xCC, 0xDE, 0x49, 0x18, 0x1C, 0x1A, 0xC9}}; // todo: TBD

	inparam.set_val1(val1);
	inparam.set_observable_id((size_t)&listener);
	std::string *cliname = new std::string();
	std::string *instname = new std::string();
	rpcobservable_data* data = rpcobservable::inst()->get_observable_data();
	assert(NULL != data);
	*cliname = data->pkg_name;
	*instname = data->inst_name;
	inparam.set_allocated_client_name(cliname);
	inparam.set_allocated_inst_name(instname);

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &myIF1_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	return inout_param.___retval();
}

static void ___myIF2_listener_on_method1_obsvhdr(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)
{
	assert(nullptr != obj);

	auto* inp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF2_listener_on_method1_inparam,	\
		inparam->get_pbfmessage());
	auto* inoutp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___myIF2_listener_on_method1_inout_param,	\
		inout_param->get_pbfmessage());

	::mytest::test_struct1 var2(inout_param, inoutp->mutable_var2());
	inoutp->set____retval(((myIF2_listener*)obj)->on_method1(inp->val1(), var2));
}

myIF2_listener::myIF2_listener()
{
	static bool registered = false;
	if (!registered)
	{
		pbf_wrapper inparam, inout_param;
		rpcinterface interface = rpcobservable::inst()->
			register_observable_interface("myIF2_listener.on_method1");

		uint128_t u1 = {0xB60D8870, 0xA242, 0x1A49, {0xC6, 0x95, 0xA2, 0x3F, 0x06, 0x86, 0x3C, 0xA0}}; // todo: TBD
		inparam = new ::mytest::struct_wrapper::___myIF2_listener_on_method1_inparam_wrapper;
		inout_param = new ::mytest::struct_wrapper::___myIF2_listener_on_method1_inout_param_wrapper;
		interface->add_method(u1, inparam, inout_param, (void*)___myIF2_listener_on_method1_obsvhdr);
		registered = true;
	}
	rpcobservable::inst()->add_observable_instance("mytest.myIF2_listener", (void*)this);
}

myIF2_listener::~myIF2_listener()
{
}

int32_t myIF2_listener::on_method1(int32_t val1, ::mytest::test_struct1& val2)
{
	return 0;
}

const char *myIF3_singleton::myIF3_singleton_clsname_ = "mytest.myIF3_singleton";
rpcproxy_static_data myIF3_singleton::myIF3_singleton_static_data_ = {
	nullptr,
	nullptr,
	"zas.system",
	"rpc.service2",
	nullptr,
	myIF3_singleton_clsname_,
};

myIF3_singleton& myIF3_singleton::inst(void)
{
	static myIF3_singleton _inst;
	if (!_inst._proxy_base) {
		_inst._proxy_base = rpcproxy::get_singleton_instance(
			&myIF3_singleton_static_data_);
		assert(_inst._proxy_base->is_vaild());
	}
	return _inst;
}

::mytest::test_struct2
myIF3_singleton::method1(const std::string& val1, int32_t &val2)
{
	::mytest_pbf::___myIF3_method1_inparam inparam;
	::mytest_pbf::___myIF3_method1_inout_param inout_param;
	uint128_t uid = {0xE716DBF3, 0x19F, 0xB9BE, {0x65, 0xCC, 0xDE, 0x49, 0x18, 0x1C, 0x1A, 0xA9}}; // todo: TBD

	inparam.set_val1(val1);
	inout_param.set_val2(val2);

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &myIF3_singleton_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	::mytest::test_struct2 retval;
	retval.copyfrom(inout_param.___retval());
	return retval;
}

myIF1 myIF3_singleton::method2(myIF1& val1)
{
	::mytest_pbf::___myIF3_method2_inout_param inout_param;
	uint128_t uid = {0xE05CF8F2, 0x36CF, 0xB8F5, {0xDB, 0xE4, 0xBE, 0x74, 0x00, 0x00, 0x00, 0x20}};
	
	inout_param.set_val1((size_t)val1.get_instid());
	auto* ret = rpcproxy::invoke(get_instid(), uid,
		nullptr, &inout_param, &myIF3_singleton_static_data_, 2);
	if (nullptr != ret) _proxy_base = ret;

	proxy::myIF1 retval((void*)inout_param.___retval());
	return retval;
}

}} // end of namespace mytest::proxy