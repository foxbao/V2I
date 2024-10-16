#include "common/traffic_signals-internal.h"

namespace traffic_signals {
namespace proxy {

const char* vehilce_dt::vehilce_dt_clsname_ = "traffic_signals.vehilce_dt";
rpcproxy_static_data vehilce_dt::vehilce_dt_static_data_ = {
	nullptr,
	nullptr,
	"zas.traffic",
	"traffic_signal",
	nullptr,
	vehilce_dt_clsname_,
};

int32_t vehilce_dt::update_vehicle_info(::traffic_signals::vehicle_info& info)
{
	::traffic_signals_pbf::___vehilce_dt_update_vehicle_info_inparam inparam;
	::traffic_signals_pbf::___vehilce_dt_update_vehicle_info_inout_param inout_param;
	uint128_t uid = {0x6F3EF2, 0xDCD7, 0x21FB, {0x69, 0x2B, 0x24, 0xC9, 0x5A, 0x09, 0xEF, 0xBF}};

	info.copyto(*inparam.mutable_info());

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &vehilce_dt_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	return inout_param.___retval();
}

static int ___traffic_signal_listener_on_traffic_light_changed_obsvhdr(void* obj, pbf_wrapper inparam, pbf_wrapper inout_param)
{
	assert(nullptr != obj);

	auto* inp = zas_downcast(google::protobuf::Message,	\
		::traffic_signals_pbf::___traffic_signal_listener_on_traffic_light_changed_inparam,	\
		inparam->get_pbfmessage());
	::traffic_signals::intersection_info int_info(inparam, inp->mutable_int_info());
	(((traffic_signal_listener*)obj)->on_traffic_light_changed(int_info));

	return 1;
}

traffic_signal_listener::traffic_signal_listener()
{
	static bool registered = false;
	if (!registered)
	{
		pbf_wrapper inparam, inout_param;
		rpcinterface interface = rpcobservable::inst()->
			register_observable_interface("traffic_signals.traffic_signal_listener");

		uint128_t u_on_traffic_light_changed = {0x64E57751, 0x3E98, 0x38, {0x1F, 0x84, 0x63, 0x07, 0xEA, 0xF9, 0xDD, 0x9B}};

		inparam = new ::traffic_signals::struct_wrapper::___traffic_signal_listener_on_traffic_light_changed_inparam_wrapper;
		interface->add_method(u_on_traffic_light_changed, inparam, nullptr, (void*)___traffic_signal_listener_on_traffic_light_changed_obsvhdr);
		registered = true;
	}
	rpcobservable::inst()->add_observable_instance("traffic_signals.traffic_signal_listener",(void*)this);
}

traffic_signal_listener::~traffic_signal_listener()
{
}

void traffic_signal_listener::on_traffic_light_changed(::traffic_signals::intersection_info& int_info)
{

}

const char* traffic_signal_info::traffic_signal_info_clsname_ = "traffic_signals.traffic_signal_info";
rpcproxy_static_data traffic_signal_info::traffic_signal_info_static_data_ = {
	nullptr,
	nullptr,
	"zas.traffic",
	"traffic_signal",
	nullptr,
	traffic_signal_info_clsname_,
};

::traffic_signals::intersection_info& traffic_signal_info::get_current()
{
	::traffic_signals_pbf::___traffic_signal_info_get_current_inout_param inout_param;
	uint128_t uid = {0xB3B5A110, 0xC36C, 0x3A84, {0xCF, 0xE2, 0xF5, 0x0C, 0x70, 0xD9, 0x9E, 0x62}};


	auto* ret = rpcproxy::invoke(get_instid(), uid,
		nullptr, &inout_param, &traffic_signal_info_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	::traffic_signals::intersection_info *retval = new::traffic_signals::intersection_info;
	retval->copyfrom(*inout_param.mutable____retval());
 return *retval;
}

int32_t traffic_signal_info::register_traffic_signal_listener(traffic_signal_listener& lnr)
{
	::traffic_signals_pbf::___traffic_signal_info_register_traffic_signal_listener_inparam inparam;
	::traffic_signals_pbf::___traffic_signal_info_register_traffic_signal_listener_inout_param inout_param;
	uint128_t uid = {0xB421D938, 0x90CC, 0x5E01, {0xEC, 0xA7, 0xF6, 0xA2, 0xDD, 0xE0, 0x03, 0x19}};

	inparam.set_lnr((size_t)& lnr);
	rpcobservable_data* data = rpcobservable::inst()->get_observable_data();
	assert(NULL != data);
	std::string cliname = data->pkg_name;
	std::string instname = data->inst_name;
	inparam.set_client_name_lnr(cliname);
	inparam.set_inst_name_lnr(instname);

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &traffic_signal_info_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	return inout_param.___retval();
}

int32_t traffic_signal_info::deregister_traffic_signal_listener(traffic_signal_listener& lnr)
{
	::traffic_signals_pbf::___traffic_signal_info_deregister_traffic_signal_listener_inparam inparam;
	::traffic_signals_pbf::___traffic_signal_info_deregister_traffic_signal_listener_inout_param inout_param;
	uint128_t uid = {0xC207392F, 0xA710, 0x8880, {0x83, 0x84, 0xDA, 0xB1, 0xF7, 0x29, 0xE5, 0x98}};

	inparam.set_lnr((size_t)& lnr);
	rpcobservable_data* data = rpcobservable::inst()->get_observable_data();
	assert(NULL != data);
	std::string cliname = data->pkg_name;
	std::string instname = data->inst_name;
	inparam.set_client_name_lnr(cliname);
	inparam.set_inst_name_lnr(instname);

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &traffic_signal_info_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	return inout_param.___retval();
}

const char* traffic_signal_mgr::traffic_signal_mgr_clsname_ = "traffic_signals.traffic_signal_mgr";
rpcproxy_static_data traffic_signal_mgr::traffic_signal_mgr_static_data_ = {
	nullptr,
	nullptr,
	"zas.traffic",
	"traffic_signal",
	nullptr,
	traffic_signal_mgr_clsname_,
};

traffic_signal_mgr& traffic_signal_mgr::inst(void)
{
	static traffic_signal_mgr _inst;
	if (!_inst._proxy_base) {
		_inst._proxy_base = rpcproxy::get_singleton_instance(
			&traffic_signal_mgr_static_data_);
		assert(_inst._proxy_base->is_vaild());
	}
	return _inst;
}

vehilce_dt& traffic_signal_mgr::register_vehicle(::traffic_signals::vehicle_info& info)
{
	::traffic_signals_pbf::___traffic_signal_mgr_register_vehicle_inparam inparam;
	::traffic_signals_pbf::___traffic_signal_mgr_register_vehicle_inout_param inout_param;
	uint128_t uid = {0x87DADA29, 0x1B2D, 0x6044, {0xAF, 0xCD, 0x4F, 0x4E, 0x7E, 0x6C, 0xE5, 0x54}};

	info.copyto(*inparam.mutable_info());

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &traffic_signal_mgr_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	auto *retval = new vehilce_dt((void*)inout_param.___retval());
	return *retval;
}

traffic_signal_info& traffic_signal_mgr::get_traffic_signal(::traffic_signals::vehicle_info& info)
{
	::traffic_signals_pbf::___traffic_signal_mgr_get_traffic_signal_inparam inparam;
	::traffic_signals_pbf::___traffic_signal_mgr_get_traffic_signal_inout_param inout_param;
	uint128_t uid = {0xD7A1940D, 0x939D, 0x1787, {0xD3, 0xD4, 0x07, 0x7B, 0xC3, 0x35, 0x5B, 0x49}};

	info.copyto(*inparam.mutable_info());

	auto* ret = rpcproxy::invoke(get_instid(), uid,
		&inparam, &inout_param, &traffic_signal_mgr_static_data_, 1);
	if (nullptr != ret) _proxy_base = ret;

	auto *retval = new traffic_signal_info((void*)inout_param.___retval());
	return *retval;
}

}} // end of namespace traffic_signals::proxy