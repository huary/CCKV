//
//  YZHKVUtils.m
//  YZHKVDemo
//
//  Created by yuan on 2019/6/30.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "YZHKVUtils.h"
#import <sys/stat.h>

#define IS_AVAILABLE_NSSTRNG(STRING)            (STRING != nil && STRING.length > 0)


@implementation YZHKVUtils

+ (NSString *)applicationTmpDirectory:(NSString *)filename
{
    NSString *tmpDir = NSTemporaryDirectory();
    if (IS_AVAILABLE_NSSTRNG(filename)) {
        return [tmpDir stringByAppendingString:filename];
    }
    return tmpDir;
}

+ (NSString *)applicationCachesDirectory:(NSString *)filename
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                         NSUserDomainMask,
                                                         YES);
    NSString *basePath = ([paths count] > 0) ? [paths objectAtIndex:0] : nil;
    if (IS_AVAILABLE_NSSTRNG(filename)) {
        return [basePath stringByAppendingPathComponent:filename];
    }
    return basePath;
}

+ (NSString *)applicationDocumentsDirectory:(NSString *)filename
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                         NSUserDomainMask,
                                                         YES);
    NSString *basePath = ([paths count] > 0) ? [paths objectAtIndex:0] : nil;
    if (IS_AVAILABLE_NSSTRNG(filename)) {
        return [basePath stringByAppendingPathComponent:filename];
    }
    return basePath;
}

+ (BOOL)checkDirectory:(NSString*)directory
{
    if (!IS_AVAILABLE_NSSTRNG(directory)) {
        return NO;
    }
    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    BOOL isExists = [fileManager fileExistsAtPath:directory isDirectory:&isDir];
    if (isExists && isDir == YES) {
        return YES;
    }
    return NO;
}

+ (BOOL)checkAndCreateDirectory:(NSString*)directory
{
    if (!IS_AVAILABLE_NSSTRNG(directory)) {
        return NO;
    }
    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    BOOL isExists = [fileManager fileExistsAtPath:directory isDirectory:&isDir];
    if ((isExists && isDir == NO) || isExists == NO) {
        if (isExists == YES) {
            [fileManager removeItemAtPath:directory error:nil];
        }
        [fileManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    }
    return YES;
}

+ (BOOL)checkFileExistsAtPath:(NSString*)filePath
{
    if (!IS_AVAILABLE_NSSTRNG(filePath)) {
        return NO;
    }
    NSFileManager *fileManager = [NSFileManager defaultManager];
    return [fileManager fileExistsAtPath:filePath];
}

+ (void)removeFileItemAtPath:(NSString*)path
{
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    [defaultManager removeItemAtPath:path error:NULL];
}

+ (void)createFileItemAtPath:(NSString *)path
{
    if (!IS_AVAILABLE_NSSTRNG(path)) {
        return;
    }
    NSString *dir = [path stringByDeletingLastPathComponent];
    [self checkAndCreateDirectory:dir];
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    [defaultManager createFileAtPath:path contents:nil attributes:nil];
}

+ (uint64_t)fileSizeAtPath:(NSString *)path
{
    if (![[self class] checkFileExistsAtPath:path]) {
        return 0;
    }
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    NSDictionary *attr = [defaultManager attributesOfItemAtPath:path error:NULL];
    return [attr fileSize];
}

+ (BOOL)checkAndMakeDirectory:(NSString*)directory
{
    if (directory.length == 0) {
        return NO;
    }
    
    NSString *tmp = directory;
    NSMutableArray *components = [NSMutableArray arrayWithObject:@"."];
    while (access(tmp.UTF8String, F_OK) != 0) {
        NSString *c = [tmp lastPathComponent];
        tmp = [tmp stringByDeletingLastPathComponent];
        [components insertObject:c atIndex:0];
    }
    while (components.count > 1) {
        NSString *c = [components firstObject];
        tmp = [tmp stringByAppendingPathComponent:c];
        if (mkdir(tmp.UTF8String, S_IRWXU) != 0) {
            return NO;
        }
        [components removeObjectAtIndex:0];
    }
    return YES;
}

+ (int64_t)fileSize:(int)fd
{
    struct stat st = {};
    if (fstat(fd, &st) != -1) {
        return st.st_size;
    }
    return -1;
}

@end
