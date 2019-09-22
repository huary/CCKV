#ifndef _CCMACRO_H_
#define _CCMACRO_H_

typedef        int                                         SIZE_TYPE_INT;
typedef        unsigned int                                SIZE_TYPE_UINT;
typedef        long                                        SIZE_TYPE_LONG;
typedef        unsigned long                               SIZE_TYPE_ULONG;
typedef        long long                                   SIZE_TYPE_LLONG;
typedef        unsigned long long                          SIZE_TYPE_ULLONG;

#define     TYPE_NOT(VAL)                                (~(VAL))
#define     TYPE_AND(VA,VB)                              ((VA) & (VB))
#define     TYPE_OR(VA,VB)                               ((VA) | (VB))
#define     TYPE_XOR(VA,VB)                              ((VA) ^ (VB))
#define     TYPE_IOR(VA,VB)                              TYPE_NOT(TYPE_XOR(VA,VB))
#define     TYPE_RS(V,RN)                                ((V) >> (RN))
#define     TYPE_LS(V,LN)                                ((V) << (LN))


#define     TYPEUBYTE_BITS_N(V)                            ( ((V) == 0) ? (0) : \
                                                                               ( ((V) > 0XF) ? ( ((V) > 0X3F) ? ( ((V) > 0X7F) ? (8) : (7) ) : \
                                                                                 ( ((V) > 0X1F) ? (6) : (5) ) ) : \
                                                                               ( ((V) > 0X3) ? ( ((V) > 0X7) ? (4) : (3) ) : \
                                                                                 ( ((V) > 0X1) ? (2) : (1) ) ) ) )

#define     TYPEUINT_BITS_N(V)                          ( ((V) == 0) ? (0) : \
                                                                              ( ((V) > 0XFFFF) ? ( ((V) > 0XFFFFFF) ? ( TYPEUBYTE_BITS_N(TYPE_RS(V,24)) + 24 ) : ( TYPEUBYTE_BITS_N(TYPE_RS(V,16)) + 16 ) ) : \
                                                                                                                     ( ((V) > 0XFF) ? ( TYPEUBYTE_BITS_N(TYPE_RS(V,8)) + 8 ) : ( TYPEUBYTE_BITS_N(V) ) ) ) )

#define        TYPEUINT_BYTES_N(V)                      (((V) == 0) ? (1) : ((SIZE_TYPE_INT(V)) < 0 ? 4 : ((((SIZE_TYPE_UINT)V) > 0XFFFF) ? ((((SIZE_TYPE_UINT)V) > 0XFFFFFF) ? (4) : (3) ) : ((((SIZE_TYPE_UINT)V) > 0XFF) ? (2) : (1)))))

#define         TYPEULL_BITS_N(V)                       (((V) == 0) ? (0) : ((((SIZE_TYPE_ULLONG)V) > 0XFFFFFFFF) ? (TYPEUINT_BITS_N(TYPE_RS((SIZE_TYPE_ULLONG)V,32)) + 32) : TYPEUINT_BITS_N(V)))

#define         TYPEULL_BYTES_N(V)                      (((V) == 0) ? (1) : (((SIZE_TYPE_LLONG)V) < 0 ? 8 :  ((((SIZE_TYPE_ULLONG)V) > 0XFFFFFFFF) ? (TYPEUINT_BYTES_N(TYPE_RS((SIZE_TYPE_ULLONG)V,32)) + 4) :  TYPEUINT_BYTES_N(V))))


#endif
