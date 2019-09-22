//
//  X.m
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "X.h"
#import <objc/runTime.h>

@implementation X

+(NSArray<NSString*>*)cc_objectCodeKeyPaths {
    return @[@"x"];
}

-(NSString*)description
{
    return [NSString stringWithFormat:@"\n_x=%@,\n_xx=%@,\n_x_y=%@",self.x,self.xx,self.x_y];
}

@end


@implementation X (X_Y)

- (void)setX_y:(NSString *)x_y
{
    objc_setAssociatedObject(self, @selector(x_y), x_y, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (NSString*)x_y
{
    return objc_getAssociatedObject(self, _cmd);
}

@end
