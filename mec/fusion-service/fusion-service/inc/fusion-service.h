#ifndef __CXX_FUSION_SERVICE_H__
#define __CXX_FUSION_SERVICE_H__

#include <string.h>

#include "std/list.h"
#include "utils/avltree.h"
#include "webcore/webapp.h"

namespace zas {
namespace fusion_service {

using namespace  zas::webcore;

class fusion_webapp_cb;
class device_fusion;

class device_fusion_service : public wa_response_factory
{
public:
	device_fusion_service();
	virtual ~device_fusion_service();
	int run(void);

public:
	wa_response* create_instance(void);
	void destory_instance(wa_response *reply);

public:
	int oninit(void);
	int onexit(void);
	int on_request(wa_response *wa_rep, void* data, size_t sz);

private:
	union {
		struct {
			uint32_t init_fusion : 1;
		} _f;
		uint32_t _flags;
	};
	

private:
	fusion_webapp_cb* _webapp_cb;
	device_fusion* _device_fusion;
};

}}	//zas::fusion_service


#endif /* __CXX_FUSION_SERVICE_H__*/