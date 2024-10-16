#ifndef __CXX_FUSION_SERVICE_FUSION_H__
#define __CXX_FUSION_SERVICE_FUSION_H__

#include <string>
#include "fusion-service-def.h"
#include "interface/fuser.h"
#include "map"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/timer.h"
#include "utils/uri.h"

namespace jos {
class junction_package;
};

namespace zas {
namespace webcore {
class wa_response;
}
};  // namespace zas

namespace zas {
namespace fusion_service {

using namespace zas::utils;
using namespace coop::v2x;

class device_fusion {
 public:
  device_fusion();
  virtual ~device_fusion();
  int on_recv(zas::webcore::wa_response *wa_rep, std::string &vincode,
              void *data, size_t sz);

 private:
  std::map<std::string, spFuser> _fusers;
};

}  // namespace fusion_service
}  // namespace zas

#endif /* __CXX_FUSION_SERVICE_FUSION_H__*/