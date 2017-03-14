/*
    WuJinlei, create at 20170120, V1.0.0
*/

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

//If this equal with 1, will print debug info
#define DEBUG_MODE 0

//Rotate Left Shift
#define ROTATE_LEFT_SHIFT(a, s) ( ( (a) << (s) ) | ( ( (a) >> (32 - s) ) ) )

//The four basic functions
#define F(X, Y, Z)  ( ( (X) & (Y) ) | ( (~X) & (Z) ) )
#define G(X, Y, Z)  ( ( (X) & (Z) ) | ( (Y) & (~Z) ) )
#define H(X, Y, Z)  ( (X) ^ (Y) ^ (Z) )
#define I(X, Y, Z)  ( (Y) ^ ( (X) | (~Z) ) )

#define FF(a,b,c,d,k,s,t) { \
    a+=((k)+(t)+F((b),(c),(d))); \
    a=ROTATE_LEFT_SHIFT(a,s); \
    a+=b; };

#define GG(a,b,c,d,k,s,t) { \
    a+=((k)+(t)+G((b),(c),(d))); \
    a=ROTATE_LEFT_SHIFT(a,s); \
    a+=b; };

#define HH(a,b,c,d,k,s,t) { \
    a+=((k)+(t)+H((b),(c),(d))); \
    a=ROTATE_LEFT_SHIFT(a,s); \
    a+=b; };

#define II(a,b,c,d,k,s,t) { \
    a+=((k)+(t)+I((b),(c),(d))); \
    a=ROTATE_LEFT_SHIFT(a,s); \
    a+=b; };

//The initial value of four nubmers
#define A_INIT_VALUE    (unsigned long)0x67452301L
#define B_INIT_VALUE    (unsigned long)0xefcdab89L
#define C_INIT_VALUE    (unsigned long)0x98badcfeL
#define D_INIT_VALUE    (unsigned long)0x10325476L

//Shift value in each four loop
#define S11 7
#define S12 12
#define S13 17
#define S14 22

#define S21 5
#define S22 9
#define S23 14
#define S24 20

#define S31 4
#define S32 11
#define S33 16
#define S34 23

#define S41 6
#define S42 10
#define S43 15
#define S44 21

//The unit length
#define UNIT_LEN_BITS   (512)
#define UNIT_LEN_BYTES   (64)
#define MOD_LEN_BITS    (448)
#define MOD_LEN_BYTES    (56)

#ifdef __cplusplus
}
#endif

#endif

