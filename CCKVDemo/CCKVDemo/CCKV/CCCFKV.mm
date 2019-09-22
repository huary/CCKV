//
//  CCCFKV.m
//  CCKVDemo
//
//  Created by yuan on 2019/9/8.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "CCCFKV.h"
#import <sys/mman.h>
#import <zlib.h>
#import <CommonCrypto/CommonDigest.h>
#import <sys/stat.h>

#import <vector>

#import "macro.h"
#import "CCAESCryptor.h"
#import "NSObject+CCCodeToTopSuperClass.h"
#import "CCCFKVDelegate.h"

static int DEFAULT_PAGE_SIZE_s = getpagesize();
static int MIN_MMAP_SIZE_s;     //64KB

#define DEFAULT_CCCFKV_NAME              "CC.cfkv.default"
#define CCCFKV_CODE_DATA_HEAD_SIZE       (128)
#define CCCFKV_CONTENT_HEADER_SIZE       (128)
#define CCCFKV_CRC_SIZE                  (134217728) //128MB

//在keyItem为y1000个以上的时候可以进行回写
#define CCCFKV_FULL_WRITE_BACK_KEY_MIN_CNT                         (1000)
//在keyItem占据CodeItem的比例小于0.5的时候可以进行回写
#define CCCFKV_FULL_WRITE_BACK_KEY_CNT_WITH_CODE_CNT_MAX_RATIO     (0.6)

#define CCCFKV_CHECK_CODE_QUEUE         //NSAssert([self _isInCodeQueue], @"must execute in codeQueue")

#define CCCFKV_IS_FILE_OPEN(CTX)         (CTX->_fd > 0 && CTX->_size > 0 && CTX->_ptr && CTX->_ptr != MAP_FAILED)

//NSString *const _CCCFKVErrorDomain = @"CCCFKVErrorDomain";


typedef NS_ENUM(uint32_t, _CCHeaderOffset)
{
    _CCHeaderOffsetVersion             = 0,
    _CCHeaderOffsetSize                = 2,
    _CCHeaderOffsetKeyItemCnt          = 10,
    _CCHeaderOffsetCodeItemCnt         = 14,
    _CCHeaderOffsetCodeContentCRC      = 18,
    _CCHeaderOffsetCryptor             = 22,
    _CCHeaderOffsetKeyHash             = 23,
};

typedef NS_ENUM(uint8_t, _CCHashType)
{
    _CCHashTypeMD2         = 1,
    _CCHashTypeMD4         = 2,
    _CCHashTypeMD5         = 3,
    _CCHashTypeSHA1        = 4,
    _CCHashTypeSHA224      = 5,
    _CCHashTypeSHA256      = 6,
    _CCHashTypeSHA384      = 7,
    _CCHashTypeSHA512      = 8,
    _CCHashTypeSHA3        = 9,
};

//内部错误,从2^16次方开始
typedef NS_ENUM(int32_t, _CCCFKVInterError)
{
    //密码长度错误
    _CCCFKVInterErrorKeySizeError        = 65536,
    //加密模式错误
    _CCCFKVInterErrorCryptModeError      = 65537,
};

typedef NS_ENUM(int32_t, _CCCFKVCacheObjectType)
{
    //空
    _CCCFKVCacheObjectTypeNone             = 0,
    //这个是明文的区域
    _CCCFKVCacheObjectTypeDataRange        = 1,
    //这个就是未编码的对象
    _CCCFKVCacheObjectTypeUncodedObject    = 2,
    //这个是编码后的
    _CCCFKVCacheObjectTypeEncodedData      = 3,
    //这个是C语言所含有的基本数据类型,这个可以和上面的进行|运算
    _CCCFKVCacheObjectTypeCTypeValue       = (1 << 16),
};

static inline void _sync_lock(dispatch_semaphore_t lock, void (^block)(void)) {
    dispatch_semaphore_wait(lock, DISPATCH_TIME_FOREVER);
    if (block) {
        block();
    }
    dispatch_semaphore_signal(lock);
}

typedef id(^CCCFKVCodeBlock)(CCCFKV *kv);
typedef void(^CCCFKVCodeCompletionBlock)(CCCFKV *kv, id result);



/**********************************************************************
 *_CCCFKVCacheObject
 * cacheObjType == _CCCFKVCacheObjectTypeDataRange，记录dataRange
 * cacheObjType == _CCCFKVCacheObjectTypeUncodedObject,记录cacheObject
 * cacheObjType == _CCCFKVCacheObjectTypeEncodedData, 记录objectData
 *对于密文的时候，记录的是解码后的对象
 ***********************************************************************/
@interface _CCCFKVCacheObject : NSObject

@property (nonatomic, assign, readonly) _CCCFKVCacheObjectType cacheObjectType;

@property (nonatomic, assign, readonly) NSRange dataRange;

//存储解码后的数据对象
@property (nonatomic, strong) id decodeObject;

@property (nonatomic, assign, readonly) int64_t CTypeValue;

@property (nonatomic, assign, readonly) CCCodeItemType CTypeItemType;


@end

@implementation _CCCFKVCacheObject
{
@private
    id _cacheObject;
}

- (instancetype)initWithCacheObject:(_CCCFKVCacheObject*)cacheObject
{
    self = [super init];
    if (self) {
        _cacheObjectType = cacheObject.cacheObjectType;
        _dataRange = cacheObject.dataRange;
        _decodeObject = cacheObject.decodeObject;
        _CTypeValue = cacheObject.CTypeValue;
        _CTypeItemType = cacheObject.CTypeItemType;
    }
    return self;
}

- (NSData*)objectEncodedData
{
    return (NSData*)_cacheObject;
}

- (id)cacheObject
{
    return _cacheObject;
}

- (void)setCacheObject:(id)cacheObject withType:(_CCCFKVCacheObjectType)type
{
    if (type == _CCCFKVCacheObjectTypeNone) {
        _cacheObject = nil;
        _cacheObjectType = type;
    }
    else if (type == _CCCFKVCacheObjectTypeUncodedObject) {
        if ([cacheObject isKindOfClass:[NSObject class]]) {
            _cacheObject = cacheObject;
            _cacheObjectType = type;
        }
    }
    else if (type == _CCCFKVCacheObjectTypeEncodedData) {
        if ([cacheObject isKindOfClass:[NSData class]]) {
            _cacheObject = cacheObject;
            _cacheObjectType = type;
        }
    }
}

- (void)setRange:(NSRange)range
{
    _dataRange = range;
    _cacheObjectType = _CCCFKVCacheObjectTypeDataRange;
}

- (void)addCTypeValue:(int64_t)CTypeValue CTypeItemType:(CCCodeItemType)CTypeItemType
{
    _cacheObjectType =  (_CCCFKVCacheObjectType)(_cacheObjectType | _CCCFKVCacheObjectTypeCTypeValue);
    _CTypeValue = CTypeValue;
    _CTypeItemType = CTypeItemType;
}

- (BOOL)haveCTypeValue
{
    return (_cacheObjectType & _CCCFKVCacheObjectTypeCTypeValue);
}

@end

static inline NSMutableDictionary *_loadFromFileWithCryptKey(struct CCCFKVContext *ctx, CCCodeData *cryptKey);
static inline void _closeFile(struct CCCFKVContext *ctx);

/**********************************************************************
 *
 *|---2字节(version)---|---8字节(size)---|---4字节(keyItemCnt)---|---4字节(codeItemCnt)---|---4字节(crc)---|---1字节(cryptor)---|
 *|---32字节keyHash(sha256)---|---73字节(保留)---|
 *
 *其中---1字节(cryptor)---如下：
 *|---6bit的加密模式（CCCryptMode）,2bit的keytype（CCAESKeyType+1）---|
 *
 ***********************************************************************/

typedef struct CCCFKVContext {
    int _fd;
    int64_t _size;
    uint8_t *_ptr;
    uint16_t _version;
    int64_t _codeSize;
    uint32_t _keyItemCnt;
    uint32_t _codeItemCnt;
    uint32_t _codeContentCRC;
    uint8_t _cryptorInfo;
    uint8_t _hashKey[CC_SHA256_DIGEST_LENGTH];

    shared_ptr<CCCodeData> _sharedPtrHeaderData;
    shared_ptr<CCCodeData> _sharedPtrContentData;
    //这个只是存储从文件mmap后解密后的contentData，主要是为了在decode的时候不需要生成NSData影响性能。
    shared_ptr<CCCodeData> _sharedPtrPlainContentData;
    
    shared_ptr<CCAESCryptor> _sharedPtrCryptor;
    
    string _filePath;
    NSMutableDictionary<id, _CCCFKVCacheObject*> *_dict;
    dispatch_semaphore_t _lock;
    shared_ptr<CCMutableCodeData> _sharedPtrTmpCodeData;
    
    CCCFKVError lastError;
    
    CCCFKV *CFKV;
}CCCFKVContext_S;

static inline CCCodeData* _hashForData(CCCodeData *data, _CCHashType hashType)
{
    int64_t dataSize = data->dataSize();
    if (dataSize == 0) {
        return NULL;
    }
    CCMutableCodeData *hashData = new CCMutableCodeData(CC_SHA512_DIGEST_LENGTH);
    uint8_t length = 0;
    switch (hashType) {
        case _CCHashTypeSHA256: {
            CC_SHA256(data->bytes(), (CC_LONG)dataSize, hashData->bytes());
            length = CC_SHA256_DIGEST_LENGTH;
            break;
        }
        case _CCHashTypeSHA512: {
            CC_SHA512(data->bytes(), (CC_LONG)dataSize, hashData->bytes());
            length = CC_SHA512_DIGEST_LENGTH;
            break;
        }
        default:
            break;
    }
    hashData->truncateTo(length);
    return hashData;
}

static inline CCCodeData * _hashCryptKey(CCCodeData *cryptKey, CCAESKeyType *keyType)
{
    CCAESKeyType keyTypeTmp = CCAESKeyType128;
    int64_t cryptKeySize = cryptKey->dataSize();
    if (cryptKeySize == 0) {
        return NULL;
    }
    CCCodeData *hashData = _hashForData(cryptKey, _CCHashTypeSHA512);
    if (cryptKeySize >= CCAESKeySize256) {
        keyTypeTmp = CCAESKeyType256;
    }
    else if (cryptKeySize >= CCAESKeySize192) {
        keyTypeTmp = CCAESKeyType192;
    }
    else {
        keyTypeTmp = CCAESKeyType128;
    }
    if (keyType) {
        *keyType = keyTypeTmp;
    }
    return hashData;
}

static inline BOOL _checkAndMakeDirectory(string &filepath)
{
    if (filepath.length() == 0) {
        return NO;
    }
    vector<size_t> vec;
    string sub = filepath;
    while (access(sub.c_str(), F_OK) != F_OK) {
        vec.insert(vec.begin(), sub.length());
        size_t p = sub.find_last_of('/');
        sub = sub.substr(0,p);
    }
    
    size_t cnt = vec.size();
    for (int i = 0; i < cnt; ++i) {
        size_t p = vec[i];
        string sub = filepath.substr(0, p);
        mkdir(sub.c_str(), S_IRWXU);
    }
    return YES;
//    return access(filepath.c_str(), F_OK) == F_OK;
}

static inline int64_t _getFileSizeWithFD(int fd)
{
    struct stat st = {};
    if (fstat(fd, &st) == F_OK) {
        return st.st_size;
    }
    return -1;
}

static inline BOOL _readHeaderInfo(struct CCCFKVContext *ctx)
{
    ctx->_sharedPtrHeaderData->seek(CCDataSeekTypeSET);
    ctx->_version = ctx->_sharedPtrHeaderData->readLittleEndian16();
    ctx->_codeSize = ctx->_sharedPtrHeaderData->readLittleEndian64();
    ctx->_keyItemCnt = ctx->_sharedPtrHeaderData->readLittleEndian32();
    ctx->_codeItemCnt = ctx->_sharedPtrHeaderData->readLittleEndian32();
    ctx->_codeContentCRC = ctx->_sharedPtrHeaderData->readLittleEndian32();
    ctx->_cryptorInfo = ctx->_sharedPtrHeaderData->readByte();
    ctx->_sharedPtrHeaderData->read(ctx->_hashKey, CC_SHA256_DIGEST_LENGTH);
    return YES;
}

static inline BOOL _readContentData(struct CCCFKVContext *ctx)
{
    ctx->_sharedPtrContentData->seekTo(ctx->_codeSize + CCCFKV_CONTENT_HEADER_SIZE);
    return YES;
}

static inline BOOL _checkDataWithCRC(struct CCCFKVContext *ctx)
{
    int64_t size = 0;
    uint32_t crc = 0;
    uint8_t *ptr = ctx->_sharedPtrContentData->bytes() + CCCFKV_CONTENT_HEADER_SIZE;
    while (size < ctx->_codeSize) {
        uint32_t cSize = CCCFKV_CRC_SIZE;
        if (ctx->_codeSize < size + cSize) {
            cSize = (uint32_t)(ctx->_codeSize - size);
        }
        crc = (uint32_t)crc32((uint32_t)crc, ptr + size, (uint32_t)cSize);
        if (size + cSize >= ctx->_codeSize) {
            break;
        }
        size += cSize;
    }
    return ctx->_codeContentCRC == crc;
}

static inline void _updateCodeSize(struct CCCFKVContext *ctx, int64_t codeSize)
{
    if (codeSize < 0 || codeSize + CCCFKV_CODE_DATA_HEAD_SIZE + CCCFKV_CONTENT_HEADER_SIZE > ctx->_size) {
        return;
    }

    ctx->_codeSize = codeSize;
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetSize);
    ctx->_sharedPtrHeaderData->writeLittleEndian64(ctx->_codeSize);
}

static inline void _updateKeyItemCnt(struct CCCFKVContext *ctx)
{
//    NSLog(@"keyItem=%ld,codeeItem=%d",ctx->_dict.count,ctx->_codeItemCnt);
    ctx->_keyItemCnt = (uint32_t)ctx->_dict.count;
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetKeyItemCnt);
    ctx->_sharedPtrHeaderData->writeLittleEndian32(ctx->_keyItemCnt);
}

static inline void _updateCodeItemCnt(struct CCCFKVContext *ctx, uint32_t codeItemCnt)
{
    if (codeItemCnt < ctx->_dict.count) {
        return;
    }
    ctx->_codeItemCnt = codeItemCnt;
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetCodeItemCnt);
    ctx->_sharedPtrHeaderData->writeLittleEndian32(ctx->_codeItemCnt);
}

static inline void _updateCRC(struct CCCFKVContext *ctx, uint32_t srcCRC, uint8_t *ptr, uint32_t size)
{
    uint32_t crc = ptr ? (uint32_t)crc32((uint32_t)srcCRC, ptr, (uint32_t)size) : srcCRC;
    ctx->_codeContentCRC = crc;
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetCodeContentCRC);
    ctx->_sharedPtrHeaderData->writeLittleEndian32(ctx->_codeContentCRC);
}

static inline void _updateCryptorInfoWithCryptKey(struct CCCFKVContext *ctx, CCCodeData *cryptKey)
{
    if (ctx->_sharedPtrCryptor.get() != nullptr) {
        CCAESKeyType keyType = ctx->_sharedPtrCryptor->getKeyType();
        CCCryptMode cryptMode = ctx->_sharedPtrCryptor->getCryptMode();
        ctx->_cryptorInfo = TYPE_OR(TYPE_LS(cryptMode, 2), TYPE_AND(keyType + 1, 3));
        ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetCryptor);
        ctx->_sharedPtrHeaderData->writeByte(ctx->_cryptorInfo);
        //这里的hashKey不可能为nil
        CCCodeData *hash = _hashForData(cryptKey, _CCHashTypeSHA256);
        if (hash) {
            memcpy(ctx->_hashKey, hash->bytes(), CC_SHA256_DIGEST_LENGTH);
            delete hash;
        }
    }
    else {
        
        ctx->_cryptorInfo = 0;
        ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetCryptor);
        ctx->_sharedPtrHeaderData->writeByte(ctx->_cryptorInfo);
        
        memset(ctx->_hashKey, 0, sizeof(ctx->_hashKey));
    }
    
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetKeyHash);
    ctx->_sharedPtrHeaderData->writeBuffer(ctx->_hashKey, CC_SHA256_DIGEST_LENGTH);
}

static inline void _updateCryptorInfoWithHashKey(struct CCCFKVContext *ctx, uint8_t cryptorInfo, uint8_t hashKey[CC_SHA256_DIGEST_LENGTH], uint8_t contentHeader[CCCFKV_CONTENT_HEADER_SIZE])
{
    ctx->_cryptorInfo = cryptorInfo;
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetCryptor);
    ctx->_sharedPtrHeaderData->writeByte(ctx->_cryptorInfo);
    
    memcpy(ctx->_hashKey, hashKey, CC_SHA256_DIGEST_LENGTH);
    ctx->_sharedPtrHeaderData->seekTo(_CCHeaderOffsetKeyHash);
    ctx->_sharedPtrHeaderData->writeBuffer(ctx->_hashKey, CC_SHA256_DIGEST_LENGTH);
    
    memcpy(ctx->_sharedPtrContentData->bytes(), contentHeader, CCCFKV_CONTENT_HEADER_SIZE);
}

static inline BOOL _checkContentCryptKeyHeader(struct CCCFKVContext *ctx)
{
    return memcmp(ctx->_sharedPtrContentData->bytes(), ctx->_hashKey, sizeof(ctx->_hashKey)) == 0;
}

static inline BOOL _shouldFullWriteBack(struct CCCFKVContext *ctx)
{
    if (ctx->_keyItemCnt == ctx->_codeItemCnt) {
        return NO;
    }
    if ((ctx->_keyItemCnt == 0 && ctx->_codeItemCnt > 0) ||
        (ctx->_keyItemCnt > CCCFKV_FULL_WRITE_BACK_KEY_MIN_CNT &&
        ctx->_keyItemCnt <= ctx->_codeItemCnt * CCCFKV_FULL_WRITE_BACK_KEY_CNT_WITH_CODE_CNT_MAX_RATIO)) {
        return YES;
    }
    return NO;
}

static inline void _reportError(struct CCCFKVContext *ctx, CCCFKVError error)
{
    CCCFKV *CFKV = ctx->CFKV;
    _closeFile(ctx);
//    if (CFKV->delegate.expired() == NO) {
//        shared_ptr<CCCFKVDelegateInterface> delegate = CFKV->delegate.lock();
//        delegate->notifyError(CFKV, error);
//    }
    ctx->lastError = error;
//    NSLog(@"error=%ld",error);
    if (CFKV->delegate) {
        CFKV->delegate->notifyError(CFKV, error);
    }
}

static inline BOOL _reportWarning(struct CCCFKVContext *ctx, CCCFKVError error)
{
    CCCFKV *CFKV = ctx->CFKV;
//    if (!CFKV->delegate.expired()) {
//        shared_ptr<CCCFKVDelegateInterface> delegate = CFKV->delegate.lock();
//        return delegate->notifyWarning(CFKV, error);
//    }
    ctx->lastError = error;
    if (CFKV->delegate) {
        return CFKV->delegate->notifyWarning(CFKV, error);
    }
    return YES;
}

static inline NSMutableDictionary<id, _CCCFKVCacheObject*>* _encodeDictionary(struct CCCFKVContext *ctx, NSDictionary<id, _CCCFKVCacheObject*>*dict, CCMutableCodeData *codeData)
{
    NSMutableDictionary *newDict = [NSMutableDictionary dictionary];
    
    [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, _CCCFKVCacheObject * _Nonnull obj, BOOL * _Nonnull stop) {
        
        
        encodeObjectToTopSuperClassIntoCodeData(key, NULL, codeData, NULL);
        int64_t startLoc = codeData->currentSeek();

        _CCCFKVCacheObjectType cacheObjectType = (_CCCFKVCacheObjectType)(obj.cacheObjectType & 0XFFFF);
        if (cacheObjectType == _CCCFKVCacheObjectTypeDataRange) {
            codeData->appendWriteBuffer(ctx->_sharedPtrPlainContentData->bytes() + obj.dataRange.location, obj.dataRange.length);
        }
        else if (cacheObjectType == _CCCFKVCacheObjectTypeEncodedData) {
            codeData->appendWriteData([obj objectEncodedData]);
        }
        else if (cacheObjectType == _CCCFKVCacheObjectTypeUncodedObject) {
            encodeObjectIntoCodeData([obj cacheObject], codeData, NULL);
        }
        else if ([obj haveCTypeValue]) {
            CCCodeItemType itemType = obj.CTypeItemType;
            int64_t CTypeVal = obj.CTypeValue;
            switch (itemType) {
                case CCCodeItemTypeInteger: {
                    encodeIntegerIntoCodeData(CTypeVal, codeData);
                    break;
                }
                case CCCodeItemTypeRealF: {
                    encodeFloatIntoCodeData(FloatFromInt64(CTypeVal), codeData);
                    break;
                }
                case CCCodeItemTypeReal: {
                    encodeDoubleIntoCodeData(DoubleFromInt64(CTypeVal), codeData);
                    break;
                }
                default:
                    break;
            }
        }
        
        int64_t endLoc = codeData->currentSeek();

        _CCCFKVCacheObject *cacheObj = [[_CCCFKVCacheObject alloc] initWithCacheObject:obj];
        [cacheObj setRange:NSMakeRange((NSUInteger)startLoc, (NSUInteger)(endLoc - startLoc))];
        [newDict setObject:cacheObj forKey:key];
    }];
    return newDict;
}

static inline NSMutableDictionary<id, _CCCFKVCacheObject*>* _decodeBuffer(uint8_t *ptr, int32_t offset, int64_t size, BOOL copyData)
{
    if (ptr == NULL || size <= 0) {
        return [NSMutableDictionary dictionary];
    }
    ptr = ptr + offset;
    int64_t location = offset;
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    while (size > 0) {
        int64_t offsetTmp = 0;
        id key = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, NULL);
        ptr += offsetTmp;
        size -= offsetTmp;
        location += offsetTmp;
        if (size <= 0 || offsetTmp <= 0 /*|| key == nil*/) {
            break;
        }
        
        NSRange r = unpackBuffer(ptr, size, NULL, NULL, NULL, &offsetTmp);
        ptr += offsetTmp;
        size -= offsetTmp;
        location += offsetTmp;
        if (key && r.location != NSNotFound) {
            if (r.length > 0) {
                _CCCFKVCacheObject *cacheObject = [[_CCCFKVCacheObject alloc] init];
                if (copyData) {
                    [cacheObject setCacheObject:[NSData dataWithBytes:ptr - offsetTmp length:(NSUInteger)offsetTmp] withType:_CCCFKVCacheObjectTypeEncodedData];
                }
                else {
                    [cacheObject setRange:NSMakeRange((NSUInteger)(location - offsetTmp),  (NSUInteger)offsetTmp)];
                }
                [dict setObject:cacheObject forKey:key];
            }
            else {
                [dict removeObjectForKey:key];
            }
        }
        else {
            //这里避免空转
            if (offsetTmp <= 0) {
                break;
            }
        }
    }
    return dict;
}

static inline CCMutableCodeData *_startEncryptContentDataFromDict(struct CCCFKVContext *ctx, NSDictionary<id, _CCCFKVCacheObject*> *currentDict, NSMutableDictionary<id, _CCCFKVCacheObject*>** outNewDict, BOOL checkCondition, CCCFKVError *error)
{
    CCAESCryptor *cryptor = ctx->_sharedPtrCryptor.get();
    /*
    if (cryptor && checkCondition && !_checkContentCryptKeyHeader(ctx)) {
        if (error) {
            *error = CCCFKVErrorCryptKeyError;
        }
        return NULL;
    }
    */
    
    CCMutableCodeData *encodeData = new CCMutableCodeData();
    
    encodeData->ensureRemSize(CCCFKV_CONTENT_HEADER_SIZE);
    encodeData->seekTo(CCCFKV_CONTENT_HEADER_SIZE);
    
    NSMutableDictionary<id, _CCCFKVCacheObject*> *newDictTmp = _encodeDictionary(ctx, currentDict, encodeData);
    if (outNewDict) {
        *outNewDict = newDictTmp;
    }
    
    if (cryptor) {
        ctx->_sharedPtrPlainContentData = make_shared<CCMutableCodeData>(encodeData);
        
        int64_t dataSize = encodeData->dataSize();
        int64_t outSize = dataSize;
        
        encodeData->ensureRemSize(dataSize + CCAESKeySize128);
        
        memcpy(encodeData->bytes(), ctx->_hashKey, sizeof(ctx->_hashKey));
        ctx->_sharedPtrCryptor->reset();
        ctx->_sharedPtrCryptor->crypt(CCCryptOperationEncrypt, encodeData, encodeData);
        outSize = encodeData->dataSize();
        if (dataSize != outSize) {
            if (error) {
                *error = (CCCFKVError)_CCCFKVInterErrorCryptModeError;
            }
        }
    }
    
    return encodeData;
}

static inline CCCodeData *_startDecryptContentDataWithDecryptCondition(struct CCCFKVContext *ctx, BOOL decryptCondition, CCCFKVError *error)
{
    if (ctx->_sharedPtrCryptor.get() != nullptr && decryptCondition) {
        int64_t dataSize = ctx->_sharedPtrContentData->dataSize();
        int64_t outSize = dataSize;
        CCMutableCodeData *codeData = new CCMutableCodeData(outSize);
        
        ctx->_sharedPtrCryptor->reset();
        ctx->_sharedPtrCryptor->crypt(CCCryptOperationDecrypt, ctx->_sharedPtrContentData.get(), codeData);
        outSize = codeData->dataSize();
        if (outSize < CCCFKV_CONTENT_HEADER_SIZE || dataSize != outSize || memcmp(codeData->bytes(), ctx->_hashKey, sizeof(ctx->_hashKey))) {
            if (error) {
                *error = CCCFKVErrorCryptKeyError;
            }
            delete codeData;
            return NULL;
        }
        codeData->seekTo(outSize);
        
        memset(codeData->bytes(), 0, CCCFKV_CONTENT_HEADER_SIZE);
        
        ctx->_sharedPtrPlainContentData = shared_ptr<CCCodeData>(codeData);
        
        return codeData;
    }
    else {
        return ctx->_sharedPtrContentData.get();
    }
}

//返回原来是否有密码
static inline BOOL _setupCryptorWithCryptKey(struct CCCFKVContext *ctx, CCCodeData *cryptKey, BOOL checkCryptKey, CCCFKVError *error)
{
    CCCodeData *hashKey = nil;
    CCAESKeySize keySize = CCAESKeySize128;
    CCAESKeyType keyType = CCAESKeyType128;
    
    CCCryptMode cryptMode = CCCryptModeCFB;
    
    CCCodeData *key = nil;
    CCCodeData *vector = nil;
    uint8_t cryptorInfo = ctx->_cryptorInfo;
    int64_t hashKeyDataSize = 0;
    
    BOOL oldHave = NO;
    if (cryptorInfo == 0) {
        hashKey = _hashCryptKey(cryptKey, &keyType);
    }
    else {
        oldHave = YES;
        keyType = (CCAESKeyType)(TYPE_AND(cryptorInfo, 3) - 1);
        cryptMode = (CCCryptMode)TYPE_RS(cryptorInfo, 2);
        
        hashKey = _hashForData(cryptKey, _CCHashTypeSHA512);
        
        if (checkCryptKey) {
            CCCodeData *cryptKeyHash = _hashForData(cryptKey, _CCHashTypeSHA256);
            if (cryptKeyHash) {
                if (memcmp(cryptKeyHash->bytes(), ctx->_hashKey, CC_SHA256_DIGEST_LENGTH)) {
                    //密码不一致
                    if (error) {
                        *error = CCCFKVErrorCryptKeyError;
                    }
                    delete cryptKeyHash;
                    goto SETUP_CRYPTOR_WITH_CRYPT_KEY_ERR_END;
                }
                else {
                    delete cryptKeyHash;
                }
            }
        }
    }
    
    hashKeyDataSize = hashKey->dataSize();
    keySize = AES_KEYSIZE_FROM_KEYTYPE(keyType);
    if (hashKeyDataSize < keySize) {
        if (error) {
            *error = (CCCFKVError)_CCCFKVInterErrorKeySizeError;
        }
        goto SETUP_CRYPTOR_WITH_CRYPT_KEY_ERR_END;
    }
    key = new CCMutableCodeData(keySize);
    key->writeBuffer(hashKey->bytes(), keySize);
    //只运行这几种流式加密的方式
    if (cryptMode != CCCryptModeCFB &&
        cryptMode != CCCryptModeCFB1 &&
        cryptMode != CCCryptModeCFB8 &&
        cryptMode != CCCryptModeOFB) {
        if (error) {
            *error = (CCCFKVError)_CCCFKVInterErrorCryptModeError;
        }
        goto SETUP_CRYPTOR_WITH_CRYPT_KEY_ERR_END;
    }
    
    vector = new CCMutableCodeData(CCAESKeySize128);
    vector->writeBuffer(hashKey->bytes() + hashKeyDataSize - CCAESKeySize128, CCAESKeySize128);
    
    ctx->_sharedPtrCryptor = make_shared<CCAESCryptor>(key, keyType, vector, cryptMode);

    _updateCryptorInfoWithCryptKey(ctx, cryptKey);
    
SETUP_CRYPTOR_WITH_CRYPT_KEY_ERR_END:
    if (hashKey) {
        delete hashKey;
    }
    if (key) {
        delete key;
    }
    if (vector) {
        delete vector;
    }
    return oldHave;
}

static inline NSMutableDictionary *_fullWriteBack(struct CCCFKVContext *ctx, NSDictionary *dict, BOOL checkCondition, CCCFKVError *error)
{
    CCCFKVError err = CCCFKVErrorNone;
    NSMutableDictionary *newDict = nil;
    int64_t dataSize = 0;
    int64_t codeSize = 0;
    CCMutableCodeData *encodeNewData = _startEncryptContentDataFromDict(ctx, dict, &newDict, checkCondition, &err);
    if (error) {
        *error = err;
    }
    if (err) {
        goto _FULL_WRITE_BACK_ERR_END;
    }
    
    _updateCodeItemCnt(ctx, (uint32_t)newDict.count);
    
    dataSize = encodeNewData->dataSize();
    codeSize = dataSize - CCCFKV_CONTENT_HEADER_SIZE;
    if (ctx->_codeSize != codeSize || memcmp(ctx->_sharedPtrContentData->bytes(), encodeNewData->bytes(), (size_t)dataSize) != 0) {
        
        _updateCodeSize(ctx, codeSize);
        
        ctx->_sharedPtrContentData->bzero();
        ctx->_sharedPtrContentData->writeCodeData(encodeNewData);
        _updateCRC(ctx, 0, (uint8_t*)encodeNewData->bytes() + CCCFKV_CONTENT_HEADER_SIZE, (uint32_t)codeSize);
    }
    else {
        _readContentData(ctx);
    }
    
_FULL_WRITE_BACK_ERR_END:
    if (encodeNewData) {
        delete encodeNewData;
    }
    
    return newDict;
}

BOOL _updateCryptKey(struct CCCFKVContext *ctx, CCCodeData *cryptKey, NSMutableDictionary **outNewDict)
{
    if ((cryptKey == NULL || cryptKey->dataSize() == 0) && ctx->_cryptorInfo == 0) {
        return YES;
    }
    CCCodeData *hashKey = _hashForData(cryptKey, _CCHashTypeSHA256);
    if (hashKey) {
        if (memcmp(hashKey->bytes(), ctx->_hashKey, CC_SHA256_DIGEST_LENGTH) == 0) {
            delete hashKey;
            return YES;
        }
        delete hashKey;
    }

    
    CCCFKVError error = CCCFKVErrorNone;
    CCCodeData *plainData = _startDecryptContentDataWithDecryptCondition(ctx, YES, &error);
    if (error) {
        NSLog(@"解密错误，error=%@",@(error));
        _reportError(ctx, error);
        return NO;
    }
    
    NSMutableDictionary *dict = _decodeBuffer(plainData->bytes(), CCCFKV_CONTENT_HEADER_SIZE, ctx->_codeSize, NO);
    NSInteger dictCnt = [dict count];
    if (ctx->_keyItemCnt != dictCnt) {
        //说明出现解码错误，不回写数据，做close
        error = CCCFKVErrorCoderError;
        NSLog(@"解码错误，error=%@",@(error));
        _reportError(ctx, error);
        return NO;
    }
    
    uint8_t oldCryptorInfo = ctx->_cryptorInfo;
    uint8_t oldHashKey[CC_SHA256_DIGEST_LENGTH] = {0};
    memcpy(oldHashKey, ctx->_hashKey, CC_SHA256_DIGEST_LENGTH);
    
    uint8_t oldContentHeader[CCCFKV_CONTENT_HEADER_SIZE] = {0};
    memcpy(oldContentHeader, ctx->_sharedPtrContentData->bytes(), CCCFKV_CONTENT_HEADER_SIZE);
    
    shared_ptr<CCAESCryptor> oldCryptor;
    ctx->_sharedPtrCryptor.swap(oldCryptor);
    
    if (cryptKey->dataSize() == 0) {
        _updateCryptorInfoWithCryptKey(ctx, cryptKey);
    }
    else {
        _setupCryptorWithCryptKey(ctx, cryptKey, NO, NULL);
    }
    
    error = CCCFKVErrorNone;
    BOOL OK = YES;
    NSMutableDictionary<id, _CCCFKVCacheObject*> *tmp = _fullWriteBack(ctx, dict, NO, &error);
    if (error) {
        OK = NO;
        
        //更新为原来的密码和cryptor
        _updateCryptorInfoWithHashKey(ctx, oldCryptorInfo, oldHashKey, oldContentHeader);

        ctx->_sharedPtrCryptor.swap(oldCryptor);
        oldCryptor.reset();

        _reportError(ctx, error);
    }
    else {
        if (oldCryptor.get()) {
            oldCryptor->reset();
        }
        if (outNewDict) {
            *outNewDict = tmp;
        }
    }
    
    
    return OK;
}

static inline BOOL _updateSize(struct CCCFKVContext *ctx, int64_t size, BOOL truncate)
{
    if (ctx->_fd <= 0) {
        if (_loadFromFileWithCryptKey(ctx, NULL) == nil) {
            return NO;
        }
    }
    
    if (size < MIN_MMAP_SIZE_s) {
        size = MIN_MMAP_SIZE_s;
    }
    
    if (ctx->_ptr && ctx->_ptr != MAP_FAILED) {
        if (munmap(ctx->_ptr, (size_t)ctx->_size)) {
            return NO;
        }
    }
    
    if (ctx->_size != size /*truncate*/) {
        //这个函数涉及到IO操作，最影响性能
        if (ftruncate(ctx->_fd, size) != 0) {
            return NO;
        }
    }
    
    uint8_t *newPtr = (uint8_t*)mmap(ctx->_ptr, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->_fd, 0);
    if (newPtr == NULL || newPtr == MAP_FAILED) {
        return NO;
    }
    ctx->_ptr = newPtr;
    ctx->_size = size;
    
    ctx->_sharedPtrHeaderData = make_shared<CCCodeData>(ctx->_ptr, CCCFKV_CODE_DATA_HEAD_SIZE);
    ctx->_sharedPtrContentData = make_shared<CCCodeData>(ctx->_ptr + CCCFKV_CODE_DATA_HEAD_SIZE, ctx->_size - CCCFKV_CODE_DATA_HEAD_SIZE);
    
    ctx->_sharedPtrPlainContentData = ctx->_sharedPtrContentData;
    
    _readHeaderInfo(ctx);
    
//    int64_t maxCodeSize = ctx->_size - CCCFKV_CODE_DATA_HEAD_SIZE - CCCFKV_CODE_DATA_HEAD_SIZE;
//    if (ctx->_codeSize > maxCodeSize) {
//        _updateCodeSize(ctx, maxCodeSize);
//    }
    
    _readContentData(ctx);
    
    return YES;
}

static inline NSMutableDictionary *_loadFromFileWithCryptKey(struct CCCFKVContext *ctx, CCCodeData *cryptKey)
{
    ctx->_fd = open(ctx->_filePath.c_str(), O_RDWR|O_CREAT, S_IRWXU);
    if (ctx->_fd < 0) {
        return nil;
    }
    
    int64_t fileSize  = _getFileSizeWithFD(ctx->_fd);
    uint64_t size = 0;
    if (fileSize <= MIN_MMAP_SIZE_s) {
        size = MIN_MMAP_SIZE_s;
    }
    else {
        size = (fileSize + DEFAULT_PAGE_SIZE_s - 1)/DEFAULT_PAGE_SIZE_s * DEFAULT_PAGE_SIZE_s;
    }
    
    if (_updateSize(ctx, size, size != fileSize) == NO) {
        _closeFile(ctx);
        return nil;
    }
    
    BOOL fullWriteBack = _shouldFullWriteBack(ctx);
    if (ctx->_codeSize > 0 && !_checkDataWithCRC(ctx)) {
        fullWriteBack = _reportWarning(ctx, CCCFKVErrorCRCError);
    }
    
    /*
     *如果cryptorInfo>0表示有加密的，但是现在没有加密器和密钥，表示有错误
     *如果第一次启动，cryptor为NULL，但是cryptKey应该有数据
     *如果非第一次启动，CFKV内部会调用_loadFromFileWithCryptKey，此时可以为cryptKey为NULL，但是cryptor不能为NULL
    */
    CCAESCryptor *cryptor = ctx->_sharedPtrCryptor.get();
    if (ctx->_cryptorInfo > 0 && cryptor == NULL && (cryptKey == NULL || cryptKey->dataSize() == 0)) {
        _reportError(ctx, CCCFKVErrorCryptKeyError);
        return nil;
    }
    
    
    uint8_t oldCryptorInfo = ctx->_cryptorInfo;
    uint8_t oldHashKey[CC_SHA256_DIGEST_LENGTH] = {0};
    memcpy(oldHashKey, ctx->_hashKey, CC_SHA256_DIGEST_LENGTH);
    
    uint8_t oldContentHeader[CCCFKV_CONTENT_HEADER_SIZE] = {0};
    memcpy(oldContentHeader, ctx->_sharedPtrContentData->bytes(), CCCFKV_CONTENT_HEADER_SIZE);
    
    CCCFKVError error = CCCFKVErrorNone;
    
    BOOL doDecrypt = cryptor ? YES : NO;
    //如果为第一次启动，
    if (cryptor == NULL && cryptKey && cryptKey->dataSize() > 0) {
        BOOL oldHave = _setupCryptorWithCryptKey(ctx, cryptKey, YES, &error);
        if (error) {
            if (error == CCCFKVErrorCryptKeyError) {
                NSLog(@"密码错误，error=%@",@(error));
            }
            else if ((_CCCFKVInterError)error == _CCCFKVInterErrorKeySizeError) {
                NSLog(@"密码长度错误，error=%@",@(error));
            }
            else if ((_CCCFKVInterError)error == _CCCFKVInterErrorCryptModeError) {
                NSLog(@"加密模式错误，error=%@",@(error));
            }
            else {
                NSLog(@"创建加密器错误，error=%@",@(error));
            }
            _reportError(ctx, error);
            return nil;
        }
        
        doDecrypt = oldHave;
        //以前没有，现在有密码，相当于修改密码，需要回写
        if (oldHave == NO) {
            fullWriteBack = YES;
        }
    }
    CCCodeData *plainData = _startDecryptContentDataWithDecryptCondition(ctx, doDecrypt, &error);
    if (error) {
        _updateCryptorInfoWithHashKey(ctx, oldCryptorInfo, oldHashKey, oldContentHeader);
        NSLog(@"密码错误，error=%@",@(error));
        _reportError(ctx, error);
        return nil;
    }
    
    
    
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (ctx->_codeSize > 0) {
        dict = _decodeBuffer((uint8_t*)plainData->bytes(), CCCFKV_CONTENT_HEADER_SIZE, ctx->_codeSize, NO);
    }
    
    NSInteger dictCnt = dict.count;
    if (ctx->_keyItemCnt != dictCnt) {
        //说明出现解码错误，不回写数据，做close
        error = CCCFKVErrorCoderError;
        NSLog(@"解码错误，error=%@,dictCnt=%@,keyItemCnt=%@",@(error),@(dictCnt),@(ctx->_keyItemCnt));
        _reportError(ctx, error);
        return nil;
    }
    
    if (fullWriteBack) {
        dict = _fullWriteBack(ctx, dict, NO, &error);
        
        if (error != CCCFKVErrorNone) {
            _reportError(ctx, error);
            return nil;
        }
    }
    
    return dict;
}


static inline BOOL _ensureAppendSize(struct CCCFKVContext *ctx, uint64_t appendSize, NSDictionary *currentDict, NSDictionary **newOutDict, CCCFKVError *error)
{
    if (ctx == NULL ) {
        return NO;
    }
    if (!CCCFKV_IS_FILE_OPEN(ctx)) {
        if (_loadFromFileWithCryptKey(ctx, nil) == nil) {
            return NO;
        }
    }
    int64_t remSize = ctx->_sharedPtrContentData->remSize();
    if (remSize > appendSize) {
        return YES;
    }
    
    //首先看下是否有许多重复的或者删除了的数据，就考虑先重新写一次，避免无条件的扩张内存
    CCCFKVError errorTmp = CCCFKVErrorNone;
    if (_shouldFullWriteBack(ctx)) {
        NSDictionary *dict = _fullWriteBack(ctx, currentDict, YES, &errorTmp);
        if (error) {
            *error = errorTmp;
        }
        if (errorTmp) {
            return NO;
        }
        
        if (newOutDict) {
            *newOutDict = dict;
        }
        
        remSize = ctx->_sharedPtrContentData->remSize();

        if (remSize > appendSize) {
            return YES;
        }
    }
    
    uint64_t newSize = ctx->_size;
    uint64_t needSize = ctx->_size + appendSize;
    do {
        newSize = TYPE_LS(newSize, 1);
    } while (newSize < needSize);
    
    if (_updateSize(ctx, newSize,YES) == NO) {
        return NO;
    }
    
    NSLog(@"appendSize:%lld,newSize:%lld",appendSize,newSize);
    if (_shouldFullWriteBack(ctx)) {
        NSDictionary *dict = _fullWriteBack(ctx, currentDict, YES, &errorTmp);
        
        if (error) {
            *error = errorTmp;
        }
        if (errorTmp) {
            return NO;
        }
        
        if (newOutDict) {
            *newOutDict = dict;
        }
    }
    return YES;
}

_CCCFKVCacheObject* _writeData(struct CCCFKVContext *ctx, CCMutableCodeData *data, int64_t keySize, NSDictionary *currentDict, NSDictionary** outNewDict)
{
    _CCCFKVCacheObject *cacheObject = [[_CCCFKVCacheObject alloc] init];
    int64_t dataSize = data->dataSize();
    
    CCAESCryptor *cryptor = ctx->_sharedPtrCryptor.get();
    CCCodeData *contentData = ctx->_sharedPtrContentData.get();
    
    if (cryptor) {
        int64_t outSize = dataSize;
        cryptor->crypt(CCCryptOperationEncrypt, data, data);
        outSize = data->dataSize();
        if (outSize != dataSize) {
            return nil;
        }
    }
    
    BOOL isOK = _ensureAppendSize(ctx, dataSize, currentDict, outNewDict, NULL);
    if (isOK == NO) {
        return nil;
    }
    contentData = ctx->_sharedPtrContentData.get();
    
    if (cryptor == NULL && dataSize > keySize) {
        [cacheObject setRange:NSMakeRange((NSUInteger)(contentData->dataSize() + keySize), (NSUInteger)(dataSize - keySize))];
    }
    contentData->writeCodeData(data);

    _updateCodeItemCnt(ctx, ctx->_codeItemCnt + 1);
    
    uint8_t *ptr = contentData->bytes() + CCCFKV_CONTENT_HEADER_SIZE + ctx->_codeSize;
    
    _updateCodeSize(ctx, ctx->_codeSize + dataSize);
    
    _updateCRC(ctx, ctx->_codeContentCRC, ptr, (uint32_t)dataSize);
    
    return cacheObject;
}

static inline void _clearEncodeData(struct CCCFKVContext *ctx, BOOL truncateFileSize)
{
    [ctx->_dict removeAllObjects];
    
    _updateCodeSize(ctx, 0);
    _updateKeyItemCnt(ctx);
    _updateCodeItemCnt(ctx, 0);
    _updateCRC(ctx, 0, NULL, 0);
    
    ctx->_sharedPtrContentData->bzero();
    
    if (truncateFileSize) {
        _updateSize(ctx, 0, YES);
    }

    if (ctx->_sharedPtrCryptor.get()) {
        ctx->_sharedPtrCryptor->reset();
        //加密HashKey
        ctx->_sharedPtrContentData->writeBuffer(ctx->_hashKey, CC_SHA256_DIGEST_LENGTH);
        ctx->_sharedPtrCryptor->crypt(CCCryptOperationEncrypt, ctx->_sharedPtrContentData.get(), ctx->_sharedPtrContentData.get());
        
        ctx->_sharedPtrPlainContentData.reset();
    }
}

static inline void _closeFile(struct CCCFKVContext *ctx)
{
    if (ctx->_ptr != NULL && ctx->_ptr != MAP_FAILED) {
        munmap(ctx->_ptr, (size_t)ctx->_size);
        ctx->_ptr = NULL;
        ctx->_size = 0;
    }
    if (ctx->_fd) {
        close(ctx->_fd);
        ctx->_fd = 0;
    }
    ctx->_version = 0;
    ctx->_codeSize = 0;
    ctx->_keyItemCnt = 0;
    ctx->_codeItemCnt = 0;
    ctx->_codeContentCRC = 0;
    ctx->_cryptorInfo = 0;
    
    [ctx->_dict removeAllObjects];
    
    memset(ctx->_hashKey, 0, sizeof(ctx->_hashKey));

    ctx->_sharedPtrHeaderData.reset();
    ctx->_sharedPtrContentData.reset();
    ctx->_sharedPtrCryptor.reset();
    ctx->_sharedPtrTmpCodeData.reset();

    ctx->_sharedPtrPlainContentData.reset();
    ctx->lastError = CCCFKVErrorNone;
    ctx->CFKV = NULL;
    
}




void CCCFKV::setupCFKVDefault()
{
    //    DEFAULT_PAGE_SIZE_s = getpagesize();
    MIN_MMAP_SIZE_s = DEFAULT_PAGE_SIZE_s;
    
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)calloc(1, sizeof(struct CCCFKVContext));
    if (ctx) {
        ctx->_fd = 0;
        ctx->_size = 0;
        ctx->_ptr = NULL;
        ctx->_version = 1;
        ctx->_codeSize = 0;
        ctx->_keyItemCnt = 0;
        ctx->_codeItemCnt = 0;
        ctx->_codeContentCRC = 0;
        ctx->_cryptorInfo = 0;
        
        memset(ctx->_hashKey, 0, sizeof(ctx->_hashKey));
        
        ctx->_dict = [NSMutableDictionary dictionary];
        ctx->_lock = dispatch_semaphore_create(1);
        
        ctx->_sharedPtrTmpCodeData = make_shared<CCMutableCodeData>();
        
        ctx->CFKV = this;
        ctx->lastError = CCCFKVErrorNone;
    }
    delegate = NULL;
    ptrCFKVContext = ctx;
}

CCCFKV::CCCFKV()
{
    CCCFKV(DEFAULT_CCCFKV_NAME, "", NULL);
}

CCCFKV::CCCFKV(const string &name)
{
    CCCFKV(name, "", NULL);
}

CCCFKV::CCCFKV(const string &name, const string &path)
{
    CCCFKV(name, path, NULL);
}

CCCFKV::CCCFKV(const string &name, const string &path, CCCodeData *cryptKey)
{
    setupCFKVDefault();
    if (ptrCFKVContext) {
        
        struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
        
        string nameT = name;
        string pathT = path;
        if (nameT.length() == 0) {
            nameT = DEFAULT_CCCFKV_NAME;
        }
        
        if (pathT.length() == 0) {
            pathT = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES) firstObject].UTF8String;
        }
        
        ctx->_filePath = pathT + "/" + nameT;
        
        size_t p = ctx->_filePath.find_last_of('/');
        string dir = ctx->_filePath.substr(0, p);
        _checkAndMakeDirectory(dir);
        
        _sync_lock(ctx->_lock, ^{
            NSMutableDictionary *dict = _loadFromFileWithCryptKey(ctx, cryptKey);
            ctx->_dict = dict;
        });
    }
}

CCCFKV::~CCCFKV()
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    _closeFile(ctx);
    if (ctx) {
        free(ctx);
        ptrCFKVContext = NULL;
    }
}

string CCCFKV::getFilePath()
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    return ctx->_filePath;
}

BOOL CCCFKV::updateCryptKey(CCCodeData *cryptKey)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO) {
        return NO;
    }
    
    _sync_lock(ctx->_lock, ^{
        NSMutableDictionary *newDict = ctx->_dict;
        _updateCryptKey(ctx, cryptKey, &newDict);
        if (ctx->_dict != newDict) {
            ctx->_dict = newDict;
        }
    });
    return YES;
}

/**
 设置以Key作为键，以Object作为值
 
 @param object 作为值，在object为nil是removeObject的操作
 @param key 作为键，不可为空
 @return 返回是否成功，YES-成功，NO-失败
 */
BOOL CCCFKV::setObjectForKey(id object, id key)
{
    Class topSuperClass = [object cc_codeToTopSuperClass];
    return setObjectForKey(object, topSuperClass, key);
}

/**
 设置以Key作为键，以Object作为值
 
 @param object 作为值，在object为nil是removeObject的操作
 @param topSuperClass object编码到父类的类名
 @param key key 作为键，不可为空
 @return 返回是否成功，YES-成功，NO-失败
 */
BOOL CCCFKV::setObjectForKey(id object, Class topSuperClass, id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return NO;
    }
    
    id encodeObject = object;
    if (object == nil) {
        encodeObject = [NSData data];
    }
    
    __block BOOL isOK = NO;
    _sync_lock(ctx->_lock, ^{
        //1、判断之前是否有
        _CCCFKVCacheObject *cache = nil;
        _CCCFKVCacheObjectType cacheObjectType = _CCCFKVCacheObjectTypeNone;
        if (object) {
            cache = [ctx->_dict objectForKey:key];
            if (cache) {
                cacheObjectType = (_CCCFKVCacheObjectType)(cache.cacheObjectType & 0XFFFF);
                if (cacheObjectType == _CCCFKVCacheObjectTypeUncodedObject) {
                    id objTmp = [cache cacheObject];
                    if ([objTmp hash] == [object hash] && [objTmp isEqual:object]) {
                        isOK = YES;
                        return;
                    }
                }
            }
        }

        //2、进行key，val编码
        CCMutableCodeData *tmpCodeData = ctx->_sharedPtrTmpCodeData.get();
        CCAESCryptor *cryptor = ctx->_sharedPtrCryptor.get();
        tmpCodeData->truncateTo(0);
        encodeObjectToTopSuperClassIntoCodeData(key, NULL, tmpCodeData, NULL);
        int64_t keySize = tmpCodeData->currentSeek();
        if (keySize == 0) {
            isOK = NO;
            return;
        }
        if (object) {
            encodeObjectToTopSuperClassIntoCodeData(encodeObject, topSuperClass, tmpCodeData, NULL);
        }
        else {
            encodeDataIntoCodeData(encodeObject, tmpCodeData);
        }
        
        //3、再通过编码判断
        if (cache && cacheObjectType == _CCCFKVCacheObjectTypeDataRange && ctx->_sharedPtrCryptor.get() == nullptr) {
            int64_t dataSize = tmpCodeData->dataSize() - keySize;
            if (dataSize == cache.dataRange.length) {
                uint8_t *ptrNew = tmpCodeData->bytes() + keySize;
                uint8_t *ptrOld =  ctx->_sharedPtrPlainContentData->bytes() + cache.dataRange.location;
                if (memcmp(ptrNew, ptrOld, dataSize) == 0) {
                    if (cryptor) {
                        [cache setCacheObject:object withType:_CCCFKVCacheObjectTypeUncodedObject];
                    }
                    isOK = YES;
                    return;
                }
            }
        }
        
        NSMutableDictionary *newDict = ctx->_dict;
        _CCCFKVCacheObject *cacheObj = _writeData(ctx, tmpCodeData, keySize, ctx->_dict, &newDict);
        if (cacheObj == nil) {
            isOK = NO;
            return;
        }
        
        if (ctx->_dict != newDict) {
            ctx->_dict = newDict;
        }
        
        if (object) {
            if (cryptor) {
                [cacheObj setCacheObject:object withType:_CCCFKVCacheObjectTypeUncodedObject];
            }
            [ctx->_dict setObject:cacheObj forKey:key];
        }
        else {
            [ctx->_dict removeObjectForKey:key];
        }
        _updateKeyItemCnt(ctx);
        
        isOK = YES;
    });
    return isOK;
}

BOOL CCCFKV::setFloatForKey(float val, id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return NO;
    }
    
    __block BOOL isOK = NO;
    _sync_lock(ctx->_lock, ^{
        
        _CCCFKVCacheObject *cache = [ctx->_dict objectForKey:key];
        if (cache && [cache haveCTypeValue] && cache.CTypeItemType == CCCodeItemTypeRealF && Int64FromFloat(val) == cache.CTypeValue) {
            isOK = YES;
            return;
        }
        
        CCMutableCodeData *tmpCodeData = ctx->_sharedPtrTmpCodeData.get();
        
        tmpCodeData->truncateTo(0);
        encodeObjectToTopSuperClassIntoCodeData(key, NULL, tmpCodeData, NULL);
        int64_t keySize = tmpCodeData->currentSeek();
        if (keySize == 0) {
            isOK = NO;
            return;
        }
        encodeFloatIntoCodeData(val, tmpCodeData);
        
        if (cache && ctx->_sharedPtrCryptor.get() == nullptr) {
             _CCCFKVCacheObjectType cacheObjectType =  (_CCCFKVCacheObjectType)(cache.cacheObjectType & 0XFFFF);
            int64_t dataSize = tmpCodeData->dataSize() - keySize;
            if (cacheObjectType == _CCCFKVCacheObjectTypeDataRange && dataSize == cache.dataRange.length) {
                uint8_t *ptrNew = tmpCodeData->bytes() + keySize;
                uint8_t *ptrOld =  ctx->_sharedPtrPlainContentData->bytes() + cache.dataRange.location;
                if (memcmp(ptrNew, ptrOld, dataSize) == 0) {
                    int32_t ival = Int32FromFloat(val);
                    [cache addCTypeValue:ival CTypeItemType:CCCodeItemTypeRealF];
                    isOK = YES;
                    return;
                }
            }
        }
        
        NSMutableDictionary *newDict = ctx->_dict;
        _CCCFKVCacheObject *cacheObj = _writeData(ctx, tmpCodeData, keySize, ctx->_dict, &newDict);
        if (cacheObj == nil) {
            isOK = NO;
            return;
        }
        
        if (ctx->_dict != newDict) {
            ctx->_dict = newDict;
        }
        
        int32_t ival = Int32FromFloat(val);
        [cacheObj addCTypeValue:ival CTypeItemType:CCCodeItemTypeRealF];
        
        [ctx->_dict setObject:cacheObj forKey:key];
        
        _updateKeyItemCnt(ctx);
        
        isOK = YES;
    });
    return isOK;
}

BOOL CCCFKV::setDoubleForKey(double val, id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return NO;
    }
    
    __block BOOL isOK = NO;
    _sync_lock(ctx->_lock, ^{
        _CCCFKVCacheObject *cache = [ctx->_dict objectForKey:key];
        if (cache && [cache haveCTypeValue] && cache.CTypeItemType == CCCodeItemTypeReal && Int64FromDouble(val) == cache.CTypeValue) {
            isOK = YES;
            return;
        }
        
        CCMutableCodeData *tmpCodeData = ctx->_sharedPtrTmpCodeData.get();
        
        tmpCodeData->truncateTo(0);
        encodeObjectToTopSuperClassIntoCodeData(key, NULL, tmpCodeData, NULL);
        int64_t keySize = tmpCodeData->currentSeek();
        if (keySize == 0) {
            isOK = NO;
            return;
        }
        encodeDoubleIntoCodeData(val, tmpCodeData);
        
        if (cache && ctx->_sharedPtrCryptor.get() == nullptr) {
            _CCCFKVCacheObjectType cacheObjectType =  (_CCCFKVCacheObjectType)(cache.cacheObjectType & 0XFFFF);
            int64_t dataSize = tmpCodeData->dataSize() - keySize;
            if (cacheObjectType == _CCCFKVCacheObjectTypeDataRange && dataSize == cache.dataRange.length) {
                uint8_t *ptrNew = tmpCodeData->bytes() + keySize;
                uint8_t *ptrOld =  ctx->_sharedPtrPlainContentData->bytes() + cache.dataRange.location;
                if (memcmp(ptrNew, ptrOld, dataSize) == 0) {
                    int64_t ival = Int64FromDouble(val);
                    [cache addCTypeValue:ival CTypeItemType:CCCodeItemTypeReal];
                    isOK = YES;
                    return;
                }
            }
        }
        
        NSMutableDictionary *newDict = ctx->_dict;
        _CCCFKVCacheObject *cacheObj = _writeData(ctx, tmpCodeData, keySize, ctx->_dict, &newDict);
        if (cacheObj == nil) {
            isOK = NO;
            return;
        }
        
        if (ctx->_dict != newDict) {
            ctx->_dict = newDict;
        }
        
        int64_t ival = Int64FromDouble(val);
        [cacheObj addCTypeValue:ival CTypeItemType:CCCodeItemTypeReal];
        
        [ctx->_dict setObject:cacheObj forKey:key];
        
        _updateKeyItemCnt(ctx);
        
        isOK = YES;
    });
    return isOK;
}

BOOL CCCFKV::setIntegerForKey(int64_t val, id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return NO;
    }
    
    __block BOOL isOK = NO;
    _sync_lock(ctx->_lock, ^{
        _CCCFKVCacheObject *cache = [ctx->_dict objectForKey:key];
        if (cache && [cache haveCTypeValue] && cache.CTypeItemType == CCCodeItemTypeInteger && val == cache.CTypeValue) {
            isOK = YES;
            return;
        }
        
        CCMutableCodeData *tmpCodeData = ctx->_sharedPtrTmpCodeData.get();
        tmpCodeData->truncateTo(0);
        encodeObjectToTopSuperClassIntoCodeData(key, NULL, tmpCodeData, NULL);
        int64_t keySize = tmpCodeData->currentSeek();
        if (keySize == 0) {
            isOK = NO;
            return;
        }
        encodeIntegerIntoCodeData(val, tmpCodeData);
        
        if (cache && ctx->_sharedPtrCryptor.get() == nullptr) {
            _CCCFKVCacheObjectType cacheObjectType =  (_CCCFKVCacheObjectType)(cache.cacheObjectType & 0XFFFF);
            int64_t dataSize = tmpCodeData->dataSize() - keySize;
            if (cacheObjectType == _CCCFKVCacheObjectTypeDataRange && dataSize == cache.dataRange.length) {
                uint8_t *ptrNew = tmpCodeData->bytes() + keySize;
                uint8_t *ptrOld =  ctx->_sharedPtrPlainContentData->bytes() + cache.dataRange.location;
                if (memcmp(ptrOld, ptrNew, dataSize) == 0) {
                    [cache addCTypeValue:val CTypeItemType:CCCodeItemTypeInteger];
                    isOK = YES;
                    return;
                }
            }
        }
        
        NSMutableDictionary *newDict = ctx->_dict;
        _CCCFKVCacheObject *cacheObj = _writeData(ctx, tmpCodeData, keySize, ctx->_dict, &newDict);
        if (cacheObj == nil) {
            isOK = NO;
            return;
        }
        
        if (ctx->_dict != newDict) {
            ctx->_dict = newDict;
        }

        [cacheObj addCTypeValue:val CTypeItemType:CCCodeItemTypeInteger];
        
        [ctx->_dict setObject:cacheObj forKey:key];
        
        _updateKeyItemCnt(ctx);
        
        isOK = YES;
    });
    return isOK;
}

id CCCFKV::getObjectForKey(id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return nil;
    }
    
    __block id decodeObject = nil;
    _sync_lock(ctx->_lock, ^{
        _CCCFKVCacheObject *cacheObject = [ctx->_dict objectForKey:key];
        
        decodeObject = cacheObject.decodeObject;
        if (decodeObject == nil) {
            _CCCFKVCacheObjectType cacheObjectType = (_CCCFKVCacheObjectType)(cacheObject.cacheObjectType & 0XFFFF);

            if (cacheObjectType == _CCCFKVCacheObjectTypeDataRange) {
                
                CCCodeData *plainContentData = ctx->_sharedPtrPlainContentData.get();
                
                decodeObject = decodeObjectFromBuffer(plainContentData->bytes() + cacheObject.dataRange.location, cacheObject.dataRange.length, NULL, NULL, NULL);
            }
            else if (cacheObjectType == _CCCFKVCacheObjectTypeUncodedObject) {
                decodeObject = [cacheObject cacheObject];
            }
            else if (cacheObjectType == _CCCFKVCacheObjectTypeEncodedData) {
                decodeObject =  decodeObjectFromData([cacheObject objectEncodedData]);
            }
            cacheObject.decodeObject = decodeObject;
        }
    });
    
    return decodeObject;
}

float CCCFKV::getFloatForKey(id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return 0;
    }
    
    __block int64_t CTypeVal = 0;
    _sync_lock(ctx->_lock, ^{
        _CCCFKVCacheObject *cacheObject = [ctx->_dict objectForKey:key];
        
        if ([cacheObject haveCTypeValue]) {
            CTypeVal = cacheObject.CTypeValue;
        }
        else {
            id decodeObject = cacheObject.decodeObject;
            if (decodeObject == nil) {
                CCCodeItemType codeType = CCCodeItemTypeRealF;
                if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeDataRange) {

                    CCCodeData *plainContentData = ctx->_sharedPtrPlainContentData.get();

                    decodeObject = decodeObjectFromBuffer(plainContentData->bytes() + cacheObject.dataRange.location, cacheObject.dataRange.length, NULL, &codeType, NULL);
                }
                else if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeUncodedObject) {
                    decodeObject = [cacheObject cacheObject];
                }
                else if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeEncodedData) {
                    NSData *data = [cacheObject objectEncodedData];
                    decodeObject = decodeObjectFromBuffer((uint8_t*)data.bytes, data.length, NULL, &codeType, NULL);
                }
                
                if (codeType == CCCodeItemTypeRealF) {
                    float fval = [decodeObject floatValue];
                    CTypeVal = Int32FromFloat(fval);
                    
                    cacheObject.decodeObject = decodeObject;
                    [cacheObject addCTypeValue:CTypeVal CTypeItemType:codeType];
                }
            }
        }
    });
    
    return FloatFromInt64(CTypeVal);
}

double CCCFKV::getDoubleForKey(id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return 0;
    }
    
    __block int64_t CTypeVal = 0;
    _sync_lock(ctx->_lock, ^{
        _CCCFKVCacheObject *cacheObject = [ctx->_dict objectForKey:key];
        
        if ([cacheObject haveCTypeValue]) {
            CTypeVal = cacheObject.CTypeValue;
        }
        else {
            id decodeObject = cacheObject.decodeObject;
            if (decodeObject == nil) {
                CCCodeItemType codeType = CCCodeItemTypeReal;
                if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeDataRange) {

                    CCCodeData *plainContentData = ctx->_sharedPtrPlainContentData.get();
                    
                    decodeObject = decodeObjectFromBuffer(plainContentData->bytes() + cacheObject.dataRange.location, cacheObject.dataRange.length, NULL, &codeType, NULL);
                }
                else if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeUncodedObject) {
                    decodeObject = [cacheObject cacheObject];
                }
                else if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeEncodedData) {
                    NSData *data = [cacheObject objectEncodedData];
                    decodeObject = decodeObjectFromBuffer((uint8_t*)data.bytes, data.length, NULL, &codeType, NULL);
                }
                
                if (codeType == CCCodeItemTypeReal) {
                    double dval = [decodeObject doubleValue];
                    CTypeVal = Int64FromDouble(dval);
                    
                    cacheObject.decodeObject = decodeObject;
                    [cacheObject addCTypeValue:CTypeVal CTypeItemType:codeType];
                }
            }
        }
    });

    return DoubleFromInt64(CTypeVal);
}

int64_t CCCFKV::getIntegerForKey(id key)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO || key == nil) {
        return 0;
    }
    
    __block int64_t CTypeVal = 0;
    _sync_lock(ctx->_lock, ^{
        _CCCFKVCacheObject *cacheObject = [ctx->_dict objectForKey:key];
        
        if ([cacheObject haveCTypeValue]) {
            CTypeVal = cacheObject.CTypeValue;
        }
        else {
            id decodeObject = cacheObject.decodeObject;
            if (decodeObject == nil) {
                CCCodeItemType codeType = CCCodeItemTypeInteger;
                if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeDataRange) {
                    
                    CCCodeData *plainContentData = ctx->_sharedPtrPlainContentData.get();
                    
                    decodeObject = decodeObjectFromBuffer(plainContentData->bytes() + cacheObject.dataRange.location, cacheObject.dataRange.length, NULL, &codeType, NULL);
                }
                else if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeUncodedObject) {
                    decodeObject = [cacheObject cacheObject];
                }
                else if (cacheObject.cacheObjectType == _CCCFKVCacheObjectTypeEncodedData) {
                    NSData *data = [cacheObject objectEncodedData];
                    decodeObject = decodeObjectFromBuffer((uint8_t*)data.bytes, data.length, NULL, &codeType, NULL);
                }
                
                if (codeType == CCCodeItemTypeInteger) {
                    CTypeVal = [decodeObject longLongValue];
                    
                    cacheObject.decodeObject = decodeObject;
                    [cacheObject addCTypeValue:CTypeVal CTypeItemType:codeType];
                }
            }
        }
    });
    
    return CTypeVal;
}

NSDictionary *CCCFKV::getAllEntries()
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO) {
        return nil;
    }
    
    __block NSDictionary *dict = nil;
    _sync_lock(ctx->_lock, ^{
        dict = [ctx->_dict copy];
    });
    return dict;
}

void CCCFKV::removeObjectForKey(id key)
{
    setObjectForKey(nil, NULL, key);
}

CCCFKVError CCCFKV::getLastError()
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    return ctx->lastError;
}

void CCCFKV::clear(BOOL truncateFileSize)
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO) {
        return;
    }
    _sync_lock(ctx->_lock, ^{
        _clearEncodeData(ctx, truncateFileSize);
    });
}

void CCCFKV::close()
{
    struct CCCFKVContext *ctx = (struct CCCFKVContext *)ptrCFKVContext;
    if (CCCFKV_IS_FILE_OPEN(ctx) == NO) {
        return;
    }
    _sync_lock(ctx->_lock, ^{
        _closeFile(ctx);
    });
}
