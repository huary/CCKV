//
//  YZHKVUtils.h
//  YZHKVDemo
//
//  Created by yuan on 2019/6/30.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface YZHKVUtils : NSObject

+ (NSString *)applicationTmpDirectory:(NSString *)filename;
+ (NSString *)applicationCachesDirectory:(NSString *)filename;
+ (NSString *)applicationDocumentsDirectory:(NSString *)filename;

+ (BOOL)checkDirectory:(NSString*)directory;
+ (BOOL)checkAndCreateDirectory:(NSString*)directory;
+ (BOOL)checkFileExistsAtPath:(NSString*)filePath;
+ (void)removeFileItemAtPath:(NSString*)path;
+ (void)createFileItemAtPath:(NSString *)path;
+ (uint64_t)fileSizeAtPath:(NSString *)path;

//这个创建文件夹的速度是上面checkAndCreateDirectory的30倍左右
+ (BOOL)checkAndMakeDirectory:(NSString*)directory;
//这个是上面fileSizeAtPath获取的2倍左右
+ (int64_t)fileSize:(int)fd;
@end

