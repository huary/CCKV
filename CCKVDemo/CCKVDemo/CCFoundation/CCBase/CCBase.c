//
//  CCBase.c
//  CCKVDemo
//
//  Created by yuan on 2019/12/16.
//  Copyright © 2019 yuan. All rights reserved.
//

#include "CCBase.h"

#define _ALLOCATOR(allocator)   ((NULL == allocator) ? CCAllocatorGetDefault() : allocator)
#define _ALLOCATOR_CTX(allocator)   ((struct CCAllocatorContext*)&(allocator->ctx))

struct _CCAllocator {
    struct CCBase base;
    struct CCAllocatorContext ctx;
};

static inline void *_CCAllocatorDefaultAllocate(CCCount size, struct CCAllocatorContext *allocatorCtx)
{
    return calloc(size, sizeof(uint8_t));
//    void *ptr = malloc(size);
//    if (ptr) {
//        memset(ptr, 0, size);
//    }
//    return ptr;
}

static inline void *_CCAllocatorDefaultReallocate(void *ptr, CCCount newsize, struct CCAllocatorContext *allocatorCtx)
{
    if (ptr) {
        return realloc(ptr, newsize);
    }
    return ptr;
}

static inline void _CCAllocatorDefaultDeallocate(void *ptr, struct CCAllocatorContext *allocatorCtx)
{
    if (ptr) {
        free(ptr);
    }
}

static struct _CCAllocator _defaultAllocator = {
    INIT_BASE(),
    {NULL, _CCAllocatorDefaultAllocate, _CCAllocatorDefaultReallocate, _CCAllocatorDefaultDeallocate}
};

static PCCAllocator_S _ptrDefaultAllocator_s = &_defaultAllocator;


PCCAllocator_S CCAllocatorGetDefault()
{
    return _ptrDefaultAllocator_s;
}


PCCAllocator_S CCAllocatorCreate(PCCAllocator_S allocator, CCAllocatorContext_S *context)
{
    if (context == NULL) {
        return NULL;
    }
    allocator = (NULL == allocator) ? CCAllocatorGetDefault() : allocator;
    
    struct _CCAllocator *ptr = NULL;
    CCAllocatorAllocateFunc allocateFunc = allocator->ctx.allocate;
    if (allocateFunc) {
        ptr = allocateFunc(sizeof(CCAllocator_S), _ALLOCATOR_CTX(allocator));
    }
    if (NULL == ptr) {
        return NULL;
    }
    ptr->base.allocator = allocator;
    ptr->ctx.info = context->info;
    ptr->ctx.allocate = context->allocate;
    ptr->ctx.reallocate = context->reallocate;
    ptr->ctx.deallocate = context->deallocate;
    return ptr;
}

void *CCAllocatorAllocate(PCCAllocator_S allocator, CCCount size)
{
    allocator = _ALLOCATOR(allocator);
    CCAllocatorAllocateFunc allocateFunc = allocator->ctx.allocate;
    if (allocateFunc) {
        return allocateFunc(size, _ALLOCATOR_CTX(allocator));
    }
    return NULL;
}

void *CCAllocatorReallocate(PCCAllocator_S allocator, void *ptr, CCCount newsize)
{
    if (ptr == NULL) {
        return NULL;
    }
    allocator = _ALLOCATOR(allocator);
    CCAllocatorReallocateFunc reallocFunc = allocator->ctx.reallocate;
    if (reallocFunc) {
        return reallocFunc(ptr, newsize, _ALLOCATOR_CTX(allocator));
    }
    return NULL;
}

void CCAllocatorDeallocate(PCCAllocator_S allocator, void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    allocator = _ALLOCATOR(allocator);
    CCAllocatorDeallocateFunc deallocateFunc = allocator->ctx.deallocate;
    if (NULL != deallocateFunc) {
        deallocateFunc(ptr, _ALLOCATOR_CTX(allocator));
    }
}

CCBase_S *CCAllocatorCreateInstance(PCCAllocator_S allocator, CCTypeID typdeId, CCCount size)
{
    allocator = _ALLOCATOR(allocator);
    void *ptr = CCAllocatorAllocate(allocator, size);
    CCBase_S *base = (CCBase_S*)ptr;
    base->allocator = allocator;
    return ptr;
}
