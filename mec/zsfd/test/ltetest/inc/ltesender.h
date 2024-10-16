#ifndef __ZAS_LET_SENDER_TEST_H__
#define __ZAS_LET_SENDER_TEST_H__

#include "utils/buffer.h"

namespace zas {
namespace lte {

using namespace zas::utils;

class ltesender
{
public:
	ltesender();
	virtual ~ltesender();
public:
	int senddata(void);
	int listendata(void);



};

}};	//zas::lte
#endif //__ZAS_LET_SENDER_TEST_H__
