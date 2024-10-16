/** @file codegen-helper.h
 * definition of rpcc code generator helper classes
 */

#ifndef __CXX_ZAS_MWARE_RPC_CODEGEN_HELPER_H__
#define __CXX_ZAS_MWARE_RPC_CODEGEN_HELPER_H__

#include <string>
#include "std/smtptr.h"
#include "mware/mware.h"
#include "mware/rpc/rpcerror.h"

namespace google {
namespace protobuf {
	class Message;
}}

namespace zrpc_pbf {
	class basictype_array;
}

namespace zas {
namespace mware {
namespace rpc {

#define _ANCESTOR(anc, member)	\
	((anc*)(((size_t)this) - ((size_t)(&((anc*)0)->member))))

#define throw_rpc_error_array_exceed_boundary() do {\
	throw new zas::mware::rpc::rpc_error(	\
		__FILE__, __LINE__,	\
		errcat_proxy, err_scenario_others,	\
		errid_array_exceed_boundary);	\
	} while (0)

#define throw_rpc_error(catid, sid, errid) do { \
	throw new zas::mware::rpc::rpc_error(	\
		__FILE__, __LINE__,	\
		catid, sid, errid);	\
	} while (0)

class MWARE_EXPORT pbf_wrapper_object
{
public:
	pbf_wrapper_object();
	virtual ~pbf_wrapper_object();

	/**
	  Get the embedded protobuffer object
	  @return google::protobuf::Message
	 */
	virtual google::protobuf::Message* get_pbfmessage(void);

	int addref(void);
	int release(void);

private:
	int _refcnt;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(pbf_wrapper_object);
};

class MWARE_EXPORT bytebuff : public std::string
{
	public:
	bytebuff();
	bytebuff(const bytebuff &str);
	bytebuff(const std::string &str);
	~bytebuff();

	const char* getbuff(void);
	size_t getlength(void);
	bytebuff& operator=(const bytebuff& ct);
	bytebuff& operator=(const std::string& ct);
	
};
// definition of the smart pointer
typedef zas::smtptr<pbf_wrapper_object> pbf_wrapper;

#define basic_type_array_definition(clazz_array, bsctype)	\
	class MWARE_EXPORT clazz_array	\
	{	\
	public:	\
		clazz_array(int maxsz = -1);	\
		virtual ~clazz_array();	\
		\
		const clazz_array& operator=(const clazz_array& val);	\
		bsctype& operator[](int index);	\
		\
		virtual void clear(void);	\
		virtual int size(void) const;	\
		int count(void) const { return size(); }	\
		virtual bsctype get(int index) const;	\
		virtual bsctype append(bsctype val);	\
		\
	private:	\
		::zrpc_pbf::basictype_array* _pbobj;	\
		int _max_size;	\
	}

basic_type_array_definition(bool_array, bool);
basic_type_array_definition(int32_array, int32_t);
basic_type_array_definition(int64_array, int64_t);
basic_type_array_definition(uint32_array, uint32_t);
basic_type_array_definition(uint64_array, uint64_t);
basic_type_array_definition(double_array, double);
basic_type_array_definition(bytes_array, std::string);
basic_type_array_definition(string_array, std::string);
basic_type_array_definition(bytebuff_array, std::string);


class MWARE_EXPORT skeleton_base
{
public:
	skeleton_base(const char* servicename, const char* class_name);
	~skeleton_base();
};

template <typename T> class interface_array
{
public:
	interface_array(int maxsz = -1);

	virtual ~interface_array();

	const interface_array& operator=(const interface_array& val);

	T& operator[](int index);
	virtual void clear(void);

	virtual int size(void) const;

	virtual T get(int index) const;
	virtual T append(T val);

private:
	::zrpc_pbf::basictype_array* _pbobj;
	int _max_size;

};

template <typename T> class enum_array
{
public:
	enum_array(int maxsz = -1);

	virtual ~enum_array();

	const enum_array& operator=(const enum_array& val);

	T& operator[](int index);
	virtual void clear(void);

	virtual int size(void) const;

	virtual T get(int index) const;
	virtual T append(T val);

private:
	::zrpc_pbf::basictype_array* _pbobj;
	int _max_size;

};

}}} // end of namespace zas::mware::rpc

#include "mware/rpc/rpcmgr.h"
#endif /* __CXX_ZAS_MWARE_RPC_CODEGEN_HELPER_H__ */
/* EOF */
