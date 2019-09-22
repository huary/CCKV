//
//  NSObject+CCCodeToTopSuperClass.m
//  CCKVDemo
//
//  Created by yuan on 2019/9/10.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "NSObject+CCCodeToTopSuperClass.h"
#import <objc/runTime.h>

@implementation NSObject (CCCodeToTopSuperClass)

-(Class)cc_codeToTopSuperClass
{
    Class objCls = object_getClass(self);
    if ([objCls respondsToSelector:@selector(cc_objectCodeToTopSuperClass)]) {
        return [objCls cc_objectCodeToTopSuperClass];
    }
    return nil;
}

@end
