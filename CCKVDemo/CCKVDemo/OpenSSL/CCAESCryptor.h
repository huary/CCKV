//
//  CCAESCryptor.h
//  CCKVDemo
//
//  Created by yuan on 2019/7/28.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CCCodeData.h"

#define AES_KEYSIZE_FROM_KEYTYPE(KEYTYPE)   ((KEYTYPE == CCAESKeyType192) ? CCAESKeySize192 : ((KEYTYPE == CCAESKeyType128) ? CCAESKeySize128 : CCAESKeySize256))

typedef NS_ENUM(uint8_t, CCCryptMode)
{
    CCCryptModeCFB     = 0,
    CCCryptModeCFB1    = 1,
    CCCryptModeCFB8    = 2,
    CCCryptModeOFB     = 3,
    CCCryptModeCBC     = 4,
    CCCryptModeECB     = 5,
};

//按字节算，SizeXXX，XXX按bit位算，枚举值按字节算
typedef NS_ENUM(uint8_t, CCAESKeySize)
{
    CCAESKeySize128    = 16,
    CCAESKeySize192    = 24,
    CCAESKeySize256    = 32,
};

typedef NS_ENUM(uint8_t, CCAESKeyType)
{
    CCAESKeyType128    = 0,
    CCAESKeyType192    = 1,
    CCAESKeyType256    = 2,
};

typedef NS_ENUM(uint8_t, CCCryptOperation)
{
    CCCryptOperationEncrypt    = 0,
    CCCryptOperationDecrypt    = 1,
};

typedef NS_ENUM(uint8_t, AESPaddingType)
{
    AESPaddingTypeZero     = 0,
    AESPaddingTypePKCS7    = 1,
};

class CCAESCryptor;
typedef void(^CCAESCryptDataPaddingBlock)(CCAESCryptor *cryptor, CCCodeData *cryptData, AESPaddingType paddingType, CCCryptOperation cryptOperation);


class CCAESCryptor {
private:
    void *ptrCryptorInfo;
    void setupDefault();
    BOOL setupCryptor(CCCodeData *key, CCAESKeyType keyType, CCCodeData *vector, CCCryptMode cryptMode);
public:
    //复制了Key中的数据
    CCAESCryptor(CCCodeData *AESKey, CCAESKeyType keyType);
    
    //复制了Key和inVector中的数据
    CCAESCryptor(CCCodeData *AESKey, CCAESKeyType keyType, CCCodeData *inVector, CCCryptMode cryptMode);
    
    virtual ~CCAESCryptor();
  
    void reset();
    
    BOOL isValidCryptor();
    
    CCAESKeyType getKeyType();
    
    CCCryptMode getCryptMode();
    
    BOOL crypt(CCCryptOperation cryptOperation, CCCodeData *input, CCCodeData *output);
    
    BOOL crypt(CCCryptOperation cryptOperation, CCCodeData *input, CCCodeData *output, AESPaddingType paddingType, CCAESCryptDataPaddingBlock paddingBlock);
};
