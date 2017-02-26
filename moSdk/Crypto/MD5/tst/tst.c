#include <stdio.h>
#include <string.h>

#include "moCrypt.h"
#include "utils.h"

//#define ROTATE_LEFT_SHIFT(a, s) ( ( (a) << (s) ) & ( (a >> (32 - s) ) ) )
static void tst_ROTATE_LEFT_SHIFT()
{
    unsigned int a = 0xa5c396b4;
    int s = 0;
    for(s = 0; s < 32; s++)
    {
        unsigned int ret = ROTATE_LEFT_SHIFT(a, s);
        printf("s = %d, ret = 0x%x\n", s, ret);
    }
}

//int moCrypt_MD5_String(const unsigned char *pCont, MO_MD5_VALUE *pValue)
static void tst_moCrypt_MD5_String()
{
//    char str0[] = "";
//    char str0[] = "abc";
//    char str0[] = "123456789012345678901234567890123456789012345678901234567890123";  //64bytes
//    char str0[] = "1234567890123456789012345678901234567890123456789012345";    //56bytes
    char str0[] = "12345678901234567890123456789012345678901234567890123456789012341234567890123456789012345678901234567890123456789012345678901234"; //129bytes
    MO_MD5_VALUE value;
    int ret = moCrypt_MD5_String(str0, &value);
    if(0 == ret)
    {
        printf("moCrypt_MD5_String succeed. md5 value is : \n");
        moCrypt_MD5_dump(&value);
    }
    else
    {
        printf("moCrypt_MD5_String failed, ret = 0x%x, str0 = [%s]\n", ret, str0);
    }    
}

static void tst_moCrypt_MD5_File()
{
    //File not exist; not a file(directory); is a link; 
    char filepath[] = "./tstFile";
    MO_MD5_VALUE value;
    memset(&value, 0x00, sizeof(MO_MD5_VALUE));
    int ret = moCrypt_MD5_File(filepath, &value);
    if(ret != MOCRYPTMD5_ERR_OK)
    {
        printf("Do md5 to file [%s] failed! ret = 0x%x\n", filepath, ret);
    }
    else
    {
        printf("Do md5 to file [%s] succeed!\n", filepath);        
        moCrypt_MD5_dump(&value);
    }
}

int main(int argc, char **argv)
{
//    tst_ROTATE_LEFT_SHIFT();
//    tst_moCrypt_MD5_String();
    tst_moCrypt_MD5_File();

	return 0;
}
