#include <stdio.h>
#include <set>

#include "utils/timer.h"
#include "mapcore/mapcore.h"
#include "mapcore/sdmap.h"
#include "mapcore/hdmap.h"

using namespace std;
using namespace zas::mapcore;

static const char* way_type_tbl[] =
{
	// main
	"motorway", "trunk", "primary", "secondary",
	"tertiary", "unclassified", "residential",

	// link roads
	"motorway_link", "trunk_link", "primary_link", "secondary_link",
	"tertiary_link", 

	// Special road types
	"living_street", "service", "pedestrian",
	"track", "bus_guideway", "escape", "raceway",
	"road", "busway",

	// paths
	"footway", "bridleway", "steps", "corridor", "path",
	"cycleway",

	// Lifecycle
	"planned", "proposed", "construction",

	// Other highway features
	"bus_stop", "crossing", "elevator", "emergency_bay",
	"emergency_access_point", "give_way", "milestone",
	"mini_roundabout", "motorway_junction", "passing_place",
	"platform", "rest_area", "speed_camera", "street_lamp",
	"services", "stop", "traffic_mirror", "traffic_signals",
	"trailhead", "turning_circle", "turning_loop",
	"toll_gantry",

	// unknown - discovered in OSM data
	"cycleway",
};

int sdmap_test(void)
{
	sdmap map;
	int ret = map.load_fromfile("/home/jimview/map/target/sh/fullmap");
	assert(ret == 0);

	double lat, lon, dist;

	lon = 121.576976;
	lat = 31.252883;

	const int round = 8;

for (;;) {

	scanf("%lf,%lf", &lon, &lat);
	if (lon == 0. || lat == 0.) {
		break;
	}
	sdmap_nearest_node nodes[round];
	if (map.search_nearest_nodes(lat, lon, round, nodes)) {
		printf("node error\n");
		return 1;
	}
	for (int i = 0; i < round; ++i) {
		auto* node = nodes[i].node;
		printf("node: <%f, %f>, dist: %.3f", node->lat(), node->lon(), nodes[i].distance);
		vector<const char*>names;
		if (node->get_intersection_roads_name(names)) {
			for (auto n : names) printf(" %s", n);
		}
		printf("\n");
	}

	sdmap_nearest_link links[round];
	ret = map.search_nearest_links(lat, lon, links, round, round);
	if (ret) {
		printf("error\n");
		return 2;
	}

	for (int i = 0; i < round; ++i) {

		sdmap_nearest_link* l = &links[i];
		auto* n1 = l->link->get_node1();
		auto* n2 = l->link->get_node2();

		printf("link: %s, (%lu<%f, %f>, %lu<%f, %f>) dist: %f, projection<lat,lon>: <%f, %f>\n",
			l->link->get_way()->get_name(), n1->getuid(), n1->lat(), n1->lon(),
			n2->getuid(), n2->lat(), n2->lon(), (l->distance > 10000) ? -1 : l->distance,
			l->projection.lat(), l->projection.lon());
		printf("way type: %s\n", way_type_tbl[l->link->get_way()->get_type()]);
	}
}
	return 0;
}

static void hdmap_list_all_junctions(const rendermap& rmap, const sdmap& smap)
{
	rendermap_info info;
	rmap.get_mapinfo(info);
	for (int i = 0; i < info.junction_count; ++i) {
		auto junc = rmap.get_junction_by_index(i);
		assert(nullptr != junc);
		double x, y, r;
		junc->center_point(x, y, r);

		printf("<%d> junction id: %u, center<%.4f, %.4f>, radius: %.4f  ",
			i, junc->getid(), x, y, r);

		vector<const char*> names;
		junc->extract_desc(&smap, &rmap, names);
		for (auto name : names) {
			printf("%s ", name);
		}
		printf("\n");
	}
}

static void hdmap_test_junction(const rendermap& rmap, const tilecache& tc)
{
	// test proj
	double lon, lat;
	// 此坐标为博园路-墨玉南路路口，高德地图上拾取点，火星坐标系
	gcj2wgs_exact(31.28027, 121.164845, &lat, &lon);
	printf("from <31.28027, 121.164845> to <%f, %f>\n", lat, lon);
	zas::mapcore::proj p(GREF_WGS84, rmap.get_georef());
	point3d prjret = p.transform(llh(lon, lat));
	prjret -= tc.get_mapoffset();

	sdmap smap;
	printf("loading sdmap (shanghai) ...");
	fflush(nullptr);
	int ret = smap.load_fromfile("/home/jimview/map/target/sh/fullmap");
	assert(ret == 0);
	printf("[success]\n");

	vector<rendermap_nearest_junctions> junc;
	ret = rmap.find_junctions(prjret.xyz.x, prjret.xyz.y, 2, junc);
	assert(ret > 0);
	for (auto& item : junc) {
		double x, y, r;
		item.junction->center_point(x, y, r);
		printf("id: %u <%.2f, %.2f, %.2f> %.2fm ", item.junction->getid(), x, y, r, item.distance);

		vector<const char*> names;
		item.junction->extract_desc(&smap, &rmap, names);
		printf("junction name: ");
		for (auto name : names) {
			printf("%s ", name);
		}
		printf("\n");
	}
	prjret += tc.get_mapoffset();

	printf("\n");
	hdmap_list_all_junctions(rmap, smap);
}

static void hdmap_rendermap_test(void)
{
	rendermap rmap;
	int ret = rmap.load_fromfile("/home/jimview/map/target/hdmap/rendermap");
	assert(ret == 0);

	rendermap_info minfo;
	rmap.get_mapinfo(minfo);
	printf("rows: %d, cols: %d, [%.2f, %.2f]-[%.2f, %.2f]\n", minfo.rows, minfo.cols,
		minfo.xmin, minfo.ymin, minfo.xmax, minfo.ymax);
	printf("georef: %s\n\n", rmap.get_georef());
	
	string buf;
	ret = rmap.extract_blocks({}, {}, buf);
	assert(ret == 0);

	set<uint32_t> output;
	rmap.get_block_dependencies({2}, output);
	printf("size = %lu\n", output.size());
	for (auto& v : output) printf("%u, ", v);
	printf("\n");

	auto* t = new tilecache();
	ret = t->add_tile(buf);
	assert(ret == 0);
	delete t;

	tilecache tc;
	ret = rmap.extract_blocks({1}, {}, buf);
	assert(ret == 0);
	ret = tc.bind_map(buf);
	assert(ret < 0);

	ret = rmap.extract_blocks({}, {}, buf);
	assert(ret == 0);

	ret = tc.bind_map(buf);
	assert(ret == 0);
	ret = tc.add_tile(buf);
	assert(ret >= 0);

	ret = rmap.extract_blocks({1}, {}, buf);
	assert(ret == 0);

	ret = tc.add_tile(buf);
	assert(ret >= 0);

	rmap.extract_blocks({2}, {1}, buf);
	ret = tc.add_tile(buf);
	assert(ret >= 0);
	printf("buffer size is %lu\n", buf.length());

	ret = rmap.extract_blocks({113}, {}, buf);
	assert(ret == 0);
	ret = tc.add_tile(buf);
	assert(ret >= 0);
	tc.get_mesh(113);

	tc.get_mesh(2);
	tc.get_mesh(1);

	tc.release_mesh(2);

	set<uint32_t> blkids;
	tc.query_unloaded_blocks(
		{{-1294, -4565}, {-1274, -4565}, {-1274, -4545}, {-1294, -4545}},
		blkids);
	for (auto v : blkids) printf("unloaded blockid: %u\n", v);
	blkids.clear();
	tc.query_loaded_blocks(
		{{-1294, -4665}, {-1274, -4665}, {-1274, -4645}, {-1294, -4645}},
		blkids);
	for (auto v : blkids) printf("loaded blockid: %u\n", v);

	tc.filter_loaded_blocks(output, true);
	printf("remaining: ");
	for (auto v : output) printf("%d, ", v); printf("\n");

	tc.release_blocks(output);
	tc.unlock_blocks(output);
	
	tc.release_blocks({2});
	tc.release_blocks({113});

	hdmap_test_junction(rmap, tc);

	// test extract_roadobjects
	set<const rendermap_roadobject*> roret;
	rmap.extract_roadobjects(85, {hdrm_robjtype_signal_light}, roret);
	for (auto ro : roret) {
		printf("signallight@85: uid = %lu\n", ro->as_siglight()->getuid());
	}
}

static void hdmap_test(void)
{
	hdmap hmap;
	int ret = hmap.load_fromfile("/home/coder/map/target/hdmap/hdmap");
	assert(ret == 0);
	hdmap_info minfo;
	hmap.get_mapinfo(minfo);
	printf("rows: %d, cols: %d, road %d, junction %d, "
		"[%.2f, %.2f]-[%.2f, %.2f]\n",
		minfo.rows, minfo.cols, minfo.road_count, minfo.junction_count,
		minfo.xmin, minfo.ymin, minfo.xmax, minfo.ymax);
	printf("georef: %s\n\n", hmap.get_georef());
	auto projc = new zas::mapcore::proj(GREF_WGS84, hmap.get_georef());
	double x, y;
	hmap.get_offset(x, y);
	zas::mapcore::point3d hdmoffset(x,y);
	point3d prjret = projc->transform(llh(121.16281801, 31.28465897));
	prjret -= hdmoffset;
	//31.28465897, 121.16281801
	vector<hdmap_nearest_laneseg> set;
	hmap.find_laneseg(prjret.xyz.x, prjret.xyz.y, 2, set);
	for (auto n : set) {
		point3d x, y;
		n.laneseg->get_endpoint(x,y);
		printf("find distance %lf, start (%lf, %lf), end (%lf, %lf)\n", n.distance, x.xyz.x, x.xyz.y, y.xyz.x, y.xyz.y);
	}
}

static void test_transform(void)
{
	// wgsLat, wgsLng, gcjLat, gcjLng
	double v[] = {31.1774276, 121.5272106, 31.17530398364597, 121.531541859215};
	double lon, lat;

	printf("ref : %f, %f, %f, %f\n\n", v[0], v[1], v[2], v[3]);

	wgs2gcj(v[0], v[1], &lat, &lon);
	printf("w2g : %f, %f, %f, %f\n", v[0], v[1], lat, lon);

	gcj2wgs(v[2], v[3], &lat, &lon);
	printf("g2w : %f, %f, %f, %f\n", lat, lon, v[2], v[3]);

	gcj2wgs_exact(v[2], v[3], &lat, &lon);
	printf("g2we: %f, %f, %f, %f\n\n", lat, lon, v[2], v[3]);

	coord_t co(v[0], v[1]);
	co.wgs84_to_gcj02();
	printf("w2g : %f, %f, %f, %f\n", v[0], v[1], co.lat(), co.lon());

	co.set(v[2], v[3]);
	co.gcj02_to_wgs84();
	printf("g2w : %f, %f, %f, %f\n", co.lat(), co.lon(), v[2], v[3]);
}

void test_point(void) {
	hdmap hmap;
	int ret = hmap.load_fromfile("/home/coder/map/target/hdmap/hdmap");
	assert(ret == 0);
	hmap.generate_lanesect_transition("/home/coder/workspace/map/lanesec.json");
	// uint64_t lane_index = 1028;
	// std::vector<Eigen::Vector3d> p3ds = hmap.get_curve(lane_index);
	// printf("point size is %ld.\n", p3ds.size());
	// for (size_t index = 0; index < p3ds.size(); index++) {
	// 	printf("point_%ld x: %f,y: %f,z: %f\n", index, p3ds[index][0], p3ds[index][1], p3ds[index][2]);
	// }

	// printf("--------------------------------------------------------\n");

	// std::vector<vector<Eigen::Vector3d>> p3d_all = hmap.get_central_curves_enu();
	// printf("point size is %ld.\n", p3d_all.size());
	// for (size_t index = 0; index < p3d_all.size(); index++) {
	// 	printf("lane_%ld size is %ld.\n", index, p3d_all[index].size());
	// }
	// double x1 = 194.5840823;
	// double y1 = 197.1982812;
	double x2 = 292.3;
	double y2 = 218.68;
	std::vector<Eigen::Vector3d> trajectory;
	Eigen::Vector3d p1;
	p1[0] = x2;
	p1[1] = y2;
	// Eigen::Vector3d p2;
	// p2[0] = x2;
	// p2[1] = y2;
	trajectory.push_back(p1);
	// trajectory.push_back(p2);
	std::vector<uint64_t> lanes = hmap.get_lanes_near_pt_enu(trajectory, 5);
	printf("lane size is %ld.\n", lanes.size());
	for (size_t i = 0; i < lanes.size(); i++) {
		printf("index : %ld, lane index : %ld.\n", i, lanes[i]);
	}
	Eigen::MatrixXd laneMatri = hmap.CalculateTransitionalProbability(lanes);
	for (size_t i = 0; i < 2; i++) {
		for (size_t j = 0; j < 2; j++) {
			printf(" pos%ld_%ld is %f.\n", i, j, laneMatri(i,j));
		}
	}
	Eigen::Vector3d p;
	double distance = hmap.get_distance_pt_closest_central_line_enu(p1, p);
	printf("x : %f, y : %f, distance : %f.\n", p[0], p[1], distance);
	// for (size_t i = 0; i < lanes.size(); i++) {
	// 	Eigen::Vector3d p3d;
	// 	// if (i == 0) {
	// 	// 	p3d[0] = x1;
	// 	// 	p3d[1] = y1;
	// 	// } else {
	// 		p3d[0] = x2;
	// 		p3d[1] = y2;
	// 	// }
	// 	Eigen::Vector3d posi3d;
	// 	uint64_t lane_index = lanes[i];
	// 	double distance = hmap.get_distance_pt_curve_enu(p3d, lane_index, posi3d);
	// 	printf("lane%ld distance is %f, posi x=%f y=%f.\n", lane_index, distance, posi3d[0], posi3d[1]);
	// }

}

//#define TEST_SDMAP
//#define TEST_RDMAP
#define TEST_HDMAP

int main(int argc, char* argv[])
{
// #if defined(TEST_SDMAP)
// 	sdmap_test();
// #elif defined(TEST_RDMAP)
// 	hdmap_rendermap_test();
// #elif defined(TEST_HDMAP) 
// 	hdmap_test();
// #endif
	// test_transform();
	test_point();
	return 0;
}
