//
//  openSSL_crypto.c
//  YZHKVDemo
//
//  Created by yuan on 2019/7/28.
//  Copyright © 2019 yuan. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include "openSSL_crypto.h"

namespace openSSL {
    
    int CRYPTO_memcmp(const void * in_a, const void * in_b, size_t len)
    {
        size_t i;
        const volatile unsigned char *a = (const volatile unsigned char *)in_a;
        const volatile unsigned char *b = (const volatile unsigned char *)in_b;
        unsigned char x = 0;
        
        for (i = 0; i < len; i++)
            x |= a[i] ^ b[i];
        
        return x;
    }
    
    void *CRYPTO_malloc(size_t num, const char *file, int line)
    {
        void *ret = NULL;
        if (num == 0)
            return NULL;
        
        ret = malloc(num);
        
        return ret;
    }
    
    void CRYPTO_free(void *str, const char *file, int line)
    {
        if (str == NULL) {
            return;
        }
        free(str);
    }
    
    void *CRYPTO_realloc(void *str, size_t num, const char *file, int line)
    {
        if (str == NULL)
            return CRYPTO_malloc(num, file, line);
        
        if (num == 0) {
            CRYPTO_free(str, file, line);
            return NULL;
        }
        
        return realloc(str, num);
    }
    
    void OPENSSL_cleanse(void *ptr, size_t len)
    {
        memset(ptr, 0, len);
    }
    
    
    
    void CRYPTO_clear_free(void *str, size_t num, const char *file, int line)
    {
        if (str == NULL)
            return;
        if (num)
            OPENSSL_cleanse(str, num);
        CRYPTO_free(str, NULL, 0);
    }
}


