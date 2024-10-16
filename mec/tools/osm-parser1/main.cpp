#include <stdio.h>
#include <string>

#include "parser.h"
#include "osm-loader.h"
#include "odr-loader.h"
#include "generator.h"

using namespace std;
using namespace zas::utils;
using namespace osm_parser1;

const char* helpstr[][2] =
{
	{ "--help", "Display this information" },
	{ "--osm-file",					"specify an xml OSM file" },
	{ "--odr-file",					"specify an xml ODR file" },
	{ "--target-dir",				"specify the target directory for generated files" },
	{ "--target-prefix",			"specify the target filename prefix" },
	{ "--config-file",				"specify a configure file for all parameters" },
	{ "--need-statistics",			"show the statistics information" },
	{ "--wcj02",					"using WCJ02 for coordination presentation" },
	{ nullptr, nullptr },
};

void show_help(const char* filename)
{
	const char* raw = filename;
	for (; *raw; ++raw) {
		if (*raw == '/') filename = raw + 1;
	}
	printf("Usage: %s [options]\nOptions\n", filename);
	for (int i = 0; helpstr[i][0]; ++i) {
		printf("%-28s%s\n", helpstr[i][0], helpstr[i][1]);
	}
}

static int generate_from_odr(cmdline_parser& parser)
{
	odr_loader odrldr(parser);
	int ret = odrldr.parse();

	if (ret) {
		fprintf(stderr, "fail in parsing the odr-file"
			" (errid:%d)\n", ret);
		return -1;
	}

	auto& map = odrldr.getmap();
	ret = map.generate();
	if (ret) {
		fprintf(stderr, "\033[41;32merror in generating map data"
			" (errid:%d)\033[0m\n", ret);
		return -2;
	}

	printf("\033[1;32msuccessfully generated from odr-file.\033[0m\n\n");
	return 0;
}

static int generate_from_osm(cmdline_parser& parser)
{
	osm_loader osmldr(parser);	
	int ret = osmldr.parse();
	if (ret) {
		fprintf(stderr, "fail in paring OSM map data"
			" (errid:%d)\n", ret);
		return -2;
	}

	osm_map* map = osmldr.get_osm_map();
	if (nullptr == map) {
		fprintf(stderr, "unexpected error: probably broken map.\n");
		return -3;
	}
	ret = map->generate();
	if (ret) {
		fprintf(stderr, "\033[41;32merror in generating map data"
			"(errid:%d)\033[0m\n", ret);
		return -3;
	}
	printf("\033[1;32msuccessfully generated from osm-file.\033[0m\n\n");
	return 0;
}

int main(int argc, char* argv[])
{
	int ret;
	if (argc == 1) {
		show_help(argv[0]);
		return 0;
	}

	cmdline_parser parser;
	if (parser.parse()) {
		return -1;
	}

	auto srctype = parser.get_srcfile_type();
	if (srctype == srcfile_type_osm) {
		ret = generate_from_osm(parser);
	}
	else if (srctype == srcfile_type_odr) {
		ret = generate_from_odr(parser);
	}
	else {
		printf("unknown source file format.\n");
		return 1;
	}
	return 0;
}

/* EOF */