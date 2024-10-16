#ifndef __CXX_CLIENT_MGR_H__
#define __CXX_CLIENT_MGR_H__

#include <string.h>
#include "load-balance-def.h"
#include "utils/timer.h"
#include "utils/uri.h"
#include "forward.h"

namespace zas {
namespace load_balance {

using namespace zas::utils;

struct client_item
{
	avl_node_t avlnode;
	std::string identity;
	static int client_identity_compare(avl_node_t*, avl_node_t*);
};

class client_mgr
{
public:
	client_mgr();
	virtual ~client_mgr();
	static client_mgr* inst();
	int init(void);

public:
	int add_client(std::string &identity);
	int remove_client(std::string &identity);
	int check_identity(std::string &identity);
	const char* get_distribute_ipaddr(void);
	const char* get_distribute_port(void);

private:
	client_item* find_client(std::string &identity);
	int remove_all_client(void);

private:
	avl_node_t* _client_tree;
	void* _context;
	webapp_socket* _distribute;
	std::string _ipaddr;
	std::string _port;
};

}}	//zas::load_balance

#endif /* __CXX_CLIENT_MGR_H__*/