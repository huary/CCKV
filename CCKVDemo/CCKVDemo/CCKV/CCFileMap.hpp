//
//  CCFileMap.hpp
//  CCKVDemo
//
//  Created by yuan on 2020/2/18.
//  Copyright © 2020 yuan. All rights reserved.
//

#ifndef CCFileMap_hpp
#define CCFileMap_hpp

#include <stdio.h>
#include <string>
#include "CCBuffer.h"

#define  CC_F_READ  (0x01)
#define  CC_F_WRITE (0x02)
#define  CC_F_RDWR  (CC_F_READ|CC_F_WRITE)

extern const int defaultPageSize_g;

using namespace std;
//File Map
class CCFileMap {
private:
    int _fd;
    uint8_t *_ptr;
    uint64_t _size;
    uint64_t _offset;
    uint64_t _fileSize;
    string _filePath;
public:
    CCFileMap(const string &filePath);
    virtual ~CCFileMap();
    
    bool open(int mode);

    bool read(uint64_t offset, uint64_t size);

    bool update(uint64_t offset, uint64_t size);
    
    bool update(uint64_t offset, uint64_t size, int mode);
    
    bool munmap();
    
    uint64_t size() const {return _size;}
    
    uint64_t offset() const {return _offset;}
    
    uint64_t fileSize() const {return _fileSize;}
    
    uint8_t *ptr() const { return _ptr;}
    
    string filePath() const { return _filePath;}
    
    CCBuffer *createMapBuffer(uint64_t offset, uint64_t size, int mode);
    
    void destroyMapBuffer(CCBuffer *buffer);
    
    bool close();
};

#endif /* CCFileMap_hpp */
