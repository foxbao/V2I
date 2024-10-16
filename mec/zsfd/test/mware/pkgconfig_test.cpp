#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/config/SourcePrefix.h>
#include "mware/pkgconfig.h"
#include "mware/pkgconfig_test.h"
#include "utils/uri.h"
#include "utils/shmem.h"
#include "utils/absfile.h"

namespace zas{
namespace pkgcfgtest{

using namespace std;
using namespace zas::utils;
using namespace zas::mware;

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( pkgconfigtest ,"alltest" );

void 
pkgconfigtest::setUp()
{
}

void 
pkgconfigtest::tearDown()
{
}

void 
pkgconfigtest::testpkgconfig()
{
	// pkgconfig bin uri
	std::string uripkgbin = 
		"file:///zassys/manifest-bin/zas.system.bin";

	const pkgconfig& p = pkgconfig::getdefault();

	// generate pkgconfig bin sharememory name
	std::string pkgsh_name = "zas.system.pkgcfg.area";
	size_t filesz = 0;
	shared_memory* pkgsh = NULL;
	void*	pkgadr = NULL;

	// load bin file
	absfile* afile = absfile_open(uripkgbin.c_str(), "rb+");
	CPPUNIT_ASSERT(NULL != afile);
	
	int ret = afile->seek(0, absfile_seek_end);
	CPPUNIT_ASSERT(0 == ret);

	filesz = afile->getpos();
	ret = afile->seek(0, absfile_seek_set);
	CPPUNIT_ASSERT(0 == ret);

	// create shared memory
	pkgsh = shared_memory::create(pkgsh_name.c_str(),
		(filesz + ZAS_PAGESZ - 1) & ~(ZAS_PAGESZ - 1));
	CPPUNIT_ASSERT(NULL != pkgsh);

	// shared memory is already created
	CPPUNIT_ASSERT(0 != pkgsh->is_creator());

	// load pkg config bin
	pkgadr = pkgsh->getaddr();
	CPPUNIT_ASSERT(NULL != pkgadr);
	afile->read(pkgadr, filesz);
	afile->close();
	afile->release();	
}

void 
pkgconfigtest::testpkgconfig_read()
{	
	const pkgconfig& p = pkgconfig::getdefault();

	uint32_t servicecnt = p.get_service_count();
	for (int i = 0; i < servicecnt; i++)
	{
		pkgcfg_service_info service = p.get_service_by_index(i);
		printf("serivce id %d, service name %s\n", i, service->get_name());
		uint32_t instcnt = service->get_instance_count();
		for (int j = 0; j < instcnt; j++) {
			printf("instance %d, name %s\n", j, 
				service->get_instance_by_index(j));
		}
	}
}

void 
pkgconfigtest::testpkgconfig_compiler()
{
	// const char* json = "file:///home/coder/zsfd/test/mware/manifest.json";
	const char* json = "file:///home/iceberg/zsfd/sys/coreapp/config/manifest.json";
	const char* bin = "file:///home/iceberg/zsfd/sys/coreapp/config/zas.system.bin";
	pkgcfg_compiler* cp = new pkgcfg_compiler(json);
	int ret = cp->compile(bin);
	CPPUNIT_ASSERT(ret == 0);
	delete cp;	

}

}} // end of namespace zas::utilstest