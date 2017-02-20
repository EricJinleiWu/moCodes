#include <stdio.h>
#include <string.h>

#include "moIniParser.h"

void testBasicParser(const char *filepath)
{
    SECTION_INFO_NODE *pHeadNode = NULL;
	pHeadNode = moIniParser_Init(filepath);
    if(NULL == pHeadNode)
    {
        printf("wjl_test : moIniParser_Init with filepath = [%s] failed!\n", filepath);
    }
    else
    {
        printf("wjl_test : moIniParser_Init with filepath = [%s] succeed!\n", filepath);        
        
        moIniParser_DumpAllInfo(pHeadNode);
        
        moIniParser_UnInit(pHeadNode);
    }
}


int main(int argc, char **argv)
{
	if(2 != argc)
	{
		fprintf(stderr, "Usage : %s iniFilePath\n", argv[0]);
		return -1;
	}

	testBasicParser(argv[1]);

	return 0;
}
