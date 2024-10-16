

#ifndef __CXX_ZAS_UTILS_DATAPOOL_MSG_H__
#define __CXX_ZAS_UTILS_DATAPOOL_MSG_H__

#include "utils/evloop.h"

namespace zas {
namespace utils {

// definition of datapool class id
#define DATAPOOL_CLSID_CTRL	(1)


#define DP_CLIENT_CTRL_REQUEST		EVL_MAKE_PKGID(DATAPOOL_CLSID_CTRL, 1)
#define DP_SERVER_CTRL_REPLY		EVL_MAKE_PKGID(DATAPOOL_CLSID_CTRL, 2)
#define DP_SERVER_CTRL_NOTIFY		EVL_MAKE_PKGID(DATAPOOL_CLSID_CTRL, 3)

union element_validity_u
{
	uint32_t all;
	struct {
		uint32_t name				: 1;
		uint32_t is_global			: 1;
		uint32_t need_persistent	: 1;
		uint32_t notifier			: 1;
		uint32_t databuf			: 1;
	} m;
};

struct element_info
{
	element_info();
	element_validity_u validity;
	bool is_global;
	bool need_persistent;
	void* notify;
	void* owner;
	uint16_t datasz;	//datasz
	uint16_t name;			//elementname offset
	uint16_t databuf;		//databuf offset
	uint16_t bufsz;
	char buf[0];
};

// datapool package owner
enum dp_pkg_owner
{
	dp_pkg_owner_unknown = 0,
	dp_pkg_owner_datapool,
	dp_pkg_owner_element,
};

enum datapool_dpowner_action
{
	dp_dpowr_act_unknown = 0,
	dp_dpowr_act_create_element,
	dp_dpowr_act_get_element,
	dp_dpowr_act_remove_element,
	dp_dpowr_act_check_element,
};

enum datapool_eleowner_action
{
	dp_eleowr_act_unknown = 0,
	dp_eleowr_act_getdata,
	dp_eleowr_act_setdata,
	dp_eleowr_act_addlistener,
	dp_eleowr_act_rmlistener,
	dp_eleowr_act_notify
};

struct datapool_client_request
{
	dp_pkg_owner pkgowr;
	bool needreply;
	union {
		datapool_dpowner_action			dpowract;
		datapool_eleowner_action		eleowract;
	};
	element_info		element;
};
EVL_DEFINE_PACKAGE(datapool_client_request, DP_CLIENT_CTRL_REQUEST);

struct datapool_server_reply
{
	int					result;
	element_info		element;
};
EVL_DEFINE_PACKAGE(datapool_server_reply, DP_SERVER_CTRL_REPLY);

struct datapool_server_noitfy
{
	element_info		element;
};
EVL_DEFINE_PACKAGE(datapool_server_noitfy, DP_SERVER_CTRL_NOTIFY);

}} // end of namespace zas::utils

#endif /*  __CXX_ZAS_UTILS_EVLOOP_MSG_H__ */
/* EOF */
