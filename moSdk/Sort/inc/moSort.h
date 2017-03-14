#ifndef __MO_SORT_H__
#define __MO_SORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moLogger.h"

#define MOSORT_LOGGER_MODULE_NAME  "MOSORT"

typedef enum
{
	MOSORT_DST_SEQUENCE_L2B,	//The sorted array will from little to big
	MOSORT_DST_SEQUENCE_B2L,	//The sorted array will from big to little
}MOSORT_DST_SEQUENCE;

/*
    Do sort to @pArray, and the result will set to this array, too.

    @param:
    	pArray : The src array should be sorted, and the dst array, too.
    	arrayLength : The length of @pArray;
    	seq : The dst array sequence;

    @return:
    	0 : sort OK, pArray is sorted array;
    	0- : sort failed;
*/
int moSort_directInsertSort(int *pArray, const unsigned int arrayLength, const MOSORT_DST_SEQUENCE seq);


#ifdef __cplusplus
}
#endif

#endif
