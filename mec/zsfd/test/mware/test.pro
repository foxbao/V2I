syntax = "proto3";
option optimize_for = CODE_SIZE;

package mytest_pbf;

message test_struct2
{
	int32 var_1 = 1;
	int32 var_2 = 2;
	repeated int32 var_3 = 3;
}

message test_struct2_array
{
	repeated test_struct2 array = 1;
}

message test_struct1
{
	int32 var_a = 1;
	int32 var_b = 2;
	test_struct2 var_c = 3;
	repeated test_struct2 var_d = 4;
}

message ___myIF1_method1_inparam
{
	int32 val1 = 1;
}

message ___myIF1_method1_inout_param
{
	test_struct2 val2 = 1;
	int32 ___retval = 2;
}

message ___myIF2_listener_on_method1_inparam
{
	int32 val1 = 1;
}

message ___myIF2_listener_on_method1_inout_param
{
	test_struct1 var2 = 1;
	int32 ___retval = 2;
}

message ___myIF3_method1_inparam
{
	string val1 = 1;
}

message ___myIF3_method1_inout_param
{
	int32 val2 = 1;
	test_struct2 ___retval = 2;
}

message ___myIF3_method2_inout_param
{
	uint64 val1 = 1;
	uint64 ___retval = 2;
}

message ___rpc_event_test_with_param
{
	test_struct2 val2 = 1;
}

message ___myIF1_add_listener_inparam
{
	int32 val1 = 1;
	int64 observable_id = 2;
	string client_name = 3;
	string inst_name = 4;
}

message ___myIF1_add_listener_inout_param
{
	int32 ___retval = 2;
}
