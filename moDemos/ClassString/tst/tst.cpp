#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "moString.h"

void tst_MoString(void)
{
    //Basic functions, constructor, c_str, length, and so on.
    MoString str0("abc");
    MoString str1(5, 'x');
    MoString str2;

    printf("str0 value [%s], length %d\n", str0.c_str(), str0.length());
    printf("str1 value [%s], length %d\n", str1.c_str(), str1.length());
    printf("str2 value [%s], length %d\n", str2.c_str(), str2.length());

    str0 = str0 + str1;
    printf("After add str1 to str0, str0 value [%s], length %d\n", str0.c_str(), str0.length());

    MoString str3("Hello ");
    str3 = str3 + "World";
    printf("After adding, str3 value [%s], length %d\n", str3.c_str(), str3.length());

    #define SRC_LEN 1024
    #define PAT_LEN 8

    //SubString function
    char pSrc[SRC_LEN] = {0x00};
    char pPat[PAT_LEN] = {0x00};
    int i = 0;
    for(i = 0; i < PAT_LEN - 2; i++)
    {
        pPat[i] = rand() % 26 + 65;
    }
    pPat[PAT_LEN - 1] = 0x00;
    for(i = 0; i < SRC_LEN - 2; i++)
    {
        pSrc[i] = rand() % 26 + 65;
    }
    pSrc[SRC_LEN - 1] = 0x00;

    strncpy(pSrc + SRC_LEN - PAT_LEN - 3, pPat, PAT_LEN - 2);
    
    MoString srcStr(pSrc);
    MoString patStr(pPat);
    int offset = srcStr.subString(patStr);
    printf("srcString [%s], patternString [%s], offset = %d\n",
        srcStr.c_str(), patStr.c_str(), offset);
}

int main(int argc, char **argv)
{
    srand((unsigned int)time(NULL));

    tst_MoString();

    return 0;
}

