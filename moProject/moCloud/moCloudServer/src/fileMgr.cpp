#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

using namespace std;
#include <string>
#include <list>

#include "fileMgr.h"

FileMgr::FileMgr() : mDirpath("")
{
    ;
}

FileMgr::FileMgr(const string & dirpath) : mDirpath(dirpath)
{
    ;
}

FileMgr::FileMgr(const FileMgr & other) : mDirpath(other.mDirpath)
{
    ;
}


FileMgr::~FileMgr()
{
    ;
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

int FileMgr::refreshFileinfoTable()
{
    //TODO
    return 0;
}

int FileMgr::readFile(const MOCLOUD_FILEINFO_KEY & key,const size_t & offset,const size_t length,char * pData)
{
    //TODO
    return 0;
}

int FileMgr::writeFile(const MOCLOUD_FILEINFO_KEY & key, const size_t & offset,
        const size_t length, char * pData)
{
    //TODO
    return 0;
}

int FileMgr::getFilelist(const MOCLOUD_FILETYPE & filetype, list<DB_FILEINFO> & filelist)
{
    //TODO
    return 0;
}

int FileMgr::getFilelist(const MOCLOUD_FILETYPE & filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)
{
    //TODO
    return 0;
}

int FileMgr::modifyDirpath(const string & newDirpath)
{
    //TODO,
    return 0;
}

FileMgr * FileMgrSingleton::pFileMgr = new FileMgr();

FileMgr * FileMgrSingleton::getInstance() 
{
    return pFileMgr;
}




