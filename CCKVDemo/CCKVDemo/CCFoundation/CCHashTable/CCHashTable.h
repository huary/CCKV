//
//  CCHashTable.h
//  CCKVDemo
//
//  Created by yuan on 2019/11/23.
//  Copyright © 2019 yuan. All rights reserved.
//


#ifndef CCHashTable_h
#define CCHashTable_h

#include "CCType.h"
#include "CCBase.h"

#if defined __cplusplus
extern "C" {
#endif

//typedef CCU64Ptr(*CCRetainFunc)(void *targert, CCU64Ptr value);
//typedef void(*CCReleaseFunc)(void *target, CCU64Ptr value);
//typedef bool(*CCEqualFunc)(void *target, CCU64Ptr value1, CCU64Ptr value2);
//typedef CCHashCode(*CCHashFunc)(void *target, CCU64Ptr key);


typedef struct _CCHashTable CCHashTable_S;
typedef struct _CCHashTable *PCCHashTable_S;
//返回false就停止enumerate
typedef bool(*CCHashTableEnumerator)(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value);
//返回需要rebuild后的capacity，但是返回的capacity不能小于CCHashTableGetCount的大小，否则无效
typedef CCCount (*CCHashTableCapacityUpdateFunc)(PCCHashTable_S ht);

typedef enum {
    CCHashHaveKey    = (1 << 0),
}CCHashTableOptionFlag_E;

typedef enum {
    //经过大量数据测试发现Linear的效率要高于L2Hash，由此可见L2Hash的扩展策略还需要在优化
    CCHashTableStyleLinear   = 0,
    CCHashTableStyleL2Hash   = 1,
}CCHashTableStyle_E;

typedef struct CCHashTableBucket {
    CCIndex idx;
    CCU64Ptr key;
    CCU64Ptr value;
    bool keyEqual;
}CCHashTableBucket_S;

typedef struct CCHashTableContext {
    CCRetainFunc retainValue;
    CCReleaseFunc releaseValue;
    
    CCRetainFunc retainKey;
    CCReleaseFunc releaseKey;
    
    CCEqualFunc equalValue;
    CCEqualFunc equalKey;
    CCHashFunc hashKey;
    
    CCHashTableCapacityUpdateFunc capacityUpdate;    
}CCHashTableContext_S;

/*
 *flags的低1位标识是否CCHashTableOptionFlag_E
 *flags的低2-3位标识CCHashStyle_E
 */
PCCHashTable_S CCHashTableCreate(PCCAllocator_S allocator, CCHashTableContext_S *ctx, CCOptionFlag flags);
//返回当前的桶的个数
CCCount CCHashTableGetBuckets(PCCHashTable_S ht);
//返回当前的capacity的大小
CCCount CCHashTableGetCapacity(PCCHashTable_S ht);
//设置当前的capacity的大小
void CCHashTableSetCapacity(PCCHashTable_S ht, CCCount capacity);
//返回已经有的key,value的个数
CCCount CCHashTableGetCount(PCCHashTable_S ht);

//需要判断返回结构的keyEqual是否为真
CCHashTableBucket_S CCHashTableFindBucket(PCCHashTable_S ht, CCU64Ptr key);
//是否包含有该key
bool CCHashTableContainsKey(PCCHashTable_S ht, CCU64Ptr key);
//返回是否包含了该key对应的value
bool CCHashTableGetValueOfKeyIfPresent(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr *value);
//key不应该存在于原来的
bool CCHashTableAddValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value);
//key需要存在于原来的
bool CCHashTableReplaceValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value);
//如果key不存在，则add，若存在，则replace
bool CCHashTableSetValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value);
//移除存在的key
void CCHashTableRemoveValue(PCCHashTable_S ht, CCU64Ptr key);
//清除所有的key和value
void CCHashTableRemoveAllValues(PCCHashTable_S ht);
//枚举
void CCHashTableEnumerate(PCCHashTable_S ht, CCHashTableEnumerator enumerator);
//释放create的hashTable
void CCHashTableDeallocate(PCCHashTable_S ht);

#if defined __cplusplus
};
#endif
#endif /* CCHashTable_h */
