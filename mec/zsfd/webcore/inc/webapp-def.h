#ifndef __CXX_ZAS_WEBAPP_DEF_H__
#define __CXX_ZAS_WEBAPP_DEF_H__

#include "webcore/webcore.h"

#include <string>
#include <curl/curl.h>

#include "std/list.h"
#include "std/smtptr.h"
#include "utils/avltree.h"

namespace zas {
namespace webcore {

#define WEB_SOCKET_MAX_COUNT_INIT (16)
#define WEB_TIMER_MIN_INTERVAL (10)
#define WEB_HTTP_REQUEST_MAX_REUSED_CNT	(100)
#define WEB_DISPATCH_TAG	"webapp_socket_dispatch"

class webapp_socket;
class wa_request_impl;
class wa_response;
class wa_response_factory;

class wa_response_proxy
{
public:
	wa_response_proxy(wa_response_factory* factory, webapp_socket* socket);
	~wa_response_proxy();

	int addref(void);
	int release(void);

	wa_response* get_reply(void);
	webapp_socket* get_webapp_socket(void);

private:
	wa_response_factory* _factory;
	wa_response* _reply;
	webapp_socket* _socket;
	int _refcnt;
};

typedef zas::smtptr<wa_response_proxy> wa_response_proxy_smtptr;

struct wa_request_item
{
	listnode_t 	ownerlist;
	zas::utils::avl_node_t seq_avlnode;
	// wa_request send sequence id
	uint64_t sequence_id;
	// wa_request sender
	wa_request_impl* sender;
	// webapp context of wa_request send;
	wa_response_proxy_smtptr prev_reply;
	
	static int seq_avl_compare(zas::utils::avl_node_t*, zas::utils::avl_node_t*);
};

struct http_request_item
{
	http_request_item();
	~http_request_item();
	void reset(void);
	
	listnode_t 		ownerlist;

	// curl http related objects
	CURL*			easy;
	curl_slist*		headers;

	std::string* 	reply_data;
	wa_request_impl*		sender;
	wa_response_proxy_smtptr prev_reply;
};

// wa_request info
struct webapp_request_socket_item
{
	listnode_t ownerlist;
	zas::utils::avl_node_t req_avlnode;
	// wa_request object
	wa_request_impl*	ztcp_req;

	//wa_request webapp socket object
	webapp_socket*	wa_socket;

	//wa_request send info(wa_request_item)
	listnode_t 		request_list;
	static int req_avl_compare(zas::utils::avl_node_t*,
		zas::utils::avl_node_t*);
};

// webapp backend socket info
struct webapp_backend_socket
{
	listnode_t ownerlist;
	zas::utils::avl_node_t wa_sock_avlnode;
	// webapp_socket object
	webapp_socket*	wa_socket;
	// wa_request object of wa_socket (webapp_request_socket_item)
	listnode_t req_socket_list;

	static int wa_sock_avl_compare(zas::utils::avl_node_t*, zas::utils::avl_node_t*);
};

enum msg_pattern
{
	msgptn_unknown = 0,
	msgptn_req_rep,
	msgptn_sub_pub,
};

enum msg_type
{
	msgtype_unknown = 0,
	msgtype_request,
	msgtype_reply,
	msgtype_sub,
	msgtype_pub,
};

}};	//namespace zas::webcore
#endif