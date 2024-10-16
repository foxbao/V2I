/** @file ota-bundle.cpp
 * implementation of ota bundle operations
 */

#include "inc/otabundle.h"

namespace zas {
namespace mware {

ota_extractor::ota_extractor(const char* filename)
: _packfp(nullptr) {
	if (!filename || !*filename) {
		return;
	}
	_pkgfile = filename;
	_packfp = fopen(filename, "rb");
}

ota_extractor::ota_extractor(FILE* fp)
: _packfp(fp) {
}

ota_extractor::~ota_extractor()
{
	if (_packfp) {
		fclose(_packfp);
		_packfp = nullptr;
	}
}

int ota_extractor::verify(void) { return -ENOTIMPL; }
int ota_extractor::copy_verify(void) { return -ENOTIMPL; }
int ota_extractor::update(
	const char* srcdir, const char* dstdir) {
	return -ENOTIMPL;
}

ota_bundle::ota_bundle(const char* filename)
: ota_extractor(filename)
{
}

int ota_bundle::verify(void)
{
	return 0;
}

}} // end of namespace zas::mware
/* EOF */
