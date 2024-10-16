#ifndef __CXX_lbservice_H__
#define __CXX_lbservice_H__

#include "load-balance-def.h"

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "webcore/webapp.h"

namespace zas {
namespace load_balance {

using namespace  zas::webcore;

class load_balance_webapp_cb;
class forward_arbitrate;

class lbservice : public wa_response_factory
{
public:
	lbservice();
	virtual ~lbservice();
	int run(void);

	wa_response* create_instance(void);
	void destory_instance(wa_response *reply);

public:
	int oninit(void);
	int onexit(void);
	int on_request(void* data, size_t sz);

private:
	int init_forward_arbitrate(void);
	int init_service_mgr(void);

private:
	load_balance_webapp_cb* _webapp_cb;
	forward_arbitrate*	_forward_mgr;

};

}}	//zas::vehicle_load_balance

#endif /* __CXX_lbservice_H__*/