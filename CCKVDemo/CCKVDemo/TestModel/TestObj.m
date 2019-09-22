//
//  TestObj.m
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "TestObj.h"
#import <objc/runTime.h>

@implementation TestObj

- (NSString*)description
{
    NSString *e = [[NSString alloc] initWithData:self.e encoding:NSUTF8StringEncoding];
    return [NSString stringWithFormat:@"\n_a=%@,\n_b=%@,\n_c=%f,\n_d=%@,\n_e=%@,\n_f=%@,\n_g=%@,\n_x=%@,\n_ext_h=%@,\n_xa=%@",@(self.a),@(self.b),self.c,self.d,e,self.f,self.g,self.x,self.ext_h,self.xa];
}

@end

@implementation TestObj (XA)

- (void)setXa:(X *)xa
{
    objc_setAssociatedObject(self, @selector(xa), xa, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (X*)xa
{
    return objc_getAssociatedObject(self, _cmd);
}


@end
