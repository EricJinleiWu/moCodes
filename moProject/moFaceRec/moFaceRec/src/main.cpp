#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "server.h"
#include "client.h"
#include "utils.h"
#include "fileMgr.h"
#include "faceOper.h"

using namespace std;

#include <list>
#include <string>

/*
    create a directory;
*/
static int createDir(string & dirpath)
{
    if(access(dirpath.c_str(), 0) == 0)
    {
        dbgError("dir [%s] has been exist, cannot create it again!\n", dirpath.c_str());
        return -1;
    }
    dbgDebug("dir [%s] donot exist, will create it now.", dirpath.c_str());
    
    int ret = mkdir(dirpath.c_str(), 0777);
    if(ret != 0)
    {
        dbgError("Create captDirpath [%s] failed! ret=%d, errno=%d, desc=[%s]\n",
            dirpath.c_str(), ret, errno, strerror(errno));
        //try mkdir order
        string cmd("mkdir -p ");
        cmd += dirpath;
        dbgDebug("Now, try to use shell order to mkdir. order is [%s]\n", cmd.c_str());
        system(cmd.c_str());
        if(access(dirpath.c_str(), 0) != 0)
        {
            //mkdir failed too
            dbgError("Use shell to create directory [%s] failed, too! check for why!\n", dirpath.c_str());
            return -2;
        }
        else
        {
            dbgDebug("Use shell to create directory [%s] succeed.\n", dirpath.c_str());
            return 0;
        }
    }
    else
    {
        dbgDebug("mkdir for [%s] succeed.\n", dirpath.c_str());
        return 0;
    }
}

/*
    if have jpgs in, parse its name to get params, then set to baseInfo;
*/
static int initBaseFiles(string & baseDirpath)
{
    return FileMgrSingleton::getInstance()->getBaseFaceFeatures();
}

/*
    If dir exist yet, get all files:
        if filename in right format, parse it to get all params we needed;
        insert it to FileMgr;
*/
static int initCaptFiles(string & captDirpath)
{
    int ret = 0;
    //direcotry has been existed yet, we should get all files in it, and deal with them looply
    DIR * pDir = opendir(captDirpath.c_str());
    if(NULL == pDir)
    {
        dbgError("opendir for [%s] failed! errno=%d, desc=[%s]\n", captDirpath.c_str(), errno, strerror(errno));
        return -1;
    }

    list<string > delFilenameList;
    delFilenameList.clear();

    struct dirent * pCurFile;
    while((pCurFile = readdir(pDir)) != NULL)
    {
        if(0 == strcmp(pCurFile->d_name, ".") || 0 == strcmp(pCurFile->d_name, ".."))
            continue;

        time_t timestamp = 0;
        int width = 0, height = 0;
        unsigned long cameraIp = 0;
        ret = sscanf(pCurFile->d_name, CAPT_FILE_FORMAT, &timestamp, &width, &height, &cameraIp);
        if(ret != CAPT_FILE_PARAM_NUM)
        {
            dbgError("captDirpath=[%s], filename=[%s], donot in right filename format!\n",
                captDirpath.c_str(), pCurFile->d_name);

            string curFilename(pCurFile->d_name);
            delFilenameList.push_back(curFilename);

            continue;
        }

        CaptFileInfo curFileInfo(width, height, timestamp, cameraIp);
        ret = FileMgrSingleton::getInstance()->insertCaptFileTask(curFileInfo);
        if(ret != 0)
        {
            dbgError("insertCaptFileTask failed! fileInfo:[w=%d, h=%d, t=%ld, ip=%lu]\n",
                width, height, timestamp, cameraIp);

            string curFilename(pCurFile->d_name);
            delFilenameList.push_back(curFilename);

            continue;
        }
        FileMgrSingleton::getInstance()->notifyCaptFileTask();
    }

    closedir(pDir);
    pDir = NULL;

    //Delete all files which donot valid
    for(list<string>::iterator it = delFilenameList.begin(); it != delFilenameList.end(); it++)
    {
        string curFilename = *it;
        string curFilepath = captDirpath + "/" + curFilename;
        dbgDebug("current filepath is [%s], should delete it now.\n", curFilepath.c_str());
        unlink(curFilepath.c_str());
    }
    delFilenameList.clear();
    
    return 0;
}

/*
    V1.0.0:
        1.start httpserver, can recv the picture being sent from PI, and save all results into db;

    V1.0.1:
        1.can get config of PI, and set new config to PI;

    V1.0.2:
        1.have a UI is better;
*/
int main(int argc, char ** argv)
{
    srand((unsigned int)time(NULL));

    FaceOper * pFaceOper = new FaceOper();
    int ret = pFaceOper->register2Arc();
    if(ret != 0)
    {
        dbgError("register to arc failed! ret=%d\n", ret);
        return -1;
    }
    dbgDebug("regitster to arc succeed.\n");

    //if dir donot exist, just mkdir it
    string captDirpath(DEFAULT_CAPT_DIRPATH);
    if(access(captDirpath.c_str(), 0) != 0 && createDir(captDirpath) != 0)
    {
        dbgError("CaptDir [%s] donot exist, we create it failed!\n", captDirpath.c_str());
        delete pFaceOper;
        pFaceOper = NULL;
        return -2;
    }
    string baseDirpath(DEFAULT_BASE_DIRPATH);
    if(access(baseDirpath.c_str(), 0) != 0 && createDir(baseDirpath) != 0)
    {
        dbgError("BaseDir [%s] donot exist, we create it failed!\n", baseDirpath.c_str());
        delete pFaceOper;
        pFaceOper = NULL;
        return -3;
    }
    FileMgrSingleton::getInstance()->setParams(captDirpath, baseDirpath, DEFAULT_DISPLAY_MAX_NUM);
    FileMgrSingleton::getInstance()->setFaceOperPtr(pFaceOper);
    FileMgrSingleton::getInstance()->start();

    ret = initBaseFiles(baseDirpath);
    if(ret != 0)
    {
        dbgError("initBaseFiles failed! ret=%d, baseDirpath=[%s]\n", ret, baseDirpath.c_str());
        FileMgrSingleton::getInstance()->stop();
        FileMgrSingleton::getInstance()->join();
        delete pFaceOper;
        pFaceOper = NULL;
        return -4;
    }
    dbgDebug("initBaseFiles succeed.\n");
    
    ret = initCaptFiles(captDirpath);
    if(ret != 0)
    {
        dbgError("initCaptFiles failed! ret=%d, captDirpath=[%s]\n", ret, captDirpath.c_str());
        FileMgrSingleton::getInstance()->stop();
        FileMgrSingleton::getInstance()->join();
        delete pFaceOper;
        pFaceOper = NULL;
        return -5;
    }
    dbgDebug("initCaptFiles succeed.\n");
    
    FaceServer * pFaceServer = new FaceServer(FACE_SERVER_HTTP_PORT);
    ret = pFaceServer->createServer();
    if(ret != 0)
    {
        dbgError("create server failed! ret=%d\n", ret);
        return -6;
    }
    dbgDebug("create server succeed.\n");

    pFaceServer->start();

    dbgDebug("start main loop now.\n");
    while(1)
    {
        sleep(10);
        break;
    }
    dbgError("Main process will exit now!\n");

    pFaceServer->stop();
    pFaceServer->join();
    pFaceServer->destroyServer();
    delete pFaceServer;
    pFaceServer = NULL;

    FileMgrSingleton::getInstance()->stop();
    FileMgrSingleton::getInstance()->join();
    
    delete pFaceOper;
    pFaceOper = NULL;

    return 0;
}

