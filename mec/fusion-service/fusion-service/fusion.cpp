#include "fusion.h"
#include "proto/junction_package.pb.h"
#include "webcore/logger.h"
#include "webcore/webapp.h"
#include "service-worker.h"
#include "mapcore/sdmap.h"
#include "utils/uri.h"
#include "webcore/sysconfig.h"
#include "webcore/webapp.h"

#include <sys/time.h>

#include "interface/fuser.h"

namespace zas {
namespace fusion_service {

using namespace zas::utils;
using namespace zas::webcore;
using namespace zas::mapcore;
using namespace coop::v2x;
using namespace jos;

device_fusion::device_fusion()
{
}

device_fusion::~device_fusion()
{

}

int device_fusion::on_recv(wa_response *wa_rep, std::string &vid,
	void* data, size_t sz)
{
	assert(nullptr != data);
	junction_fusion_package junpkg;
	junpkg.ParseFromArray(data, sz);
	fusion_service_pkg fpkg;
	auto it = _fusers.find(vid);
	if (it == _fusers.end()) {
		std::cout << "create new fusion" << std::endl;
		_fusers[vid] = std::make_shared<Fuser>();
	}
	auto curfuser = _fusers[vid];
	fpkg.Clear();
	curfuser->fuse_frame(junpkg, fpkg);
	auto* fstgt = fpkg.mutable_fus_pkg();
	fstgt->set_junc_id(vid);
	std::string senddata;
	fpkg.SerializeToString(&senddata);
	wa_rep->response((void*)senddata.c_str(), senddata.length());
	return 0;
}


}}	//zas::vehicle_snapshot_service




