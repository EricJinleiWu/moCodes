#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include "cmdlineUI.h"
#include "utils.h"

static void dumpCryptInfo(const int cryptMethod, const int algoId, const char *key, 
        const char *srcFilepath, const char *dstFilepath)
{
    printf("\nDump your crypt info :\n");
    printf("cryptMethodId = %d, method is : [%s]\n", cryptMethod, getCryptMethodById(cryptMethod));
    printf("algoId = %d, algo is : [%s]\n", algoId, getAlgoNameById(algoId));
    printf("The key is [%s]\n", key);
    printf("The srcfilepath is [%s]\n", srcFilepath);
    printf("The dstfilepath is [%s]\n\n", dstFilepath);
}

void ShowCmdlineUI()
{
    while(1)
    {
        //Crypt method : encrypt or decrypt;
        printf("Do encrypt, input 1; \nDo decrypt, input 2;\n\t");
        char temp[16] = {0x00};
        memset(temp, 0x00, 16);
        scanf("%s", temp);
        int cryptMethod = 0;
        cryptMethod = atoi(temp);
        printf("cryptMethod = %d\n", cryptMethod);

        if(!isValidCryptMethod(cryptMethod))
        {
            printf("Input value [%d], donot valid!\n\n", cryptMethod);
            continue;
        }

        //Crypt algorithm : RC4, AES128, AES256, DES, 3DES
        printf("Currently support algorithms and each of them id is :\n");
        showSupportAlgos();
        printf("Input the id with which algorithm you want to use : ");
        memset(temp, 0x00, 16);
        scanf("%s", temp);
        int cryptAlgoId = atoi(temp);

        if(!isValidAlgoId(cryptAlgoId))
        {
            printf("Input algoId = %d, donot valid!\n\n", cryptAlgoId);
            continue;
        }

        //Crypt key, this is most important value!
        char key[1024] = {0x00};    //1024bytes is enough;
        memset(key, 0x00, 1024);
        printf("Input your key now:");
        scanf("%s", key);

        //source file path, this is important
        printf("Input the source filepath you want to do crypt : ");
        char srcFilepath[FILEPATH_LEN] = {0x00};
        memset(srcFilepath, 0x00, FILEPATH_LEN);
        scanf("%s", srcFilepath);

        if(!isFileExist(srcFilepath))
        {
            printf("Input filepath [%s] donot exist!\n\n", srcFilepath);
            continue;
        }

        //destination file path, important too, this path must be write.
        printf("Input the destination filepath you want to store crypt result : ");
        char dstFilepath[FILEPATH_LEN] = {0x00};
        memset(dstFilepath, 0x00, FILEPATH_LEN);
        scanf("%s", dstFilepath);

        dumpCryptInfo(cryptMethod, cryptAlgoId, key, srcFilepath, dstFilepath);

        //generate crypt info
        CRYPT_INFO cryptInfo;
        memset(&cryptInfo, 0x00, sizeof(CRYPT_INFO));
        cryptInfo.method = cryptMethod;
        cryptInfo.algoId = cryptAlgoId;
        cryptInfo.key = key;
        cryptInfo.keyLen = strlen(key);
        strcpy(cryptInfo.srcFilepath, srcFilepath);
        strcpy(cryptInfo.dstFilepath, dstFilepath);

        //do crypt 
        int ret = doCrypt(&cryptInfo);
        if(0 != ret)
        {
            printf("doCrypt failed! ret = %d. check for reason!\n\n", ret);
        }
        else
        {
            printf("doCrypt succeed.\n\n");
        }

        //Looply or exit
        printf("Do crypt to another file? y / n \n");
        char loop[8] = {0x00};
        scanf("%s", loop);
        //If donot input "n" or "no", we think is "yes"
        if(0 == strcmp(loop, "n") || 0 == strcmp(loop, "no"))
        {
            break;
        }
        else
        {
            continue;
        }
    }
}


