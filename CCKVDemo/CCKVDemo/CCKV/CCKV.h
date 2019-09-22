//
//  CCKV.h
//  CCKVDemo
//
//  Created by yuan on 2019/6/30.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CCAESCryptor.h"

@class CCKV;
@protocol CCKVDelegate <NSObject>

//报告严重错误，一般是直接关闭，
- (void)kv:(CCKV *)kv reportError:(NSError *)error;
//报告警告，返回YES表示继续，返回NO表示不再执行
- (BOOL)kv:(CCKV *)kv reportWarning:(NSError *)error;

@end



@interface CCKV : NSObject


@property (nonatomic, copy, readonly) NSString *name;

/** <#注释#> */
@property (nonatomic, weak) id<CCKVDelegate> delegate;

//提供了一个全局单利，也可以自己申请
+ (instancetype)defaultKV;

- (instancetype)initWithName:(NSString*)name path:(NSString*)path;

- (instancetype)initWithName:(NSString *)name path:(NSString *)path cryptKey:(NSData *)cryptKey;

- (BOOL)setObject:(id)object forKey:(id)key;

- (BOOL)setObject:(id)object topSuperClass:(Class)topSuperClass forKey:(id)key;

- (BOOL)setFloat:(float)val forKey:(id)key;

- (BOOL)setDouble:(double)val forKey:(id)key;

- (BOOL)setInteger:(int64_t)val forKey:(id)key;

- (BOOL)setBool:(BOOL)val forKey:(id)key;

- (BOOL)setInt8:(int8_t)val forKey:(id)key;

- (BOOL)setUInt8:(uint8_t)val forKey:(id)key;

- (BOOL)setInt16:(int16_t)val forKey:(id)key;

- (BOOL)setUInt16:(uint16_t)val forKey:(id)key;

- (BOOL)setInt32:(int32_t)val forKey:(id)key;

- (BOOL)setUInt32:(uint32_t)val forKey:(id)key;

- (BOOL)setInt64:(int64_t)val forKey:(id)key;

- (BOOL)setUInt64:(uint64_t)val forKey:(id)key;

- (id)getObjectForKey:(id)key;

- (BOOL)getBoolForKey:(id)key;

- (int8_t)getInt8ForKey:(id)key;

- (uint8_t)getUInt8ForKey:(id)key;

- (int16_t)getInt16ForKey:(id)key;

- (uint16_t)getUInt16ForKey:(id)key;

- (int32_t)getInt32ForKey:(id)key;

- (uint32_t)getUInt32ForKey:(id)key;

- (int64_t)getInt64ForKey:(id)key;

- (uint64_t)getUInt64ForKey:(id)key;

- (float)getFloatForKey:(id)key;

- (double)getDoubleForKey:(id)key;

- (int64_t)getIntegerForKey:(id)key;

- (NSDictionary*)allEntries;

- (void)removeObjectForKey:(id)key;

- (void)updateCryptKey:(NSData*)cryptKey;

- (NSError*)lastError;

- (void)clear:(BOOL)truncateFileSize;

- (void)close;

- (NSString*)filePath;


@end


