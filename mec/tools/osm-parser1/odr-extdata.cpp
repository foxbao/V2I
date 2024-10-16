#include "utils/dir.h"
#include "odr-extdata.h"
#include "odr-loader.h"

namespace osm_parser1 {
using namespace std;
using namespace XERCES_CPP_NAMESPACE;

#define odr_extdata_check(expr) \
do {	\
	if (expr) break;	\
	fprintf(stderr, "odr_extdata: error at %s <ln:%u>: \"%s\"," \
		" abort!\n", __FILE__, __LINE__, #expr);	\
	exit(115);	\
} while (0)

odr_extdata::odr_extdata(odr_map& map)
: _map(map), _reader(nullptr)
, _flags(0)
{
	try {
		// initialize the xerces
		XMLPlatformUtils::Initialize();
	} catch (const XMLException& toCatch) {
		char* message = XMLString::transcode(toCatch.getMessage());
		fprintf(stderr, "fatal error: fail in init xerces.\n");
		fprintf(stderr, "error desc: %s\n", message);
		XMLString::release(&message);
	}
}

odr_extdata::~odr_extdata()
{
	if (_reader) {
		delete _reader, _reader = nullptr;
	}
}

void odr_extdata::startElement(
	const XMLCh* const uri, const XMLCh* const localname,
	const XMLCh* const qname, const Attributes& attrs)
{
	xmlstring element(qname);
	if (!strcmp(element, "dataset")) {
		odr_extdata_check(_f.dataset_parsed == 0);
		_f.dataset_parsed = 1;
		return;
	}

	auto node = make_shared<xmlnode>();
	odr_extdata_check(nullptr != node);

	node->name = element;
	XMLSize_t count = attrs.getLength();
	for (XMLSize_t i = 0; i < count; ++i)
	{
		string key = xmlstring(attrs.getQName(i)).stdstr();
		string value = xmlstring(attrs.getValue(i)).stdstr();
		node->attrs.insert({key, value});
	}
	if (!_stack.empty()) {
		auto parents = _stack.top();
		parents->children.insert({node->name, node});
		node->parents = parents;
	}
	// push to the stack
	_stack.push(node);
}

void odr_extdata::endElement(
	const XMLCh* const uri, const XMLCh* const localname,
	const XMLCh* const qname)
{
	xmlstring element(qname);

	if (!strcmp(element, "dataset")) {
		odr_extdata_check(_f.dataset_parsed);
		_f.dataset_parsed = 0;
		return;
	}

	// check if we match with the stack
	assert(!_stack.empty());
	auto node = _stack.top();
	odr_extdata_check(!strcmp(element, node->name.c_str()));

	// handle the xmlnode at last
	handle_xmlnode(node);
	_stack.pop();
}

void odr_extdata::characters(
	const XMLCh* const chars, const XMLSize_t length)
{
	if (_stack.empty()) return;
	if (*chars == u'\n') return;
	xmlstring tmp(chars);
	auto node = _stack.top();
	node->content = tmp.c_str();
}

void odr_extdata::fatalError(const SAXParseException& exception)
{
	char* message = XMLString::transcode(exception.getMessage());
	fprintf(stderr, "fatal error: %s at line: %lu\n", message,
		exception.getLineNumber());
	XMLString::release(&message);
	exit(1005);
}

int odr_extdata::execute(void)
{
	_reader = XMLReaderFactory::createXMLReader();
	assert(nullptr != _reader);
	
	_reader->setFeature(XMLUni::fgSAX2CoreValidation, true);
	_reader->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);   // optional

	_reader->setContentHandler(this);
	_reader->setErrorHandler(this);

	const char* filename = _map.get_parser().get_extdata_file();
	if (!filename || !*filename) {
		return 0;
	}

	if (!zas::utils::fileexists(filename)) {
		fprintf(stderr, "dataset file %s not found.\n", filename);
		return -1;
	}

	fprintf(stdout, "merging dataset from \"%s\" ... ", filename);
	fflush(nullptr);

	// parse the xml file
	try { _reader->parse(filename); }
	catch (const XMLException& c) {
		char* message = XMLString::transcode(c.getMessage());
		fprintf(stdout, "\rerror in parsing xml: %s\n", message);
		XMLString::release(&message);
		return -2;
	}
	catch (const SAXParseException& c) {
		char* message = XMLString::transcode(c.getMessage());
		fprintf(stdout, "\rerror in parsing xml: %s\n", message);
		XMLString::release(&message);
		return -3;
	} catch (...) {
		fprintf(stdout, "\runknown error in paring xml.\n");
		return -4;
	}

	fprintf(stdout, "[success]\n");
	return 0;
}

void odr_extdata::handle_xmlnode(shared_ptr<xmlnode> node)
{
	if (node->name == "roadObject") {
		handle_roadobject(node);
	}
	else if (node->name == "coordOffset") {
		handle_coordoffset(node);
	}
	else if (node->name == "arrow") {
		handle_road_arrow(node);
	}
	else if (node->name == "signalBoard") {
		// 2022-07-06 set sigboard dispear。
		// handle_sigboard(node);
	}
	else if (node->name == "approach") {
		handle_approach(node);
	}
	else if (node->name == "junconver") {
		handle_junconver(node);
	}
}

exd_approach_info::exd_approach_info()
{

}

exd_junconver_info::exd_junconver_info()
: ascending(-1.), descending(-1.) {
	listnode_init(ownerlist);
}

exd_junconvers::exd_junconvers()
: ascending(-1.), descending(-1.) {
	listnode_init(juncvr_list);
}

exd_junconvers::~exd_junconvers()
{
	// detach all items
	while (!listnode_isempty(juncvr_list)) {
		auto juncvr = list_entry(exd_junconver_info, ownerlist, juncvr_list.next);
		listnode_del(juncvr->ownerlist);
		delete juncvr;
	}
}

exd_arrow_info::exd_arrow_info()
: uid(0), type(arrow_type_unknown)
, fillcolor(1), ascending(-1.), descending(-1.) {
	listnode_init(ownerlist);
}

exd_arrows::exd_arrows()
: ascending(-1.), descending(-1.)
{
	listnode_init(arrow_list);
}

exd_arrows::~exd_arrows()
{
	// detach all items
	while (!listnode_isempty(arrow_list)) {
		auto arrow = list_entry(exd_arrow_info, ownerlist, arrow_list.next);
		listnode_del(arrow->ownerlist);
	}
}

exd_sigboards::exd_sigboards()
: height(-1.) {
	listnode_init(sigboard_list);
}

exd_sigboards::~exd_sigboards()
{
	// detach all items
	while (!listnode_isempty(sigboard_list)) {
		auto sigboard = list_entry(exd_sigboard_info, ownerlist, sigboard_list.next);
		listnode_del(sigboard->ownerlist);
	}
}

exd_sigboard_info::exd_sigboard_info()
: uid(0), width(0.), length(0.), shape_type(0)
, kind(0) , major_type(0), sub_type(0)
, height(-1.)
{
	listnode_init(ownerlist);
}

void odr_extdata::handle_road_arrow(shared_ptr<xmlnode> node)
{
	exd_arrows* arrows;
	auto p = node->parents.lock();
	assert(nullptr != p);

	if (nullptr == p->userdata) {
		arrows = new exd_arrows();
		odr_extdata_check(nullptr != arrows);
		p->userdata = (void*)arrows;
	}
	else arrows = (exd_arrows*)p->userdata;

	// let's create the arrow
	auto arrow = new exd_arrow_info();
	odr_extdata_check(nullptr != arrow);
	handle_arrow_property(node, arrow);
	handle_arrow_polygon(node, arrow);

	// add to list
	listnode_add(arrows->arrow_list, arrow->ownerlist);

	// finished, just remove myself
	auto iter = p->children.find("arrow");
	odr_extdata_check(iter != p->children.end() && iter->second == node);
	p->children.erase(iter);
}

void odr_extdata::handle_junconver(shared_ptr<xmlnode> node)
{
	exd_junconvers* juncvrs;
	auto p = node->parents.lock();
	assert(nullptr != p);

	if (nullptr == p->userdata) {
		juncvrs = new exd_junconvers();
		odr_extdata_check(nullptr != juncvrs);
		p->userdata = (void*)juncvrs;
	}
	else juncvrs = (exd_junconvers*)p->userdata;

	// let's create the arrow
	auto juncvr = new exd_junconver_info();

	juncvr->roadid = strtol(node->attrs["roadseg"].c_str(), nullptr, 10);
	juncvr->junctionid = strtol(node->attrs["junction"].c_str(), nullptr, 10);
	juncvr->uuid = strtol(node->attrs["uuid"].c_str(), nullptr, 10);

	odr_extdata_check(nullptr != juncvr);
	handle_junconver_property(node, juncvr);
	handle_junconver_polygon(node, juncvr);

	// add to list
	listnode_add(juncvrs->juncvr_list, juncvr->ownerlist);

	// finished, just remove myself
	auto iter = p->children.find("junconver");
	odr_extdata_check(iter != p->children.end() && iter->second == node);
	p->children.erase(iter);
}

void odr_extdata::handle_approach(shared_ptr<xmlnode> node)
{
	size_t uid;
	shared_ptr<xmlnode> item = check_approach_match_with_map(node, uid);
	if (nullptr == item) { return; }

	exd_approach_info* approach_info = new exd_approach_info();
	approach_info->uniqueId = uid;

	string junctionid = item->attrs["junction"];
	if (!junctionid.empty()) {
		approach_info->junctionid = strtoull(junctionid.c_str(), nullptr, 10);
	}

	update_approach_info(node, approach_info);
}

void odr_extdata::handle_sigboard(shared_ptr<xmlnode> node)
{
	exd_sigboards* sigboards;
	auto p = node->parents.lock();
	assert(nullptr != p);

	if (nullptr == p->userdata) {
		sigboards = new exd_sigboards();
		odr_extdata_check(nullptr != sigboards);
		p->userdata = (void*)sigboards;
	}
	else sigboards = (exd_sigboards*)p->userdata;

	// let's create the arrow
	auto sigboard = new exd_sigboard_info();
	odr_extdata_check(nullptr != sigboard);
	handle_sigboard_property(node, sigboard);
	if (handle_sigboard_polygon(node, sigboard)) {
		delete sigboard;
	} else {
		// add to list
		listnode_add(sigboards->sigboard_list, sigboard->ownerlist);
	}
	// finished, just remove myself
	auto iter = p->children.find("signalBoard");
	odr_extdata_check(iter != p->children.end() && iter->second == node);
	p->children.erase(iter);
}

void odr_extdata::handle_sigboard_property(
	shared_ptr<xmlnode>node, exd_sigboard_info* sigboard)
{
	auto xprop_iter = node->children.find("properties");
	odr_extdata_check(xprop_iter != node->children.end());
	auto xprop = xprop_iter->second;

	auto xuid_iter = xprop->children.find("uniqueId");
	odr_extdata_check(xuid_iter != xprop->children.end());
	sigboard->uid = strtol(xuid_iter->second->content.c_str(), nullptr, 10);

	auto xwidth_iter = xprop->children.find("width");
	odr_extdata_check(xwidth_iter != xprop->children.end());
	sigboard->width = strtod(xwidth_iter->second->content.c_str(), nullptr);
	sigboard->width /= 100.0;

	auto xlength_iter = xprop->children.find("length");
	odr_extdata_check(xlength_iter != xprop->children.end());
	sigboard->length = strtod(xlength_iter->second->content.c_str(), nullptr);
	sigboard->length /= 100.0;

	auto xshptype_iter = xprop->children.find("shapeType");
	odr_extdata_check(xshptype_iter != xprop->children.end());
	if (xshptype_iter->second->content == "square") {
		sigboard->shape_type = hdrm_sigboard_shptype_square;
	}
	else if (xshptype_iter->second->content == "rectangle") {
		sigboard->shape_type = hdrm_sigboard_shptype_rectangle;
	}
	else if (xshptype_iter->second->content == "circle") {
		sigboard->shape_type = hdrm_sigboard_shptype_circle;
	}
	else if (xshptype_iter->second->content == "triangle") {
		sigboard->shape_type = hdrm_sigboard_shptype_triangle;
	}
	else if (xshptype_iter->second->content == "inv-triangle") {
		sigboard->shape_type = hdrm_sigboard_shptype_inv_triangle;
	}
	else {
		fprintf(stderr, "signal board: unknown shape type \"%s\".\n",
		xshptype_iter->second->content.c_str());
		exit(115);
	}

	auto xkind_iter = xprop->children.find("kind");
	odr_extdata_check(xkind_iter != xprop->children.end());
	if (xkind_iter->second->content == "main") {
		sigboard->kind = hdrm_sigboard_kind_main;
	}
	else if (xkind_iter->second->content == "auxillary") {
		sigboard->kind = hdrm_sigboard_kind_auxillary;
	}
	else if (xkind_iter->second->content == "composite") {
		sigboard->kind = hdrm_sigboard_kind_composite;
	}
	else {
		fprintf(stderr, "signal board: unknown kind \"%s\".\n",
		xkind_iter->second->content.c_str());
		exit(115);
	}

	auto xmajortype_iter = xprop->children.find("majorType");
	odr_extdata_check(xmajortype_iter != xprop->children.end());
	sigboard->major_type = strtoul(xmajortype_iter->second
		->content.c_str(), nullptr, 10);
	
	auto xsubtype_iter = xprop->children.find("subType");
	odr_extdata_check(xsubtype_iter != xprop->children.end());
	sigboard->sub_type = strtoul(xsubtype_iter->second
		->content.c_str(), nullptr, 10);

	// height
	auto xheight_iter = xprop->children.find("height");
	if (xheight_iter != xprop->children.end()) {
		sigboard->height = strtod(xheight_iter->second->content.c_str(), nullptr);
		if (sigboard->height < 0.) {
			fprintf(stderr, "signal board: height shall not be"
				" less than 0 (cur:=%.1f)\n", sigboard->height);
			exit(115);
		}
	}
}

int odr_extdata::handle_sigboard_polygon(shared_ptr<xmlnode> node, exd_sigboard_info* sigboard)
{
	auto xploygon_iter = node->children.find("polygon");
	odr_extdata_check(xploygon_iter != node->children.end());
	auto xpolygon = xploygon_iter->second;

	for (auto& xpt_iter : xpolygon->children) {
		if (xpt_iter.first != "point3d") {
			continue;
		}
		auto xpt = xpt_iter.second;

		// handle lon, lat, zoffset
		point3d pt;
		auto xlon = xpt->attrs.find("lon");
		odr_extdata_check(xlon != xpt->attrs.end());
		auto xlat = xpt->attrs.find("lat");
		odr_extdata_check(xlat != xpt->attrs.end());
		auto xzoff = xpt->attrs.find("zoffset");
		odr_extdata_check(xzoff != xpt->attrs.end());
		auto xproj = xpt->attrs.find("proj");
		if (xproj != xpt->attrs.end()) {
			// currently only support wgs84
			odr_extdata_check(xproj->second == "wgs84");
		}
		sigboard->polygon.push_back(llh(
			strtod(xlon->second.c_str(), nullptr),
			strtod(xlat->second.c_str(), nullptr),
			strtod(xzoff->second.c_str(), nullptr)));
	}
	if (sigboard->polygon.size() != 5) {
		fprintf(stderr, "signal board: unrecognized outline.\n");
		return -1;
	}
	return 0;
}

void odr_extdata::handle_arrow_polygon(shared_ptr<xmlnode> node, exd_arrow_info* arrow)
{
	auto xploygon_iter = node->children.find("polygon");
	odr_extdata_check(xploygon_iter != node->children.end());
	auto xpolygon = xploygon_iter->second;

	for (auto& xpt_iter : xpolygon->children) {
		if (xpt_iter.first != "point3d") {
			continue;
		}
		auto xpt = xpt_iter.second;

		// handle lon, lat, zoffset
		point3d pt;
		auto xlon = xpt->attrs.find("lon");
		odr_extdata_check(xlon != xpt->attrs.end());
		auto xlat = xpt->attrs.find("lat");
		odr_extdata_check(xlat != xpt->attrs.end());
		auto xzoff = xpt->attrs.find("zoffset");
		odr_extdata_check(xzoff != xpt->attrs.end());
		auto xproj = xpt->attrs.find("proj");
		if (xproj != xpt->attrs.end()) {
			// currently only support wgs84
			odr_extdata_check(xproj->second == "wgs84");
		}
		arrow->polygon.push_back(llh(
			strtod(xlon->second.c_str(), nullptr),
			strtod(xlat->second.c_str(), nullptr),
			strtod(xzoff->second.c_str(), nullptr)));
	}
}

void odr_extdata::handle_junconver_polygon(shared_ptr<xmlnode> node, exd_junconver_info* juncvr)
{
	auto xploygon_iter = node->children.find("polygon");
	odr_extdata_check(xploygon_iter != node->children.end());
	auto xpolygon = xploygon_iter->second;

	for (auto& xpt_iter : xpolygon->children) {
		if (xpt_iter.first != "point3d") {
			continue;
		}
		auto xpt = xpt_iter.second;

		// handle lon, lat, zoffset
		point3d pt;
		auto xlon = xpt->attrs.find("lon");
		odr_extdata_check(xlon != xpt->attrs.end());
		auto xlat = xpt->attrs.find("lat");
		odr_extdata_check(xlat != xpt->attrs.end());
		auto xzoff = xpt->attrs.find("zoffset");
		odr_extdata_check(xzoff != xpt->attrs.end());
		auto xproj = xpt->attrs.find("proj");
		if (xproj != xpt->attrs.end()) {
			// currently only support wgs84
			odr_extdata_check(xproj->second == "wgs84");
		}
		juncvr->polygon.push_back(llh(
			strtod(xlon->second.c_str(), nullptr),
			strtod(xlat->second.c_str(), nullptr),
			strtod(xzoff->second.c_str(), nullptr)));
	}
}

void odr_extdata::handle_arrow_property(shared_ptr<xmlnode>node, exd_arrow_info* arrow)
{
	auto xprop_iter = node->children.find("properties");
	odr_extdata_check(xprop_iter != node->children.end());
	auto xprop = xprop_iter->second;

	auto xuid_iter = xprop->children.find("uniqueId");
	odr_extdata_check(xuid_iter != xprop->children.end());
	arrow->uid = strtol(xuid_iter->second->content.c_str(), nullptr, 10);

	auto xtype_iter = xprop->children.find("type");
	odr_extdata_check(xtype_iter != xprop->children.end());
	arrow->type = strtoul(xtype_iter->second->content.c_str(), nullptr, 10);

	auto xfillclr_iter = xprop->children.find("fillcolor");
	if (xfillclr_iter != xprop->children.end()) {
		arrow->fillcolor = strtol(xfillclr_iter->second->content.c_str(), nullptr, 10);
	}
	// ascending
	auto xascending_iter = xprop->children.find("ascending");
	if (xascending_iter != xprop->children.end()) {
		arrow->ascending = strtod(xascending_iter->second->content.c_str(), nullptr);
		if (arrow->ascending < 0.) {
			fprintf(stderr, "arrow: ascending shall not be"
				" less than 0 (cur:=%.1f)\n", arrow->ascending);
			exit(115);
		}
	}
	// descending
	auto xdescending_iter = xprop->children.find("descending");
	if (xdescending_iter != xprop->children.end()) {
		arrow->descending = strtod(xdescending_iter->second->content.c_str(), nullptr);
		if (arrow->descending < 0.) {
			fprintf(stderr, "arrow: descending shall not be"
				" less than 0 (cur:=%.1f)\n", arrow->descending);
			exit(115);
		}
	}
}

void odr_extdata::handle_junconver_property(shared_ptr<xmlnode> node, exd_junconver_info* juncvr)
{
	auto xprop_iter = node->children.find("properties");
	odr_extdata_check(xprop_iter != node->children.end());
	auto xprop = xprop_iter->second;

	auto xtype_iter = xprop->children.find("areaid");
	odr_extdata_check(xtype_iter != xprop->children.end());
	juncvr->areaid = xtype_iter->second->content.c_str();

	auto xuid_iter = xprop->children.find("id");
	odr_extdata_check(xuid_iter != xprop->children.end());
	juncvr->id = xuid_iter->second->content.c_str();

	auto xvalid_iter = xprop->children.find("valid");
	odr_extdata_check(xvalid_iter != xprop->children.end());
	juncvr->valid = strtol(xvalid_iter->second->content.c_str(), nullptr, 10);

	auto xshapeid_iter = xprop->children.find("shapeid");
	odr_extdata_check(xshapeid_iter != xprop->children.end());
	juncvr->shapeid = xshapeid_iter->second->content.c_str();

	auto xmajorclass_iter = xprop->children.find("majorclass");
	odr_extdata_check(xmajorclass_iter != xprop->children.end());
	juncvr->majorclass = strtol(xmajorclass_iter->second->content.c_str(), nullptr, 10);

	auto xsubclass_iter = xprop->children.find("subclass");
	odr_extdata_check(xsubclass_iter != xprop->children.end());
	juncvr->subclass = strtol(xsubclass_iter->second->content.c_str(), nullptr, 10);

	auto xbcdirect_iter = xprop->children.find("bcdirect");
	odr_extdata_check(xbcdirect_iter != xprop->children.end());
	juncvr->bcdirect = strtol(xbcdirect_iter->second->content.c_str(), nullptr, 10);

	auto xtrans_iter = xprop->children.find("transiting");
	odr_extdata_check(xtrans_iter != xprop->children.end());
	juncvr->transiting = strtol(xtrans_iter->second->content.c_str(), nullptr, 10);

	auto xacross_iter = xprop->children.find("across");
	odr_extdata_check(xacross_iter != xprop->children.end());
	juncvr->across = strtol(xacross_iter->second->content.c_str(), nullptr, 10);

	// ascending
	auto xascending_iter = xprop->children.find("ascending");
	if (xascending_iter != xprop->children.end()) {
		juncvr->ascending = strtod(xascending_iter->second->content.c_str(), nullptr);
		if (juncvr->ascending < 0.) {
			fprintf(stderr, "juncvr: ascending shall not be"
				" less than 0 (cur:=%.1f)\n", juncvr->ascending);
			exit(115);
		}
	}
	// descending
	auto xdescending_iter = xprop->children.find("descending");
	if (xdescending_iter != xprop->children.end()) {
		juncvr->descending = strtod(xdescending_iter->second->content.c_str(), nullptr);
		if (juncvr->descending < 0.) {
			fprintf(stderr, "juncvr: descending shall not be"
				" less than 0 (cur:=%.1f)\n", juncvr->descending);
			exit(115);
		}
	}
}

void odr_extdata::update_arrows_info(shared_ptr<xmlnode> node)
{
	assert(nullptr != node->userdata);
	auto arrows = (exd_arrows*)node->userdata;

	// handle <info>
	auto info = node->children.equal_range("info");
	for (auto it = info.first; it != info.second; ++it) {
		auto info_node = it->second;
		// ascending
		auto ascending = info_node->attrs.find("ascending");
		if (ascending != info_node->attrs.end()) {
			arrows->ascending = strtod(ascending->second.c_str(), nullptr);
			if (arrows->ascending < 0.) {
				fprintf(stderr, "arrows: ascending shall not be less than 0 (cur:=%.1f)\n",
					arrows->ascending);
				exit(115);
			}
		}
		// descending
		auto descending = info_node->attrs.find("descending");
		if (descending != info_node->attrs.end()) {
			arrows->descending = strtod(descending->second.c_str(), nullptr);
			if (arrows->descending < 0.) {
				fprintf(stderr, "arrows: descending shall not be less than 0 (cur:=%.1f)\n",
					arrows->descending);
				exit(115);
			}
		}
	}

	auto i = arrows->arrow_list.next;
	for (; i != &arrows->arrow_list; i = i->next) {
		auto arrow = list_entry(exd_arrow_info, ownerlist, i);
		auto* ro = _map.get_road_object(arrow->uid);
		if (nullptr == ro) {
			fprintf(stderr, "arrow \"%lu\" not merged to odr-map.\n", arrow->uid);
			continue;
		}

		// update arrow info
		if (arrow->ascending < 0) {
			arrow->ascending = (arrows->ascending < 0)
			? 0. : arrows->ascending;
		}
		if (arrow->descending < 0) {
			arrow->descending = (arrows->descending < 0)
			? 0. : arrows->descending;
		}

		if (ro->_arrowinfo) {
			fprintf(stderr, "road arrow: duplicate binding of object (id:%lu)\n", arrow->uid);
			continue;
		}
		ro->_arrowinfo = arrow;

		// get the odr_road of the object
		auto *parents = ro->get_parents()->get_parents();
		odr_extdata_check(parents->get_type() == odr_elemtype_road);
		update_arrow_object(zas_downcast(odr_element, odr_road, parents), ro);
	}

	// finished all
	delete arrows;
	node->userdata = nullptr;
}

void odr_extdata::update_arrow_object(odr_road* r, odr_object* obj)
{
	auto outline = obj->_outline;
	// replace the original outline to the new one in arrow object
	outline->_cornerlocals.clear();

	auto arrow = obj->_arrowinfo;
	int sz = arrow->polygon.size();
	if (sz != 0) {
		auto corners = outline->_cornerlocals.new_objects(sz);
		int i = 0;
		for (auto pt : arrow->polygon) {
			_map.get_georef().transform_point(pt);
			pt -= _map.get_mapoffset();

			double height = arrow->ascending + arrow->descending;
			auto corner = new odr_cornerlocal(outline,
				pt.xyz.x, pt.xyz.y, pt.xyz.z - arrow->descending, height);
			corners[i++] = corner;
		}
	}
	// update the odrv_roadobject
	r->odrv_update_roadobject(obj, nullptr, RoadObjectCorner::Type::Road);
}

void odr_extdata::update_junconver_object(odr_road* r, odr_object* obj)
{
	auto outline = new odr_outline(obj);
	obj->set_outline(outline);
	outline->_cornerlocals.clear();

	auto juncvr = obj->_juncvrinfo;
	int sz = juncvr->polygon.size();
	if (sz != 0) {
		auto corners = outline->_cornerlocals.new_objects(sz);
		int i = 0;
		for (auto pt : juncvr->polygon) {
			_map.get_georef().transform_point(pt);
			pt -= _map.get_mapoffset();

			// double height = juncvr->ascending + juncvr->descending;
			auto corner = new odr_cornerlocal(outline,
				pt.xyz.x, pt.xyz.y, pt.xyz.z - juncvr->ascending, juncvr->descending); // 0.1 is height
			corners[i++] = corner;
		}
	}
	// update the odrv_roadobject
	r->odrv_update_roadobject(obj, make_shared<RoadObject>(), RoadObjectCorner::Type::Road);
}

void odr_extdata::update_sigboards_info(shared_ptr<xmlnode> node)
{
	assert(nullptr != node->userdata);
	auto sigboards = (exd_sigboards*)node->userdata;

	// handle <info>
	auto info = node->children.equal_range("info");
	for (auto it = info.first; it != info.second; ++it) {
		auto info_node = it->second;
		// height
		auto height = info_node->attrs.find("height");
		if (height != info_node->attrs.end()) {
			sigboards->height = strtod(height->second.c_str(), nullptr);
			if (sigboards->height < 0.) {
				fprintf(stderr, "sigboards: height shall not be less than 0 (cur:=%.1f)\n",
					sigboards->height);
				exit(115);
			}
		}
	}

	auto i = sigboards->sigboard_list.next;
	for (; i != &sigboards->sigboard_list; i = i->next) {
		auto sigboard = list_entry(exd_sigboard_info, ownerlist, i);
		auto* ro = _map.get_road_object(sigboard->uid);
		if (nullptr == ro) {
			fprintf(stderr, "signal board \"%lu\" not merged to odr-map.\n", sigboard->uid);
			continue;
		}

		// update sigboard info
		if (sigboard->height < 0) {
			sigboard->height = (sigboards->height < 0)
			? 0. : sigboards->height;
		}

		if (ro->_sigboardinfo) {
			fprintf(stderr, "signal board: duplicate binding of object (id:%lu)\n", sigboard->uid);
			continue;
		}
		ro->_sigboardinfo = sigboard;

		// get the odr_road of the object
		auto *parents = ro->get_parents()->get_parents();
		odr_extdata_check(parents->get_type() == odr_elemtype_road);
		update_sigboard_object(zas_downcast(odr_element, odr_road, parents), ro);
	}

	// finished all
	delete sigboards;
	node->userdata = nullptr;
}

inline static double utm_distance(const point3d& a, const point3d& b)
{
	double dx = a.xyz.x - b.xyz.x;
	double dy = a.xyz.y - b.xyz.y;
	return sqrt(dx * dx + dy * dy);
}

inline static double gradient_rad(double x1, double y1, double x2, double y2)
{
	double rad;
	double dx = x2 - x1;
	double drad = (dx < 0.) ? M_PI : 0.;
	if (fabs(dx) < 1e-5) {
		rad = (y1 < y2) ? M_PI_2 : -M_PI_2;
	}
	else rad = atan((y2 - y1) / dx);
	return rad + drad;
}


#define odr_object_clear_all_param(obj)	\
do {	\
	(obj)->_z_offset = 0.;	\
	(obj)->_valid_length = 0.;	\
	(obj)->_length = 0.;	\
	(obj)->_width = 0.;	\
	(obj)->_radius = 0.;	\
	(obj)->_height = 0.;	\
	(obj)->_hdg = 0.;	\
	(obj)->_pitch = 0.;	\
	(obj)->_roll = 0.;	\
} while (0)

void odr_extdata::update_sigboard_object(odr_road* r, odr_object* obj)
{
	if (nullptr == obj->_sigboardinfo) {
		return; // not the one we'll handle
	}

	// convert all point to UTM
	for (auto& pt : obj->_sigboardinfo->polygon) {
		_map.get_georef().transform_point(pt);
		pt -= _map.get_mapoffset();
	}
	auto fstpt = obj->_sigboardinfo->polygon.begin();
	auto lstpt = prev(obj->_sigboardinfo->polygon.end());
	odr_extdata_check(*fstpt == *lstpt);

	auto p1 = next(fstpt);
	double dist1 = utm_distance(*fstpt, *p1);
	auto p2 = next(p1);
	double dist2 = utm_distance(*p1, *p2);
	auto cfg = _map.get_parser().get_hdmap_info();

	odr_object_clear_all_param(obj);
	if (fabs(dist1 - obj->_sigboardinfo->length)
		< fabs(dist2 - obj->_sigboardinfo->length)) {
		// edge fst-p1 be the heading direction
		obj->_hdg = gradient_rad(fstpt->xyz.x, fstpt->xyz.y, p1->xyz.x, p1->xyz.y);
		// calculate the height
		obj->_height = (obj->_sigboardinfo->height < 1e-3)
			? ((cfg->sigboard.height > 1e-3) ? cfg->sigboard.height
			: dist2) : obj->_sigboardinfo->height;
	} else {
		// edge p1-p2 be the heading direction
		obj->_hdg = gradient_rad(p1->xyz.x, p1->xyz.y, p2->xyz.x, p2->xyz.y);
		// calculate the height
		obj->_height = (obj->_sigboardinfo->height < 1e-3)
			? ((cfg->sigboard.height > 1e-3) ? cfg->sigboard.height
			: dist1) : obj->_sigboardinfo->height;
	}
	while (obj->_hdg > M_PI * 2.0) obj->_hdg -= M_PI * 2.0;

	if (obj->_sigboardinfo->shape_type == hdrm_sigboard_shptype_rectangle) {
		update_sigboard_rectangle_object(obj, r);
	}
	else if (obj->_sigboardinfo->shape_type == hdrm_sigboard_shptype_square) {
		assert(obj->_sigboardinfo->length == obj->_sigboardinfo->width);
		update_sigboard_rectangle_object(obj, r);
	}
	else if (obj->_sigboardinfo->shape_type == hdrm_sigboard_shptype_circle) {
		assert(obj->_sigboardinfo->length == obj->_sigboardinfo->width);
		update_sigboard_circle_object(obj, r);
	}
	else if (obj->_sigboardinfo->shape_type == hdrm_sigboard_shptype_triangle) {
		update_sigboard_triangle_object(obj, r, false);
	}
	else if (obj->_sigboardinfo->shape_type == hdrm_sigboard_shptype_inv_triangle) {
		update_sigboard_triangle_object(obj, r, true);
	}
}

hdmapcfg* odr_extdata::update_sigboard_object_common_prepare(odr_object* obj, odr_road* r)
{
	auto cfg = _map.get_parser().get_hdmap_info();
	auto p1 = obj->_sigboardinfo->polygon.begin();
	auto p2 = next(p1);
	auto p3 = next(p2);
	auto p4 = next(p3);

	obj->_outline_base.xyz.x = (p1->xyz.x + p2->xyz.x + p3->xyz.x + p4->xyz.x) / 4.0;
	obj->_outline_base.xyz.y = (p1->xyz.y + p2->xyz.y + p3->xyz.y + p4->xyz.y) / 4.0;
	obj->_outline_base.xyz.z = (p1->xyz.z + p2->xyz.z + p3->xyz.z + p4->xyz.z) / 4.0;

	// check if we need a pole
	if (!cfg->sigboard.need_pole) {
		return cfg;
	}

	// obj->height is the height of signal board
	// if the radius is not specified in config file, we just
	// use (signal board height) / 2 as the radius of the pole
	double radius = (cfg->sigboard.pole_radius > 1e-3)
		? cfg->sigboard.pole_radius : (obj->_height / 2.0);
	if (radius < 1e-3) {
		// radius = 0, so we do not need this pole
		return cfg;
	}

	assert(obj->_parents->get_type() == odr_elemtype_objects);
	auto objs = zas_downcast(odr_element, odr_objects, obj->_parents);
	assert(objs == r->_objects);

	size_t id = _map.get_last_objectid();
	auto pole = new odr_object(r, ++id, "signalBoardPole");
	odr_extdata_check(nullptr != pole);
	pole->_outline = new odr_outline(pole);
	odr_extdata_check(nullptr != pole->_outline);

	// create the cylinder surface
	int i, count = M_PI / cfg->eps;
	auto polygon = pole->_outline->_cornerlocals.new_objects(count + 1);
	odr_extdata_check(nullptr != polygon);

	double eps = M_PI * 2.0 / double(count);
	for (i = 0; i < count; ++i) {
		double x = radius * cos(i * eps);
		double y = radius * sin(i * eps);
		polygon[i] = new odr_cornerlocal(pole->_outline, x, y, 0., 0.);
		odr_extdata_check(polygon[i] != nullptr);
	}
	polygon[i] = new odr_cornerlocal(obj->_outline, radius, 0., 0., 0.);
	odr_extdata_check(polygon[i] != nullptr);

	pole->_outline_base = obj->_outline_base;
	pole->_type = object_type_userdef;

	// add to road objects array
	objs->add_object(pole);
	_map.set_road_object(pole);
	pole->_pole_related_sigboard = obj;
	return cfg;
}

void odr_extdata::update_sigboard_triangle_object(odr_object* obj, odr_road* r, bool inv)
{
	update_sigboard_object_common_prepare(obj, r);
	double l_2 = obj->_sigboardinfo->length / 2.0;
	double w_2 = obj->_sigboardinfo->width / 2.0;
	obj->_outline_base.xyz.z += w_2;
	obj->_outline->_cornerlocals.clear();

	auto polygon = obj->_outline->_cornerlocals.new_objects(4);
	odr_extdata_check(nullptr != polygon);

	if (inv) {
		polygon[0] = new odr_cornerlocal(obj->_outline, l_2, -w_2, -obj->_height / 2.0, obj->_height);
		polygon[1] = new odr_cornerlocal(obj->_outline, 0, w_2, -obj->_height / 2.0, obj->_height);
		polygon[2] = new odr_cornerlocal(obj->_outline, -l_2, -w_2, -obj->_height / 2.0, obj->_height);
		polygon[3] = new odr_cornerlocal(obj->_outline, l_2, -w_2, -obj->_height / 2.0, obj->_height);
	} else {
		polygon[0] = new odr_cornerlocal(obj->_outline, 0, -w_2, -obj->_height / 2.0, obj->_height);
		polygon[1] = new odr_cornerlocal(obj->_outline, l_2, w_2, -obj->_height / 2.0, obj->_height);
		polygon[2] = new odr_cornerlocal(obj->_outline, -l_2, w_2, -obj->_height / 2.0, obj->_height);
		polygon[3] = new odr_cornerlocal(obj->_outline, 0, -w_2, -obj->_height / 2.0, obj->_height);
	}

	// nullptr check
	for (int i = 0; i < 4; ++i) {
		odr_extdata_check(polygon[i] != nullptr);
	}

	obj->_roll = M_PI_2 - 1e-9;
	r->odrv_update_roadobject(obj, nullptr, RoadObjectCorner::Type::Road);
}

void odr_extdata::update_sigboard_rectangle_object(odr_object* obj, odr_road* r)
{
	update_sigboard_object_common_prepare(obj, r);
	double l_2 = obj->_sigboardinfo->length / 2.0;
	double w_2 = obj->_sigboardinfo->width / 2.0;
	obj->_outline_base.xyz.z += w_2;
	obj->_outline->_cornerlocals.clear();

	auto polygon = obj->_outline->_cornerlocals.new_objects(5);
	odr_extdata_check(nullptr != polygon);

	polygon[0] = new odr_cornerlocal(obj->_outline, l_2, w_2, -obj->_height / 2.0, obj->_height);
	polygon[1] = new odr_cornerlocal(obj->_outline, l_2, -w_2, -obj->_height / 2.0, obj->_height);
	polygon[2] = new odr_cornerlocal(obj->_outline, -l_2, -w_2, -obj->_height / 2.0, obj->_height);
	polygon[3] = new odr_cornerlocal(obj->_outline, -l_2, w_2, -obj->_height / 2.0, obj->_height);
	polygon[4] = new odr_cornerlocal(obj->_outline, l_2, w_2, -obj->_height / 2.0, obj->_height);

	// nullptr check
	for (int i = 0; i < 5; ++i) {
		odr_extdata_check(polygon[i] != nullptr);
	}

	obj->_roll = M_PI_2 - 1e-9;
	r->odrv_update_roadobject(obj, nullptr, RoadObjectCorner::Type::Road);
}

void odr_extdata::update_sigboard_circle_object(odr_object* obj, odr_road* r)
{
	auto cfg = update_sigboard_object_common_prepare(obj, r);
	double circle_r = obj->_sigboardinfo->length / 2.0;
	obj->_outline_base.xyz.z += circle_r;
	obj->_outline->_cornerlocals.clear();

	int i, count = M_PI * 2.0 / cfg->eps;
	auto polygon = obj->_outline->_cornerlocals.new_objects(count + 1);
	odr_extdata_check(nullptr != polygon);

	double eps = M_PI * 2.0 / double(count);
	for (i = 0; i < count; ++i) {
		double x = circle_r * cos(i * eps);
		double y = circle_r * sin(i * eps);
		polygon[i] = new odr_cornerlocal(obj->_outline, x, y,
			-obj->_height / 2.0, obj->_height);
		odr_extdata_check(polygon[i] != nullptr);
	}
	polygon[i] = new odr_cornerlocal(obj->_outline, circle_r, 0.,
		-obj->_height / 2.0, obj->_height);
	odr_extdata_check(polygon[i] != nullptr);
	
	obj->_roll = M_PI_2 - 1e-9;
	r->odrv_update_roadobject(obj, nullptr, RoadObjectCorner::Type::Road);
}

void odr_extdata::handle_roadobject(shared_ptr<xmlnode> node)
{
	string type = node->attrs["type"];
	odr_extdata_check(!type.empty());

	size_t uid;
	shared_ptr<xmlnode> item = check_roadobj_match_with_map(node, uid);
	if (nullptr == item) { return; }

	if (type == "signalLight") {
		update_siglight_info(uid, item, node);
	}
	else if (type == "roadSideDevice") {
		update_road_side_device_info(uid, item, node);
	}
	else if (type == "roadArrows") {
		update_arrows_info(node);
	}
	else if (type == "signalBoards") {
		// update_sigboards_info(node);
	}
	else if (type == "junconverse") {
		update_junconver_info(node);
	}
}

shared_ptr<xmlnode>
odr_extdata::check_approach_match_with_map(shared_ptr<xmlnode> node, size_t& uid)
{
	uid = 0;
	string uniqueid = node->attrs["uniqueId"];
	if (!uniqueid.empty()) {
		uid = strtoull(uniqueid.c_str(), nullptr, 10);
	}
	// find a relation that fixs the current map vender
	auto relations = node->children.equal_range("relation");
	for (auto it = relations.first; it != relations.second; ++it)
	{
		auto relation = it->second;
		odr_extdata_check(nullptr != relation);
		if (relation->attrs["type"] != "zas-hdrmap") {
			continue;
		}

		for (auto item : relation->children) {
			if (item.first != "item") {
				continue;
			}
			if (item.second->attrs["vendor"] != _map.get_vendor()) {
				continue;
			}
			if (item.second->attrs["version"] != _map.get_parser().get_version()) {
				continue;
			}
			// found a match item
			return item.second;
		}
	}
	return nullptr;
}

void odr_extdata::update_approach_info(shared_ptr<xmlnode> node, exd_approach_info* approach_info)
{
	auto info = node->children.equal_range("info");
	for (auto it = info.first; it != info.second; ++it) {
		auto info_node = it->second;
		string uuid = info_node->attrs["uuid"];
		approach_info->uuid = uuid;
	}

	auto* junc = _map.getjunction_byid(approach_info->junctionid);
	if(nullptr != junc) {
		junc->add_approach(approach_info->uuid);
	}
}

shared_ptr<xmlnode>
odr_extdata::check_roadobj_match_with_map(shared_ptr<xmlnode> node, size_t& uid)
{
	uid = 0;
	string uniqueid = node->attrs["uniqueId"];
	if (!uniqueid.empty()) {
		uid = strtoull(uniqueid.c_str(), nullptr, 10);
	}
	// find a relation that fixs the current map vender
	auto relations = node->children.equal_range("relation");
	for (auto it = relations.first; it != relations.second; ++it)
	{
		auto relation = it->second;
		odr_extdata_check(nullptr != relation);
		if (relation->attrs["type"] != "zas-hdrmap") {
			continue;
		}

		for (auto item : relation->children) {
			if (item.first != "item") {
				continue;
			}
			if (item.second->attrs["vendor"] != _map.get_vendor()) {
				continue;
			}
			if (item.second->attrs["version"] != _map.get_parser().get_version()) {
				continue;
			}
			// found a match item
			return item.second;
		}
	}
	return nullptr;
}

void odr_extdata::update_siglight_info(size_t uid,
	shared_ptr<xmlnode> node, shared_ptr<xmlnode> sig)
{
	string& idstr = node->attrs["id"];
	odr_extdata_check(!idstr.empty());
	if (idstr == "new") {
		create_signallight(node, uid);
		return;
	}

	size_t id = strtoull(idstr.c_str(), nullptr, 10);
	auto* ro = _map.get_road_object(id);
	if (nullptr == ro) {
		return;
	}

	// change the name of the road object to "signallight"
	// no matter what it is before
	ro->_name = "signalLight";
	ro->_id = uid;

	// fix all items
	auto fixs = node->children.equal_range("fix");
	for (auto it = fixs.first; it != fixs.second; ++it) {
		auto fix = it->second;
		fix_siglight_info(ro, fix);
	}
}

void odr_extdata::update_junconver_info(shared_ptr<xmlnode> node)
{
	assert(nullptr != node->userdata);
	auto juncvrs = (exd_junconvers*)node->userdata;

	auto info = node->children.equal_range("info");
	for (auto it = info.first; it != info.second; ++it) {
		auto info_node = it->second;
		// ascending
		auto ascending = info_node->attrs.find("ascending");
		if (ascending != info_node->attrs.end()) {
			juncvrs->ascending = strtod(ascending->second.c_str(), nullptr);
			if (juncvrs->ascending < 0.) {
				fprintf(stderr, "juncvrs: ascending shall not be less than 0 (cur:=%.1f)\n",
					juncvrs->ascending);
				exit(115);
			}
		}
		// descending
		auto descending = info_node->attrs.find("descending");
		if (descending != info_node->attrs.end()) {
			juncvrs->descending = strtod(descending->second.c_str(), nullptr);
			if (juncvrs->descending < 0.) {
				fprintf(stderr, "juncvrs: descending shall not be less than 0 (cur:=%.1f)\n",
					juncvrs->descending);
				exit(115);
			}
		}
	}

	auto i = juncvrs->juncvr_list.next;
	for (; i != &juncvrs->juncvr_list; i = i->next) {
		auto juncvr = list_entry(exd_junconver_info, ownerlist, i);
		auto* road = _map.getroad_byid(juncvr->roadid);
		auto* ro = new odr_object(road, juncvr->uuid, "junconver");
		assert(nullptr != ro);

		// update juncvr info
		if (juncvr->ascending < 0) {
			juncvr->ascending = (juncvrs->ascending < 0)
			? 0. : juncvrs->ascending;
		}
		if (juncvr->descending < 0) {
			juncvr->descending = (juncvrs->descending < 0)
			? 0. : juncvrs->descending;
		}

		ro->_type = object_type_patch;
		ro->_juncvrinfo = juncvr;
		road->_objects->add_object(ro);
		_map.set_road_object(ro);
		if (nullptr == ro) {
			fprintf(stderr, "arrow \"%u\" not merged to odr-map.\n", juncvr->uuid);
			continue;
		}

		// get the odr_road of the object
		auto *parents = ro->get_parents();
		odr_extdata_check(parents->get_type() == odr_elemtype_road);
		update_junconver_object(zas_downcast(odr_element, odr_road, parents), ro);

	}

	// finished all
	delete juncvrs;
	node->userdata = nullptr;
}

void odr_extdata::create_signallight(shared_ptr<xmlnode> node,
	size_t uid)
{
	auto roadseg = node->attrs.find("roadseg");
	odr_extdata_check(roadseg != node->attrs.end());
	
	uint32_t roadid = strtol(roadseg->second.c_str(), nullptr, 10);
	auto* road = _map.getroad_byid(roadid);
	if (road == nullptr) {
		fprintf(stdout, "warning: the roadseg(%u), which is required "
		"by new signal light(%lu), is not found.\n", roadid, uid);
		fprintf(stdout, "the rquested \"new\" signal light is not created.\n");
		return;
	}

	auto* siglight = new odr_object(road, uid, "signalLight");
	assert(nullptr != siglight);
	siglight->_type = object_type_userdef;

	// get all data item
	auto data = node->children.equal_range("data");
	assert(!siglight->_sigl_attrs.attrs);
	for (auto it = data.first; it != data.second; ++it) {
		fix_new_siglight_info(siglight, it->second);
	}
	odr_extdata_check(siglight->_sigl_attrs.a.lon_avail
		&& siglight->_sigl_attrs.a.lat_avail
		&& siglight->_sigl_attrs.a.zoffset_avail);
	siglight->_sigl_attrs.attrs = 0; // restore

	// add to road
	road->_objects->add_object(siglight);
	_map.set_road_object(siglight);
}

bool odr_extdata::attr_default(const string& str)
{
	return (!strncmp(str.c_str(), "default", 7))
		? true : false;
}

void odr_extdata::fix_new_siglight_info(odr_object* ro, shared_ptr<xmlnode> data)
{
    auto* minfo = _map.get_parser().get_hdmap_info();

    auto d = data->attrs.find("lon");
    if (d != data->attrs.end()) {
        ro->_lon = strtod(d->second.c_str(), nullptr);
        ro->_sigl_attrs.a.lon_avail = 1;
    }

    d = data->attrs.find("lat");
    if (d != data->attrs.end()) {
        ro->_lat = strtod(d->second.c_str(), nullptr);
        ro->_sigl_attrs.a.lat_avail = 1;
    }

    d = data->attrs.find("zoffset");
    if (d != data->attrs.end()) {
        ro->_z_offset = attr_default(d->second)
            ? minfo->siginfo.zoffset : strtod(d->second.c_str(), nullptr);
        ro->_sigl_attrs.a.zoffset_avail = 1;
    }

    auto attr = data->attrs.find("height");
    if (attr != data->attrs.end()) {
        ro->_height = attr_default(attr->second)
            ? minfo->siginfo.height : strtod(attr->second.c_str(), nullptr);
    }

    attr = data->attrs.find("orientation");
    if (attr != data->attrs.end()) {
        odr_extdata_check(attr->second == "+" || attr->second == "-");
        ro->_orientation = attr->second;
    }

    attr = data->attrs.find("hdg");
    if (attr != data->attrs.end()) {
        int isabs = 0;
        double hdg = strtod(attr->second.c_str(), nullptr);
        ro->_hdg = hdg;
    }

    attr = data->attrs.find("pitch");
    if (attr != data->attrs.end()) {
        ro->_pitch = attr_default(attr->second)
            ? minfo->siginfo.pitch : strtod(attr->second.c_str(), nullptr);
    }

    attr = data->attrs.find("length");
    if (attr != data->attrs.end()) {
        ro->_length = attr_default(attr->second)
            ? minfo->siginfo.length : strtod(attr->second.c_str(), nullptr);
    }

    attr = data->attrs.find("width");
    if (attr != data->attrs.end()) {
        ro->_width = attr_default(attr->second)
            ? minfo->siginfo.width : strtod(attr->second.c_str(), nullptr);
    }

    attr = data->attrs.find("radius");
    if (attr != data->attrs.end()) {
        ro->_radius = attr_default(attr->second)
            ? minfo->siginfo.radius : strtod(attr->second.c_str(), nullptr);
    }

    attr = data->attrs.find("roll");
    if (attr != data->attrs.end()) {
        ro->_roll = attr_default(attr->second)
            ? minfo->siginfo.roll : strtod(attr->second.c_str(), nullptr);
    }
}

void odr_extdata::fix_siglight_info(odr_object* ro, shared_ptr<xmlnode> fix)
{
	auto* minfo = _map.get_parser().get_hdmap_info();

	auto attr = fix->attrs.find("zoffset");
	if (attr != fix->attrs.end()) {
		ro->_z_offset = attr_default(attr->second)
			? minfo->siginfo.zoffset : strtod(attr->second.c_str(), nullptr);
	}

	attr = fix->attrs.find("height");
	if (attr != fix->attrs.end()) {
		ro->_height = attr_default(attr->second)
			? minfo->siginfo.height : strtod(attr->second.c_str(), nullptr);
	}

	attr = fix->attrs.find("orientation");
	if (attr != fix->attrs.end()) {
		odr_extdata_check(attr->second == "+" || attr->second == "-");
		ro->_orientation = attr->second;
	}

	attr = fix->attrs.find("hdg");
	if (attr != fix->attrs.end()) {
		int isabs = 0;
		double hdg = strtod(attr->second.c_str(), nullptr);
		auto ref = fix->attrs.find("hdgref");
		if (ref != fix->attrs.end()) {
			isabs = (ref->second == "absolute") ? 1 : (
				(ref->second == "relative") ? 0 : -1);
			odr_extdata_check(isabs >= 0);
		}
		if (isabs) {
			if (nullptr == ro->_siginfo) ro->_siginfo
				= new exd_siglight_info;
			odr_extdata_check(nullptr != ro->_siginfo);
			ro->_siginfo->heading = hdg;
		}
		else ro->_hdg = hdg;
	}

	attr = fix->attrs.find("pitch");
	if (attr != fix->attrs.end()) {
		ro->_pitch = attr_default(attr->second)
			? minfo->siginfo.pitch : strtod(attr->second.c_str(), nullptr);
	}

	attr = fix->attrs.find("length");
	if (attr != fix->attrs.end()) {
		ro->_length = attr_default(attr->second)
			? minfo->siginfo.length : strtod(attr->second.c_str(), nullptr);
	}

	attr = fix->attrs.find("width");
	if (attr != fix->attrs.end()) {
		ro->_width = attr_default(attr->second)
			? minfo->siginfo.width : strtod(attr->second.c_str(), nullptr);
	}

	attr = fix->attrs.find("radius");
	if (attr != fix->attrs.end()) {
		ro->_radius = attr_default(attr->second)
			? minfo->siginfo.radius : strtod(attr->second.c_str(), nullptr);
	}

	attr = fix->attrs.find("roll");
	if (attr != fix->attrs.end()) {
		ro->_roll = attr_default(attr->second)
			? minfo->siginfo.roll : strtod(attr->second.c_str(), nullptr);
	}
}

int odr_extdata::get_road_side_device_type(shared_ptr<xmlnode> node)
{
	auto info = node->children.equal_range("info");
	for (auto it = info.first; it != info.second; ++it) {
		auto info_node = it->second;
		auto type = info_node->attrs.find("type");
		if (type == info_node->attrs.end()) {
			continue;
		}
		// check type
		if (type->second == "radar-vision") {
			return hdrm_rsdtype_radar_vision;
		}
		else if (type->second == "radar") {
			return hdrm_rsdtype_radar;
		}
		else if (type->second == "camera") {
			return hdrm_rsdtype_camera;
		}
	}
	return hdrm_rsdtype_unknown;
}

void odr_extdata::update_road_side_device_info(size_t uid,
	shared_ptr<xmlnode> node, shared_ptr<xmlnode> rsd)
{
	auto roadseg = node->attrs.find("roadseg");
	odr_extdata_check(roadseg != node->attrs.end());

	uint32_t roadid = strtol(roadseg->second.c_str(), nullptr, 10);
	auto* road = _map.getroad_byid(roadid);
	if (road == nullptr) {
		fprintf(stdout, "warning: the roadseg(%u), which is required "
		"by new road side device(%lu), is not found.\n", roadid, uid);
		fprintf(stdout, "the rquested \"new\" road side device is not created.\n");
		return;
	}

	uint32_t juncid = 0;
	auto junction = node->attrs.find("junction");
	if (junction != node->attrs.end()) {
		juncid = strtol(junction->second.c_str(), nullptr, 10);
	}

	auto* ro = new odr_object(road, uid, "roadSideDevice");
	assert(nullptr != ro);
	ro->_type = object_type_userdef;
	ro->_rsdinfo = new exd_roadside_device_info();
	assert(nullptr != ro->_rsdinfo);

	// fix all items
	auto fixs = node->children.equal_range("data");
	for (auto it = fixs.first; it != fixs.second; ++it) {
		load_rsd_info(ro, it->second);
	}
	
	odr_extdata_check(ro->_rsdinfo->attrs.pos_avail);
	odr_extdata_check(ro->_rsdinfo->attrs.hdg_avail);
	odr_extdata_check(ro->_rsdinfo->attrs.zoffset_avail);
	odr_extdata_check(ro->_rsdinfo->attrs.pitch_avail);
	odr_extdata_check(ro->_rsdinfo->attrs.roll_avail);

	ro->_rsdinfo->roadid = roadid;
	ro->_rsdinfo->junctionid = juncid;
	ro->_rsdinfo->devtype = get_road_side_device_type(rsd);

	// add to road
	road->_objects->add_object(ro);
	_map.set_road_object(ro);
}

void odr_extdata::load_rsd_info(odr_object* ro, shared_ptr<xmlnode> fix)
{
	auto ilat = fix->attrs.find("lat");
	auto ilon = fix->attrs.find("lon");
	auto iproj = fix->attrs.find("proj");
	if (iproj != fix->attrs.end()) {
		// currently only support wgs84
		odr_extdata_check(iproj->second == "wgs84");
	}

	if (ilat != fix->attrs.end() && ilon != fix->attrs.end()) {
		ro->_lon = strtod(ilon->second.c_str(), nullptr);
		ro->_lat = strtod(ilat->second.c_str(), nullptr);
		ro->_rsdinfo->attrs.pos_avail = 1;
	}

	auto attr = fix->attrs.find("hdg");
	if (attr != fix->attrs.end()) {
		ro->_hdg = strtod(attr->second.c_str(), nullptr);
		ro->_rsdinfo->attrs.hdg_avail = 1;
	}
	ro->_rsdinfo->attrs.abs_hdg = 0;
	auto ref = fix->attrs.find("hdgref");
	if (ref != fix->attrs.end()) {
		if (attr->second == "absolute") {
			ro->_rsdinfo->attrs.abs_hdg = 1;
		}
		else odr_extdata_check(attr->second == "relative");
	}

	attr = fix->attrs.find("zoffset");
	if (attr != fix->attrs.end()) {
		ro->_z_offset = strtod(attr->second.c_str(), nullptr);
		ro->_rsdinfo->attrs.zoffset_avail = 1;
	}

	attr = fix->attrs.find("pitch");
	if (attr != fix->attrs.end()) {
		ro->_pitch = strtod(attr->second.c_str(), nullptr);
		ro->_rsdinfo->attrs.pitch_avail = 1;
	}

	attr = fix->attrs.find("roll");
	if (attr != fix->attrs.end()) {
		ro->_roll = strtod(attr->second.c_str(), nullptr);
		ro->_rsdinfo->attrs.roll_avail = 1;
	}
}

void odr_extdata::handle_coordoffset(shared_ptr<xmlnode> node)
{
	auto vendor = node->attrs["vendor"];
	if (vendor != _map.get_vendor()) {
		return;
	}
	auto version = node->attrs["version"];
	if (version != _map.get_parser().get_version()) {
		return;
	}
	auto offs = node->children.find("offset");
	if (offs == node->children.end()) {
		return;
	}
	string dx = offs->second->attrs["dx"];
	string dy = offs->second->attrs["dy"];
	if (dx.empty() || dy.empty()) {
		return;
	}
	_map.set_mapoffset(strtod(dx.c_str(), nullptr),
		strtod(dy.c_str(), nullptr));
}

} // end of osm_parser1
/* EOF */
