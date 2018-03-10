#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;
#include <string>

//#include "sqlite3.h"
#include "fileMgr.h"


DbCtrlMysql::DbCtrlMysql() : DbCtrl(), mDbName(SQLITE_DBNAME), 
    mUserinfoTableName(USERINFO_TABLENAME),
    mFileinfoTableName(FILEINFO_TABLENAME)
{
    //TODO, open sqlite db, create table if not exist
    ;
}

DbCtrlMysql::DbCtrlMysql(const DbCtrlMysql & other) : mDbName(other.mDbName),
    mUserinfoTableName(other.mUserinfoTableName),
    mFileinfoTableName(other.mFileinfoTableName)
{
    //TODO, open sqlite db, create table if not exist
    ;
}

DbCtrlMysql::~DbCtrlMysql()
{
    //TODO, close sqlite db
    ;
}

DbCtrlMysql & DbCtrlMysql::operator = (const DbCtrlMysql & other)
{
    if(this == &other)
        return *this;

    mDbName = other.mDbName;
    mUserinfoTableName = other.mUserinfoTableName;
    mFileinfoTableName = other.mFileinfoTableName;
    return *this;
}



int DbCtrlMysql::insertUserinfo(const DB_USERINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::deleteUserinfo(const string & username)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::getUserinfo(const string & username, DB_USERINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::modifyUserinfo(const string & username, DB_USERINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::insertFileinfo(const DB_FILEINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::deleteFileinfo(const MOCLOUD_FILEINFO_KEY & key)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::getFileinfo(DB_FILEINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::modifyFileinfo(const MOCLOUD_FILEINFO_KEY & key, DB_FILEINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::getFilelist(const int filetype, list<DB_FILEINFO> & filelist)
{
    //TODO, 
    return 0;
}

int DbCtrlMysql::getFilelist(const int filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)       
{
    //TODO, 
    return 0;
}

bool DbCtrlMysql::isUserExist(const string & username)   
{
    //TODO, 
    return 0;
}

bool DbCtrlMysql::isFileExist(const MOCLOUD_FILEINFO_KEY & key)
{
    //TODO, 
    return 0;
}



bool DbCtrlMysql::userLogin(const string & username, const string & passwd)
{
    //TODO
    return true;
}

bool DbCtrlMysql::userLogout(const string & username)
{
    //TODO
    return true;
}

