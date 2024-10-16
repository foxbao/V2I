#ifndef __CXX_RPCC_UTILITIES_H__
#define __CXX_RPCC_UTILITIES_H__

#include <string>

using namespace std;

namespace utilities
{

typedef struct uint128
{
	//unsigned long Data1;//for x32 system only
	unsigned int Data1;//for both x32 x64 : x64 long is 64bit, int is 32 bit;x32 long is 32bit, int is also 32bit
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
}
uint128_t;

// generate the md5 code
int md5Encode(uint128_t& result, void *data, size_t sz);

// get the full path of a filename
string CanonicalizeFileName(string filename);

struct TimeInfo
{
	int Second;
	int Minute;
	int Hour;
	int MonthDay;
	int Month;
	int Year;
	int Weekday;
	int YearDay;
	bool bDayLightSaving;
};

// get the local time
void GetLocalTime(TimeInfo& Info);

//
bool IsFileExist(const char* pFile);

bool get_currentdir(char* pbuf, size_t len);
bool set_currentdir(const char* path);

int createdir(const char* dir);
int removedir(const char* dir);
bool rename(const char* oldname, const char* newname);
int uint128_to_hex(uint128_t& result, std::string &str);
int upperFirstWord(std::string &str, std::string &result);

};
#endif
