#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

using namespace std;
#include <string>

#include "sqlite3.h"
#include "fileMgr.h"
#include "moLogger.h"

DbCtrlSqlite::DbCtrlSqlite() : DbCtrl(), mDbName(SQLITE_DBNAME), 
    mUserinfoTableName(USERINFO_TABLENAME),
    mFileinfoTableName(FILEINFO_TABLENAME)
{
    int ret = sqlite3_open(mDbName.c_str(), &mpDb);
    if(ret != 0)
    {
        printf("sqlite3_open failed! ret=%d, desc=[%s]\n",
            ret, sqlite3_errmsg(mpDb));
        mpDb = NULL;
        return ;
    }

    //create userinfo table if not exist
    string cmd("create table if not exists ");
    cmd += mUserinfoTableName;
    cmd += "(username char(32),password char(32),role int not null, lastLoginTime bigint, signUpTime bigint);";
    printf("create userinfo table cmd is [%s]\n", cmd.c_str());
    char * errmsg = NULL;
    ret = sqlite3_exec(mpDb, cmd.c_str(), NULL, NULL, &errmsg);
    if(ret != SQLITE_OK)
    {
        printf("Create userinfo table failed! ret=%d, errmsg=[%s], desc=[%s]\n", 
            ret, sqlite3_errmsg(mpDb), errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(mpDb);
        mpDb = NULL;
        return ;
    }
    printf("Create userinfo table succeed.\n");
    
    //create fileinfo table if not exist
    cmd = "create table if not exists ";
    cmd += mFileinfoTableName;
    cmd += "(isInited int, filetype int, filename char(256), filesize bigint, owner char(32), state int,";
    cmd += "readHdr int, readNum int, writeHdr int);";
    printf("create fileinfo table cmd is [%s]\n", cmd.c_str());
    ret = sqlite3_exec(mpDb, cmd.c_str(), NULL, NULL, &errmsg);
    if(ret != SQLITE_OK)
    {
        printf("Create userinfo table failed! ret=%d, errmsg=[%s], desc=[%s]\n", 
            ret, sqlite3_errmsg(mpDb), errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(mpDb);
        mpDb = NULL;
        return ;
    }
    printf("Create fileinfo table succeed.\n");

    sqlite3_free(errmsg);
}

DbCtrlSqlite::DbCtrlSqlite(const DbCtrlSqlite & other) : mDbName(other.mDbName),
    mUserinfoTableName(other.mUserinfoTableName),
    mFileinfoTableName(other.mFileinfoTableName)
{
    printf("wjl_test : 1111 \n");
    //TODO
}

DbCtrlSqlite::~DbCtrlSqlite()
{
    if(mpDb != NULL)
        sqlite3_close(mpDb);
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

int DbCtrlSqlite::getFilelist(const int filetype, list<DB_FILEINFO> & filelist)
{
    //TODO, 
    return 0;
}

int DbCtrlSqlite::getFilelist(const int filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)       
{
    //TODO, 
    return 0;
}

static int isExistCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    int * isExist = (int *)data;
    if(column_value[0])
    {
        *isExist = 1;
    }
    else
    {
        *isExist = 0;
    }
    return 0;
}


bool DbCtrlSqlite::isUserExist(const string & username)   
{
    //TODO, 
    return 0;
}

bool DbCtrlSqlite::isFileExist(const MOCLOUD_FILEINFO_KEY & key)
{
    char selectCmd[128] = {0x00};
    snprintf(selectCmd, 128, "select count(*) from %s where filetype=%d and filename=\"%s\";",
        mFileinfoTableName.c_str(), key.filetype, key.filename);
    selectCmd[127] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int isExist = 0;
    char *errmsg = NULL;
    int ret = sqlite3_exec(mpDb, selectCmd, isExistCallback, &isExist, &errmsg);
    if(ret != SQLITE_OK)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "sqlite3_exec failed! ret=%d, errmsg=[%s]\n", ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sqlite3_exec succeed, isExist=%d.\n", isExist);
    sqlite3_free(errmsg);
    errmsg = NULL;

    return isExist == 0 ? false : true;
}

/*
    1.set all file info in table to unInited;
    2.use type+filename, check fileinfo exist or not;
    3.if exist, set to inited; else, insert it;
    4.after filelistMap being looped, all fileinfo in table unInited should deleted;
*/
int DbCtrlSqlite::refreshFileinfo(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)
{
    string cmd = "update ";
    cmd += mFileinfoTableName;
    cmd += " set isInited = 0;";
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cmd=[%s]\n", cmd.c_str());
    char *errmsg = NULL;
    int ret = sqlite3_exec(mpDb, cmd.c_str(), NULL, NULL, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sqlite_exec succeed.\n");

    for(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> >::iterator it = filelistMap.begin(); it != filelistMap.end(); it++)
    {
        MOCLOUD_FILETYPE type = it->first;
        list<DB_FILEINFO> & curFileinfoList = it->second;
        for(list<DB_FILEINFO>::iterator iter = curFileinfoList.begin(); iter != curFileinfoList.end(); iter++)
        {
            if(!isFileExist(iter->basicInfo.key))
            {
                //insert it to database
                char insertCmd[512] = {0x00};
                int isInited = 1;
                char owner[MOCLOUD_USERNAME_MAXLEN] = {0x00};
                if(strlen(iter->basicInfo.ownerName) == 0)
                    strcpy(owner, "EricJinleiWu");
                else
                {
                    strncpy(owner, iter->basicInfo.ownerName, MOCLOUD_USERNAME_MAXLEN);
                    owner[MOCLOUD_USERNAME_MAXLEN - 1] = 0x00;
                }
                snprintf(insertCmd, 512, "insert into %s values(%d, %d, \"%s\", %ld, \"%s\", %d, %d, %d, %d);",
                    mFileinfoTableName.c_str(), isInited, iter->basicInfo.key.filetype,
                    iter->basicInfo.key.filename, iter->basicInfo.filesize, owner,
                    iter->basicInfo.state, 0, -1, 0);
                insertCmd[511] = 0x00;
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insertCmd=[%s]\n", insertCmd);
                ret = sqlite3_exec(mpDb, insertCmd, NULL, NULL, &errmsg);
                if(ret != SQLITE_OK)
                {
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                        "insert failed! ret=%d, errmsg=[%s], cmd=[%s]\n",
                        ret, errmsg, insertCmd);
                    sqlite3_free(errmsg);
                    errmsg = NULL;
                    return -2;
                }
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insert succeed.\n");
            }
            else
            {
                //update its info in database
                char updateCmd[512] = {0x00};
                snprintf(updateCmd, 512, "update %s set isInited = %d, filesize=%ld where filetype=%d and filename=\"%s\";", 
                    mFileinfoTableName.c_str(), 1, iter->basicInfo.filesize, 
                    iter->basicInfo.key.filetype, iter->basicInfo.key.filename);
                updateCmd[511] = 0x00;
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "updateCmd=[%s]\n", updateCmd);
                ret = sqlite3_exec(mpDb, updateCmd, NULL, NULL, &errmsg);
                if(ret != SQLITE_OK)
                {
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                        "update failed! ret=%d, errmsg=[%s], cmd=[%s]\n",
                        ret, errmsg, updateCmd);
                    sqlite3_free(errmsg);
                    errmsg = NULL;
                    return -3;
                }
            }
        }
    }

    //check all isInited==0 data, delete them
    char deleteCmd[128] = {0x00};
    snprintf(deleteCmd, 128, "delete from %s where isInited = %d;",
        mFileinfoTableName.c_str(), 0);    
    deleteCmd[127] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "deleteCmd=[%s]\n", deleteCmd);
    ret = sqlite3_exec(mpDb, deleteCmd, NULL, NULL, &errmsg);
    if(ret != SQLITE_OK)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "update failed! ret=%d, errmsg=[%s], cmd=[%s]\n",
            ret, errmsg, deleteCmd);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -4;
    }

    sqlite3_free(errmsg);
    return 0;
}

int DbCtrlSqlite::userLogin(const string & username, const string & passwd)
{
    //TODO
    return 0;
}

int DbCtrlSqlite::userLogout(const string & username)
{
    //TODO
    return 0;
}


