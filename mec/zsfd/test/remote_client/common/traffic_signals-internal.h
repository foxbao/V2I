/*
 * Generated by the zrpc compiler (rpcc).  DO NOT EDIT!
 * @(#)$Id: traffic_signals-internal.h
 */

#ifndef __CXX_ZRPCC_AUTOGEN_TRAFFIC_SIGNAL_TRAFFIC_SIGNALS_INTERNAL_H__
#define __CXX_ZRPCC_AUTOGEN_TRAFFIC_SIGNAL_TRAFFIC_SIGNALS_INTERNAL_H__

#include "traffic_signals.h"
#include "proto/traffic_signals.pro.pb.h"

using namespace zas::mware::rpc;

namespace traffic_signals {
namespace struct_wrapper {

class ___movement_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::movement _protobuf_object;
};

class ___intersection_info_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::intersection_info _protobuf_object;
};

class ___gps_info_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::gps_info _protobuf_object;
};

class ___vehicle_info_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::vehicle_info _protobuf_object;
};

class ___vehilce_dt_update_vehicle_info_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___vehilce_dt_update_vehicle_info_inparam _protobuf_object;
};

class ___vehilce_dt_update_vehicle_info_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___vehilce_dt_update_vehicle_info_inout_param _protobuf_object;
};

class ___traffic_signal_listener_on_traffic_light_changed_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_listener_on_traffic_light_changed_inparam _protobuf_object;
};

class ___traffic_signal_info_get_current_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_info_get_current_inout_param _protobuf_object;
};

class ___traffic_signal_info_register_traffic_signal_listener_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_info_register_traffic_signal_listener_inparam _protobuf_object;
};

class ___traffic_signal_info_register_traffic_signal_listener_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_info_register_traffic_signal_listener_inout_param _protobuf_object;
};

class ___traffic_signal_info_deregister_traffic_signal_listener_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_info_deregister_traffic_signal_listener_inparam _protobuf_object;
};

class ___traffic_signal_info_deregister_traffic_signal_listener_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_info_deregister_traffic_signal_listener_inout_param _protobuf_object;
};

class ___traffic_signal_mgr_register_vehicle_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_mgr_register_vehicle_inparam _protobuf_object;
};

class ___traffic_signal_mgr_register_vehicle_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_mgr_register_vehicle_inout_param _protobuf_object;
};

class ___traffic_signal_mgr_get_traffic_signal_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_mgr_get_traffic_signal_inparam _protobuf_object;
};

class ___traffic_signal_mgr_get_traffic_signal_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	traffic_signals_pbf::___traffic_signal_mgr_get_traffic_signal_inout_param _protobuf_object;
};

}// end of namespace struct_wrapper
}// end of namespace traffic_signals
#endif /* __CXX_ZRPCC_AUTOGEN_TRAFFIC_SIGNAL_TRAFFIC_SIGNALS_INTERNAL_H__ */
/* EOF */