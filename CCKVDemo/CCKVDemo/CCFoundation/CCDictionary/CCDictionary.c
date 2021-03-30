//
//  CCDictionary.c
//  CCKVDemo
//
//  Created by yuan on 2020/1/4.
//  Copyright © 2020 yuan. All rights reserved.
//

#include "CCDictionary.h"

static PCCHashTable_S _CCDictionaryCreate(PCCAllocator_S allocator, const CCDictionaryKeyFunc_S *keyFunc, const CCDictionaryValueFunc_S *valueFunc, CCHashTableStyle_E style)
{
    CCOptionFlag flags = CCHashHaveKey;
    CC_SET_BIT_FIELD(flags, 1, 2, style);
    
    CCHashTableContext_S ctx = {NULL};
    if (valueFunc) {
        ctx.retainValue = valueFunc->retain;
        ctx.releaseValue = valueFunc->release;
        ctx.equalValue = valueFunc->equal;
    }
    if (keyFunc) {
        ctx.retainKey = keyFunc->retain;
        ctx.releaseKey = keyFunc->release;
        ctx.equalKey = keyFunc->equal;
        ctx.hashKey = keyFunc->hash;
    }
    
    PCCHashTable_S ht = CCHashTableCreate(allocator, &ctx, flags);
    return ht;
}

PCCDictionary CCDictionaryCreate(PCCAllocator_S allocator, const CCU64Ptr *keys, const CCU64Ptr *values, CCCount keyCnt, const CCDictionaryKeyFunc_S *keyFunc, const CCDictionaryValueFunc_S *valueFunc, CCHashTableStyle_E style)
{
    PCCDictionary dictionary = _CCDictionaryCreate(allocator, keyFunc, valueFunc, style);
    if (keyCnt > 0) {
        CCHashTableSetCapacity(dictionary, keyCnt);
        for (CCIndex idx = 0; idx < keyCnt ; ++keyCnt) {
            CCHashTableAddValue(dictionary, keys[idx], values[idx]);
        }
    }
    return dictionary;
}

void CCDictionaryDeallocate(PCCDictionary dictionary)
{
    CCHashTableDeallocate(dictionary);
}
