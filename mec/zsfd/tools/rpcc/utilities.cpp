
#include <time.h>
#include "utilities.h"


#ifndef WIN32
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#else
#include <windows.h>
#include <io.h>
#include <process.h>
#define R_OK 4 /* Test for read permission. */
#define W_OK 2 /* Test for write permission. */
#define X_OK 1 /* Test for execute permission. */
#define F_OK 0 /* Test for existence. */
#endif

#ifdef DJGPP
#include <sys/stat.h>
#endif

extern "C"
{

#include <string.h>

#define R_memset(x, y, z) memset(x, y, z)
#define R_memcpy(x, y, z) memcpy(x, y, z)
#define R_memcmp(x, y, z) memcmp(x, y, z) 

//typedef unsigned long UINT4;//for x32 system only
typedef unsigned int UINT4;//for both x32 x64 : x64 long is 64bit, int is 32 bit;x32 long is 32bit, int is also 32bit
typedef unsigned char *POINTER; 

/* MD5 context. */ 
typedef struct _MD5_CTX
{
	/* state (ABCD) */   

	UINT4 state[4];   

	/* number of bits, modulo 2^64 (lsb first) */    

	UINT4 count[2];

	/* input buffer */ 

	unsigned char buffer[64];  
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char [16], MD5_CTX *); 

/* Constants for MD5Transform routine. */

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21 

static void MD5Transform(UINT4 [4], unsigned char [64]);
static void Encode(unsigned char *, UINT4 *, unsigned int);
static void Decode(UINT4 *, unsigned char *, unsigned int); 

/*

*/
static unsigned char PADDING[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}; 


/* F, G, H and I are basic MD5 functions.
*/
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z))) 

/* ROTATE_LEFT rotates x left n bits.
*/
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n)))) 

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
*/
#define FF(a, b, c, d, x, s, ac) { \
	(a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}
#define GG(a, b, c, d, x, s, ac) { \
	(a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}
#define HH(a, b, c, d, x, s, ac) { \
	(a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}
#define II(a, b, c, d, x, s, ac) { \
	(a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	} 

/* MD5 initialization. Begins an MD5 operation, writing a new context. */
void MD5Init (MD5_CTX *context)
{
	context->count[0] = context->count[1] = 0; 

	/* Load magic initialization constants.*/
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
} 

/* MD5 block update operation. Continues an MD5 message-digest
operation, processing another message block, and updating the
context. */
void MD5Update(MD5_CTX *context,unsigned char * input,unsigned int  inputLen)
{
	unsigned int i, index, partLen;
	
	/* Compute number of bytes mod 64 */
	index = (unsigned int)((context->count[0] >> 3) & 0x3F); 

	/* Update number of bits */
	if((context->count[0] += ((UINT4)inputLen << 3)) < ((UINT4)inputLen << 3))
		context->count[1]++;
	context->count[1] += ((UINT4)inputLen >> 29); 

	partLen = 64 - index;
	/* Transform as many times as possible.
	*/
	if(inputLen >= partLen) 
	{
		R_memcpy((POINTER)&context->buffer[index], (POINTER)input, partLen);
		MD5Transform(context->state, context->buffer); 

		for(i = partLen; i + 63 < inputLen; i += 64 )
			MD5Transform(context->state, &input[i]); 

		index = 0;
	}
	else
		i = 0; 

	/* Buffer remaining input */
	R_memcpy((POINTER)&context->buffer[index], (POINTER)&input[i], inputLen-i);
} 

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
the message digest and zeroizing the context. */
void MD5Final (unsigned char digest[16],MD5_CTX *context)
{
	unsigned char bits[8];
	unsigned int index, padLen; 

	/* Save number of bits */
	Encode(bits, context->count, 8); 

	/* Pad out to 56 mod 64. */
	index = (unsigned int)((context->count[0] >> 3) & 0x3f);
	/*??????????????????padLen??????��??1-64???*/
	padLen = (index < 56) ? (56 - index) : (120 - index);
	MD5Update(context, PADDING, padLen); 

	/* Append length (before padding) */
	MD5Update(context, bits, 8); 

	/* Store state in digest */
	Encode(digest, context->state, 16); 

	/* Zeroize sensitive information. */ 

	R_memset((POINTER)context, 0, sizeof(*context));
} 

/* MD5 basic transformation. Transforms state based on block. */
static void MD5Transform (UINT4 state[4], unsigned char block[64])
{
	UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16]; 

	Decode(x, block, 64); 

	/* Round 1 */
	FF(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
	FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
	FF(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
	FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
	FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
	FF(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
	FF(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
	FF(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
	FF(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
	FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
	FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
	FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
	FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
	FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
	FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
	FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */ 

	/* Round 2 */ 
	GG(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
	GG(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
	GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
	GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
	GG(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
	GG(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
	GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
	GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
	GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
	GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
	GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
	GG(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
	GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
	GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
	GG(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
	GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */ 

	/* Round 3 */ 
	HH(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
	HH(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
	HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
	HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
	HH(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
	HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
	HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
	HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
	HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
	HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
	HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
	HH(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
	HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
	HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
	HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
	HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */ 

	/* Round 4 */
	II(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
	II(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
	II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
	II(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
	II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
	II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
	II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
	II(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
	II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
	II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
	II(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
	II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
	II(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
	II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
	II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
	II(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */ 

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d; 

	/* Zeroize sensitive information. */
	R_memset((POINTER)x, 0, sizeof(x));
} 

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
a multiple of 4. */
static void Encode(unsigned char *output, UINT4 *input,unsigned int  len)
{
	unsigned int i, j; 

	for(i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
} 

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
a multiple of 4. */
static void Decode(UINT4 *output, unsigned char *input,unsigned int  len)
{
	unsigned int i, j; 

	for(i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) |
		(((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
} 

};

namespace utilities
{

int md5Encode(uint128_t& result, void *data, size_t sz)
{
	MD5_CTX md5ctx;

	if (NULL == data || !sz)
		return 1;

	MD5Init(&md5ctx);
	MD5Update(&md5ctx, (unsigned char *)data, sz);
	MD5Final((unsigned char *)&result, &md5ctx);
	
	return 0;
}

string CanonicalizeFileName(string filename)
{
#ifdef WIN32
	char buffer[300];
	::GetFullPathName(filename.c_str(), 300, buffer, NULL);
	return string(buffer);
#endif

#ifdef DJGPP
	char buffer[300];
	_fixpath(filename.c_str(), buffer);
	return string(buffer);
#endif

#ifdef LINUX
	char *ret = canonicalize_file_name(filename.c_str());
	string tmp= "";//??NULL??string??????????????????WIN32????????
	if( NULL != ret)
	{
		tmp = ret;
	}
	free(ret);
	return tmp;
#endif

}

void GetLocalTime(TimeInfo& Info)
{
	//todo: current this function is implemented by using
	// the C standard runtime
	// need to check there is problems?
	time_t t;
	time(&t);

	struct tm *tblock = localtime(&t);

	Info.Second		= tblock->tm_sec;
	Info.Minute		= tblock->tm_min;
	Info.Hour		= tblock->tm_hour;
	Info.MonthDay	= tblock->tm_mday;
	Info.Month		= tblock->tm_mon + 1;
	Info.Year		= tblock->tm_year + 1900;
	Info.Weekday	= tblock->tm_wday;
	Info.YearDay	= tblock->tm_yday;
	Info.bDayLightSaving = (tblock->tm_isdst) ? true : false;
}

bool IsFileExist(const char* pFile)
{
#ifndef WIN32
	if(access(pFile,F_OK) == -1){
#else 
	if(_access(pFile,F_OK) == -1){
#endif
		return false;
	}
	return true;
}

bool get_currentdir(char* buffer, size_t len)
{
#ifndef WIN32
	if( NULL ==  getcwd(buffer,len) )
		return false;
#else 
	if( 0 == GetCurrentDirectory(len,buffer) )
		return false;
#endif
	return true;
}

bool set_currentdir(const char* path)
{
#ifndef WIN32
	int iret = chdir(path);
	if (0 == iret)
		return true;
	else
		return false;
#else
	return SetCurrentDirectory(path) ? true : false;
#endif
}

int createdir(const char* dir)
{
	char buffer[64], *buf, *cur;
	if (NULL == dir || *dir == '\0') {
		return -1;
	}

	size_t len = strlen(dir);
	buf = (len < 64) ? buffer : (char*)alloca(len + 1);
	strcpy(buf, dir);
	cur = buf + 1;

	for (; *cur; ++cur)
	{
		if (*cur == '/')
		{
			*cur = '\0';
			if (access(buf, F_OK) != 0 && mkdir(buf, 0755) == -1)
				return -2;
			*cur = '/';
		}
	}
	if (access(buf, F_OK) != 0 && mkdir(buf, 0755) == -1)
		return -3;
	return 0;
}

bool rename(const char* oldname, const char* newname)
{
	int ret = ::rename(oldname, newname);
	return (!ret) ? true : false;
}

int removedir(const char* dir)
{
	string tmp;
	dirent* ent;

	if (NULL == dir || *dir == '\0') {
		return -1;
	}
	DIR* d = opendir(dir);
	if (NULL == d) {
		return (errno == ENOENT) ? 0 : -2;
	}
	while (NULL != (ent = readdir(d)))
	{
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		tmp = dir;
		tmp += "/";
		tmp += ent->d_name;
		if (ent->d_type == DT_DIR) {
			tmp += '/';
			if (removedir(tmp.c_str())) return -3;
		}
		else if (-1 == remove(tmp.c_str()))
			return -4;
	}
	if (-1 == remove(dir))
		return -5;
	return 0;
}

int uint128_to_hex(uint128_t& result, std::string &str)
{
	char buf[64] = {0};
	sprintf(buf, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		result.Data1,
		result.Data2,
		result.Data3,
		result.Data4[0],
		result.Data4[1],
		result.Data4[2],
		result.Data4[3],
		result.Data4[4],
		result.Data4[5],
		result.Data4[6],
		result.Data4[7]
	);
	str = buf;
	return 0;
}

int upperFirstWord(std::string &str, std::string &result){
	std::string::size_type pos;
	int len = str.length();
	pos = str.find("_",0);
	int start = 0;
	while(pos<len){
		string s=str.substr(start,pos-start);
		s[0]=s[0]-32;
		result +=s;
		start = pos+1;
		pos = str.find("_",pos+1);
		if(pos==-1){
			s=str.substr(start,len);
			s[0]=s[0]-32;
			result +=s;
		}
	}
	return 0;
}

};
