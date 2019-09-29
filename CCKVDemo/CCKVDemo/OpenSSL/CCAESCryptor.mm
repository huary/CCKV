//
//  CCAESCryptor.m
//  CCKVDemo
//
//  Created by yuan on 2019/7/28.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "CCAESCryptor.h"
#import "openSSL_aes.h"
#import "CCMacro.h"
#include <string>

//是否使用智能指针
#define CRYPTOR_USE_SMT_PTR    (1)

typedef NS_ENUM(NSInteger, _AESVectorType)
{
    _AESVectorTypeNone          = 0,
    //各自的
    _AESVectorTypeSingle        = 1,
    //公用的
    _AESVectorTypePublic        = 2,
};

static const int8_t AESBlockSize_s   = CCAESKeySize128;//16;

typedef void(*_AES_CFB)(const unsigned char *in, unsigned char *out, size_t length, const openSSL::AES_KEY *key, unsigned char *ivec, int *num, const int enc);

typedef int64_t(^_CCAESEncryptSizeBlock)(int64_t inputSize, AESPaddingType paddingType);

typedef struct _CCAESCryptKey
{
    openSSL::AES_KEY *ptr_AESEncryptKey;
    openSSL::AES_KEY *ptr_AESDecryptKey;
}_CCAESCryptKey_S, *_PTR_CCAESCryptKey_S;

typedef struct _CCAESCryptorInfo
{
#if CRYPTOR_USE_SMT_PTR
    std::shared_ptr<CCCodeData> key;
    std::shared_ptr<CCCodeData> inVertor;
    std::shared_ptr<CCCodeData> encryptVector;
    std::shared_ptr<CCCodeData> decryptVector;
#else
    CCCodeData *key;
    CCCodeData *inVertor;
    CCCodeData *encryptVector;
    CCCodeData *decryptVector;
#endif
    
    _AES_CFB ptr_func;
    openSSL::AES_KEY AESEncryptKey;
    openSSL::AES_KEY AESDecryptKey;
    openSSL::AES_KEY *ptrAESKey;
    _CCAESCryptKey_S cryptKey;

    int32_t offset;
    _AESVectorType vectorType;
    _CCAESEncryptSizeBlock encryptSizeBlock;
    CCAESCryptDataPaddingBlock paddingBlock;
    
//public
    CCAESKeyType keyType;
    CCCryptMode cryptMode;
}_CCAESCryptorInfo_S;

void _AES_ofb128_crypt_block_data(const unsigned char *in, unsigned char *out,
                                  size_t length, const openSSL::AES_KEY *key,
                                  unsigned char *ivec, int *num, const int enc)
{
    AES_ofb128_encrypt(in, out, length, key, ivec, num);
}

void _AES_cfb1_crypt_block_data(const unsigned char *in, unsigned char *out,
                                size_t length, const openSSL::AES_KEY *key,
                                unsigned char *ivec, int *num, const int enc)
{
    openSSL::AES_cfb1_encrypt(in, out, TYPE_LS(length, 3), key, ivec, num, enc);
}

void _AES_cbc_crypt_block_data(const unsigned char *in, unsigned char *out,
                               size_t length, const openSSL::AES_KEY *key,
                               unsigned char *ivec, int *num, const int enc)
{
    if (length <= 0 || key == nullptr) {
        return;
    }
    BOOL r = TYPE_AND(length, (AESBlockSize_s - 1));
    assert(r == 0);
    
    _CCAESCryptKey_S *ptr_cryptKey = (_CCAESCryptKey_S*)key;
    
    openSSL::AES_KEY AESKey = (enc == AES_ENCRYPT) ? *(ptr_cryptKey->ptr_AESEncryptKey) : *(ptr_cryptKey->ptr_AESDecryptKey);
    openSSL::AES_cbc_encrypt(in, out, length, &AESKey, ivec, enc);
}

void _AES_ecb_crypt_block_data(const unsigned char *in, unsigned char *out,
                               size_t length, const openSSL::AES_KEY *key,
                               unsigned char *ivec, int *num, const int enc)
{
    if (length <= 0 || key == nullptr) {
        return;
    }
    BOOL r = TYPE_AND(length, (AESBlockSize_s - 1));
    assert(r == 0);
    
    _CCAESCryptKey_S *ptr_cryptKey = (_CCAESCryptKey_S*)key;
    
    openSSL::AES_KEY AESKey = (enc == AES_ENCRYPT) ? *(ptr_cryptKey->ptr_AESEncryptKey) : *(ptr_cryptKey->ptr_AESDecryptKey);
    
    size_t cryptLen = 0;
    uint8_t *inTmp = (uint8_t*)in;
    uint8_t *outTmp = (uint8_t*)out;
    while (cryptLen < length) {
        openSSL::AES_KEY AESKeyTmp = AESKey;
        openSSL::AES_ecb_encrypt(inTmp, outTmp, &AESKeyTmp, enc);
        
        inTmp += AESBlockSize_s;
        outTmp += AESBlockSize_s;
        cryptLen += AESBlockSize_s;
    }
}

static inline CCCodeData * _copyCodeData(CCCodeData *codeData)
{
    int64_t dataSize = codeData->dataSize();
    CCCodeData *copy = new CCMutableCodeData(dataSize);
    copy->writeBuffer(codeData->bytes(), dataSize);
    return copy;
}

static inline uint8_t _getPaddingSize(int64_t inputSize, AESPaddingType paddingType)
{
    if (inputSize == 0) {
        return 0;
    }
    uint8_t r = TYPE_AND(inputSize, (AESBlockSize_s - 1));
    uint8_t appendSize = 0;
    if (paddingType == AESPaddingTypeZero) {
        appendSize = r ? AESBlockSize_s - r : 0;
    }
    else if (paddingType == AESPaddingTypePKCS7) {
        appendSize = AESBlockSize_s - r;
    }
    return appendSize;
}

static inline int64_t _getEncryptSize(int64_t inputSize, AESPaddingType paddingType)
{
    uint8_t paddingSize = _getPaddingSize(inputSize, paddingType);
    return inputSize + paddingSize;
}

static inline void _paddingData(CCCodeData *codeData, AESPaddingType paddingType, CCCryptOperation cryptOperation)
{
    if (codeData == NULL) {
        return;
    }
    int64_t len = codeData->dataSize();
    if (len == 0) {
        return;
    }
    uint8_t paddingSize = _getPaddingSize(len, paddingType);
    uint8_t paddingValue = 0;
    
    if (paddingType == AESPaddingTypeZero) {
        if (cryptOperation == CCCryptOperationEncrypt) {
            paddingValue = 0;
        }
    }
    else if (paddingType == AESPaddingTypePKCS7) {
        if (cryptOperation == CCCryptOperationEncrypt) {
            paddingValue = paddingSize;
        }
        else {
            codeData->seekTo(len-1);
            uint8_t last = codeData->readByte();
            if (last > 0 && last <= AESBlockSize_s && last < len) {
                codeData->truncateToWithSeek(len - last, CCDataSeekTypeEND);
            }
        }
    }
    
    int64_t afterPaddingSize = len + paddingSize;
    if (cryptOperation == CCCryptOperationEncrypt && paddingSize > 0 && codeData->bufferSize() >= afterPaddingSize) {
//        codeData->ensureRemSize(paddingSize);
        uint8_t *ptr = codeData->bytes() + len;
        memset(ptr, (uint8_t)paddingValue, paddingSize);
        codeData->truncateToWithSeek(afterPaddingSize, CCDataSeekTypeEND);
    }
}

BOOL _checkKey(CCCodeData *key, CCAESKeyType keyType, CCCodeData **outKeyData)
{
    uint8_t keySize = 0;
    switch (keyType) {
        case CCAESKeyType128: {
            keySize = CCAESKeySize128;
            break;
        }
        case CCAESKeyType192: {
            keySize = CCAESKeySize192;
            break;
        }
        case CCAESKeyType256: {
            keySize = CCAESKeySize256;
            break;
        }
        default:
            break;
    }
    
    if (keySize > 0 && key->dataSize() >= keySize) {
        if (outKeyData) {
            CCMutableCodeData *codeData = new CCMutableCodeData(keySize);
            codeData->writeBuffer(key->bytes(), keySize);
            *outKeyData = codeData;
        }
        return YES;
        
    }
    return NO;
}

void CCAESCryptor::setupDefault()
{
    struct _CCAESCryptorInfo *cryptorInfo = (struct _CCAESCryptorInfo *)calloc(1, sizeof(struct _CCAESCryptorInfo));
    if (cryptorInfo) {
        cryptorInfo->vectorType = _AESVectorTypeNone;
        cryptorInfo->cryptKey.ptr_AESEncryptKey = &cryptorInfo->AESEncryptKey;
        cryptorInfo->cryptKey.ptr_AESDecryptKey = &cryptorInfo->AESDecryptKey;
    }
    ptrCryptorInfo = cryptorInfo;
}

BOOL CCAESCryptor::setupCryptor(CCCodeData *key, CCAESKeyType keyType, CCCodeData *vector, CCCryptMode cryptMode)
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
    if (info == NULL) {
        return NO;
    }
    CCCodeData *keyData = NULL;
    BOOL OK = _checkKey(key, keyType, &keyData);
    if (OK == NO) {
        return NO;
    }
#if CRYPTOR_USE_SMT_PTR
    std::shared_ptr<CCCodeData> sharedKey(keyData);
    info->key = sharedKey;
#else
    if (info->key) {
        delete info->key;
    }
    info->key = keyData;
#endif
    info->keyType = keyType;
    info->cryptMode = cryptMode;
    
    switch (cryptMode) {
        case CCCryptModeCFB: {
            info->ptrAESKey = &(info->AESEncryptKey);
            info->vectorType = _AESVectorTypePublic;
            info->ptr_func = (openSSL::AES_cfb128_encrypt);
            info->encryptSizeBlock = ^int64_t(int64_t inputSize, AESPaddingType paddingType) {
                return inputSize;
            };
            break;
        }
        case CCCryptModeCFB1: {
            info->ptrAESKey = &(info->AESEncryptKey);
            info->vectorType = _AESVectorTypePublic;
            info->ptr_func = _AES_cfb1_crypt_block_data;//(openSSL::AES_cfb1_encrypt);
            info->encryptSizeBlock = ^int64_t(int64_t inputSize, AESPaddingType paddingType) {
                return inputSize;
            };
            break;
        }
        case CCCryptModeCFB8: {
            info->ptrAESKey = &(info->AESEncryptKey);
            info->vectorType = _AESVectorTypePublic;
            info->ptr_func = (openSSL::AES_cfb8_encrypt);
            info->encryptSizeBlock = ^int64_t(int64_t inputSize, AESPaddingType paddingType) {
                return inputSize;
            };
            break;
        }
        case CCCryptModeOFB: {
            info->ptrAESKey = &(info->AESEncryptKey);
            info->vectorType = _AESVectorTypePublic;
            info->ptr_func = _AES_ofb128_crypt_block_data;
            info->encryptSizeBlock = ^int64_t(int64_t inputSize, AESPaddingType paddingType) {
                return inputSize;
            };
            break;
        }
        case CCCryptModeCBC: {
            info->ptrAESKey = (openSSL::AES_KEY *)&(info->cryptKey);
            info->vectorType = _AESVectorTypeSingle;
            info->ptr_func = _AES_cbc_crypt_block_data;
            info->encryptSizeBlock = ^int64_t(int64_t inputSize, AESPaddingType paddingType) {
                return _getEncryptSize((int64_t)inputSize, paddingType);
            };
            info->paddingBlock = ^(CCAESCryptor *cryptor, CCCodeData *cryptData, AESPaddingType paddingType, CCCryptOperation cryptOperation) {
                _paddingData(cryptData, paddingType, cryptOperation);
            };
            break;
        }
        case CCCryptModeECB: {
            info->ptrAESKey = (openSSL::AES_KEY *)&(info->cryptKey);
            info->vectorType = _AESVectorTypeNone;
            info->ptr_func = _AES_ecb_crypt_block_data;
            info->encryptSizeBlock = ^int64_t(int64_t inputSize, AESPaddingType paddingType) {
                return _getEncryptSize(inputSize, paddingType);
            };
            info->paddingBlock = ^(CCAESCryptor *cryptor, CCCodeData *cryptData, AESPaddingType paddingType, CCCryptOperation cryptOperation) {
                _paddingData(cryptData, paddingType, cryptOperation);
            };
            break;
        }
        default:
            break;
    }
    
    if (cryptMode != CCCryptModeECB) {
        if (vector == NULL || vector->dataSize() < AESBlockSize_s) {
            return NO;
        }
#if CRYPTOR_USE_SMT_PTR
        if (info->inVertor == NULL) {
            info->inVertor = std::make_shared<CCMutableCodeData>(AESBlockSize_s);
        }
#else
        if (info->inVertor == NULL) {
            info->inVertor = new CCMutableCodeData(AESBlockSize_s);
        }
#endif
        if (info->inVertor) {
            info->inVertor->seek(CCDataSeekTypeSET);
            info->inVertor->writeBuffer(vector->bytes(), AESBlockSize_s);
        }
    }
    return YES;
}


CCAESCryptor::CCAESCryptor(CCCodeData *AESKey, CCAESKeyType keyType)
{
    CCAESCryptor(AESKey, keyType, NULL, CCCryptModeECB);
}

CCAESCryptor::CCAESCryptor(CCCodeData *AESKey, CCAESKeyType keyType, CCCodeData *inVector, CCCryptMode cryptMode)
{
    setupDefault();
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
    if (info) {
        BOOL OK = setupCryptor(AESKey, keyType, inVector, cryptMode);
        if (OK) {
            reset();
        }
    }
}

CCAESCryptor::~CCAESCryptor()
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
    if (info) {
#if CRYPTOR_USE_SMT_PTR
        info->encryptVector.reset();
        info->decryptVector.reset();
        info->inVertor.reset();
        info->key.reset();
#else
        if (info->encryptVector == info->decryptVector) {
            if (info->encryptVector) {
                delete info->encryptVector;
                info->encryptVector = info->decryptVector = NULL;
            }
        }
        else {
            delete info->encryptVector;
            info->encryptVector = NULL;
            delete info->decryptVector;
            info->decryptVector = NULL;
        }
        
        if (info->inVertor) {
            delete info->inVertor;
            info->inVertor = NULL;
        }
        
        if (info->key) {
            delete info->key;
            info->key = NULL;
        }
#endif
        free(info);
        info = NULL;
        
        ptrCryptorInfo = NULL;
    }
}



void CCAESCryptor::reset()
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
    if (info == NULL) {
        return;
    }
    info->offset = 0;
    if (info->inVertor && info->vectorType != _AESVectorTypeNone) {
#if CRYPTOR_USE_SMT_PTR
        if (info->vectorType == _AESVectorTypeSingle) {
            
            info->encryptVector = std::shared_ptr<CCCodeData>(_copyCodeData(info->inVertor.get()));
            info->decryptVector = std::shared_ptr<CCCodeData>(_copyCodeData(info->inVertor.get()));
        }
        else if (info->vectorType == _AESVectorTypePublic) {
            info->encryptVector = info->decryptVector = std::shared_ptr<CCCodeData>(_copyCodeData(info->inVertor.get()));
        }
        else if (info->vectorType == _AESVectorTypeNone) {
            info->encryptVector = info->decryptVector = std::shared_ptr<CCCodeData>();
        }
#else
        if (info->encryptVector == info->decryptVector) {
            if (info->encryptVector) {
                delete info->encryptVector;
                info->encryptVector = info->decryptVector = NULL;
            }
        }
        else {
            delete info->encryptVector;
            info->encryptVector = NULL;
            delete info->decryptVector;
            info->decryptVector = NULL;
        }
        
        if (info->vectorType == _AESVectorTypeSingle) {
            info->encryptVector = _copyCodeData(info->inVertor);
            info->decryptVector = _copyCodeData(info->inVertor);
        }
        else if (info->vectorType == _AESVectorTypePublic) {
            info->encryptVector = info->decryptVector = _copyCodeData(info->inVertor);
        }
#endif
    }
    if (info->key) {
#if CRYPTOR_USE_SMT_PTR
        std::shared_ptr<CCCodeData> key = info->key;
#else
        CCCodeData *key = info->key;
#endif
        uint8_t *ptrKey = key->bytes();
        int32_t keyBits = TYPE_LS((int32_t)info->key->dataSize(), 3);
        openSSL::AES_set_encrypt_key(ptrKey, keyBits, &info->AESEncryptKey);
        openSSL::AES_set_decrypt_key(ptrKey, keyBits, &info->AESDecryptKey);
    }
}

BOOL CCAESCryptor::isValidCryptor()
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
#if CRYPTOR_USE_SMT_PTR
    BOOL OK = _checkKey(info->key.get(), info->keyType, NULL);
#else
    BOOL OK = _checkKey(info->key, info->keyType, NULL);
#endif
    return OK;
}

CCAESKeyType CCAESCryptor::getKeyType()
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
    return info->keyType;
}

CCCryptMode CCAESCryptor::getCryptMode()
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;
    return info->cryptMode;
}

//http://www.seacha.com/tools/aes.html
BOOL CCAESCryptor::crypt(CCCryptOperation cryptOperation, CCCodeData *input, CCCodeData *output)
{
    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;

    return crypt(cryptOperation, input, output, AESPaddingTypePKCS7, info->paddingBlock);
}

BOOL CCAESCryptor::crypt(CCCryptOperation cryptOperation, CCCodeData *input, CCCodeData *output, AESPaddingType paddingType, CCAESCryptDataPaddingBlock paddingBlock)
{
    
    if (input == NULL || output == NULL || NO ==  isValidCryptor()) {
        return NO;
    }
    
    int64_t inputDataSize = input->dataSize();
    int64_t inputBufferSize = input->bufferSize();
    int64_t outputBufferSize = output->bufferSize();
    if (inputDataSize <= 0 || outputBufferSize < inputDataSize) {
        return NO;
    }

    struct _CCAESCryptorInfo *info = (struct _CCAESCryptorInfo*)ptrCryptorInfo;

    int offset = info->offset;
    
    int64_t cryptSize = inputDataSize;
    if (cryptOperation == CCCryptOperationEncrypt) {
        cryptSize = info->encryptSizeBlock(inputDataSize, paddingType);
        if (inputBufferSize < cryptSize || outputBufferSize < cryptSize) {
            return NO;
        }
        
        if (paddingBlock) {
            paddingBlock(this, input, paddingType, cryptOperation);
            //在经过paddingBlock后有可能改变input->dataSize != cryptSize;
            cryptSize = MIN(cryptSize, input->dataSize());
        }
        
        uint8_t *vectorPtr = info->encryptVector.get() ? info->encryptVector->bytes() : NULL;
        
        ((_AES_CFB)*(info->ptr_func))((uint8_t*)input->bytes(), (uint8_t*)output->bytes(), (size_t)cryptSize, info->ptrAESKey, (uint8_t*)vectorPtr, &offset, AES_ENCRYPT);
    }
    else {
        uint8_t *vectorPtr = info->decryptVector.get() ? info->decryptVector->bytes() : NULL;

        ((_AES_CFB)*(info->ptr_func))((uint8_t*)input->bytes(), (uint8_t*)output->bytes(), (size_t)inputDataSize, info->ptrAESKey, (uint8_t*)vectorPtr, &offset, AES_DECRYPT);
        
        if (paddingBlock) {
            output->seekTo(cryptSize);
            paddingBlock(this, output, paddingType, cryptOperation);
            
            cryptSize = MIN(cryptSize, output->dataSize());
        }
    }
    info->offset = offset;
    
    output->truncateToWithSeek(cryptSize, CCDataSeekTypeEND);
    return YES;
}

