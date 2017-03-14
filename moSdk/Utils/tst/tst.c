#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "moUtils.h"

/* Do test to moUitls_File module. */
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

int main(int argc, char **argv)
{
    moLoggerInit("./");
    
    tst_moUtils_File_getSize();

    tst_moUtils_File_getAbsFilePath();

    tst_moUtils_File_getAbsDirPath();

    tst_moUtils_File_getDirAndFilename();

    tst_moUtils_File_getFilepathSameState();

    moLoggerUnInit();
    
    return 0;
}
