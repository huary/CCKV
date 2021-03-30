//
//  CCBase.h
//  CCKVDemo
//
//  Created by yuan on 2019/12/16.
//  Copyright © 2019 yuan. All rights reserved.
//

#ifndef CCBase_h
#define CCBase_h

#include "CCType.h"

#define INIT_BASE(...)   {NULL, 0, NULL}


#if defined __cplusplus
extern "C" {
#endif


typedef struct _CCAllocator CCAllocator_S;
typedef const struct _CCAllocator *PCCAllocator_S;

typedef struct CCBase {
    void *isa;
    uint32_t info;
    PCCAllocator_S allocator;
}CCBase_S, *PCCBase_S;

struct CCAllocatorContext;

typedef void * (*CCAllocatorAllocateFunc)(CCCount size, struct CCAllocatorContext *allocatorCtx);
typedef void * (*CCAllocatorReallocateFunc)(void *ptr, CCCount newsize, struct CCAllocatorContext *allocatorCtx);
typedef void (*CCAllocatorDeallocateFunc)(void *ptr, struct CCAllocatorContext *allocatorCtx);

typedef struct CCAllocatorContext {
    void *info;
    CCAllocatorAllocateFunc allocate;
    CCAllocatorReallocateFunc reallocate;
    CCAllocatorDeallocateFunc deallocate;
}CCAllocatorContext_S;

PCCAllocator_S CCAllocatorGetDefault(void);
PCCAllocator_S CCAllocatorCreate(PCCAllocator_S allocator, CCAllocatorContext_S *context);

void *CCAllocatorAllocate(PCCAllocator_S allocator, CCCount size);
void *CCAllocatorReallocate(PCCAllocator_S allocator, void *ptr, CCCount newsize);
void CCAllocatorDeallocate(PCCAllocator_S allocator, void *ptr);


CCBase_S *CCAllocatorCreateInstance(PCCAllocator_S allocator, CCTypeID typdeId, CCCount size);

#if defined __cplusplus
};
#endif
#endif /* CCBase_h */
