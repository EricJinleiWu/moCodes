#ifndef __MOCRYPT_DES_H__
#define __MOCRYPT_DES_H__

#ifdef __cplusplus
extern "C" {
#endif

//keyEx[16][48bit]
#define KEYEX_ARRAY_LEN 16
#define KEYEX_ELE_LEN   6   //48bit = 6byte

//When key expand, should convert 64bits key to 56bits firstly by replaceTable, this is the length of table.
#define KEYEX_RE_TABLE_LEN  56
#define KEYEX_RE_KEY_LEN    7   //the replaceKey length is 56bits
#define KEYEX_RE_KEY_HALF_BITSLEN   28  //the replaceKey has length 56bits, half is 28bits


#ifdef __cplusplus
}
#endif

#endif
