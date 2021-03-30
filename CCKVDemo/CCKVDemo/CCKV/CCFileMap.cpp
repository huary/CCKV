//
//  CCFileMap.cpp
//  CCKVDemo
//
//  Created by yuan on 2020/2/18.
//  Copyright © 2020 yuan. All rights reserved.
//

#include "CCFileMap.hpp"
#import <sys/mman.h>
#import <sys/stat.h>
#import <sys/fcntl.h>
#import <zlib.h>

const int defaultPageSize_g = getpagesize();
static const int defaultMinMmapSize_s = defaultPageSize_g;     //4KB
//static const int defaultMaxMmapSize_s = 1024 * 1024;     //1MB
static const int ceilFlag_s = (defaultPageSize_g & (defaultPageSize_g - 1)) == 0;

static inline int64_t getFileSize(int fd)
{
    struct stat st = {};
    if (fstat(fd, &st) == F_OK) {
        return st.st_size;
    }
    return -1;
}

static inline void truncate(int fd, int64_t size)
{
    lseek(fd, size-1, SEEK_SET);
    char a = 0;
    write(fd, &a, 1);
//    return true;
}

static inline uint64_t calMapSize(uint64_t size)
{
    if (size < defaultMinMmapSize_s) {
        size = defaultMinMmapSize_s;
    }
    
    //取DEFAULT_PAGE_SIZE_s的整数倍
    if (ceilFlag_s) {
        if (size & (defaultPageSize_g - 1)) {
            size = (size & (~(defaultPageSize_g - 1))) + defaultPageSize_g;
        }
    }
    else {
        if (size % defaultPageSize_g) {
            size = (size + defaultPageSize_g - 1) / defaultPageSize_g * defaultPageSize_g;
        }
    }
    return size;
}

CCFileMap::CCFileMap(const string &filePath) : _filePath(filePath)
{
    _ptr = nullptr;
}

CCFileMap::~CCFileMap()
{
    close();
}

bool CCFileMap::open(int mode)
{
    int m = O_RDONLY;
    if (mode == CC_F_READ ) {
        m = O_RDONLY;
    }
    else if (mode == CC_F_WRITE) {
        m = O_WRONLY;
    }
    else {
        m = O_RDWR | O_CREAT;
    }
    
    _fd = ::open(_filePath.c_str(), m, S_IRWXU);
    if (_fd < 0) {
        return false;
    }
    _fileSize = getFileSize(_fd);
    return true;
}

bool CCFileMap::read(uint64_t offset, uint64_t size)
{
    return update(offset, size, CC_F_READ);
}

bool CCFileMap::update(uint64_t offset, uint64_t size)
{
    return update(offset, size, CC_F_RDWR);
}

bool CCFileMap::update(uint64_t offset, uint64_t size, int mode)
{
    uint64_t fsize = offset + size;
    if (fsize > _fileSize && (mode & CC_F_WRITE)) {
        truncate(_fd, fsize);
        _fileSize = fsize;
    }
    if (_offset == offset && _size == size) {
        return true;
    }
    
    if (_ptr && _ptr != MAP_FAILED) {
        if (::munmap(_ptr, (size_t)_size)) {
            return false;
        }
    }
    
    _offset = offset;
    if (size == 0) {
        size = _fileSize;
    }
    
    size = calMapSize(size);
    
    uint8_t *newPtr = (uint8_t*)::mmap(NULL, (size_t)size, mode, MAP_SHARED, _fd, offset);
    if (newPtr == NULL || newPtr == MAP_FAILED) {
        return false;
    }
    _ptr = newPtr;
    _size = size;
    return true;
}

bool CCFileMap::munmap()
{
    if (_ptr && _ptr != MAP_FAILED) {
        if (::munmap(_ptr, (size_t)_size)) {
            return false;
        }
        _ptr = NULL;
        _size = 0;
    }
    return true;
}

CCBuffer *CCFileMap::createMapBuffer(uint64_t offset, uint64_t size, int mode)
{
    uint64_t fsize = offset + size;
    if (fsize > _fileSize && (mode & CC_F_WRITE)) {
        truncate(_fd, fsize);
        _fileSize = fsize;
//        ftruncate(_fd, fsize);
//        _fileSize = fsize;
    }
    
    if (size == 0) {
        size = _fileSize;
    }

    size = calMapSize(size);
    
    uint8_t *newPtr = (uint8_t*)::mmap(NULL, (size_t)size, mode, MAP_SHARED, _fd, offset);
    if (newPtr == NULL || newPtr == MAP_FAILED) {
        return nullptr;
    }
    CCBuffer *mb = new CCBuffer(newPtr, size);
    return mb;
}

void CCFileMap::destroyMapBuffer(CCBuffer *buffer)
{
    if (buffer) {
        uint8_t *ptr = buffer->bytes();
        if (ptr && ptr != MAP_FAILED) {
//            msync(ptr, buffer->bufferSize(), MS_SYNC);
            ::munmap(ptr, (size_t)buffer->bufferSize());
        }
        delete buffer;
    }
}

bool CCFileMap::close()
{
    
    if (_ptr != NULL && _ptr != MAP_FAILED) {
        ::munmap(_ptr, (size_t)_size);
        _ptr = NULL;
        _size = 0;
    }
    if (_fd) {
        ::close(_fd);
        _fd = 0;
    }
    return true;
}
