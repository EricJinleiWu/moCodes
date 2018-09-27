#ifndef __MYTEST_UTILS_H__
#define __MYTEST_UTILS_H__


#define dbgDebug(format, ...)
//#define dbgDebug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgError(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)


#define MAX_FILEPATH_LEN    256

typedef enum
{
    OUTPUT_PICTYPE_PGM,
    OUTPUT_PICTYPE_PNG,
    
    OUTPUT_PICTYPE_MAX
}OUTPUT_PICTYPE;

#endif
