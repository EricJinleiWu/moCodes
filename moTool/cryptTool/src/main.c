/*
wujl, create at 20161222, a tool to encrypt files, after encrypt, I can upload my files to website for saving;

	V1.0.0, create;
	        1.just do encrypt and decrypt to file, donot support directory;
	        2.just can be used in cmdline, donot have UI;
	        3.just support a RC4 encrypt;

	        Bug I know: when input anything in cmdline, space will not be dealed because I used scanf("%s", str) to get your input msg;
	                    I tried several methods, like : fflush()+gets(); fflush()+fgets(); scanf("%[^\n]"); But donot OK;
	                    So I just use it......

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "cmdlineUI.h"

#define UI_CMDLINE  1
#define UI_QT       0

typedef enum
{
    UI_TYPE_CMDLINE,
    UI_TYPE_QT
}UI_TYPE;

static UI_TYPE gUiType = UI_TYPE_CMDLINE;

static void showAuthorInfo(void)
{
    printf("\n\tTool by EricWu, You can connect me with email : wujl_0351@163.com\n\n");
}

static void signalInt(int val)
{
    printf("\n\tReceive ctrl+c, tool will exit now.\n");
    showAuthorInfo();
    exit(0);
}

int main(int argc, char **argv)
{
    //register a function to ctrl+c
    signal(SIGINT, signalInt);
    
    showAuthorInfo();

    switch(gUiType)
    {
        case UI_TYPE_CMDLINE:
        {
            ShowCmdlineUI();
        }
        break;

        case UI_TYPE_QT:
        {
            //TODO, exec this function
//            ShowQtUI();
        }
        break;

        default:
            printf("gUiType = %d, cannot deal with it currently;\n");
            break;
    }

    printf("\n\ttool will exit now.\n");
    showAuthorInfo();
    
    return 0;
}
