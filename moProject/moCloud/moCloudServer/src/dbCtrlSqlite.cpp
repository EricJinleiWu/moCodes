#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

using namespace std;
#include <string>

#include "sqlite3.h"
#include "fileMgr.h"
#include "moLogger.h"
#include "moCloudUtilsTypes.h"

#define USERINFO_TABLE_USERNAME "username"
#define USERINFO_TABLE_PASSWORD "password"
#define USERINFO_TABLE_ROLE "role"
#define USERINFO_TABLE_LASTLOGINTIME "lastLoginTime"
#define USERINFO_TABLE_SIGNUPTIME "signUpTime"

#define FILEINFO_TABLE_ISINITED "isInited"
#define FILEINFO_TABLE_FILETYPE "filetype"
#define FILEINFO_TABLE_FILENAME "filename"
#define FILEINFO_TABLE_FILESIZE "filesize"
#define FILEINFO_TABLE_OWNER "owner"
#define FILEINFO_TABLE_STATE "state"
#define FILEINFO_TABLE_READHDR "readHdr"
#define FILEINFO_TABLE_READNUM "readNum"
#define FILEINFO_TABLE_WRITEHDR "writeHdr"

#define SQL_CMD_MAXLEN  256

static int isExistCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    int * isExist = (int *)data;
    if(column_value[0])
    {
        int ret = atoi(column_value[0]);
        *isExist = (ret > 0) ? 1 : 0;
    }
    else
        *isExist = 0;
    return 0;
}

static int getPasswdCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    char * passwd = (char *)data;
    if(column_value[0])
    {
        strncpy(passwd, column_value[0], MOCLOUD_PASSWD_MAXLEN);
        passwd[MOCLOUD_PASSWD_MAXLEN - 1] = 0x00;
    }
    else
    {
        memset(passwd, 0x00, MOCLOUD_PASSWD_MAXLEN);
    }
    return 0;
}

static int getFileinfoCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    DB_FILEINFO * pDbFileinfo = (DB_FILEINFO *)data;
     
    int isInited = atoi(column_value[0]);
    pDbFileinfo->isInited = (isInited == 0) ? false : true;

    int filetype = atoi(column_value[1]);
    pDbFileinfo->basicInfo.key.filetype = MOCLOUD_FILETYPE(filetype);

    strcpy(pDbFileinfo->basicInfo.key.filename, column_value[2]);

    pDbFileinfo->basicInfo.filesize = atol(column_value[3]);

    strcpy(pDbFileinfo->basicInfo.ownerName, column_value[4]);

    int state = atoi(column_value[5]);
    pDbFileinfo->basicInfo.state= MOCLOUD_FILE_STATE(state);

    pDbFileinfo->readHdr = atoi(column_value[6]);
    pDbFileinfo->readerNum = atoi(column_value[7]);
    pDbFileinfo->writeHdr = atoi(column_value[8]);

    return 0;
}

static int getUserinfoCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    DB_USERINFO * pDbUserinfo = (DB_USERINFO *)data;

    pDbUserinfo->username = column_value[0];
    pDbUserinfo->password= column_value[1];
    pDbUserinfo->role = DB_USER_ROLE(atoi(column_value[2]));
    pDbUserinfo->lastLogInTime = atol(column_value[3]);
    pDbUserinfo->signUpTime = atol(column_value[4]);
    
    return 0;
}

static int getFilelistListCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "n_columns=%d\n", n_columns);
    
    list<DB_FILEINFO> * curList = (list<DB_FILEINFO> *)data;
    DB_FILEINFO curFileinfo;
    
    int isInited = atoi(column_value[0]);
    curFileinfo.isInited = (isInited == 0) ? false : true;

    int filetype = atoi(column_value[1]);
    curFileinfo.basicInfo.key.filetype = MOCLOUD_FILETYPE(filetype);

    strcpy(curFileinfo.basicInfo.key.filename, column_value[2]);

    curFileinfo.basicInfo.filesize = atol(column_value[3]);

    strcpy(curFileinfo.basicInfo.ownerName, column_value[4]);

    int state = atoi(column_value[5]);
    curFileinfo.basicInfo.state= MOCLOUD_FILE_STATE(state);

    curFileinfo.readHdr = atoi(column_value[6]);
    curFileinfo.readerNum = atoi(column_value[7]);
    curFileinfo.writeHdr = atoi(column_value[8]);

    curList->push_back(curFileinfo);

    return 0;
}

static int getFilelistMapCallback(void *data, int n_columns, char **column_value, char **column_names)
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "n_columns=%d\n", n_columns);
    
    map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > * curMap = (map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > *)data;
    
    DB_FILEINFO curFileinfo;
    
    int isInited = atoi(column_value[0]);
    curFileinfo.isInited = (isInited == 0) ? false : true;

    int filetype = atoi(column_value[1]);
    curFileinfo.basicInfo.key.filetype = MOCLOUD_FILETYPE(filetype);

    strcpy(curFileinfo.basicInfo.key.filename, column_value[2]);

    curFileinfo.basicInfo.filesize = atol(column_value[3]);

    strcpy(curFileinfo.basicInfo.ownerName, column_value[4]);

    int state = atoi(column_value[5]);
    curFileinfo.basicInfo.state= MOCLOUD_FILE_STATE(state);

    curFileinfo.readHdr = atoi(column_value[6]);
    curFileinfo.readerNum = atoi(column_value[7]);
    curFileinfo.writeHdr = atoi(column_value[8]);

    for(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> >::iterator it = curMap->begin(); it != curMap->end(); it++)
    {
        if(it->first == curFileinfo.basicInfo.key.filetype)
        {
            list<DB_FILEINFO> & curList = it->second;
            curList.push_back(curFileinfo);
            break;
        }
    }
    
    return 0;
}

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
    char createCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(createCmd, SQL_CMD_MAXLEN, 
        "create table if not exists %s(%s char(32),%s char(32), %s int not null, %s bigint, %s bigint);",
        mUserinfoTableName.c_str(), USERINFO_TABLE_USERNAME, USERINFO_TABLE_PASSWORD, 
        USERINFO_TABLE_ROLE, USERINFO_TABLE_LASTLOGINTIME, USERINFO_TABLE_SIGNUPTIME);
    createCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    printf("create userinfo table cmd is [%s]\n", createCmd);
    char * errmsg = NULL;
    ret = sqlite3_exec(mpDb, createCmd, NULL, NULL, &errmsg);
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
    memset(createCmd, 0x00, SQL_CMD_MAXLEN);
    snprintf(createCmd, SQL_CMD_MAXLEN, 
        "create table if not exists %s(%s int, %s int, %s char(256), %s bigint, %s char(32), %s int, %s int, %s int, %s int);",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_ISINITED, FILEINFO_TABLE_FILETYPE, FILEINFO_TABLE_FILENAME,
        FILEINFO_TABLE_FILESIZE, FILEINFO_TABLE_OWNER, FILEINFO_TABLE_STATE, FILEINFO_TABLE_READHDR,
        FILEINFO_TABLE_READNUM, FILEINFO_TABLE_WRITEHDR);
    createCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    printf("create userinfo table cmd is [%s]\n", createCmd);
    ret = sqlite3_exec(mpDb, createCmd, NULL, NULL, &errmsg);
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
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    //1.userinfo valid or not
    if(info.username.length() > MOCLOUD_USERNAME_MAXLEN || 
        info.password.length() > MOCLOUD_PASSWD_MAXLEN ||
        info.password.length() < MOCLOUD_PASSWD_MINLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username.length=%d, passwd.length=%d, some invalid value!\n",
            info.username.length(), info.password.length());
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], length=%d, passwd=[%s], length=%d\n",
        info.username.c_str(), info.username.length(), info.password.c_str(), 
        info.password.length());

    //2.user exist yet or not
    if(isUserExist(info.username))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Username=[%s], has exist yet, cannot insert again!\n",
            info.username.c_str());
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], should insert it now.\n",
        info.username.c_str());

    //3.insert it
    char insertCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(insertCmd, SQL_CMD_MAXLEN, "insert into %s values(\"%s\",\"%s\", %d, %ld, %ld);",
        mUserinfoTableName.c_str(), info.username.c_str(), info.password.c_str(), 
        info.role, info.lastLogInTime, info.signUpTime);
    insertCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insertCmd=[%s]\n", insertCmd);
    char * errmsg = NULL;
    int ret = sqlite3_exec(mpDb, insertCmd, NULL, NULL, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "insert userinfo to database failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insert userinfo to database succeed.\n");
    
    sqlite3_free(errmsg);
    errmsg = NULL;
    return 0;
}

int DbCtrlSqlite::deleteUserinfo(const string & username)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    //1.userinfo valid or not
    if(username.length() > MOCLOUD_USERNAME_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username.length=%d, invalid value!\n", username.length());
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], length=%d\n", username.c_str(), username.length());

    //2.user exist yet or not
    if(!isUserExist(username))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Username=[%s], donot exist! cannot delete it!\n",
            username.c_str());
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], should delete it now.\n",
        username.c_str());

    //3.delete from database
    char deleteCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(deleteCmd, SQL_CMD_MAXLEN, "delete from %s where %s=\"%s\";", 
        mUserinfoTableName.c_str(), USERINFO_TABLE_USERNAME, username.c_str());
    deleteCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "deleteCmd=[%s]\n", deleteCmd);
    char * errmsg = NULL;
    int ret = sqlite3_exec(mpDb, deleteCmd, NULL, NULL, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "delete userinfo from database failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "delete userinfo from database succeed.\n");
    
    sqlite3_free(errmsg);
    errmsg = NULL;
    return 0;
}

int DbCtrlSqlite::getUserinfo(const string & username, DB_USERINFO & info)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(!isUserExist(username))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username=[%s], donot exist in database, cannot get its info!\n",
            username.c_str());
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], get its info now.\n", username.c_str());

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s=\"%s\";",
        mUserinfoTableName.c_str(), USERINFO_TABLE_USERNAME, username.c_str());
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    char * errmsg = NULL;
    int ret = sqlite3_exec(mpDb, selectCmd, getUserinfoCallback, &info, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, cmd=[%s], errmsg=[%s]\n",
            ret, selectCmd, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get user info succeed.\n");
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], passwd=[%s], role=%d, lastLoginTime=%ld, signupTime=%ld\n",
        info.username.c_str(), info.password.c_str(), info.role, info.lastLogInTime, info.signUpTime);
    
    sqlite3_free(errmsg);
    errmsg = NULL;
    return 0;
}

int DbCtrlSqlite::modifyUserinfo(const string & username, DB_USERINFO & info)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    //TODO, modify which attribute

    return 0;
}

int DbCtrlSqlite::insertFileinfo(const DB_FILEINFO & info)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(isFileExist(info.basicInfo.key))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "File has been exist in database, cannot insert it again! filetype=%d, filename=[%s]\n",
            info.basicInfo.key.filetype, info.basicInfo.key.filename);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s]\n", 
        info.basicInfo.key.filetype, info.basicInfo.key.filename);

    char insertCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(insertCmd, SQL_CMD_MAXLEN, 
        "insert into %s values(%d, %d, \"%s\", %ld, \"%s\", %d, %d, %d, %d);",
        mFileinfoTableName.c_str(), 1, info.basicInfo.key.filetype,
        info.basicInfo.key.filename, info.basicInfo.filesize, 
        info.basicInfo.ownerName, info.basicInfo.state, 0, -1, 0);
    insertCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insertCmd=[%s]\n", insertCmd);
    char *errmsg = NULL;
    int ret = sqlite3_exec(mpDb, insertCmd, NULL, NULL, &errmsg);
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

    sqlite3_free(errmsg);
    errmsg = NULL;
    
    return 0;
}

int DbCtrlSqlite::deleteFileinfo(const MOCLOUD_FILEINFO_KEY & key)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(!isFileExist(key))
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
            "fileinfo donot exist in database, cannot delete it! filetype=%d, filename=[%s]\n",
            key.filetype, key.filename);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s]\n", key.filetype, key.filename);

    char deleteCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(deleteCmd, SQL_CMD_MAXLEN, "delete from %s where %s=%d and %s=\"%s\";",
        mFileinfoTableName.c_str(), 
        FILEINFO_TABLE_FILETYPE, key.filetype,
        FILEINFO_TABLE_FILENAME, key.filename);
    deleteCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "deleteCmd=[%s]\n", deleteCmd);
    char * errmsg = NULL;
    int ret = sqlite3_exec(mpDb, deleteCmd, NULL, NULL, &errmsg);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite_exec failed! ret=%d, cmd=[%s], errmsg=[%s]\n",
            ret, deleteCmd, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "sqlite3_exec succeed, cmd=[%s]\n", deleteCmd);

    sqlite3_free(errmsg);
    errmsg = NULL;

    return 0;
}

int DbCtrlSqlite::getFileinfo(DB_FILEINFO & info)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(!isFileExist(info.basicInfo.key))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "File donot exist, cannot get its info! filetype=%d, filename=[%s]\n",
            info.basicInfo.key.filetype, info.basicInfo.key.filename);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s]\n",
        info.basicInfo.key.filetype, info.basicInfo.key.filename);

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s=%d and %s=\"%s\";",
        mFileinfoTableName.c_str(),
        FILEINFO_TABLE_FILETYPE, info.basicInfo.key.filetype,
        FILEINFO_TABLE_FILENAME, info.basicInfo.key.filename);
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    char * errmsg = NULL;
    int ret = sqlite3_exec(mpDb, selectCmd, getFileinfoCallback, &info, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, cmd=[%s], errmsg=[%s]\n",
            ret, selectCmd, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get file info succeed.\n");
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s], " \
        "filesize=%ld, ownerName=[%s], state=%d, readerNum=%d, readHdr=%d, writeHdr=%d\n",
        info.basicInfo.key.filetype, info.basicInfo.key.filename,
        info.basicInfo.filesize, info.basicInfo.ownerName, info.basicInfo.state,
        info.readerNum, info.readHdr, info.writeHdr);
    
    sqlite3_free(errmsg);
    errmsg = NULL;
    return 0;
}

int DbCtrlSqlite::modifyFileinfo(const MOCLOUD_FILEINFO_KEY & key, DB_FILEINFO & info)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    //TODO, modify which attribute? should make sure this

    return 0;
}

int DbCtrlSqlite::getFilelist(const int filetype, list<DB_FILEINFO> & filelist)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(filetype & MOCLOUD_FILETYPE_VIDEO == 0 && 
        filetype & MOCLOUD_FILETYPE_AUDIO == 0 && 
        filetype & MOCLOUD_FILETYPE_PIC == 0 && 
        filetype & MOCLOUD_FILETYPE_OTHERS == 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "filetype=%d, invalid value!\n", filetype);
        return MOCLOUD_GETFILELIST_ERR_TYPE_INVALID;
    }

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s in (%d, %d, %d, %d);",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_FILETYPE,
        filetype & MOCLOUD_FILETYPE_VIDEO, filetype & MOCLOUD_FILETYPE_AUDIO, 
        filetype & MOCLOUD_FILETYPE_PIC, filetype & MOCLOUD_FILETYPE_OTHERS);
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    char *errmsg = NULL;
    int ret = sqlite3_exec(mpDb, selectCmd, getFilelistListCallback, &filelist, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return MOCLOUD_GETFILELIST_ERR_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select filelist succeed.\n");

#if 0
    //just for debug
    printf("wjl_test : filelist.size=%d\n", filelist.size());
    for(list<DB_FILEINFO>::iterator iter = filelist.begin(); iter != filelist.end(); iter++)
    {
        printf("wjl_test : filetype=%d, filename=[%s], filesize=%ld, owner=[%s]\n",
            iter->basicInfo.key.filetype, iter->basicInfo.key.filename, iter->basicInfo.filesize,
            iter->basicInfo.ownerName);
    }
#endif
    sqlite3_free(errmsg);
    errmsg = NULL;    
    return MOCLOUD_GETFILELIST_ERR_OK;
}

int DbCtrlSqlite::getFilelist(const int filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)       
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(filetype & MOCLOUD_FILETYPE_VIDEO == 0 && 
        filetype & MOCLOUD_FILETYPE_AUDIO == 0 && 
        filetype & MOCLOUD_FILETYPE_PIC == 0 && 
        filetype & MOCLOUD_FILETYPE_OTHERS == 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "filetype=%d, invalid value!\n", filetype);
        return MOCLOUD_GETFILELIST_ERR_TYPE_INVALID;
    }

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s in (%d, %d, %d, %d);",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_FILETYPE, 
        filetype & MOCLOUD_FILETYPE_VIDEO, filetype & MOCLOUD_FILETYPE_AUDIO, 
        filetype & MOCLOUD_FILETYPE_PIC, filetype & MOCLOUD_FILETYPE_OTHERS);
    selectCmd[SQL_CMD_MAXLEN] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    char *errmsg = NULL;
    int ret = sqlite3_exec(mpDb, selectCmd, getFilelistMapCallback, &filelistMap, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return MOCLOUD_GETFILELIST_ERR_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select filelist succeed.\n");

    sqlite3_free(errmsg);
    errmsg = NULL;    
#if 0
    //just for debug
    for(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> >::iterator it = filelistMap.begin(); it != filelistMap.end(); it++)
    {
        list<DB_FILEINFO> & curList = it->second;
        for(list<DB_FILEINFO>::iterator iter = curList.begin(); iter != curList.end(); iter++)
        {
            printf("wjl_test : it->first=%d, filetype=%d, filename=[%s], filesize=%ld, owner=[%s]\n",
                it->first, iter->basicInfo.key.filetype, iter->basicInfo.key.filename, iter->basicInfo.filesize,
                iter->basicInfo.ownerName);
        }
    }
#endif
    return MOCLOUD_GETFILELIST_ERR_OK;
}

bool DbCtrlSqlite::isUserExist(const string & username)   
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select count(*) from %s where %s=\"%s\";",
        mUserinfoTableName.c_str(), USERINFO_TABLE_USERNAME, username.c_str());
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
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

bool DbCtrlSqlite::isFileExist(const MOCLOUD_FILEINFO_KEY & key)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select count(*) from %s where %s=%d and %s=\"%s\";",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_FILETYPE, key.filetype, 
        FILEINFO_TABLE_FILENAME, key.filename);
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
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
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

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
                char insertCmd[SQL_CMD_MAXLEN] = {0x00};
                int isInited = 1;
                char owner[MOCLOUD_USERNAME_MAXLEN] = {0x00};
                if(strlen(iter->basicInfo.ownerName) == 0)
                    strcpy(owner, "EricJinleiWu");
                else
                {
                    strncpy(owner, iter->basicInfo.ownerName, MOCLOUD_USERNAME_MAXLEN);
                    owner[MOCLOUD_USERNAME_MAXLEN - 1] = 0x00;
                }
                snprintf(insertCmd, SQL_CMD_MAXLEN, "insert into %s values(%d, %d, \"%s\", %ld, \"%s\", %d, %d, %d, %d);",
                    mFileinfoTableName.c_str(), isInited, iter->basicInfo.key.filetype,
                    iter->basicInfo.key.filename, iter->basicInfo.filesize, owner,
                    iter->basicInfo.state, 0, -1, 0);
                insertCmd[SQL_CMD_MAXLEN - 1] = 0x00;
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
                char updateCmd[SQL_CMD_MAXLEN] = {0x00};
                snprintf(updateCmd, SQL_CMD_MAXLEN, "update %s set %s=%d, %s=%ld where %s=%d and %s=\"%s\";", 
                    mFileinfoTableName.c_str(), FILEINFO_TABLE_ISINITED, 1, 
                    FILEINFO_TABLE_FILESIZE, iter->basicInfo.filesize, 
                    FILEINFO_TABLE_FILETYPE, iter->basicInfo.key.filetype, 
                    FILEINFO_TABLE_FILENAME, iter->basicInfo.key.filename);
                updateCmd[SQL_CMD_MAXLEN - 1] = 0x00;
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
    char deleteCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(deleteCmd, SQL_CMD_MAXLEN, "delete from %s where %s = %d;",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_ISINITED, 0);    
    deleteCmd[SQL_CMD_MAXLEN - 1] = 0x00;
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
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(username.length() > MOCLOUD_USERNAME_MAXLEN || passwd.length() > MOCLOUD_PASSWD_MAXLEN ||
        passwd.length() < MOCLOUD_PASSWD_MINLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username.length=%d, passwd.length=%d, have invalid one!\n",
            username.length(), passwd.length());
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "username=[%s], passwd=[%s]\n",
        username.c_str(), passwd.c_str());

    if(!isUserExist(username))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username=[%s], donot exist in database!\n", username.c_str());
        return MOCLOUD_LOGIN_RET_USERNAME_DONOT_EXIST;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "username=[%s], exist.\n", username.c_str());

    //get password from database
    char selectPasswdCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectPasswdCmd, SQL_CMD_MAXLEN, "select %s from %s where %s=\"%s\";",
        USERINFO_TABLE_PASSWORD, mUserinfoTableName.c_str(), 
        USERINFO_TABLE_USERNAME, username.c_str());
    selectPasswdCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectPasswdCmd=[%s]\n", selectPasswdCmd);
    char passwdDb[MOCLOUD_PASSWD_MAXLEN] = {0x00};
    char * errmsg = NULL;
    int ret = sqlite3_exec(mpDb, selectPasswdCmd, getPasswdCallback, passwdDb, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "passwdDb=[%s]\n", passwdDb);

    //check passwdDb and @passwd equal or not
    if(passwd != passwdDb)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "input passwd=[%s], passwdInDb=[%s], donot equal! login failed!\n",
            passwd.c_str(), passwdDb);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return MOCLOUD_LOGIN_RET_PASSWD_WRONG;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "passwd check ok.\n");

    //update lastloginTime in database
    char updateCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(updateCmd, SQL_CMD_MAXLEN, "update %s=%ld from %s where %s=\"\%s\";",
        USERINFO_TABLE_LASTLOGINTIME, time(NULL), mUserinfoTableName.c_str(), 
        USERINFO_TABLE_USERNAME, username.c_str());
    updateCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "updateCmd=[%s]\n", updateCmd);
    ret = sqlite3_exec(mpDb, updateCmd, NULL, NULL, &errmsg);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errmsg=[%s]\n",
            ret, errmsg);
        sqlite3_free(errmsg);
        errmsg = NULL;
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Update last login time succeed.\n");
    
    sqlite3_free(errmsg);
    errmsg = NULL;
    return MOCLOUD_LOGIN_RET_OK;
}

int DbCtrlSqlite::userLogout(const string & username)
{
    if(mpDb == NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "mpDb==NULL, init database failed! cannot do anything!\n");
        return -1;
    }

    if(username.length() > MOCLOUD_USERNAME_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username.length=%d, invalid!\n", username.length());
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "username=[%s]\n", username.c_str());

    if(!isUserExist(username))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username=[%s], donot exist in database!\n", username.c_str());
        return MOCLOUD_LOGIN_RET_USERNAME_DONOT_EXIST;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "username=[%s], exist.\n", username.c_str());

    return 0;
}


