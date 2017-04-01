#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

#include "moSort.h"
#include "moLogger.h"

#define MAX_ARRAY_VALUE				(0xffff)
#define ARRAY_LEN   256

static int isL2B(const int *pArray)
{
    int ret = 1;
    int i = 0;
    for(; i < ARRAY_LEN - 1; i++)
    {
        if(pArray[i] > pArray[i + 1])
        {
            ret = 0;
            break;
        }
    }
    return ret;
}

static int isB2L(const int *pArray)
{
    int ret = 1;
    int i = 0;
    for(; i < ARRAY_LEN - 1; i++)
    {
        if(pArray[i] < pArray[i + 1])
        {
            ret = 0;
            break;
        }
    }
    return ret;
}

static int GenIntArray(int *pArray)
{
	if(NULL == pArray)
	{
		printf("Input param is NULL!\n");
		return -1;
	}

	srand((unsigned int)time(NULL));

	int i = 0;
	for(; i < ARRAY_LEN; i++)
	{
		*(pArray + i) = rand() % MAX_ARRAY_VALUE;
	}
	return 0;
}

static void DumpArray(int *pArray)
{
	printf("Dump array :\n\t");
	int i = 0;
	for(; i < ARRAY_LEN; i++)
	{
		printf("0x%04x ", *(pArray + i));
	}
	printf("\nDump over.\n");
}

static void tst_moSort_directInsertSort(const int *pOrgArray, const MOSORT_DST_SEQUENCE seq)
{
    int array[ARRAY_LEN] = {0x00};
    //Copy org array to local
    memcpy(array, pOrgArray, ARRAY_LEN);
    int ret = moSort_directInsertSort(array, ARRAY_LEN, seq);
    if(ret != 0)
    {
        printf("moSort_directInsertSort failed! ret = %d\n", ret);
    }
    else
    {
        switch(seq)
        {
            case MOSORT_DST_SEQUENCE_B2L:
            {
                if(isB2L(array))
                {
                    printf("Sort with Direct insert method succeed.\n");
                }
                else
                {
                    printf("Sort with Direct insert method failed! Org array info showing : \n");
                    DumpArray(pOrgArray);
                    printf("Sorted array info showing : \n");
                    DumpArray(array);
                    printf("Check for why!!\n");
                }
            }
            break;
            case MOSORT_DST_SEQUENCE_L2B:
            {
                if(isL2B(array))
                {
                    printf("Sort with Direct insert method succeed.\n");
                }
                else
                {
                    printf("Sort with Direct insert method failed! Org array info showing : \n");
                    DumpArray(pOrgArray);
                    printf("Sorted array info showing : \n");
                    DumpArray(array);
                    printf("Check for why!!\n");
                }
            }
            break;
            default:
                break;
        }
    }
}


static void tst_moSort_selectSort(const int *pOrgArray, const MOSORT_DST_SEQUENCE seq)
{
    int array[ARRAY_LEN] = {0x00};
    //Copy org array to local
    memcpy(array, pOrgArray, ARRAY_LEN);
    int ret = moSort_selectSort(array, ARRAY_LEN, seq);
    if(ret != 0)
    {
        printf("moSort_selectSort failed! ret = %d\n", ret);
    }
    else
    {
        switch(seq)
        {
            case MOSORT_DST_SEQUENCE_B2L:
            {
                if(isB2L(array))
                {
                    printf("Sort with select method succeed.\n");
                }
                else
                {
                    printf("Sort with select method failed! Org array info showing : \n");
                    DumpArray(pOrgArray);
                    printf("Sorted array info showing : \n");
                    DumpArray(array);
                    printf("Check for why!!\n");
                }
            }
            break;
            case MOSORT_DST_SEQUENCE_L2B:
            {
                if(isL2B(array))
                {
                    printf("Sort with select method succeed.\n");
                }
                else
                {
                    printf("Sort with select method failed! Org array info showing : \n");
                    DumpArray(pOrgArray);
                    printf("Sorted array info showing : \n");
                    DumpArray(array);
                    printf("Check for why!!\n");
                }
            }
            break;
            default:
                break;
        }
    }
}


static void tst_moSort_bubbleSort(const int *pOrgArray, const MOSORT_DST_SEQUENCE seq)
{
    int array[ARRAY_LEN] = {0x00};
    //Copy org array to local
    memcpy(array, pOrgArray, ARRAY_LEN);
    int ret = moSort_bubbleSort(array, ARRAY_LEN, seq);
    if(ret != 0)
    {
        printf("moSort_bubbleSort failed! ret = %d\n", ret);
    }
    else
    {
        switch(seq)
        {
            case MOSORT_DST_SEQUENCE_B2L:
            {
                if(isB2L(array))
                {
                    printf("Sort with bubble method succeed.\n");
                }
                else
                {
                    printf("Sort with bubble method failed! Org array info showing : \n");
                    DumpArray(pOrgArray);
                    printf("Sorted array info showing : \n");
                    DumpArray(array);
                    printf("Check for why!!\n");
                }
            }
            break;
            case MOSORT_DST_SEQUENCE_L2B:
            {
                if(isL2B(array))
                {
                    printf("Sort with bubble method succeed.\n");
                }
                else
                {
                    printf("Sort with bubble method failed! Org array info showing : \n");
                    DumpArray(pOrgArray);
                    printf("Sorted array info showing : \n");
                    DumpArray(array);
                    printf("Check for why!!\n");
                }
            }
            break;
            default:
                break;
        }
    }
}


/* 1.Generate an random array firstly;
 * 2.Sort it with moSort_directInsert;
 * */
int main(int argc, char **argv)
{
    int ret = moLoggerInit("./");
    if(0 != ret)
    {
        printf("Init moLogger failed! ret = %d\n", ret);
        return -1;
    }

    int pArray[ARRAY_LEN] = {0x00};
    memset(pArray, 0x00, ARRAY_LEN);
	ret = GenIntArray(pArray);
	if(0 != ret)
	{
		printf("Generate array failed! ret = %d\n", ret);
        moLoggerUnInit();
		return -2;
	}

	DumpArray(pArray);

    tst_moSort_directInsertSort(pArray, MOSORT_DST_SEQUENCE_B2L);
    tst_moSort_selectSort(pArray, MOSORT_DST_SEQUENCE_B2L);
    tst_moSort_bubbleSort(pArray, MOSORT_DST_SEQUENCE_B2L);
    
    tst_moSort_directInsertSort(pArray, MOSORT_DST_SEQUENCE_L2B);
    tst_moSort_selectSort(pArray, MOSORT_DST_SEQUENCE_L2B);
    tst_moSort_bubbleSort(pArray, MOSORT_DST_SEQUENCE_L2B);
    
    moLoggerUnInit();

	return 0;
}
