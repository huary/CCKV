//
//  CCCoderC.h
//  CCKVDemo
//
//  Created by yuan on 2019/9/6.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CCCodeData.h"

//最多只能定义16中数字类型
typedef NS_ENUM(NSInteger, CCCodeItemType)
{
    //是一个整数，可以是1-8个字节
    CCCodeItemTypeInteger      = 0,
    //是一个浮点数，存储为 8 字节的浮点数字。
    CCCodeItemTypeReal         = 1,
    //是一个浮点数，存储为 4 字节的浮点数字。
    CCCodeItemTypeRealF        = 2,
    //是一个文本字段
    CCCodeItemTypeText         = 3,
    //是一个二进制数据
    CCCodeItemTypeBlob         = 4,
    //是一个数组[]
    CCCodeItemTypeArray        = 5,
    //是一个字典{}
    CCCodeItemTypeDictionary   = 6,
    //NSObject 或者其子类对象
    CCCodeItemTypeObject       = 7,
    //最大只能到15
    CCCodeItemTypeMax          = 15,
};


typedef NS_ENUM(NSInteger, CCCoderError)
{
    //指针为空或者执行的内容长度为0
    CCCoderErrorPtrNull        = 1,
    //没有找到编码的range
    CCCoderErrorNotFound       = 2,
    //编码类型错误
    CCCoderErrorTypeError      = 3,
    //数据错误
    CCCoderErrorDataError      = 4,
    //编码的class错误
    CCCoderErrorClassError     = 5,
};

/*
 *第1个字节：最高位为1，第4位到第7位为上面的值，第3位到1位为字节数(最大支持到8字节存储的数值：2^64的值)
 *第2-8个字节：存储后面数据长度的数值，(正真存储的字节数按第一字节的最后3比特位组成的数字+1来决定)
 *附后：数据
 *
 */

//encode
NSData *encodeObject(id object);

NSData *encodeObjectToTopSuperClass(id object, Class topSuperClass, NSError **error);

void encodeObjectIntoCodeData(id object, CCMutableCodeData *codeData, NSError **error);

void encodeObjectToTopSuperClassIntoCodeData(id object, Class topSuperClass, CCMutableCodeData *codeData, NSError **error);

void encodeFloatIntoCodeData(float val, CCMutableCodeData *codeData);

void encodeDoubleIntoCodeData(double val, CCMutableCodeData *codeData);

//可以8、U8,16,U16,32,U32,64,U64
void encodeIntegerIntoCodeData(int64_t val, CCMutableCodeData *codeData);

void encodeStringIntoCodeData(NSString *text, CCMutableCodeData *codeData);

void encodeDataIntoCodeData(NSData *data, CCMutableCodeData *codeData);

//decode
id decodeObjectFromData(NSData *data);

id decodeObjectFromBuffer(uint8_t *buffer ,int64_t length, int64_t *offset, CCCodeItemType *codeType, NSError **error);

float decodeFloatFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset ,NSError **error);

double decodeDoubleFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset, NSError **error);

int64_t decodeIntegerFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset,NSError **error);

NSString *decodeStringFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset,NSError **error);

NSData *decodeDataFromBuffer(uint8_t *buffer, int64_t length, int64_t *offset,NSError **error);

//packet
NSData *packetData(NSData *data,CCCodeItemType codeType);

void packetDataIntoCodeData(NSData *data,CCCodeItemType codeType, CCMutableCodeData *codeData);

void packetCodeData(CCCodeData *data, CCCodeItemType codeType, CCMutableCodeData *codeData);

NSData *unpackData(NSData *data, CCCodeItemType *codeType, int64_t *size, int64_t *offset);

NSRange unpackBuffer(uint8_t *buffer, int64_t bufferSize, CCCodeItemType *codeType, int8_t *len, int64_t *size, int64_t *offset);


//这些是同字节数（sizeof）的转换
int32_t Int32FromFloat(float val);

float FloatFromInt32(int32_t val);

int64_t Int64FromDouble(double val);

double DoubleFromInt64(int64_t val);

//将float和Int32进行转换，Int32存在Int64上
int64_t Int64FromFloat(float val);

float FloatFromInt64(int64_t val);
