//
//  CALayer+Geometry.h
//  YZHUIExcelViewDemo
//
//  Created by yuan on 2017/9/20.
//  Copyright © 2017年 yuan. All rights reserved.
//

#import <QuartzCore/QuartzCore.h>
#import "UIView+Geometry.h"

@interface CALayer (Geometry)

@property (nonatomic, assign) CGFloat top;
@property (nonatomic, assign) CGFloat left;
@property (nonatomic, assign) CGFloat right;
@property (nonatomic, assign) CGFloat bottom;

@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) CGFloat height;

@property (nonatomic, assign) CGPoint center;
@property (nonatomic, assign) CGFloat centerX;
@property (nonatomic, assign) CGFloat centerY;

@property (nonatomic, assign) CGPoint origin;
@property (nonatomic, assign) CGSize  size;

@end
