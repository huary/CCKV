//
//  CCDictionary.h
//  CCKVDemo
//
//  Created by yuan on 2020/1/4.
//  Copyright © 2020 yuan. All rights reserved.
//

#ifndef CCDictionary_h
#define CCDictionary_h

#include "CCHashTable.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct CCDictionaryKeyFunc
{
    CCRetainFunc retain;
    CCReleaseFunc release;
    CCEqualFunc equal;
    CCHashFunc  hash;
}CCDictionaryKeyFunc_S;

typedef struct CCDictionaryValueFunc
{
    CCRetainFunc retain;
    CCReleaseFunc release;
    CCEqualFunc equal;
}CCDictionaryValueFunc_S;

typedef CCHashTable_S CCDictionary;
typedef PCCHashTable_S PCCDictionary;

//style 默认为CCHashTableStyleLinear
PCCDictionary CCDictionaryCreate(PCCAllocator_S allocator, const CCU64Ptr *keys, const CCU64Ptr *values, CCCount keyCnt, const CCDictionaryKeyFunc_S *keyFunc, const CCDictionaryValueFunc_S *valueFunc, CCHashTableStyle_E style);

void CCDictionaryDeallocate(PCCDictionary dictionary);

#if defined __cplusplus
};
#endif

#endif /* CCDictionary_h */
