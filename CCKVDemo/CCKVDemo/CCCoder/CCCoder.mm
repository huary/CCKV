//
//  CCCoderC.m
//  CCKVDemo
//
//  Created by yuan on 2019/9/6.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "CCCoder.h"
#import "CCMacro.h"
#import <objc/runTime.h>
#import "NSObject+CCCodeToTopSuperClass.h"


#define BYTE_CNT_FIELD_LEN      (4)
#define BYTE_CNT_FIELD_MASK     (TYPE_LS(1,BYTE_CNT_FIELD_LEN) - 1)

#define TYPE_FIELD_OFFSET       BYTE_CNT_FIELD_LEN
#define TYPE_FIELD_LEN          (7 - BYTE_CNT_FIELD_LEN)
#define TYPE_FIELD_MASK         (TYPE_LS(1, TYPE_FIELD_LEN) - 1)

#define INTEGER_TO_ZIGZAG(N)    (((N) << 1) ^ ((N) >> 63))
#define ZIGZAG_TO_INTEGER(N)    ((((uint64_t)N) >> 1 ) ^ (-((N) & 1)))


#define CC_CODER_ESTIMATE_RESERVED_BYTE_CNT        (4)
#define CC_CODER_ESTIMATE_RESERVED_MUCH_NOT_MOVE   (0)



#define _SUPPRESS_AVAILABILITY_BEGIN        _Pragma("clang diagnostic push") \
                                            _Pragma("clang diagnostic ignored \"-Wunsupported-availability-guard\"")\
                                            _Pragma("clang diagnostic ignored \"-Wunguarded-availability-new\"")

#define _SUPPRESS_AVAILABILITY_END           _Pragma("clang diagnostic pop")

#define _AVAILABLE_GUARD(platform, os, future, conditions, IfAvailable, IfUnavailable) \
                                             _SUPPRESS_AVAILABILITY_BEGIN \
                                             if (__builtin_available(platform os, future) && conditions) {\
                                                 _SUPPRESS_AVAILABILITY_END \
                                                 if (@available(platform os, future)) { \
                                                     IfAvailable \
                                                 } \
                                                 else { \
                                                     IfUnavailable \
                                                 } \
                                             } \
                                             else { \
                                                 _SUPPRESS_AVAILABILITY_END \
                                                 IfUnavailable \
                                             } \


#define _IOS_AVAILABLE_GUARD(os, conditions, IfAvailable, IfUnavailable)     \
_AVAILABLE_GUARD(iOS, os, *, conditions, IfAvailable, IfUnavailable)



NSString *const _CCCoderErrorDomain = @"CCCoderErrorDomain";

typedef void(^_CCCoderEstimateReservedEncodeBlock)(CCMutableCodeData *codeData);


template <typename F, typename T>
union Converter {
    static_assert(sizeof(F) == sizeof(T),"size not match");
    F from;
    T to;
    
public:
    void decodeFromBuffer(uint8_t *ptr) {
        if (ptr == NULL) return;
        uint8_t len = sizeof(from);
        to = 0;
        for (uint8_t i = 0; i < len; ++i) {
            T v = ptr[i];
            to |= TYPE_LS(v, TYPE_LS(i, 3));
        }
    }
    void decodeFromData(NSData *data) {
        decodeFromBuffer(data.bytes);
    }
    
    void encodeToBuffer(uint8_t *buffer) {
        if (buffer == NULL) return;
        uint8_t len = sizeof(from);
        for (uint8_t i = 0; i < len; ++i) {
            buffer[i] = TYPE_AND(TYPE_RS(to, TYPE_LS(i, 3)), 0XFF);
        }
    }
    NSData *encodeToData() {
        uint8_t len = sizeof(from);
        NSMutableData *dt = [NSMutableData dataWithLength:len];
        encodeToBuffer((uint8_t*)dt.bytes);
        return dt;
    }
    
};

static inline void _encodeObject(id object, Class topSuperClass, CCMutableCodeData *codeData, NSError **error);

BOOL objectIsKindOfClass(id object, Class cls)
{
//    return [object isKindOfClass:cls];
    Class objCls = [object class];
    while (objCls) {
        objCls = class_getSuperclass(objCls);
        if (objCls == cls) {
            return YES;
        }
    }
    return NO;
}

CCCodeItemType objectCodeType(id object)
{
    Class objCls = [object class];
    Class numberCls = [NSNumber class];
    Class stringCls = [NSString class];
    Class dataCls = [NSData class];
    Class arrayCls = [NSArray class];
    Class dictCls = [NSDictionary class];
    Class objectCls = [NSObject class];
    CCCodeItemType codeType = CCCodeItemTypeMax;
    while (objCls) {
        if (objCls == numberCls) {
            NSNumber *num = object;
            int8_t type = num.objCType ? num.objCType[0] : 0;
            if (type == 'd') {
                return CCCodeItemTypeReal;
            }
            else if (type == 'f') {
                return CCCodeItemTypeRealF;
            }
            else {
                return CCCodeItemTypeInteger;
            }
        }
        else if (objCls == stringCls) {
            return CCCodeItemTypeText;
        }
        else if (objCls == dataCls) {
            return CCCodeItemTypeBlob;
        }
        else if (objCls == arrayCls) {
            return CCCodeItemTypeArray;
        }
        else if (objCls == dictCls) {
            return CCCodeItemTypeDictionary;
        }
        else if (objCls == objectCls) {
            return CCCodeItemTypeObject;
        }
        else {
            objCls = class_getSuperclass(objCls);
        }
    }
    return codeType;
}

BOOL classIsSubclassOfClass(Class sub, Class cls)
{
    while (sub) {
        sub = class_getSuperclass(sub);
        if (sub == cls) {
            return YES;
        }
    }
    return NO;
}

static inline int64_t integerToZigzag(int64_t n)
{
    return (n << 1 ) ^ (n >> 63);
}

static inline int64_t zigzagToInteger(int64_t n)
{
    return (((uint64_t)n) >> 1 ) ^ (-(n & 1));
}

//static inline NSData* _encodeInt64(int64_t val)
//{
//    uint8_t len = TYPEULL_BYTES_N(val);
//    NSMutableData *dt = [NSMutableData dataWithLength:len];
//    uint8_t *ptr = (uint8_t*)dt.mutableBytes;
//    for (uint8_t i = 0; i < len; ++i) {
//        ptr[i] = TYPE_AND(TYPE_RS(val, TYPE_LS(i, 3)), 0XFF);
//    }
//    return dt;//[dt copy];
//}

static inline uint8_t _encodeInt64ToBuffer(int64_t val, uint8_t* buffer)
{
    if (buffer == NULL) {
        return 0;
    }
    uint8_t len = TYPEULL_BYTES_N(val);
    for (uint8_t i = 0; i < len; ++i) {
        buffer[i] = TYPE_AND(TYPE_RS(val, TYPE_LS(i, 3)), 0XFF);
    }
    return len;
}

static inline int64_t _decodeInt64(uint8_t *ptr, uint8_t len)
{
    int64_t val = 0;
    for (uint8_t i = 0; i < len; ++i) {
        int64_t v = ptr[i];
        val |= TYPE_LS(v, TYPE_LS(i, 3));
    }
    return val;
}

//static inline NSData* _encodeDouble(double val)
//{
//    Converter<double, uint64_t> converter;
//    converter.from = val;
//    return converter.encodeToData();
//}

static inline void _encodeDoubleToBuffer(double val, uint8_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    Converter<double, uint64_t> converter;
    converter.from = val;
    converter.encodeToBuffer(buffer);
}

static inline double _decodeDouble(uint8_t *ptr)
{
    Converter<double, uint64_t> converter;
    converter.decodeFromBuffer(ptr);
    return converter.from;
}


//static inline NSData* _encodeFloat(float val)
//{
//    Converter<float, uint32_t> converter;
//    converter.from = val;
//    return converter.encodeToData();
//}

static inline void _encodeFloatToBuffer(float val, uint8_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    Converter<float, uint32_t> converter;
    converter.from = val;
    converter.encodeToBuffer(buffer);
}

static inline float _decodeFloat(uint8_t *ptr)
{
    Converter<float, uint32_t> converter;
    converter.decodeFromBuffer(ptr);
    return converter.from;
}


static inline NSData* _archivedDataWithObject(id object)
{
    NSData *data = nil;
    if ([object conformsToProtocol:@protocol(NSCoding)]) {
        if ([NSKeyedArchiver respondsToSelector:@selector(archivedDataWithRootObject:requiringSecureCoding:error:)]) {
            NSError *error = nil;
            
            _IOS_AVAILABLE_GUARD(11.0, YES, {
                data = [NSKeyedArchiver archivedDataWithRootObject:object requiringSecureCoding:NO error:&error];
            }, {});
            
//            NSLog(@"error=%@",error);
        }
        if (data == nil && [NSKeyedArchiver respondsToSelector:@selector(archivedDataWithRootObject:)]) {
            data = [NSKeyedArchiver archivedDataWithRootObject:object];
        }
    }
    return data;
}

static inline id _unarchiveObjectWithData(NSData *data, Class cls)
{
    if (data == nil) {
        return nil;
    }
    id object = [cls new];
    if ([object conformsToProtocol:@protocol(NSCoding)]) {
        if ([NSKeyedUnarchiver respondsToSelector:@selector(unarchivedObjectOfClass:fromData:error:)]) {
            NSError *error = nil;
            _IOS_AVAILABLE_GUARD(11.0, YES, {
                object = [NSKeyedUnarchiver unarchivedObjectOfClass:cls fromData:data error:&error];
            }, {});
//            NSLog(@"error=%@",error);
        }
        
        if (object == nil && [NSKeyedUnarchiver respondsToSelector:@selector(unarchiveObjectWithData:)]) {
            object = [NSKeyedUnarchiver unarchiveObjectWithData:data];
        }
    }
    return object;
}

static inline NSArray<NSString*>* _propertiesForClass(Class cls)
{
    uint32_t count = 0;
    objc_property_t *properties = class_copyPropertyList(cls, &count);
    NSMutableArray *list = [NSMutableArray new];
    for (uint32_t i = 0; i < count; ++i) {
        @autoreleasepool {
            objc_property_t property = properties[i];
            const char *nameStr = property_getName(property);
            NSString *name = [NSString stringWithUTF8String:nameStr];
            
            const char *attr = property_getAttributes(property);
            NSString *attrName = [NSString stringWithUTF8String:attr];
            
            //只保护property的，分类动态联合的不进行编码
            NSString *contains = [NSString stringWithFormat:@"V_%@",name];
            if ([attrName containsString:contains]) {
                [list addObject:name];
            }
        }
    }
    if (properties) {
        free(properties);
    }
    return list;//[list copy];
}

static inline void _mergeEncodeObjectWithKey(id object, Class topSuperClass, id key, CCMutableCodeData *codeData)
{
    int64_t start = codeData->currentSeek();
    _encodeObject(key, NULL, codeData, NULL);
    int64_t keyOffset = codeData->currentSeek();
    if (keyOffset - start <= 0) {
        codeData->truncateToWithSeek(start, CCDataSeekTypeEND);
        return;
    }
    _encodeObject(object, topSuperClass, codeData, NULL);
    int64_t objectOffset = codeData->currentSeek();
    if (objectOffset - keyOffset <= 0) {
        codeData->truncateToWithSeek(start, CCDataSeekTypeEND);
    }
}

/*
 *这个是不对参数object、from、topEdgeSupperClass检查
 */
static inline void _recursionObjectWithoutCheck(id object, Class from, Class topSuperClass, CCMutableCodeData *codeData)
{
    if (codeData == nil) {
        return;
    }
    if (topSuperClass && [from isEqual:topSuperClass] == NO) {
        _recursionObjectWithoutCheck(object, [from superclass], topSuperClass, codeData);
    }
    
    NSArray *codeKeyPaths = nil;
    if ([from respondsToSelector:@selector(cc_objectCodeKeyPaths)]) {
        codeKeyPaths = [from cc_objectCodeKeyPaths];
    }
    else {
        codeKeyPaths = _propertiesForClass(from);
    }
    
    [codeKeyPaths enumerateObjectsUsingBlock:^(id  _Nonnull key, NSUInteger idx, BOOL * _Nonnull stop) {
        if ([object respondsToSelector:NSSelectorFromString(key)]) {
            id propertyValue = [object valueForKeyPath:key];
            
            _mergeEncodeObjectWithKey(propertyValue, [propertyValue cc_codeToTopSuperClass], key, codeData);
        }
    }];
}

static inline void _encodeObjectFromClassToTopSuperClassIntoCodeData(id object, Class from, Class topSuperClass,CCMutableCodeData *codeData)
{
    if (!object) {
        return;
    }
    
    if (from == NULL || objectIsKindOfClass(object, from) == NO) {
        from = object_getClass(object);
    }
    
    if (topSuperClass == NULL) {
        topSuperClass = from;
    }
    else {
        if (objectIsKindOfClass(object, topSuperClass) == NO ||
            classIsSubclassOfClass(from, topSuperClass) == NO ) {
            topSuperClass = NULL;
        }
    }
    _recursionObjectWithoutCheck(object, from, topSuperClass, codeData);
}

static inline void _packetHeader(int64_t payloadLength, CCCodeItemType codeType,CCMutableCodeData *codeData)
{
    uint8_t type = TYPE_LS(1, 7);
    int64_t len = payloadLength;
    uint8_t cnt = TYPEULL_BYTES_N(len);
    BOOL haveLoadLength = YES;
    if (codeType == CCCodeItemTypeReal || codeType == CCCodeItemTypeRealF || codeType == CCCodeItemTypeInteger) {
        cnt = len;
        haveLoadLength = NO;
    }
    type |= TYPE_AND(cnt - 1, BYTE_CNT_FIELD_MASK);
    type |= TYPE_LS(TYPE_AND(codeType, TYPE_FIELD_MASK), TYPE_FIELD_OFFSET);
    
    codeData->appendWriteByte(type);
    
    if (haveLoadLength) {
        codeData->ensureRemSize(cnt);
        int64_t startOffset = codeData->currentSeek();
        _encodeInt64ToBuffer(len, codeData->bytes() + startOffset);
        codeData->seekTo(startOffset + cnt);
    }
}


static inline NSData* _unpackData(NSData *data, CCCodeItemType *codeType, int8_t *len, int64_t *size, int64_t *offset)
{
    NSRange r = unpackBuffer((uint8_t*)data.bytes, data.length, codeType, len, size, offset);
    if (r.location == NSNotFound) {
        return nil;
    }
    return [data subdataWithRange:r];
}

static inline void _estimateReservedEncodeObjectWithCodeType(CCCodeItemType codeType, CCMutableCodeData *codeData, _CCCoderEstimateReservedEncodeBlock encodeBlock)
{
    int64_t oldSize = codeData->currentSeek();
    codeData->ensureRemSize(CC_CODER_ESTIMATE_RESERVED_BYTE_CNT);
    int64_t startOffset = oldSize + CC_CODER_ESTIMATE_RESERVED_BYTE_CNT;
    codeData->seekTo(startOffset);

    if (encodeBlock) {
        encodeBlock(codeData);
    }

    int64_t endOffset = codeData->currentSeek();
    int64_t addSize = endOffset - startOffset;
    if (addSize > 0) {
        int8_t payloadHeaderSize = TYPEULL_BYTES_N(addSize) + 1;
        int8_t shift = payloadHeaderSize - CC_CODER_ESTIMATE_RESERVED_BYTE_CNT;
        if (shift != 0) {
            if (shift > 0) {
                codeData->ensureRemSize(shift);
            }
            memmove(codeData->bytes() + oldSize + payloadHeaderSize, codeData->bytes() + startOffset, (size_t)addSize);
        }
        codeData->seekTo(oldSize);
        _packetHeader(addSize, codeType, codeData);
        codeData->truncateToWithSeek(oldSize + payloadHeaderSize + addSize, CCDataSeekTypeEND);
    }
    else {
        codeData->truncateToWithSeek(oldSize, CCDataSeekTypeEND);
    }
}


static inline void _encodeObject(id object, Class topSuperClass, CCMutableCodeData *codeData, NSError **error)
{
    if (object == nil || codeData == nullptr) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return;
    }
    
    Class objCls = [object class];
    Class numberCls = [NSNumber class];
    Class stringCls = [NSString class];
    Class dataCls = [NSData class];
    Class arrayCls = [NSArray class];
    Class dictCls = [NSDictionary class];
    Class objectCls = [NSObject class];
    while (objCls) {
        if (objCls == numberCls) {
            NSNumber *num = object;
            int8_t type = num.objCType ? num.objCType[0] : 0;
            if (type == 'd') {
                double val = [num doubleValue];
                encodeDoubleIntoCodeData(val, codeData);
            }
            else if (type == 'f') {
                float val = [num floatValue];
                encodeFloatIntoCodeData(val, codeData);
            }
            else {
                int64_t val = [num longLongValue];
                encodeIntegerIntoCodeData(val, codeData);
            }
            return;
        }
        else if (objCls == stringCls) {
            NSString *text = object;
            encodeStringIntoCodeData(text, codeData);
            return;
        }
        else if (objCls == dataCls) {
            NSData *data = object;
            encodeDataIntoCodeData(data, codeData);
            return;
        }
        else if (objCls == arrayCls) {
            NSArray *array = object;
            
            _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeArray, codeData, ^(CCMutableCodeData *codeData) {
                [array enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
                    encodeObjectIntoCodeData(obj, codeData, NULL);
                }];
            });
            return;
        }
        else if (objCls == dictCls) {
            NSDictionary *dict = object;
            
            _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeDictionary, codeData, ^(CCMutableCodeData *codeData) {
                [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
                    
                    _mergeEncodeObjectWithKey(obj, [obj cc_codeToTopSuperClass], key, codeData);
                }];
            });
            return;
        }
        else if (objCls == objectCls) {
            _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeObject, codeData, ^(CCMutableCodeData *codeData) {
                
                uint64_t startOffset = codeData->currentSeek();
                
                Class cls = object_getClass(object);
                NSString *clsName = [NSString stringWithUTF8String:class_getName(cls)];
                encodeStringIntoCodeData(clsName, codeData);
                
                uint64_t startObjOffset = codeData->currentSeek();
                
                _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeBlob, codeData, ^(CCMutableCodeData *codeData) {
                    if ([object conformsToProtocol:@protocol(NSCoding)]) {
                        NSData *objDt = _archivedDataWithObject(object);
                        codeData->appendWriteData(objDt);
                    }
                    else {
                        _encodeObjectFromClassToTopSuperClassIntoCodeData(object, object_getClass(object), topSuperClass, codeData);
                    }
                });
                
                int64_t endObjOffset = codeData->currentSeek();
                if (endObjOffset - startObjOffset <= 0) {
                    codeData->truncateToWithSeek(startOffset, CCDataSeekTypeEND);
                }
            });
            return;
        }
        else {
            objCls = class_getSuperclass(objCls);
        }
    }
    if (*error) {
        *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
    }
    
//    if (objectIsKindOfClass(object, [NSNumber class])) {
//        NSNumber *num = object;
//        int8_t type = num.objCType ? num.objCType[0] : 0;
//        if (type == 'd') {
//            double val = [num doubleValue];
//            encodeDoubleIntoCodeData(val, codeData);
//        }
//        else if (type == 'f') {
//            float val = [num floatValue];
//            encodeFloatIntoCodeData(val, codeData);
//        }
//        else {
//            int64_t val = [num longLongValue];
//            encodeIntegerIntoCodeData(val, codeData);
//        }
//    }
//    else if (objectIsKindOfClass(object, [NSString class])) {
//        NSString *text = object;
//        encodeStringIntoCodeData(text, codeData);
//    }
//    else if (objectIsKindOfClass(object, [NSData class])) {
//        NSData *data = object;
//        encodeDataIntoCodeData(data, codeData);
//    }
//    else if (objectIsKindOfClass(object, [NSArray class])) {
//        NSArray *array = object;
//        
//        _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeArray, codeData, ^(CCMutableCodeData *codeData) {
//            [array enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
//                encodeObjectIntoCodeData(obj, codeData, NULL);
//            }];
//        });
//    }
//    else if (objectIsKindOfClass(object, [NSDictionary class])) {
//        NSDictionary *dict = object;
//        
//        _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeDictionary, codeData, ^(CCMutableCodeData *codeData) {
//            [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
//                
//                _mergeEncodeObjectWithKey(obj, [obj cc_codeToTopSuperClass], key, codeData);
//            }];
//        });
//    }
//    else if (objectIsKindOfClass(object, [NSObject class])) {
//        _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeObject, codeData, ^(CCMutableCodeData *codeData) {
//            
//            uint64_t startOffset = codeData->currentSeek();
//            
//            Class cls = object_getClass(object);
//            NSString *clsName = [NSString stringWithUTF8String:class_getName(cls)];
//            encodeStringIntoCodeData(clsName, codeData);
//            
//            uint64_t startObjOffset = codeData->currentSeek();
//            
//            _estimateReservedEncodeObjectWithCodeType(CCCodeItemTypeBlob, codeData, ^(CCMutableCodeData *codeData) {
//                if ([object conformsToProtocol:@protocol(NSCoding)]) {
//                    NSData *objDt = _archivedDataWithObject(object);
//                    codeData->appendWriteData(objDt);
//                }
//                else {
//                    _encodeObjectFromClassToTopSuperClassIntoCodeData(object, object_getClass(object), topSuperClass, codeData);
//                }
//            });
//            
//            int64_t endObjOffset = codeData->currentSeek();
//            if (endObjOffset - startObjOffset <= 0) {
//                codeData->truncateToWithSeek(startOffset, CCDataSeekTypeEND);
//            }
//        });
//    }
//    else {
//        if (*error) {
//            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
//        }
//    }
}

NSData *_encodeObjectToTopSuperClass(id object, Class topSuperClass, NSError **error)
{
    CCMutableCodeData codeData;
    _encodeObject(object, topSuperClass, &codeData, error);
    return codeData.copyData();
}

NSData *encodeObject(id object)
{
    Class topSuperClass = [object cc_codeToTopSuperClass];
    return _encodeObjectToTopSuperClass(object, topSuperClass, NULL);
}

NSData *encodeObjectToTopSuperClass(id object, Class topSuperClass, NSError **error)
{
    return _encodeObjectToTopSuperClass(object, topSuperClass, error);
}

void encodeObjectIntoCodeData(id object, CCMutableCodeData *codeData, NSError **error)
{
    Class topSuperClass = [object cc_codeToTopSuperClass];
    _encodeObject(object, topSuperClass, codeData, error);
}

void encodeObjectToTopSuperClassIntoCodeData(id object, Class topSuperClass, CCMutableCodeData *codeData, NSError **error)
{
    _encodeObject(object, topSuperClass, codeData, error);
}

void encodeFloatIntoCodeData(float val, CCMutableCodeData *codeData)
{
    if (codeData == NULL) {
        return;
    }
    
    uint8_t type = TYPE_LS(1, 7);
    uint8_t cnt = sizeof(val);
    CCCodeItemType codeType = CCCodeItemTypeRealF;
    
    type |= TYPE_AND(cnt - 1, BYTE_CNT_FIELD_MASK);
    type |= TYPE_LS(TYPE_AND(codeType, TYPE_FIELD_MASK), TYPE_FIELD_OFFSET);
    
    codeData->appendWriteByte(type);
    
    codeData->ensureRemSize(cnt);
    
    int64_t startOffset = codeData->currentSeek();
    _encodeFloatToBuffer(val, codeData->bytes() + startOffset);
    codeData->seekTo(startOffset + cnt);
}

void encodeDoubleIntoCodeData(double val, CCMutableCodeData *codeData)
{
    if (codeData == NULL) {
        return;
    }
    uint8_t type = TYPE_LS(1, 7);
    uint8_t cnt = sizeof(val);
    CCCodeItemType codeType = CCCodeItemTypeReal;
    
    type |= TYPE_AND(cnt - 1, BYTE_CNT_FIELD_MASK);
    type |= TYPE_LS(TYPE_AND(codeType, TYPE_FIELD_MASK), TYPE_FIELD_OFFSET);
    
    codeData->appendWriteByte(type);
    
    codeData->ensureRemSize(cnt);
    
    int64_t startOffset = codeData->currentSeek();
    _encodeDoubleToBuffer(val, codeData->bytes() + startOffset);
    codeData->seekTo(startOffset + cnt);
}

//可以8、U8,16,U16,32,U32,64,U64
void encodeIntegerIntoCodeData(int64_t val, CCMutableCodeData *codeData)
{
    if (codeData == NULL) {
        return;
    }
    val = integerToZigzag(val);
    uint8_t type = TYPE_LS(1, 7);
    uint8_t cnt = TYPEULL_BYTES_N(val);
    CCCodeItemType codeType = CCCodeItemTypeInteger;
    
    type |= TYPE_AND(cnt - 1, BYTE_CNT_FIELD_MASK);
    type |= TYPE_LS(TYPE_AND(codeType, TYPE_FIELD_MASK), TYPE_FIELD_OFFSET);

    codeData->appendWriteByte(type);
    
    codeData->ensureRemSize(cnt);
    
    int64_t startOffset = codeData->currentSeek();
    _encodeInt64ToBuffer(val, codeData->bytes() + startOffset);
    codeData->seekTo(startOffset + cnt);
}

void encodeStringIntoCodeData(NSString *text, CCMutableCodeData *codeData)
{
    if (text == nil || codeData == NULL) {
        return;
    }
    uint8_t type = TYPE_LS(1, 7);
    int64_t len = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    uint8_t cnt = TYPEULL_BYTES_N(len);
    CCCodeItemType codeType = CCCodeItemTypeText;
    
    type |= TYPE_AND(cnt - 1, BYTE_CNT_FIELD_MASK);
    type |= TYPE_LS(TYPE_AND(codeType, TYPE_FIELD_MASK), TYPE_FIELD_OFFSET);
    
    codeData->appendWriteByte(type);
    
    codeData->ensureRemSize(cnt + len);
    
    int64_t startOffset = codeData->currentSeek();
    _encodeInt64ToBuffer(len, codeData->bytes() + startOffset);
    codeData->seekTo(startOffset + cnt);
    
    [text getBytes:codeData->bytes() + startOffset + cnt
         maxLength:(NSUInteger)len
        usedLength:0
          encoding:NSUTF8StringEncoding
           options:0
             range:NSMakeRange(0, text.length)
    remainingRange:NULL];
    
    codeData->seekTo(startOffset + cnt + len);
}

void encodeDataIntoCodeData(NSData *data, CCMutableCodeData *codeData)
{
    if (data == nil || codeData == NULL) {
        return;
    }
    uint8_t type = TYPE_LS(1, 7);
    int64_t len = data.length;
    uint8_t cnt = TYPEULL_BYTES_N(len);
    CCCodeItemType codeType = CCCodeItemTypeBlob;
    
    type |= TYPE_AND(cnt - 1, BYTE_CNT_FIELD_MASK);
    type |= TYPE_LS(TYPE_AND(codeType, TYPE_FIELD_MASK), TYPE_FIELD_OFFSET);
    
    codeData->appendWriteByte(type);
    
    codeData->ensureRemSize(cnt + len);
    
    int64_t startOffset = codeData->currentSeek();
    _encodeInt64ToBuffer(len, codeData->bytes() + startOffset);
    codeData->seekTo(startOffset + cnt);
    
    codeData->writeData(data);
}

#pragma mark decode
id decodeObjectFromData(NSData *data)
{
    id obj = decodeObjectFromBuffer((uint8_t*)data.bytes, data.length, NULL, NULL, NULL);
    return obj;
}

id decodeObjectFromBuffer(uint8_t *buffer ,int64_t length, int64_t *offset, CCCodeItemType *codeType, NSError **error)
{
    if (buffer == nil || length <= 0) {
        if (offset) {
            *offset = 0;
        }
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return nil;
    }
    
    int8_t len = 0;
    int64_t size = 0;
    CCCodeItemType codeTypeTmp;
    NSRange r = unpackBuffer(buffer, length, &codeTypeTmp, &len, &size, offset);
    if (r.location == NSNotFound) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorNotFound userInfo:nil];
        }
        return nil;
    }
    if (codeType) {
        *codeType = codeTypeTmp;
    }
    uint8_t *ptr = buffer + r.location;
    
    switch (codeTypeTmp) {
        case CCCodeItemTypeReal: {
            double val =  _decodeDouble(ptr);
            return [NSNumber numberWithDouble:val];
        }
        case CCCodeItemTypeRealF: {
            float val = _decodeFloat(ptr);
            return [NSNumber numberWithFloat:val];
        }
        case CCCodeItemTypeInteger: {
            int64_t val = size;
            val = zigzagToInteger(val);
            return @(val);
        }
        case CCCodeItemTypeText: {
            NSString *text = [[NSString alloc] initWithBytes:ptr
                                                      length:(NSUInteger)size
                                                    encoding:NSUTF8StringEncoding];
            return text;
        }
        case CCCodeItemTypeBlob: {
            return [NSData dataWithBytes:ptr length:r.length];
        }
        case CCCodeItemTypeArray: {
            NSMutableArray *array = [NSMutableArray array];
            while (size > 0) {
                int64_t offsetTmp = 0;
                id obj = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, error);
                if (obj) {
                    [array addObject:obj];
                }
                if (offsetTmp == 0) {
                    break;
                }
                ptr += offsetTmp;
                size -= offsetTmp;
            }
            return array;
        }
        case CCCodeItemTypeDictionary: {
            NSMutableDictionary *dict = [NSMutableDictionary dictionary];
            while (size > 0) {
                int64_t offsetTmp = 0;
                id key = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, error);
                ptr += offsetTmp;
                size -= offsetTmp;
                //这里key有可能为nil,但是offsetTmp不能为0
                if (size <= 0 || offsetTmp <= 0/*key == nil*/) {
                    break;
                }
                
                id obj = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, error);
                ptr += offsetTmp;
                size -= offsetTmp;
                if (key && obj) {
                    [dict setObject:obj forKey:key];
                }
                if (offsetTmp <= 0) {
                    break;
                }
            }
            return dict;
        }
        case CCCodeItemTypeObject: {
            int64_t offsetTmp = 0;
            NSString *clsName = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, error);
            ptr += offsetTmp;
            size -= offsetTmp;
            if (size <= 0) {
                if (error) {
                    *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorDataError userInfo:nil];
                }
                return nil;
            }
            Class cls = NSClassFromString(clsName);
            //这里的cls可能为nil,申请的object为nil时判断
            id object = [cls new];
            if (object == nil) {
                if (error) {
                    *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorClassError userInfo:nil];
                }
                return object;
            }
            
            NSData *data = decodeObjectFromBuffer(ptr, size, NULL, NULL, error);
            ptr = (uint8_t*)data.bytes;
            size = data.length;
            if ([object conformsToProtocol:@protocol(NSCoding)]) {
                object = _unarchiveObjectWithData(data, cls);
            }
            else {
                while (size > 0) {
                    int64_t offsetTmp = 0;
                    id key = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, error);
                    ptr += offsetTmp;
                    size -= offsetTmp;
                    //key可能为nil，但是offsetTmp必须大于0
                    if (size <= 0 || offsetTmp <= 0) {
                        break;
                    }
                    
                    id obj = decodeObjectFromBuffer(ptr, size, &offsetTmp, NULL, error);
                    ptr += offsetTmp;
                    size -= offsetTmp;
                    if (key) {
                        if ([object respondsToSelector:NSSelectorFromString(key)]) {
                            [object setValue:obj forKey:key];
                        }
                    }
                    if (offsetTmp <= 0) {
                        break;
                    }
                }
            }
            return object;
        }
        default:
            break;
    }
    return nil;
}

float decodeFloatFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset ,NSError **error)
{
    if (buffer == nil || length <= 0) {
        if (offset) {
            *offset = 0;
        }
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return 0;
    }
    
    int8_t len = 0;
    int64_t size = 0;
    CCCodeItemType codeType;
    NSRange r = unpackBuffer(buffer, length, &codeType, &len, &size, offset);
    if (r.location == NSNotFound) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorNotFound userInfo:nil];
        }
        return 0;
    }
    if (codeType != CCCodeItemTypeRealF) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
        }
        return 0;
    }
    uint8_t *ptr = buffer + r.location;
    float val = _decodeFloat(ptr);
    return val;
}

double decodeDoubleFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset, NSError **error)
{
    if (buffer == nil || length <= 0) {
        if (offset) {
            *offset = 0;
        }
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return 0;
    }
    
    int8_t len = 0;
    int64_t size = 0;
    CCCodeItemType codeType;
    NSRange r = unpackBuffer(buffer, length, &codeType, &len, &size, offset);
    if (r.location == NSNotFound) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorNotFound userInfo:nil];
        }
        return 0;
    }
    if (codeType != CCCodeItemTypeReal) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
        }
        return 0;
    }
    uint8_t *ptr = buffer + r.location;
    double val = _decodeDouble(ptr);
    return val;
}

int64_t decodeIntegerFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset,NSError **error)
{
    if (buffer == nil || length <= 0) {
        if (offset) {
            *offset = 0;
        }
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return 0;
    }
    
    int8_t len = 0;
    int64_t size = 0;
    CCCodeItemType codeType;
    NSRange r = unpackBuffer(buffer, length, &codeType, &len, &size, offset);
    if (r.location == NSNotFound) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorNotFound userInfo:nil];
        }
        return 0;
    }
    if (codeType != CCCodeItemTypeInteger) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
        }
        return 0;
    }
    int64_t val = size;
    val = zigzagToInteger(val);
    return val;
}

NSString *decodeStringFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset,NSError **error)
{
    if (buffer == nil || length <= 0) {
        if (offset) {
            *offset = 0;
        }
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return nil;
    }
    
    int8_t len = 0;
    int64_t size = 0;
    CCCodeItemType codeType;
    NSRange r = unpackBuffer(buffer, length, &codeType, &len, &size, offset);
    if (r.location == NSNotFound) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorNotFound userInfo:nil];
        }
        return nil;
    }
    if (codeType != CCCodeItemTypeText) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
        }
        return nil;
    }
    uint8_t *ptr = buffer + r.location;
    NSString *text = [[NSString alloc] initWithBytes:ptr
                                              length:(NSUInteger)size
                                            encoding:NSUTF8StringEncoding];
    return text;
}

NSData *decodeDataFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset,NSError **error)
{
    if (buffer == nil || length <= 0) {
        if (offset) {
            *offset = 0;
        }
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorPtrNull userInfo:nil];
        }
        return nil;
    }
    
    int8_t len = 0;
    int64_t size = 0;
    CCCodeItemType codeType;
    NSRange r = unpackBuffer(buffer, length, &codeType, &len, &size, offset);
    if (r.location == NSNotFound) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorNotFound userInfo:nil];
        }
        return nil;
    }
    if (codeType != CCCodeItemTypeBlob) {
        if (error) {
            *error = [NSError errorWithDomain:_CCCoderErrorDomain code:CCCoderErrorTypeError userInfo:nil];
        }
        return nil;
    }
    uint8_t *ptr = buffer + r.location;
    
    return [NSData dataWithBytes:ptr length:r.length];
}


NSData* packetData(NSData *data,CCCodeItemType codeType)
{
    CCMutableCodeData codeData;
    packetDataIntoCodeData(data, codeType, &codeData);
    return codeData.copyData();
}

void packetDataIntoCodeData(NSData *data,CCCodeItemType codeType, CCMutableCodeData *codeData)
{
    CCCodeData cdData(data);
    packetCodeData(&cdData, codeType, codeData);
}

void packetCodeData(CCCodeData *data, CCCodeItemType codeType, CCMutableCodeData *codeData)
{
    if (codeData == NULL) {
        return;
    }
    int64_t len = data->dataSize();
    _packetHeader(len, codeType, codeData);
    if (data) {
        codeData->appendWriteBuffer(data->bytes(), len);
    }
}

NSData* unpackData(NSData *data, CCCodeItemType *codeType, int64_t *size, int64_t *offset)
{
    return _unpackData(data, codeType, NULL, size, offset);
}

NSRange unpackBuffer(uint8_t *buffer, int64_t bufferSize, CCCodeItemType *codeType, int8_t *len, int64_t *size, int64_t *offset)
{
    NSRange r = NSMakeRange(NSNotFound, 0);
    if (buffer == nullptr || bufferSize <= 0) {
        return r;
    }
    int64_t length = bufferSize;
    uint8_t *ptr = buffer;
    uint8_t type = ptr[0];
    if (type < TYPE_LS(1, 7)) {
        return r;
    }
    uint8_t lenTmp = TYPE_AND(type, BYTE_CNT_FIELD_MASK) + 1;
    if (len) {
        *len = lenTmp;
    }
    uint8_t codeTypeTmp = TYPE_AND(TYPE_RS(type, TYPE_FIELD_OFFSET), TYPE_FIELD_MASK);
    if (codeType) {
        *codeType = (CCCodeItemType)codeTypeTmp;
    }
    
    if (length < 1 + lenTmp) {
        return r;
    }
    
    ++ptr;
    if (offset) {
        *offset = 1;
    }
    
    uint64_t sizeTmp = _decodeInt64(ptr, lenTmp);
    ptr += lenTmp;
    if (offset) {
        *offset = 1 + lenTmp;
    }
    if (size) {
        *size = sizeTmp;
    }
    if (codeTypeTmp == CCCodeItemTypeReal || codeTypeTmp == CCCodeItemTypeRealF || codeTypeTmp == CCCodeItemTypeInteger) {
        return NSMakeRange(1, lenTmp);
    }
    
    if (length < 1 + lenTmp + sizeTmp) {
        return r;
    }
    
    if (offset) {
        *offset = 1 + lenTmp + sizeTmp;
    }
    return NSMakeRange(NSUInteger(1 + lenTmp), (NSUInteger)sizeTmp);
}

//这些是同字节数（sizeof）的转换
int32_t Int32FromFloat(float val)
{
    Converter<float, int32_t> cvter;
    cvter.from = val;
    return cvter.to;
}

float FloatFromInt32(int32_t val)
{
    Converter<int32_t, float> cvter;
    cvter.from = val;
    return cvter.to;
}

int64_t Int64FromDouble(double val)
{
    Converter<double, int64_t> cvter;
    cvter.from = val;
    return cvter.to;
}

double DoubleFromInt64(int64_t val)
{
    Converter<int64_t, double> cvter;
    cvter.from = val;
    return cvter.to;
}


//将float和Int32进行转换，Int32存在Int64上
int64_t Int64FromFloat(float val)
{
    return Int32FromFloat(val);
}

float FloatFromInt64(int64_t val)
{
    int32_t ival = TYPE_AND(val, 0XFFFFFFFF);
    return FloatFromInt32(ival);
}
