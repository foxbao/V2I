#ifndef __CXX_OSM_PARSER1_EXTDATA_H__
#define __CXX_OSM_PARSER1_EXTDATA_H__

#include <stack>
#include "std/list.h"
#include "mapcore/mapcore.h"
#include "xml.h"
#include "odr-map.h"

namespace osm_parser1 {
using namespace std;
using namespace zas::utils;
using namespace zas::mapcore;
using namespace XERCES_CPP_NAMESPACE;

union ext_siglight_attr
{
	size_t attrs;
	struct {
		size_t lon_avail : 1;
		size_t lat_avail : 1;
		size_t zoffset_avail : 1;
	} a;
};

struct exd_siglight_info
{
	double heading;
};

struct exd_roadside_device_info
{
	exd_roadside_device_info() {
		memset(&attrs, 0, sizeof(attrs));
	}
	struct {
		uint32_t abs_hdg : 1;
		uint32_t pos_avail : 1;
		uint32_t hdg_avail : 1;
		uint32_t zoffset_avail : 1;
		uint32_t pitch_avail : 1;
		uint32_t roll_avail : 1;
	} attrs;
	uint32_t devtype;
	uint32_t roadid, junctionid;
};

struct exd_approach_info
{
	exd_approach_info();
	string uuid;
	uint32_t uniqueId;
	uint32_t junctionid;
};

enum exd_arrow_type
{
	arrow_type_unknown = 0,
	arrow_type_straight_ahead,
	arrow_type_turn_left,
	arrow_type_turn_right,
};

struct exd_arrow_info
{
	exd_arrow_info();
	listnode_t ownerlist;
	uint64_t uid;
	uint32_t type;
	int fillcolor;
	double ascending;
	double descending;
	vector<point3d> polygon;
};

struct exd_arrows
{
	exd_arrows();
	~exd_arrows();
	listnode_t arrow_list;
	double ascending;
	double descending;
};

struct exd_junconver_info
{
	exd_junconver_info();
	listnode_t ownerlist;
	uint32_t roadid, junctionid;
	uint32_t uuid;

	string id;
	string areaid;
	uint32_t valid;
	string shapeid;
	uint32_t majorclass;
	uint32_t subclass;
	uint32_t bcdirect;
	uint32_t transiting;
	uint32_t across;

	double ascending;
	double descending;

	vector<point3d> polygon;
};

struct exd_junconvers
{
	exd_junconvers();
	~exd_junconvers();
	listnode_t juncvr_list;
	double ascending;
	double descending;
};

struct exd_sigboards
{
	exd_sigboards();
	~exd_sigboards();
	listnode_t sigboard_list;
	double height;
};

struct exd_sigboard_info
{
	exd_sigboard_info();
	listnode_t ownerlist;
	uint64_t uid;
	double height;
	double width;
	double length;
	int shape_type;
	int kind;
	int major_type;
	int sub_type;
	vector<point3d> polygon;
};

class odr_extdata : public DefaultHandler
{
	friend class odr_map;
public:
	odr_extdata(odr_map& map);
	~odr_extdata();

	int execute(void);

protected:

	void startElement(
		const XMLCh* const uri,
		const XMLCh* const localname,
		const XMLCh* const qname,
		const Attributes& attrs);
	
    void endElement(
		const XMLCh* const uri,
		const XMLCh* const localname,
		const XMLCh* const qname);

	void characters(
		const XMLCh* const chars, const XMLSize_t length);

	void fatalError(const SAXParseException&);

private:
	void handle_xmlnode(shared_ptr<xmlnode> node);
	void handle_roadobject(shared_ptr<xmlnode> node);
	void handle_coordoffset(shared_ptr<xmlnode> node);
	shared_ptr<xmlnode> check_roadobj_match_with_map(shared_ptr<xmlnode>, size_t&);

	// approach parser
	void handle_approach(shared_ptr<xmlnode> node);
	shared_ptr<xmlnode> check_approach_match_with_map(shared_ptr<xmlnode>, size_t&);
	void update_approach_info(shared_ptr<xmlnode> node, exd_approach_info*);

	// arrow parser
	void handle_road_arrow(shared_ptr<xmlnode> node);
	void handle_arrow_property(shared_ptr<xmlnode>, exd_arrow_info*);
	void handle_arrow_polygon(shared_ptr<xmlnode>, exd_arrow_info*);
	void update_arrows_info(shared_ptr<xmlnode>);
	void update_arrow_object(odr_road*, odr_object*);

	// junconver parser
	void handle_junconver(shared_ptr<xmlnode> node);
	void handle_junconver_property(shared_ptr<xmlnode>, exd_junconver_info*);
	void handle_junconver_polygon(shared_ptr<xmlnode>, exd_junconver_info*);
	void update_junconver_info(shared_ptr<xmlnode>);
	void update_junconver_object(odr_road*, odr_object*);

	// signalboard parser
	void handle_sigboard(shared_ptr<xmlnode> node);
	void handle_sigboard_property(shared_ptr<xmlnode>, exd_sigboard_info*);
	int handle_sigboard_polygon(shared_ptr<xmlnode>, exd_sigboard_info*);
	void update_sigboards_info(shared_ptr<xmlnode>);
	void update_sigboard_object(odr_road*, odr_object*);
	hdmapcfg* update_sigboard_object_common_prepare(odr_object*, odr_road*);
	void update_sigboard_circle_object(odr_object*, odr_road*);
	void update_sigboard_rectangle_object(odr_object*, odr_road*);
	void update_sigboard_triangle_object(odr_object*, odr_road*, bool);

	// signal parser
	void update_siglight_info(size_t uid,
		shared_ptr<xmlnode>, shared_ptr<xmlnode>);
	void fix_siglight_info(odr_object*, shared_ptr<xmlnode>);
	void load_rsd_info(odr_object*, shared_ptr<xmlnode>);
	void finalize_rsd_info(odr_object*);
	void fix_new_siglight_info(odr_object* ro, shared_ptr<xmlnode> data);
	void create_signallight(shared_ptr<xmlnode> node, size_t uid);

	// road side device parser
	void update_road_side_device_info(size_t uid,
		shared_ptr<xmlnode> node, shared_ptr<xmlnode> rsd);
	int get_road_side_device_type(shared_ptr<xmlnode> node);

	bool attr_default(const string& str);

private:
	// xerces objects
	SAX2XMLReader* _reader;
	odr_map& _map;
	union {
		struct {
			uint32_t dataset_parsed : 1;
		} _f;
		uint32_t _flags;
	};
	stack<shared_ptr<xmlnode>> _stack;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_EXTDATA_H__
/* EOF */
