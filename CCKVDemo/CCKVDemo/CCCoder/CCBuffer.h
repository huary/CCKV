//
//  CCBuffer.h
//  CCKVDemo
//
//  Created by yuan on 2019/10/20.
//  Copyright © 2019 yuan. All rights reserved.
//

//#import <Foundation/Foundation.h>
#ifndef CCBuffer_h
#define CCBuffer_h

#include <stdio.h>
#include <string>

using namespace std;

typedef enum CCBufferSeekType {
    CCBufferSeekTypeSET  = 0,
    CCBufferSeekTypeCUR  = 1,
    CCBufferSeekTypeEND  = 2,
}CCBufferSeekType_E;

class CCBuffer {
protected:
    uint8_t *_ptr;
    //空间大小
    int64_t _size;
private:
    //写入数据的长度或者是seekTo的位置（有条件）
    int64_t _length;
    //写入数据的位置
    int64_t _position;
    
public:
//    CCBuffer(NSData *data);
    
    CCBuffer(uint8_t *ptr, int64_t size);
    
    virtual ~CCBuffer();
    
//    void writeData(NSData *data);
    
    void writeBuffer(CCBuffer *buffer);
    
    void writeBuffer(uint8_t *ptr, int64_t size);
    
    void writeByte(uint8_t value);
    
    void writeLittleEndian16(uint16_t value);
    
    void writeLittleEndian32(uint32_t value);
    
    void writeLittleEndian64(uint64_t value);
    
    uint8_t readByte();
    
    uint16_t readLittleEndian16();
    
    uint32_t readLittleEndian32();
    
    uint64_t readLittleEndian64();
    
//    NSData* readBuffer(int64_t size);
    
    void read(uint8_t *ptr, int64_t size);
    
    int64_t seek(CCBufferSeekType seekType);
    
    int64_t seekTo(int64_t to);
    
    int64_t currentSeek();
    
    int64_t bufferSize();
    
    int64_t dataSize();
    
    int64_t remSize();
    
    //从position计算remSize
    int64_t remSeekSize();
    
    void bzero();
    
    /*
     *truncate不把内容置零,truncateTo会改变dataSize
     */
    bool truncateTo(int64_t size);
    /*
     *truncate把内容置setval
     */
    bool truncateTo(int64_t size, uint8_t setval);
    
    /*
     *truncate不把内容置零,并且最大seek到size,为CCDataSeekTypeCUR时同truncateTo(int64_t size)
     */
    bool truncateToWithSeek(int64_t size, CCBufferSeekType seekType);
    
    uint8_t* bytes();
    
//    NSData* data();
    
//    NSData* copyData();
    
    void print();
    
};

#endif
