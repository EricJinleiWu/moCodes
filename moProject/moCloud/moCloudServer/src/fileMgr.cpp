#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;
#include <string>
#include <list>
#include <new>

#include "fileMgr.h"

#define SUBDIR_VIDEO    "/Video"
#define SUBDIR_AUDIO    "/Audio"
#define SUBDIR_PIC    "/Picture"
#define SUBDIR_OTHER    "/Other"

static USED_DB_TYPE gDbType = USED_DB_TYPE_MYSQL;

FileMgr::FileMgr() : mDirpath("")
{
    if(gDbType == USED_DB_TYPE_SQLITE)
    {
        mpDbCtrl = new (nothrow) DbCtrlSqlite;
        //DbCtrlSqlite *p = dynamic_cast<DbCtrlSqlite *>(mpDbCtrl);
    }
    else if(gDbType == USED_DB_TYPE_MYSQL)
    {
        mpDbCtrl = new DbCtrlMysql;
    }
    else
    {
        mpDbCtrl = NULL;
    }
}

FileMgr::FileMgr(const string & dirpath) : mDirpath(dirpath)
{
    if(gDbType == USED_DB_TYPE_SQLITE)
    {
        mpDbCtrl = new (nothrow) DbCtrlSqlite;
        //DbCtrlSqlite *p = dynamic_cast<DbCtrlSqlite *>(mpDbCtrl);
    }
    else if(gDbType == USED_DB_TYPE_MYSQL)
    {
        mpDbCtrl = new DbCtrlMysql;
    }
    else
    {
        mpDbCtrl = NULL;
    }
}

FileMgr::FileMgr(const FileMgr & other) : mDirpath(other.mDirpath)
{
    //TODO
    ;
}


FileMgr::~FileMgr()
{
    if(mpDbCtrl != NULL)
    {
        delete mpDbCtrl;
    }
}

FileMgr & FileMgr::operator=(const FileMgr & other)
{
    if(this == &other)
    {
        return *this;
    }

    mDirpath = other.mDirpath;
    return *this;
}

int FileMgr::getSubDirpath(const MOCLOUD_FILETYPE type, string & subDirpath)
{
    int ret = 0;
    switch(type)
    {
        case MOCLOUD_FILETYPE_VIDEO:
            subDirpath = mDirpath + SUBDIR_VIDEO;
            ret = 0;
            break;
        case MOCLOUD_FILETYPE_AUDIO:
            subDirpath = mDirpath + SUBDIR_AUDIO;
            ret = 0;
            break;
        case MOCLOUD_FILETYPE_PIC:
            subDirpath = mDirpath + SUBDIR_PIC;
            ret = 0;
            break;
        case MOCLOUD_FILETYPE_OTHERS:
            subDirpath = mDirpath + SUBDIR_OTHER;
            ret = 0;
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}

/*
    1.get all file info from local dir;
    2.set it to @filelistMap;
*/
int FileMgr::getLocalFilelistMap(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)
{
    filelistMap.clear();

    int type = MOCLOUD_FILETYPE_VIDEO;
    for(; type < MOCLOUD_FILETYPE_ALL; type *= 2)
    {
        string subDirPath;
        int ret = getSubDirpath(MOCLOUD_FILETYPE(type), subDirPath);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "getSubDirpath failed! ret=%d, type=%d, dirpath=[%s]\n",
                ret, type, mDirpath.c_str());
            filelistMap.clear();
            return -1;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "getSubDirPath succeed, type=%d, subDirpath=[%s]\n",
            type, subDirPath.c_str());

        //if sub directory donot exist, make it.
        ret = access(subDirPath.c_str(), 0);
        if(ret != 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "subDirPath=[%s], donot exist! should make it now.\n", subDirPath.c_str());
            ret = mkdir(subDirPath.c_str(), 0777);
            if(ret != 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "mkdir to subDirPath=[%s] failed! ret=%d, errno=%d, desc=[%s]\n", 
                    subDirPath.c_str(), ret, errno, strerror(errno));
                filelistMap.clear();
                return -2;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                "subDirpath=[%s], donot exist, make it succeed.\n", subDirPath.c_str());
            list<DB_FILEINFO> dbFileinfoList;
            dbFileinfoList.clear();
            filelistMap[MOCLOUD_FILETYPE(type)] = dbFileinfoList;
            continue;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "subDirPath=[%s], exist now.\n", subDirPath.c_str());

        list<DB_FILEINFO> dbFileinfoList;
        dbFileinfoList.clear();

        //get all files in this sub dir, set to local memory
        DIR * pDir = opendir(subDirPath.c_str());
        if(NULL == pDir)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "opendir failed! dirpath=[%s], errno=%d, desc=[%s]\n",
                subDirPath.c_str(), errno, strerror(errno));
            return -1;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "opendir [%s] succeed.\n", subDirPath.c_str());

        struct dirent * pCurFile = NULL;
        while((pCurFile = readdir(pDir)) != NULL)
        {
            if(0 == strcmp(pCurFile->d_name, ".") || 0 == strcmp(pCurFile->d_name, ".."))
            {
                continue;
            }
            
            if(isRegFile(subDirPath.c_str(), pCurFile->d_name))
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                    "A file find in dir[%s], its name is [%s].\n",
                    subDirPath.c_str(), pCurFile->d_name);

                MOCLOUD_BASIC_FILEINFO fileinfo;
                ret = getBasicFileinfo(subDirPath, pCurFile->d_name, MOCLOUD_FILETYPE(type), &fileinfo);
                if(ret < 0)
                {
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                        "getBasicFileinfo failed! subDirpath=[%s], filename=[%s], ret=%d, will ignore this file.\n",
                        subDirPath.c_str(), pCurFile->d_name, ret);
                    continue;
                }
                else
                {
                    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getBasicFileinfo succeed.\n");
                    
                    DB_FILEINFO dbFileInfo;
                    memset(&dbFileInfo, 0x00, sizeof(DB_FILEINFO));
                    memcpy(&dbFileInfo.basicInfo, &fileinfo, sizeof(MOCLOUD_BASIC_FILEINFO));

                    dbFileinfoList.push_back(dbFileInfo);
                }
            }
        }

        filelistMap[MOCLOUD_FILETYPE(type)] = dbFileinfoList;

        closedir(pDir);   
    }

    return 0;
}

int FileMgr::getBasicFileinfo(string & subDirpath, const char * pName, const MOCLOUD_FILETYPE type, 
    MOCLOUD_BASIC_FILEINFO * pFileinfo)
{
    if(NULL == pName || NULL == pFileinfo)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    
    string subFilepath = subDirpath + "/";
    subFilepath += pName;
    pFileinfo->filesize = moUtils_File_getSize(subFilepath.c_str());
    if(pFileinfo->filesize < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moUtils_File_getSize failed! ret=%d, filepath=[%s]\n",
            pFileinfo->filesize, subFilepath.c_str());
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moUtils_File_getSize succeed, size=%d\n", pFileinfo->filesize);

    memset(pFileinfo, 0x00, sizeof(MOCLOUD_BASIC_FILEINFO));
    strncpy(pFileinfo->key.filename, pName, MOCLOUD_FILENAME_MAXLEN);
    pFileinfo->key.filename[MOCLOUD_FILENAME_MAXLEN - 1] = 0x00;
    pFileinfo->key.filetype = type;

    //Set other value to default
    memset(pFileinfo->ownerName, 0x00, MOCLOUD_USERNAME_MAXLEN);
    pFileinfo->state = MOCLOUD_FILE_STATE_NORMAL;

    return 0;
}

/*
    Check input path being a regular file or not;
    Only be a regular file, return 1;
    not a regular, or other errors, return 0;
*/
bool FileMgr::isRegFile(const char *pDirpath, const char *pSubFileName)
{
    if(NULL == pDirpath || NULL == pSubFileName)
        return false;
    
    char subfilepath[MOCLOUD_FILEPATH_MAXLEN] = {0x00};
    memset(subfilepath, 0x00, MOCLOUD_FILEPATH_MAXLEN);
    snprintf(subfilepath, MOCLOUD_FILEPATH_MAXLEN, "%s/%s", pDirpath, pSubFileName);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "dirpath is [%s], subdirname is [%s], subdirpath is [%s]\n", 
        pDirpath, pSubFileName, subfilepath);

    struct stat curStat;
    int ret = stat(subfilepath, &curStat);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "stat failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return 0;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "stat ok.\n");

    return S_ISREG(curStat.st_mode) ? true : false;
}


void FileMgr::dumpFilelistMap(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)
{
#if 1
    printf("dump filelist map now.\n");
    for(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> >::iterator it = filelistMap.begin(); 
        it != filelistMap.end(); it++)
    {
        list<DB_FILEINFO> & curList = it->second;
        for(list<DB_FILEINFO>::iterator iter = curList.begin(); iter != curList.end(); iter++)
        {
            printf("\tfiletype=%d, filename=[%s], filesize=%d\n",
                iter->basicInfo.key.filetype, iter->basicInfo.key.filename, iter->basicInfo.filesize);
        }
    }
    printf("dump filelist map over.\n");
#endif
}

int FileMgr::refreshFileinfoTable()
{
    if(NULL == mpDbCtrl)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDbCtrl is NULL!\n");
        return -1;
    }

    map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > filelistMap;
    int ret = getLocalFilelistMap(filelistMap);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getLocalFilelistMap failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getLocalFilelistMap succeed.\n");
    dumpFilelistMap(filelistMap);

    if(gDbType == USED_DB_TYPE_SQLITE)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Use sqlite db.\n");
        DbCtrlSqlite *p = dynamic_cast<DbCtrlSqlite *>(mpDbCtrl);
        ret = p->refreshFileinfo(filelistMap);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "refreshFileinfo failed, ret=%d.\n", ret);
            ret = -3;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refreshFileinfo succeed.\n");
        }
    }
    else if(gDbType == USED_DB_TYPE_MYSQL)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Use mysql db.\n");
        DbCtrlMysql *p = dynamic_cast<DbCtrlMysql *>(mpDbCtrl);
        ret = p->refreshFileinfo(filelistMap);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "refreshFileinfo failed, ret=%d.\n", ret);
            ret = -4;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refreshFileinfo succeed.\n");
        }
    }
    else
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "gDbType=%d, invalid!\n", gDbType);
        ret = -5;
    }

    return ret;
}

bool FileMgr::isFileExist(const MOCLOUD_FILEINFO_KEY & key)
{
    bool ret = false;
    if(gDbType == USED_DB_TYPE_MYSQL)
    {
        DbCtrlMysql * p = dynamic_cast<DbCtrlMysql *>(mpDbCtrl);
        ret = p->isFileExist(key);
    }
    else if(gDbType == USED_DB_TYPE_SQLITE)
    {
        DbCtrlSqlite * p = dynamic_cast<DbCtrlSqlite *>(mpDbCtrl);
        ret = p->isFileExist(key);
    }
    else
    {
        ret = false;
    }

    return ret;
}

/*
    If file being open yet, just set @fd, and return OK;
    if donot open, open it!
*/
int FileMgr::openFile(const MOCLOUD_FILEINFO_KEY & key, int & fd)
{
    //Check file exist or not
    if(!isFileExist(key))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "filetype=%d, filename=[%s], donot exist!\n",
            key.filetype, key.filename);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Start open file now.\n");

    fd = -1;
    //should open file, and refresh info to database
    string filepath;
    int ret = getAbsFilepath(key, filepath);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "getAbsFilepath failed! ret=%d, filetype=%d, filename=[%s]\n",
            ret, key.filetype, key.filename);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "absFilepath=[%s]\n", filepath.c_str());
    
    fd = open(filepath.c_str(), O_RDONLY);
    if(fd < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "open file failed! file=[%s], errno=%d, desc=[%s]\n",
            filepath.c_str(), errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "File [%s] open succeed.\n", filepath.c_str());

    return 0;
}

/*
    After read, should close it.
*/
int FileMgr::closeFile(int & fd)
{
    if(fd < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "fd=%d, invalid!\n", fd);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "fd = %d\n", fd);

    close(fd);
    fd = -1;

    return 0;
}

int FileMgr::getAbsFilepath(const MOCLOUD_FILEINFO_KEY & key, string & absPath)
{
    int ret = 0;

    if(mDirpath[mDirpath.length() - 1] == '/')
        absPath = mDirpath;
    else
        absPath = mDirpath + "/";
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "1, absPath=[%s]\n", absPath.c_str());

    switch(key.filetype)
    {
        case MOCLOUD_FILETYPE_VIDEO:
            absPath += SUBDIR_VIDEO;
            break;
        case MOCLOUD_FILETYPE_AUDIO:
            absPath += SUBDIR_AUDIO;
            break;
        case MOCLOUD_FILETYPE_PIC:
            absPath += SUBDIR_PIC;
            break;
        case MOCLOUD_FILETYPE_OTHERS:
            absPath += SUBDIR_OTHER;
            break;
        default:
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, invalid!\n", key.filetype);
            ret = -1;
            break;
    }
    if(ret < 0)
    {
        return -1;
    }

    absPath += "/";
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "2, absPath=[%s]\n", absPath.c_str());

    absPath += key.filename;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "3, absPath=[%s]\n", absPath.c_str());

    return 0;
}


int FileMgr::writeFile(const MOCLOUD_FILEINFO_KEY & key, const size_t & offset,
        const size_t length, char * pData)
{
    if(NULL == pData)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "filetype=%d, filename=[%s], offset=%d, length=%d\n",
        key.filetype, key.filename, offset, length);
    
    //TODO

    return 0;
}

int FileMgr::getFilelist(const int filetype, list<DB_FILEINFO> & filelist)
{
    if(NULL == mpDbCtrl)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDbCtrl is NULL!\n");
        return -1;
    }

    int ret = 0;
    if(gDbType == USED_DB_TYPE_SQLITE)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Use sqlite db.\n");
        DbCtrlSqlite *p = dynamic_cast<DbCtrlSqlite *>(mpDbCtrl);
        ret = p->getFilelist(filetype, filelist);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist failed, ret=%d.\n", ret);
            ret = -2;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist succeed.\n");
        }
    }
    else if(gDbType == USED_DB_TYPE_MYSQL)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Use mysql db.\n");
        DbCtrlMysql *p = dynamic_cast<DbCtrlMysql *>(mpDbCtrl);
        ret = p->getFilelist(filetype, filelist);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist failed, ret=%d.\n", ret);
            ret = -3;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist succeed.\n");
        }
    }
    else
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "gDbType=%d, invalid!\n", gDbType);
        ret = -4;
    }

    return ret;
}

int FileMgr::getFilelist(const int filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)
{
    if(NULL == mpDbCtrl)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDbCtrl is NULL!\n");
        return -1;
    }

    int ret = 0;

    if(gDbType == USED_DB_TYPE_SQLITE)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Use sqlite db.\n");
        DbCtrlSqlite *p = dynamic_cast<DbCtrlSqlite *>(mpDbCtrl);
        ret = p->getFilelist(filetype, filelistMap);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist failed, ret=%d.\n", ret);
            ret = -2;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist succeed.\n");
        }
    }
    else if(gDbType == USED_DB_TYPE_MYSQL)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Use mysql db.\n");
        DbCtrlMysql *p = dynamic_cast<DbCtrlMysql *>(mpDbCtrl);
        ret = p->getFilelist(filetype, filelistMap);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist failed, ret=%d.\n", ret);
            ret = -3;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "getFilelist succeed.\n");
        }
    }
    else
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "gDbType=%d, invalid!\n", gDbType);
        ret = -4;
    }

    return ret;

}

int FileMgr::modifyDirpath(const string & newDirpath)
{
    mDirpath = newDirpath;
    return 0;
}

DbCtrl * FileMgr::getDbCtrlHdl()
{
    return mpDbCtrl;
}

FileMgr * FileMgrSingleton::pFileMgr = new FileMgr();

FileMgr * FileMgrSingleton::getInstance() 
{
    return pFileMgr;
}




