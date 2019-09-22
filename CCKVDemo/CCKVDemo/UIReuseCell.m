//
//  UIReuseCell.m
//  YZHKVDemo
//
//  Created by yuan on 2019/9/4.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "UIReuseCell.h"

@implementation UIReuseCell

-(instancetype)init
{
    self = [super init];
    if (self) {
        [self _setupExcelCellSubView];
    }
    return self;
}

-(void)_setupExcelCellSubView
{
    UILabel *textLabel =[[UILabel alloc] init];
    textLabel.font = FONT(16);
    textLabel.textAlignment = NSTextAlignmentCenter;
    [self addSubview:textLabel];
    
    _textLabel = textLabel;
}

-(void)layoutSubviews
{
    [super layoutSubviews];
    self.textLabel.frame = self.bounds;
}


@end
