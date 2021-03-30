//
//  AppDelegate.m
//  CCKVDemo
//
//  Created by yuan on 2019/9/18.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "AppDelegate.h"
#import "ViewController.h"
#import "CCMacro.h"
#import "CCType.h"
#define  _CCARRAY_CAPACITY_ROUNDUP(C)    ({uint8_t B = TYPEULL_BITS_N(C); B = MAX(MIN(B, 63), 2); MIN((1ULL << B),ULONG_MAX);})

//#if !defined(MIN)
//    #define MIN(A,B)    ((A) < (B) ? (A) : (B))
//#endif
//
//#if !defined(MAX)
//    #define MAX(A,B)    ((A) > (B) ? (A) : (B))
//#endif


//#if !defined(MIN)
//    #define MIN(A,B)    ({ __typeof__(A) __a = (A); __typeof__(B) __b = (B); __a < __b ? __a : __b; })
//#endif
//
//#if !defined(MAX)
//    #define MAX(A,B)    ({ __typeof__(A) __a = (A); __typeof__(B) __b = (B); __a < __b ? __b : __a; })
//#endif

#define _MIN_TEST(A,B)          MIN(({NSLog(@"A=%@",@(A));A;}),({NSLog(@"B=%@",@(B));B;}))

typedef struct T
{
    int a;
    char b;
    int c[0];
    int d[0];
};

typedef union {
    CCUInt16_t op;
    struct {
        CCUInt16_t :9;
        CCUByte_t cow:1;
        CCUByte_t recnew:1;
        CCUByte_t exponent:5;
    }bits;
}option;



@interface AppDelegate ()

/** <#注释#> */
//@property (nonatomic, assign) NSMutableArray *array;

/** <#注释#> */
//@property (nonatomic, assign) NSString *txt;

@end

@implementation AppDelegate

- (void)pri_test
{
    option opt;
//    opt.bits.cow = 1;
//    opt.bits.recnew = 1;
//    opt.bits.exponent = 2;
    CC_SET_BIT_FIELD(opt.op, 0, 5, 2);
    CC_SET_BIT_FIELD(opt.op, 5, 1, 1);
    CC_SET_BIT_FIELD(opt.op, 6, 2, 1);
    
    NSLog(@"opt.op=%@",@(opt.op));
    
//    NSLog(@"sizeof=%d",sizeof(struct T));
//    struct T *t = calloc(24, 1);
//    t->c[0] = 4;
//    t->c[1] = 5;
//    t->c[2] = 6;
//    t->c[3] = 7;
//    NSLog(@"a=%d,b=%d",t->a,t->b);
////    t->d[0] = 8;
////    t->d = t->c[0] + 1;
//    int *ptr = t->d;
//    int *pc = t->c;
//    int d = (char*)pc - (char*)t;
//    NSLog(@"t->c=%p,t->d=%p,t=%p,t->a=%p,diff=%d",t->c,t->d,t,&t->a,d);
//    NSLog(@"a=%d,b=%d,T=%d,t->c=%d",t->a,t->b,sizeof(struct T),t->c[0]);
//    for (int i = 0; i < 4; ++i) {
//        NSLog(@"t->c[%d]=%d",i,t->c[i]);
//    }
//
//    NSLog(@"Umax=%@,Umax2=%@",@((uint32_t)-1),@((uint32_t)~(0)));
//    NSLog(@"max=%@,max2=%@",@(((uint32_t)-1) >> 1),@((~((uint32_t)0)) >> 1));
//
//    uint64_t cap = ULONG_MAX;
//    NSLog(@"B=%d",TYPEULL_BITS_N(cap));
//    uint64_t c = _CCARRAY_CAPACITY_ROUNDUP(cap);
//    uint64_t M = -1;
//    NSLog(@"cap=%@,r=%@,M=%@",@(cap),@(1ULL << 63),@(M));
//    NSLog(@"cap=%@",@(c));
//
////    NSLog(@"%@",@(MIN(({NSLog(@"A");3;}), ({NSLog(@"B");4;}))));
//    NSLog(@"_MIN_TEST=%@",@(_MIN_TEST(5,9)));
//    NSLog(@"ccountMax=%@",@(CCCountMax));
//
//    free(t);
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    
    [self pri_test];

    return YES;
}


- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
}


- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}


- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
}


- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}


- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}


@end
