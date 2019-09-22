//
//  CCCFKVDelegate.m
//  CCKVDemo
//
//  Created by yuan on 2019/9/11.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "CCCFKVDelegate.h"


CCCFKVDelegate::CCCFKVDelegate(CCKV *KVTarget)
{
    _KVTarget = KVTarget;
}

CCCFKVDelegate::~CCCFKVDelegate()
{
    
}

void CCCFKVDelegate::notifyError(CCCFKV *CFKV, CCCFKVError error)
{
    if ([_KVTarget.delegate respondsToSelector:@selector(kv:reportError:)]) {
        NSError *err = [NSError errorWithDomain:_KVErrorDomain code:error userInfo:NULL];
        [_KVTarget.delegate kv:_KVTarget reportError:err];
    }
}

BOOL CCCFKVDelegate::notifyWarning(CCCFKV *CFKV, CCCFKVError error)
{
    if ([_KVTarget.delegate respondsToSelector:@selector(kv:reportWarning:)]) {
        NSError *err = [NSError errorWithDomain:_KVErrorDomain code:error userInfo:NULL];
        return [_KVTarget.delegate kv:_KVTarget reportWarning:err];
    }
    return YES;
}
