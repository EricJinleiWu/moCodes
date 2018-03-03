#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;
#include <string>

//#include "sqlite3.h"
#include "fileMgr.h"


DbCtrlSqlite::DbCtrlSqlite() : DbCtrl(), mDbName(SQLITE_DBNAME), 
    mUserinfoTableName(USERINFO_TABLENAME),
    mFileinfoTableName(FILEINFO_TABLENAME)
{
    //TODO, open sqlite db, create table if not exist
    ;
}

DbCtrlSqlite::DbCtrlSqlite(const DbCtrlSqlite & other) : mDbName(other.mDbName),
    mUserinfoTableName(other.mUserinfoTableName),
    mFileinfoTableName(other.mFileinfoTableName)
{
    //TODO, open sqlite db, create table if not exist
    ;
}

DbCtrlSqlite::~DbCtrlSqlite()
{
    //TODO, close sqlite db
    ;
}

DbCtrlSqlite & DbCtrlSqlite::operator = (const DbCtrlSqlite & other)
{
    if(this == &other)
        return *this;

    mDbName = other.mDbName;
    mUserinfoTableName = other.mUserinfoTableName;
    mFileinfoTableName = other.mFileinfoTableName;
    return *this;
}

int DbCtrlSqlite::insertUserinfo(const DB_USERINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::deleteUserinfo(const string & username)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::getUserinfo(const string & username, DB_USERINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::modifyUserinfo(const string & username, DB_USERINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::insertFileinfo(const DB_FILEINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::deleteFileinfo(const MOCLOUD_FILEINFO_KEY & key)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::getFileinfo(DB_FILEINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::modifyFileinfo(const MOCLOUD_FILEINFO_KEY & key, DB_FILEINFO & info)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::getFilelist(const MOCLOUD_FILETYPE & filetype, list<DB_FILEINFO> & filelist)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::getFilelist(const MOCLOUD_FILETYPE & filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)       
{
    //TODO, 
    return 0;
}

bool DbCtrlSqlite::isUserExist(const string & username)   
{
    //TODO, 
    return 0;
}

bool DbCtrlSqlite::isFileExist(const MOCLOUD_FILEINFO_KEY & key)
{
    //TODO, 
    return 0;
}

