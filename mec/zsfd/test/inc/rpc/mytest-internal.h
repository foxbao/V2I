#ifndef __CXX_ZAS_MWARE_RPC_MYTEST_INTERNAL_H__
#define __CXX_ZAS_MWARE_RPC_MYTEST_INTERNAL_H__

#include "mytest.h"
#include "test.pro.pb.h"

using namespace zas::mware::rpc;

namespace mytest {
namespace struct_wrapper {

class ___test_struct2_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::test_struct2 _protobuf_object;
};

class ___test_struct1_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::test_struct1 _protobuf_object;
};

class ___myIF1_method1_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF1_method1_inparam _protobuf_object;
};

class ___myIF1_method1_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF1_method1_inout_param _protobuf_object;
};

class ___myIF3_method1_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF3_method1_inparam _protobuf_object;
};

class ___myIF3_method1_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF3_method1_inout_param _protobuf_object;
};

class ___myIF3_method2_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF3_method2_inout_param _protobuf_object;
};

class ___myIF1_add_listener_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF1_add_listener_inparam _protobuf_object;
};

class ___myIF1_add_listener_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF1_add_listener_inout_param _protobuf_object;
};

class ___myIF2_listener_on_method1_inparam_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF2_listener_on_method1_inparam _protobuf_object;
};

class ___myIF2_listener_on_method1_inout_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___myIF2_listener_on_method1_inout_param _protobuf_object;
};

class ___rpc_event_test_with_param_wrapper : public pbf_wrapper_object
{
public:
	google::protobuf::Message* get_pbfmessage(void) {
		return &_protobuf_object;
	}

private:
	mytest_pbf::___rpc_event_test_with_param _protobuf_object;
};

} // end of namespace struct_wrapper
} // end of namespace mytest
#endif /* __CXX_ZAS_MWARE_RPC_MYTEST_INTERNAL_H__ */
/* EOF */
