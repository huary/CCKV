//
//  CCCFKV.h
//  CCKVDemo
//
//  Created by yuan on 2019/9/8.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CCCoder.h"
#include <string>
using namespace std;

typedef NS_ENUM(int32_t, CCCFKVError)
{
    CCCFKVErrorNone              = 0,
    //密码错误
    CCCFKVErrorCryptKeyError     = 1,
    //编码错误
    CCCFKVErrorCoderError        = 2,
    //CRC不一样错误,这是一个warning
    CCCFKVErrorCRCError          = 3,
};

class CCCFKV;
class CCCFKVDelegateInterface {
public:
    virtual void notifyError(CCCFKV *CFKV, CCCFKVError error) = 0;
    
    virtual BOOL notifyWarning(CCCFKV *CFKV, CCCFKVError error) = 0;
};



class CCCFKV {
private:
    void *ptrCFKVContext;
    string name;
    
    string path;
    
    void setupCFKVDefault();
    
public:
    CCCFKV();

    CCCFKV(const string &name);

    CCCFKV(const string &name, const string &path);
    
    CCCFKV(const string &name, const string &path, CCCodeData *cryptKey);
    
    virtual ~CCCFKV();
    
    string getFilePath();
    
    BOOL updateCryptKey(CCCodeData *cryptKey);
    
    
    /**
     设置以Key作为键，以Object作为值

     @param object 作为值，在object为nil是removeObjectForKey的操作
     @param key 作为键，不可为空
     @return 返回是否成功，YES-成功，NO-失败
     */
    BOOL setObjectForKey(id object, id key);
    
    
    /**
     设置以Key作为键，以Object作为值

     @param object 作为值，在object为nil是removeObjectForKey的操作
     @param topSuperClass object编码到父类的类名
     @param key key 作为键，不可为空
     @return 返回是否成功，YES-成功，NO-失败
     */
    BOOL setObjectForKey(id object, Class topSuperClass, id key);
    
    BOOL setFloatForKey(float val, id key);
    
    BOOL setDoubleForKey(double val, id key);
    
    BOOL setIntegerForKey(int64_t val, id key);
    
    id getObjectForKey(id key);
    
    float getFloatForKey(id key);
    
    double getDoubleForKey(id key);
    
    int64_t getIntegerForKey(id key);
    
    NSDictionary *getAllEntries();
    
    void removeObjectForKey(id key);
    
    CCCFKVError getLastError();

    void clear(BOOL truncateFileSize);
    
    void close();
    
public:
//    weak_ptr<CCCFKVDelegateInterface> delegate;
    CCCFKVDelegateInterface *delegate;
};
