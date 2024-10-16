#ifndef __CXX_OSM_PARSER1_XML_H__
#define __CXX_OSM_PARSER1_XML_H__

#include <map>
#include <memory>

#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/sax2/Attributes.hpp>

#define xc(t) (const XMLCh*)u##t

namespace osm_parser1 {
using namespace std;
using namespace XERCES_CPP_NAMESPACE;

class xmlstring
{
public:
	xmlstring() : _str(nullptr) {}
	xmlstring(const XMLCh* str)
	: _str((str) ? XMLString::transcode(str) : nullptr) {
	}
	~xmlstring() {
		if (_str) {
			XMLString::release(&_str);
			_str = nullptr;
		}
	}
	xmlstring(const xmlstring&) = delete;

	const char* operator=(const XMLCh* str) {
		if (_str) {
			XMLString::release(&_str);
		}
		_str = (str) ? XMLString::transcode(str) : nullptr;
		return _str;
	}

	operator const char*(void) {
		return _str;
	}

	bool equal_to(const xmlstring& s) {
		if (this == &s) return true;
		if (_str == s._str) return true;
		return (_str && s._str && !strcmp(_str, s._str))
			? true : false;
	}

	const char* c_str(void) {
		return _str;
	}

	const string stdstr(void) {
		return (_str) ? _str : "";
	}

private:
	char* _str;
};

struct xmlnode
{
	string name, content;
	map<string, string> attrs;
	multimap<string, shared_ptr<xmlnode>> children;
	weak_ptr<xmlnode> parents;
	void *userdata = nullptr;
};

} // end of namespace osm_parser1
#endif // __CXX_OSM_PARSER1_XML_H__
/* EOF */
