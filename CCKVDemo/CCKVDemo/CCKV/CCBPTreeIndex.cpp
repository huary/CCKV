//
//  CCBPTreeIndex.cpp
//  CCKVDemo
//
//  Created by yuan on 2020/2/19.
//  Copyright © 2020 yuan. All rights reserved.
//

#include "CCBPTreeIndex.hpp"
#include "CCType.h"
#include "CCBuffer.h"
#include "CCMutableBuffer.h"
#include "CCFileMap.hpp"
#include "CCMacro.h"
#include "CCBase.h"
#include "CCArray.h"
#include "CCHashTable.h"
#include "CCDictionary.h"

//存储索引长度所占用的字节数(1字节)能表示索引长度为0-255
#define CCBT_PAGE_NODE_IDX_SIZE_BYTES      (1)
#define CCBT_PAGE_NODE_IDX_VALUE_BYTES     (8)
#define CCBT_PAGE_NODE_SIZE(PNODE)         ((PNODE)->indexSize + CCBT_PAGE_NODE_IDX_SIZE_BYTES + CCBT_PAGE_NODE_IDX_VALUE_BYTES)
#define CCBT_PAGE_NODE_OFFSET(PNODE)       ((PNODE)->indexOffset - CCBT_PAGE_NODE_IDX_SIZE_BYTES)
#define CCBT_PAGE_NODE_END_OFFSET(PNODE)   ((PNODE)->indexOffset + (PNODE)->indexSize + CCBT_PAGE_NODE_IDX_VALUE_BYTES)

#define CCBT_DEC_BUFFER(OFF,BF,BFType)      CCBuffer *BF = ctx->rootBuffer; \
                                            _CCBufferType_E BFType = _CCBufferTypeRoot; \
                                            if (OFF != ctx->rootOff) { \
                                                _CCMapBufferNode_S *MBN = NULL;\
                                                CCHashTableGetValueOfKeyIfPresent(ctx->bufferCache, (CCU64Ptr)OFF, (CCU64Ptr*)(&MBN));\
                                                if (MBN && MBN->buffer) { \
                                                    ++MBN->refCnt;\
                                                    BF = MBN->buffer; \
                                                    BFType = MBN->bufferType; \
                                                } \
                                                else { \
                                                    if (MBN == NULL) { \
                                                        MBN = (_CCMapBufferNode_S *)calloc(1, sizeof(_CCMapBufferNode_S)); \
                                                        CCHashTableSetValue(ctx->bufferCache, (CCU64Ptr)OFF, (CCU64Ptr)MBN);\
                                                    } \
                                                    BF = ctx->fileMap->createMapBuffer(OFF, ctx->pageSize, CC_F_RDWR); \
                                                    if (BF) BFType = _CCBufferTypeCRT; \
                                                    MBN->refCnt = 1;\
                                                    MBN->buffer = BF;\
                                                    MBN->bufferType = BFType; \
                                                } \
                                            }

//这里为了避免重复的calloc MapBufferNode，在refCnt== 0时可以不释放，只是destroy buffer，
#define CCBT_FRE_BUFFER(OFF,RM)             if (OFF != ctx->rootOff) { \
                                                _CCMapBufferNode_S *MBN = NULL;\
                                                CCHashTableGetValueOfKeyIfPresent(ctx->bufferCache, (CCU64Ptr)OFF, (CCU64Ptr*)(&MBN));\
                                                if (MBN) { \
                                                    if (MBN->refCnt > 0) --MBN->refCnt;\
                                                    if (MBN->refCnt == 0) { \
                                                        ctx->fileMap->destroyMapBuffer(MBN->buffer); \
                                                        MBN->buffer = NULL;\
                                                        MBN->bufferType = _CCBufferTypeNull;\
                                                        if (RM) CCHashTableRemoveValue(ctx->bufferCache, (CCU64Ptr)OFF); \
                                                    } \
                                                } \
                                            }

#define CCBT_GET_OPTION                     CC_GET_BIT_FIELD(ctx->option, 0, 5)
#define CCBT_GET_COW                        CC_GET_BIT_FIELD(ctx->option, 5, 1)
#define CCBT_GET_NEW                        CC_GET_BIT_FIELD(ctx->option, 6, 1)

#define CCBT_SET_OPTION(V)                  CC_SET_BIT_FIELD(ctx->option, 0, 5, V)
#define CCBT_SET_COW(V)                     CC_SET_BIT_FIELD(ctx->option, 5, 1, V)
#define CCBT_SET_NEW(V)                     CC_SET_BIT_FIELD(ctx->option, 6, 1, V)

#define CCBT_UPDATE_HEADER_ROOT(OFF)        {ctx->rootOff = OFF;ctx->headBuffer->seekTo(_CCHeaderOffRoot);ctx->headBuffer->writeLittleEndian64(OFF);}
#define CCBT_UPDATE_HEADER_EOF(OFF)         {ctx->EOFOff = OFF;ctx->headBuffer->seekTo(_CCHeaderOffEOF);ctx->headBuffer->writeLittleEndian64(OFF);}
#define CCBT_UPDATE_HEADER_NEW(OFF)         if(CCBT_GET_NEW) {ctx->newOff = OFF;ctx->headBuffer->seekTo(_CCHeaderOffNew);ctx->headBuffer->writeLittleEndian64(OFF);}
#define CCBT_UPDATE_HEADER_IDLE(OFF)        {ctx->idleOff = OFF;ctx->headBuffer->seekTo(_CCHeaderOffIdle);ctx->headBuffer->writeLittleEndian64(OFF);}

#define CCBT_PAGE_SIZE_FROM_OP              (1 << (ctx->option & 31))
#define CCBT_PAGE_IS_LEAF(PAGE)             ((PAGE->pageType & _CCPageTypeLeaf) == _CCPageTypeLeaf)

#define CCBT_PAGE_NODE_AT_IDX(PAGE,IDX)     ((_CCPageNode_S*)CCArrayGetValueAtIndex(PAGE->pageNodeArray, IDX))
#define CCBT_PAGE_SUB_AT_IDX(PAGE,IDX)      (((_CCSubPage_S*)CCArrayGetValueAtIndex(PAGE->subPageArray, IDX)))

#define CCBT_PAGE_NODE_FIRST(PAGE)          ((_CCPageNode_S*)CCArrayGetFirstValue(PAGE->pageNodeArray))
#define CCBT_PAGE_NODE_LAST(PAGE)           ((_CCPageNode_S*)CCArrayGetLastValue(PAGE->pageNodeArray))

#define CCBT_SUBPAGE_FIRST(PAGE)            ((_CCSubPage_S*)CCArrayGetFirstValue(PAGE->subPageArray))
#define CCBT_SUBPAGE_LAST(PAGE)             ((_CCSubPage_S*)CCArrayGetLastValue(PAGE->subPageArray))


#define CCBT_SUBPAGE_CNT(PAGE)              CCArrayGetCount(PAGE->subPageArray)
#define CCBT_NODE_CNT(PAGE)                 CCArrayGetCount(PAGE->pageNodeArray)
#define CCBT_IS_ROOT_OFF(OFF)               (OFF == ctx->rootOff)
#define CCBT_IS_ROOT_PAGE(PAGE)             CCBT_IS_ROOT_OFF(PAGE->pageOffset)


static int16_t _CCBPTreeIndexHeaderLen_s    = 256;

//page的size
typedef uint32_t CCBTPageSize_T;
//page的个数，最多由2^16-1个物理page页组成一个虚拟的page
typedef uint16_t CCBTPageCnt_T;
//page中所含有的所有的node个数，最大为2^16-2
typedef uint16_t CCBTNodeCnt_T;
//page相对整个文件的偏移量
typedef uint64_t CCBTPageOff_T;
typedef uint16_t CCBTOption_T;
typedef uint16_t CCBTVersion_T;

static CCBTNodeCnt_T _CCNodeListStepCnt_s = 128;
static CCBTPageCnt_T __CCSubPage_SListStepCnt_s = 1;

typedef enum
{
    _CCBufferTypeNull    = 0,
    //rootbuffer,不需要delete和destroy
    _CCBufferTypeRoot    = (1 << 0),
    //需要detroy
    _CCBufferTypeCRT     = (1 << 1),
}_CCBufferType_E;

typedef enum
{
    //查找错误
    _CCFindCodeError     = -1,
    //查找OK
    _CCFindCodeOK        = 0,
    //已经存在
    _CCFindCodeExists    = 1,
}_CCFindCode_E;

typedef enum
{
    _CCHeaderOffVersion          = 0,
    _CCHeaderOffNodeCnt          = _CCHeaderOffVersion + 2,
    _CCHeaderOffFlag             = _CCHeaderOffNodeCnt + 2,
    _CCHeaderOffRoot             = _CCHeaderOffFlag + 2,
    _CCHeaderOffEOF              = _CCHeaderOffRoot + 8,
    _CCHeaderOffIdle             = _CCHeaderOffEOF + 8,
    _CCHeaderOffCOWStart         = _CCHeaderOffIdle + 8,
    _CCHeaderOffCOWEnd           = _CCHeaderOffCOWStart + 8,
    _CCHeaderOffWriteF           = _CCHeaderOffCOWEnd + 8,
    _CCHeaderOffIdxVal1          = _CCHeaderOffWriteF + 2,
    _CCHeaderOffIdxVal2          = _CCHeaderOffIdxVal1 + 8,
    _CCHeaderOffNew              = _CCHeaderOffIdxVal2 + 8,
    _CCHeaderOffEnd              = _CCHeaderOffNew + 8,
}_CCHeaderOff_E;

typedef enum
{
    //next的offset(8)
    _CCPageHeaderOffNext         = 0,
    //firstChild的offset(8)
    _CCPageHeaderOffFChild       = _CCPageHeaderOffNext + 8,
    //brother的offset(8)
    _CCPageHeaderOffBrother      = _CCPageHeaderOffFChild + 8,
    //Cow
    _CCPageHeaderOffCow          = _CCPageHeaderOffBrother + 8,
    //copy-on-write next
    _CCPageHeaderOffCOWNext      = _CCPageHeaderOffCow + 8,
    //copy-on-write prev
    _CCPageHeaderOffCOWPrev      = _CCPageHeaderOffCOWNext + 8,
    //copy-on-write parent
    _CCPageHeaderOffCOWParent    = _CCPageHeaderOffCOWPrev + 8,
    //copy-on-write slotIdx
    _CCPageHeaderOffCOWSlotIdx   = _CCPageHeaderOffCOWParent + 2,
    //pageType的offset(1)
    _CCPageHeaderOffPType        = _CCPageHeaderOffBrother + 8,
    //索引的offset
    _CCPageHeaderOffIndex        = _CCPageHeaderOffPType + sizeof(CCBTIndexSize_T),
}_CCPageHeaderOff_E;

typedef enum {
    _CCPageTypeNone  = 0,
    _CCPageTypeLeaf  = 1 << 0,
    _CCPageTypeRoot  = 1 << 1,
    _CCPageTypeIndex = 1 << 2,
    //copy-on-write
    _CCPageTypeCOW   = 1 << 3,
    //copy-on-write remote
    _CCPageTypeCOWR  = 1 << 4,
}_CCPageType_E;

typedef struct {
    CCBuffer *buffer;
    CCCount refCnt;
    _CCBufferType_E bufferType;
}_CCMapBufferNode_S;

typedef struct {
    CCBTPageOff_T off[2];
    _CCMapBufferNode_S *ptr[2];
}_ccMapCowNode_S;

typedef struct {
    CCBTPageSize_T indexOffset;
    CCBTIndexSize_T indexSize;
    //如果是叶子节点，value的值就是存储的data，否则就是孩子节点的位置
    CCBTIndexValue_T value;
}_CCPageNode_S;

typedef struct
{
    CCBTNodeCnt_T sIdx;
    CCBTNodeCnt_T eIdx;
    CCBTPageSize_T remSize;
    CCBTPageOff_T offset;
}_CCSubPage_S;

typedef struct _CCPathNode {
    CCBTPageOff_T offset;
    CCBTNodeCnt_T parentSlotIdx;
    _CCPathNode *parent;
}_CCPathNode_S;

/*page的header
 *|---8字节(next,8字节(pageOffset))---|
 *|---8字节(parent,8字节(pageOffset))---|
 *|---8字节(firstChild,8字节(pageOffset))---|
 *|---8字节(brother,8字节(pageOffset))---|
 *|---1字节(pageType)---|
 *|---剩余是索引---|
*/
typedef struct {
    _CCPageType_E pageType;
    //在父类中的idx
    CCBTNodeCnt_T parentNodeSlotIdx;
    //当前page的offset
    CCBTPageOff_T pageOffset;
    //父page，保留
    CCBTPageOff_T parent;
    //兄弟page
    CCBTPageOff_T brother;
    //第一个孩子page
    CCBTPageOff_T firstChild;
    //路径节点
    _CCPathNode_S *pNode;
    //所有的pageNode的数组
    PCCArray_S pageNodeArray;
    //所有的subPage的数据
    PCCArray_S subPageArray;
}_CCPage_S;

typedef struct {
    //nodeIdx为-1表示已经存在这个索引
    _CCFindCode_E fcd;
    CCBTNodeCnt_T nodeIdx;
    CCBTPageCnt_T subPageIdx;
    CCBTIndexValue_T value;
    _CCPage_S *page;
}_CCPageFind_S;


/*B+树索引的header
*|---2字节(version)---|---2字节(pageNodeCnt)---|---1字节flag(其中最低5表示组成的值表示pageSize的2^exp,如是16，那么pageSize=2^16=32KB,高3位组成的值表示B+树有多少+1层，如果值是5，表示有6层，最大可以表示8层)---|
*|---8字节(rootOffset的位置)---|---8字节(contentSize)---|---8字节(freePageOffset的位置)---|
*
*/
typedef struct
{
    CCBTVersion_T version;
    CCBTNodeCnt_T pageNodeCnt;
    CCBTPageSize_T pageSize;
    CCUInt16_t option;
//    union {
//        CCUInt16_t op;
//        struct {
//            CCUInt16_t :9;
//            CCUByte_t cow:1;
//            CCUByte_t recnew:1;
//            CCUByte_t exponent:5;
//        }bits;
//    }option;
    CCBTPageOff_T rootOff;
    CCBTPageOff_T EOFOff;
    CCBTPageOff_T idleOff;
    CCBTPageOff_T newOff;
    CCFileMap *fileMap;
    _CCPage_S *root;
    CCBuffer *headBuffer;
    CCBuffer *rootBuffer;
    CCArray_S *cowArray;
    CCDictionary *bufferCache;
}_CCBPTreeIndexContext_S, *_PCCBPTreeIndexContext_S;

static inline void test(_CCBPTreeIndexContext_S *ctx) {
    
}

static inline CCCount expandArrayCapacity(PCCArray_S array, CCCount curCnt, CCCount addCnt, CCCount maxCnt) {
    CCCount cap = array ? CCArrayGetCapacity(array) : 0;
    CCCount featureCnt = cap;
    if (curCnt >= cap) {
        featureCnt = curCnt + addCnt;
    }
    return MIN(featureCnt, maxCnt);
}

static inline void arrayValueRelease(void *target, CCU64Ptr value) {
    void *ft = (void*)value;
    if (ft) {
        free(ft);
    }
}

static inline PCCArray_S createArray(CCCount capacity, void *isa)
{
    CCArrayContext ctx;
    ctx.retain = NULL;
    ctx.release = arrayValueRelease;
    ctx.equal = NULL;
    CCArrayOption_U option;
    option.option = 0;
    PCCArray_S array = CCArrayCreate(NULL, NULL, 0, &ctx, option);
    CCArraySetCapacity(array, capacity);
    CCBase *base = (CCBase*)array;
    base->isa = isa;
    return array;
}

//static inline void destroyArray(PCCArray_S array) {
//    CCArrayDeallocate(array);
//}

static inline void resizePageNodeArray(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, bool shrink)
{
    if (page->pageNodeArray) {
        PCCArray_S array = page->pageNodeArray;
        CCCount curCnt = CCArrayGetCount(array);
        CCCount capacity = CCArrayGetCapacity(array);
        CCBTNodeCnt_T featureCnt = expandArrayCapacity(array, curCnt, _CCNodeListStepCnt_s, ctx->pageNodeCnt);
        if ((shrink && featureCnt < capacity) || featureCnt > capacity) {
            CCArraySetCapacity(array, featureCnt);
        }
    }
    else {
        page->pageNodeArray = createArray(_CCNodeListStepCnt_s, ctx);
    }
}

static inline void resizeSubPageArray(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, bool shrink)
{
    if (page->subPageArray) {
        PCCArray_S array = page->subPageArray;
        CCCount curCnt = CCArrayGetCount(array);
        CCCount capacity = CCArrayGetCapacity(array);
        CCBTPageCnt_T featureCnt = expandArrayCapacity(array, curCnt, __CCSubPage_SListStepCnt_s, CCCountMax);
        if ((shrink && featureCnt < capacity) || featureCnt > capacity) {
            CCArraySetCapacity(array, featureCnt);
        }
    }
    else {
        page->subPageArray = createArray(__CCSubPage_SListStepCnt_s, ctx);
    }
}

static inline bool bufferCacheKeyEqual(void *target, CCU64Ptr value1, CCU64Ptr value2) {
    return value1 == value2;
}

static inline CCHashCode bufferCacheKeyhash(void *target, CCU64Ptr key) {
    return key;
}

static inline void bufferCacheValueRelease(void *target, CCU64Ptr value) {
    CCBase *base = (CCBase*)target;
    _CCBPTreeIndexContext_S *ctx = (_CCBPTreeIndexContext_S *)base->isa;
    _CCMapBufferNode_S *mapBufferNode = (_CCMapBufferNode_S*)value;
    if (mapBufferNode) {
        if (mapBufferNode->buffer) {
            ctx->fileMap->destroyMapBuffer(mapBufferNode->buffer);
        }
        free(mapBufferNode);
    }
}

static inline void createBufferCache(_CCBPTreeIndexContext_S *ctx, CCCount capacity) {
    struct CCDictionaryKeyFunc keyFunc = {NULL, NULL, bufferCacheKeyEqual, bufferCacheKeyhash};
    struct CCDictionaryValueFunc valueFunc = {NULL, bufferCacheValueRelease, NULL};
    ctx->bufferCache = CCDictionaryCreate(NULL, NULL, NULL, 0, &keyFunc, &valueFunc, CCHashTableStyleLinear);
    if (ctx->bufferCache) {
        CCBase *base = (CCBase*)ctx->bufferCache;
        base->isa = ctx;
        CCHashTableSetCapacity(ctx->bufferCache, capacity);
    }
}

static inline void destroyBufferCache(_CCBPTreeIndexContext_S *ctx) {
    if (ctx->bufferCache) {
        CCDictionaryDeallocate(ctx->bufferCache);
        ctx->bufferCache = nil;
    }
}

static void readNextPage(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBTNodeCnt_T fromIdx, CCBTPageOff_T next)
{
    if (next <= 0) {
        return;
    }
    CCBTPageSize_T pageSize = ctx->pageSize;
    
    CCBT_DEC_BUFFER(next, buffer, bufferType);
    buffer->seekTo(_CCPageHeaderOffNext);
    CCBTPageOff_T nextTmp = buffer->readLittleEndian64();
    buffer->seekTo(_CCPageHeaderOffIndex);
    CCBTPageSize_T cur = (CCBTPageSize_T)buffer->currentSeek();
    CCBTPageSize_T rem = pageSize - cur;
    PCCArray_S pageNodeArray = page->pageNodeArray;
    while (rem > 0) {
        CCBTIndexSize_T size = buffer->readByte();
        if (size == 0) {
            break;
        }
        
        _CCPageNode_S *node = (_CCPageNode_S*)calloc(1, sizeof(_CCPageNode_S));
        if (!node) {
            break;
        }
        
        node->indexSize = size;
        CCBTPageSize_T offset = (CCBTPageSize_T)buffer->currentSeek();
        node->indexOffset = offset;
        buffer->seekTo(offset + size);
        node->value = buffer->readLittleEndian64();
        rem = pageSize - (CCBTPageSize_T)buffer->currentSeek();
        
        if (CCArrayGetCount(pageNodeArray) >= CCArrayGetCapacity(pageNodeArray)) {
            resizePageNodeArray(ctx, page, false);
        }
        else {
            CCArrayAppendValue(pageNodeArray, (CCU64Ptr)node);
        }

    }
    CCBT_FRE_BUFFER(next, false);
    
    _CCSubPage_S *subPage = (_CCSubPage_S*)calloc(1, sizeof(_CCSubPage_S));
    if (!subPage) {
        return;
    }
    CCBTNodeCnt_T nodeCnt = CCArrayGetCount(page->pageNodeArray);
    subPage->sIdx = fromIdx;
    subPage->eIdx = nodeCnt > 0 ? nodeCnt - 1 : 0;
    subPage->remSize = rem;
    subPage->offset = next;
    
    resizeSubPageArray(ctx, page, false);
    
    CCArrayAppendValue(page->subPageArray, (CCU64Ptr)subPage);

    if (nextTmp > 0) {
        readNextPage(ctx, page, nodeCnt, nextTmp);
    }
}

static void readPageNode(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBuffer *buffer, CCBTPageOff_T next)
{
    resizePageNodeArray(ctx, page, false);
    CCBTPageSize_T rem = (CCBTPageSize_T)buffer->remSize();
    PCCArray_S pageNodeArray = page->pageNodeArray;
    while (rem > 0) {
        CCBTIndexSize_T size = buffer->readByte();
        if (size == 0) {
            break;
        }
        
        _CCPageNode_S *node = (_CCPageNode_S*)calloc(1, sizeof(_CCPageNode_S));
        if (!node) {
            break;
        }
        
        node->indexSize = size;
        CCBTPageSize_T offset = (CCBTPageSize_T)buffer->currentSeek();
        node->indexOffset = offset;
        buffer->seekTo(offset + size);
        node->value = buffer->readLittleEndian64();
        rem = (CCBTPageSize_T)buffer->remSize();
        
        if (CCArrayGetCount(pageNodeArray) >= CCArrayGetCapacity(pageNodeArray)) {
            resizePageNodeArray(ctx, page, false);
        }
        else {
            CCArrayAppendValue(pageNodeArray, (CCU64Ptr)node);
        }
    }

    _CCSubPage_S *subPage = (_CCSubPage_S*)calloc(1, sizeof(_CCSubPage_S));
    if (!subPage) {
        return;
    }
    CCBTNodeCnt_T nodeCnt = CCArrayGetCount(pageNodeArray);
    subPage->sIdx = 0;
    subPage->eIdx = nodeCnt > 0 ? nodeCnt - 1 : 0;
    subPage->remSize = rem;
    subPage->offset = page->pageOffset;
    
    resizeSubPageArray(ctx, page, false);
    
    CCArrayAppendValue(page->subPageArray, (CCU64Ptr)subPage);

    if (next > 0) {
        readNextPage(ctx, page, (CCBTNodeCnt_T)nodeCnt, next);
    }
}

static _CCPage_S *readPage(_CCBPTreeIndexContext_S *ctx, CCBTPageOff_T offset, bool readNode)
{
    _CCPage_S *page = (_CCPage_S *)calloc(1, sizeof(_CCPage_S));;
    if (page == NULL) {
        return NULL;
    }
    CCBT_DEC_BUFFER(offset, buffer, bufferType);

    page->pageOffset = offset;
    buffer->seekTo(_CCPageHeaderOffNext);
    CCBTPageOff_T next = buffer->readLittleEndian64();
    buffer->seekTo(_CCPageHeaderOffFChild);
    page->firstChild = buffer->readLittleEndian64();
    buffer->seekTo(_CCPageHeaderOffBrother);
    page->brother = buffer->readLittleEndian64();
    buffer->seekTo(_CCPageHeaderOffPType);
    page->pageType = (_CCPageType_E)buffer->readByte();
    if (readNode) {
        buffer->seekTo(_CCPageHeaderOffIndex);
        readPageNode(ctx, page, buffer, next);
    }
    CCBT_FRE_BUFFER(offset, false);
    
    return page;
}

void freePagePathNode(_CCPathNode_S *pNode) {
    if (pNode) {
        freePagePathNode(pNode->parent);
        pNode->parent = NULL;
        free(pNode);
    }
}

void freePage(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, BOOL root)
{
    if (CCBT_IS_ROOT_PAGE(page) && root == false) {
        return;
    }
    if (page && page->pageNodeArray) {
        CCArrayDeallocate(page->pageNodeArray);
        page->pageNodeArray = NULL;
    }
    if (page && page->subPageArray) {
        CCArrayDeallocate(page->subPageArray);
        page->subPageArray = NULL;
    }
    
    if (page) {
        freePagePathNode(page->pNode);
        free(page);
    }
}

static void readRootPage(_CCBPTreeIndexContext_S *ctx)
{
    CCBTPageOff_T rootOffset = ctx->rootOff;
    if (ctx->rootBuffer == NULL) {
        CCBTPageSize_T pageSize = ctx->pageSize;
        ctx->rootBuffer = ctx->fileMap->createMapBuffer(rootOffset, pageSize, CC_F_RDWR);
        if (ctx->rootBuffer == nullptr) {
            return;
        }
    }
    ctx->root = readPage(ctx, rootOffset, true);
}

static _PCCBPTreeIndexContext_S _CCBPTreeIndexContextCreate(PCCAllocator_S allocator, const string &indexFile, CCBTPageCnt_T pageNodeCnt, CCBTPageSize_T pageSize)
{
    _PCCBPTreeIndexContext_S ctx = (_CCBPTreeIndexContext_S *)calloc(1, sizeof(_CCBPTreeIndexContext_S));
    CCFileMap *fileMap = new CCFileMap(indexFile);
    ctx->fileMap = fileMap;
    fileMap->open(CC_F_RDWR);
    
    int64_t fileSize = ctx->fileMap->fileSize();
    CCBTPageSize_T headerLen = MAX(_CCBPTreeIndexHeaderLen_s,pageSize);
    
    CCBuffer *header = fileMap->createMapBuffer(0, headerLen, CC_F_RDWR);
    if (header == nullptr) {
        return nullptr;
    }
    ctx->headBuffer = header;
    
    if (fileSize > 0) {
        ctx->version = header->readLittleEndian16();
        ctx->pageNodeCnt = header->readLittleEndian16();
        CCUInt16_t opt = header->readLittleEndian16();
        ctx->option = opt;
        ctx->pageSize = CCBT_PAGE_SIZE_FROM_OP;
        ctx->rootOff = header->readLittleEndian64();
        ctx->EOFOff = header->readLittleEndian64();
        ctx->idleOff = header->readLittleEndian64();
    }
    else {
        CCBTPageSize_T pageSize = (CCBTPageSize_T)header->bufferSize();
        ctx->version = 1.0;
        ctx->pageNodeCnt = pageNodeCnt;
        CCBT_SET_OPTION(TYPEUINT_BITS_N(pageSize) & 31);
        CCBT_SET_COW(1);
        CCBT_SET_NEW(1);
        ctx->pageSize = CCBT_PAGE_SIZE_FROM_OP;
        ctx->rootOff = header->bufferSize();
        ctx->EOFOff = header->bufferSize();
        ctx->idleOff = 0;
        
        header->seekTo(_CCHeaderOffVersion);
        header->writeLittleEndian16(ctx->version);
        header->seekTo(_CCHeaderOffNodeCnt);
        header->writeLittleEndian16(ctx->pageNodeCnt);
        header->seekTo(_CCHeaderOffFlag);
        header->writeLittleEndian16(ctx->option);
        header->seekTo(_CCHeaderOffRoot);
        header->writeLittleEndian64(ctx->rootOff);
        header->seekTo(_CCHeaderOffEOF);
        header->writeLittleEndian64(ctx->EOFOff);
        header->seekTo(_CCHeaderOffIdle);
        header->writeLittleEndian64(ctx->idleOff);
    }
    
    createBufferCache(ctx, 4);
    
    readRootPage(ctx);
    
    return ctx;
}

static _CCPageFind_S lookupPage(_CCBPTreeIndexContext_S *ctx, _CCPathNode_S *pNode, CCBTPageOff_T offset, CCBTIndex_T *index, CCBTIndexSize_T indexLen, bool leaf)
{
    _CCPage_S *page = nullptr;
    if (offset == 0 || CCBT_IS_ROOT_OFF(offset)) {
        page = ctx->root;
    }
    if (!page) {
        page = readPage(ctx, offset, true);
    }
    page->pNode = pNode;
    _CCPageFind_S f;
    f.page = page;
    f.nodeIdx = 0;
    PCCArray_S pageNodeArray = page->pageNodeArray;
    CCCount nodeCnt = CCArrayGetCount(pageNodeArray);
    if (nodeCnt == 0) {
        return f;
    }
    
    PCCArray_S subPageArray = page->subPageArray;
    CCCount subPageCnt = CCArrayGetCount(subPageArray);
    
    CCBTPageCnt_T ps = 0;
    CCBTPageCnt_T pe = subPageCnt-1;
    while (ps <= pe) {
        CCBTPageCnt_T pIdx = (ps + pe)/2;
        _CCSubPage_S *subPage = (_CCSubPage_S*)CCArrayGetValueAtIndex(subPageArray, pIdx);;
        const CCBTPageOff_T offset = subPage->offset;
        
        CCBT_DEC_BUFFER(offset, buffer, bufferType);
        uint8_t *ptr = buffer->bytes();
        
        _CCPageNode_S *ns = (_CCPageNode_S*)CCArrayGetValueAtIndex(pageNodeArray, subPage->sIdx);
        int r = memcmp(index, ptr + ns->indexOffset, MIN(indexLen, ns->indexSize));
        
        //index < ns->index
        if (r < 0 || (r == 0 && indexLen < ns->indexSize)) {
            pe = pIdx - 1;
        }
        //index == ns->index
        else if (r == 0 && indexLen == ns->indexSize) {
            f.fcd = _CCFindCodeExists;
            f.nodeIdx = subPage->sIdx;
            f.subPageIdx = pIdx;
            CCBT_FRE_BUFFER(offset, false);
            break;
        }
        //index > ns->index
        else {
            _CCPageNode_S *ne = (_CCPageNode_S*)CCArrayGetValueAtIndex(pageNodeArray, subPage->eIdx);
            r = memcmp(index, ptr + ne->indexOffset, MIN(indexLen, ne->indexSize));
            if (r < 0 || (r == 0 && indexLen < ns->indexSize)) {
                //就在此page中寻找
                f.subPageIdx = pIdx;
                CCBTNodeCnt_T i = subPage->sIdx;
                CCBTNodeCnt_T j = subPage->eIdx;
                while (i <= j) {
                    CCBTNodeCnt_T m = (i + j)/2;
                    _CCPageNode_S *t = (_CCPageNode_S*)CCArrayGetValueAtIndex(pageNodeArray, m);
                    r = memcmp(index, ptr + t->indexOffset, MIN(indexLen, t->indexSize));
                    if (r < 0 || (r == 0 && indexLen < t->indexSize)) {
                        j = m - 1;
                    }
                    else if (r == 0 && indexLen == t->indexSize) {
                        f.fcd = _CCFindCodeExists;
                        f.nodeIdx = m;
                        break;
                    }
                    else {
                        i = m + 1;
                    }
                    //当i>=j时break
                    if (i >= j) {
                        if (i == j) {
                            ++i;
                        }
                        f.fcd = _CCFindCodeOK;
                        f.nodeIdx = i;
                        break;
                    }
                }
                CCBT_FRE_BUFFER(offset, false);
                break;
            }
            else if (r == 0 && indexLen == ne->indexSize) {
                f.fcd = _CCFindCodeExists;
                f.nodeIdx = subPage->eIdx;
                f.subPageIdx = pIdx;
                CCBT_FRE_BUFFER(offset, false);
                break;
            }
            else {
                ps = pIdx + 1;
            }
            //当ps>=pe时break
            if (ps >= pe) {
                if (ps == pe) {
                    ++ps;
                }
                f.fcd = _CCFindCodeOK;
                f.nodeIdx = ((_CCSubPage_S*)CCArrayGetValueAtIndex(subPageArray, ps))->sIdx;
                f.subPageIdx = ps;
                CCBT_FRE_BUFFER(offset, false);
                break;
            }
        }
        CCBT_FRE_BUFFER(offset, false);
    }
    
    CCBTPageOff_T nextPageOffset = 0;
    CCBTNodeCnt_T slotIdx = 0;
    if (f.fcd == _CCFindCodeOK) {
        if (f.nodeIdx >= 1) {
            slotIdx = f.nodeIdx;
            nextPageOffset = ((_CCPageNode_S*)CCArrayGetValueAtIndex(pageNodeArray, f.nodeIdx-1))->value;
        }
        else {
            slotIdx = 0;
            nextPageOffset = page->firstChild;
        }
    }
    else {
        slotIdx = f.nodeIdx;
        nextPageOffset = ((_CCPageNode_S*)CCArrayGetValueAtIndex(pageNodeArray, f.nodeIdx))->value;
    }
    f.value = nextPageOffset;
    
    if (leaf && nextPageOffset > 0 && (page->pageType & _CCPageTypeLeaf) != _CCPageTypeLeaf) {
        struct _CCPathNode *pInfoTmp = (_CCPathNode_S*)malloc(sizeof(_CCPathNode_S));
        if (pInfoTmp) {
            pInfoTmp->offset = page->pageOffset;
            pInfoTmp->parentSlotIdx = slotIdx;
            pInfoTmp->parent = pNode;
            return lookupPage(ctx, pInfoTmp, nextPageOffset, index, indexLen, leaf);
        }
    }
    return f;
}

static inline CCBTPageOff_T createNewPageOffset(_CCBPTreeIndexContext_S *ctx)
{
    if (ctx->newOff > 0) {
        CCBTPageOff_T newOff = ctx->newOff;
//        CCBT_UPDATE_HEADER_NEW(0)
        return newOff;
    }
    if (ctx->idleOff > 0) {
        const CCBTPageOff_T idleOff = ctx->idleOff;
        CCBT_DEC_BUFFER(idleOff, buffer, bufferType);
        buffer->seekTo(_CCPageHeaderOffNext);
        CCBTPageOff_T next = buffer->readLittleEndian64();
        memset(buffer->bytes(), 0, ctx->pageSize);
        CCBT_FRE_BUFFER(idleOff, false);
        
        CCBT_UPDATE_HEADER_IDLE(next);
        CCBT_UPDATE_HEADER_NEW(idleOff)
        return idleOff;
    }
    CCBTPageOff_T newPageOffset = ctx->EOFOff;
    CCBT_UPDATE_HEADER_EOF(newPageOffset + ctx->pageSize);
    CCBT_UPDATE_HEADER_NEW(newPageOffset)
    
    return newPageOffset;
}

static inline void addFreePageOffset(_CCBPTreeIndexContext_S *ctx, CCBTPageOff_T freeOffset)
{
    if (freeOffset <= 0) {
        return;
    }
    //将freeOffset 设置在上一个freepage的前面
    CCBT_DEC_BUFFER(freeOffset, buffer, bufferType);
    CCBTPageOff_T idleOff = ctx->idleOff;
    buffer->seekTo(_CCPageHeaderOffNext);
    buffer->writeLittleEndian64(idleOff);
    CCBT_FRE_BUFFER(freeOffset, false);
    
    CCBT_UPDATE_HEADER_IDLE(freeOffset);
}

static inline CCBTPageCnt_T getPageIdxFromNodeIdx(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBTNodeCnt_T nodeIdx)
{
    CCBTPageCnt_T pageIdx = 0;
    CCBTPageCnt_T pageCnt = CCBT_SUBPAGE_CNT(page);
    if (pageCnt == 1) {
        return 0;
    }
    for (pageIdx = 0; pageIdx < pageCnt; ++pageIdx) {
        _CCSubPage_S *sub = CCBT_PAGE_SUB_AT_IDX(page, pageIdx);
        if (nodeIdx >= sub->sIdx && nodeIdx <= sub->eIdx) {
            return pageIdx;
        }
    }
    return pageIdx;
}

//这个函数不能保证subPage的remSize一定>=needSize，如果不满足的话，nextSubPage的前面一定存在needSize的空间
static inline bool ensureSubPageRemSize(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBTPageCnt_T subPageIdx, CCBTPageSize_T needSize, CCBTNodeCnt_T maxMoveIdx, CCBTPageSize_T newPagePrevFreeSize)
{
    CCBTPageSize_T pageSize = ctx->pageSize;
    _CCSubPage_S *sub = CCBT_PAGE_SUB_AT_IDX(page, subPageIdx);
    const CCBTPageOff_T subOffset = sub->offset;
    //1.判断本sub是否有足够的剩余空间
    if (sub->remSize >= needSize) {
        return true;
    }
    CCBTPageCnt_T endIdx = sub->eIdx;
    //2.计算需要从后面copy几个index，value出去
    CCBTPageSize_T cpSize = 0;
    CCBTPageSize_T addSize = 0;
    CCBTNodeCnt_T copyItemCnt = 0;
    CCBTPageSize_T remSize = sub->remSize;
    
    //3.计算需要copy的size
    while (remSize < needSize && endIdx >= maxMoveIdx) {
        _CCPageNode_S *pageNode = CCBT_PAGE_NODE_AT_IDX(page, endIdx);
        addSize = CCBT_PAGE_NODE_SIZE(pageNode);
        cpSize += addSize;
        remSize += addSize;
        --endIdx;
        ++copyItemCnt;
    }
    
    _CCPageNode_S *endNode = CCBT_PAGE_NODE_AT_IDX(page, endIdx + 1);
    CCBTPageSize_T copyFrom = CCBT_PAGE_NODE_OFFSET(endNode);
    CCBTPageSize_T newSize = cpSize;
    CCBTPageSize_T nextSubIndexOffset = _CCPageHeaderOffIndex;
    //如果此subPage还是不足needSize，则将下一个subPage前面预留needSize的大小
    if (remSize < needSize) {
        newSize = cpSize + needSize;
        nextSubIndexOffset += needSize;
    }
    
    bool result = false;
    bool createNewPage = true;
    CCBTPageOff_T nextOffset = 0;
    CCBTPageCnt_T nextSubPageIdx = subPageIdx + 1;
    CCBTPageCnt_T pageCnt = CCBT_SUBPAGE_CNT(page);
    CCBT_DEC_BUFFER(subOffset, buffer, bufType);
    /*如果下一个subPage不是最后一个page，并且有newSize的剩余空间，
     *直接将前面的subPage从后面拷贝过来，不创建新的subPage
     */
    if (nextSubPageIdx < pageCnt) {
        _CCSubPage_S *nextSub = CCBT_PAGE_SUB_AT_IDX(page, nextSubPageIdx);
        CCBTPageSize_T nextSubPageRemSize = nextSub->remSize;
        if (nextSubPageRemSize > newSize) {
            createNewPage = false;
            const CCBTPageOff_T nextSubOffset = nextSub->offset;
            CCBT_DEC_BUFFER(nextSubOffset, nextBuffer, nextBufferType);
            uint8_t *ptr = nextBuffer->bytes() + _CCPageHeaderOffIndex;
            memmove(ptr + newSize, ptr, pageSize - nextSub->remSize - _CCPageHeaderOffIndex);
            memset(ptr, 0, newSize);
            nextBuffer->seekTo(nextSubIndexOffset);
            nextBuffer->writeBuffer(buffer->bytes() + copyFrom, cpSize);
            memset(buffer->bytes() + copyFrom, 0, cpSize);
            CCBT_FRE_BUFFER(nextSubOffset, false);
            nextSub->remSize -= newSize;
            nextSub->sIdx -= copyItemCnt;
        }
        else {
            //需要创建一个新的subPage
            nextOffset = nextSub->offset;
        }
    }
    sub->eIdx = endIdx;
    sub->remSize = remSize;
    
    if (createNewPage) {
        const CCBTPageOff_T nextNewPageOffset = createNewPageOffset(ctx);
        //修改当前subPage的next
        buffer->seekTo(_CCPageHeaderOffNext);
        buffer->writeLittleEndian64(nextNewPageOffset);
        
        //更改新的subPage的next
        CCBT_DEC_BUFFER(nextNewPageOffset, nextBuffer, nextBufferType);
        
        nextBuffer->seekTo(_CCPageHeaderOffNext);
        nextBuffer->writeLittleEndian64(nextOffset);
        nextBuffer->seekTo(nextSubIndexOffset);
        nextBuffer->writeBuffer(buffer->bytes() + copyFrom, cpSize);
        memset(buffer->bytes() + copyFrom, 0, cpSize);
        CCBT_FRE_BUFFER(nextNewPageOffset, false);
        
        resizeSubPageArray(ctx, page, false);
        _CCSubPage_S *subPage = (_CCSubPage_S*)calloc(1, sizeof(_CCSubPage_S));
        if (!subPage) {
            goto  ENSURE_SUBPAGE_REMSIZE_END;
        }
        subPage->sIdx = endIdx + 1;
        subPage->eIdx = endIdx + copyItemCnt;
        subPage->remSize = pageSize - newSize - _CCPageHeaderOffIndex;
        subPage->offset = nextNewPageOffset;
        CCArrayInsertValueAtIndex(page->subPageArray, nextSubPageIdx, (CCU64Ptr)subPage);
    }

    result = true;
ENSURE_SUBPAGE_REMSIZE_END:
    CCBT_FRE_BUFFER(subOffset, false);
    return result;
}

static void insertIndexIntoNotFullPage(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBTPageCnt_T subPageIdx, CCBTNodeCnt_T nodeIdx, CCBTIndexValue_T prevIndexValue, CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue)
{
    CCBTNodeCnt_T pageNodeCnt = CCBT_NODE_CNT(page);
    if (pageNodeCnt >= ctx->pageNodeCnt) {
        return;
    }
    
    _CCSubPage_S *sub = CCBT_PAGE_SUB_AT_IDX(page, subPageIdx);
    CCBTNodeCnt_T endIdx = sub->eIdx;
    CCBTPageOff_T offset = sub->offset;
    CCBTPageSize_T needSize = indexLen + CCBT_PAGE_NODE_IDX_SIZE_BYTES + CCBT_PAGE_NODE_IDX_VALUE_BYTES;
    
    //确保本subPage有足够的剩余空间，如果不足，取下一个subPage
    ensureSubPageRemSize(ctx, page, subPageIdx, needSize, nodeIdx, needSize);
    bool remSizeEnough = true;
    if (sub->remSize < needSize) {
        remSizeEnough = false;
        offset = CCBT_PAGE_SUB_AT_IDX(page, subPageIdx + 1)->offset;
    }
    
    CCBT_DEC_BUFFER(offset, buffer, bufType);
    
    //如果是rootPage，并且pageType== _CCPageTypeNone，修改为_CCPageTypeRoot | _CCPageTypeLeaf
    if (CCBT_IS_ROOT_OFF(offset) && ctx->root->pageType == _CCPageTypeNone) {
        ctx->root->pageType = (_CCPageType_E)(_CCPageTypeRoot | _CCPageTypeLeaf);
        buffer->seekTo(_CCPageHeaderOffPType);
        buffer->writeByte(ctx->root->pageType);
    }
    
    /*（在进行分裂时，向上产生了一个新的page，此时prevIndexValue就是有值的）
    * 如果前面的indexValue有值，此prevIndexValue就是firstChild
     */
    if (prevIndexValue > 0 && nodeIdx == 0) {
        /*如果向上分裂的page存在node的时候，而插入的indexValue是为0，说明新插入的indexValue就是本page的firstChild
         */
        if (indexValue == 0) {
            indexValue = page->firstChild;
        }
        page->firstChild = prevIndexValue;
        buffer->seekTo(_CCPageHeaderOffFChild);
        buffer->writeLittleEndian64(prevIndexValue);
    }
    
    CCBTPageSize_T endOff = 0;
    if (remSizeEnough) {
        endIdx = sub->eIdx;
        endOff = endIdx == 0 ? _CCPageHeaderOffIndex : CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, endIdx));
        if (endIdx >= nodeIdx && endIdx > 0) {
            CCBTPageSize_T offsetTmp = CCBT_PAGE_NODE_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, nodeIdx));
            uint8_t *ptr = buffer->bytes() + offsetTmp;
            CCBTPageSize_T move = endOff - offsetTmp;
            memmove(ptr + needSize, ptr, move);
            buffer->seekTo(offsetTmp);
        }
        else {
            buffer->seekTo(endOff);
        }
        
        buffer->writeByte(indexLen);
        buffer->writeBuffer(index, indexLen);
        buffer->writeLittleEndian64(indexValue);
    }
    else {
        endOff = _CCPageHeaderOffIndex;
        buffer->seekTo(_CCPageHeaderOffIndex);
        buffer->writeByte(indexLen);
        buffer->writeBuffer(index, indexLen);
        buffer->writeLittleEndian64(indexValue);
    }

    CCBT_FRE_BUFFER(offset, false);
    if (page == ctx->root) {
        _CCPageNode_S *insert = (_CCPageNode_S*)calloc(1, sizeof(_CCPageNode_S));
        if (!insert) {
            return;
        }
        insert->indexOffset = endOff;
        insert->indexSize = indexLen;
        insert->value = indexValue;
        CCArrayInsertValueAtIndex(page->pageNodeArray, nodeIdx, (CCU64Ptr)insert);
    }
}

static int insertIndexWithPathNode(_CCBPTreeIndexContext_S *ctx, _CCPathNode_S *pathNode, CCBTIndexValue_T prevIndexValue, CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue)
{
    _CCPage_S *page = NULL;
    CCBTNodeCnt_T nodeCnt = 0;
    CCBTNodeCnt_T findNodeIdx = 0;
    CCBTPageCnt_T findSubPageIdx = 0;
    PCCArray_S pageNodeArray = NULL;
    if (pathNode) {
        _CCPageFind_S find = lookupPage(ctx, NULL, pathNode->offset, index, indexLen, false);
        if (find.fcd == _CCFindCodeExists) {
            return find.fcd;
        }
        if (find.page == NULL) {
            return -1;
        }
        page = find.page;
        pageNodeArray = page->pageNodeArray;
        nodeCnt = CCArrayGetCount(pageNodeArray);
        findNodeIdx = find.nodeIdx;
        findSubPageIdx = find.subPageIdx;
    }
    else {
        page = ctx->root;
    }

    if (nodeCnt < ctx->pageNodeCnt) {
        insertIndexIntoNotFullPage(ctx, page, findSubPageIdx, findNodeIdx, prevIndexValue, index, indexLen, indexValue);
        freePage(ctx, page, false);
    }
    else {
        //进行分裂
        CCBTPageCnt_T pageCnt = CCBT_SUBPAGE_CNT(page);
        PCCArray_S subPageArray = page->subPageArray;
        bool isLeafPage = (page->pageType & _CCPageTypeLeaf) == _CCPageTypeLeaf;
        CCBTNodeCnt_T mid = ctx->pageNodeCnt >> 1;
        CCBTNodeCnt_T prevCnt = mid;
        CCBTNodeCnt_T insertIntoParentIdx = prevCnt;
        bool copyFromPrev = false;
        bool isInsertPrev = false;
        //1.找出分裂的位置
        /*
         *如果是奇数个，偶数个时进行分裂
         *1、如果是叶子节点分裂的话，两边各一半，将左边最大的index copy到父节点
         *2、如果是索引节点分裂，左边pageNodeCnt/2-1，右边pageNodeCnt/2，将左边最大的index移动到父节点
         */
        if (IS_ODD_NUM(ctx->pageNodeCnt)) {
            if (findNodeIdx < mid) {
                isInsertPrev = true;
            }
            else {
                prevCnt += 1;
            }
            insertIntoParentIdx = prevCnt - 1;
            copyFromPrev = true;
        }
        else {
            /*
             *如果是偶数个，奇数时进行分裂
             *1、如果是叶子节点分裂的话，左边pageNodeCnt/2，右边是ctx->pageNodeCnt - pageNodeCnt/2,将右边的最小索引copy到父节点
             *2、如果是索引节点分裂的话，左右两边都是pageNodeCnt/2，将中间的节点移动到父节点中
             */
            if (findNodeIdx < mid) {
                prevCnt -= 1;
                isInsertPrev = true;
            }
            insertIntoParentIdx = prevCnt;
        }
        
        //2. 计算父类的offset
        CCBTPageOff_T parent = 0;
        bool newParentOffset = false;
        if (pathNode->parent) {
            parent = pathNode->parent->offset;
        }
        if (parent == 0) {
            newParentOffset = true;
            parent = createNewPageOffset(ctx);
        }

        //3. 修改当前的page
        const CCBTPageOff_T curPageOffset = page->pageOffset;
        CCBT_DEC_BUFFER(curPageOffset, buffer, bufferType);

        //3.1修改pageType
        //如果pageType就是root，那就是修改为index
        if (page->pageType == _CCPageTypeRoot) {
            page->pageType = _CCPageTypeIndex;
            buffer->seekTo(_CCPageHeaderOffPType);
            buffer->writeByte(page->pageType);
        }
        //如果pageType包含root，那么这个page就即使root也是leaf(叶子)，那么就将该page修改leaf page
        else if (page->pageType & _CCPageTypeRoot) {
            page->pageType = _CCPageTypeLeaf;
            buffer->seekTo(_CCPageHeaderOffPType);
            buffer->writeByte(page->pageType);
        }
        
        //3.2修改page的brother
        bool newBrotherOffset = false;
        CCBTPageOff_T brother = 0;
        //兄弟节点的next
        CCBTPageOff_T brotherNextOffset = 0;
        //取prevCnt（要分裂出去的）所在的pageIdx
        CCBTPageCnt_T nextSubPageIdx = getPageIdxFromNodeIdx(ctx, page, prevCnt);
        _CCSubPage_S *split = CCBT_PAGE_SUB_AT_IDX(page,nextSubPageIdx);
        if (split->sIdx == prevCnt) {
            brother = split->offset;
        }
        else {
            newBrotherOffset = true;
            brother = createNewPageOffset(ctx);
            CCBTPageCnt_T nextSubPageIdxTmp = nextSubPageIdx + 1;
            if (nextSubPageIdxTmp < CCBT_SUBPAGE_CNT(page)) {
                brotherNextOffset = CCBT_PAGE_SUB_AT_IDX(page, nextSubPageIdxTmp)->offset;
            }
        }
        //如果是B*树，都有brother
        if (isLeafPage) {
            buffer->seekTo(_CCPageHeaderOffBrother);
            buffer->writeLittleEndian64(brother);
        }
        
        //3.3修改page的next为0
        CCBTPageOff_T prevOffsetTmp = 0;
        _CCSubPage_S *prevSubPage = NULL;
        /*如果是新建brother，则split就是第一个subPage
         *否则prevSubPageIdx就是第一个subPage
         */
        if (newBrotherOffset) {
            prevOffsetTmp = split->offset;
            prevSubPage = split;
        }
        else {
            CCBTPageCnt_T prevSubPageIdx = 0;
            //在这里,这个nextSubPageIdx肯定会大于0
            if (nextSubPageIdx >= 1) {
                prevSubPageIdx = nextSubPageIdx - 1;
            }
            else {
                printf("error\n");
            }
            _CCSubPage_S *prev = CCBT_PAGE_SUB_AT_IDX(page, prevSubPageIdx);
            prevOffsetTmp = prev->offset;
            prevSubPage = prev;
        }

        //创建prevBuffer，将prevPage的最后一个subPage的next设置为0；
        const CCBTPageOff_T prevSubOffset = prevOffsetTmp;
        CCBT_DEC_BUFFER(prevSubOffset, prevBuffer, prevBufferType);

        prevBuffer->seekTo(_CCPageHeaderOffNext);
        prevBuffer->writeLittleEndian64(0);
        
        //4.修改brother的page
        //索引节点第一个孩子节点
        const CCBTPageOff_T brotherOffset = brother;
        CCBTPageOff_T brotherFirstChild = indexValue;
        CCBT_DEC_BUFFER(brotherOffset, brotherBuffer, brotherBufferType);
        
        //4.1 copy 需要插入到parent的index，value
        bool useCurIndex = false;
        CCBTPageSize_T insertIntoParentIndexSize = 0;
        if (insertIntoParentIdx == findNodeIdx) {
            useCurIndex = true;
            insertIntoParentIndexSize = indexLen;
        }
        else {
            insertIntoParentIndexSize = CCBT_PAGE_NODE_AT_IDX(page, insertIntoParentIdx)->indexSize;
        }
        insertIntoParentIndexSize += CCBT_PAGE_NODE_IDX_VALUE_BYTES;
        CCMutableBuffer insertIntoParentIndex = CCMutableBuffer(insertIntoParentIndexSize);
        if (useCurIndex) {
            insertIntoParentIndex.writeBuffer(index, indexLen);
            insertIntoParentIndex.writeLittleEndian64(indexValue);
        }
        else {
            uint8_t *ptr = prevBuffer->bytes() + CCBT_PAGE_NODE_AT_IDX(page, insertIntoParentIdx)->indexOffset;
            insertIntoParentIndex.writeBuffer(ptr, insertIntoParentIdx);
            if (!isLeafPage) {
                memset(ptr - CCBT_PAGE_NODE_IDX_SIZE_BYTES, 0, insertIntoParentIndexSize + CCBT_PAGE_NODE_IDX_SIZE_BYTES);
            }
            CCBTPageOff_T off = insertIntoParentIndexSize - CCBT_PAGE_NODE_IDX_VALUE_BYTES;
            insertIntoParentIndex.seekTo(off);
            brotherFirstChild = insertIntoParentIndex.readLittleEndian64();
            //修改插入到parentPage的索引value为brother
            insertIntoParentIndex.seekTo(off);
            insertIntoParentIndex.writeLittleEndian64(brother);
        }
        if (isLeafPage == false) {
            brotherBuffer->seekTo(_CCPageHeaderOffFChild);
            brotherBuffer->writeLittleEndian64(brotherFirstChild);
        }
        brotherBuffer->seekTo(_CCPageHeaderOffBrother);
        brotherBuffer->writeLittleEndian64(page->brother);
        brotherBuffer->seekTo(_CCPageHeaderOffPType);
        brotherBuffer->writeByte(page->pageType);
        page->brother = brother;
        
        if (newBrotherOffset) {
            //4.1 计算需要copy到insertIntoParentIndex的数据
            CCBTPageCnt_T cpIdx = prevCnt;
            if (useCurIndex == false && !isLeafPage) {
                cpIdx = prevCnt + 1;
            }
            
            brotherBuffer->bzero();
            brotherBuffer->seekTo(_CCPageHeaderOffNext);
            brotherBuffer->writeLittleEndian64(brotherNextOffset);
            
            //判断是否有数据需要copy到brotherBuffer
            if (cpIdx <= prevSubPage->eIdx) {
                CCBTPageSize_T sOff = CCBT_PAGE_NODE_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, cpIdx));
                CCBTPageSize_T eOff = CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, split->eIdx));
                if (eOff > sOff) {
                    CCBTPageSize_T cp = eOff - sOff;
                    uint8_t *ptr = prevBuffer->bytes() + sOff;
                    brotherBuffer->seekTo(_CCPageHeaderOffIndex);
                    brotherBuffer->writeBuffer(ptr, cp);
                    memset(ptr, 0, cp);
                }
            }
        }
        else {
            //如果不是新的brother，那就是原来的split的subPage
            if (useCurIndex == false && !isLeafPage && !copyFromPrev && insertIntoParentIdx < split->eIdx) {
                //将后面的往前面移动
                uint8_t *ptr = brotherBuffer->bytes() + _CCPageHeaderOffIndex;
                CCBTPageSize_T cpSize = CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, split->eIdx)) - CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, insertIntoParentIdx));
                memmove(ptr, ptr + insertIntoParentIndexSize, cpSize);
                memset(ptr + cpSize, 0, ctx->pageSize - cpSize);
            }
        }
        
        CCBT_FRE_BUFFER(curPageOffset, false);
        CCBT_FRE_BUFFER(prevSubOffset, false);
        CCBT_FRE_BUFFER(brotherOffset, false);
        
        //5 来到parent进行插入
        
        //如果是叶子节点或者插入的和移动到parent的不是同一个，就需要插入
        if (isLeafPage || useCurIndex == false) {
            //在前半段插入
           if (isInsertPrev) {
               CCBTNodeCnt_T r = nodeCnt - prevCnt;
               CCArrayRemoveValuesAtRange(pageNodeArray, CCRangeMake(prevCnt, r));
               r = pageCnt - nextSubPageIdx;
               CCArrayRemoveValuesAtRange(subPageArray, CCRangeMake(nextSubPageIdx, r));
               CCBTPageCnt_T subPageIdx = getPageIdxFromNodeIdx(ctx, page, findNodeIdx);
               insertIndexIntoNotFullPage(ctx, page, subPageIdx, findNodeIdx, 0, index, indexLen, indexValue);
           }
           else {
               //在后半段插入
               CCArrayRemoveValuesAtRange(pageNodeArray, CCRangeMake(0, prevCnt));
               CCArrayRemoveValuesAtRange(subPageArray, CCRangeMake(0, nextSubPageIdx));
               CCBTPageCnt_T subPageIdx = getPageIdxFromNodeIdx(ctx, page, findNodeIdx) - nextSubPageIdx;
               insertIndexIntoNotFullPage(ctx, page, subPageIdx, findNodeIdx, 0, index, indexLen, indexValue);
           }
        }
        
        freePage(ctx, page, false);
        //如果分裂的是root节点，修改ctx的root，rootOffset，rootBuffer
        if (curPageOffset == ctx->rootOff) {
            ctx->rootOff = parent;
            ctx->headBuffer->seekTo(_CCHeaderOffRoot);
            ctx->headBuffer->writeLittleEndian64(ctx->rootOff);
            readRootPage(ctx);
        }
        
        //在父节点中插入
        if (useCurIndex) {
            insertIndexWithPathNode(ctx, pathNode->parent, curPageOffset, index, indexLen, brother);
        }
        else {
            uint8_t *indexTmp = insertIntoParentIndex.bytes();
            CCBTIndexSize_T indexLenTmp = insertIntoParentIndexSize - CCBT_PAGE_NODE_IDX_VALUE_BYTES;
            insertIndexWithPathNode(ctx, pathNode->parent, curPageOffset, indexTmp, indexLenTmp, brother);
        }
    }
    return 0;
}

static int insertIndex(_CCBPTreeIndexContext_S *ctx, CCBTPageOff_T pageOffset, CCBTIndexValue_T prevIndexValue, CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue)
{
    _CCPageFind_S find = lookupPage(ctx, nullptr, pageOffset, index, indexLen, true);
    if (find.fcd == _CCFindCodeExists) {
        return find.fcd;
    }
    if (find.page == NULL) {
        return -1;
    }
    _CCPage_S *page = find.page;
    int r = insertIndexWithPathNode(ctx, page->pNode, prevIndexValue, index, indexLen, indexValue);
    freePage(ctx, page, false);
    return r;
#if 0
    PCCArray_S pageNodeArray = page->pageNodeArray;
    CCBTNodeCnt_T nodeCnt = CCArrayGetCount(pageNodeArray);
    if (nodeCnt < ctx->pageNodeCnt) {
        insertIndexIntoNotFullPage(ctx, page, find.subPageIdx, find.nodeIdx, prevIndexValue, index, indexLen, indexValue);
        freePage(ctx, page);
    }
    else {
        //进行分裂
        CCBTPageCnt_T pageCnt = CCBT_SUBPAGE_CNT(page);
        PCCArray_S subPageArray = page->subPageArray;
        bool isLeafPage = (page->pageType & _CCPageTypeLeaf) == _CCPageTypeLeaf;
        CCBTNodeCnt_T mid = ctx->pageNodeCnt >> 1;
        CCBTNodeCnt_T prevCnt = mid;
        CCBTNodeCnt_T insertIntoParentIdx = prevCnt;
        bool copyFromPrev = false;
        bool isInsertPrev = false;
        //1.找出分裂的位置
        /*
         *如果是奇数个，偶数个时进行分裂
         *1、如果是叶子节点分裂的话，两边各一半，将左边最大的index copy到父节点
         *2、如果是索引节点分裂，左边pageNodeCnt/2-1，右边pageNodeCnt/2，将左边最大的index移动到父节点
         */
        if (IS_ODD_NUM(ctx->pageNodeCnt)) {
            if (find.nodeIdx < mid) {
                isInsertPrev = true;
            }
            else {
                prevCnt += 1;
            }
            insertIntoParentIdx = prevCnt - 1;
            copyFromPrev = true;
        }
        else {
            /*
             *如果是偶数个，奇数时进行分裂
             *1、如果是叶子节点分裂的话，左边pageNodeCnt/2，右边是ctx->pageNodeCnt - pageNodeCnt/2,将右边的最小索引copy到父节点
             *2、如果是索引节点分裂的话，左右两边都是pageNodeCnt/2，将中间的节点移动到父节点中
             */
            if (find.nodeIdx < mid) {
                prevCnt -= 1;
                isInsertPrev = true;
            }
            insertIntoParentIdx = prevCnt;
        }
        
        //2. 计算父类的offset
        CCBTPageOff_T parent = 0;
        bool newParentOffset = false;
        if (page->pNode) {
            parent = page->pNode->offset;
        }
        if (parent == 0) {
            newParentOffset = true;
            parent = createNewPageOffset(ctx);
        }

        //3. 修改当前的page
        const CCBTPageOff_T curPageOffset = page->pageOffset;
        CCBT_DEC_BUFFER(curPageOffset, buffer, bufferType);
//        if (buffer == NULL) {
//            freePage(ctx, page);
//            if (newParentOffset) {
//                addFreePageOffset(ctx, parent);
//            }
//            return -1;
//        }
        
        //3.1修改pageType
        //如果pageType就是root，那就是修改为index
        if (page->pageType == _CCPageTypeRoot) {
            page->pageType = _CCPageTypeIndex;
            buffer->seekTo(_CCPageHeaderOffPType);
            buffer->writeByte(page->pageType);
        }
        //如果pageType包含root，那么这个page就即使root也是leaf(叶子)，那么就将该page修改leaf page
        else if (page->pageType & _CCPageTypeRoot) {
            page->pageType = _CCPageTypeLeaf;
            buffer->seekTo(_CCPageHeaderOffPType);
            buffer->writeByte(page->pageType);
        }
        
        //3.2修改page的brother
        bool newBrotherOffset = false;
        CCBTPageOff_T brother = 0;
        //兄弟节点的next
        CCBTPageOff_T brotherNextOffset = 0;
        //取prevCnt（要分裂出去的）所在的pageIdx
        CCBTPageCnt_T nextSubPageIdx = getPageIdxFromNodeIdx(ctx, page, prevCnt);
        _CCSubPage_S *split = CCBT_PAGE_SUB_AT_IDX(page,nextSubPageIdx);
        if (split->sIdx == prevCnt) {
            brother = split->offset;
        }
        else {
            newBrotherOffset = true;
            brother = createNewPageOffset(ctx);
            CCBTPageCnt_T nextSubPageIdxTmp = nextSubPageIdx + 1;
            if (nextSubPageIdxTmp < CCBT_SUBPAGE_CNT(page)) {
                brotherNextOffset = CCBT_PAGE_SUB_AT_IDX(page, nextSubPageIdxTmp)->offset;
            }
        }
        //如果是B*树，都有brother
        if (isLeafPage) {
            buffer->seekTo(_CCPageHeaderOffBrother);
            buffer->writeLittleEndian64(brother);
        }
        
        //3.3修改page的next为0
        CCBTPageOff_T prevOffsetTmp = 0;
        _CCSubPage_S *prevSubPage = NULL;
        /*如果是新建brother，则split就是第一个subPage
         *否则prevSubPageIdx就是第一个subPage
         */
        if (newBrotherOffset) {
            prevOffsetTmp = split->offset;
            prevSubPage = split;
        }
        else {
            CCBTPageCnt_T prevSubPageIdx = 0;
            //在这里,这个nextSubPageIdx肯定会大于0
            if (nextSubPageIdx >= 1) {
                prevSubPageIdx = nextSubPageIdx - 1;
            }
            else {
                printf("error\n");
            }
            _CCSubPage_S *prev = CCBT_PAGE_SUB_AT_IDX(page, prevSubPageIdx);
            prevOffsetTmp = prev->offset;
            prevSubPage = prev;
        }

        //创建prevBuffer，将prevPage的最后一个subPage的next设置为0；
        const CCBTPageOff_T prevSubOffset = prevOffsetTmp;
        CCBT_DEC_BUFFER(prevSubOffset, prevBuffer, prevBufferType);
//        if (prevBuffer == nullptr) {
//            if (newBrotherOffset) {
//                addFreePageOffset(ctx, brother);
//            }
//            if (newParentOffset) {
//                addFreePageOffset(ctx, parent);
//            }
//            CCBT_FRE_BUFFER(pageOffset, false);
//            return -1;
//        }
        prevBuffer->seekTo(_CCPageHeaderOffNext);
        prevBuffer->writeLittleEndian64(0);
        
        //4.修改brother的page
        //索引节点第一个孩子节点
        const CCBTPageOff_T brotherOffset = brother;
        CCBTPageOff_T brotherFirstChild = indexValue;
        CCBT_DEC_BUFFER(brotherOffset, brotherBuffer, brotherBufferType);
//        if (brotherBuffer == NULL) {
//            if (newBrotherOffset) {
//                addFreePageOffset(ctx, brother);
//            }
//            if (newParentOffset) {
//                addFreePageOffset(ctx, parent);
//            }
//            CCBT_FRE_BUFFER(buffer, bufferType);
//            CCBT_FRE_BUFFER(prevBuffer, prevBufferType);
//            return -1;
//        }
        
        //4.1 copy 需要插入到parent的index，value
        bool useCurIndex = false;
        CCBTPageSize_T insertIntoParentIndexSize = 0;
        if (insertIntoParentIdx == find.nodeIdx) {
            useCurIndex = true;
            insertIntoParentIndexSize = indexLen;
        }
        else {
            insertIntoParentIndexSize = CCBT_PAGE_NODE_AT_IDX(page, insertIntoParentIdx)->indexSize;
        }
        insertIntoParentIndexSize += CCBT_PAGE_NODE_IDX_VALUE_BYTES;
        CCMutableBuffer insertIntoParentIndex = CCMutableBuffer(insertIntoParentIndexSize);
        if (useCurIndex) {
            insertIntoParentIndex.writeBuffer(index, indexLen);
            insertIntoParentIndex.writeLittleEndian64(indexValue);
        }
        else {
            uint8_t *ptr = prevBuffer->bytes() + CCBT_PAGE_NODE_AT_IDX(page, insertIntoParentIdx)->indexOffset;
            insertIntoParentIndex.writeBuffer(ptr, insertIntoParentIdx);
            if (!isLeafPage) {
                memset(ptr - CCBT_PAGE_NODE_IDX_SIZE_BYTES, 0, insertIntoParentIndexSize + CCBT_PAGE_NODE_IDX_SIZE_BYTES);
            }
            CCBTPageOff_T off = insertIntoParentIndexSize - CCBT_PAGE_NODE_IDX_VALUE_BYTES;
            insertIntoParentIndex.seekTo(off);
            brotherFirstChild = insertIntoParentIndex.readLittleEndian64();
            //修改插入到parentPage的索引value为brother
            insertIntoParentIndex.seekTo(off);
            insertIntoParentIndex.writeLittleEndian64(brother);
        }
        if (isLeafPage == false) {
            brotherBuffer->seekTo(_CCPageHeaderOffFChild);
            brotherBuffer->writeLittleEndian64(brotherFirstChild);
        }
        brotherBuffer->seekTo(_CCPageHeaderOffBrother);
        brotherBuffer->writeLittleEndian64(page->brother);
        brotherBuffer->seekTo(_CCPageHeaderOffPType);
        brotherBuffer->writeByte(page->pageType);
        page->brother = brother;
        
        if (newBrotherOffset) {
            //4.1 计算需要copy到insertIntoParentIndex的数据
            CCBTPageCnt_T cpIdx = prevCnt;
            if (useCurIndex == false && !isLeafPage) {
                cpIdx = prevCnt + 1;
            }
            
            brotherBuffer->bzero();
            brotherBuffer->seekTo(_CCPageHeaderOffNext);
            brotherBuffer->writeLittleEndian64(brotherNextOffset);
            
            //判断是否有数据需要copy到brotherBuffer
            if (cpIdx <= prevSubPage->eIdx) {
                CCBTPageSize_T sOff = CCBT_PAGE_NODE_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, cpIdx));
                CCBTPageSize_T eOff = CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, split->eIdx));
                if (eOff > sOff) {
                    CCBTPageSize_T cp = eOff - sOff;
                    uint8_t *ptr = prevBuffer->bytes() + sOff;
                    brotherBuffer->seekTo(_CCPageHeaderOffIndex);
                    brotherBuffer->writeBuffer(ptr, cp);
                    memset(ptr, 0, cp);
                }
            }
        }
        else {
            //如果不是新的brother，那就是原来的split的subPage
            if (useCurIndex == false && !isLeafPage && !copyFromPrev && insertIntoParentIdx < split->eIdx) {
                //将后面的往前面移动
                uint8_t *ptr = brotherBuffer->bytes() + _CCPageHeaderOffIndex;
                CCBTPageSize_T cpSize = CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, split->eIdx)) - CCBT_PAGE_NODE_END_OFFSET(CCBT_PAGE_NODE_AT_IDX(page, insertIntoParentIdx));
                memmove(ptr, ptr + insertIntoParentIndexSize, cpSize);
                memset(ptr + cpSize, 0, ctx->pageSize - cpSize);
            }
        }
        
        CCBT_FRE_BUFFER(curPageOffset, false);
        CCBT_FRE_BUFFER(prevSubOffset, false);
        CCBT_FRE_BUFFER(brotherOffset, false);
        
        //5 来到parent进行插入
        
        //如果是叶子节点或者插入的和移动到parent的不是同一个，就需要插入
        if (isLeafPage || useCurIndex == false) {
            //在前半段插入
           if (isInsertPrev) {
               CCBTNodeCnt_T r = nodeCnt - prevCnt;
               CCArrayRemoveValuesAtRange(pageNodeArray, CCRangeMake(prevCnt, r));
               r = pageCnt - nextSubPageIdx;
               CCArrayRemoveValuesAtRange(subPageArray, CCRangeMake(nextSubPageIdx, r));
               CCBTPageCnt_T subPageIdx = getPageIdxFromNodeIdx(ctx, page, find.nodeIdx);
               insertIndexIntoNotFullPage(ctx, page, subPageIdx, find.nodeIdx, 0, index, indexLen, indexValue);
           }
           else {
               //在后半段插入
               CCArrayRemoveValuesAtRange(pageNodeArray, CCRangeMake(0, prevCnt));
               CCArrayRemoveValuesAtRange(subPageArray, CCRangeMake(0, nextSubPageIdx));
               CCBTPageCnt_T subPageIdx = getPageIdxFromNodeIdx(ctx, page, find.nodeIdx) - nextSubPageIdx;
               insertIndexIntoNotFullPage(ctx, page, subPageIdx, find.nodeIdx, 0, index, indexLen, indexValue);
           }
        }
        
        freePage(ctx, page);
        //如果分裂的是root节点，修改ctx的root，rootOffset，rootBuffer
        if (curPageOffset == ctx->rootOffset) {
            ctx->rootOffset = parent;
            ctx->headBuffer->seekTo(_CCHeaderOffRoot);
            ctx->headBuffer->writeLittleEndian64(ctx->rootOffset);
            readRootPage(ctx);
        }
        
        //在父节点中插入
        if (useCurIndex) {
            insertIndex(ctx, parent, curPageOffset, index, indexLen, brother, false);
        }
        else {
            uint8_t *indexTmp = insertIntoParentIndex.bytes();
            CCBTIndexSize_T indexLenTmp = insertIntoParentIndexSize - CCBT_PAGE_NODE_IDX_VALUE_BYTES;
            insertIndex(ctx, parent, curPageOffset, indexTmp, indexLenTmp, brother, false);
        }
    }
    return 0;
#endif
}

//返回left的brother的offset，通过rightBrother返回rightBrother
static CCBTPageOff_T findBrother(_CCBPTreeIndexContext_S *ctx, CCBTPageOff_T parent, CCBTNodeCnt_T slotIdx, CCBTPageOff_T *rightBrother, _CCPage_S **ptr_parentPage)
{
    if (parent == 0) {
        return 0;
    }
    CCBTPageOff_T lBrother = 0;
    CCBTPageOff_T rBrother = 0;
    _CCPage_S *parentPage = ctx->root;
    
    CCBT_DEC_BUFFER(parent, buffer, bufType)
    if (!CCBT_IS_ROOT_OFF(parent)) {
        parentPage = readPage(ctx, parent, true);
    }
    CCBTNodeCnt_T cnt = CCBT_NODE_CNT(parentPage);
    if (slotIdx == 0) {
        lBrother = 0;
        if (cnt > 0) {
            rBrother = CCBT_PAGE_NODE_AT_IDX(parentPage, 0)->value;
        }
    }
    else {
        CCBTNodeCnt_T lIdx = slotIdx - 1;
        if (lIdx == 0) {
            lBrother = parentPage->firstChild;
        }
        else if (lIdx > 0 && lIdx <= cnt) {
            lBrother = CCBT_PAGE_NODE_AT_IDX(parentPage, lIdx - 1)->value;
        }
        
        if (slotIdx >= 0 && slotIdx < cnt) {
            rBrother = CCBT_PAGE_NODE_AT_IDX(parentPage, slotIdx)->value;
        }
    }
    CCBT_FRE_BUFFER(parent, false);
    if (rightBrother) {
        *rightBrother = rBrother;
    }
    
    if (ptr_parentPage) {
        *ptr_parentPage = parentPage;
    }
    freePage(ctx, parentPage, false);
    return lBrother;
}

static inline void updateIndexFromPage(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBTPageCnt_T subPageIdx, CCBTNodeCnt_T nodeIdx, CCBTIndex_T *index, CCBTIndexSize_T indexSize, CCBTIndexValue_T indexValue, bool updateIndexValue)
{
    CCBTPageSize_T pageSize = ctx->pageSize;
    _CCSubPage_S *sub = CCBT_PAGE_SUB_AT_IDX(page, subPageIdx);
    const CCBTPageOff_T subOffset = sub->offset;
    _CCPageNode_S *node = CCBT_PAGE_NODE_AT_IDX(page, nodeIdx);
    CCBTIndexSize_T indexSizeOld = node->indexSize;
    CCBTPageSize_T indexOffset = node->indexOffset;
    
    if (indexSizeOld == indexSize) {
        CCBT_DEC_BUFFER(subOffset, buffer, bufType);

        buffer->seekTo(indexOffset);
        buffer->writeBuffer(index, indexSize);
        if (updateIndexValue) {
            buffer->writeLittleEndian64(indexValue);
        }
        CCBT_FRE_BUFFER(subOffset, false);
    }
    else if (indexSizeOld > indexSize) {
        CCBT_DEC_BUFFER(subOffset, buffer, bufType);
        CCBTPageSize_T diff = indexSizeOld - indexSize;
        CCBTPageSize_T off = indexOffset - CCBT_PAGE_NODE_IDX_SIZE_BYTES;
        CCBTPageSize_T endOff = indexOffset + indexSizeOld + CCBT_PAGE_NODE_IDX_VALUE_BYTES;
        uint8_t *endPtr = buffer->bytes() + endOff;
        CCBTIndexValue_T indexValueTmp = indexValue;
        if (!updateIndexValue) {
            buffer->seekTo(off + CCBT_PAGE_NODE_IDX_SIZE_BYTES + indexSizeOld);
            indexValueTmp = buffer->readLittleEndian64();
            uint8_t *ptr = buffer->bytes() + off;
            memset(ptr, 0, CCBT_PAGE_NODE_IDX_SIZE_BYTES + indexSizeOld + CCBT_PAGE_NODE_IDX_VALUE_BYTES);
        }
        buffer->seekTo(off);
        buffer->writeByte(indexSize);
        buffer->writeBuffer(index, indexSize);
        buffer->writeLittleEndian64(indexValueTmp);
        CCBTPageSize_T mvCnt = pageSize - sub->remSize - endOff;
        if (mvCnt > 0) {
            memmove(endPtr - diff, endPtr, mvCnt);
            memset(endPtr + mvCnt - diff, 0, diff);
        }
        
        CCBT_FRE_BUFFER(subOffset, false);
    }
    else {
        CCBTPageSize_T diff = indexSize - indexSizeOld;
        CCBTPageSize_T needSize = CCBT_PAGE_NODE_IDX_SIZE_BYTES + indexSize + CCBT_PAGE_NODE_IDX_VALUE_BYTES;
        ensureSubPageRemSize(ctx, page, subPageIdx, needSize, nodeIdx, diff);
        bool remSizeEnough = true;
        CCBTPageOff_T subOffTmp = subOffset;
        if (sub->remSize < needSize) {
            remSizeEnough = false;
            subOffTmp = CCBT_PAGE_SUB_AT_IDX(page, nodeIdx + 1)->offset;
        }
        
        const CCBTPageOff_T subOffsetNew = subOffTmp;
        CCBT_DEC_BUFFER(subOffsetNew, buffer, bufType);
        
        CCBTIndexValue_T indexValueTmp = indexValue;
        
        if (remSizeEnough) {
            if (!updateIndexValue) {
                buffer->seekTo(indexOffset + indexSizeOld);
                indexValueTmp = buffer->readLittleEndian64();
            }
            CCBTPageSize_T endOff = pageSize - sub->remSize;
            CCBTPageSize_T nodeEndOff = indexOffset + indexSizeOld + CCBT_PAGE_NODE_IDX_VALUE_BYTES;
            uint8_t *ptr = nullptr;
            if (endOff > nodeEndOff) {
                ptr = buffer->bytes() + nodeEndOff;
                memmove(ptr + diff, ptr, endOff - nodeEndOff);
            }
            ptr = buffer->bytes() + indexOffset - CCBT_PAGE_NODE_IDX_SIZE_BYTES;
            memset(ptr, 0, CCBT_PAGE_NODE_IDX_SIZE_BYTES + indexSize + CCBT_PAGE_NODE_IDX_VALUE_BYTES);
            buffer->seekTo(indexOffset - CCBT_PAGE_NODE_IDX_SIZE_BYTES);
            buffer->writeByte(indexSize);
            buffer->writeBuffer(index, indexSize);
            buffer->writeLittleEndian64(indexValueTmp);
        }
        else {
            buffer->seekTo(_CCPageHeaderOffIndex);
            buffer->writeByte(indexSize);
            buffer->writeBuffer(index, indexSize);
            buffer->writeLittleEndian64(indexValueTmp);
        }
        CCBT_FRE_BUFFER(subOffsetNew, false);
    }
}

static int updateIndex(_CCBPTreeIndexContext_S *ctx, CCBTPageOff_T pageOffset, CCBTIndex_T *index, CCBTIndexSize_T indexSize, CCBTIndexValue_T indexValue)
{
    _CCPageFind_S find = lookupPage(ctx, nullptr, pageOffset, index, indexSize, true);
    if (find.fcd != _CCFindCodeExists) {
        //表示不存在
        return _CCFindCodeOK;
    }
    if (find.page == nullptr) {
        return _CCFindCodeOK;
    }
    _CCPage_S *page = find.page;
    
    updateIndexFromPage(ctx, page, find.subPageIdx, find.nodeIdx, index, indexSize, indexValue, true);
    return _CCFindCodeOK;
}

static inline void deleteIndexFromPage(_CCBPTreeIndexContext_S *ctx, _CCPage_S *page, CCBTPageCnt_T subPageIdx, CCBTNodeCnt_T nodeIdx)
{
    CCBTPageSize_T pageSize = ctx->pageSize;
    _CCSubPage_S *sub = CCBT_PAGE_SUB_AT_IDX(page, subPageIdx);
    const CCBTPageOff_T subOffset = sub->offset;
    CCBT_DEC_BUFFER(subOffset, buffer, bufType);
    
    if (nodeIdx == 0) {
        page->firstChild = CCBT_PAGE_NODE_AT_IDX(page, nodeIdx)->value;
        const CCBTPageOff_T offsetTmp = page->pageOffset;
        CCBT_DEC_BUFFER(offsetTmp, pageBuffer, pageBufType);
        pageBuffer->seekTo(_CCPageHeaderOffFChild);
        pageBuffer->writeLittleEndian64(page->firstChild);
        CCBT_FRE_BUFFER(offsetTmp, false);
    }
    
    _CCPageNode_S *node = CCBT_PAGE_NODE_AT_IDX(page, nodeIdx);
    CCBTPageSize_T size = CCBT_PAGE_NODE_SIZE(node);
    CCBTPageSize_T offset = CCBT_PAGE_NODE_OFFSET(node);
    CCBTPageSize_T endOffset = pageSize - sub->remSize;
    uint8_t *ptr = buffer->bytes() + offset;
    memset(ptr, 0, size);
    CCBTPageSize_T mvSize = endOffset - size - offset;
    if (mvSize > 0) {
        memmove(ptr, ptr + size, mvSize);
        memset(ptr + mvSize, 0, size);
    }

    if (CCBT_IS_ROOT_PAGE(page)) {
        CCArrayRemoveValueAtIndex(page->pageNodeArray, nodeIdx);
    }

    CCBT_FRE_BUFFER(subOffset, bufType);
}

int deleteIndex(_CCBPTreeIndexContext_S *ctx, CCBTPageOff_T pageOffset, CCBTIndex_T *index, CCBTIndexSize_T indexLen, bool leaf)
{
    _CCPageFind_S find = lookupPage(ctx, nullptr, pageOffset, index, indexLen, leaf);
    if (find.fcd != _CCFindCodeExists) {
        //表示不存在
        return _CCFindCodeOK;
    }
    if (find.page == nullptr) {
        return _CCFindCodeOK;
    }
    _CCPage_S *page = find.page;
    CCBTNodeCnt_T minCnt = (ctx->pageNodeCnt + 1)/2;
    if (CCBT_NODE_CNT(page) > minCnt) {
        deleteIndexFromPage(ctx, page, find.subPageIdx, find.nodeIdx);
        freePage(ctx, page, false);
    }
    else {
        //1. 获取是否叶子节点
        bool isLeafPage = CCBT_PAGE_IS_LEAF(page);
        //2. 获取父节点
        CCBTNodeCnt_T slotIdx = 0;
        CCBTPageOff_T parent = 0;
        bool findBrother = false;
        if (page->pNode) {
            parent = page->pNode->offset;
            slotIdx = page->pNode->parentSlotIdx;
            findBrother = true;
        }
        //3. 查找左右兄弟节点
        CCBTPageOff_T rBrother = 0;
        CCBTPageOff_T lBrother = 0;
        _CCPage_S *parentPage = nullptr;
        if (findBrother) {
            lBrother = ::findBrother(ctx, parent, slotIdx, &rBrother, &parentPage);
        }
        //4. 删除当前节点
        deleteIndexFromPage(ctx, page, find.subPageIdx, find.nodeIdx);
        
        CCBTNodeCnt_T parentNodeIdx = slotIdx > 0 ? slotIdx - 1 : 0;
        CCBTPageCnt_T parentSubPageIdx = getPageIdxFromNodeIdx(ctx, parentPage, parentNodeIdx);
        //5. 判断兄弟节点的情况
        if (lBrother > 0) {
            _CCPage_S *lPage = readPage(ctx, lBrother, true);
            _CCSubPage_S *lastSub = (_CCSubPage_S*)CCArrayGetLastValue(lPage->subPageArray);
            const CCBTPageOff_T lastOffset = lastSub->offset;
            CCBT_DEC_BUFFER(lastOffset, lastBuffer, lastBufferType);
            //5.1如果兄弟节点有富裕，从兄弟节点借一个节点过来，否则和兄弟节点合并
            CCBTNodeCnt_T lNodeCnt = CCBT_NODE_CNT(lPage);
            if (lNodeCnt > minCnt) {
                CCBTNodeCnt_T eIdx = lNodeCnt - 1;
                /*
                 *5.1.1如果是叶子节点，
                 * (1)、将兄弟最后一个节点插入到本page第一位；
                 * (2)、将父节点的索引值更新为借过来兄弟节点的索引;
                 * (3)、将从兄弟节点借过来的节点清空
                 *5.1.2如果是索引节点
                 * (1)、将父节点的索引值更新到本page的第一位；
                 * (2)、将兄弟节点的最后一个节点索引替换父节点的索引
                 * (3)、将兄弟节点的最后一个节点清空
                 */
                if (isLeafPage) {
                    //(1)、将兄弟最后一个节点插入到本page第一位
                    _CCPageNode_S *lNode = CCBT_PAGE_NODE_AT_IDX(lPage, eIdx);
                    CCBTIndexSize_T indexSize = lNode->indexSize;
                    CCBTPageSize_T indexOffset = lNode->indexOffset;
                    CCBTIndexValue_T indexValue = lNode->value;
                    uint8_t *ptr = lastBuffer->bytes() + indexOffset;
                    insertIndexIntoNotFullPage(ctx, page, 0, 0, 0, ptr, indexSize, indexValue);
                    //(2)、更新父节点的index的值
                    updateIndexFromPage(ctx, parentPage, parentSubPageIdx, parentNodeIdx, ptr, indexSize, 0, false);
                    //(3)、将兄弟节点借过来的index清空
                    memset(ptr - CCBT_PAGE_NODE_IDX_SIZE_BYTES, 0, indexSize + CCBT_PAGE_NODE_IDX_SIZE_BYTES + CCBT_PAGE_NODE_IDX_VALUE_BYTES);
                }
                else {
                    //(1)将父节点的索引值更新到本page的第一位；
//                    CCBTPageOff_T parentSubOff = parentPage->subPageList[parentSubPageIdx].offset;
                    const CCBTPageOff_T parentSubOff = CCBT_PAGE_SUB_AT_IDX(parentPage, parentSubPageIdx)->offset;
                    CCBT_DEC_BUFFER(parentSubOff, parentSubBuffer, parentSubBufferType);
                    
                    _CCPageNode_S *pNode = CCBT_PAGE_NODE_AT_IDX(parentPage, parentNodeIdx);
                    CCBTIndexSize_T indexSize = pNode->indexSize;
                    CCBTPageSize_T indexOffset = pNode->indexOffset;
                    
                    CCBTIndexValue_T indexValue = page->firstChild;
                    
//                    CCBTIndexValue_T prevIndexValue = lPage->pageNodeList[eIdx].value;
                    CCBTIndexValue_T prevIndexValue = CCBT_PAGE_NODE_AT_IDX(lPage, eIdx)->value;
                    
                    uint8_t *ptr = parentSubBuffer->bytes() + indexOffset;
                    insertIndexIntoNotFullPage(ctx, page, 0, 0, prevIndexValue, ptr, indexSize, indexValue);
                    
                    //(2)将兄弟节点的最后一个节点索引替换父节点的索引
                    _CCPageNode_S *lNode = CCBT_PAGE_NODE_AT_IDX(lPage, eIdx);
                    indexSize = lNode->indexSize;
                    indexOffset = lNode->indexOffset;
                    ptr = lastBuffer->bytes() + indexOffset;
                    updateIndexFromPage(ctx, parentPage, parentSubPageIdx, parentNodeIdx, ptr, indexSize, 0, false);
                    //(3)将兄弟节点的最后一个节点清空
                    memset(ptr - CCBT_PAGE_NODE_IDX_SIZE_BYTES, 0, indexSize + CCBT_PAGE_NODE_IDX_SIZE_BYTES + CCBT_PAGE_NODE_IDX_VALUE_BYTES);
                    
                    CCBT_FRE_BUFFER(parentSubOff, false);
                }
                
                //如果最后一个page没有节点后,将最后的一个page回收
                CCBTPageCnt_T lPageCnt = CCBT_SUBPAGE_CNT(lPage);
                if (lastSub->eIdx == lastSub->sIdx && lPageCnt > 1) {
                    _CCSubPage_S *last = CCBT_PAGE_SUB_AT_IDX(lPage, lPageCnt - 2);
                    const CCBTPageOff_T lastOffset = last->offset;
                    CCBT_DEC_BUFFER(lastOffset, lastTmp, lastType);
                    lastTmp->seekTo(_CCPageHeaderOffNext);
                    lastTmp->writeLittleEndian64(0);
                    CCBT_FRE_BUFFER(lastOffset, false);
                    addFreePageOffset(ctx, last->offset);
                }
                --lastSub->eIdx;
            }
            else {
                //5.2 进行合并
                /*
                 *5.2.1如果是叶子节点，
                 * (1)、将当前节点和兄弟节点合并为一个page
                 * (2)、将父节点删除;
                 *5.2.2如果是索引节点
                 * (1)、将父节点下移
                 * (2)、将当前节点、父节点和兄弟节点合并为一个page
                 * (3)、将父节点清空
                 */
                
                if (isLeafPage) {
                    //(1)、将当前节点和兄弟节点合并为一个page
                    lastBuffer->seekTo(_CCPageHeaderOffNext);
                    lastBuffer->writeLittleEndian64(page->pageOffset);
                    
                    const CCBTPageOff_T lOffset = lPage->pageOffset;
                    CCBT_DEC_BUFFER(lPage->pageOffset, firstBuffer, firstBufferType);
                    firstBuffer->seekTo(_CCPageHeaderOffBrother);
                    firstBuffer->writeLittleEndian64(page->brother);
                    CCBT_FRE_BUFFER(lOffset, false);
                    //(2)、将父节点删除
                    _CCPageNode_S *pNode = CCBT_PAGE_NODE_AT_IDX(parentPage, parentNodeIdx);
                    CCBTIndexSize_T parentIndexSize = pNode->indexSize;
                    CCBTPageSize_T parentIndexOffset = pNode->indexOffset;

                    const CCBTPageOff_T parentSubOff = CCBT_PAGE_SUB_AT_IDX(parentPage, parentSubPageIdx)->offset;
                    CCBT_DEC_BUFFER(parentSubOff, parentSubBuffer, parentSubBufferType);
                    CCBTIndex_T *parentIndex = parentSubBuffer->bytes() + parentIndexOffset;
                    deleteIndex(ctx, parent, parentIndex, parentIndexSize, NO);
                    CCBT_FRE_BUFFER(parentSubOff, false);
                    //TODO：修改内存数据
                }
                else {
                    const CCBTPageOff_T parentSubOff = CCBT_PAGE_SUB_AT_IDX(parentPage, parentSubPageIdx)->offset;
                    CCBT_DEC_BUFFER(parentSubOff, parentSubBuffer, parentBufferType);
                    
                    //(1)、将父节点下移；
                    _CCPageNode_S *pNode = CCBT_PAGE_NODE_AT_IDX(parentPage, parentNodeIdx);
                    CCBTIndexSize_T indexSize = pNode->indexSize;
                    CCBTPageSize_T indexOffset = pNode->indexOffset;
                    CCBTIndexValue_T indexValue = page->firstChild;
                    uint8_t *ptr = parentSubBuffer->bytes() + indexOffset;
                    insertIndexIntoNotFullPage(ctx, lPage, CCBT_SUBPAGE_CNT(lPage) - 1, CCBT_NODE_CNT(lPage), 0, ptr, indexSize, indexValue);
                    
                    CCBT_FRE_BUFFER(parentSubOff, false);
                    
                    //(2)、将当前节点、父节点和兄弟节点合并为一个page
                    lastBuffer->seekTo(_CCPageHeaderOffNext);
                    lastBuffer->writeLittleEndian64(page->pageOffset);
                    
                    //(3)、将父节点清空
                    deleteIndex(ctx, parent, ptr, indexSize, NO);


                    //TODO：修改内存数据
                }
                //如果父节点没有节点后，将父节点回收，当前节点为叶子节点和root节点
                if (CCBT_NODE_CNT(parentPage) == 0) {
                    addFreePageOffset(ctx, parentPage->pageOffset);
                    lPage->pageType = (_CCPageType_E)(_CCPageTypeLeaf | _CCPageTypeRoot);

                    const CCBTPageOff_T lOffset = lPage->pageOffset;
                    CCBT_DEC_BUFFER(lOffset, tmpBuf, tmpBufType);
                    tmpBuf->seekTo(_CCPageHeaderOffPType);
                    tmpBuf->writeByte(lPage->pageType);
                    CCBT_FRE_BUFFER(lOffset, false);
                }
            }
            CCBT_FRE_BUFFER(lastOffset, false);
            freePage(ctx, lPage, false);
        }
        else {
            //6.判断右兄弟节点的情况
            _CCPage_S *rPage = readPage(ctx, rBrother, true);
            _CCSubPage_S *firstSub = (_CCSubPage_S*)CCArrayGetFirstValue(rPage->subPageArray);
            const CCBTPageOff_T firstOffset = firstSub->offset;
            CCBT_DEC_BUFFER(firstOffset, firstBuffer, firstBufferType);
            //6.1如果兄弟节点有富裕，从兄弟节点借一个节点过来，否则和兄弟节点合并
            if (CCBT_NODE_CNT(rPage) > minCnt) {
                CCBTNodeCnt_T sIdx = 0;
                CCBTPageCnt_T pIdx = CCBT_SUBPAGE_CNT(page) - 1;//page->pageCnt - 1;
                CCBTNodeCnt_T nIdx = CCBT_NODE_CNT(page) - 1;//page->nodeCnt - 1;
                /*
                 *6.1.1如果是叶子节点，
                 * (1)、将兄弟第一个节点插入到本page最后一位；
                 * (2)、将父节点的索引值更新为借过来兄弟节点的索引;
                 * (3)、将从兄弟节点借过来的节点清空
                 *6.1.2如果是索引节点
                 * (1)、将父节点的索引值更新到本page的最后一位；
                 * (2)、将兄弟节点的第一个节点索引替换父节点的索引
                 * (3)、将兄弟节点的第一个节点清空
                 */
                if (isLeafPage) {
                    //(1)、将兄弟第一个节点插入到本page最后一位；
                    _CCPageNode_S *rNode = CCBT_PAGE_NODE_AT_IDX(rPage, sIdx);
                    CCBTIndexSize_T indexSize = rNode->indexSize;
                    CCBTPageSize_T indexOffset = rNode->indexOffset;
                    CCBTIndexValue_T indexValue = rNode->value;
                    uint8_t *ptr = firstBuffer->bytes() + indexOffset;
                    insertIndexIntoNotFullPage(ctx, page, pIdx, nIdx, 0, ptr, indexSize, indexValue);
                    //(2)、将父节点的索引值更新为借过来兄弟节点的索引;
                    updateIndexFromPage(ctx, parentPage, parentSubPageIdx, parentNodeIdx, ptr, indexSize, 0, false);
                    //(3)、将从兄弟节点借过来的节点清空
                    deleteIndexFromPage(ctx, rPage, 0, 0);
                }
                else {
                    //(1)、将父节点的索引值更新到本page的最后一位；
//                    CCBTPageOff_T parentSubOff = parentPage->subPageList[parentSubPageIdx].offset;
                    const CCBTPageOff_T parentSubOff = CCBT_PAGE_SUB_AT_IDX(parentPage, parentSubPageIdx)->offset;
                    CCBT_DEC_BUFFER(parentSubOff, parentSubBuffer, parentSubBufferType);
                    
                    _CCPageNode_S *pNode = CCBT_PAGE_NODE_AT_IDX(parentPage, parentNodeIdx);
                    CCBTIndexSize_T indexSize = pNode->indexSize;
                    CCBTPageSize_T indexOffset = pNode->indexOffset;
                    CCBTIndexValue_T indexValue = page->firstChild;
                    
                    uint8_t *ptr = parentSubBuffer->bytes() + indexOffset;
                    insertIndexIntoNotFullPage(ctx, page, pIdx, nIdx, 0, ptr, indexSize, indexValue);
                    //(2)、将兄弟节点的第一个节点索引替换父节点的索引
                    _CCPageNode_S *rNode = CCBT_PAGE_NODE_AT_IDX(rPage, 0);
                    indexSize = rNode->indexSize;
                    indexOffset = rNode->indexOffset;
                    ptr = firstBuffer->bytes() + indexOffset;
                    updateIndexFromPage(ctx, parentPage, parentSubPageIdx, parentNodeIdx, ptr, indexSize, 0, false);
                    CCBT_FRE_BUFFER(parentSubOff, false);
                    
                    //(3)、将兄弟节点的第一个节点清空
                    deleteIndexFromPage(ctx, rPage, 0, 0);
                }
                
                //如果第一个page没有节点后,将第一个page回收，也可以不回收，不进行回收，因为有可能前面page的brother指向此firstPage
                ++firstSub->sIdx;
//                if (firstSub->sIdx > firstSub->eIdx) {
//                    _CCSubPage_S *first = &rPage->subPageList[1];
//                    CCBT_DEC_BUFFER(first->offset, firstSubBuffer, fistType);
//                    CCBT_FRE_BUFFER(firstSubBuffer, fistType);
//                }
            }
            else {
                //6.2 进行合并
                /*
                 *6.2.1如果是叶子节点，
                 * (1)、将当前节点和兄弟节点合并为一个page
                 * (2)、将父节点删除
                 *6.2.2如果是索引节点
                 * (1)、将父节点下移
                 * (2)、将当前节点、父节点和兄弟节点合并为一个page
                 * (3)、将父节点删除
                 */
                const CCBTPageOff_T lastSubOff = ((_CCSubPage_S*)CCArrayGetLastValue(page->subPageArray))->offset;
                if (isLeafPage) {
                    // (1)、将当前节点和兄弟节点合并为一个page
//                    CCBTPageOff_T off = page->subPageList[0].offset;
                    const CCBTPageOff_T off =  CCBT_SUBPAGE_FIRST(page)->offset;
                    CCBT_DEC_BUFFER(off, bufferTmp, bufferTmpType);
                    bufferTmp->seekTo(_CCPageHeaderOffBrother);
                    bufferTmp->writeLittleEndian64(rPage->brother);

                    if (off == lastSubOff) {
                        bufferTmp->seekTo(_CCPageHeaderOffNext);
                        bufferTmp->writeLittleEndian64(rPage->pageOffset);
                        CCBT_FRE_BUFFER(off, false);
                    }
                    else {
                        CCBT_FRE_BUFFER(off, false);
                        CCBT_DEC_BUFFER(lastSubOff, lastSubBuffer, lastSubBufferType);
                        lastSubBuffer->seekTo(_CCPageHeaderOffNext);
                        lastSubBuffer->writeLittleEndian64(rPage->pageOffset);
                        CCBT_FRE_BUFFER(lastSubOff, false);
                    }

                    //(2)、将父节点删除
                    const CCBTPageOff_T parentSubOff = CCBT_PAGE_SUB_AT_IDX(parentPage, parentSubPageIdx)->offset;
                    _CCPageNode_S *pNode = CCBT_PAGE_NODE_AT_IDX(parentPage, parentNodeIdx);
                    CCBTIndexSize_T parentIndexSize = pNode->indexSize;
                    CCBTPageSize_T parentIndexOffset = pNode->indexOffset;
                    
                    CCBT_DEC_BUFFER(parentSubOff, parentSubBuffer, parentSubBufferType);
                    
                    CCBTIndex_T *parentIndex = parentSubBuffer->bytes() + parentIndexOffset;
                    deleteIndex(ctx, parent, parentIndex, parentIndexSize, NO);
                    
                    CCBT_FRE_BUFFER(parentSubOff, false);
                }
                else {
                    const CCBTPageOff_T parentSubOff = CCBT_PAGE_SUB_AT_IDX(parentPage, parentSubPageIdx)->offset;
                    CCBT_DEC_BUFFER(parentSubOff, parentBuffer, parentBufferType);
                    // (1)、将父节点下移
                    _CCPageNode_S *pNode = CCBT_PAGE_NODE_AT_IDX(parentPage, parentNodeIdx);
                    CCBTIndexSize_T indexSize = pNode->indexSize;
                    CCBTPageSize_T indexOffset = pNode->indexOffset;
                    CCBTIndexValue_T indexValue = page->firstChild;
                    uint8_t *ptr = parentBuffer->bytes() + indexOffset;
                    
                    insertIndexIntoNotFullPage(ctx, page, CCBT_SUBPAGE_CNT(page) - 1, CCBT_NODE_CNT(page), 0, ptr, indexSize, indexValue);
                    CCBT_FRE_BUFFER(parentSubOff, false);

                    //(2)、将当前节点、父节点和兄弟节点合并为一个page
                    CCBT_DEC_BUFFER(lastSubOff, lastSubBuffer, lastSubBufferType);
                    lastSubBuffer->seekTo(_CCPageHeaderOffNext);
                    lastSubBuffer->writeLittleEndian64(rPage->pageOffset);
                    CCBT_FRE_BUFFER(lastSubOff, false);
                    
                    //(3)、将父节点删除
                    deleteIndex(ctx, parent, ptr, indexSize, NO);
                }
                
                if (CCBT_NODE_CNT(parentPage) == 0) {
                    addFreePageOffset(ctx, parentPage->pageOffset);
                    page->pageType = (_CCPageType_E)(_CCPageTypeLeaf | _CCPageTypeRoot);

                    const CCBTPageOff_T tmpOffset = page->pageOffset;
                    CCBT_DEC_BUFFER(tmpOffset, tmpBuf, tmpBufType);
                    tmpBuf->seekTo(_CCPageHeaderOffPType);
                    tmpBuf->writeByte(page->pageType);
                    CCBT_FRE_BUFFER(tmpOffset, false);
                }
            }
            CCBT_FRE_BUFFER(firstOffset, false);
            freePage(ctx, rPage, false);
        }
        freePage(ctx, parentPage, false);
    }
    return 0;
}

CCBPTreeIndex::CCBPTreeIndex(const string &indexFile)
{
    _ptrRBTreeIndexContext = _CCBPTreeIndexContextCreate(nullptr, indexFile, 1024, 0);
}

CCBPTreeIndex::CCBPTreeIndex(const string &indexFile, CCBTIndexSize_T indexLen)
{
    CCBTNodeCnt_T nodeCnt = 1024;
    CCBTPageSize_T pageSize = nodeCnt * indexLen;
    pageSize = MAX(pageSize, defaultPageSize_g);
    _ptrRBTreeIndexContext = _CCBPTreeIndexContextCreate(nullptr, indexFile, nodeCnt, pageSize);
}

CCBPTreeIndex::CCBPTreeIndex(const string &indexFile, CCUInt16_t pageNodeCount, CCBTIndexSize_T indexLen)
{
    CCBTPageSize_T pageSize = pageNodeCount * indexLen;
    pageSize = MAX(pageSize, defaultPageSize_g);
    _ptrRBTreeIndexContext = _CCBPTreeIndexContextCreate(nullptr, indexFile, pageNodeCount, pageSize);
}

CCBPTreeIndex::~CCBPTreeIndex()
{
    
    _PCCBPTreeIndexContext_S ctx = (_PCCBPTreeIndexContext_S)_ptrRBTreeIndexContext;
    if (ctx) {
        destroyBufferCache(ctx);
        
        if (ctx->headBuffer) {
            ctx->fileMap->destroyMapBuffer(ctx->headBuffer);
            ctx->headBuffer = NULL;
        }

        if (ctx->rootBuffer) {
            ctx->fileMap->destroyMapBuffer(ctx->rootBuffer);
            ctx->rootBuffer = NULL;
        }
        
        if (ctx->fileMap) {
            ctx->fileMap->close();
            delete ctx->fileMap;
            ctx->fileMap = NULL;
        }
        if (ctx->root) {
            freePage(ctx, ctx->root, true);
            ctx->root = NULL;
        }
        
        free(ctx);
        _ptrRBTreeIndexContext = nullptr;
    }
}

int CCBPTreeIndex::selectIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T *indexValue)
{
    _PCCBPTreeIndexContext_S ctx = (_PCCBPTreeIndexContext_S)_ptrRBTreeIndexContext;
    _CCPageFind_S find = lookupPage(ctx, nullptr, ctx->rootOff, index, indexLen, true);
    if (indexValue) {
        *indexValue = find.value;
    }
    return find.fcd == _CCFindCodeExists ? 0 : 1;
}

int CCBPTreeIndex::insertIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue)
{
    _PCCBPTreeIndexContext_S ctx = (_PCCBPTreeIndexContext_S)_ptrRBTreeIndexContext;
    return ::insertIndex(ctx, ctx->rootOff, 0, index, indexLen, indexValue);
}

int CCBPTreeIndex::deleteIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen)
{
    _PCCBPTreeIndexContext_S ctx = (_PCCBPTreeIndexContext_S)_ptrRBTreeIndexContext;
    ::deleteIndex(ctx, ctx->rootOff, index, indexLen, true);
    return 0;
}

int CCBPTreeIndex::updateIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue)
{
    _PCCBPTreeIndexContext_S ctx = (_PCCBPTreeIndexContext_S)_ptrRBTreeIndexContext;
    ::updateIndex(ctx, ctx->rootOff, index, indexLen, indexValue);
    return 0;
}



