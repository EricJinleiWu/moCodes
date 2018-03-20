#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

using namespace std;
#include <string>

#include "mysql.h"
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

DbCtrlMysql::DbCtrlMysql() : DbCtrl(), mDbName(MYSQL_DBNAME), 
    mUserinfoTableName(USERINFO_TABLENAME),
    mFileinfoTableName(FILEINFO_TABLENAME)
{
    isDbOk = false;
    
    mysql_library_init(0, NULL, NULL);
    mysql_init(&mDb);
    
    if(mysql_real_connect(&mDb, "localhost", "root", "", MYSQL_DBNAME, 0, NULL, CLIENT_FOUND_ROWS))
    {
        printf("mysql_real_connect failed! errno=%d, errmsg=[%s]\n",
            mysql_errno(&mDb), mysql_error(&mDb));
        mysql_close(&mDb);
        mysql_library_end();
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
    int ret = mysql_query(&mDb, createCmd);
    if(ret != 0)
    {
        printf("mysql_real_connect failed! ret=%d, errno=%d, errmsg=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        mysql_close(&mDb);
        mysql_library_end();
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
    ret = mysql_query(&mDb, createCmd);
    if(ret != 0)
    {
        printf("mysql_real_connect failed! ret=%d, errno=%d, errmsg=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        mysql_close(&mDb);
        mysql_library_end();
        return ;
    }
    printf("Create fileinfo table succeed.\n");

    isDbOk = true;
}

DbCtrlMysql::DbCtrlMysql(const DbCtrlMysql & other) : mDbName(other.mDbName),
    mUserinfoTableName(other.mUserinfoTableName),
    mFileinfoTableName(other.mFileinfoTableName)
{
    //TODO
}

DbCtrlMysql::~DbCtrlMysql()
{
    if(isDbOk)
    {
        mysql_close(&mDb);
        mysql_library_end();
    }
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
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
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
        return -2;
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
        return -3;
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
    int ret = mysql_query(&mDb, insertCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "insert userinfo to database failed! ret=%d, errno=%d, errmsg=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insert userinfo to database succeed.\n");
    
    return 0;
}

int DbCtrlMysql::deleteUserinfo(const string & username)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    //1.userinfo valid or not
    if(username.length() > MOCLOUD_USERNAME_MAXLEN)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username.length=%d, invalid value!\n", username.length());
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], length=%d\n", username.c_str(), username.length());

    //2.user exist yet or not
    if(!isUserExist(username))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Username=[%s], donot exist! cannot delete it!\n",
            username.c_str());
        return -3;
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
    int ret = mysql_query(&mDb, deleteCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "delete userinfo from database failed! ret=%d, errno=%d, errmsg=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "delete userinfo from database succeed.\n");
    
    return 0;
}

int DbCtrlMysql::getUserinfo(const string & username, DB_USERINFO & info)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s=\"%s\";",
        mUserinfoTableName.c_str(), USERINFO_TABLE_USERNAME, username.c_str());
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int ret = mysql_query(&mDb, selectCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, selectCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get user info succeed.\n");

    MYSQL_RES * pRes = mysql_store_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "username=[%s], donot exist in table %s, cannot get its info!\n",
            username.c_str(), mUserinfoTableName.c_str());
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get results succeed.\n");

    int rowNum = mysql_num_rows(pRes);
    if(rowNum != 1)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "rowNum=%d, means donot just one username(%s) in this database table(%s)!\n",
            rowNum, username.c_str(), mUserinfoTableName.c_str());
        mysql_free_result(pRes);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "rowNum==1.\n");

    MYSQL_ROW row = mysql_fetch_row(pRes);
    info.username = row[0];
    info.password = row[1];
    info.role = DB_USER_ROLE(atoi(row[2]));
    info.lastLogInTime = atol(row[3]);
    info.signUpTime = atol(row[4]);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "username=[%s], passwd=[%s], role=%d, lastLoginTime=%ld, signupTime=%ld\n",
        info.username.c_str(), info.password.c_str(), info.role, info.lastLogInTime, info.signUpTime);

    mysql_free_result(pRes);
    
    return 0;
}

int DbCtrlMysql::modifyUserinfo(const string & username, DB_USERINFO & info)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    //TODO, modify which attribute

    return 0;
}

int DbCtrlMysql::insertFileinfo(const DB_FILEINFO & info)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    if(isFileExist(info.basicInfo.key))
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "File has been exist in database, cannot insert it again! filetype=%d, filename=[%s]\n",
            info.basicInfo.key.filetype, info.basicInfo.key.filename);
        return -2;
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
    int ret = mysql_query(&mDb, insertCmd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "insert failed! ret=%d, errno=%d, errmsg=[%s], cmd=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb), insertCmd);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "insert succeed.\n");

    return 0;
}

int DbCtrlMysql::deleteFileinfo(const MOCLOUD_FILEINFO_KEY & key)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    if(!isFileExist(key))
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
            "fileinfo donot exist in database, cannot delete it! filetype=%d, filename=[%s]\n",
            key.filetype, key.filename);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s]\n", key.filetype, key.filename);

    char deleteCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(deleteCmd, SQL_CMD_MAXLEN, "delete from %s where %s=%d and %s=\"%s\";",
        mFileinfoTableName.c_str(), 
        FILEINFO_TABLE_FILETYPE, key.filetype,
        FILEINFO_TABLE_FILENAME, key.filename);
    deleteCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "deleteCmd=[%s]\n", deleteCmd);
    int ret = mysql_query(&mDb, deleteCmd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "delete failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, deleteCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "delete succeed, cmd=[%s]\n", deleteCmd);

    return 0;
}

int DbCtrlMysql::getFileinfo(DB_FILEINFO & info)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s=%d and %s=\"%s\";",
        mFileinfoTableName.c_str(),
        FILEINFO_TABLE_FILETYPE, info.basicInfo.key.filetype,
        FILEINFO_TABLE_FILENAME, info.basicInfo.key.filename);
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int ret = mysql_query(&mDb, selectCmd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "select failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, selectCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get file info succeed.\n");

    MYSQL_RES * pRes = mysql_store_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Filetype=%d, filename=[%s], donot find it in database.\n",
            info.basicInfo.key.filetype, info.basicInfo.key.filename);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Get result succeed.\n");

    int rowNum = mysql_num_rows(pRes);
    if(rowNum != 1)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "rowNum=%d!\n", rowNum);
        mysql_free_result(pRes);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "rowNum==1\n");

    MYSQL_ROW row = mysql_fetch_row(pRes);
    info.basicInfo.key.filetype = MOCLOUD_FILETYPE(atoi(row[1]));
    strncpy(info.basicInfo.key.filename, row[2], MOCLOUD_FILENAME_MAXLEN);
    info.basicInfo.key.filename[MOCLOUD_FILENAME_MAXLEN - 1] = 0x00;
    info.basicInfo.filesize = atol(row[3]);
    strncpy(info.basicInfo.ownerName, row[4], MOCLOUD_USERNAME_MAXLEN);
    info.basicInfo.ownerName[MOCLOUD_USERNAME_MAXLEN - 1] = 0x00;
    info.basicInfo.state = MOCLOUD_FILE_STATE(atoi(row[5]));
    info.readHdr = atoi(row[6]);
    info.readerNum = atoi(row[7]);
    info.writeHdr = atoi(row[8]);
    
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filetype=%d, filename=[%s], " \
        "filesize=%ld, ownerName=[%s], state=%d, readerNum=%d, readHdr=%d, writeHdr=%d\n",
        info.basicInfo.key.filetype, info.basicInfo.key.filename,
        info.basicInfo.filesize, info.basicInfo.ownerName, info.basicInfo.state,
        info.readerNum, info.readHdr, info.writeHdr);

    mysql_free_result(pRes);
    
    return 0;
}

int DbCtrlMysql::modifyFileinfo(const MOCLOUD_FILEINFO_KEY & key, DB_FILEINFO & info)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    //TODO, modify which attribute? should make sure this

    return 0;
}

int DbCtrlMysql::getFilelist(const int filetype, list<DB_FILEINFO> & filelist)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
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

    filelist.clear();

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s in (%d, %d, %d, %d);",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_FILETYPE,
        filetype & MOCLOUD_FILETYPE_VIDEO, filetype & MOCLOUD_FILETYPE_AUDIO, 
        filetype & MOCLOUD_FILETYPE_PIC, filetype & MOCLOUD_FILETYPE_OTHERS);
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int ret = mysql_query(&mDb, selectCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, selectCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return MOCLOUD_GETFILELIST_ERR_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select filelist succeed.\n");

    MYSQL_RES * pRes = mysql_use_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "Donot find any file with cmd [%s]\n", selectCmd);
        return MOCLOUD_GETFILELIST_ERR_OK;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "mysql_use_result succeed.\n");

    MYSQL_ROW row;
    DB_FILEINFO curDbFileinfo;
    while(NULL != (row = mysql_fetch_row(pRes)))
    {
        memset(&curDbFileinfo, 0x00, sizeof(DB_FILEINFO));
        curDbFileinfo.isInited = atoi(row[0]) == 0 ? false : true;
        curDbFileinfo.basicInfo.key.filetype = MOCLOUD_FILETYPE(atoi(row[1]));
        strncpy(curDbFileinfo.basicInfo.key.filename, row[2], MOCLOUD_FILENAME_MAXLEN);
        curDbFileinfo.basicInfo.key.filename[MOCLOUD_FILENAME_MAXLEN - 1] = 0x00;
        curDbFileinfo.basicInfo.filesize = atol(row[3]);
        strncpy(curDbFileinfo.basicInfo.ownerName, row[4], MOCLOUD_USERNAME_MAXLEN);
        curDbFileinfo.basicInfo.ownerName[MOCLOUD_USERNAME_MAXLEN - 1] = 0x00;
        curDbFileinfo.basicInfo.state = MOCLOUD_FILE_STATE(atoi(row[5]));
        curDbFileinfo.readHdr = atoi(row[6]);
        curDbFileinfo.readerNum = atoi(row[7]);
        curDbFileinfo.writeHdr = atoi(row[8]);

        filelist.push_back(curDbFileinfo);
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filelist being set.\n");
    mysql_free_result(pRes);
    pRes = NULL;

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

    return MOCLOUD_GETFILELIST_ERR_OK;
}

int DbCtrlMysql::getFilelist(const int filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)       
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
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

    filelistMap.clear();

    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select * from %s where %s in (%d, %d, %d, %d);",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_FILETYPE, 
        filetype & MOCLOUD_FILETYPE_VIDEO, filetype & MOCLOUD_FILETYPE_AUDIO, 
        filetype & MOCLOUD_FILETYPE_PIC, filetype & MOCLOUD_FILETYPE_OTHERS);
    selectCmd[SQL_CMD_MAXLEN] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int ret = mysql_query(&mDb, selectCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, selectCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return MOCLOUD_GETFILELIST_ERR_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select filelist succeed.\n");

    MYSQL_RES * pRes = mysql_use_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "Donot find any file with cmd [%s]\n", selectCmd);
        return MOCLOUD_GETFILELIST_ERR_OK;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "mysql_use_result succeed.\n");

    MYSQL_ROW row;
    DB_FILEINFO curDbFileinfo;
    while(NULL != (row = mysql_fetch_row(pRes)))
    {
        memset(&curDbFileinfo, 0x00, sizeof(DB_FILEINFO));
        curDbFileinfo.isInited = atoi(row[0]) == 0 ? false : true;
        curDbFileinfo.basicInfo.key.filetype = MOCLOUD_FILETYPE(atoi(row[1]));
        strncpy(curDbFileinfo.basicInfo.key.filename, row[2], MOCLOUD_FILENAME_MAXLEN);
        curDbFileinfo.basicInfo.key.filename[MOCLOUD_FILENAME_MAXLEN - 1] = 0x00;
        curDbFileinfo.basicInfo.filesize = atol(row[3]);
        strncpy(curDbFileinfo.basicInfo.ownerName, row[4], MOCLOUD_USERNAME_MAXLEN);
        curDbFileinfo.basicInfo.ownerName[MOCLOUD_USERNAME_MAXLEN - 1] = 0x00;
        curDbFileinfo.basicInfo.state = MOCLOUD_FILE_STATE(atoi(row[5]));
        curDbFileinfo.readHdr = atoi(row[6]);
        curDbFileinfo.readerNum = atoi(row[7]);
        curDbFileinfo.writeHdr = atoi(row[8]);

        for(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> >::iterator it = filelistMap.begin();
            it != filelistMap.end(); it++)
        {
            if(curDbFileinfo.basicInfo.key.filetype == it->first)
            {
                list<DB_FILEINFO> & curList = it->second;
                curList.push_back(curDbFileinfo);
                break;
            }
        }
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "filelistMap being set.\n");
    mysql_free_result(pRes);
    pRes = NULL;
    
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

bool DbCtrlMysql::isUserExist(const string & username)   
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }
    
    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select count(*) from %s where %s=\"%s\";",
        mUserinfoTableName.c_str(), USERINFO_TABLE_USERNAME, username.c_str());
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int ret = mysql_query(&mDb, selectCmd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errno=%d, errmsg=[%s]\n", 
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select succeed.\n");

    MYSQL_RES * pRes = mysql_store_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pRes=NULL. errno=%d, errmsg=[%s]\n", mysql_errno(&mDb), mysql_error(&mDb));
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get result succeed.\n");

    int rowNum = mysql_num_rows(pRes);
    if(rowNum != 1)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "rowNum=%d!\n", rowNum);
        mysql_free_result(pRes);
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "rowNum==1\n");

    MYSQL_ROW row = mysql_fetch_row(pRes);
    int isExist = atoi(row[0]);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "isExist=%d.\n", isExist);
    return isExist == 0 ? false : true;
}

bool DbCtrlMysql::isFileExist(const MOCLOUD_FILEINFO_KEY & key)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }
    
    char selectCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(selectCmd, SQL_CMD_MAXLEN, "select count(*) from %s where %s=%d and %s=\"%s\";",
        mFileinfoTableName.c_str(), FILEINFO_TABLE_FILETYPE, key.filetype, 
        FILEINFO_TABLE_FILENAME, key.filename);
    selectCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "selectCmd=[%s]\n", selectCmd);
    int ret = mysql_query(&mDb, selectCmd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "sqlite3_exec failed! ret=%d, errno=%d, errmsg=[%s]\n", 
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select succeed.\n");

    MYSQL_RES * pRes = mysql_store_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "pRes=NULL. errno=%d, errmsg=[%s]\n", mysql_errno(&mDb), mysql_error(&mDb));
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get result succeed.\n");

    int rowNum = mysql_num_rows(pRes);
    if(rowNum != 1)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "rowNum=%d!\n", rowNum);
        mysql_free_result(pRes);
        return false;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "rowNum==1\n");

    MYSQL_ROW row = mysql_fetch_row(pRes);
    int isExist = atoi(row[0]);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "isExist=%d.\n", isExist);
    return isExist == 0 ? false : true;
}

/*
    1.set all file info in table to unInited;
    2.use type+filename, check fileinfo exist or not;
    3.if exist, set to inited; else, insert it;
    4.after filelistMap being looped, all fileinfo in table unInited should deleted;
*/
int DbCtrlMysql::refreshFileinfo(map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
        return -1;
    }

    string cmd = "update ";
    cmd += mFileinfoTableName;
    cmd += " set isInited = 0;";
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "cmd=[%s]\n", cmd.c_str());
    int ret = mysql_query(&mDb, cmd.c_str());
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "update failed! ret=%d, errmsg=[%s]\n", ret, mysql_error(&mDb));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "update succeed.\n");

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
                ret = mysql_query(&mDb, insertCmd);
                if(ret != 0)
                {
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                        "insert failed! ret=%d, errmsg=[%s], cmd=[%s]\n",
                        ret, mysql_error(&mDb), insertCmd);
                    return -3;
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
                ret = mysql_query(&mDb, updateCmd);
                if(ret != 0)
                {
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                        "update failed! ret=%d, errmsg=[%s], cmd=[%s]\n",
                        ret, mysql_error(&mDb), updateCmd);
                    return -4;
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
    ret = mysql_query(&mDb, deleteCmd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "update failed! ret=%d, errmsg=[%s], cmd=[%s]\n",
            ret, mysql_error(&mDb), deleteCmd);
        return -5;
    }

    return 0;
}

int DbCtrlMysql::userLogin(const string & username, const string & passwd)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
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
    int ret = mysql_query(&mDb, selectPasswdCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "select failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, selectPasswdCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select succeed.\n");

    MYSQL_RES * pRes = mysql_store_result(&mDb);
    if(NULL == pRes)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "pRes == NULL, errno=%d, error=[%s]\n",
            mysql_errno(&mDb), mysql_error(&mDb));
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Get result succeed.\n");

    int rowNum = mysql_num_rows(pRes);
    if(rowNum != 1)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "rowNum=%d, invalid!\n", rowNum);
        mysql_free_result(pRes);
        pRes = NULL;
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "rowNum==1\n");

    MYSQL_ROW row = mysql_fetch_row(pRes);
    char passwdDb[MOCLOUD_PASSWD_MAXLEN] = {0x00};
    strncpy(passwdDb, row[0], MOCLOUD_PASSWD_MAXLEN);
    passwdDb[MOCLOUD_PASSWD_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "passwdDb=[%s]\n", passwdDb);
    mysql_free_result(pRes);
    pRes = NULL;

    //check passwdDb and @passwd equal or not
    if(passwd != passwdDb)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "input passwd=[%s], passwdInDb=[%s], donot equal! login failed!\n",
            passwd.c_str(), passwdDb);
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
    ret = mysql_query(&mDb, updateCmd);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "update failed! ret=%d, cmd=[%s], errno=%d, errmsg=[%s]\n",
            ret, updateCmd, mysql_errno(&mDb), mysql_error(&mDb));
        return MOCLOUD_LOGIN_RET_OTHERS;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Update last login time succeed.\n");
    
    return MOCLOUD_LOGIN_RET_OK;
}

int DbCtrlMysql::userLogout(const string & username)
{
    if(!isDbOk)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Db init failed! cannot do anything!\n");
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


