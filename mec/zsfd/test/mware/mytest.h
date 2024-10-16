#ifndef __CXX_ZAS_MWARE_RPC_MYTEST_H__
#define __CXX_ZAS_MWARE_RPC_MYTEST_H__

#include "mware/rpc/codegen-helper.h"

using namespace zas::mware::rpc;

namespace mytest_pbf {
	// protobuffer classes
	class test_struct1;
	class test_struct2;
	class test_struct2_array;
}

namespace mytest {

class test_struct2
{
public:
	test_struct2();
	test_struct2(pbf_wrapper wrapper, ::mytest_pbf::test_struct2* pbfmsg);
	test_struct2(const test_struct2& obj) {
		assign(obj);
	}

	void copyfrom(const ::mytest_pbf::test_struct2& obj);
	void copyto(::mytest_pbf::test_struct2& obj) const;

	inline const test_struct2& operator=(const test_struct2& obj) {
		return assign(obj);
	}

	void ___bind(pbf_wrapper wrapper,
		::mytest_pbf::test_struct2* pbfmsg) {
		_pbfobj = pbfmsg;
		_pbf_wrapper = wrapper;
	}

	class ___clazz_var_1
	{
	public:
		int operator=(int val);
		operator int(void);

	private:
		inline test_struct2* ancestor(void) const {
			return _ANCESTOR(test_struct2, var_1);
		}
	} var_1;

	class ___clazz_var_3 : public zas::mware::rpc::int32_array
	{
	public:
		const zas::mware::rpc::int32_array& operator=(
			const zas::mware::rpc::int32_array& val);
		int& operator[](int index);

		void clear(void);
		int size(void) const;
		int get(int index) const;
		int append(int val);

	private:
		inline test_struct2* ancestor(void) const {
			return _ANCESTOR(test_struct2, var_3);
		}
	} var_3;

private:
	const test_struct2& assign(const test_struct2& obj);
	::mytest_pbf::test_struct2* _pbfobj;
	pbf_wrapper _pbf_wrapper;
};

/*
message test_struct1
{
	int32 var_a = 1;
	int32 var_b = 2;
	test_struct2 var_c = 3;
	repeated test_struct2 var_d = 4;
}
*/
namespace array {

class test_struct2
{
public:
	test_struct2(int maxsz = -1);
	~test_struct2();
	const test_struct2& operator=(const test_struct2& val);
	::mytest::test_struct2 operator[](int index);

	virtual void clear(void);
	virtual int size(void) const;

protected:
	virtual const mytest_pbf::test_struct2& array_object(int index) const;
	virtual void add(const mytest_pbf::test_struct2& obj);
	virtual mytest_pbf::test_struct2& add(void);
	virtual mytest_pbf::test_struct2& mutable_array_object(int index);

private:
	mytest_pbf::test_struct2_array* _array;
	int _max_size;
};

} // end of namespace array

class test_struct1
{
public:
	test_struct1();
	test_struct1(pbf_wrapper wrapper, ::mytest_pbf::test_struct1* pbfmsg);
	test_struct1(const test_struct1& obj) {
		assign(obj);
	}
	void copyto(::mytest_pbf::test_struct1& obj) const;

	const test_struct1& operator=(const test_struct1& obj) {
		return assign(obj);
	}

	void ___bind(pbf_wrapper wrapper,
		::mytest_pbf::test_struct1* pbfmsg) {
		_pbfobj = pbfmsg;
		_pbf_wrapper = wrapper;
	}

	test_struct2 var_c;

	class ___clazz_var_d : public array::test_struct2
	{
	public:
		const array::test_struct2& operator=(const array::test_struct2& val);
		::mytest::test_struct2 operator[](int index);

		int count(void) const { return size(); }
		const ::mytest::test_struct2 get(int index) const;
		const ::mytest::test_struct2 append(const ::mytest::test_struct2& val);

		void clear(void);
		int size(void) const;

	protected:
		const mytest_pbf::test_struct2& array_object(int index) const;
		void add(const mytest_pbf::test_struct2& obj);
		mytest_pbf::test_struct2& add(void);
		mytest_pbf::test_struct2& mutable_array_object(int index);

	private:
		inline test_struct1* ancestor(void) const {
			return _ANCESTOR(test_struct1, var_d);
		}
	} var_d;

private:
	const test_struct1& assign(const test_struct1& obj);
	::mytest_pbf::test_struct1* _pbfobj;
	pbf_wrapper _pbf_wrapper;
};

class rpc_event_no_param : public rpcevent
{
public:
	rpc_event_no_param(bool auto_listen = false) {
		if (!auto_listen) return;
		start_listen();
	}

	virtual ~rpc_event_no_param() {
		rpcevent::end_listen("mytest.rpc_event_no_param");
	}

public:
	void start_listen(void) {
		pbf_wrapper event_data;
		rpcevent::start_listen("mytest.rpc_event_no_param",event_data);
	}

	void end_listen(void) {
		rpcevent::end_listen("mytest.rpc_event_no_param");
	}

	void force_trigger_last_event(void) {
		rpcevent::force_trigger_last_event(
			"mytest.rpc_event_no_param");
	}

	virtual void on_rpc_event_no_param_triggered(void) {

	}

protected:
	void on_triggered(pbf_wrapper event) {
			printf("rpcevent ontrigger no param\n");
		on_rpc_event_no_param_triggered();
	}

private:
	/* disable evil constructors */
	rpc_event_no_param(const rpc_event_no_param&);
	void operator=(const rpc_event_no_param&);
};

class rpc_event_test_with_param : public rpcevent
{
public:
	rpc_event_test_with_param(bool auto_listen = false) {
		if (!auto_listen) return;
		start_listen();
	}

	virtual ~rpc_event_test_with_param() {
		rpcevent::end_listen("mytest.rpc_event_test_with_param");
	}

public:
	void start_listen(void);

	void end_listen(void) {
		rpcevent::end_listen("mytest.rpc_event_test_with_param");
	}

	void force_trigger_last_event(void) {
		rpcevent::force_trigger_last_event(
			"mytest.rpc_event_test_with_param");
	}

	virtual void on_rpc_event_test_with_param_triggered(test_struct2 &testdata) {

	}

protected:
	void on_triggered(pbf_wrapper event);

private:
	/* disable evil constructors */
	rpc_event_test_with_param(const rpc_event_test_with_param&);
	void operator=(const rpc_event_test_with_param&);
};

namespace proxy {

// an observable object
class myIF2_listener
{
public:
	myIF2_listener();
	virtual ~myIF2_listener();

public:
	virtual int32_t on_method1(int32_t val1, ::mytest::test_struct1& val2);
};

// an non-singleton interface
class myIF1
{
public:
	myIF1()
	: _proxy_base(nullptr) {
	}

	myIF1(const myIF1& obj)
	: _proxy_base(nullptr) {
		if (nullptr == obj._proxy_base) {
			return;
		}
		_proxy_base = obj._proxy_base;
		_proxy_base->addref();
	}

	myIF1(void* instid)
	: _proxy_base(rpcproxy::from_instid(instid,
		&myIF1_static_data_)) {
		assert(nullptr != _proxy_base);
		_proxy_base->addref();
	}

	~myIF1() {
		if (nullptr != _proxy_base) {
			_proxy_base->release();
			_proxy_base = nullptr;
		}
	}

	const myIF1& operator=(const myIF1& obj) {
		if (this == &obj) {
			return *this;
		}
		if (_proxy_base) _proxy_base->release();
		if (obj._proxy_base) obj._proxy_base->addref();
		_proxy_base = obj._proxy_base;
		return *this;
	}

	void* get_instid(void) {
		if (_proxy_base)
			return _proxy_base->get_instid();
		_proxy_base = rpcproxy::get_instance(nullptr, &myIF1_static_data_);
		return (_proxy_base)
		? _proxy_base->get_instid()
		: nullptr;
	}

public:
	int32_t method1(int val1, test_struct2& val2);
	int32_t add_listener(int val1, myIF2_listener &listener);

private:
	rpcproxy*	_proxy_base;
	static const char* myIF1_clsname_;
	static rpcproxy_static_data myIF1_static_data_;
};

class myIF3_singleton
{
public:
	myIF3_singleton()
	: _proxy_base(nullptr) {
	}

	myIF3_singleton(void* instid)
	: _proxy_base(rpcproxy::from_instid(instid,
		&myIF3_singleton_static_data_)) {
		assert(nullptr != _proxy_base);
		_proxy_base->addref();
	}

	myIF3_singleton(const myIF3_singleton&) = delete;

public:
	static myIF3_singleton& inst(void);
	virtual ~myIF3_singleton() {
		if (nullptr != _proxy_base) {
			_proxy_base->release();
			_proxy_base = nullptr;
		}
	}

	void* get_instid(void) {
		return (_proxy_base)
		? _proxy_base->get_instid()
		: nullptr;
	}

private:
	const myIF3_singleton& operator=(const myIF3_singleton&);

public:
	::mytest::test_struct2 method1(const std::string& val1, int32_t &val2);
	myIF1 method2(myIF1& val1);

private:
	rpcproxy* _proxy_base;
	static const char* myIF3_singleton_clsname_;
	static rpcproxy_static_data myIF3_singleton_static_data_;
};


} // end of namespace proxy

namespace skeleton {
	void import_myIF1(void);
};

namespace skeleton {

class myIF2_listener
{
public:
	myIF2_listener();
	myIF2_listener(void* inst_id, const char* cliname, const char* inst_name);
	myIF2_listener(const myIF2_listener&);
	const myIF2_listener& operator=(const myIF2_listener&);
	~myIF2_listener();

public:
	int32_t on_method1(int32_t val1, ::mytest::test_struct1& val2);

private:
	void* _inst_id;
	std::string _cliname;
	std::string _inst_name;
};
};

// an non-singleton interface
class myIF1 : public skeleton_base
{
public:
	myIF1();
	~myIF1();	// if there is virtual method

	myIF1(const myIF1&) = delete;
	void operator=(const myIF1&) = delete;

	static myIF1* create_instance(void);
	static void destroy_instance(myIF1*);

public:
	virtual int32_t method1(int val1, test_struct2& val2);
	virtual int32_t add_listener(int val1, skeleton::myIF2_listener &listener);
};

namespace skeleton {
	void import_myIF3_singleton(void);
};

class myIF3_singleton : public skeleton_base
{
public:
	myIF3_singleton();
	virtual ~myIF3_singleton();

	myIF3_singleton(const myIF3_singleton&) = delete;
	void operator=(const myIF3_singleton&) = delete;

	static myIF3_singleton* create_instance(void);
	static void destroy_instance(myIF3_singleton* obj);

public:
	virtual ::mytest::test_struct2 method1(const std::string& val1, int32_t &val2);
	virtual myIF1* method2(myIF1& val1);

};

namespace skeleton {
	class rpc_event_no_param
	{
	public:
		rpc_event_no_param(){}
		~rpc_event_no_param(){}

		void trigger(void) {
			trigger_event("mytest.rpc_event_no_param", NULL);
		}
	};

}

namespace skeleton {
	class rpc_event_test_with_param
	{
	public:
		rpc_event_test_with_param(){}
		~rpc_event_test_with_param(){}

		void trigger(test_struct2 &testdata);
	};

}

} // end of namespace mytest
#endif /* __CXX_ZAS_MWARE_RPC_MYTEST_H__ */
