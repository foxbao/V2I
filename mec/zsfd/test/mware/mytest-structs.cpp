#include "mytest-internal.h"

namespace mytest {

void rpc_event_test_with_param::start_listen(void)
{
	printf("start listener\n");
	pbf_wrapper event_data =
		new ::mytest::struct_wrapper::___rpc_event_test_with_param_wrapper;
	rpcevent::start_listen("mytest.rpc_event_test_with_param",
		event_data);
}	

void rpc_event_test_with_param::on_triggered(pbf_wrapper event)
{
	printf("rpcevent ontrigger with param\n");
	if (!event.get()) {
		printf("rpc_event_test_with_param on_triggered data NULL\n");
		return;
	}
	auto* inp = zas_downcast(google::protobuf::Message,	\
		::mytest_pbf::___rpc_event_test_with_param,	\
		event->get_pbfmessage());
	::mytest::test_struct2 val2(event, inp->mutable_val2());
	on_rpc_event_test_with_param_triggered(val2);
}	

test_struct2::test_struct2()
{
	_pbf_wrapper = new struct_wrapper::___test_struct2_wrapper();
	assert(_pbf_wrapper.get());
	_pbfobj = reinterpret_cast<mytest_pbf::test_struct2*>
		(_pbf_wrapper->get_pbfmessage());
	assert(_pbfobj);
}

test_struct2::test_struct2(pbf_wrapper wrapper, ::mytest_pbf::test_struct2* pbfmsg)
: _pbf_wrapper(wrapper), _pbfobj(pbfmsg)
{
	assert(nullptr != pbfmsg);
}

const test_struct2& test_struct2::assign(const test_struct2& obj)
{
	if (this == &obj || _pbfobj == obj._pbfobj) {
		return *this;
	}
	*_pbfobj = *obj._pbfobj;
	return *this;
}

void test_struct2::copyfrom(const ::mytest_pbf::test_struct2& obj)
{
	if (&obj == _pbfobj) return;
	*_pbfobj = obj;
}

void test_struct2::copyto(::mytest_pbf::test_struct2& obj) const
{
	if (&obj == _pbfobj) return;
	obj = *_pbfobj;
}

int test_struct2::___clazz_var_1::operator=(int val)
{
	auto* pobj = ancestor()->_pbfobj;
	pobj->set_var_1(val);
}

test_struct2::___clazz_var_1::operator int(void)
{
	auto* pobj = ancestor()->_pbfobj;
	return pobj->var_1();
}

const zas::mware::rpc::int32_array&
test_struct2::___clazz_var_3::operator=(
	const zas::mware::rpc::int32_array& val)
{
	return zas::mware::rpc::int32_array::operator=(val);
}

int& test_struct2::___clazz_var_3::operator[](int index)
{
	auto* pobj = ancestor()->_pbfobj;
	if (index >= 10) {
		throw_rpc_error_array_exceed_boundary();
	}
	if (index < 0 || index >= pobj->var_3_size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	return *ancestor()->_pbfobj->mutable_var_3()->Mutable(index);
}

void test_struct2::___clazz_var_3::clear(void)
{
	auto* pobj = ancestor()->_pbfobj;
	pobj->clear_var_3();
}

int test_struct2::___clazz_var_3::size(void) const
{
	auto* pobj = ancestor()->_pbfobj;
	return pobj->var_3_size();
}

int test_struct2::___clazz_var_3::get(int index) const
{
	auto* pobj = ancestor()->_pbfobj;
	if (index >= 10) {
		throw_rpc_error_array_exceed_boundary();
	}
	if (index < 0 || index >= pobj->var_3_size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	return pobj->var_3(index);
}

int test_struct2::___clazz_var_3::append(int val)
{
	auto* pobj = ancestor()->_pbfobj;
	if (pobj->var_3_size() >= 10) {
		throw new zas::mware::rpc::rpc_error(
			__FILE__, __LINE__,
			errcat_proxy, err_scenario_others,
			errid_array_exceed_boundary);		
	}

	pobj->add_var_3(val);
	return val;
}
/*
int test_struct2::___clazz_var_3::___clazz_var_3_element::operator=(
	int val) {
	auto* p = _p->ancestor()->_pbfobj;
	if (_index < 0 || _index >= p->var_3_size()) {
		throw new zas::mware::rpc::rpc_error(
			__FILE__, __LINE__,
			errcat_proxy, err_scenario_others,
			errid_array_exceed_boundary);
	}
	p->set_var_3(_index, val);
	return val;
}

const test_struct2::___clazz_var_3::___clazz_var_3_element&
test_struct2::___clazz_var_3::___clazz_var_3_element::operator=(
	const test_struct2::___clazz_var_3::___clazz_var_3_element& ele)
{
	auto* to = _p->ancestor()->_pbfobj;
	auto* from = ele._p->ancestor()->_pbfobj;
	if (from == to) return *this;
	
	// validaty check
	if (_index < 0 || _index >= to->var_3_size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	if (ele._index < 0 || ele._index >= from->var_3_size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	to->set_var_3(_index, from->var_3(ele._index));
	return *this;
}

test_struct2::___clazz_var_3::___clazz_var_3_element::operator int(void)
{
	auto* p = _p->ancestor()->_pbfobj;
	if (_index < 0 || _index >= p->var_3_size()) {
		throw new zas::mware::rpc::rpc_error(
			__FILE__, __LINE__,
			errcat_proxy, err_scenario_others,
			errid_array_exceed_boundary);
	}
	return p->var_3(_index);
}
*/
namespace array {

test_struct2::test_struct2(int maxsz)
: _array(new mytest_pbf::test_struct2_array())
, _max_size(maxsz) {
	assert(nullptr != _array);
}

test_struct2::~test_struct2()
{
	if (nullptr != _array) {
		delete _array;
		_array = nullptr;
	}
}

const test_struct2& test_struct2::operator=(
	const test_struct2& val)
{
	if (this == &val) {
		return *this;
	}
	int sz = val.size();
	if (_max_size > 0 && sz > _max_size) {
		sz = _max_size;
	}
	// copy the array
	clear();
	for (int i = 0; i < sz; ++i) {
		add(val.array_object(i));
	}
	return *this;
}

::mytest::test_struct2 test_struct2::operator[](int index)
{
	if (_max_size > 0 && index >= _max_size) {
		throw_rpc_error_array_exceed_boundary();
	}
	if (index >= size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	return ::mytest::test_struct2(
		zas::mware::rpc::pbf_wrapper(nullptr),
		&mutable_array_object(index));
}

void test_struct2::clear(void)
{
	if (_array) {
		_array->clear_array();
	}
}

int test_struct2::size(void) const
{
	if (!_array) {
		return 0;
	}
	int ret = _array->array_size();
	if (_max_size > 0 && _max_size < ret) {
		ret = _max_size;
	}
	return ret;
}

const mytest_pbf::test_struct2& test_struct2::array_object(int index) const
{
	if (index < 0 || index >= size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	assert(nullptr != _array);
	return _array->array(index);
}

void test_struct2::add(const mytest_pbf::test_struct2& obj)
{
	assert(nullptr != _array);
	if (_max_size > 0 && size() >= _max_size) {
		throw_rpc_error_array_exceed_boundary();
	}
	auto* new_obj = _array->add_array();
	*new_obj = obj;
}

mytest_pbf::test_struct2& test_struct2::add(void)
{
	assert(nullptr != _array);
	if (_max_size > 0 && size() >= _max_size) {
		throw_rpc_error_array_exceed_boundary();
	}
	return *_array->add_array();
}

mytest_pbf::test_struct2& test_struct2::mutable_array_object(int index)
{
	if (index < 0 || index >= size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	assert(nullptr != _array);
	return *_array->mutable_array(index);
}

} // end of namespace array

test_struct1::test_struct1()
: _pbfobj(NULL)
{
	_pbf_wrapper = new struct_wrapper::___test_struct1_wrapper();
	assert(_pbf_wrapper.get());
	_pbfobj = reinterpret_cast<mytest_pbf::test_struct1*>
		(_pbf_wrapper->get_pbfmessage());
	assert(_pbfobj);
	var_c.___bind(_pbf_wrapper, _pbfobj->mutable_var_c());
}

test_struct1::test_struct1(pbf_wrapper wrapper, ::mytest_pbf::test_struct1* pbfmsg)
: _pbf_wrapper(wrapper), _pbfobj(pbfmsg)
, var_c(wrapper, pbfmsg->mutable_var_c())
{
	assert(wrapper.get() && pbfmsg);
}

const test_struct1& test_struct1::assign(const test_struct1& obj)
{
	if (this == &obj) return *this;
	*_pbfobj = *obj._pbfobj;
	return *this;
}

void test_struct1::copyto(::mytest_pbf::test_struct1& obj) const
{
	if (&obj == _pbfobj) return;
	obj = *_pbfobj;
}

const array::test_struct2&
test_struct1::___clazz_var_d::operator=(
	const array::test_struct2& val)
{
	return array::test_struct2::operator=(val);
}

::mytest::test_struct2
test_struct1::___clazz_var_d::operator[](int index)
{
	if (index >= 10) {
		throw_rpc_error_array_exceed_boundary();
	}
	if (index < 0 || index >= size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	return ::mytest::test_struct2(
		ancestor()->_pbf_wrapper, 
		&mutable_array_object(index));
}

const ::mytest::test_struct2
test_struct1::___clazz_var_d::get(int index) const
{
	if (index >= 10) {
		throw_rpc_error_array_exceed_boundary();
	}
	if (index < 0 || index >= size()) {
		throw_rpc_error_array_exceed_boundary();
	}
	mytest_pbf::test_struct2* item = const_cast<
		mytest_pbf::test_struct2*>(&array_object(index));
	return ::mytest::test_struct2(
		ancestor()->_pbf_wrapper, item);
}

const ::mytest::test_struct2
test_struct1::___clazz_var_d::append(const ::mytest::test_struct2& val)
{
	if (size() >= 10) {
		throw_rpc_error_array_exceed_boundary();
	}
	auto& item = add();
	val.copyto(item);
	return ::mytest::test_struct2(ancestor()->_pbf_wrapper, &item);
}

void test_struct1::___clazz_var_d::clear(void)
{
	auto* p = ancestor()->_pbfobj;
	p->clear_var_d();
}

int test_struct1::___clazz_var_d::size(void) const
{
	auto* p = ancestor()->_pbfobj;
	return p->var_d_size();
}

const mytest_pbf::test_struct2&
test_struct1::___clazz_var_d::array_object(int index) const
{
	auto* p = ancestor()->_pbfobj;
	return p->var_d(index);
}

void test_struct1::___clazz_var_d::add(const mytest_pbf::test_struct2& obj)
{
	auto* p = ancestor()->_pbfobj;
	auto* new_obj = p->add_var_d();
	*new_obj = obj;
}

mytest_pbf::test_struct2& test_struct1::___clazz_var_d::add(void)
{
	auto* p = ancestor()->_pbfobj;
	return *p->add_var_d();
}

mytest_pbf::test_struct2&
test_struct1::___clazz_var_d::mutable_array_object(int index)
{
	auto* p = ancestor()->_pbfobj;
	return *p->mutable_var_d(index);
}

} // end of namespace mytest
/* EOF */
