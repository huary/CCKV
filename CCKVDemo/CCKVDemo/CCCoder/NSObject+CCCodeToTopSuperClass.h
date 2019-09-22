//
//  NSObject+CCCodeToTopSuperClass.h
//  CCKVDemo
//
//  Created by yuan on 2019/9/10.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol CCCodeObjectProtocol <NSObject>


+(NSArray<NSString*>*)cc_objectCodeKeyPaths;

+(Class)cc_objectCodeToTopSuperClass;

@end


@interface NSObject (CCCodeToTopSuperClass)

-(Class)cc_codeToTopSuperClass;

@end

NS_ASSUME_NONNULL_END
