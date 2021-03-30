//
//  CCBPTreeIndex.hpp
//  CCKVDemo
//
//  Created by yuan on 2020/2/19.
//  Copyright © 2020 yuan. All rights reserved.
//

#ifndef CCBPTreeIndex_hpp
#define CCBPTreeIndex_hpp

#include <stdio.h>
#include <string>
#include "CCType.h"
//#include "CCFileMap.hpp"

typedef CCUByte_t CCBTIndex_T;
//索引的长度,为0-255个字节
typedef CCUByte_t CCBTIndexSize_T;
//索引的value，必须是uint64_t
typedef CCUInt64_t CCBTIndexValue_T;

using namespace std;

class CCBPTreeIndex {
private:
    void *_ptrRBTreeIndexContext;
public:
    CCBPTreeIndex(const string &indexFile);
    
    CCBPTreeIndex(const string &indexFile, CCBTIndexSize_T indexLen);
    
    CCBPTreeIndex(const string &indexFile, CCUInt16_t pageNodeCount, CCBTIndexSize_T indexLen);
    
    virtual ~CCBPTreeIndex();
    
    //查
    int selectIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T *indexValue);
    //增
    int insertIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue);
    //删
    int deleteIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen);
    //改
    int updateIndex(CCBTIndex_T *index, CCBTIndexSize_T indexLen, CCBTIndexValue_T indexValue);
};

#endif /* CCBPTreeIndex_hpp */
