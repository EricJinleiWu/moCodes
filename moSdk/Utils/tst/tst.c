#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "moUtils.h"

/* ********************************************************************* 
 ********************** Do test to moUitls_File module. ****************
 ***********************************************************************/
static void tst_moUtils_File_getSize(void)
{
    char *path = "./a.txt";
    int size = moUtils_File_getSize(path);
    printf("path = [%s], size = %d\n", path, size);
}

static void tst_moUtils_File_getAbsFilePath(void)
{
    char *path0 = "a.txt";
    char *absPath0 = moUtils_File_getAbsFilePath(path0);
    printf("path0 = %s, absPath0 = %s\n", path0, absPath0);

    char *path1 = "/b.txt";
    char *absPath1 = moUtils_File_getAbsFilePath(path1);
    printf("path1 = %s, absPath1 = %s\n", path1, absPath1);

    char *path2 = "./c.txt";
    char *absPath2 = moUtils_File_getAbsFilePath(path2);
    printf("path2 = %s, absPath2 = %s\n", path2, absPath2);

    char *path3 = "../File/src/file.c";
    char *absPath3 = moUtils_File_getAbsFilePath(path3);
    printf("path3 = %s, absPath3 = %s\n", path3, absPath3);

    if(NULL != absPath0)
        free(absPath0);
    if(NULL != absPath1)
        free(absPath1);
    if(NULL != absPath2)
        free(absPath2);
    if(NULL != absPath3)
        free(absPath3);
}

static void tst_moUtils_File_getAbsDirPath(void)
{
    char *path0 = "dir0";
    char *absPath0 = moUtils_File_getAbsDirPath(path0);
    printf("path0 = %s, absPath0 = %s\n", path0, absPath0);
    
    char *path1 = "../File/src";
    char *absPath1 = moUtils_File_getAbsDirPath(path1);
    printf("path1 = %s, absPath1 = %s\n", path1, absPath1);

    if(NULL != absPath0)
        free(absPath0);
    if(NULL != absPath1)
        free(absPath1);
    absPath0 = NULL;
    absPath1 = NULL;    
}

static void tst_moUtils_File_getDirAndFilename(void)
{
    char *filepath0 = "../tst/a.txt";
    MOUTILS_FILE_DIR_FILENAME info;
    memset(&info, 0x00, sizeof(MOUTILS_FILE_DIR_FILENAME));
    int ret = moUtils_File_getDirAndFilename(filepath0, &info);
    if(ret == 0)
    {
        printf("filepath [%s], dir path [%s], filename[%s]\n", filepath0, info.pDirpath, info.pFilename);
        free(info.pDirpath);
        info.pDirpath = NULL;
        free(info.pFilename);
        info.pFilename = NULL;
    }
    else
    {
        printf("moUtils_File_getDirAndFilename failed! ret = %d(0x%x)\n", ret, ret);
    }
}

static void tst_moUtils_File_getFilepathSameState(void)
{
    char *pSrcFilepath = "a.txt";
    char *pDstFilepath = "../tst/a.txt";
    MOUTILS_FILE_ABSPATH_STATE state;
    int ret = moUtils_File_getFilepathSameState(pSrcFilepath, pDstFilepath, &state);
    if(ret == 0)
    {
        printf("src[%s], dst[%s], state=%d\n", pSrcFilepath, pDstFilepath, state);
    }
    else
    {
        printf("moUtils_File_getFilepathSameState failed! ret = %d(0x%x)\n", ret, ret);
    }
}



/* ********************************************************************* 
 ********************** Do test to moUitls_Search module. ****************
 ***********************************************************************/
static void DumpArray(const unsigned char *pArray, const unsigned int len)
{
    printf("Dump array :\n\t");
    int i = 0;
    for(; i < len; i++)
    {
        printf("0x%04x ", *(pArray + i));
    }
    printf("\nDump over.\n");
}

static void tst_moUtils_Search_Basic()
{
    unsigned char pattern[8] = {0x00};
    unsigned char src[16] = {0x00};

//    //src and pattern has same length
//    unsigned int patLen = rand() % 8;
//    memset(pattern, 0x00, 8);
//    memset(src, 0x00, 16);
//    int i = 0;
//    for(i = 0; i < patLen; i++)
//    {
//        //A pattern in randm
//        pattern[i] = rand() % 256;
//        src[i] = pattern[i];    //same string
//        src[i] = (pattern[i] + 1) % 256;    //different string
//        
//    }
//    int retBF = moUtils_Search_BF(src, patLen, pattern, patLen);
//    int retKmp = moUtils_Search_KMP(src, patLen, pattern, patLen);
//    int retBm = moUtils_Search_BM(src, patLen, pattern, patLen);
//    printf("src info : \n");
//    DumpArray(src, patLen);
//    printf("pattern info : \n");
//    DumpArray(pattern, patLen);
//    printf("After search, retBF=%d, retKmp=%d, retBm=%d\n",
//            retBF, retKmp, retBm);

//    //A pattern include same charactor 
//    unsigned int patLen = rand() % 8;
//    memset(pattern, 0x00, 8);
//    memset(src, 0x00, 16);
//    int i = 0;
//    for(i = 0; i < patLen; i++)
//    {        
//        //A pattern include a duplicate charactor
//        if(i == patLen / 2 && i > 0)
//        {
//            pattern[i] = pattern[i - 1];
//        }
//        else
//        {
//            pattern[i] = rand() % 256;
//        }
//        //Src include this pattern, can from beginning, and other position
//        src[i] = pattern[i];    //src include pattern, pos is 0;
//        src[i + 8] = pattern[i];    //src include pattern, pos is 8;

//    }
//    int retBF = moUtils_Search_BF(src, 8 + patLen, pattern, patLen);
//    int retKmp = moUtils_Search_KMP(src, 8 + patLen, pattern, patLen);
//    int retBm = moUtils_Search_BM(src, 8 + patLen, pattern, patLen);
//    printf("src info : \n");
//    DumpArray(src, 8 + patLen);
//    printf("pattern info : \n");
//    DumpArray(pattern, patLen);
//    printf("After search, retBF=%d, retKmp=%d, retBm=%d\n",
//            retBF, retKmp, retBm);

    //A pattern include good suffix
//    memset(pattern, 0x00, 8);
//    memset(src, 0x00, 16);
//    strcpy(pattern, "abdab");
//    strcpy(src, "defdabdabce"); //good suffix is "ab", and exist in pattern
//    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
//    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern));
//    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern));
//    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d\n",
//            src, pattern, retBF, retKmp, retBm);
    
//    memset(pattern, 0x00, 8);
//    memset(src, 0x00, 16);
//    strcpy(pattern, "bdab");
//    strcpy(src, "xyzmbdxbce"); //good suffix is "b", and exist in pattern
//    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
//    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern));
//    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern));
//    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d\n",
//            src, pattern, retBF, retKmp, retBm);
    
//    memset(pattern, 0x00, 8);
//    memset(src, 0x00, 16);
//    strcpy(pattern, "bdab");
//    strcpy(src, "xyzmbdxbdabc"); //good suffix is "b", and exist in pattern
//    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
//    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern));
//    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern));
//    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d\n",
//            src, pattern, retBF, retKmp, retBm);

//    memset(pattern, 0x00, 8);
//    memset(src, 0x00, 16);
//    strcpy(pattern, "cadab");
//    strcpy(src, "defdabdabce"); //good suffix is "ab", donot exist in pattern
//    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
//    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern));
//    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern));
//    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d\n",
//            src, pattern, retBF, retKmp, retBm);

    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    strcpy(pattern, "cbab");
    strcpy(src, "xyzmbdxbce"); //good suffix is "b", donot exist in pattern
    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern));
    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern));
    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d\n",
            src, pattern, retBF, retKmp, retBm);
}

#define SRC_LEN     256
#define PAT_LEN     26

/*
    tst BF algo. is right or not, just use strstr to cmp result.
*/
static void tst_moUtils_Search_Rand(void)
{
    int cnt = 0;
    for(cnt = 0; cnt < 10240; cnt++)
    {
        unsigned char src[SRC_LEN] = {0x00};
        unsigned char pat[PAT_LEN] = {0x00};
    
        //Gen this two strings firstly
        int i = 0;
        for(i = 0; i < SRC_LEN - 1; i++)
        {
            src[i] = rand() % 26 + 65;
        }
        for(i = 0; i < PAT_LEN - 1; i++)
        {
    //        pat[i] = rand() % 26 + 65;
            pat[i] = src[i + 160];
        }
        //use BF firstly
        int retBF = moUtils_Search_BF(src, SRC_LEN - 1, pat, PAT_LEN - 1);
        //use KMP secondly
        int retKMP = moUtils_Search_KMP(src, SRC_LEN - 1, pat, PAT_LEN - 1);
        //use KMP secondly
        int retBM = moUtils_Search_BM(src, SRC_LEN - 1, pat, PAT_LEN - 1);
        //Cmp result
        if(retBF != retKMP || retBF != retBM)
        {
            printf("cnt = %d, retBF = %d, retKMP = %d, retBM = %d\n", cnt, retBF, retKMP, retBM);
            
            printf("src info : \n");
            DumpArray(src, SRC_LEN - 1);
            printf("pattern info : \n");
            DumpArray(pat, PAT_LEN - 1);

            break;
        }
    }
}


int main(int argc, char **argv)
{
    srand((unsigned int )time(NULL));
    
    moLoggerInit("./");
#if 0    
    tst_moUtils_File_getSize();

    tst_moUtils_File_getAbsFilePath();

    tst_moUtils_File_getAbsDirPath();

    tst_moUtils_File_getDirAndFilename();

    tst_moUtils_File_getFilepathSameState();
#endif

//    tst_moUtils_Search_Basic();

    tst_moUtils_Search_Rand();

    moLoggerUnInit();
    
    return 0;
}
