//
//  CCMutableBuffer.h
//  CCKVDemo
//
//  Created by yuan on 2019/10/20.
//  Copyright © 2019 yuan. All rights reserved.
//


#include "CCBuffer.h"

class CCMutableBuffer : public CCBuffer  {
    
public:
    CCMutableBuffer();
    
    CCMutableBuffer(int64_t size);
    
//    CCMutableBuffer(NSData *data);
    //复制构造函数
    CCMutableBuffer(CCBuffer *buffer);
    
    virtual ~CCMutableBuffer();
    
    //这个是从length计算remSize
    BOOL ensureRemSize(int64_t remSize);
    
    //这个是从position计算remSize
    BOOL ensureRemSeekSize(int64_t remSeekSize);
    
    BOOL increaseBufferSizeBy(int64_t increaseSize);
    
    //下面在长度不足时会进行increase
//    void appendWriteData(NSData *data);
    
    void appendWriteBuffer(uint8_t *ptr, int64_t size);
    
    void appendWriteByte(uint8_t value);
    
//    NSMutableData* mutableData();
    
//    NSMutableData* mutableCopyData();
};

