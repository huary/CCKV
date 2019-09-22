//
//  Account.m
//  YZHKVDemo
//
//  Created by yuan on 2019/8/13.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "Account.h"
#import <objc/runTime.h>


@implementation Base

- (NSString*)description
{
    return [NSString stringWithFormat:@"Base:\n_uid=%@,\n_accid=%@",@(self.uin),self.accid];
}

@end

@implementation User

- (NSString*)description
{
    NSString *sd = [super description];
    
    NSString *cookie = [[NSString alloc] initWithData:self.cookie encoding:NSUTF8StringEncoding];
    
    return [NSString stringWithFormat:@"User:\nsuper=%@,\n_token=%@,\n_session=%@,\n_cookie=%@",sd,self.token,self.session,cookie];
}

@end



@implementation Account

-(NSString*)description
{
    NSString *sd = [super description];

    NSString *autoAuthKey = [[NSString alloc] initWithData:self.autoAuthKey encoding:NSUTF8StringEncoding];
    return [NSString stringWithFormat:@"Account:\nsuper=%@,\n_autoAuthKey=%@,\n_appKey=%@,\n_rangeStart=%@,\n_watershed=%@,\n_height=%@,\n_weight=%@,\n_ext=%@,\n_name=%@",sd,autoAuthKey,self.appKey,@(self.rangeStart),@(self.watershed),@(self.height),@(self.weight),@(self.ext),self.name];
}

+ (Class)cc_objectCodeToTopSuperClass
{
    return [Base class];
}

@end

@implementation Account (category)

-(void)setName:(NSString *)name
{
    objc_setAssociatedObject(self, @selector(name), name, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

-(NSString*)name
{
    return objc_getAssociatedObject(self, _cmd);
}

@end
