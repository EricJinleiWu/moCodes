#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace std;

#include "uiTerminal.h"
#include "cpSocket.h"

UiTerminal::UiTerminal() : UI()
{
    mProgress = 0;
    mState = UI_CRYPT_STATE_IDLE;
    memset(&mTaskInfo, 0x00, sizeof(MOCFT_TASKINFO));
    mpCpClient = new CpClientSocket();
    mpCpClient->addListener(this);
    
    //start its thread
    dynamic_cast<CpClientSocket *>(mpCpClient)->run();
}

UiTerminal::UiTerminal(const UiTerminal & other)
{
    mProgress = other.mProgress;
    mState = other.mState;
    memcpy(&mTaskInfo, &other.mTaskInfo, sizeof(MOCFT_TASKINFO));
    mpCpClient = other.mpCpClient;
}

UiTerminal::~UiTerminal()
{
    mpCpClient->removeListener(this);
    
    if(NULL != mpCpClient)
        delete mpCpClient;
}

void UiTerminal::showBasicInfo()
{
    printf("Name : MoCryptFileTool\nAuthor : EricWu\nVersion : %s\nDescription : " \
        "A tool to do crypt operation to a file.\n", MOCFT_VERSION);
}

int UiTerminal::getRequest()
{
    char tmp[MOCRYPT_FILEPATH_LEN] = {0x00};
    
    printf("Crypt method(enc/dec) : ");
    memset(tmp, 0x00, MOCRYPT_FILEPATH_LEN);
    scanf("%s", tmp);
    if(0 == strcmp(tmp, "enc"))
    {
        mTaskInfo.method = MOCRYPT_METHOD_ENCRYPT;
    }
    else if(0 == strcmp(tmp, "dec"))
    {
        mTaskInfo.method = MOCRYPT_METHOD_DECRYPT;
    }
    else
    {
        printf("Input value [%s] donot valid! just support [\"enc\", \"dec\"]", tmp);
        return -1;
    }

    printf("Crypt algorithm(RC4/BASE64) : ");
    memset(tmp, 0x00, MOCRYPT_FILEPATH_LEN);
    scanf("%s", tmp);
    if(0 == strcmp(tmp, "RC4"))
    {
        mTaskInfo.algo = MOCFT_ALGO_RC4;
    }
    else if(0 == strcmp(tmp, "BASE64"))
    {
        mTaskInfo.algo = MOCFT_ALGO_BASE64;
    }
    else
    {
        printf("Input value [%s] donot valid! just support [\"RC4\", \"BASE64\"]", tmp);
        return -2;
    }

    //In RC4, we need a key to do crypt, other algos, like AES, DES, and so on, all need key
    if(mTaskInfo.algo == MOCFT_ALGO_RC4)
    {
        printf("Key : ");
        memset(tmp, 0x00, MOCRYPT_KEY_MAXLEN);
        scanf("%s", tmp);
        strcpy(mTaskInfo.key, tmp);

        mTaskInfo.keyLen = strlen(mTaskInfo.key);
    }

    printf("Src filepath : ");
    memset(tmp, 0x00, MOCRYPT_FILEPATH_LEN);
    scanf("%s", tmp);
    strcpy(mTaskInfo.srcfilepath, tmp);

    printf("Dst filepath : ");
    memset(tmp, 0x00, MOCRYPT_FILEPATH_LEN);
    scanf("%s", tmp);
    strcpy(mTaskInfo.dstfilepath, tmp);

    return 0;
}


void UiTerminal::showHelp()
{
    showBasicInfo();
}

void UiTerminal::setProgress(const int prog)
{
    mProgress = prog;
}

//TODO, can break from loop
void UiTerminal::run()
{
    showHelp();

    while(true)
    {
        //In one moment, just one task can be done
        if(mState != UI_CRYPT_STATE_IDLE)
        {
            sleep(1);
        }

        memset(&mTaskInfo, 0x00, sizeof(MOCFT_TASKINFO));
        int ret = getRequest();
        if(ret != 0)
        {
            printf("Input value error!\n");
            continue;
        }

        ret = mpCpClient->sendReq(mTaskInfo);
        if(ret != 0)
        {
            printf("Send request to background server failed! ret = %d\n", ret);
            continue;
        }

        mState = UI_CRYPT_STATE_CRYPTING;

        showProgBar();
    }
}

void UiTerminal::showProgBar()
{
    while(mState == UI_CRYPT_STATE_CRYPTING)
    {
        //Some error occurred when crypting, should check for why
        if(mProgress < 0)
        {
            printf("\tError when crypting! progress = %d, check for why!\n", mProgress);
            //reset mState and mProgress
            mProgress = 0;
            mState = UI_CRYPT_STATE_IDLE;
        }
        else
        {
            if(0 != mProgress && 0 == mProgress % 10)
                printf("\tProgress : %d%%\n", mProgress);
            if(mProgress >= 100)
            {
                printf("Crypt over! srcfilepath=[%s], dstfilepath=[%s], method=[%d], algo=[%d]\n",
                    mTaskInfo.srcfilepath, mTaskInfo.dstfilepath, mTaskInfo.method, mTaskInfo.algo);

                //reset mState and mProgress
                mProgress = 0;
                mState = UI_CRYPT_STATE_IDLE;
            }
        }
    }
}

