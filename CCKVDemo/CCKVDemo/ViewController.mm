//
//  ViewController.m
//  YZHKVDemo
//
//  Created by yuan on 2019/6/30.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "ViewController.h"
#import <objc/runTime.h>
#import <mach/mach_time.h>
#import "CCCoder.h"
#import "CCAESCryptor.h"

#import "X.h"
#import "TestObj.h"
#import "Account.h"

#import "YZHMachTimeUtils.h"

#import "YZHKVUtils.h"
#import "YZHUIExcelView.h"
#import "UIReuseCell.h"
#import <MMKV/MMKV.h>


#import "CCCFKV.h"
#import "CCKV.h"
#import "CCMacro.h"
//#import "CCDictionary.h"
#include "CCDictionary.h"
#include "CCArray.h"
#include "CCType.h"

#include "CCFileMap.hpp"

#include "CCBPTreeIndex.hpp"

#include <string>

typedef NS_ENUM(NSInteger, NSExcelRowTag)
{
    NSExcelRowTagCCKV          = 1,
    NSExcelRowTagMMKV           = 2,
    NSExcelRowTagUserDefault    = 3,
};

typedef NS_ENUM(NSInteger, NSExcelColumnTag)
{
    NSExcelColumnTagFirstInit   = 1,
    NSExcelColumnTagLoadInit    = 2,
    NSExcelColumnTagWrite       = 3,
    NSExcelColumnTagRead        = 4,
    NSExcelColumnTagDelete      = 5,
    NSExcelColumnTagClear       = 6,
    NSExcelColumnTagUpdateCryptKey  = 7,
};

typedef NS_ENUM(NSInteger, NSTestOption)
{
    NSTestOptionInt     = 0,
    NSTestOptionDouble  = 1,
    NSTestOptionString  = 2,
};

class Test {
    
public:
    Test() {}
    virtual ~Test() { NSLog(@"%p free",this);}
};




@interface ViewController ()<YZHUIExcelViewDelegate>
{
    shared_ptr<CCAESCryptor> _cryptor;
    CCDictionary *_dictionary;
}

@property (nonatomic, copy) NSString *basePath;

@property (nonatomic, copy) NSString *kvName;

@property (nonatomic, strong) CCKV *kv;

@property (nonatomic, copy) NSString *mmkvName;

@property (nonatomic, strong) MMKV *mmkv;

@property (nonatomic, copy) NSString *cryptKeyString;

@property (nonatomic, assign) int32_t loopCnt;

@property (nonatomic, strong) NSMutableArray *intKeys;

@property (nonatomic, strong) NSMutableArray *doubleKeys;

@property (nonatomic, strong) NSMutableArray *strKeys;

@property (nonatomic, strong) NSMutableArray *numbers;

@property (nonatomic, strong) NSMutableArray *strings;

@property (nonatomic, strong) YZHUIExcelView *excelView;


@property (strong, nonatomic) UITextField *cryptKeyTextField;

@property (strong, nonatomic) UITextField *loopCntTextField;
@property (strong, nonatomic) UISegmentedControl *TestOptionSegment;
@property (strong, nonatomic) UIView *containerView;

@property (strong, nonatomic) UITextView *otherInfoView;

@property (nonatomic, strong) NSMutableArray<NSMutableArray*> *excelData;

@property (nonatomic, assign) NSTestOption testOption;

@end

static inline NSArray<NSString*>* _propertiesForClass(Class cls)
{
    uint32_t count = 0;
    objc_property_t *properties = class_copyPropertyList(cls, &count);
    NSMutableArray *list = [NSMutableArray new];
    for (uint32_t i = 0; i < count; ++i) {
        @autoreleasepool {
            objc_property_t property = properties[i];
            const char *nameStr = property_getName(property);
            NSString *name = [NSString stringWithUTF8String:nameStr];
            
            const char *attr = property_getAttributes(property);
            NSString *attrName = [NSString stringWithUTF8String:attr];
            
            NSLog(@"attrName=%@",attrName);
            //只保护property的，分类动态联合的不进行编码
            NSString *contains = [NSString stringWithFormat:@"V_%@",name];
            if ([attrName containsString:contains]) {
//                NSLog(@"name=%@",name);
                [list addObject:name];
            }
        }
    }
    if (properties) {
        free(properties);
    }
    return list;//[list copy];
}

bool _keyEqual(void *target, CCU64Ptr value1, CCU64Ptr value2)
{
    return value1 == value2;
}

bool _valueEqual(void *target, CCU64Ptr value1, CCU64Ptr value2)
{
    return value1 == value2;
}

CCHashCode _hash(void *target, CCU64Ptr key)
{
    return key;
}

bool _HashTableEnumerator(PCCHashTable_S ht, CCU64Ptr key, CCU64Ptr value)
{
    NSLog(@"-----key=%@,value=%@",@(key),@(value));
    return true;
}


bool _arrayEnumerator(PCCArray_S array, CCU64Ptr value, CCIndex index)
{
    NSLog(@"-----idx=%@,value=%@",@(index),@(value));
    return true;
}


@implementation ViewController
{
    CGFloat *_ptr_double;
    int64_t *_ptr_integer;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
//    return;
//    [self _testMapBuffer];
//    return;
    
//    [self _testDictionary];
//
//    return;
    
//    [self _testProperties];
//    [self _testSharedPtr];
    
//    [self _testCoder];
//    return;
//
//    [self _setupDefaultData];
    
//    [self _testAccount];
    
//    [self kvBaselineTest:10000];
    
//    [self _testCryptor];
    
//    [self _setupChildView];
    
//    [self _startTest:self.loopCnt testOption:self.testOption];
}

- (void)_testMapBuffer
{
    NSString *path = [YZHKVUtils applicationDocumentsDirectory:@"testMapBuffer"];
    NSLog(@"path=%@",path);
    NSData *data = [NSData dataWithContentsOfFile:path];
    NSLog(@"data=%@",data);

    CCFileMap *fileMap =new CCFileMap(path.UTF8String);
    
    int mode = CC_F_READ;

    fileMap->open(CC_F_RDWR);
    
//    fileMap->update(0, 100);
//    NSMutableData *data = [NSMutableData dataWithCapacity:10];
//    memcpy((void*)data.bytes, ptr, 10);
//    NSLog(@"data=%@",data);
    
    CCBuffer *b1 = fileMap->createMapBuffer(0, 100, CC_F_RDWR);
//    CCBuffer *b2 = fileMap->createMapBuffer(b1->bufferSize(), 4, CC_F_RDWR);
    b1->writeByte(8);
    b1->writeByte(9);
//    exit(1);
//    b1->seekTo(0);
//    uint8_t a = b1->readByte();
//    NSLog(@"a=%@",@(a));
    
//    b2->seekTo(4097);
//    b2->writeByte(2);
//    b2->seekTo(0);
//    uint8_t b = b2->readByte();
//    NSLog(@"b=%@",@(b));
    
//    fileMap->destroyMapBuffer(b1);
//    fileMap->destroyMapBuffer(b2);
//
//    fileMap->close();
}

- (void)_testDictionary
{
    CCDictionary *ht = nullptr;
    
    CCU64Ptr *keys = nullptr;
    CCU64Ptr *vals = nullptr;

    if (self->_dictionary == nullptr) {
        struct CCDictionaryKeyFunc keyFunc = {NULL, NULL, _keyEqual, _hash};
        struct CCDictionaryValueFunc valueFunc = {NULL, NULL, _valueEqual};
        self->_dictionary = CCDictionaryCreate(NULL, NULL, NULL, 0, &keyFunc, &valueFunc, CCHashTableStyleLinear);
        
        ht = CCDictionaryCreate(NULL, NULL, NULL, 0, &keyFunc, &valueFunc, CCHashTableStyleLinear);
    }
    
    CCCount capacity = 1000000;
    CCHashTableSetCapacity(self->_dictionary, capacity);
    
    keys = (CCU64Ptr *)calloc(capacity, sizeof(CCU64Ptr));
    vals = (CCU64Ptr *)calloc(capacity, sizeof(CCU64Ptr));
    
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    CCIndex j = 0;
    for (CCIndex i = 0; i < capacity; ++i) {
        CCU64Ptr key = arc4random();
        CCU64Ptr val = arc4random();
        if (![dict objectForKey:@(key)]) {
            [dict setObject:@(val) forKey:@(key)];
            keys[j] = key;
            vals[j] = val;
            ++j;
            bool ret = CCHashTableAddValue(self->_dictionary, key, val);
            if (ret == false) {
                NSLog(@"add failed,key=%@,val=%@",@(key),@(val));
            }
        }
    }
    
    __block CCU64Ptr k = 0;
    [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        CCU64Ptr keyTmp = [key longValue];
        CCU64Ptr valTmp = [obj longValue];
        CCU64Ptr val = 0;
        CCHashTableGetValueOfKeyIfPresent(self->_dictionary, keyTmp, &val);
        if (val != valTmp) {
            NSLog(@"keyT=%@,valT=%@,val=%@",@(keyTmp),@(valTmp),@(val));
            *stop = YES;
            k = keyTmp;
        }
    }];
    
    
    CCU64Ptr val = 0;
    if (k) {
        CCHashTableGetValueOfKeyIfPresent(self->_dictionary, k, &val);
    }
    
    
//    NSMutableDictionary *a = [NSMutableDictionary dictionaryWithCapacity:capacity];
    NSMutableDictionary *a = [NSMutableDictionary dictionary];
//    CCHashTableSetCapacity(ht, capacity);

    
    NSLog(@"dict=");
    [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
            [a setObject:obj forKey:key];
        }];
    }];
    
    
    NSLog(@"hashtable=");
    [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (CCIndex i = 0; i < j; ++i) {
            CCU64Ptr keyTmp = keys[i];
            CCU64Ptr valTmp = vals[i];
//            CCHashTableAddValue(ht, keyTmp, valTmp);
            CCHashTableSetValue(ht, keyTmp, valTmp);
        }
//        [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
//            CCU64Ptr keyTmp = [key longValue];
//            CCU64Ptr valTmp = [obj longValue];
//            CCHashTableAddValue(ht, keyTmp, valTmp);
////            CCHashTableAddValue(ht, keyt, objt);
//        }];
    }];
    
    free(keys);
    free(vals);
    
    
//    CCU64Ptr base = 100;
//    CCHashTableAddValue(self->_dictionary, 1, base + 1);
//    CCHashTableAddValue(self->_dictionary, 2, base + 2);
//    CCHashTableAddValue(self->_dictionary, 3, base + 3);
//    CCHashTableAddValue(self->_dictionary, 4, base + 4);
//    CCHashTableAddValue(self->_dictionary, 5, base + 5);
//    CCHashTableAddValue(self->_dictionary, 14, base + 14);
//    CCHashTableAddValue(self->_dictionary, 27, base + 27);
    
//    CCHashTableAddValue(self->_dictionary, 100, base + 100);
    
//    CCHashTableEnumerate(self->_dictionary, _HashTableEnumerator);
    
    
    NSLog(@"usedCnt=%@,dict.cnt=%@",@(CCHashTableGetCount(self->_dictionary)),@(dict.count));
    
    CCHashTableDeallocate(self->_dictionary);
    self->_dictionary = NULL;
}

- (void)_testProperties
{
    _propertiesForClass([Account class]);
}

- (void)_testSharedPtr
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    
//    CCCodeData codeKeyData(keyData);
//    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, nullptr, CCCryptModeECB);
    shared_ptr<Test> src = make_shared<Test>();
    NSLog(@"src.get=%p",src.get());
    shared_ptr<Test> old;
    src.swap(old);
    NSLog(@"src.get=%p,old.get=%p",src.get(),old.get());
    src = make_shared<Test>();
    NSLog(@"src2.get=%p",src.get());
    src.reset();
    old.reset();
    NSLog(@"end");
}

- (NSMutableArray*)intKeys
{
    if (_intKeys == nil) {
        _intKeys = [NSMutableArray arrayWithCapacity:self.loopCnt];
    }
    return _intKeys;
}

- (NSMutableArray*)doubleKeys
{
    if (_doubleKeys == nil) {
        _doubleKeys = [NSMutableArray arrayWithCapacity:self.loopCnt];
    }
    return _doubleKeys;
}

- (NSMutableArray*)strKeys {
    if (_strKeys == nil) {
        _strKeys = [NSMutableArray arrayWithCapacity:self.loopCnt];
    }
    return _strKeys;
}

- (NSMutableArray*)numbers
{
    if (_numbers == nil) {
        _numbers = [NSMutableArray arrayWithCapacity:self.loopCnt];
    }
    return _numbers;
}

- (NSMutableArray*)strings
{
    if (_strings == nil) {
        _strings = [NSMutableArray arrayWithCapacity:self.loopCnt];
    }
    return _strings;
}

- (NSMutableArray<NSMutableArray*>*)excelData
{
    if (_excelData == nil) {
        _excelData = [NSMutableArray array];
    }
    return _excelData;
}

- (NSString*)cryptKeyString{
    _cryptKeyString = self.cryptKeyTextField.text;
    return _cryptKeyString;
}

static inline BOOL objectIsKindOfClass(id object, Class cls)
{
    Class objCls = [object class];
    while (objCls) {
        objCls = class_getSuperclass(objCls);
        if (objCls == cls) {
            return YES;
        }
    }
    return NO;
}

- (void)_setupDefaultData
{
    NSMutableArray *firstRow = [NSMutableArray array];
    [firstRow addObject:@"测试Kit/结果"];
    [firstRow addObject:@"空加载(ms)"];
    [firstRow addObject:@"数据加载(ms)"];
    [firstRow addObject:@"写(ms)"];
    [firstRow addObject:@"读(ms)"];
    [firstRow addObject:@"删除(ms)"];
    [firstRow addObject:@"清除(ms)"];
    [firstRow addObject:@"更新密钥(ms)"];
    [self.excelData addObject:firstRow];
    NSMutableArray *kvRow = [NSMutableArray array];
    [kvRow addObject:@"KV"];
    [kvRow addObject:@""];
    [kvRow addObject:@""];
    [kvRow addObject:@""];
    [kvRow addObject:@""];
    [kvRow addObject:@""];
    [kvRow addObject:@""];
    [kvRow addObject:@""];
    [self.excelData addObject:kvRow];
    
    NSMutableArray *mmkvRow = [NSMutableArray array];
    [mmkvRow addObject:@"MMKV"];
    [mmkvRow addObject:@""];
    [mmkvRow addObject:@""];
    [mmkvRow addObject:@""];
    [mmkvRow addObject:@""];
    [mmkvRow addObject:@""];
    [mmkvRow addObject:@""];
    [mmkvRow addObject:@""];
    [self.excelData addObject:mmkvRow];
    
    NSMutableArray *defaultRow = [NSMutableArray array];
    [defaultRow addObject:@"NSUserDefaults"];
    [defaultRow addObject:@""];
    [defaultRow addObject:@""];
    [defaultRow addObject:@""];
    [defaultRow addObject:@""];
    [defaultRow addObject:@""];
    [defaultRow addObject:@""];
    [defaultRow addObject:@""];
    [self.excelData addObject:defaultRow];
    
    self.loopCnt = 10000;
    self.testOption = NSTestOptionInt;
    self.kvName = @"com.kv";
    self.mmkvName = @"com.wx.mmkv";
    self.basePath = [YZHKVUtils applicationDocumentsDirectory:@"KV"];
}

- (UIView*)_inputViewWithFrame:(CGRect)frame tips:(NSString*)tips placeHolder:(NSString*)placeHolder
{
    
    UIView *input = [UIView new];
    input.frame =frame;
    
    UILabel *label = [UILabel new];
    label.tag = 1;
    label.text = tips;
    [label sizeToFit];
    label.frame = CGRectMake(10, 0, label.width, input.height);
    [input addSubview:label];
    
    UITextField *textField = [UITextField new];
    textField.tag = 2;
    textField.frame = CGRectMake(label.right + 5, 0, input.width - label.right - 10, input.height);
    textField.placeholder = placeHolder;
    [input addSubview:textField];
    
    textField.layer.borderWidth = 1.0;
    textField.layer.borderColor = GRAY_COLOR.CGColor;
    textField.layer.cornerRadius = 5.0;

    return input;
}

- (UIView*)_optionViewWithFrame:(CGRect)frame tips:(NSString*)tips
{
    
    UIView *input = [UIView new];
    input.frame =frame;
    
    UILabel *label = [UILabel new];
    label.tag = 1;
    label.text = tips;
    [label sizeToFit];
    label.frame = CGRectMake(10, 0, label.width, input.height);
    [input addSubview:label];
    
    
    UISegmentedControl *segmenedControl = [[UISegmentedControl alloc] initWithItems:@[@"Int",@"Double",@"String"]];
    segmenedControl.tag = 2;
    segmenedControl.frame = CGRectMake(label.right + 5, 0, input.width - label.right - 10, input.height);
    segmenedControl.selectedSegmentIndex = 0;
    [segmenedControl addTarget:self action:@selector(_optionChangedAction:) forControlEvents:UIControlEventValueChanged];
    [input addSubview:segmenedControl];
    
    return input;
}

- (UIButton*)_buttonWithFrame:(CGRect)frame title:(NSString*)title action:(SEL)action
{
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.frame = frame;
    [button setTitle:title forState:UIControlStateNormal];
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    button.layer.borderColor = button.titleLabel.textColor.CGColor;
    button.layer.borderWidth = 1.0;
    button.layer.cornerRadius = 5.0;
    return button;
}


- (void)_setupChildView
{
    
    int pageSize = getpagesize();
    
    int64_t tmp = TYPE_NOT(pageSize - 1);
    
    int64_t size = 132029352;
    NSLog(@"tmp=%ld, size=%ld", tmp, (size & tmp) + pageSize);
    
    self.view.backgroundColor = [UIColor whiteColor];
    
    CGFloat x = 10;
    CGFloat y = 40;
    CGFloat w = SCREEN_WIDTH - 2 * x;
    CGFloat h = 40;
    UIView *inputView = [self _inputViewWithFrame:CGRectMake(x, y, w, h) tips:@"密钥：" placeHolder:@"有则密文，无则明文"];
    [self.view addSubview:inputView];
    self.cryptKeyTextField = [inputView viewWithTag:2];
    
    y = inputView.bottom + 10;
    inputView = [self _inputViewWithFrame:CGRectMake(x, y, w, h) tips:@"读写次数：" placeHolder:@"10000"];
    [self.view addSubview:inputView];
    self.loopCntTextField = [inputView viewWithTag:2];
    
    y = inputView.bottom + 10;
    inputView = [self _optionViewWithFrame:CGRectMake(x, y, w, h) tips:@"测试选项："];
    [self.view addSubview:inputView];
    self.TestOptionSegment = [inputView viewWithTag:2];
    
    y = inputView.bottom + 10;
    self.excelView = [[YZHUIExcelView alloc] initWithFrame:CGRectMake(x, y, w, 200)];
    self.excelView.delegate = self;
    self.excelView.lockIndexPath = [NSIndexPath indexPathForExcelRow:1 excelColumn:1];
    self.excelView.backgroundColor = self.containerView.backgroundColor;
    [self.view addSubview:self.excelView];
    
    y = self.excelView.bottom + 10;
    w = (inputView.width - 10)/2;

    UIButton *button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"重置" action:@selector(_resetAction:)];
    [self.view addSubview:button];
    
    x = button.right + 10;
    button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"重启" action:@selector(_rebootAction:)];
    [self.view addSubview:button];
    
    x = self.excelView.left;
    y = button.bottom + 10;
    button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"读" action:@selector(_readDataAction:)];
    [self.view addSubview:button];
    
    x = button.right + 10;
    button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"写" action:@selector(_writeData:)];
    [self.view addSubview:button];
    
    x = self.excelView.left;
    y = button.bottom + 10;
    button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"清除" action:@selector(_clearAction:)];
    [self.view addSubview:button];
    
    x = button.right + 10;
    button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"删除" action:@selector(_deleteAction:)];
    [self.view addSubview:button];
    
    x = self.excelView.left;
    y = button.bottom + 10;
    button = [self _buttonWithFrame:CGRectMake(x, y, w, h) title:@"修改密码" action:@selector(_updateCryptKeyAction:)];
    [self.view addSubview:button];
    
    y = button.bottom + 10;
    w = self.excelView.width;
    h = self.view.height - y - 34;
    self.otherInfoView = [UITextView new];
    self.otherInfoView.frame = CGRectMake(x, y, w, h);
    self.otherInfoView.backgroundColor = GROUP_TABLEVIEW_BG_COLOR;
    [self.view addSubview:self.otherInfoView];
    
    self.loopCntTextField.text = NEW_STRING_WITH_FORMAT(@"%d",self.loopCnt);
    self.TestOptionSegment.selectedSegmentIndex = self.testOption;
}

- (NSString*)_textFromTime:(CGFloat)time
{
    return NEW_STRING_WITH_FORMAT(@"%.5f",time);
}

-(CCKV*)kv{
    if (_kv == nil) {
        CGFloat t = 0;
        NSString *fileName = self.kvName;
        NSString *filePath = [self.basePath stringByAppendingPathComponent:fileName];
        
        NSLog(@"filePath=%@",filePath);
        self.otherInfoView.text = filePath;
        NSExcelColumnTag tag = [YZHKVUtils checkFileExistsAtPath:filePath] ? NSExcelColumnTagLoadInit : NSExcelColumnTagFirstInit;
        NSLog(@"initKV");
        NSData *cryptKey = [self.cryptKeyString dataUsingEncoding:NSUTF8StringEncoding];
        [YZHMachTimeUtils recordPointWithText:@"start"];
        if (self.cryptKeyString.length > 0) {
            t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
                _kv = [[CCKV alloc] initWithName:fileName path:self.basePath cryptKey:cryptKey];
            }];
        }
        else {
            t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
                _kv = [[CCKV alloc] initWithName:fileName path:self.basePath cryptKey:nil];
            }];
        }
        [YZHMachTimeUtils recordPointWithText:@"end"];
        
        NSLog(@"error=%@",[_kv lastError]);
        
        NSLog(@"text=%f",t);

        NSLog(@"KV.cnt=%@",@(_kv.allEntries.count));
        
        NSString *text = [self _textFromTime:t];
        NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
        [row setObject:text atIndexedSubscript:tag];
        [self.excelView reloadData];
    }
    return _kv;
}

- (MMKV*)mmkv
{
    if (_mmkv == nil) {
        CGFloat t = 0;
        NSString *fileName = self.mmkvName;
        [MMKV setMMKVBasePath:self.basePath];
        NSString *filePath = [[MMKV mmkvBasePath] stringByAppendingPathComponent:fileName];
        NSLog(@"filePath=%@",filePath);
        NSExcelColumnTag tag = [YZHKVUtils checkFileExistsAtPath:filePath] ? NSExcelColumnTagLoadInit : NSExcelColumnTagFirstInit;

        if (self.cryptKeyString.length == 0) {
            t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
                _mmkv = [MMKV mmkvWithID:fileName];
            }];
        }
        else {
            NSData *cryptKey = [self.cryptKeyString dataUsingEncoding:NSUTF8StringEncoding];
            t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
                _mmkv = [MMKV mmkvWithID:fileName cryptKey:cryptKey];
            }];
        }
        
        NSLog(@"MMKV.cnt=%@",@(_mmkv.allKeys.count));


        NSString *text = [self _textFromTime:t];
        NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
        [row setObject:text atIndexedSubscript:tag];
        [self.excelView reloadData];
    }
    return _mmkv;
}


- (void)_testCoder
{
    NSDictionary *dict = [[NSBundle mainBundle] infoDictionary];
    NSLog(@"dict=%@",dict);
    
    NSInteger cnt = 10000;

    NSLog(@"json cost:");
    [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (NSInteger i = 0; i < cnt; ++i) {
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dict options:0 error:NULL];
//            id obj = [NSJSONSerialization JSONObjectWithData:jsonData options:NSJSONReadingMutableLeaves error:NULL];
        }
    }];
    
    
    CCMutableCodeData *codeData = new CCMutableCodeData();
    
    __block NSDictionary *decodeDict = nil;
    
    NSLog(@"encode cost:");
    [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (NSInteger i = 0; i < cnt; ++i) {
            codeData->truncateTo(0);
            encodeObjectToTopSuperClassIntoCodeData(dict, NULL, codeData, NULL);
//            NSLog(@"data=%@",codeData->copyData());
//            NSLog(@"dtlen=%lld",codeData->dataSize());
//            decodeDict = decodeObjectFromBuffer(codeData->bytes(), codeData->currentSeek(), NULL, NULL, NULL);
//            NSLog(@"decodeDict=%@",decodeDict);
        }
    }];
    
    NSData *data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableDictionary *mDict = [NSMutableDictionary dictionary];
    [mDict setObject:data forKey:@"data"];
    NSLog(@"data=%@",data);
    codeData->truncateTo(0);
    encodeObjectToTopSuperClassIntoCodeData(mDict, NULL, codeData, NULL);
    NSDictionary *t = decodeObjectFromBuffer(codeData->bytes(), codeData->currentSeek(), NULL, NULL, NULL);
    NSLog(@"t=%@",t);
    
    
    if (codeData) {
        delete codeData;
    }
}

-(void)_testAccount
{
#if 0
    //Account
    Account *accout = [Account new];
    accout.uin = 5050023337659471103;
    accout.accid = @"accid";
    accout.token = @"token";
    accout.session = @"session";
    accout.cookie = nil;//[@"cookie" dataUsingEncoding:NSUTF8StringEncoding];
    accout.autoAuthKey = [@"autoAuthKey" dataUsingEncoding:NSUTF8StringEncoding];
    accout.appKey = @"appKey";
    accout.rangeStart = 1234;
    accout.watershed = 56789;
    accout.height = 180.5;
    accout.weight = 65.8;
    
    accout.ext = 430822188812180923;
    accout.name = @"name";
    
    //X
    X *x = [X new];
    x.x = @"201907291824202ccbd1ce844a4279483f097d78fd2e7a01a09d71b45d0d93";
    x.xx = @"hAqDtdVguWqL0EES8GIEEKaG8E0NN46CIqpNxtRap5g|82073FF2-3A72-4D6D-8613-202DF4820BB2|201907291824202ccbd1ce844a4279483f097d78fd2e7a01a09d71b45d0d93";
    x.x_y = [NSString stringWithFormat:@"%@-%@",x.x,x.xx];

    //TestObj
    TestObj *test = [TestObj new];
    test.a = (NSInteger)156561289410101;
    test.b = 15656128.9410101;
    test.c = 156568.88;
    test.d = @"中华民族有着悠久的历史";
    test.e = [@"从遥远的古代起，中华各民族人民的祖先就劳动、生息、繁衍在我们祖国的土地上，共同为中华文明和建立统一的多民族国家贡献着自己的才智" dataUsingEncoding:NSUTF8StringEncoding];
    
    test.f = @[@"1",@(2),@{@"key":@"value"}];
    
    test.g = @{@"k1":@"v1", @"k2":@"v2", @(1):@(2), @"1":@"2"};
    test.x = [X new];
    test.x.x = @"花容月貌";
    test.x.xx = @"古来圣贤皆寂寞，惟有饮者留其名";
    test.x.x_y = @"人生得意须尽欢，莫使金樽空对月";
    
    
    NSArray *list = @[accout,x,test];
    NSMutableArray *result = [NSMutableArray array];
    [result addObjectsFromArray:list];
    
    NSLog(@"start KV storage:cnt=%ld",result.count);
    [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (id obj in result) {
            NSString *key = NSStringFromClass([obj class]);
            [self.kv setObject:obj forKey:key];
        }
    }];
    NSLog(@"end KV storage");
#else
    __block id acc = nil;
    __block id x = nil;
    __block id testObj = nil;
    self.kv;
    NSLog(@"get account");
    [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        acc = [self.kv getObjectForKey:@"Account"];
        x = [self.kv getObjectForKey:@"X"];
        testObj = [self.kv getObjectForKey:@"TestObj"];
    }];
    NSLog(@"acc=%@",acc);
    NSLog(@"x=%@",x);
    NSLog(@"testObj=%@",testObj);
    
    float f = 156568.88;
    int32_t int32f = Int32FromFloat(f);
    int64_t int64f = Int64FromFloat(f);
    float fc = FloatFromInt32(int32f);
    NSLog(@"float=%.5f,int32f=%d,fc=%.5f,fn=%@,int64f=%lld",f,int32f, fc,@(f),int64f);
    NSLog(@"end");
//    NSData *cryptKey = [@"yuanzhen" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *cryptKey = nil;
    [self.kv updateCryptKey:cryptKey];
#endif
}





-(void)_testCryptor
{
    [self _testECB];
    [self _testCBC];
    [self _testOFB];
    [self _testCFB];
    [self _testCFB1];
    [self _testCFB8];
}

- (void)_testECB
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    
    CCCodeData codeKeyData(keyData);
    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, nullptr, CCCryptModeECB);
    
    NSString *text = @"12345";//@"12345";//@"1234567890123456";//@"12345678901234561234567890123456";
    NSData *input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    CCMutableCodeData inputCodeData(4096);
    inputCodeData.writeData(input);

    shared_ptr<CCMutableCodeData> output = make_shared<CCMutableCodeData>();
    
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), output.get());
    NSString *plainText = [[NSString alloc] initWithBytes:output->bytes() length:(NSUInteger)output->dataSize() encoding:NSUTF8StringEncoding];
    NSLog(@"ECB.plainText.1=%@,same=%@",plainText,@([plainText isEqualToString:text]));
    
    text = @"1234567890123456";//@"12345678901234561234567890123456";
    input = [text dataUsingEncoding:NSUTF8StringEncoding];
    

    inputCodeData.truncateTo(0);
    inputCodeData.writeData(input);
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), output.get());
    plainText = [[NSString alloc] initWithBytes:output->bytes() length:(NSUInteger)output->dataSize() encoding:NSUTF8StringEncoding];
    NSLog(@"ECB.plainText.2=%@,same=%@",plainText,@([plainText isEqualToString:text]));
    
    
    text = @"12345678901234561234567890123456789";
    input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    inputCodeData.truncateTo(0);
    inputCodeData.writeData(input);
    
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), output.get());
    
    plainText = [[NSString alloc] initWithBytes:output->bytes() length:(NSUInteger)output->dataSize() encoding:NSUTF8StringEncoding];
    NSLog(@"ECB.plainText.3=%@,same=%@",plainText,@([plainText isEqualToString:text]));
    
    
}

- (void)_testCBC
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    
    CCCodeData codeKeyData(keyData);
    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, &codeKeyData, CCCryptModeCBC);


    NSString *text = @"12345";//@"1234567890123456";//@"12345678901234561234567890123456";
    NSData *input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    CCMutableCodeData inputCodeData(4096);
    inputCodeData.writeData(input);
    shared_ptr<CCMutableCodeData> output = make_shared<CCMutableCodeData>();
    
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), output.get());
    NSString *plainText = [[NSString alloc] initWithBytes:output->bytes() length:(NSUInteger)output->dataSize() encoding:NSUTF8StringEncoding];
    
    NSLog(@"CBC.plainText.1=%@,same=%@",plainText,@([plainText isEqualToString:text]));
    
    
    text = @"1234567890123456";//@"12345678901234561234567890123456";
    input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    inputCodeData.truncateTo(0);
    inputCodeData.writeData(input);
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), output.get());
    plainText = [[NSString alloc] initWithBytes:output->bytes() length:(NSUInteger)output->dataSize() encoding:NSUTF8StringEncoding];
    NSLog(@"CBC.plainText.2=%@,same=%@",plainText,@([plainText isEqualToString:text]));
    
    
    text = @"12345678901234561234567890123456789";
    input = [text dataUsingEncoding:NSUTF8StringEncoding];

    inputCodeData.truncateTo(0);
    inputCodeData.writeData(input);
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), output.get());
    plainText = [[NSString alloc] initWithBytes:output->bytes() length:(NSUInteger)output->dataSize() encoding:NSUTF8StringEncoding];
    NSLog(@"CBC.plainText.3=%@,same=%@",plainText,@([plainText isEqualToString:text]));
    
}

- (void)_testOFB
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    
    NSLog(@"OFB:");
    
    CCCodeData codeKeyData(keyData);
    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, &codeKeyData, CCCryptModeOFB);

    
    [self _testFlowCrypt1];
    [self _testFlowCrypt2];
}

- (void)_testCFB
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    NSLog(@"CFB:");
    CCCodeData codeKeyData(keyData);
    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, &codeKeyData, CCCryptModeCFB);

    [self _testFlowCrypt1];
    [self _testFlowCrypt2];
}

- (void)_testCFB1
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    NSLog(@"CFB1:");
    CCCodeData codeKeyData(keyData);
    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, &codeKeyData, CCCryptModeCFB1);
    
    [self _testFlowCrypt1];
    [self _testFlowCrypt2];
}

- (void)_testCFB8
{
    NSString *key = @"1234567890123456";
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    NSLog(@"CFB8:");
    CCCodeData codeKeyData(keyData);
    _cryptor = make_shared<CCAESCryptor>(&codeKeyData, CCAESKeyType128, &codeKeyData, CCCryptModeCFB8);
    
    [self _testFlowCrypt1];
    [self _testFlowCrypt2];
}

- (void)_testFlowCrypt1
{
    NSString *text = @"12345678901234561234567890123456";
    NSMutableString *allInputText = [[NSMutableString alloc] initWithString:text];
    
    NSData *input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    CCMutableCodeData inputCodeData(1024);
    inputCodeData.writeData(input);
    
    shared_ptr<CCMutableCodeData> output = make_shared<CCMutableCodeData>();
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    
    NSData *cipherData = output->copyData();
    
    self->_cryptor->reset();
    
    int64_t outSize = output.get()->dataSize();
    CCMutableCodeData plainData(outSize);
    self->_cryptor->crypt(CCCryptOperationDecrypt, output.get(), &plainData);
    NSString *plainText = [[NSString alloc] initWithBytes:plainData.bytes() length:(NSUInteger)plainData.dataSize() encoding:NSUTF8StringEncoding];
    NSLog(@"plainText.1=%@,isSame=%@",plainText,@([plainText isEqualToString:allInputText]));

    
    output->truncateTo(0);
    //在解密的基础上进行加密
    text = @"123456789012345689";
    [allInputText appendString:text];
    input = [text dataUsingEncoding:NSUTF8StringEncoding];
    inputCodeData.truncateTo(0);
    inputCodeData.appendWriteData(input);
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    NSData *cipherData2 = output->copyData();
    
    NSMutableData *all = [NSMutableData dataWithData:cipherData];
    [all appendData:cipherData2];
    
    //重新解密全部
    self->_cryptor->reset();
    
    output->truncateTo(0);
    inputCodeData.truncateTo(0);
    inputCodeData.appendWriteData(all);
    
    self->_cryptor->crypt(CCCryptOperationDecrypt, &inputCodeData, output.get());
    NSData *p = output->copyData();
    plainText = [[NSString alloc] initWithData:p encoding:NSUTF8StringEncoding];
    NSLog(@"plainText.2=%@,isSame=%@",plainText,@([plainText isEqualToString:allInputText]));

}

-(void)_testFlowCrypt2
{
    self->_cryptor->reset();
    
    NSMutableData *all = [NSMutableData data];
    NSString *text = @"12345";//@"12345";//@"1234567890123456";//@"12345678901234561234567890123456";
    NSMutableString *allInputText = [[NSMutableString alloc] initWithString:text];
    NSData *input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    CCMutableCodeData inputCodeData(1024);
    inputCodeData.writeData(input);
    
    shared_ptr<CCMutableCodeData> output = make_shared<CCMutableCodeData>();
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    
    NSData *cipherData = output->copyData();
    [all appendData:cipherData];
    
    text = @"01389";//@"12345";//@"1234567890123456";//@"12345678901234561234567890123456";
    [allInputText appendString:text];
    input = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    output->truncateTo(0);
    inputCodeData.truncateTo(0);
    inputCodeData.appendWriteData(input);
    self->_cryptor->crypt(CCCryptOperationEncrypt, &inputCodeData, output.get());
    cipherData = output->copyData();
    
    [all appendData:cipherData];
    
    self->_cryptor->reset();
    
    output->truncateTo(0);
    inputCodeData.truncateTo(0);
    inputCodeData.appendWriteData(all);
    
    self->_cryptor->crypt(CCCryptOperationDecrypt, &inputCodeData, output.get());
    NSData *plainData = output->copyData();
    
    NSString *plainText = [[NSString alloc] initWithData:plainData encoding:NSUTF8StringEncoding];
    NSLog(@"all=%@,isSame=%@",plainText,@([plainText isEqualToString:allInputText]));
}






#pragma mark CCUIExcelViewDelegate
-(NSInteger)numberOfRowsInExcelView:(YZHUIExcelView *)excelView
{
    return self.excelData.count;
}
-(NSInteger)numberOfColumnsInExcelView:(YZHUIExcelView *)excelView
{
    return [[self.excelData objectAtIndex:0] count];
}

-(CGFloat)excelView:(YZHUIExcelView *)excelView heightForRowAtIndex:(NSInteger)rowIndex
{
    return 50;
}

-(CGFloat)excelView:(YZHUIExcelView *)excelView widthForColumnAtIndex:(NSInteger)columnIndex
{
    return 90;
}

-(UIView*)excelView:(YZHUIExcelView *)excelView excelCellForItemAtIndexPath:(NSIndexPath*)indexPath withReusableExcelCellView:(UIView *)reusableExcelCellView
{
    UIReuseCell *cell = (UIReuseCell*)reusableExcelCellView;
    if (cell == nil) {
        cell = [[UIReuseCell alloc] init];
    }
    cell.backgroundColor = excelView.backgroundColor;
    
    NSMutableArray *r = [self.excelData objectAtIndex:indexPath.excelRow];
    NSString *text = [r objectAtIndex:indexPath.excelColumn];
    cell.textLabel.text = text;
    cell.textLabel.numberOfLines = 0;
    // NEW_STRING_WITH_FORMAT(@"(%ld,%ld)",indexPath.excelRow,indexPath.excelColumn);
    return cell;
}






- (IBAction)_optionChangedAction:(UISegmentedControl *)sender {
    [self.view endEditing:YES];
    NSLog(@"selectedIdx=%ld",sender.selectedSegmentIndex);
}


- (IBAction)_resetAction:(UIButton *)sender {
    [self.view endEditing:YES];
    if (_kv) {
        [_kv close];
        [YZHKVUtils removeFileItemAtPath:[_kv filePath]];
        _kv = nil;
    }
    
    if (_mmkv) {
        [_mmkv close];
        [YZHKVUtils removeFileItemAtPath:[MMKV mmkvBasePath]];
        _mmkv = nil;
    }
}

- (IBAction)_rebootAction:(UIButton *)sender {
    [self.view endEditing:YES];

    if (_kv) {
        [_kv close];
        _kv = nil;        
    }
    if (_mmkv) {
        [_mmkv close];
        _mmkv = nil;
    }
    
    [self mmkv];

    [NSThread sleepForTimeInterval:1];
    [self kv];
    NSString *filePath = [self.kv filePath];
    NSString *mmkvPath = [[MMKV mmkvBasePath] stringByAppendingPathComponent:self.mmkvName];
    self.otherInfoView.text = [filePath stringByAppendingFormat:@"\n%@",mmkvPath];
}

- (IBAction)_readDataAction:(UIButton *)sender {
    [self.view endEditing:YES];
    
    if (self.testOption == NSTestOptionInt) {
        [self _mmkvBatchReadIntTest:self.loopCnt];
        
        [self _kvBatchReadIntTest:self.loopCnt];
        
        [self _defaultBatchReadIntTest:self.loopCnt];
    }
    else if (self.testOption == NSTestOptionDouble) {
        [self _mmkvBatchReadDoubleTest:self.loopCnt];
        
        [self _kvBatchReadDoubleTest:self.loopCnt];
        
        [self _defaultBatchReadDoubleTest:self.loopCnt];
    }
    else if (self.testOption == NSTestOptionString) {
        [self _mmkvBatchReadStringTest:self.loopCnt];
        
        [self _kvBatchReadStringTest:self.loopCnt];
        
        [self _defaultBatchReadStringTest:self.loopCnt];
    }
    
}

- (IBAction)_writeData:(UIButton *)sender {
    [self.view endEditing:YES];
    
    int32_t loopCnt = [self.loopCntTextField.text intValue];
    int32_t idx = (int32_t)self.TestOptionSegment.selectedSegmentIndex;
    if (loopCnt != self.loopCnt || idx != self.testOption) {
        self.loopCnt = loopCnt;
        self.testOption = (NSTestOption)idx;
        [self _startTest:loopCnt testOption:(NSTestOption)idx];
    }
    
    if (self.testOption == NSTestOptionInt) {
        
        [NSThread sleepForTimeInterval:0.1];
        [self _mmkvBatchWriteIntTest:self.loopCnt];
        
        [NSThread sleepForTimeInterval:0.1];
        [self _kvBatchWriteIntTest:self.loopCnt];
        
        [NSThread sleepForTimeInterval:0.1];
        [self _defaultBatchWriteIntTest:self.loopCnt];
    }
    else if (self.testOption == NSTestOptionDouble) {
        [NSThread sleepForTimeInterval:0.1];
        [self _mmkvBatchWriteDoubleTest:self.loopCnt];
        
        [NSThread sleepForTimeInterval:0.1];
        [self _kvBatchWriteDoubleTest:self.loopCnt];
        
        [NSThread sleepForTimeInterval:0.1];
        [self _defaultBatchWriteDoubleTest:self.loopCnt];
    }
    else if (self.testOption == NSTestOptionString) {
        [NSThread sleepForTimeInterval:0.1];
        [self _mmkvBatchWriteStringTest:self.loopCnt];
        
        [NSThread sleepForTimeInterval:0.1];
        [self _kvBatchWriteStringTest:self.loopCnt];
        
        [NSThread sleepForTimeInterval:0.1];
        [self _defaultBatchWriteStringTest:self.loopCnt];
    }
    
    NSLog(@"KV.cnt=%@,MMKV.cnt=%@",@(_kv.allEntries.count),@(_mmkv.allKeys.count));
}

- (void)_startTest:(int)loops testOption:(NSTestOption)testOption
{
    [self.strings removeAllObjects];
    [self.strKeys removeAllObjects];
    [self.intKeys removeAllObjects];
    
    
    if (testOption == NSTestOptionInt) {
        if (_ptr_integer) {
            int64_t *ptrTmp = (int64_t*)realloc(_ptr_integer, loops * sizeof(int64_t));
            if (ptrTmp == NULL) {
                free(_ptr_integer);
                _ptr_integer = NULL;
                return;
            }
            else {
                self->_ptr_integer = ptrTmp;
            }
        }
        
        if (_ptr_integer == NULL) {
            _ptr_integer = (int64_t*)calloc(loops, sizeof(int64_t));
            if (_ptr_integer == NULL) {
                return;
            }
        }
        
        
        for (int32_t index = 0; index < loops; index++) {
            NSString *intKey = [NSString stringWithFormat:@"%d", index];
            [self.intKeys addObject:intKey];
            
            _ptr_integer[index] = arc4random();//* arc4random();
        }
    }
    else if (testOption == NSTestOptionDouble) {
        if (_ptr_double) {
            CGFloat *ptrTmp = (CGFloat*)realloc(self->_ptr_double, loops * sizeof(CGFloat));
            if (ptrTmp == NULL) {
                free(_ptr_double);
                _ptr_double = NULL;
                return;
            }
            else {
                self->_ptr_double = ptrTmp;
            }
        }
        
        if (_ptr_double == NULL) {
            _ptr_double = (CGFloat*)calloc(loops, sizeof(CGFloat));
            if (_ptr_double == NULL) {
                return;
            }
        }
        
        for (int32_t index = 0; index < loops; index++) {
            NSString *doubleKeys = [NSString stringWithFormat:@"double-%d", index];
            [self.doubleKeys addObject:doubleKeys];
            
            _ptr_double[index] = drand48() * arc4random() * arc4random();
        }
    }
    else if (testOption == NSTestOptionString) {
        for (int32_t index = 0; index < loops; index++) {
            NSString *str = [NSString stringWithFormat:@"%s-%d", __FILE__, index];
            [self.strings addObject:str];
            
            NSString *strKey = [NSString stringWithFormat:@"str-%d", index];
            [self.strKeys addObject:strKey];
        }
    }
}

- (void)_kvBatchWriteIntTest:(int32_t)loops
{
    NSLog(@"kv write int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            int64_t tmp = self->_ptr_integer[index];
            NSString *intKey = self.intKeys[index];
            [self.kv setInteger:tmp forKey:intKey];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}

- (void)_mmkvBatchWriteIntTest:(int32_t)loops
{
    NSLog(@"mmkv write int %d times, cost:", loops);
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            int64_t tmp = self->_ptr_integer[index];
            NSString *intKey = self.intKeys[index];
            [self.mmkv setInt64:tmp forKey:intKey];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}


- (void)_defaultBatchWriteIntTest:(int32_t)loops
{
    NSLog(@"default write int %d times, cost:", loops);
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            int64_t tmp = self->_ptr_integer[index];
            NSString *intKey = self.intKeys[index];
            [[NSUserDefaults standardUserDefaults] setInteger:tmp forKey:intKey];
        }
        [[NSUserDefaults standardUserDefaults] synchronize];
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagUserDefault];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}

- (void)_kvBatchReadIntTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"kv read int %d times, cost:", loops);

    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        
        for (int index = 0; index < loops; index++) {
            @autoreleasepool {
                NSString *intKey = self.intKeys[index];
//                int64_t OKVal = _ptr_integer[index];
                int64_t val = [self.kv getIntegerForKey:intKey];
//                if (val != OKVal) {
//                    self.otherInfoView.text = NEW_STRING_WITH_FORMAT(@"出现读取错误idx:%d val:%@,readVal=%@,", index,@(OKVal),@(val));
//                    break;
//                }
            }
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (void)_mmkvBatchReadIntTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"mmkv read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        
        for (int index = 0; index < loops; index++) {
            @autoreleasepool {
                NSString *intKey = self.intKeys[index];
//                int64_t OKVal = _ptr_integer[index];
                int64_t val = [self.mmkv getInt64ForKey:intKey];
//                if (val != OKVal) {
//                    self.otherInfoView.text = NEW_STRING_WITH_FORMAT(@"出现读取错误idx:%d val:%@,readVal=%@,", index,@(OKVal),@(val));
//                    break;
//                }
            }
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (void)_defaultBatchReadIntTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"default read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *intKey = self.intKeys[index];
            int64_t val = (int64_t)[[NSUserDefaults standardUserDefaults] integerForKey:intKey];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagUserDefault];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}


//double
- (void)_kvBatchWriteDoubleTest:(int32_t)loops
{
    NSLog(@"kv write int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            double tmp = self->_ptr_double[index];
            NSString *doubleKey = self.doubleKeys[index];
            [self.kv setDouble:tmp forKey:doubleKey];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}

- (void)_mmkvBatchWriteDoubleTest:(int32_t)loops
{
    NSLog(@"mmkv write int %d times, cost:", loops);
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            double tmp = self->_ptr_double[index];
            NSString *doubleKey = self.doubleKeys[index];
            [self.mmkv setDouble:tmp forKey:doubleKey];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}


- (void)_defaultBatchWriteDoubleTest:(int32_t)loops
{
    NSLog(@"default write int %d times, cost:", loops);
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            double tmp = self->_ptr_double[index];
            NSString *doubleKey = self.doubleKeys[index];
            [[NSUserDefaults standardUserDefaults] setDouble:tmp forKey:doubleKey];
        }
        [[NSUserDefaults standardUserDefaults] synchronize];
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagUserDefault];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}

- (void)_kvBatchReadDoubleTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"kv read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        
        for (int index = 0; index < loops; index++) {
            @autoreleasepool {
                NSString *key = self.doubleKeys[index];
                double val = [self.kv getDoubleForKey:key];
            }
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (void)_mmkvBatchReadDoubleTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"mmkv read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        
        for (int index = 0; index < loops; index++) {
            @autoreleasepool {
                NSString *key = self.doubleKeys[index];
                double val = [self.mmkv getDoubleForKey:key];
            }
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (void)_defaultBatchReadDoubleTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"default read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *key = self.doubleKeys[index];
            int64_t val = (int64_t)[[NSUserDefaults standardUserDefaults] doubleForKey:key];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagUserDefault];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}


//string
- (void)_kvBatchWriteStringTest:(int32_t)loops
{
    NSLog(@"kv write int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *tmp = self.strings[index];
            NSString *key = self.strKeys[index];
            [self.kv setObject:tmp forKey:key];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}

- (void)_mmkvBatchWriteStringTest:(int32_t)loops
{
    NSLog(@"mmkv write int %d times, cost:", loops);
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *tmp = self.strings[index];
            NSString *key = self.strKeys[index];
            [self.mmkv setObject:tmp forKey:key];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}


- (void)_defaultBatchWriteStringTest:(int32_t)loops
{
    NSLog(@"default write int %d times, cost:", loops);
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *tmp = self.strings[index];
            NSString *key = self.strKeys[index];
            [[NSUserDefaults standardUserDefaults] setObject:tmp forKey:key];
        }
        [[NSUserDefaults standardUserDefaults] synchronize];
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagUserDefault];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagWrite];
    [self.excelView reloadData];
}

- (void)_kvBatchReadStringTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"kv read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *key = self.strKeys[index];
            NSString *val = [self.kv getObjectForKey:key];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (void)_mmkvBatchReadStringTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"mmkv read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        
        for (int index = 0; index < loops; index++) {
            NSString *key = self.strKeys[index];
            [self.mmkv getObjectOfClass:[NSString class] forKey:key];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (void)_defaultBatchReadStringTest:(int32_t)loops
{
    if (_ptr_integer == NULL) {
        return;
    }
    NSLog(@"default read int %d times, cost:", loops);
    
    CGFloat t = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        for (int index = 0; index < loops; index++) {
            NSString *key = self.strKeys[index];
            int64_t val = (int64_t)[[NSUserDefaults standardUserDefaults] objectForKey:key];
        }
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagUserDefault];
    [row setObject:[self _textFromTime:t] atIndexedSubscript:NSExcelColumnTagRead];
    [self.excelView reloadData];
}

- (IBAction)_clearAction:(UIButton *)sender {
    
    CGFloat t1 = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        [self.kv clear:YES];
    }];
    
    [NSThread sleepForTimeInterval:1];
    
    CGFloat t2 = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        [self.mmkv clearAll];
    }];
    
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t1] atIndexedSubscript:NSExcelColumnTagClear];
    
    row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t2] atIndexedSubscript:NSExcelColumnTagClear];

    
    [self.excelView reloadData];
    
}

- (IBAction)_deleteAction:(UIButton *)sender {
    NSArray *mmkvKeys = [self.mmkv allKeys];
    
    NSArray *kvKeys = [[self.kv allEntries] allKeys];
    
    CGFloat t1 = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        [kvKeys enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [self.kv removeObjectForKey:obj];
        }];
    }];
    
    [NSThread sleepForTimeInterval:1];
    
    CGFloat t2 = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        [mmkvKeys enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [self.mmkv removeValueForKey:obj];
        }];
    }];
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t1] atIndexedSubscript:NSExcelColumnTagDelete];
    
    row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t2] atIndexedSubscript:NSExcelColumnTagDelete];
    
    
    [self.excelView reloadData];
    
    
}

- (IBAction)_updateCryptKeyAction:(UIButton *)sender {
    
    NSData *cryptKey = [self.cryptKeyString dataUsingEncoding:NSUTF8StringEncoding];
    
    __block BOOL OK = YES;
    CGFloat t1 = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        OK = [self.mmkv reKey:cryptKey];
    }];
    NSLog(@"OK=%@",@(OK));
    
    [NSThread sleepForTimeInterval:1];
    CGFloat t2 = [YZHMachTimeUtils elapsedMSTimeInBlock:^{
        [self.kv updateCryptKey:cryptKey];
    }];
    
    
    NSMutableArray *row = [self.excelData objectAtIndex:NSExcelRowTagMMKV];
    [row setObject:[self _textFromTime:t1] atIndexedSubscript:NSExcelColumnTagUpdateCryptKey];
    
    row = [self.excelData objectAtIndex:NSExcelRowTagCCKV];
    [row setObject:[self _textFromTime:t2] atIndexedSubscript:NSExcelColumnTagUpdateCryptKey];
    
    
    [self.excelView reloadData];

}





- (void)pri_testArray
{
    CCArrayContext ctx;
    ctx.retain = NULL;
    ctx.release = NULL;
    ctx.equal = NULL;
    
    CCArrayOption_U option;
    option.option = 0;
    
    PCCArray_S array = CCArrayCreate(NULL, NULL, 0, &ctx, option);
    CCArraySetCapacity(array, 8);
//    CCArrayAppendValue(array, 2);
//    CCArrayAppendValue(array, 3);
//    CCArrayAppendValue(array, 4);
//    CCArrayAppendValue(array, 5);

#if 1
    //insert
//    CCArrayAppendValue(array, 7);
//    CCArrayAppendValue(array, 8);
//
//    CCArrayInsertValueAtIndex(array, 0, 1);
//    CCArrayInsertValueAtIndex(array, 1, 6);
//    CCArrayInsertValueAtIndex(array, 1, 5);
//    CCArrayInsertValueAtIndex(array, 1, 4);
//    CCArrayInsertValueAtIndex(array, 1, 2);
//
//    CCArrayInsertValueAtIndex(array, 2, 3);
    
    CCArrayAppendValue(array, 1);
    CCArrayAppendValue(array, 2);
    CCArrayAppendValue(array, 3);
    CCArrayAppendValue(array, 4);
    CCArrayAppendValue(array, 5);
    CCArrayAppendValue(array, 6);
    CCArrayAppendValue(array, 7);
    CCArrayAppendValue(array, 8);
    
    NSLog(@"==end");
    CCArrayPrint(array, 8);
    
    
    //delete
//     CCArrayRemoveValueAtIndex(array, 2);
    CCArrayRemoveValuesAtRange(array, CCRangeMake(3,2));
//    CCArrayRemoveValuesAtRange(array, CCRangeMake(2,3));
    CCArrayPrint(array, 8);
#endif
#if 0
    CCArrayAppendValue(array, 3);
    CCArrayInsertValueAtIndex(array, 0, 2);
    CCArrayInsertValueAtIndex(array, 0, 1);
//    CCArrayInsertValueAtIndex(array, 0, 1);

    CCArrayAppendValue(array, 4);
    CCArrayAppendValue(array, 5);
    CCArrayAppendValue(array, 6);
    CCArrayAppendValue(array, 7);
    CCArrayAppendValue(array, 8);
    CCArrayPrint(array, 8);
    
    CCArrayRemoveValueAtIndex(array, 7);
    CCArrayPrint(array, 8);
#endif
    CCArrayDeallocate(array);
}



- (NSDictionary*)pri_createBPTreeData:(NSInteger)count
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    for (CCIndex i = 0; i < count; ++i) {
       uint32_t key = arc4random();
       uint32_t val = arc4random();
       if (![dict objectForKey:@(key)]) {
           [dict setObject:@(val) forKey:@(key)];
       }
    }
    return dict;
}

- (void)pri_testBPTreeIndex
{
    NSString *path = [YZHKVUtils applicationDocumentsDirectory:@"testBPIndex"];
    NSLog(@"path=%@",path);
//    std::string filePath =
    CCUInt16_t pageNodeCnt = 8;
    CCBTIndexSize_T indexSize = 8;
    
    NSDictionary *dict = [self pri_createBPTreeData:64];
    
    CCBPTreeIndex *bpTreeIndex = new CCBPTreeIndex(path.UTF8String, pageNodeCnt, indexSize);
    
    [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {

        uint32_t iKey = [key intValue];
        uint32_t iVal = [obj intValue];

        bpTreeIndex->insertIndex((CCUByte_t*)&iKey, sizeof(uint32_t), iVal);
    }];
    
    
    delete bpTreeIndex;
}






























- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _testMapBuffer];
//    [self.view endEditing:YES];
    
//    [self pri_testArray];
    
    
//    [self pri_testBPTreeIndex];
}


- (void)dealloc
{
    self->_cryptor.reset();
    if (_ptr_double) {
        free(_ptr_double);
        _ptr_double = NULL;
    }
    if (_ptr_integer) {
        free(_ptr_integer);
        _ptr_integer = NULL;
    }
}


@end
