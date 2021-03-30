//
//  CCArray.h
//  CCKVDemo
//
//  Created by yuan on 2020/6/30.
//  Copyright © 2020 yuan. All rights reserved.
//

#ifndef CCArray_h
#define CCArray_h

#include "CCType.h"
#include "CCBase.h"

#if defined __cplusplus
extern "C" {
#endif


typedef struct _CCArray CCArray_S;
typedef struct _CCArray *PCCArray_S;
//返回false就停止enumerate
typedef bool(*CCArrayEnumerator)(PCCArray_S array, CCU64Ptr value, CCIndex index);

typedef union {
    CCByte_t option;
    struct {
        //0表示数组是连续的,1表示数组值是分散的
        CCByte_t valueOptions:1;
        /*0表示capacity就是数据大小,
         *1表示对capacity值进行2^n操作,n由capacity的二进制的位数决定,
         *例：如果capacity为5,又3个二进制位，那么capactity就是8
         */
        CCByte_t capacityOptions:1;
    }bits;
}CCArrayOption_U;

typedef struct CCArrayContext {
    CCRetainFunc retain;
    CCReleaseFunc release;
    CCEqualFunc equal;
}CCArrayContext_S, *PCCArrayContext_S;

//PCCArray_S CCArrayCreate(PCCAllocator_S allocator, PCCArrayContext_S ctx, CCOptionFlag optionFlag);
PCCArray_S CCArrayCreate(PCCAllocator_S allocator, const CCU64Ptr *values, CCCount valueCnt, PCCArrayContext_S ctx, CCArrayOption_U options);

CCCount CCArrayGetCapacity(PCCArray_S array);

void CCArraySetCapacity(PCCArray_S array, CCCount capacity);

CCCount CCArrayGetCount(PCCArray_S array);

CCCount CCArrayGetCountOfValue(PCCArray_S array, CCU64Ptr value);

bool CCArrayContainsValue(PCCArray_S array, CCU64Ptr value);

//返回第一个匹配的index
CCIndex CCArrayGetIndexOfValue(PCCArray_S array, CCU64Ptr value);

CCU64Ptr CCArrayGetValueAtIndex(PCCArray_S array, CCIndex index);

CCU64Ptr CCArrayGetFirstValue(PCCArray_S array);

CCU64Ptr CCArrayGetLastValue(PCCArray_S array);

void CCArrayAppendValue(PCCArray_S array, CCU64Ptr value);

void CCArrayAppendValues(PCCArray_S array, CCU64Ptr *values, CCCount valuesCnt);

void CCArrayInsertValueAtIndex(PCCArray_S array, CCIndex index, CCU64Ptr value);

void CCArrayInsertValueAtRange(PCCArray_S array, CCU64Ptr *values, CCRange range);

void CCArraySetValueAtIndex(PCCArray_S array, CCIndex index, CCU64Ptr value);

void CCArrayRemoveValueAtIndex(PCCArray_S array, CCIndex index);

void CCArrayRemoveValuesInArray(PCCArray_S array, PCCArray_S remove);

void CCArrayRemoveValuesAtRange(PCCArray_S array, CCRange range);

void CCArrayRemoveAllValues(PCCArray_S array);

void CCArrayEnumerate(PCCArray_S array, CCArrayEnumerator enumerator);

void CCArrayPrint(PCCArray_S array, CCCount count);

void CCArrayDeallocate(PCCArray_S array);

#if defined __cplusplus
};
#endif
#endif /* CCArray_h */
