#ifndef		__FORCE_INCLUDE_H__
#define		__FORCE_INCLUDE_H__

#ifdef _HAS_ITERATOR_DEBUGGING
#undef _HAS_ITERATOR_DEBUGGING
#endif

#define _HAS_ITERATOR_DEBUGGING 0

#ifdef USE_DEBUG_NEW
#if defined(_DEBUG)
	#define		_CRTDBG_MAP_ALLOC
	#include	<stdlib.h>
	#include	<crtdbg.h>

	#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
	#define new DEBUG_NEW
#endif
#endif

#if defined(_DEBUG) || defined(TESTING)
	#define LVN_ASSERT(x) { if (!(x)) __debugbreak(); }
#else
	#define LVN_ASSERT(x) (x);
#endif

#define LVN_ALWAYS_ASSERT(msg, x) { if (!(x)) { printf("%s\n", msg); __debugbreak(); } }

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

const int LVN_SUCCESS = 0;
const int LVN_ERR_INVALID_PARAM = 1;
const int LVN_ERR_INVALID_PARAM_SIZE = 2;
const int LVN_ERR_INVALID_PARAM_PTR = 3;


#endif	//	__FORCE_INCLUDE_H__

