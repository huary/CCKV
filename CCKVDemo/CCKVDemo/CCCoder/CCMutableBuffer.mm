//
//  CCMutableBuffer.m
//  CCKVDemo
//
//  Created by yuan on 2019/10/20.
//  Copyright © 2019 yuan. All rights reserved.
//

#import "CCMutableBuffer.h"

static const int32_t expandBufferSize_s = 4096;

CCMutableBuffer::CCMutableBuffer() : CCBuffer(nullptr, 0)
{
    _size = expandBufferSize_s;
    _ptr = (uint8_t*)calloc((size_t)_size, sizeof(uint8_t));
}

CCMutableBuffer::CCMutableBuffer(int64_t size) : CCBuffer(nullptr, 0)
{
    _size = size > 0 ? size : expandBufferSize_s;
    _ptr = (uint8_t*)calloc((size_t)_size, sizeof(uint8_t));
}

//CCMutableBuffer::CCMutableBuffer(NSData *data) : CCBuffer(nullptr, 0)
//{
//    _size = data.length;
//    if (_size <= 0) {
//        _size = expandBufferSize_s;
//    }
//    _ptr = (uint8_t*)calloc((size_t)_size, sizeof(uint8_t));
//    if (_ptr && data.bytes) {
//        memcpy(_ptr, (uint8_t*)data.bytes, (size_t)_size);
//        seekTo(_size);
//    }
//}

CCMutableBuffer::CCMutableBuffer(CCBuffer *codeData) : CCBuffer(nullptr, 0)
{
    int64_t dataSize = codeData->dataSize();
    _size = dataSize;
    if (_size <= 0) {
        _size = expandBufferSize_s;
    }
    _ptr = (uint8_t*)calloc((size_t)_size, sizeof(uint8_t));
    if (_ptr && codeData->bytes()) {
        memcpy(_ptr, (uint8_t*)codeData->bytes(), (size_t)dataSize);
        seekTo(dataSize);
    }
}

CCMutableBuffer::~CCMutableBuffer()
{
    if (_ptr) {
        free(_ptr);
        _ptr = NULL;
    }
    _size = 0;
}

BOOL CCMutableBuffer::ensureRemSize(int64_t remSize)
{
    int64_t leftSize = CCBuffer::remSize();
    if (leftSize > remSize) {
        return YES;
    }
    int64_t extra = MAX(remSize, expandBufferSize_s);
    return increaseBufferSizeBy(extra);
}

//这个是从position计算remSize
BOOL CCMutableBuffer::ensureRemSeekSize(int64_t remSeekSize)
{
    int64_t leftSize = CCBuffer::remSeekSize();
    if (leftSize > remSeekSize) {
        return YES;
    }
    int64_t extra = MAX(remSeekSize, expandBufferSize_s);
    return increaseBufferSizeBy(extra);
}

BOOL CCMutableBuffer::increaseBufferSizeBy(int64_t increaseSize)
{
    int64_t total = _size + increaseSize;
    uint8_t *ptr = (uint8_t*)realloc(_ptr, (size_t)(total * sizeof(uint8_t)));
    if (ptr == NULL) {
        return NO;
    }
    _ptr = ptr;
    _size = total;
    return YES;
}

//void CCMutableBuffer::appendWriteData(NSData *data)
//{
//    appendWriteBuffer((uint8_t*)data.bytes, data.length);
//}

void CCMutableBuffer::appendWriteBuffer(uint8_t *ptr, int64_t size)
{
    int64_t rem = CCBuffer::remSize();
    int64_t diff = rem - size;
    if (diff < 0) {
        int64_t s = (int64_t)MAX(_size, expandBufferSize_s);
        increaseBufferSizeBy(s);
    }
    writeBuffer(ptr,size);
}

void CCMutableBuffer::appendWriteByte(uint8_t value)
{
    int64_t rem = CCBuffer::remSize();
    int64_t diff = rem - 1;
    if (diff < 0) {
        int64_t s = (int64_t)MAX(_size, expandBufferSize_s);
        increaseBufferSizeBy(s);
    }
    writeByte(value);
}

//NSMutableData* CCMutableBuffer::mutableData()
//{
//    if (_ptr == NULL) {
//        return nil;
//    }
//    return [NSMutableData dataWithBytesNoCopy:_ptr length:(NSUInteger)this->dataSize() freeWhenDone:NO];
//}

//NSMutableData* CCMutableBuffer::mutableCopyData()
//{
//    if (_ptr == NULL) {
//        return nil;
//    }
//    return [NSMutableData dataWithBytes:_ptr length:(NSUInteger)this->dataSize()];
//}
