//
//  CCType.h
//  CCKVDemo
//
//  Created by yuan on 2019/12/27.
//  Copyright © 2019 yuan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#ifndef CCType_h
#define CCType_h

#define M_CONCAT_(a, b) a ## b
#define M_CONCAT(a, b) M_CONCAT_(a, b)

#define M_CHECK_(a, ...) a
#define M_CHECK(...) M_CHECK_(__VA_ARGS__)

#define DEF(x) M_CHECK(x)


#if defined(DEBUG)
#define CCAssert(cond, desc, ...)     do { if (!cond) { printf(desc, ##__VA_ARGS__); assert(cond); } } while (0)
#else
#define CCAssert(cond, desc, ...)
#endif

/*
 * bits are numbered from 0 of low to 63 of high
 * F is from
 * N is bit count
 */
#define CC_BIT_FIELD_MASK(F, N)             (((1ULL << (N)) - 1) << (F))
#define CC_GET_BIT_FIELD(V, F, N)           (((V) & CC_BIT_FIELD_MASK(F, N)) >> (F))
#define CC_SET_BIT_FIELD(V, F, N, X)        ((V) = (((V) & ~CC_BIT_FIELD_MASK(F,N)) | (((X) << (F)) & CC_BIT_FIELD_MASK(F, N))))

#define CCCountMax                          ((CCCount)(~0))
#define CCRangeMake(Loc,Len)                ({CCRange _r={.location=Loc,.length=Len};_r;})

//long and pointer are 64bit
#define __LP64__    1
//long long and pointer are 64bit
#define __LLP64__   0
//int,long and pointer are 32bit
#define __ILP32__   0
//long,pointer are 32bit
#define __LP32__    0

typedef int8_t CCByte_t;
typedef uint8_t CCUByte_t;
typedef int16_t CCInt16_t;
typedef uint16_t CCUInt16_t;
typedef int32_t CCInt32_t;
typedef uint32_t CCUInt32_t;
typedef int64_t CCInt64_t;
typedef uint64_t CCUInt64_t;

#if __LP64__
typedef signed long CCInt;
typedef unsigned long CCCount;
typedef unsigned long CCTypeID;
typedef unsigned long CCPtr;
typedef unsigned long CCU64Ptr;
//typedef unsigned long uintptr_t;
typedef unsigned long CCHashCode;
typedef unsigned long CCOptionFlag;

#elif __LLP64__
typedef signed long long CCInt;
typedef unsigned long long CCCount;
typedef unsigned long long CCTypeID;
typedef unsigned long long CCPtr;
typedef unsigned long long CCU64Ptr;
//typedef unsigned long long uintptr_t;
typedef unsigned long long CCHashCode;
typedef unsigned long long CCOptionFlag;

#elif __ILP32__
typedef signed int CCInt;
typedef unsigned int CCCount;
typedef unsigned int CCTypeID;
typedef unsigned int CCPtr;
//为了向下兼容，一最大的为准
typedef unsigned long long CCU64Ptr;
//typedef unsigned long long uintptr_t;
typedef unsigned int CCHashCode;
typedef unsigned int CCOptionFlag;

#elif __LP32__
typedef signed long CCInt;
typedef unsigned long CCCount;
typedef unsigned long CCTypeID;
typedef unsigned long CCPtr;
//为了向下兼容，一最大的为准
typedef unsigned long long CCU64Ptr;
//typedef unsigned long long uintptr_t;
typedef unsigned long CCHashCode;
typedef unsigned long CCOptionFlag;
#endif
typedef CCCount CCIndex;

typedef struct
{
    CCIndex location;
    CCCount length;
}CCRange;

//inline CCRange CCRangeMake(CCIndex loc, CCCount len) {
//    CCRange r;
//    r.location = loc;
//    r.length = len;
//    return r;
//}

static const CCIndex CCNotFound = -1;

typedef CCU64Ptr(*CCRetainFunc)(void *targert, CCU64Ptr value);
typedef void(*CCReleaseFunc)(void *target, CCU64Ptr value);
typedef bool(*CCEqualFunc)(void *target, CCU64Ptr value1, CCU64Ptr value2);
typedef CCHashCode(*CCHashFunc)(void *target, CCU64Ptr key);


#endif /* CCType_h */
