//
//  YZHMachTimeUtils.h
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface YZHMachTimeUtils : NSObject

+ (uint64_t)machAbsoluteTime;

+ (CGFloat)machTimeToMS:(uint64_t)machTime;

+ (void)startRecordPoint;

+ (void)recordPointWithText:(NSString*)tagText;

+ (CGFloat)elapsedMSTimeInBlock:(void(^)(void))block;

+ (uint64_t)elapsedMachTimeInBlock:(void(^)(void))block;

@end

NS_ASSUME_NONNULL_END
