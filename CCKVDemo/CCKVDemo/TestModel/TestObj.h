//
//  TestObj.h
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "X.h"

NS_ASSUME_NONNULL_BEGIN

//参与编码
@interface TestObj : NSObject

@property (nonatomic, assign) NSInteger a;

@property (nonatomic, assign) CGFloat b;

@property (nonatomic, assign) float c;

@property (nonatomic, copy) NSString *d;

@property (nonatomic, copy) NSData *e;

@property (nonatomic, copy) NSArray *f;

@property (nonatomic, copy) NSDictionary *g;

@property (nonatomic, strong) X *x;

@end


//参与编码
@interface TestObj ()

@property (nonatomic, copy) NSString *ext_h;

@end



//不参与编码
@interface TestObj (XA)

@property (nonatomic, strong) X *xa;

@end

NS_ASSUME_NONNULL_END
