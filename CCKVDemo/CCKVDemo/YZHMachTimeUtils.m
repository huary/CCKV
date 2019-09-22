//
//  YZHMachTimeUtils.m
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "YZHMachTimeUtils.h"
#import <mach/mach_time.h>

static uint64_t recordPointTime_s = 0;
static NSString *recordPointText_s = nil;

@implementation YZHMachTimeUtils

+ (uint64_t)machAbsoluteTime
{
    return mach_absolute_time();
}

//转换为毫秒
+ (CGFloat)machTimeToMS:(uint64_t)machTime
{
    mach_timebase_info_data_t timebase;
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
        return -1;
    }
    
    return machTime * timebase.numer * 1.0 * 1e-6 / timebase.denom;
}

+ (CGFloat)elapsedMSTimeInBlock:(void(^)(void))block
{
    uint64_t start = [self machAbsoluteTime];
    
    if (block) {
        block();
    }
    
    uint64_t end = [self machAbsoluteTime];
    
    uint64_t diff = end - start;
    CGFloat ms = [self machTimeToMS:diff];
    NSLog(@"ms=%f MS",ms);
    return ms;
}

+ (void)startRecordPoint
{
    recordPointTime_s = 0;
    recordPointText_s = nil;
}

+ (void)recordPointWithText:(NSString *)text
{
//#if DEBUG
    uint64_t last = recordPointTime_s;
    NSString *lastText = recordPointText_s;
    recordPointTime_s = [self machAbsoluteTime];
    recordPointText_s = text;
    int64_t diff = recordPointTime_s - last;
    if (last > 0 && diff > 0) {
        CGFloat ms = [self machTimeToMS:diff];
        NSLog(@"从 [%@] -> [%@] 耗时: %f MS", lastText, text,ms);
    }
//#else
//#endif
}

+ (uint64_t)elapsedMachTimeInBlock:(void(^)(void))block
{
    uint64_t start = [self machAbsoluteTime];
    
    if (block) {
        block();
    }
    
    uint64_t end = [self machAbsoluteTime];
    
    uint64_t diff = end - start;
    return diff;
}

@end
