//
//  X.h
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "NSObject+CCCodeToTopSuperClass.h"

NS_ASSUME_NONNULL_BEGIN

@interface X : NSObject <CCCodeObjectProtocol>

@property (nonatomic, copy) NSString *x;

+(NSArray<NSString*>*)cc_objectCodeKeyPaths;
@end


//不参与编码
@interface X ()

@property (nonatomic, copy) NSString *xx;

@end


//不参与编码
@interface X (X_Y)

@property (nonatomic, copy) NSString *x_y;

@end

NS_ASSUME_NONNULL_END
