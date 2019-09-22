//
//  CCCFKVDelegate.h
//  CCKVDemo
//
//  Created by yuan on 2019/9/11.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CCCFKV.h"
#import "CCKV.h"


class CCCFKVDelegate : public CCCFKVDelegateInterface {
private:
    __weak CCKV *_KVTarget;
public:
    CCCFKVDelegate(CCKV *KVTarget);
    
    virtual ~CCCFKVDelegate();
    
    void notifyError(CCCFKV *CFKV, CCCFKVError error);
    
    BOOL notifyWarning(CCCFKV *CFKV, CCCFKVError error);

    NSString *_KVErrorDomain;
};
