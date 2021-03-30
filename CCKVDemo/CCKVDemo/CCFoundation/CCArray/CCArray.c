//
//  CCArray.c
//  CCKVDemo
//
//  Created by yuan on 2020/6/30.
//  Copyright © 2020 yuan. All rights reserved.
//

#include "CCArray.h"
#include "CCMacro.h"

#define  _CCARRAY_NODE_IDX               (0)
#define  _CCARRAY_CAPACITY_ROUNDUP(C)    (IS_POWER_OF_2(C) ? (C) : ({uint8_t B = TYPEULL_BITS_N(C); B = MAX(MIN(B, 63), 2); MIN((1ULL << B),CCCountMax);}))
#define  _CCARRAY_CAPACITY(PA,C)         (PA->op.capacityOption == _CCArrayCapacityReal ? C : _CCARRAY_CAPACITY_ROUNDUP(C))
#define  _CCARRAY_INDEX(CAP,IDX)         (IS_POWER_OF_2(CAP) ? TYPE_AND(IDX, CAP-1) : ((IDX) % (CAP)))

typedef enum {
    //数组值是连续的
    _CCArrayValueContinuous  = 0,
    //数组值是分散的
    _CCArrayValueDisperse    = 1,
}_CCArrayValue_E;

typedef enum {
    _CCArrayCapacityReal     = 0,
    _CCArrayCapacityCeil     = 1,
}_CCArrayCapacity_E;


typedef enum {
    _CCArrayReplace = 0,
    _CCArrayInsert  = 1,
    _CCArrayDelete  = 2,
}_CCArrayAction;

typedef union {
    CCU64Ptr value;
}_CCArrayNode;

struct _CCArray {
    CCBase_S base;
    CCCount capacity;
    CCCount count;
    CCIndex sIdx;
    //只有valueOption是CCArrayValueDisperse时才使用
    CCIndex eIdx;
    struct {
        CCByte_t bitsIdx:1; //it is 1 if CCArrayValueDisperse
        _CCArrayValue_E valueOption:1;
        _CCArrayCapacity_E capacityOption:1;
//        uint8_t deallocated:1;
    } op;
    CCArrayContext_S ctx;
    _CCArrayNode *nodes[1];
};

static inline CCTypeID _CCArrayGetTypeID(void) {
    return 0;
}

static inline CCU64Ptr _CCArrayRetainValue(PCCArray_S array, CCU64Ptr value)
{
    CCRetainFunc f = array->ctx.retain;
    if (f) {
        return f(array, value);
    }
    return value;
}

static inline void _CCArrayReleaseValue(PCCArray_S array, CCU64Ptr value)
{
    CCReleaseFunc f = array->ctx.release;
    if (f) {
        f(array, value);
    }
}

static inline bool _CCArrayEqualValue(PCCArray_S array, CCU64Ptr value1, CCU64Ptr value2)
{
    CCEqualFunc f = array->ctx.equal;
    if (f) {
        return f(array, value1, value2);
    }
    return (value1 == value2);
}

static inline void _CCArraySetValue(PCCArray_S array, CCIndex idx, CCU64Ptr value, bool retain, bool release)
{
    _CCArrayNode *node = &(array->nodes[_CCARRAY_NODE_IDX][idx]);
    CCU64Ptr oldValue = node->value;
    node->value = retain ? _CCArrayRetainValue(array, value) : value;
    if (release) {
        _CCArrayReleaseValue(array, oldValue);
    }
}

static void _CCArrayRebuild(PCCArray_S array, CCCount capacity)
{
//    CCCount cap = _CCARRAY_CAPACITY_ROUNDUP(capacity);
    CCCount cap = _CCARRAY_CAPACITY(array,capacity);
    if (cap < array->count || (cap == array->capacity && array->op.valueOption == _CCArrayValueContinuous)) {
        return;
    }
    
    PCCAllocator_S allocator = array->base.allocator;
    
    CCUByte_t *newBits = NULL;
    _CCArrayNode *newValues = CCAllocatorAllocate(allocator, cap * sizeof(_CCArrayNode));
    if (newValues == NULL) {
        goto CCARRAY_REBUILD_ERR_END;
    }

    CCUByte_t *oldBits = NULL;
    _CCArrayNode *oldValues = NULL;
    
    if (array->op.valueOption == _CCArrayValueDisperse) {
        CCCount size = (cap + 7) >> 3;
        newBits = CCAllocatorAllocate(allocator, size);
        if (newBits == NULL) {
            goto CCARRAY_REBUILD_ERR_END;
        }
        oldBits = (CCUByte_t*)array->nodes[array->op.bitsIdx];
    }
    oldValues = array->nodes[_CCARRAY_NODE_IDX];
    
    CCCount oldCapacity = array->capacity;
    CCCount oldCount = array->count;
    CCCount oldSIdx = array->sIdx;
    CCCount newSIdx = cap >> 1;
    CCCount cp = 0;
    if (array->op.valueOption == _CCArrayValueContinuous) {
        CCCount R = MIN(oldCapacity - oldSIdx, oldCount);
        CCCount newR = cap - newSIdx;
        CCCount idx = newSIdx;
        CCCount oldIdx = oldSIdx;
        R = MIN(newR, R);
        //1.从左边复制到新的左边
        if (R > 0) {
            memmove(newValues + idx, oldValues + oldIdx, R * sizeof(_CCArrayNode));
            cp += R;
            idx += R;
            oldIdx += R;
            
            idx = _CCARRAY_INDEX(cap, idx);
            oldIdx = _CCARRAY_INDEX(oldCapacity, oldIdx);
        }
        //2.如果新的左边不够旧的左边容量多，将旧的左边剩余数据复制到新的右边区域
        if (oldCount > cp) {
            if (oldIdx > oldSIdx) {
                R = MIN(oldCount - cp, oldCapacity - oldIdx);
            }
            else {
                R = oldCount - cp;
            }
            memmove(newValues + idx, oldValues + oldIdx, R * sizeof(_CCArrayNode));
            cp += R;
            idx += R;
            oldIdx += R;
            
            idx = _CCARRAY_INDEX(cap, idx);
            oldIdx = _CCARRAY_INDEX(oldCapacity, oldIdx);
        }
        //3.如果旧的右边有剩余数据，将旧的右边剩余数据复制到新的区域
        if (oldCount > cp) {
            R = oldCount - cp;
            memmove(newValues + idx, oldValues + oldIdx, R * sizeof(_CCArrayNode));
        }
//        array->eIdx = _CCARRAY_INDEX(cap, newSIdx + oldCount);
    }
    else {
//        for (CCIndex i = 0; i < oldCount; ++i) {
//            CCIndex idx = _CCARRAY_INDEX(oldCapacity, i + oldSIdx);
//
//        }
    }
    
    array->capacity = cap;
    array->sIdx = newSIdx;
    array->nodes[_CCARRAY_NODE_IDX]= newValues;
    if (array->op.valueOption == _CCArrayValueDisperse) {
        array->nodes[array->op.bitsIdx] = (_CCArrayNode *)newBits;
    }
    CCAllocatorDeallocate(allocator, oldValues);
    CCAllocatorDeallocate(allocator, oldBits);
    return;
CCARRAY_REBUILD_ERR_END:
    CCAllocatorDeallocate(allocator, newValues);
    CCAllocatorDeallocate(allocator, newBits);
    return;
}

static void _print(PCCArray_S array,CCCount count)
{
    CCIndex sIdx = array->sIdx;
//    CCCount count = array->count;
    CCCount capacity = array->capacity;

    _CCArrayNode *values = array->nodes[_CCARRAY_NODE_IDX];
    if (array->op.valueOption == _CCArrayValueContinuous) {
        for (CCIndex i = 0; i < count; ++i) {
            CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + i);
            printf("%lu,%ld:%lu\n",i,idx,values[idx].value);
        }
    }
}

static bool _CCArrayCheckRebuild(PCCArray_S array, CCInt newItemCnt)
{
    CCCount featureCnt = array->count + newItemCnt;
    if ((newItemCnt > 0 && featureCnt < array->count) || (newItemCnt < 0 && -newItemCnt > array->count)) {
        return false;
    }
    
    if (featureCnt > array->capacity) {
        _CCArrayRebuild(array, featureCnt);
    }
    return true;
}

static void _CCArrayUpdateValues(PCCArray_S array, const CCU64Ptr *values, CCRange range, _CCArrayAction action)
{
    CCIndex sIdx = array->sIdx;
    CCCount count = array->count;
    CCCount capacity = array->capacity;
    if (range.location > count) {
        CCAssert(0, "location is greater than count %lu",count);
        return;
    }
    if (_CCArrayReplace == action) {
        if (values == NULL || range.location + range.length > count) {
            CCAssert(0, "replace end is greater than count %lu",count);
            return;
        }
    }
    else if (_CCArrayInsert == action) {
        if (values == NULL) {
            return;
        }
        _CCArrayCheckRebuild(array, range.length);
    }
    else {
        if (range.location + range.length > count) {
            CCAssert(0, "delete end is greater than count %lu",count);
            return;
        }
        _CCArrayCheckRebuild(array, -range.length);
    }
    
    if (array->op.valueOption == _CCArrayValueContinuous) {
        if (_CCArrayReplace == action) {
            for (CCIndex i = 0; i < range.length; ++i) {
                CCIndex idx = range.location + i;
                CCIndex index = _CCARRAY_INDEX(capacity, sIdx + idx);
                _CCArraySetValue(array, index, values[i], true, true);
            }
        }
        else if (_CCArrayInsert == action)  {
            _CCArrayNode *oldValues = array->nodes[_CCARRAY_NODE_IDX];
            CCIndex L = range.location;
            CCCount mvLen = range.length;
            CCCount B = count - L;
            CCIndex eIdx = _CCARRAY_INDEX(capacity, sIdx + count);
            if (L < B) {
                //向前移动数据
                CCIndex sIdxNew = _CCARRAY_INDEX(capacity, capacity + sIdx - mvLen);
                CCCount S = sIdx + 1;
                CCCount m = (sIdx < eIdx) ? (S > mvLen ? mvLen : (mvLen - S)) : (capacity - S);
                memmove(oldValues + sIdxNew, oldValues + sIdx, m * sizeof(_CCArrayNode));
                if (m < L) {
                    CCCount n = (sIdx < eIdx) ? (L - m) : MIN(L - m, mvLen);
                    CCIndex fromIdx = _CCARRAY_INDEX(capacity, sIdx + m);
                    CCIndex toIdx = _CCARRAY_INDEX(capacity, sIdxNew + m);
                    memmove(oldValues + toIdx, oldValues + fromIdx, n * sizeof(_CCArrayNode));
                    if (m + n < L) {
                        CCCount r = L - m - n;
                        fromIdx = _CCARRAY_INDEX(capacity, sIdx + m + n);
                        toIdx = _CCARRAY_INDEX(capacity, sIdxNew + m + n);
                        memmove(oldValues + toIdx, oldValues + fromIdx, r * sizeof(_CCArrayNode));
                    }
                }
                array->sIdx = sIdxNew;
            }
            else {
                if (B > 0) {
                    CCCount E = eIdx;
                    CCIndex eIdxNew = _CCARRAY_INDEX(capacity, eIdx + mvLen - 1);
                    CCCount m = E;
                    if (sIdx < eIdx) {
                        CCCount r = capacity - E;
                        m = r >= mvLen ? mvLen : mvLen - r;
                    }
                    memmove(oldValues + eIdxNew, oldValues + eIdx - m, m * sizeof(_CCArrayNode));
                    if (m < B) {
                        CCCount n = MIN(B - m, mvLen);
                        CCIndex fromIdx = _CCARRAY_INDEX(capacity, capacity + eIdx - m - n);
                        CCIndex toIdx = _CCARRAY_INDEX(capacity, capacity + eIdxNew - m);
                        memmove(oldValues + toIdx, oldValues + fromIdx, n * sizeof(_CCArrayNode));
                        if (m + n < B) {
                            CCCount r = B - m - n;
                            fromIdx = _CCARRAY_INDEX(capacity, sIdx + L);
                            toIdx = _CCARRAY_INDEX(capacity, sIdx + L + mvLen);
                            memmove(oldValues + toIdx, oldValues + fromIdx, r * sizeof(_CCArrayNode));
                        }
                    }
                }
            }
            
            array->count += mvLen;
            sIdx = array->sIdx;
            for (CCIndex i = 0; i < mvLen; ++i) {
                CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + L + i);
                _CCArraySetValue(array, idx, values[i], true, false);
            }
        }
        else {
            CCIndex L = range.location;
            CCCount rmLen = range.length;
            CCIndex rmEIdx = L + rmLen;
            for (CCIndex i = 0; i < rmLen; ++i) {
                CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + L + i);
                _CCArraySetValue(array, idx, 0, false, true);
            }
            if (L == 0) {
                array->sIdx = _CCARRAY_INDEX(capacity, sIdx + rmLen);
            }
            else if (rmEIdx == count) {
            }
            else {
                CCIndex R = count - L - rmLen;
                _CCArrayNode *oldValues = array->nodes[_CCARRAY_NODE_IDX];
#if 0
                //方法1: 一个个的移动
                if (L < R) {
                    for (CCCount i = 1; i <= L; ++i) {
                        CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + L - i);
                        CCIndex idxTmp = _CCARRAY_INDEX(capacity, sIdx + L - i + rmLen);
                        oldValues[idxTmp] = oldValues[idx];
                    }
                    array->sIdx = _CCARRAY_INDEX(capacity, array->sIdx + rmLen);
                }
                else {
                    for (CCCount i = 0; i < L; ++i) {
                        CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + L + rmLen + i);
                        CCIndex idxTmp = _CCARRAY_INDEX(capacity, sIdx + L + i);
                        oldValues[idxTmp] = oldValues[idx];
                    }
                }
#else
                //方法2:memmove
                if (L < R) {
                    CCIndex lIdx = _CCARRAY_INDEX(capacity, sIdx + L);
                    CCIndex rIdx = _CCARRAY_INDEX(capacity, sIdx + L + rmLen);

                    CCCount m = MIN(MIN(lIdx, L), rIdx);
                    CCIndex fromIdx = _CCARRAY_INDEX(capacity, sIdx + L - m);
                    CCIndex toIdx = _CCARRAY_INDEX(capacity, sIdx + L + rmLen - m);
                    memmove(oldValues + toIdx, oldValues + fromIdx, m * sizeof(_CCArrayNode));
                    if (m < L) {
                        CCCount n = MIN(L - m, rmLen);
                        fromIdx = _CCARRAY_INDEX(capacity, sIdx + L - m - n);
                        toIdx = _CCARRAY_INDEX(capacity, sIdx + L + rmLen - m - n);
                        memmove(oldValues + toIdx, oldValues + fromIdx, n * sizeof(_CCArrayNode));
                        if (m + n < L) {
                            CCCount r = L - m - n;
                            toIdx = _CCARRAY_INDEX(capacity, sIdx + rmLen);
                            memmove(oldValues + toIdx, oldValues + sIdx, r * sizeof(_CCArrayNode));
                        }
                    }
                    array->sIdx = _CCARRAY_INDEX(capacity, sIdx + rmLen);
                }
                else {
                    //最多三次memmove
                    CCIndex lIdx = _CCARRAY_INDEX(capacity, sIdx + L);
                    CCIndex rIdx = _CCARRAY_INDEX(capacity, sIdx + L + rmLen);
                    CCCount m = MIN(MIN(R, capacity - rIdx), capacity - lIdx);
                    memmove(oldValues + lIdx, oldValues + rIdx, m * sizeof(_CCArrayNode));
                    if (m < R) {
                        CCCount n = MIN(R - m, rmLen);
                        CCIndex fromIndex = _CCARRAY_INDEX(capacity, sIdx + L + rmLen + m);
                        CCIndex toIndex = _CCARRAY_INDEX(capacity, sIdx + L + m);
                        memmove(oldValues + toIndex, oldValues + fromIndex, n * sizeof(_CCArrayNode));
                        if (m + n < R) {
                            CCCount r = R - m - n;
                            memmove(oldValues, oldValues + rmLen, r * sizeof(_CCArrayNode));
                        }
                    }
                }
#endif
            }
            array->count -= rmLen;
            if (array->count == 0) {
                array->sIdx = capacity >> 1;
            }
        }
    }
    
}

static PCCArray_S _CCArrayCreate(PCCAllocator_S allocator, PCCArrayContext_S ctx, CCArrayOption_U options)
{
    if (NULL == ctx) {
        return NULL;
    }
    
    CCCount size = sizeof(CCArray_S);
    
    _CCArrayValue_E valueOption = CC_GET_BIT_FIELD(options.option, 0, 1);
    if (valueOption == _CCArrayValueDisperse) {
        size += sizeof(CCUByte_t *);
    }
        
    PCCArray_S array = (PCCArray_S)CCAllocatorCreateInstance(allocator, _CCArrayGetTypeID(), size);
    if (!array) {
        return NULL;
    }
    
    array->capacity = 0;
    array->count = 0;
    array->sIdx = 0;
    array->eIdx = 0;
    array->ctx = *ctx;
//    array->op.deallocated = false;
    array->op.valueOption = valueOption;
    array->op.capacityOption = CC_GET_BIT_FIELD(options.option, 1, 1);
    if (array->op.valueOption == _CCArrayValueDisperse) {
        array->op.bitsIdx = 1;
    }
    return array;
}

static void _CCArrayDrain(PCCArray_S array, bool freeBuffer)
{
    PCCAllocator_S allocator = array->base.allocator;
    CCIndex sIdx = array->sIdx;
    CCCount count = array->count;
    CCCount capacity = array->capacity;

    CCUByte_t *oldBits = NULL;
    _CCArrayNode *oldValues = array->nodes[_CCARRAY_NODE_IDX];
    if (array->op.valueOption == _CCArrayValueContinuous) {
        for (CCIndex i = 0; i < count; ++i) {
            CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + i);
            _CCArrayReleaseValue(array, oldValues[idx].value);
        }
    }
    else {
        oldBits = (CCUByte_t*)array->nodes[array->op.bitsIdx];
        if (freeBuffer) {
            array->nodes[array->op.bitsIdx] = NULL;
        }
    }
    
    if (freeBuffer) {
        CCAllocatorDeallocate(allocator, oldValues);
        array->nodes[_CCARRAY_NODE_IDX] = NULL;
        CCAllocatorDeallocate(allocator, oldBits);
    }
    array->count = 0;
    array->eIdx = array->sIdx = 0;
}

PCCArray_S CCArrayCreate(PCCAllocator_S allocator, const CCU64Ptr *values, CCCount valueCnt, PCCArrayContext_S ctx, CCArrayOption_U options)
{
    PCCArray_S array =_CCArrayCreate(allocator, ctx, options);
    
    if (valueCnt > 0) {
//        CCCount capacity = _CCARRAY_CAPACITY_ROUNDUP(valueCnt);
        CCArraySetCapacity(array, valueCnt);
        
        _CCArrayUpdateValues(array, values, CCRangeMake(0, valueCnt), _CCArrayInsert);
    }
    
    return array;
}

CCCount CCArrayGetCapacity(PCCArray_S array)
{
    return array->capacity;
}

void CCArraySetCapacity(PCCArray_S array, CCCount capacity)
{
    _CCArrayRebuild(array, capacity);
}


CCCount CCArrayGetCount(PCCArray_S array)
{
    return array->count;
}

CCCount CCArrayGetCountOfValue(PCCArray_S array, CCU64Ptr value)
{
    CCCount cnt = 0;
    CCCount count = array->count;
    CCCount capacity = array->capacity;
    CCIndex sIdx = array->sIdx;
    _CCArrayNode *values = array->nodes[_CCARRAY_NODE_IDX];
    if (array->op.valueOption == _CCArrayValueContinuous) {
        for (CCIndex i = 0; i < count; ++i) {
            CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + i);
            if (_CCArrayEqualValue(array, values[idx].value, value)) {
                ++cnt;
            }
        }
    }
    return cnt;
}

bool CCArrayContainsValue(PCCArray_S array, CCU64Ptr value)
{
    return CCArrayGetCountOfValue(array, value) > 0;
}

CCIndex CCArrayGetIndexOfValue(PCCArray_S array, CCU64Ptr value)
{
    CCIndex index = CCNotFound;
    CCCount count = array->count;
    CCCount capacity = array->capacity;
    CCIndex sIdx = array->sIdx;
    _CCArrayNode *values = array->nodes[_CCARRAY_NODE_IDX];
    if (array->op.valueOption == _CCArrayValueContinuous) {
        for (index = 0; index < count; ++index) {
            CCIndex idx = _CCARRAY_INDEX(capacity, index + sIdx);
            if (_CCArrayEqualValue(array, values[idx].value, value)) {
                break;
            }
        }
    }
    return index;
}


CCU64Ptr CCArrayGetValueAtIndex(PCCArray_S array, CCIndex index)
{
    if (index >= array->count) {
        CCAssert(0, "index greater than %lu",array->count);
        return 0;
    }
    CCIndex idx = _CCARRAY_INDEX(array->capacity, index + array->sIdx);
    if (array->op.valueOption == _CCArrayValueContinuous) {
        return array->nodes[_CCARRAY_NODE_IDX][idx].value;
    }
    else {
        
    }
    return 0;
}

CCU64Ptr CCArrayGetFirstValue(PCCArray_S array) {
    if (array->count == 0) {
        CCAssert(0, "array is empty");
        return 0;
    }
    if (array->op.valueOption == _CCArrayValueContinuous) {
        return array->nodes[_CCARRAY_NODE_IDX][array->sIdx].value;
    }
    else {
        
    }
    return 0;
}

CCU64Ptr CCArrayGetLastValue(PCCArray_S array)
{
    if (array->count == 0) {
        CCAssert(0, "array is empty");
        return 0;
    }
    CCIndex idx = _CCARRAY_INDEX(array->capacity, array->count + array->sIdx);
    if (array->op.valueOption == _CCArrayValueContinuous) {
        return array->nodes[_CCARRAY_NODE_IDX][idx].value;
    }
    else {
        
    }
    return 0;
}

void CCArrayAppendValue(PCCArray_S array, CCU64Ptr value)
{
    CCU64Ptr vals[1] = {value};
    CCIndex index = array->count;
    _CCArrayUpdateValues(array, vals, CCRangeMake(index, 1), _CCArrayInsert);
}

void CCArrayAppendValues(PCCArray_S array, CCU64Ptr *values, CCCount valuesCnt)
{
    CCIndex index = array->count;
    _CCArrayUpdateValues(array, values, CCRangeMake(index, valuesCnt), _CCArrayInsert);

}

void CCArrayInsertValueAtIndex(PCCArray_S array, CCIndex index, CCU64Ptr value)
{
    CCU64Ptr vals[1] = {value};
    _CCArrayUpdateValues(array, vals, CCRangeMake(index, 1), _CCArrayInsert);
}

void CCArrayInsertValueAtRange(PCCArray_S array, CCU64Ptr *values, CCRange range)
{
    _CCArrayUpdateValues(array, values, range, _CCArrayInsert);
}

void CCArraySetValueAtIndex(PCCArray_S array, CCIndex index, CCU64Ptr value)
{
    CCU64Ptr vals[1] = {value};
    _CCArrayUpdateValues(array, vals, CCRangeMake(index, 1), _CCArrayReplace);
}

void CCArrayRemoveValueAtIndex(PCCArray_S array, CCIndex index)
{
    _CCArrayUpdateValues(array, NULL, CCRangeMake(index, 1), _CCArrayDelete);
}

void CCArrayRemoveValuesInArray(PCCArray_S array, PCCArray_S remove)
{
    CCIndex sIdx = remove->sIdx;
    CCCount count = remove->count;
    CCCount capacity = remove->capacity;
    _CCArrayNode *removeValues = remove->nodes[_CCARRAY_NODE_IDX];
    for (CCIndex idx = 0; idx < count; ++idx) {
        CCIndex index = CCArrayGetIndexOfValue(array, removeValues[_CCARRAY_INDEX(capacity, sIdx + idx)].value);
        if (index != CCNotFound) {
            _CCArrayUpdateValues(array, NULL, CCRangeMake(index, 1), _CCArrayDelete);            
        }
    }
}

void CCArrayRemoveValuesAtRange(PCCArray_S array, CCRange range)
{
    _CCArrayUpdateValues(array, NULL, range, _CCArrayDelete);
}

void CCArrayRemoveAllValues(PCCArray_S array)
{
    CCCount len = array->count;
    _CCArrayUpdateValues(array, NULL, CCRangeMake(0, len), _CCArrayDelete);
}

void CCArrayEnumerate(PCCArray_S array, CCArrayEnumerator enumerator)
{
    if (!enumerator) {
        return;
    }
    CCIndex sIdx = array->sIdx;
    CCCount count = array->count;
    CCCount capacity = array->capacity;

    _CCArrayNode *values = array->nodes[_CCARRAY_NODE_IDX];
    if (array->op.valueOption == _CCArrayValueContinuous) {
        for (CCIndex i = 0; i < count; ++i) {
            CCIndex idx = _CCARRAY_INDEX(capacity, sIdx + i);
            if (!enumerator(array, values[idx].value, i)) {
                break;
            }
        }
    }
}

void CCArrayPrint(PCCArray_S array, CCCount count)
{
    _print(array, count);
}

void CCArrayDeallocate(PCCArray_S array)
{
    _CCArrayDrain(array, true);
//    array->op.deallocated = true;
    CCAllocatorDeallocate(array->base.allocator, array);
}
