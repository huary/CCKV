//
//  Account.h
//  CCKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "NSObject+CCCodeToTopSuperClass.h"

NS_ASSUME_NONNULL_BEGIN


/**********************************************************************
 *Base
 ***********************************************************************/
@interface Base : NSObject

@property(nonatomic, assign) int64_t uin;

@property (nonatomic, copy) NSString *accid;

@end


/**********************************************************************
 *User
 ***********************************************************************/
@interface User : Base

@property(nonatomic, strong) NSString *token;
@property(nonatomic, strong) NSString *session;
@property(nonatomic, strong) NSData *cookie;

@end


/**********************************************************************
 *Account
 ***********************************************************************/
@interface Account : User <CCCodeObjectProtocol>

@property (nonatomic, strong) NSData *autoAuthKey;
@property (nonatomic, strong) NSString *appKey;

@property (nonatomic, assign) int64_t rangeStart;
@property (nonatomic, assign) int64_t watershed;

@property (nonatomic, assign) CGFloat height;

@property (nonatomic, assign) float weight;

+(Class)cc_objectCodeToTopSuperClass;


@end


//参与编码
@interface Account ()

@property (nonatomic, assign) int64_t ext;

@end


//不参与编码
@interface Account (category)

@property(nonatomic, strong) NSString *name;

@end

NS_ASSUME_NONNULL_END
