#ifndef __CXX_SNAPSHOT_KAFKA_CONSUMER_H__
#define __CXX_SNAPSHOT_KAFKA_CONSUMER_H__

#include <string.h>
#include <vector>
#include <map>
#include <set>

#include "snapshot-service-def.h"
#include "std/list.h"
#include "utils/avltree.h"
#include "utils/timer.h"
#include "utils/thread.h"
#include "utils/mutex.h"
#include "utils/uri.h"


namespace zas {
namespace vehicle_snapshot_service {

using namespace zas::utils;

struct light_info {
	int id;
	uint64_t timestamp;
	int state;
	int enttime;
};

DEFINE_ZAS_EXTEND_TYPE(light_info);

struct junc_lights
{
	bool update;
	std::map<int, splight_info> lights;
};
DEFINE_ZAS_EXTEND_TYPE(junc_lights);

class vss_kafka_consumer : public thread
{
public:
	vss_kafka_consumer();
	virtual ~vss_kafka_consumer();
	int run(void);
	int consumer_stop(void);

private:
	int load_consumerkafka_config(void);

private:
	std::string _brokers;
	std::string _group_id;
	std::vector<std::string> _topics;
};

class vss_light_mgr
{
public:
	vss_light_mgr();
	virtual ~vss_light_mgr();

	static vss_light_mgr* inst();

	int add_light(int junc_id, int light_id);
	int remove_light(int junc_id, int light_id);
	int get_light_state(int junc_id, std::map<int, int> *lights);
	int update_light_state(const char *light);
	
private:
	std::map<int, splight_info>	_lights;
	std::map<int, std::set<int>>	_light_juncs;
	std::map<int, spjunc_lights>	_junc_lights;
	mutex	_light_mut;
};

}}	//zas::vehicle_snapshot_service

#endif /* __CXX_SNAPSHOT_KAFKA_TO_CENTER_H__*/