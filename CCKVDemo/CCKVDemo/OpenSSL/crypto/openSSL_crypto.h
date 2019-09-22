//
//  openSSL_crypto.h
//  YZHKVDemo
//
//  Created by yuan on 2019/7/28.
//  Copyright © 2019 yuan. All rights reserved.
//

#ifndef openSSL_crypto_h
#define openSSL_crypto_h

#include <stdio.h>

#define OPENSSL_FILE    __FILE__
#define OPENSSL_LINE    __LINE__

namespace openSSL {
    
    int CRYPTO_memcmp(const void * in_a, const void * in_b, size_t len);
    
    void *CRYPTO_malloc(size_t num, const char *file, int line);
    void *CRYPTO_realloc(void *str, size_t num, const char *file, int line);
    void CRYPTO_clear_free(void *str, size_t num, const char *file, int line);
    void CRYPTO_free(void *str, const char *file, int line);
    
    void OPENSSL_cleanse(void *ptr, size_t len);
    
#define OPENSSL_malloc(num)             CRYPTO_malloc(num, OPENSSL_FILE, OPENSSL_LINE)
#define OPENSSL_realloc(addr, num)      CRYPTO_realloc(addr, num, OPENSSL_FILE, OPENSSL_LINE)
#define OPENSSL_clear_free(addr, num)   CRYPTO_clear_free(addr, num, OPENSSL_FILE, OPENSSL_LINE)
#define OPENSSL_free(addr)              CRYPTO_free(addr, OPENSSL_FILE, OPENSSL_LINE)
    
#define CRYPTOerr(f,r)             
}



#endif /* openSSL_crypto_h */
