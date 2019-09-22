//
//  CALayer+Geometry.m
//  YZHUIExcelViewDemo
//
//  Created by yuan on 2017/9/20.
//  Copyright © 2017年 yuan. All rights reserved.
//

#import "CALayer+Geometry.h"

@implementation CALayer (Geometry)

GET_SET_PROPERTY(CGFloat, top, Top)

GET_SET_PROPERTY(CGFloat, left, Left)

GET_SET_PROPERTY(CGFloat, right, Right)

GET_SET_PROPERTY(CGFloat, bottom, Bottom)

GET_SET_PROPERTY(CGFloat, width, Width)

GET_SET_PROPERTY(CGFloat, height, Height)

-(CGPoint)center
{
    return CGPointMake(self.frame.origin.x + self.frame.size.width * 0.5,
                       self.frame.origin.y + self.frame.size.height * 0.5);
}

-(void)setCenter:(CGPoint)center
{
    CGRect frame = self.frame;
    frame.origin.x = center.x - frame.size.width * 0.5;
    frame.origin.y = center.y - frame.size.height * 0.5;
    self.frame = frame;
}

GET_SET_PROPERTY(CGFloat, centerX, CenterX)

GET_SET_PROPERTY(CGFloat, centerY, CenterY)

GET_SET_PROPERTY(CGPoint, origin, Origin)

GET_SET_PROPERTY(CGSize, size, Size)

@end
