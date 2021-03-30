//
//  CCBuffer.m
//  CCKVDemo
//
//  Created by yuan on 2019/10/20.
//  Copyright © 2019 yuan. All rights reserved.
//

#include "CCBuffer.h"

//CCBuffer::CCBuffer(NSData *data) : _ptr((uint8_t*)data.bytes), _size(data.length)
//{
//    _length = _size;
//    _position = 0;
//}

CCBuffer::CCBuffer(uint8_t *ptr, int64_t size) : _ptr(ptr), _size(size)
{
    _length = 0;
    _position = 0;
}

CCBuffer::~CCBuffer()
{
    _ptr = nullptr;
    _size = 0;
    _length = 0;
    _position = 0;
}

//void CCBuffer::writeData(NSData *data)
//{
//    if (data == nil || data.length == 0) {
//        return;
//    }
//    this->writeBuffer((uint8_t *)data.bytes, (int64_t)data.length);
//}

void CCBuffer::writeBuffer(CCBuffer *buffer)
{
    this->writeBuffer(buffer->_ptr, buffer->_length);
}

void CCBuffer::writeBuffer(uint8_t *ptr, int64_t size)
{
    if (_ptr == NULL || ptr == NULL || size <= 0) {
        return;
    }
    if (_position + size > _size) {
        return;
    }
    //    memcpy(_ptr + _position, ptr, size);
    memmove(_ptr + _position, ptr, (size_t)size);
    _position += size;
    _length = MAX(_length, _position);
}

void CCBuffer::writeByte(uint8_t value)
{
    if (_ptr == NULL || _position + 1 > _size) {
        return;
    }
    _ptr[_position] = value;
    ++_position;
    _length = MAX(_length, _position);
}

void CCBuffer::writeLittleEndian16(uint16_t value)
{
    writeByte(value & 0XFF);
    writeByte((value >> 8) & 0XFF);
}

void CCBuffer::writeLittleEndian32(uint32_t value)
{
    writeByte(value & 0XFF);
    writeByte((value >> 8) & 0XFF);
    writeByte((value >> 16) & 0XFF);
    writeByte((value >> 24) & 0XFF);
}

void CCBuffer::writeLittleEndian64(uint64_t value)
{
    writeByte(value & 0XFF);
    writeByte((value >> 8) & 0XFF);
    writeByte((value >> 16) & 0XFF);
    writeByte((value >> 24) & 0XFF);
    writeByte((value >> 32) & 0XFF);
    writeByte((value >> 40) & 0XFF);
    writeByte((value >> 48) & 0XFF);
    writeByte((value >> 56) & 0XFF);
}

uint8_t CCBuffer::readByte()
{
    if (_ptr == NULL || _position == _size) {
        return 0;
    }
    uint8_t value = _ptr[_position];
    ++_position;
    return value;
}

uint16_t CCBuffer::readLittleEndian16()
{
    uint16_t a = readByte();
    uint16_t b = readByte();
    uint16_t value = (a | (b << 8));
    return value;
}

uint32_t CCBuffer::readLittleEndian32()
{
    uint32_t a = readByte();
    uint32_t b = readByte();
    uint32_t c = readByte();
    uint32_t d = readByte();
    uint32_t value = (a | (b << 8) | (c << 16) | ( d << 24));
    return value;
}

uint64_t CCBuffer::readLittleEndian64()
{
    uint64_t a = readByte();
    uint64_t b = readByte();
    uint64_t c = readByte();
    uint64_t d = readByte();
    
    uint64_t e = readByte();
    uint64_t f = readByte();
    uint64_t g = readByte();
    uint64_t h = readByte();
    uint64_t value = (a | (b << 8) | (c << 16) | ( d << 24) | (e << 32) | (f << 40) | (g << 48) | (h << 56));
    return value;
}

//NSData* CCBuffer::readBuffer(int64_t size)
//{
//    if (_ptr == NULL || size <= 0) {
//        return nil;
//    }
//    if (_position + size > _size) {
//        return nil;
//    }
//    NSData *data = [NSMutableData dataWithLength:(NSUInteger)size];
//    memcpy((uint8_t*)data.bytes, _ptr, (size_t)size);
//    _position += size;
//    return [data copy];
//}

void CCBuffer::read(uint8_t *ptr, int64_t size)
{
    if (_ptr == NULL || size <= 0 || ptr == NULL) {
        return;
    }
    if (_position + size > _size) {
        return;
    }
    memmove(ptr, _ptr + _position, (size_t)size);
    _position += size;
}

int64_t CCBuffer::seek(CCBufferSeekType seekType)
{
    int64_t pos = _position;
    switch (seekType) {
        case CCBufferSeekTypeSET: {
            pos = 0;
            break;
        }
        case CCBufferSeekTypeCUR: {
            break;
        }
        case CCBufferSeekTypeEND: {
            pos = _length;
            break;
        }
        default:
            break;
    }
    return seekTo(pos);
}

int64_t CCBuffer::seekTo(int64_t to)
{
    if (_ptr == NULL || to < 0 || to > _size) {
        return 0;
    }
    _position = to;
    if (to > _length) {
        _length = to;
    }
    return _position;
}

int64_t CCBuffer::currentSeek()
{
    return _position;
}

int64_t CCBuffer::bufferSize()
{
    return _size;
}

int64_t CCBuffer::dataSize()
{
    return _length;
}

int64_t CCBuffer::remSize()
{
    return _size - _length;
}

//从position计算remSize
int64_t CCBuffer::remSeekSize()
{
    return _size - _position;
}

void CCBuffer::bzero()
{
    if (_ptr == NULL) {
        return;
    }
    memset(_ptr, 0, (size_t)_size);
    _length = 0;
    _position = 0;
}

/*
 *truncate不把内容置零
 */
bool CCBuffer::truncateTo(int64_t size)
{
    if (size > _size || size < 0) {
        return false;
    }
    
    _length = size;
    _position = MIN(_position, _length);
    
    return true;
}

bool CCBuffer::truncateTo(int64_t size, uint8_t setval)
{
    if (size > _size || size < 0) {
        return false;
    }
    
    int64_t differ = size - _length;
    
    _length = size;
    _position = MIN(_position, _length);
    
    if (differ) {
        if (differ > 0) {
            memset(_ptr + size - differ, setval, (size_t)differ);
        }
        else {
            differ = _size - size;//-differ;
            memset(_ptr + size, setval, (size_t)differ);
        }
    }
    
    return true;
}

/*
 *truncate不把内容置零,并且最大seek到size,为CCDataSeekTypeCUR时同truncateTo(int64_t size)
 */
bool CCBuffer::truncateToWithSeek(int64_t size, CCBufferSeekType seekType)
{
    if (size > _size || size < 0) {
        return false;
    }
    
    _length = size;
    switch (seekType) {
        case CCBufferSeekTypeSET: {
            _position = 0;
            break;
        }
        case CCBufferSeekTypeCUR: {
            _position = MIN(_position, _length);
        }
        case CCBufferSeekTypeEND: {
            _position = _length;
            break;
        }
        default:
            break;
    }
    return true;
}

uint8_t* CCBuffer::bytes()
{
    return _ptr;
}

//NSData* CCBuffer::data()
//{
//    if (_ptr == NULL) {
//        return nil;
//    }
//    NSData *data = [NSData dataWithBytesNoCopy:_ptr length:(NSUInteger)_length freeWhenDone:NO];
//    return data;
//}

//NSData* CCBuffer::copyData()
//{
//    if (_ptr == NULL) {
//        return nil;
//    }
//    NSData *data = [NSData dataWithBytes:_ptr length:(NSUInteger)_length];
//    return data;
//}

void CCBuffer::print()
{
//    NSLog(@"_size=%lld,_length=%lld,_pos=%lld",_size,_length,_position);
}
