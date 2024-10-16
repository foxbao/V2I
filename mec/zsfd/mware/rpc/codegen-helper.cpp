/** @file codegen-helper.h
 * implementation of rpcc code generator helper classes
 */

#include "mware/rpc/codegen-helper.h"
#include "proto/bsctype_array.pb.h"

namespace zas {
namespace mware {
namespace rpc {

pbf_wrapper_object::pbf_wrapper_object()
: _refcnt(0)
{
}

pbf_wrapper_object::~pbf_wrapper_object()
{
}

google::protobuf::Message* pbf_wrapper_object::get_pbfmessage(void)
{
	return NULL;
}

int pbf_wrapper_object::addref(void)
{
	return __sync_add_and_fetch(&_refcnt, 1);
}

int pbf_wrapper_object::release(void)
{
	int cnt = __sync_sub_and_fetch(&_refcnt, 1);
	if (cnt <= 0) delete this;
	return cnt;
}

#define basic_type_arrary_implementation(clazz_array, bsctype, pbftype)	\
clazz_array::clazz_array(int maxsz)	\
: _pbobj(new ::zrpc_pbf::basictype_array())	\
, _max_size(maxsz)	\
{	\
}	\
\
clazz_array::~clazz_array()	\
{	\
	if (_pbobj) {	\
		delete _pbobj;	\
		_pbobj = nullptr;	\
	}	\
}	\
\
const clazz_array& clazz_array::operator=(const clazz_array& val)	\
{	\
	assert(nullptr != _pbobj);	\
	if (_pbobj == val._pbobj) {	\
		return *this;	\
	}	\
	int sz = val.size();	\
	if (_max_size > 0 && sz > _max_size) {	\
		sz = _max_size;	\
	}	\
	clear();	\
	for (int i = 0; i < sz; ++i) {	\
		append(val.get(i));	\
	}	\
	return *this;	\
}	\
\
bsctype& clazz_array::operator[](int index)	\
{	\
	return *_pbobj->mutable_##pbftype##_array()->Mutable(index);	\
}	\
\
void clazz_array::clear(void)	\
{	\
	_pbobj->clear_##pbftype##_array();	\
}	\
\
int clazz_array::size(void) const	\
{	\
	int ret = _pbobj->pbftype##_array_size();	\
	if (_max_size > 0 && _max_size < ret) {	\
		ret = _max_size;	\
	}	\
	return ret;	\
}	\
\
bsctype clazz_array::get(int index) const	\
{	\
	return _pbobj->pbftype##_array(index);	\
}	\
\
bsctype clazz_array::append(bsctype val)	\
{	\
	_pbobj->add_##pbftype##_array(val);	\
	return get(size() - 1);	\
}

basic_type_arrary_implementation(bool_array, bool, boolean);
basic_type_arrary_implementation(int32_array, int32_t, int32);
basic_type_arrary_implementation(int64_array, int64_t, int64);
basic_type_arrary_implementation(uint32_array, uint32_t, uint32);
basic_type_arrary_implementation(uint64_array, uint64_t, uint64);
basic_type_arrary_implementation(double_array, double, double);
basic_type_arrary_implementation(bytes_array, std::string, bytes);
basic_type_arrary_implementation(string_array, std::string, string);
basic_type_arrary_implementation(bytebuff_array, std::string, bytes);


skeleton_base::skeleton_base(const char* servicename, const char* class_name)
{
	rpchost host = rpcmgr::inst()->get_service(servicename, nullptr);
	host->add_instid(class_name, this);
}
skeleton_base::~skeleton_base()
{
}

bytebuff::bytebuff()
{
	
}

bytebuff::bytebuff(const bytebuff &bf)
{
	this->assign(bf.c_str(), bf.length());
}

bytebuff::bytebuff(const std::string &bf)
{
	this->assign(bf.c_str(), bf.length());
}

bytebuff::~bytebuff()
{

}

const char* bytebuff::getbuff(void)
{
	return  ::std::string::c_str();
}

size_t bytebuff::getlength(void)
{
	return  ::std::string::length();
}

bytebuff& bytebuff::operator=(const bytebuff& bf)
{
	this->assign(bf.c_str(), bf.length());
	return *this;
}

bytebuff& bytebuff::operator=(const std::string& ct)
{
	this->assign(ct.c_str(), ct.length());
	return *this;
}

template<typename T>
enum_array<T>::enum_array(int maxsz)
: _pbobj(new ::zrpc_pbf::basictype_array())
, _max_size(maxsz)	
{
}

template<typename T>
enum_array<T>::~enum_array() {
	if (_pbobj) {
		delete _pbobj;
		_pbobj = nullptr;
	}
}

template<typename T>
const enum_array<T>& enum_array<T>::operator=(const enum_array<T>& val) {
	assert(nullptr != _pbobj);
	if (_pbobj == val._pbobj) {
		return *this;
	}
	int sz = val.size();
	if (_max_size > 0 && sz > _max_size) {
		sz = _max_size;
	}
	clear();
	for (int i = 0; i < sz; ++i) {
		append(val.get(i));
	}
	return *this;
}

template<typename T>
T& enum_array<T>::operator[](int index) {
	return *((T*)_pbobj->mutable_enum_array()->Mutable(index));
}

template<typename T>
void enum_array<T>::clear(void) {
	_pbobj->clear_enum_array();
}

template<typename T>
int enum_array<T>::size(void) const {
	int ret = _pbobj->enum_array_size();
	if (_max_size > 0 && _max_size < ret) {
		ret = _max_size;
	}
	return ret;
}

template<typename T>
T enum_array<T>::get(int index) const {
	return _pbobj->enum_array(index);
}

template<typename T>
T enum_array<T>::append(T val){
	_pbobj->add_enum_array((int32_t)val);
	return get(size() - 1);
}

template<typename T>
interface_array<T>::interface_array(int maxsz)
: _pbobj(new ::zrpc_pbf::basictype_array())
, _max_size(maxsz)	
{
}

template<typename T>
interface_array<T>::~interface_array() {
	if (_pbobj) {
		delete _pbobj;
		_pbobj = nullptr;
	}
}

template<typename T>
const interface_array<T>& interface_array<T>::operator=(const interface_array<T>& val) {
	assert(nullptr != _pbobj);
	if (_pbobj == val._pbobj) {
		return *this;
	}
	int sz = val.size();
	if (_max_size > 0 && sz > _max_size) {
		sz = _max_size;
	}
	clear();
	for (int i = 0; i < sz; ++i) {
		append(val.get(i));
	}
	return *this;
}

template<typename T>
T& interface_array<T>::operator[](int index) {
	return *((T*)_pbobj->mutable_interface_array()->Mutable(index));
}

template<typename T>
void interface_array<T>::clear(void) {
	_pbobj->clear_interface_array();
}

template<typename T>
int interface_array<T>::size(void) const {
	int ret = _pbobj->interface_array_size();
	if (_max_size > 0 && _max_size < ret) {
		ret = _max_size;
	}
	return ret;
}

template<typename T>
T interface_array<T>::get(int index) const {
	return *((T*)_pbobj->interface_array(index));
}

template<typename T>
T interface_array<T>::append(T val){
	_pbobj->add_interface_array((int64_t)&val);
	return get(size() - 1);
}


}}} // end of namespace zas::mware::rpc
/* EOF */
