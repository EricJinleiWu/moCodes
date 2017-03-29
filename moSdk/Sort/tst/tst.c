#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

#include "moSort.h"
#include "moLogger.h"

#define MAX_ARRAY_VALUE				(0xffff)

static int GenIntArray(int *pArray, const unsigned int len)
{
	if(NULL == pArray)
	{
		printf("Input param is NULL!\n");
		return -1;
	}

	srand((unsigned int)time(NULL));

	int i = 0;
	for(; i < len; i++)
	{
		*(pArray + i) = rand() % MAX_ARRAY_VALUE;
	}
	return 0;
}

static void DumpArray(int *pArray, const unsigned int len)
{
	printf("Dump array :\n\t");
	int i = 0;
	for(; i < len; i++)
	{
		printf("0x%04x ", *(pArray + i));
	}
	printf("\nDump over.\n");
}

/* 1.Generate an random array firstly;
 * 2.Sort it with moSort_directInsert;
 * */
int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("Usage : %s ArrayLength\n", argv[0]);
		return -1;
	}

    int ret = moLoggerInit("./");
    if(0 != ret)
    {
        printf("Init moLogger failed! ret = %d\n", ret);
        return -2;
    }

	int arrayLength = atoi(argv[1]);
	printf("The array length = [%d], will sort this array.\n", arrayLength);

	int *pArray = NULL;
	pArray = (int *)malloc(arrayLength * (sizeof(int)));
	if(NULL == pArray)
	{
		printf("Malloc for array failed!\n");
        moLoggerUnInit();
		return -2;
	}

	ret = GenIntArray(pArray, arrayLength);
	if(0 != ret)
	{
		printf("Generate array failed! ret = %d\n", ret);
		free(pArray);
        moLoggerUnInit();
		return -3;
	}

	DumpArray(pArray, arrayLength);

	ret = moSort_directInsertSort(pArray, arrayLength, MOSORT_DST_SEQUENCE_B2L);
	if(0 != ret)
	{
		printf("Do direct insert sort failed, ret = %d\n", ret);
		free(pArray);
        moLoggerUnInit();
		return -4;
	}

	DumpArray(pArray, arrayLength);

	printf("Do direct insert sort over.\n");
    
    moLoggerUnInit();

	return 0;
}
