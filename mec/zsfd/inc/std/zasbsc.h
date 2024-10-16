/** @file zasbsc.h
 * Definition of all basic things for zas
 */

#ifndef __CXX_ZAS_BASIC_H__
#define __CXX_ZAS_BASIC_H__

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

// check architecture
#if defined(__x86_64__) || defined(_M_X64)
#	define zas_x86_64		(1)
#	define zas_i386			(0)
#	define zas_arm64		(0)
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#	define zas_x86_64		(0)
#	define zas_i386			(1)
#	define zas_arm64		(0)
#elif defined(__aarch64__) || defined(_M_ARM64)
#	define zas_x86_64		(0)
#	define zas_i386			(0)
#	define zas_arm64		(1)
#endif

// zas common error codes
#define EBADPARM		(1000)
#define EEXISTS			(1001)
#define EINVALID		(1002)		// invalid parameters or values
#define ELOGIC			(1003)
#define EEOF			(1004)
#define ENOTEXISTS		(1005)
#define ETIMEOUT		(1006)
#define ENOTHANDLED		(1007)
#define EINTRUPT		(1008)
#define ENOTREADY		(1009)
#define ENOTSUPPORT		(1010)
#define ENOTAVAIL		(1011)
#define ENOTALLOWED		(1012)
#define EBADOBJ			(1013)
#define EUNRECOGNIZED	(1014)
#define ENOMEMORY		(1015)
#define ETOOMANYITEMS	(1016)
#define ENOTFOUND		(1017)
#define EFAILTOLOAD		(1018)
#define ECORRUPTTED		(1019)
#define EDATANOTAVAIL	(1020)
#define ETOOLONG		(1021)
#define EBADFORMAT		(1022)
#define ENOTIMPL		(1023)
#define EACCESS			(1024)
#define EOUTOFOSCOPE	(1025)
#define ELOSS			(1026)
#define ETERMINATE		(1027)
#define EDISCARD		(1028)

#define PACKED	__attribute__((packed))
#define zas_interface	struct
#define ZAS_PAGESZ		(4096)

/* definition of fatal error string */
#define fatal_error_str(modname)	\
	#modname": fatal error at " __FILE__" <ln:%u>: \"%s\", abort!\n"

/*
 * configuration parameters mapping area
 * process specific details will be mapped
 */
#define CONFIG_MAPPING_BASE		(0x800000)

/**
 * Compile-time computation of number of items in a hardcoded array.
 *
 * @param a the array being measured.
 * @return the number of items hardcoded into the array.
 */
#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

/**
 * Returns the smaller of two values.
 *
 * @param x the first item to compare.
 * @param y the second item to compare.
 * @return the value that evaluates to lesser than the other.
 */
#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

/**
 * Returns the bigger of two values.
 *
 * @param x the first item to compare.
 * @param y the second item to compare.
 * @return the value that evaluates to more than the other.
 */
#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif

#define zas_ancestor(anc, member)	\
	((anc*)(((size_t)this) - ((size_t)(&((anc*)0)->member))))
#define zas_offsetof(type, memb) __builtin_offsetof(type, memb)
#define zas_container_of(ptr, sample, member)	\
	(__typeof__(sample))((char *)(ptr) -		\
		__builtin_offsetof(__typeof__(*sample), member))

// uint128 definition for uuid
struct uint128_t
{
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	uint8_t data4[8];
} PACKED;

union guid_t
{
	uint8_t uuid[16];
	uint128_t data;
};

// float zero check
#define EPSILON		0.00001
#define float_is0(v) ((v) < EPSILON && (v) > -EPSILON)
#define float_equal(a, b) (fabs((a) - (b)) < EPSILON)

// evil constructor
#define ZAS_DISABLE_EVIL_CONSTRUCTOR(clazz)	\
private:	\
	clazz(const clazz&);	\
	void operator=(const clazz&)

#define bug(...)

#define ZAS_PROF	1

#define zas_downcast(base, derive, ptr)	\
	((derive*)(((size_t)(ptr)) - (size_t)((base*)((derive*)0))))

#define likely(x)		__builtin_expect(!!(x), 1)     
#define unlikely(x)		__builtin_expect(!!(x), 0)

#endif /* __CXX_ZAS_BASIC_H__ */
/* EOF */
