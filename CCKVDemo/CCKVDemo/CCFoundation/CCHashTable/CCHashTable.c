//
//  CCHashTable.c
//  CCKVDemo
//
//  Created by yuan on 2019/11/23.
//  Copyright © 2019 yuan. All rights reserved.
//

#include "CCHashTable.h"
#import <libkern/OSAtomic.h>

#define _HASH_VALUE_IDX         (0)

#define _HASH_EMPTY_VALUE_IDX        (0)
#define _HASH_DELETE_VALUE_IDX       (1)

#define _SPECAIL_BUCKETS_CNT         (2)

#define _MAX_BUCKETS_CNT             (63)

#define _HASH_STYLE_BIT_OFFSET          (1)
#define _HASH_STYLE_BIT_MASK            (0X3)

//typedef enum {
//    //key的值为0，
//    _CCHashTableSpecialKeyTypeEmpty          = (1 << 0),
//    //key的值为~0，
//    _CCHashTableSpecialKeyTypeDelete         = (1 << 1),
//}_CCHashTableSpecialKeyType_E;

static const CCU64Ptr _CCHashTableEmptyValue_s = 0;
static const CCU64Ptr _CCHashTableDeleteValue_s = ~_CCHashTableEmptyValue_s;

#define _IS_HASHTABLE_VALID_VAL(VAL)                ((VAL) != _CCHashTableEmptyValue_s && (VAL) != _CCHashTableDeleteValue_s)
#define _CHECK_HASHTABLE_DEALLOCATED(HT, RET)       if ((HT) == NULL /*|| (HT)->bits.deallocated*/) return RET;

#define _CCHASHTABLE_GET_KEY(HT, IDX, DEF)          ((HT)->bits.haveKey ? (HT)->nodes[(HT)->bits.keyIdx][IDX].value : DEF)

#define _CCHASHTABLE_GET_TYPE(HT, IDX, DEF)         ((HT)->bits.hashStyle == CCHashTableStyleL2Hash ? ((uint8_t*)(HT)->nodes[(HT)->bits.typeIdx])[IDX] : DEF)
#define _CCHASHTABLE_SET_TYPE(HT, IDX, X)           if ((HT)->bits.hashStyle == CCHashTableStyleL2Hash) ((uint8_t*)(HT)->nodes[(HT)->bits.typeIdx])[IDX] = X;

#define _CCHASHTABLE_SUB_HASHTABLE(HT, IDX)         (_CCHASHTABLE_GET_TYPE(HT, IDX, _CCHashTableValueTypeNull) == _CCHashTableValueTypeHashTable ? (PCCHashTable_S)HT->nodes[_HASH_VALUE_IDX][IDX].value : NULL)

#define _CCHASHTABLE_HAVE_EMPTY_KEY(HT)             CC_GET_BIT_FIELD(HT->bits.specialType, 0, 1)
#define _CCHASHTABLE_HAVE_DELETE_KEY(HT)            CC_GET_BIT_FIELD(HT->bits.specialType, 1, 1)
#define _CCHASHTABLE_HAVE_SPECIAL_KEY(HT)           CC_GET_BIT_FIELD(HT->bits.specialType, 0, 2)
#define _CCHASHTABLE_SET_EMPTY_KEY(HT, X)           CC_SET_BIT_FIELD(HT->bits.specialType, 0, 1, X)
#define _CCHASHTABLE_SET_DELETE_KEY(HT, X)          CC_SET_BIT_FIELD(HT->bits.specialType, 1, 1, X)

typedef enum {
    _CCHashTableValueTypeNull       = 0,
    _CCHashTableValueTypeNormal     = 1,
    _CCHashTableValueTypeHashTable  = 2,
}_CCHashTableValueType_E;

static const CCCount _CCHashTableBuckets[_MAX_BUCKETS_CNT] = {
    0, 3, 7, 13, 23, 41, 71, 127, 191, 251, 383, 631, 1087, 1723,
    2803, 4523, 7351, 11959, 19447, 31231, 50683, 81919, 132607,
    214519, 346607, 561109, 907759, 1468927, 2376191, 3845119,
    6221311, 10066421, 16287743, 26354171, 42641881, 68996069,
    111638519, 180634607, 292272623, 472907251,
#if __LP64__
    765180413UL, 1238087663UL, 2003267557UL, 3241355263UL, 5244622819UL,
#if 0
    8485977589UL, 13730600407UL, 22216578047UL, 35947178479UL,
    58163756537UL, 94110934997UL, 152274691561UL, 246385626107UL,
    398660317687UL, 645045943807UL, 1043706260983UL, 1688752204787UL,
    2732458465769UL, 4421210670577UL, 7153669136377UL,
    11574879807461UL, 18728548943849UL, 30303428750843UL
#endif
#endif
};

static const CCCount _CCHashTableCapacities[_MAX_BUCKETS_CNT] = {
    0, 3, 6, 11, 19, 32, 52, 85, 118, 155, 237, 390, 672, 1065,
    1732, 2795, 4543, 7391, 12019, 19302, 31324, 50629, 81956,
    132580, 214215, 346784, 561026, 907847, 1468567, 2376414,
    3844982, 6221390, 10066379, 16287773, 26354132, 42641916,
    68996399, 111638327, 180634415, 292272755,
#if __LP64__
    472907503UL, 765180257UL, 1238087439UL, 2003267722UL, 3241355160UL,
#if 0
    5244622578UL, 8485977737UL, 13730600347UL, 22216578100UL,
    35947178453UL, 58163756541UL, 94110935011UL, 152274691274UL,
    246385626296UL, 398660317578UL, 645045943559UL, 1043706261135UL,
    1688752204693UL, 2732458465840UL, 4421210670552UL,
    7153669136706UL, 11574879807265UL, 18728548943682UL
#endif
#endif
};

static bool _CCHashTableAddValidValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value, bool retain);
static bool _CCHashTableReplaceValidValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value, bool retain);


typedef union {
    CCU64Ptr value;
}_CCHashNode;

struct _CCHashTable {
    CCBase_S base;
    struct {
        uint8_t currBucketsIdx:7;
        uint8_t haveKey:1;
        uint8_t keyIdx:1;//it is 1 if have key
//        uint8_t deallocated:1;
        uint8_t specialType:2;
        uint8_t hashStyle:2;
        uint8_t typeIdx:2; //it is 2 if hashStyle == CCHashTableStyleL2Hash
    } bits;
    
    CCCount usedCnt;
    CCCount deleteCnt;
    CCHashTableContext_S ctx;
    _CCHashNode *specialNodes;
    _CCHashNode *nodes[1];
};

static inline CCTypeID _CCHashTableGetTypeID(void) {
    return 0;
}

static inline CCIndex _CCHashTableGetBucketsIndexForCapacity(PCCHashTable_S ht, CCCount capacity)
{
    for (CCIndex idx = 0; idx < _MAX_BUCKETS_CNT; ++idx) {
        if (capacity <= _CCHashTableCapacities[idx]) {
            return idx;
        }
    }
    return 0;
}

static inline CCU64Ptr _CCHashTableRetainValue(PCCHashTable_S ht, CCU64Ptr value)
{
    CCRetainFunc f = ht->ctx.retainValue;
    if (f) {
        return f(ht, value);
    }
    return value;
}

static inline CCU64Ptr _CCHashTableRetainKey(PCCHashTable_S ht, CCU64Ptr key)
{
    CCRetainFunc f = ht->ctx.retainKey;
    if (f) {
        return f(ht, key);
    }
    return key;
}

static inline void _CCHashTableReleaseValue(PCCHashTable_S ht, CCU64Ptr value)
{
    CCReleaseFunc f = ht->ctx.releaseValue;
    if (f) {
        f(ht, value);
    }
}

static inline void _CCHashTableReleaseKey(PCCHashTable_S ht, CCU64Ptr key)
{
    CCReleaseFunc f = ht->ctx.releaseKey;
    if (f) {
        f(ht, key);
    }
}

static inline bool _CCHashTableEqualValue(PCCHashTable_S ht, CCU64Ptr value1, CCU64Ptr value2)
{
    CCEqualFunc f = ht->ctx.equalValue;
    if (f) {
        return f(ht, value1, value2);
    }
    return (value1 == value2);
}

static inline bool _CCHashTableEqualKey(PCCHashTable_S ht, CCU64Ptr key1, CCU64Ptr key2)
{
    CCEqualFunc f = ht->ctx.equalKey;
    if (f) {
        return f(ht, key1, key2);
    }
    return (key1 == key2);
}

static inline CCHashCode _CCHashTableHashKey(PCCHashTable_S ht, CCU64Ptr key)
{
    CCHashFunc f = ht->ctx.hashKey;
    if (f) {
        return f(ht, key);
    }
    return (CCHashCode)key;
}

static inline void _CCHashTableSetValue(PCCHashTable_S ht, CCIndex idx, CCU64Ptr value, bool retain)
{
    _CCHashNode *node = &(ht->nodes[_HASH_VALUE_IDX][idx]);
    CCU64Ptr oldValue = node->value;
    node->value = retain ? _CCHashTableRetainValue(ht, value) : value;
    _CCHashTableReleaseValue(ht, oldValue);
}

static inline CCU64Ptr _CCHashTableGetSpecialValue(PCCHashTable_S ht, CCIndex idx)
{
    return ht->specialNodes[idx].value;
}

static inline void _CCHashTableSetSpecialValue(PCCHashTable_S ht, CCIndex idx, CCU64Ptr value, bool retain)
{
    _CCHashNode *node = &(ht->specialNodes[idx]);
    CCU64Ptr oldValue = node->value;
    node->value = retain ? _CCHashTableRetainValue(ht, value) : value;
    _CCHashTableReleaseValue(ht, oldValue);
}

static inline void _CCHashTableSetKey(PCCHashTable_S ht, CCIndex idx, CCU64Ptr key, bool retain)
{
    if (!ht->bits.haveKey) {
        return;
    }
    _CCHashNode *node = &(ht->nodes[ht->bits.keyIdx][idx]);
    CCU64Ptr oldKey = node->value;
    node->value = retain ? _CCHashTableRetainKey(ht, key) : key;
    _CCHashTableReleaseKey(ht, oldKey);
}


static CCIndex _CCHashTableFindBucketIndex(PCCHashTable_S ht, CCU64Ptr key, CCHashCode keyHash, bool rebuild, bool *equal)
{
    CCCount bucketCnt = CCHashTableGetBuckets(ht);
    if (bucketCnt <= 0) {
        return CCNotFound;
    }
    CCHashCode hash = keyHash;
    if (rebuild) {
        hash = keyHash ? keyHash : _CCHashTableHashKey(ht, key);
    }
    else {
        hash = _CCHashTableHashKey(ht, key);
    }
    CCIndex probe = hash % bucketCnt;
    _CCHashNode *keyNodes = ht->bits.haveKey ? ht->nodes[ht->bits.keyIdx] : ht->nodes[_HASH_VALUE_IDX];
    if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        CCU64Ptr currKey = keyNodes[probe].value;
        _CCHashTableValueType_E type = _CCHASHTABLE_GET_TYPE(ht, probe, _CCHashTableValueTypeNull);
        if (equal) {
            *equal = false;
            if (type == _CCHashTableValueTypeNormal) {
                if (currKey == key || _CCHashTableEqualKey(ht, currKey, key)) {
                    *equal = true;
                }
            }
        }
        return probe;
    }
    CCIndex deleteIdx = CCNotFound;
    for (CCIndex idx = 0; idx < bucketCnt; ++idx) {
        CCU64Ptr currKey = keyNodes[probe].value;
        if (currKey == _CCHashTableEmptyValue_s) {
            
            CCIndex find = CCNotFound == deleteIdx ? probe : deleteIdx;
            
            return find;
        }
        else {
            if (!rebuild) {
                if (currKey == _CCHashTableDeleteValue_s) {
                    if (CCNotFound == deleteIdx) {
                        deleteIdx = probe;
                    }
                }
                else {
                    if (currKey == key || _CCHashTableEqualKey(ht, currKey, key)) {
                        if (equal) {
                            *equal = true;
                        }
                        return probe;
                    }
                }
            }
        }
        probe += 1;
        if (probe >= bucketCnt) {
            probe -= bucketCnt;
        }
    }
    return deleteIdx;
}

static void _CCHashTableAddValueAction(PCCHashTable_S ht, CCIndex bktIdx, CCU64Ptr key, CCU64Ptr value, bool retain)
{
    _CCHashTableValueType_E type = _CCHASHTABLE_GET_TYPE(ht, bktIdx, _CCHashTableValueTypeNull);
    if (type == _CCHashTableValueTypeHashTable) {
        PCCHashTable_S tmp = (PCCHashTable_S)ht->nodes[_HASH_VALUE_IDX][bktIdx].value;
        _CCHashTableAddValidValue(tmp, key, value, retain);
    }
    else if (type == _CCHashTableValueTypeNormal) {
        CCU64Ptr oldValue = ht->nodes[_HASH_VALUE_IDX][bktIdx].value;
        CCU64Ptr oldKey = _CCHASHTABLE_GET_KEY(ht, bktIdx, value);
        CCOptionFlag flags = 0;
        CC_SET_BIT_FIELD(flags, 0, 1, ht->bits.haveKey);
        CC_SET_BIT_FIELD(flags, 1, 2, CCHashTableStyleLinear);
        PCCHashTable_S tmp = CCHashTableCreate(ht->base.allocator, &ht->ctx, flags);
        _CCHashTableAddValidValue(tmp, oldKey, oldValue, false);
        _CCHashTableAddValidValue(tmp, key, value, retain);
        
        ht->nodes[_HASH_VALUE_IDX][bktIdx].value = (CCU64Ptr)tmp;
        _CCHASHTABLE_SET_TYPE(ht, bktIdx, _CCHashTableValueTypeHashTable);
    }
    else {
        _CCHashTableSetValue(ht, bktIdx, value, retain);
        if (ht->bits.haveKey) {
            _CCHashTableSetKey(ht, bktIdx, key, retain);
        }
        _CCHASHTABLE_SET_TYPE(ht, bktIdx, _CCHashTableValueTypeNormal);
    }
    ++ht->usedCnt;
}

//newItemCnt可以为0, >0, <0
static void _CCHashTableRebuild(PCCHashTable_S ht, CCInt newItemCnt)
{
    CCIndex newBucketsIdx = ht->bits.currBucketsIdx;
    if (newItemCnt) {
        CCCount newCapacity = ht->usedCnt + newItemCnt;
        //如果newItem为负数时,TODO
        if (newItemCnt < 0 && ht->usedCnt < -newItemCnt) {
            newCapacity = 0;
        }
        else if (newItemCnt > 0 && newCapacity < ht->usedCnt) {
            newCapacity = CCCountMax;
        }
        newBucketsIdx = _CCHashTableGetBucketsIndexForCapacity(ht, newCapacity);
    }
    if (newBucketsIdx == ht->bits.currBucketsIdx && ht->deleteCnt == 0) {
        return;
    }
    
    CCCount newBuckets = _CCHashTableBuckets[newBucketsIdx];
    if (newBuckets < ht->usedCnt) {
        return;
    }
    CCCount oldBuckets = _CCHashTableBuckets[ht->bits.currBucketsIdx];
    
    CCCount size = 0;
    _CCHashNode *newValues = NULL, *newKeys = NULL, *newTypes = NULL;
    if (newBuckets > 0) {
        size = newBuckets * sizeof(_CCHashNode);
        newValues = CCAllocatorAllocate(ht->base.allocator, size);
        memset(newValues, 0, size);
        if (ht->bits.haveKey) {
            newKeys = CCAllocatorAllocate(ht->base.allocator, size);
            memset(newKeys, 0, size);
        }
        if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
            newTypes = CCAllocatorAllocate(ht->base.allocator, newBuckets * sizeof(uint8_t));
            memset(newTypes, 0, newBuckets);
        }
    }
    _CCHashNode *oldValues = ht->nodes[_HASH_VALUE_IDX];
    ht->nodes[_HASH_VALUE_IDX] = newValues;
    
    _CCHashNode *oldKeys = NULL;
    if (ht->bits.haveKey) {
        CCIndex keyIdx = ht->bits.keyIdx;
        oldKeys = ht->nodes[keyIdx];
        ht->nodes[keyIdx] = newKeys;
    }
    
    uint8_t *oldTypes = NULL;
    if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        oldTypes = (uint8_t*)ht->nodes[ht->bits.typeIdx];
        ht->nodes[ht->bits.typeIdx] = newTypes;
    }
    ht->bits.currBucketsIdx = newBucketsIdx;
    
    for (CCIndex i = 0; i < oldBuckets; ++i) {
        _CCHashNode *node = &(oldValues[i]);
        CCU64Ptr value = node->value;
        CCU64Ptr key = value;
        if (oldKeys) {
            key = oldKeys[i].value;
        }
        if (!_IS_HASHTABLE_VALID_VAL(key)) {
            continue;
        }
        
        if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
            _CCHashTableValueType_E type = _CCHashTableValueTypeNormal;
            if (oldTypes) {
                type = oldTypes[i];
            }
            if (type == _CCHashTableValueTypeHashTable) {
                PCCHashTable_S tmp = (PCCHashTable_S)value;
                CCCount bktCnt = _CCHashTableBuckets[tmp->bits.currBucketsIdx];
                for (CCIndex j = 0; j < bktCnt; ++j) {
                    CCU64Ptr valueTmp = tmp->nodes[_HASH_VALUE_IDX][j].value;
                    tmp->nodes[_HASH_VALUE_IDX][j].value = _CCHashTableEmptyValue_s;
                    CCU64Ptr keyTmp = valueTmp;
                    if (tmp->bits.haveKey) {
                        keyTmp = tmp->nodes[tmp->bits.keyIdx][j].value;
                        tmp->nodes[tmp->bits.keyIdx][j].value = _CCHashTableEmptyValue_s;
                    }
                    if (!_IS_HASHTABLE_VALID_VAL(keyTmp)) {
                        continue;
                    }
                    
                    CCIndex bktIdx = _CCHashTableFindBucketIndex(ht, keyTmp, 0, true, NULL);
                    if (bktIdx != CCNotFound) {
                        _CCHashTableAddValueAction(ht, bktIdx, keyTmp, valueTmp, false);
                    }
                }
                CCHashTableDeallocate(tmp);
                continue;
            }
        }
        
        CCIndex bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, true, NULL);
        if (bktIdx != CCNotFound) {
            _CCHashTableAddValueAction(ht, bktIdx, key, value, false);
        }
    }
        
    CCAllocatorDeallocate(ht->base.allocator, oldValues);
    CCAllocatorDeallocate(ht->base.allocator, oldKeys);
    CCAllocatorDeallocate(ht->base.allocator, oldTypes);
}

static bool _checkRebuild(PCCHashTable_S ht)
{
    CCCount curCapacity = _CCHashTableCapacities[ht->bits.currBucketsIdx];
    CCCount newItemCnt = 1;
    bool shouldRebuild = curCapacity < ht->usedCnt + newItemCnt;
    if (ht->ctx.capacityUpdate) {
        CCCount capacity = ht->ctx.capacityUpdate(ht);
        if (capacity >= ht->usedCnt) {
            newItemCnt = capacity - ht->usedCnt;
            shouldRebuild = true;
        }
    }
    if (shouldRebuild) {
        _CCHashTableRebuild(ht, newItemCnt);
    }
    return shouldRebuild;
}

static inline void _checkAllocateSpecialNode(PCCHashTable_S ht)
{
    if (ht->specialNodes == NULL) {
        ht->specialNodes = CCAllocatorAllocate(ht->base.allocator, _SPECAIL_BUCKETS_CNT  * sizeof(_CCHashNode));
    }
}

//这里是key不能存在
static void _CCHashTableAddValue(PCCHashTable_S ht, CCIndex bktIdx, CCU64Ptr key, CCU64Ptr value, bool retain)
{
//    CCCount curCapacity = _CCHashTableCapacities[ht->bits.currBucketsIdx];
//    CCCount newItemCnt = 1;
//    bool shouldRebuild = curCapacity < ht->usedCnt + newItemCnt;
//    if (ht->ctx.capacityUpdate) {
//        CCCount capacity = ht->ctx.capacityUpdate(ht);
//        if (capacity >= ht->usedCnt) {
//            newItemCnt = capacity - ht->usedCnt;
//            shouldRebuild = true;
//        }
//    }
//    if (shouldRebuild) {
//        _CCHashTableRebuild(ht, newItemCnt);
//        bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, true, NULL);
//    }
    bool rebuild = _checkRebuild(ht);
    if (rebuild) {
        bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, true, NULL);
    }
    if (bktIdx == CCNotFound) {
        return;
    }
    
    _CCHashTableAddValueAction(ht, bktIdx, key, value, retain);
}

static bool _CCHashTableAddValidValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value, bool retain)
{
    bool equal = false;
    CCIndex bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, false, &equal);
    if (equal == false && bktIdx != CCNotFound && ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        PCCHashTable_S tmp = _CCHASHTABLE_SUB_HASHTABLE(ht, bktIdx);
        equal = CCHashTableContainsKey(tmp, key);
    }
    if (equal) {
        return false;
    }
    _CCHashTableAddValue(ht, bktIdx, key, value, retain);
    return true;
}

//这里的Key必须存在
static void _CCHashTableReplaceValue(PCCHashTable_S ht, CCIndex bktIdx, CCU64Ptr key, CCU64Ptr value, bool retain)
{
    if (bktIdx == CCNotFound) {
        return;
    }
    _CCHashTableValueType_E type = _CCHASHTABLE_GET_TYPE(ht, bktIdx, _CCHashTableValueTypeNull);
    if (type == _CCHashTableValueTypeHashTable) {
        PCCHashTable_S tmp = (PCCHashTable_S)ht->nodes[_HASH_VALUE_IDX][bktIdx].value;
        _CCHashTableReplaceValidValue(tmp, key, value, retain);
    }
    else {
        _CCHashTableSetValue(ht, bktIdx, value, retain);
        if (ht->bits.haveKey) {
            _CCHashTableSetKey(ht, bktIdx, key, retain);
        }
        _CCHASHTABLE_SET_TYPE(ht, bktIdx, _CCHashTableValueTypeNormal);
    }
}

static bool _CCHashTableReplaceValidValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value, bool retain)
{
    bool equal = false;
    CCIndex bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, false, &equal);
    if (equal == false && bktIdx != CCNotFound && ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        PCCHashTable_S tmp = _CCHASHTABLE_SUB_HASHTABLE(ht, bktIdx);
        equal = CCHashTableContainsKey(tmp, key);
    }
    if (!equal) {
        return false;
    }
    _CCHashTableReplaceValue(ht, bktIdx, key, value, retain);
    return true;
}

static bool _CCHashTableSetValidValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value, bool retain)
{
    bool equal = false;
    CCIndex bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, false, &equal);
    if (equal == false && bktIdx != CCNotFound && ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        PCCHashTable_S tmp = _CCHASHTABLE_SUB_HASHTABLE(ht, bktIdx);
        equal = CCHashTableContainsKey(tmp, key);
    }
    if (equal) {
        _CCHashTableReplaceValue(ht, bktIdx, key, value, retain);
    }
    else {
        _CCHashTableAddValue(ht, bktIdx, key, value, retain);
    }
    return true;
}

static void _CCHashTableRemoveValue(PCCHashTable_S ht, CCIndex bktIdx)
{
    if (bktIdx == CCNotFound) {
        return;
    }
    _CCHashTableSetValue(ht, bktIdx, _CCHashTableDeleteValue_s, false);
    if (ht->bits.haveKey) {
        _CCHashTableSetKey(ht, bktIdx, _CCHashTableDeleteValue_s, false);
    }
    --ht->usedCnt;
    ++ht->deleteCnt;
    bool rebuild = (ht->bits.currBucketsIdx > 2 && ht->usedCnt < _CCHashTableCapacities[ht->bits.currBucketsIdx - 2]);
    CCCount newItemCnt = -1;
    if (ht->ctx.capacityUpdate) {
        CCCount capacity = ht->ctx.capacityUpdate(ht);
        if (capacity >= ht->usedCnt) {
            newItemCnt = capacity - ht->usedCnt;
            rebuild = true;
        }
    }
    if (rebuild) {
        _CCHashTableRebuild(ht, newItemCnt);
        return;
    }
    CCCount bktCnt = _CCHashTableBuckets[ht->bits.currBucketsIdx];
    //bktCnt > 20个，并且删除的大于4分之一时，重构
    if (bktCnt >= 20 && bktCnt / 4 <= ht->deleteCnt) {
        _CCHashTableRebuild(ht, 0);
    }
}

static bool _CCHashTableRemoveValidValue(PCCHashTable_S ht, CCU64Ptr key)
{
    bool equal = false;
    CCIndex bktIdx = _CCHashTableFindBucketIndex(ht, key, 0, false, &equal);
    if (equal == false && bktIdx != CCNotFound && ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        PCCHashTable_S tmp = _CCHASHTABLE_SUB_HASHTABLE(ht, bktIdx);
        if (tmp) {
            return _CCHashTableRemoveValidValue(tmp, key);
        }
    }
    if (!equal) {
        return false;
    }
    _CCHashTableRemoveValue(ht, bktIdx);
    return true;
}

static void _CCHashTableDrain(PCCHashTable_S ht, bool freeBuffer)
{
    PCCAllocator_S allocator = ht->base.allocator;
    
    _CCHashNode *oldValues = ht->nodes[_HASH_VALUE_IDX];
    if (freeBuffer) {
        ht->nodes[_HASH_VALUE_IDX] = NULL;
    }
    
    _CCHashNode *oldKeys = NULL;
    if (ht->bits.haveKey) {
        CCIndex keyIdx = ht->bits.keyIdx;
        oldKeys = ht->nodes[keyIdx];
        if (freeBuffer) {
            ht->nodes[keyIdx] = NULL;
        }
    }
    
    uint8_t *oldTypes = NULL;
    if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        oldTypes = (uint8_t*)ht->nodes[ht->bits.typeIdx];
        if (freeBuffer) {
            ht->nodes[ht->bits.typeIdx] = NULL;
        }
    }
    
    if (_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
        CCU64Ptr value = ht->specialNodes[_HASH_EMPTY_VALUE_IDX].value;
        _CCHashTableReleaseValue(ht, value);
    }
    if (_CCHASHTABLE_HAVE_DELETE_KEY(ht)) {
        CCU64Ptr value = ht->specialNodes[_HASH_DELETE_VALUE_IDX].value;
        _CCHashTableReleaseValue(ht, value);
    }
    
    CCCount oldBuckets = _CCHashTableBuckets[ht->bits.currBucketsIdx];
    if (0 < oldBuckets) {
        for (CCIndex i = 0; i < oldBuckets; ++i) {
            CCU64Ptr value = oldValues[i].value;
            uint8_t type = _CCHashTableValueTypeNormal;
            if (oldTypes) {
                type = oldTypes[i];
            }
            if (type == _CCHashTableValueTypeHashTable) {
                PCCHashTable_S tmp = (PCCHashTable_S)value;
                CCHashTableDeallocate(tmp);
                oldTypes[i] = _CCHashTableValueTypeNull;
                oldValues[i].value = _CCHashTableEmptyValue_s;
            }
            else {
                if (_IS_HASHTABLE_VALID_VAL(value)) {
                    _CCHashTableReleaseValue(ht, value);
                }
            }
            if (oldKeys) {
                CCU64Ptr key = oldKeys[i].value;
                if (_IS_HASHTABLE_VALID_VAL(key)) {
                    _CCHashTableReleaseKey(ht, key);                    
                }
            }
            
        }
    }
    
    if (freeBuffer) {
        CCAllocatorDeallocate(allocator, ht->specialNodes);
        ht->specialNodes = NULL;
        CCAllocatorDeallocate(allocator, oldValues);
        CCAllocatorDeallocate(allocator, oldKeys);
        CCAllocatorDeallocate(allocator, oldTypes);
    }
    ht->usedCnt = 0;
    ht->deleteCnt = 0;
//    ht->bits.deallocated = true;
}

PCCHashTable_S CCHashTableCreate(PCCAllocator_S allocator, CCHashTableContext_S *ctx, CCOptionFlag flags)
{
    if (ctx == NULL) {
        return NULL;
    }
    
    size_t size = sizeof(CCHashTable_S);
    if (flags & CCHashHaveKey) {
        size += sizeof(_CCHashNode *);
    }
    CCHashTableStyle_E style = CC_GET_BIT_FIELD(flags, 1, 2);
    if (style == CCHashTableStyleL2Hash) {
        size += sizeof(uint8_t *);
    }
    
    PCCHashTable_S ht = (PCCHashTable_S)CCAllocatorCreateInstance(allocator, _CCHashTableGetTypeID(), size);
    if (!ht) {
        return NULL;
    }
    ht->base.allocator = allocator;
    
    ht->bits.currBucketsIdx = 0;
    ht->bits.haveKey = CC_GET_BIT_FIELD(flags, 0, 1);
//    ht->bits.deallocated = false;
    ht->bits.hashStyle = style;
    
    ht->usedCnt = 0;
    ht->deleteCnt = 0;
    ht->ctx.retainValue = ctx->retainValue;
    ht->ctx.releaseValue = ctx->releaseValue;
    ht->ctx.retainKey = ctx->retainKey;
    ht->ctx.releaseKey = ctx->releaseKey;
    ht->ctx.equalValue = ctx->equalValue;
    ht->ctx.equalKey = ctx->equalKey;
    ht->ctx.hashKey = ctx->hashKey;
    
    CCIndex idx = 0;
    ht->nodes[_HASH_VALUE_IDX] = NULL;
    if (ht->bits.haveKey) {
        ht->bits.keyIdx = ++idx;
        ht->nodes[idx] = NULL;
    }
    
    if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        ht->bits.typeIdx = ++idx;
        ht->nodes[idx] = NULL;
    }
    
    return ht;
}

CCCount CCHashTableGetBuckets(PCCHashTable_S ht)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, 0);
    return _CCHashTableBuckets[ht->bits.currBucketsIdx];
}

CCCount CCHashTableGetCapacity(PCCHashTable_S ht)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, 0);
    return _CCHashTableCapacities[ht->bits.currBucketsIdx];
}

void CCHashTableSetCapacity(PCCHashTable_S ht, CCCount capacity)
{
    _CCHashTableRebuild(ht, capacity - ht->usedCnt);
}

CCCount CCHashTableGetCount(PCCHashTable_S ht)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, 0);
    return ht->usedCnt;
}

CCHashTableBucket_S CCHashTableGetBucket(PCCHashTable_S ht, CCIndex idx)
{
    CCHashTableBucket_S result = {CCNotFound, 0, 0};
    _CHECK_HASHTABLE_DEALLOCATED(ht, result);
    if (idx < 0 || idx >= _CCHashTableBuckets[ht->bits.currBucketsIdx]) {
        return result;
    }
    result.idx = idx;
    result.key = _CCHASHTABLE_GET_KEY(ht, idx, 0);//_CCHashTableGetKey(ht, idx);
    result.value = ht->nodes[_HASH_VALUE_IDX][idx].value;//_CCHashTableGetValue(ht, idx);
    return result;
}

CCHashTableBucket_S CCHashTableFindBucket(PCCHashTable_S ht, CCU64Ptr key)
{
    CCHashTableBucket_S result = {CCNotFound, 0, 0};
    _CHECK_HASHTABLE_DEALLOCATED(ht, result);
    if (!_IS_HASHTABLE_VALID_VAL(key)) {
        if (key == _CCHashTableEmptyValue_s) {
            if (_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
                result.idx = _HASH_EMPTY_VALUE_IDX;
                result.key = key;
                result.value = ht->specialNodes[_HASH_EMPTY_VALUE_IDX].value;
                result.keyEqual = true;
            }
        }
        else {
            if (_CCHASHTABLE_HAVE_DELETE_KEY(ht)) {
                result.idx = _HASH_DELETE_VALUE_IDX;
                result.key = key;
                result.value = ht->specialNodes[_HASH_DELETE_VALUE_IDX].value;
                result.keyEqual = true;
            }
        }
        return result;
    }
    bool equal = false;
    result.idx = _CCHashTableFindBucketIndex(ht, key, 0, false, &equal);
    if (equal) {
        result.key = _CCHASHTABLE_GET_KEY(ht, result.idx, key);
        result.value = ht->nodes[_HASH_VALUE_IDX][result.idx].value;
    }
    else {
        if (result.idx != CCNotFound && ht->bits.hashStyle == CCHashTableStyleL2Hash) {
            PCCHashTable_S tmp = _CCHASHTABLE_SUB_HASHTABLE(ht, result.idx);
            if (tmp) {
                return CCHashTableFindBucket(tmp, key);
            }
        }
    }
    result.keyEqual = equal;
    return result;
}

bool CCHashTableContainsKey(PCCHashTable_S ht, CCU64Ptr key)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, false);
    CCHashTableBucket_S bkt = CCHashTableFindBucket(ht, key);
    return bkt.keyEqual;
}

bool CCHashTableGetValueOfKeyIfPresent(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr *value)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, false);
    CCHashTableBucket_S bkt = CCHashTableFindBucket(ht, key);
    if (bkt.keyEqual) {
        if (value) {
            *value = bkt.value;
        }
        return true;
    }
    return false;
}

bool CCHashTableAddValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, false);
    if (!_IS_HASHTABLE_VALID_VAL(key)) {
        _checkAllocateSpecialNode(ht);
        if (key == _CCHashTableEmptyValue_s) {
            if (!_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
                _CCHashTableSetSpecialValue(ht, _HASH_EMPTY_VALUE_IDX, value, true);
                _CCHASHTABLE_SET_EMPTY_KEY(ht, 1);
                ++ht->usedCnt;
                return true;
            }
        }
        else {
            if (!_CCHASHTABLE_HAVE_DELETE_KEY(ht)) {
                _CCHashTableSetSpecialValue(ht, _HASH_DELETE_VALUE_IDX, value, true);
                _CCHASHTABLE_SET_DELETE_KEY(ht, 1);
                ++ht->usedCnt;
                return true;
            }
        }
        return false;
    }
    return _CCHashTableAddValidValue(ht, key, value, true);
}

bool CCHashTableReplaceValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, false);
    if (!_IS_HASHTABLE_VALID_VAL(key)) {
        _checkAllocateSpecialNode(ht);
        if (key == _CCHashTableEmptyValue_s) {
            if (_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
                _CCHashTableSetSpecialValue(ht, _HASH_EMPTY_VALUE_IDX, value, true);
                return true;
            }
        }
        else {
            if (_CCHASHTABLE_HAVE_DELETE_KEY(ht) ) {
                _CCHashTableSetSpecialValue(ht, _HASH_DELETE_VALUE_IDX, value, true);
                return true;
            }
        }
        return false;
    }
    return _CCHashTableReplaceValidValue(ht, key, value, true);
}

bool CCHashTableSetValue(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, false);
    if (!_IS_HASHTABLE_VALID_VAL(key)) {
        _checkAllocateSpecialNode(ht);
        if (key == _CCHashTableEmptyValue_s) {
            _CCHashTableSetSpecialValue(ht, _HASH_EMPTY_VALUE_IDX, value, true);
            if (!_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
                _CCHASHTABLE_SET_EMPTY_KEY(ht, 1);
                ++ht->usedCnt;
            }
        }
        else {
            _CCHashTableSetSpecialValue(ht, _HASH_DELETE_VALUE_IDX, value, true);
            if (!_CCHASHTABLE_HAVE_DELETE_KEY(ht)) {
                _CCHASHTABLE_SET_DELETE_KEY(ht, 1);
                ++ht->usedCnt;
            }
        }
        return true;
    }
    return _CCHashTableSetValidValue(ht, key, value, true);
}

void CCHashTableRemoveValue(PCCHashTable_S ht, CCU64Ptr key)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht, );
    if (!_IS_HASHTABLE_VALID_VAL(key)) {
        if (key == _CCHashTableEmptyValue_s) {
            if (_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
                _CCHASHTABLE_SET_EMPTY_KEY(ht, 0);
                _CCHashTableSetSpecialValue(ht, _HASH_EMPTY_VALUE_IDX, _CCHashTableDeleteValue_s, false);
                --ht->usedCnt;
            }
        }
        else {
            if (_CCHASHTABLE_HAVE_DELETE_KEY(ht)) {
                _CCHASHTABLE_SET_DELETE_KEY(ht, 0);
                _CCHashTableSetSpecialValue(ht, _HASH_DELETE_VALUE_IDX, _CCHashTableDeleteValue_s, false);
                --ht->usedCnt;
            }
        }
    }
    _CCHashTableRemoveValidValue(ht, key);
}

void CCHashTableRemoveAllValues(PCCHashTable_S ht)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht,)
    if (ht->usedCnt == 0) {
        return;
    }
    _CCHashTableDrain(ht, false);
//    ht->bits.deallocated = false;
}

bool _CCHashTableEnumerateValidValue(PCCHashTable_S ht, CCHashTableEnumerator enumerator, CCCount enumerateCnt)
{
    CCCount bktCnt = _CCHashTableBuckets[ht->bits.currBucketsIdx];
    _CCHashNode *values = ht->nodes[_HASH_VALUE_IDX];
    _CCHashNode *keys = NULL;
    if (ht->bits.haveKey) {
        keys = ht->nodes[ht->bits.keyIdx];
    }
    
    uint8_t *types = NULL;
    if (ht->bits.hashStyle == CCHashTableStyleL2Hash) {
        types = (uint8_t*)ht->nodes[ht->bits.typeIdx];
    }
    
    bool finish = false;
    
    for (CCIndex i = 0; i < bktCnt; ++i) {
        CCU64Ptr value = values[i].value;
        CCU64Ptr key = value;
        if (keys) {
            key = keys[i].value;
        }
        
        if (_IS_HASHTABLE_VALID_VAL(key) && enumerator) {
            _CCHashTableValueType_E type = _CCHashTableValueTypeNormal;
            if (types) {
                type = types[i];
            }
            if (type == _CCHashTableValueTypeHashTable) {
                PCCHashTable_S tmp = (PCCHashTable_S)value;
                bool finishTmp = _CCHashTableEnumerateValidValue(tmp, enumerator, 0);
                if (finishTmp) {
                    enumerateCnt += tmp->usedCnt;
                }
                else {
                    finish = finishTmp;
                    break;
                }
            }
            else {
                ++enumerateCnt;
                if (!enumerator(ht, key, value)) {
                    finish = false;
                    break;
                }
            }
        }
        if (enumerateCnt >= ht->usedCnt) {
            finish = true;
            break;
        }
    }
    return finish;
}

void CCHashTableEnumerate(PCCHashTable_S ht, CCHashTableEnumerator enumerator)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht,)
    if (enumerator == NULL || ht->usedCnt == 0) {
        return;
    }
    CCCount enumerateCnt = 0;
    if (_CCHASHTABLE_HAVE_EMPTY_KEY(ht)) {
        if (enumerator) {
            ++enumerateCnt;
            if (!enumerator(ht, _CCHashTableEmptyValue_s, ht->specialNodes[_HASH_EMPTY_VALUE_IDX].value)) {
                return;
            }
        }
    }
    if (_CCHASHTABLE_HAVE_DELETE_KEY(ht)) {
        if (enumerator) {
            ++enumerateCnt;
            if (!enumerator(ht, _CCHashTableDeleteValue_s, ht->specialNodes[_HASH_DELETE_VALUE_IDX].value)) {
                return;
            }
        }
    }
    
    _CCHashTableEnumerateValidValue(ht, enumerator, enumerateCnt);
}

void CCHashTableDeallocate(PCCHashTable_S ht)
{
    _CHECK_HASHTABLE_DEALLOCATED(ht,)
    _CCHashTableDrain(ht, true);
    CCAllocatorDeallocate(ht->base.allocator, ht);

}
