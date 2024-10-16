#ifndef __CXX_SNAPSHOT_SERVICE_DEF_H__
#define __CXX_SNAPSHOT_SERVICE_DEF_H__

#include "std/zasbsc.h"
#include <memory>

namespace zas {
namespace vehicle_snapshot_service {

#define SNAPSHOT_WORKER_TAG	"vss_worker"
#define SNAPSHOT_SNAPSHOT_TAG	"vss_snapshot"

#define SNAPSHOT_SERVICE_TIMER_MIN_INTERVAL (10)
#define JUNCTION_KAFKA_SEND_MIN_INTERVAL (100)
#define VEHICLE_KAFKA_SEND_MIN_INTERVAL (100)
#define VEHICLE_MAXDISTANCE (50.0)

#define FUSION_TIMER_UPDATE_INTERVAL (10)
#define FUSION_UPDATE_INTERVAL (8)
#define FUSION_FRAME_INTERVAL (80)
#define JUNCTION_MAX_DISTANCE (200)
#define VEHICLE_IN_JUNCTION_MAX_DISTANCE (100)
#define DEG2RAD M_PI / 180

#define VIRTUAL_MAIN_TIME_INTERVAL (100)

}}	//zas::vehicle_snapshot_service


#define DEFINE_ZAS_EXTEND_TYPE(T)                         \
  using cr##T = T const &;                            \
  using sp##T = std::shared_ptr<T>;                   \
  using up##T = std::unique_ptr<T>;                   \
  using crsp_c##T = std::shared_ptr<T const> const &; \
  using sp_c##T = std::shared_ptr<T const>

enum service_msg_type
{
	service_msg_unknonw = 0,
	service_msg_forward,
	service_msg_request,
	service_msg_register,
	service_msg_heartbeat,
	service_msg_deregister,
	service_msg_reply,
};

struct server_header
{
	service_msg_type svc_type;
};

struct server_request_info
{
	uint32_t veh_cnt;
	size_t name_len;
	char buf[0];
};

struct server_reply_info
{
	int result;
};

#endif /* __CXX_SNAPSHOT_SERVICE_DEF_H__*/