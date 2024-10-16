/** @file evlmsg.h
 * definition of evloop message and relative things
 */

#ifndef __CXX_ZAS_UTILS_EVLOOP_MSG_H__
#define __CXX_ZAS_UTILS_EVLOOP_MSG_H__

#include <sys/time.h>
#include "utils/evloop.h"

namespace zas {
namespace utils {

#define PKG_HEADER_MAGIC	(0xDEADBEEF)
#define PKG_FOOTER_MAGIC	(0xBEEFDEAD)

// definition of class id
#define EVL_CLSID_BSCCTRL	(0)
#define EVL_CLSID_SYSSVC	(1)

// definition of basic control package id
// BCTL stands for Basic Control

/**
  update the client info
  */
#define EVL_BCTL_CLIENT_INFO		EVL_MAKE_PKGID(EVL_CLSID_BSCCTRL, 1)
#define EVL_BCTL_CLIENT_INFO_ACK	EVL_MAKE_PKGID(EVL_CLSID_BSCCTRL, 2)
#define EVL_BCTL_RETRIVE_CLIENT		EVL_MAKE_PKGID(EVL_CLSID_BSCCTRL, 3)
#define EVL_BCTL_RETRIVE_CLIENT_ACK	EVL_MAKE_PKGID(EVL_CLSID_BSCCTRL, 4)

/**
 * system services related
 */
#define EVL_SSVC_LOG_SUBMISSION		EVL_MAKE_PKGID(EVL_CLSID_SYSSVC, 1)

#define CLINFO_TYPE_UNKNOWN			(0)
#define CLINFO_TYPE_SYSSVR			(1)
#define CLINFO_TYPE_CLIENT			(2)

union clinfo_validity_u
{
	uint32_t all;
	struct {
		uint32_t client_name		: 1;
		uint32_t client_pid			: 1;
		uint32_t client_ipv4addr	: 1;
		uint32_t client_port		: 1;
		uint32_t client_uuid		: 1;
		uint32_t instance_name		: 1;
		uint32_t flags				: 1;
	} m;
};

struct evl_bctl_client_info
{
	evl_bctl_client_info();
	clinfo_validity_u validity;
	guid_t client_uuid;
	uint32_t pid;
	uint32_t ipv4;
	uint32_t port;
	struct {
		uint32_t type				: 2;
		uint32_t service_container	: 1;
		uint32_t service_shared 	: 1;
	} flags;
	// indicate if we need a ack for this message
	// typically we need an ack when connecting syssvr
	// in order to update local syssvr client info
	// while we do not need an ack when connecting to
	// a peer since the peer info has been updated earlier
	bool need_reply;

	// name table
	uint16_t client_name;	// client name
	uint16_t inst_name;		// instance name

	uint16_t bufsz;
	char buf[0];
};

EVL_DEFINE_PACKAGE(evl_bctl_client_info, EVL_BCTL_CLIENT_INFO);

struct evl_bctl_client_info_ack
{
	int status;
	evl_bctl_client_info info;
};
EVL_DEFINE_PACKAGE(evl_bctl_client_info_ack, EVL_BCTL_CLIENT_INFO_ACK);

struct evl_bctl_retrive_client
{
	uint16_t inst_name;
	char client_name[0];
};
EVL_DEFINE_PACKAGE(evl_bctl_retrive_client, EVL_BCTL_RETRIVE_CLIENT);

struct evl_bctl_retrive_client_ack
{
	int status;
	evl_bctl_client_info info;
};
EVL_DEFINE_PACKAGE(evl_bctl_retrive_client_ack, EVL_BCTL_RETRIVE_CLIENT_ACK);

struct evl_ssvc_log_submission
{
	uint8_t level;
	// the timestamp of the log content
	timeval timestamp;
	// the length (w/o \0) of tag
	uint8_t tag_length;
	// the length (w/o \0) of content
	uint16_t content_length;
	// contents
	uint8_t data[0];
} PACKED;

EVL_DEFINE_PACKAGE(evl_ssvc_log_submission, EVL_SSVC_LOG_SUBMISSION);

}} // end of namespace zas::utils

#endif /*  __CXX_ZAS_UTILS_EVLOOP_MSG_H__ */
/* EOF */
