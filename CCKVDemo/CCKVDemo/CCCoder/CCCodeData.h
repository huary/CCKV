//
//  CCCodeData.h
//  CCKVDemo
//
//  Created by yuan on 2019/9/10.
//  Copyright © 2019 yuan. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, CCDataSeekType)
{
    CCDataSeekTypeSET  = 0,
    CCDataSeekTypeCUR  = 1,
    CCDataSeekTypeEND  = 2,
};

class CCCodeData {
    
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
    CCCodeData(NSData *data);
    
    CCCodeData(uint8_t *ptr, int64_t size);
    
    virtual ~CCCodeData();
    
    void writeData(NSData *data);
    
    void writeCodeData(CCCodeData *codeData);
    
    void writeBuffer(uint8_t *ptr, int64_t size);
    
    void writeByte(uint8_t value);
    
    void writeLittleEndian16(uint16_t value);
    
    void writeLittleEndian32(uint32_t value);
    
    void writeLittleEndian64(uint64_t value);
    
    uint8_t readByte();
    
    uint16_t readLittleEndian16();
    
    uint32_t readLittleEndian32();
    
    uint64_t readLittleEndian64();
    
    NSData* readBuffer(int64_t size);
    
    void read(uint8_t *ptr, int64_t size);
    
    int64_t seek(CCDataSeekType seekType);
    
    int64_t seekTo(int64_t to);
    
    int64_t currentSeek();
    
    int64_t bufferSize();

    int64_t dataSize();
    
    int64_t remSize();
    
    
    void bzero();
    
    /*
     *truncate不把内容置零,truncateTo会改变dataSize
     */
    BOOL truncateTo(int64_t size);
    /*
     *truncate把内容置setval
     */
    BOOL truncateTo(int64_t size, uint8_t setval);
    
    /*
     *truncate不把内容置零,并且最大seek到size,为CCDataSeekTypeCUR时同truncateTo(int64_t size)
     */
    BOOL truncateToWithSeek(int64_t size, CCDataSeekType seekType);
    
    uint8_t* bytes();
    
    NSData* data();
    
    NSData* copyData();
    
    void print();
};


class CCMutableCodeData : public CCCodeData {
public:
    CCMutableCodeData();
    
    CCMutableCodeData(int64_t size);
    
    CCMutableCodeData(NSData *data);
    //复制构造函数
    CCMutableCodeData(CCCodeData *codeData);
    
    virtual ~CCMutableCodeData();
    
    BOOL ensureRemSize(int64_t remSize);
    
    BOOL increaseBufferSizeBy(int64_t increaseSize);
    
    //下面在长度不足时会进行increase
    void appendWriteData(NSData *data);
    
    void appendWriteBuffer(uint8_t *ptr, int64_t size);
    
    void appendWriteByte(uint8_t value);
    
    NSMutableData* mutableData();
    
    NSMutableData* mutableCopyData();
};
