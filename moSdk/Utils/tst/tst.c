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
    unsigned char kmpNext[8] = {0x00};
    unsigned char bmBct[MOUTILS_SEARCH_BM_BCT_LEN] = {0x00};
    unsigned char bmGst[8] = {0x00};
    unsigned char sundayNext[MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN] = {0X00};
    
#if 0
    //src and pattern has same length
    unsigned int patLen = rand() % 8;
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    int i = 0;
    for(i = 0; i < patLen; i++)
    {
        //A pattern in randm
        pattern[i] = rand() % 256;
        src[i] = pattern[i];    //same string
//        src[i] = (pattern[i] + 1) % 256;    //different string
        
    }
    int retBF = moUtils_Search_BF(src, patLen, pattern, patLen);
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, patLen);
    int retKmp = moUtils_Search_KMP(src, patLen, pattern, patLen, kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, patLen);
    moUtils_Search_BM_GenGST(bmGst, pattern, patLen);
    int retBm = moUtils_Search_BM(src, patLen, pattern, patLen, bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, patLen);
    int retSunday = moUtils_Search_Sunday(src, patLen, pattern, patLen, sundayNext);
    printf("src info : \n");
    DumpArray(src, patLen);
    printf("pattern info : \n");
    DumpArray(pattern, patLen);
    printf("After search, retBF=%d, retKmp=%d, retBm=%d, retSunday = %d\n",
            retBF, retKmp, retBm, retSunday);
#endif

#if 0
    //A pattern include same charactor 
    unsigned int patLen = rand() % 8;
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    int i = 0;
    for(i = 0; i < patLen; i++)
    {        
        //A pattern include a duplicate charactor
        if(i == patLen / 2 && i > 0)
        {
            pattern[i] = pattern[i - 1];
        }
        else
        {
            pattern[i] = rand() % 256;
        }
        //Src include this pattern, can from beginning, and other position
//        src[i] = pattern[i];    //src include pattern, pos is 0;
        src[i + 8] = pattern[i];    //src include pattern, pos is 8;
    }
    int retBF = moUtils_Search_BF(src, patLen + 8, pattern, patLen);
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, patLen);
    int retKmp = moUtils_Search_KMP(src, patLen + 8, pattern, patLen, kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, patLen);
    moUtils_Search_BM_GenGST(bmGst, pattern, patLen);
    int retBm = moUtils_Search_BM(src, patLen + 8, pattern, patLen, bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, patLen);
    int retSunday = moUtils_Search_Sunday(src, patLen + 8, pattern, patLen, sundayNext);
    printf("src info : \n");
    DumpArray(src, 8 + patLen);
    printf("pattern info : \n");
    DumpArray(pattern, patLen);
    printf("After search, retBF=%d, retKmp=%d, retBm=%d, retSunday = %d\n",
            retBF, retKmp, retBm, retSunday);
#endif

#if 0
    //A pattern include good suffix
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    strcpy(pattern, "abdab");
    strcpy(src, "defdabdabce"); //good suffix is "ab", and exist in pattern
    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, strlen(pattern));
    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern), kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, strlen(pattern));
    moUtils_Search_BM_GenGST(bmGst, pattern, strlen(pattern));
    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern), bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, strlen(pattern));
    int retSunday = moUtils_Search_Sunday(src, strlen(src), pattern, strlen(pattern), sundayNext);
    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d, retSunday=%d\n",
            src, pattern, retBF, retKmp, retBm, retSunday);
#endif

#if 0
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    strcpy(pattern, "bdab");
    strcpy(src, "xyzmbdxbce"); //good suffix is "b", and exist in pattern
    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, strlen(pattern));
    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern), kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, strlen(pattern));
    moUtils_Search_BM_GenGST(bmGst, pattern, strlen(pattern));
    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern), bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, strlen(pattern));
    int retSunday = moUtils_Search_Sunday(src, strlen(src), pattern, strlen(pattern), sundayNext);
    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d, retSunday=%d\n",
            src, pattern, retBF, retKmp, retBm, retSunday);
#endif

#if 0
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    strcpy(pattern, "bdab");
    strcpy(src, "xyzmbdxbdabc"); //good suffix is "b", and exist in pattern
    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, strlen(pattern));
    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern), kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, strlen(pattern));
    moUtils_Search_BM_GenGST(bmGst, pattern, strlen(pattern));
    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern), bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, strlen(pattern));
    int retSunday = moUtils_Search_Sunday(src, strlen(src), pattern, strlen(pattern), sundayNext);
    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d, retSunday=%d\n",
            src, pattern, retBF, retKmp, retBm, retSunday);
#endif

#if 0
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    strcpy(pattern, "cadab");
    strcpy(src, "defdabdabce"); //good suffix is "ab", donot exist in pattern
    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, strlen(pattern));
    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern), kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, strlen(pattern));
    moUtils_Search_BM_GenGST(bmGst, pattern, strlen(pattern));
    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern), bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, strlen(pattern));
    int retSunday = moUtils_Search_Sunday(src, strlen(src), pattern, strlen(pattern), sundayNext);
    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d, retSunday=%d\n",
            src, pattern, retBF, retKmp, retBm, retSunday);
#endif 

#if 1
    memset(pattern, 0x00, 8);
    memset(src, 0x00, 16);
    memset(kmpNext, 0x00, 8);
    memset(bmBct, 0x00, MOUTILS_SEARCH_BM_BCT_LEN);
    memset(bmGst, 0x00, 8);
    memset(sundayNext, 0x00, MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN);
    strcpy(pattern, "cbab");
    strcpy(src, "xyzmbdxbcecba"); //good suffix is "b", donot exist in pattern
    int retBF = moUtils_Search_BF(src, strlen(src), pattern, strlen(pattern));
    moUtils_Search_KMP_GenNextArray(kmpNext, pattern, strlen(pattern));
    int retKmp = moUtils_Search_KMP(src, strlen(src), pattern, strlen(pattern), kmpNext);
    moUtils_Search_BM_GenBCT(bmBct, pattern, strlen(pattern));
    moUtils_Search_BM_GenGST(bmGst, pattern, strlen(pattern));
    int retBm = moUtils_Search_BM(src, strlen(src), pattern, strlen(pattern), bmBct, bmGst);
    moUtils_Search_Sunday_GenNextTable(sundayNext, pattern, strlen(pattern));
    int retSunday = moUtils_Search_Sunday(src, strlen(src), pattern, strlen(pattern), sundayNext);
    printf("After search, src = [%s], pattern = [%s], retBF=%d, retKmp=%d, retBm=%d, retSunday=%d\n",
            src, pattern, retBF, retKmp, retBm, retSunday);
#endif
}

#define SRC_LEN     2560
#define PAT_LEN     26

/*
    tst many times to check our algo. return right or not.
*/
static void tst_moUtils_Search_Rand(void)
{
    int cnt = 0;
    for(cnt = 0; cnt < 102400; cnt++)
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
//            pat[i] = rand() % 26 + 65;
            pat[i] = src[i + 160];
        }
        //use BF firstly
        int retBF = moUtils_Search_BF(src, SRC_LEN - 1, pat, PAT_LEN - 1);
        //use KMP secondly
        unsigned char kmpNext[PAT_LEN] = {0x00};
        moUtils_Search_KMP_GenNextArray(kmpNext, pat, PAT_LEN - 1);
        int retKMP = moUtils_Search_KMP(src, SRC_LEN - 1, pat, PAT_LEN - 1, kmpNext);
        //use BM thirdly
        unsigned char bmBct[MOUTILS_SEARCH_BM_BCT_LEN] = {0x00};
        moUtils_Search_BM_GenBCT(bmBct, pat, PAT_LEN - 1);
        unsigned char bmGst[PAT_LEN] = {0x00};
        moUtils_Search_BM_GenGST(bmGst, pat, PAT_LEN - 1);
        int retBM = moUtils_Search_BM(src, SRC_LEN - 1, pat, PAT_LEN - 1, bmBct, bmGst);
        //use Sunday lastly
        unsigned char sundayNext[MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN] = {0x00};
        moUtils_Search_Sunday_GenNextTable(sundayNext, pat, PAT_LEN - 1);
        int retSunday = moUtils_Search_Sunday(src, SRC_LEN - 1, pat, PAT_LEN - 1, sundayNext);
        //Cmp result
        if(retBF != retKMP || retBF != retBM || retBF != retSunday)
        {
            printf("cnt = %d, retBF = %d, retKMP = %d, retBM = %d, retSunday = %d\n", 
                cnt, retBF, retKMP, retBM, retSunday);
            
            printf("src info : \n");
            DumpArray(src, SRC_LEN - 1);
            printf("pattern info : \n");
            DumpArray(pat, PAT_LEN - 1);

            break;
        }
    }
}

static void tst_moUtils_Search_Effencicy(void)
{
    unsigned char src[SRC_LEN] = {0x00};
    unsigned char pat[PAT_LEN] = {0x00};

    //Gen this two strings firstly
    int i = 0;
    for(i = 0; i < SRC_LEN - 1; i++)
    {
        src[i] = rand() % 26 + 65;
    }
    printf("src info : \n");
    DumpArray(src, SRC_LEN - 1);
    for(i = 0; i < PAT_LEN - 1; i++)
    {
        pat[i] = rand() % 26 + 65;
        pat[i] = src[i + 2160];
    }
    printf("pattern info : \n");
    DumpArray(pat, PAT_LEN - 1);
    
    int cnt = 0, ret = 0, timeMaxTimes = 102400;

    //BF algo. firstly
    struct timeval beforeTime;
    gettimeofday(&beforeTime, NULL);
    for(cnt = 0; cnt < timeMaxTimes; cnt++)
    {
        //use BF firstly
        ret = moUtils_Search_BF(src, SRC_LEN - 1, pat, PAT_LEN - 1);
    }
    struct timeval afterTime;
    gettimeofday(&afterTime, NULL);
    //calc the difftime
    double difftime = (afterTime.tv_usec >= beforeTime.tv_usec) ? 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 + (afterTime.tv_usec - beforeTime.tv_usec) / 1000) : 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 - 1 + (afterTime.tv_usec + 1000 - beforeTime.tv_usec) / 1000);
    printf("BF algo, %d times, ret = %d, difftime = %f\n", timeMaxTimes, ret, difftime);
    
    //KMP algo. secondly
    gettimeofday(&beforeTime, NULL);
    unsigned char kmpNext[PAT_LEN] = {0x00};
    moUtils_Search_KMP_GenNextArray(kmpNext, pat, PAT_LEN -1);
    for(cnt = 0; cnt < timeMaxTimes; cnt++)
    {
        //use KMP secondly
        ret = moUtils_Search_KMP(src, SRC_LEN - 1, pat, PAT_LEN - 1, kmpNext);
    }
    gettimeofday(&afterTime, NULL);
    //calc the difftime
    difftime = (afterTime.tv_usec >= beforeTime.tv_usec) ? 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 + (afterTime.tv_usec - beforeTime.tv_usec) / 1000) : 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 - 1 + (afterTime.tv_usec + 1000 - beforeTime.tv_usec) / 1000);
    printf("KMP algo, %d times, ret = %d, difftime = %f\n", timeMaxTimes, ret, difftime);
    
    //BM algo. thirdly
    gettimeofday(&beforeTime, NULL);
    unsigned char bmBct[MOUTILS_SEARCH_BM_BCT_LEN] = {0x00};
    moUtils_Search_BM_GenBCT(bmBct, pat, PAT_LEN - 1);
    unsigned char bmGst[PAT_LEN] = {0x00};
    moUtils_Search_BM_GenGST(bmGst, pat, PAT_LEN - 1);
    for(cnt = 0; cnt < timeMaxTimes; cnt++)
    {
        //use BM thirdly
        ret = moUtils_Search_BM(src, SRC_LEN - 1, pat, PAT_LEN - 1, bmBct, bmGst);
    }
    gettimeofday(&afterTime, NULL);
    //calc the difftime
    difftime = (afterTime.tv_usec >= beforeTime.tv_usec) ? 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 + (afterTime.tv_usec - beforeTime.tv_usec) / 1000) : 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 - 1 + (afterTime.tv_usec + 1000 - beforeTime.tv_usec) / 1000);
    printf("BM algo, %d times, ret = %d, difftime = %f\n", timeMaxTimes, ret, difftime);
    
    //Sunday algo. fourthly
    gettimeofday(&beforeTime, NULL);
    unsigned char sundayNext[MOUTILS_SEARCH_SUNDAY_NEXTTABLE_LEN] = {0x00};
    moUtils_Search_Sunday_GenNextTable(sundayNext, pat, PAT_LEN - 1);
    for(cnt = 0; cnt < timeMaxTimes; cnt++)
    {
        //use sunday lastly
        ret = moUtils_Search_Sunday(src, SRC_LEN - 1, pat, PAT_LEN - 1, sundayNext);
    }
    gettimeofday(&afterTime, NULL);
    //calc the difftime
    difftime = (afterTime.tv_usec >= beforeTime.tv_usec) ? 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 + (afterTime.tv_usec - beforeTime.tv_usec) / 1000) : 
        ((afterTime.tv_sec - beforeTime.tv_sec) * 1000 - 1 + (afterTime.tv_usec + 1000 - beforeTime.tv_usec) / 1000);
    printf("Sunday algo, %d times, ret = %d, difftime = %f\n", timeMaxTimes, ret, difftime);
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

//    tst_moUtils_Search_Rand();

    tst_moUtils_Search_Effencicy();

    moLoggerUnInit();
    
    return 0;
}
