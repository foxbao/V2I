#include <stdio.h>
#include "osm-loader.h"

namespace osm_parser1 {

osm_loader::osm_loader(cmdline_parser& psr)
: _osmfp(nullptr), _buffer(nullptr)
, _filesize(0), _xmlraw_osmhdr_length(0)
, _map(psr)
{
	FILE *fp = fopen(psr.get_srcfile(), "rb");
	if (nullptr == fp) {
		return;
	}

	// calculate the file size
	fseek(fp, 0, SEEK_END);
	_filesize = ftell(fp);

	// allocate the buffer
	_buffer = new char [ buffer_length ];
	if (nullptr == _buffer) {
		fclose(fp); return;
	}

	// save all values
	_osmfp = fp;
	_f.ready = 1;
}

osm_loader::~osm_loader()
{
	if (_osmfp) {
		fclose(_osmfp);
		_osmfp = nullptr;
	}
	if (_buffer) {
		delete [] _buffer;
		_buffer = nullptr;
	}
	_filesize = 0;
	_f.ready = 0;
}

int osm_loader::parse(void)
{
	assert(nullptr != _osmfp);
	fseek(_osmfp, 0, SEEK_SET);

	int total_blk = (_filesize + buffer_length
		- 1) / buffer_length;
	fprintf(stdout, "xml block size: %u KB\n",
		buffer_length / 1024);

	for (int blkcnt = 1; !feof(_osmfp); ++blkcnt)
	{
		if (blkcnt > total_blk) total_blk = blkcnt;
		fprintf(stdout, "parsing block (%d/%d) ... %d%%\r",
			blkcnt, total_blk, blkcnt * 100 / total_blk);
		fflush(stdout);

		char* begin = &_buffer[_xmlraw_osmhdr_length];
		size_t sz = fread(begin, 1, buffer_length -
			_xmlraw_osmhdr_length - 7, _osmfp);
		assert(sz > 0);

		if (!_xmlraw_osmhdr_length) {
			if (calc_xmlraw_osm_header_length(sz)) {
				return -1;
			}
		}

		int realsz = truncate_xmlbuffer(begin, sz);
		if (realsz <= 0) {
			return -2;
		}
		if (XML_SUCCESS != _doc.Parse(_buffer)) {
			return -3;
		}
		if (_map.append(_doc)) {
			return -4;
		}
	}
	fputs("\n", stdout);
	int ret = _map.finalize();
	if (!ret) _f.map_loaded = 1;
	return ret;
}

int osm_loader::truncate_xmlbuffer(char* begin, int sz)
{
	char* start = _buffer;
	char* end = begin + sz;

	int cnt = 0;
	char* last_pos = end;
	while (start < end)
	{
		// check [<]xxx>
		if (*start == '<') {
			if (++start >= end) break;
			if (*start == '/') {
				++start;
				if (--cnt <= 1) {
					// find </xxx[>]
					for (; start < end && *start != '>'; ++start);
					if (start >= end) break;
					++start;
					last_pos = start;
				}
			}
			else if (*start != '?') ++cnt;
		}
		
		// check <xxx[/>]
		else if (*start == '/') {
			if (++start >= end) break;
			if (*start == '>') {
				++start;
				if (--cnt <= 1) {
					last_pos = start;
				}
			}
		}
		else ++start;
	}

	int pos = last_pos - begin;
	if (pos < sz) {		
		fseek(_osmfp, pos - sz, SEEK_CUR);
	}
	if (cnt > 0) {
		// write a </osm> to make the xml
		// buffer valid
		strcpy(last_pos, "</osm>");
	}
	else *last_pos = '\0';
	return pos;
}

int osm_loader::calc_xmlraw_osm_header_length(int sz)
{
	assert(_xmlraw_osmhdr_length == 0);

	char* start = _buffer;
	char* end = _buffer + sz;

	// find the first "<"
	for (; start < end && *start != '<'; ++start);
	if (start >= end) return -1;

	// check if it is <?xml?>
	if (++start >= end) return -2;
	if (*start == '?') {
		start = strstr(++start, "?>");
		if (nullptr == start) return -3;

		// find the first "<" again
		for (; start < end && *start != '<'; ++start);
		if (++start >= end) return -4;
	}

	// check if it is <osm>
	if (strncmp(start, "osm", 3)) return -5;
	start += 3;
	if (start >= end) return -6;

	// find ">"
	for (; start < end && *start != '>'; ++start);
	if (start >= end) return -7;

	// save osm_header length
	if (++start >= end) return -8;
	_xmlraw_osmhdr_length = start - _buffer;

	return 0;
}

} // end of namespace osm_parser1
/* EOF */
