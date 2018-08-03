#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "db.h"
#include "utils.h"

using namespace std;
#include <string>
#include <list>

#define DEFAULT_PEOPLE_NAME "defaultPeopleName"
#define DEFAULT_CAMERA_NAME "defaultCameraName"

#define DEFAULT_MYSQL_DATABASE_NAME "mysql"

DbItem::DbItem() : 
    mTimestamp(0), mCameraIp(0), mCameraName(DEFAULT_CAMERA_NAME)
{
    mPeopleNameVector.clear();
}

DbItem::DbItem(const time_t timestamp, const unsigned long long cameraIp, 
    const string & cameraName, const vector<string> &peopleNameVector) : 
    mTimestamp(timestamp), mCameraIp(cameraIp), 
    mCameraName(cameraName), mPeopleNameVector(peopleNameVector)
{
    ;
}

DbItem::DbItem(const DbItem & other) : 
    mTimestamp(other.mTimestamp), mCameraIp(other.mCameraIp), 
    mCameraName(other.mCameraName), mPeopleNameVector(other.mPeopleNameVector)
{
    ;
}

DbItem & DbItem::operator = (const DbItem & other)
{
    if(this == &other)
        return *this;

    mTimestamp = other.mTimestamp;
    mCameraIp = other.mCameraIp;
    mCameraName = other.mCameraName;
    mPeopleNameVector = other.mPeopleNameVector;

    return *this;
}
    
DbItem::~DbItem()
{
    ;
}

DbMysql::DbMysql() : mDbName(MYSQL_DB_NAME), mFaceRecTabName(MYSQL_FACEREC_TABNAME),
    mIsLogin(false), mState(DB_MYSQL_STATE_IDLE)
{
    mysql_library_init(0, NULL, NULL);
    mysql_init(&mDb);
    pthread_mutex_init(&mStateLock, NULL);
}

DbMysql::DbMysql(const string & dbName,const string & faceRecordTableName) : 
    mDbName(dbName), mFaceRecTabName(faceRecordTableName), mIsLogin(false),
    mState(DB_MYSQL_STATE_IDLE)
{
    mysql_library_init(0, NULL, NULL);
    mysql_init(&mDb);
    pthread_mutex_init(&mStateLock, NULL);
}

DbMysql::~DbMysql()
{
    pthread_mutex_destroy(&mStateLock);
    mysql_close(&mDb);
    mysql_library_end();
}

/*
    if table donot exist, should create it here.
*/
int DbMysql::login()
{
    if(mIsLogin)
    {
        dbgError("DbMysql has been login, cannot login again!\n");
        return -1;
    }

    if(!mysql_real_connect(&mDb, "localhost", "root", "0", DEFAULT_MYSQL_DATABASE_NAME, 0, NULL, 0))
    {
        dbgError("mysql_real_connect to default db failed! errno=%d, errmsg=[%s]\n",
            mysql_errno(&mDb), mysql_error(&mDb));
        return -2;
    }
    dbgDebug("mysql_real_connect succeed.\n");    

    char createDbCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(createDbCmd, SQL_CMD_MAXLEN, 
        "create database if not exists %s character set utf8;",
        mDbName.c_str());
    dbgDebug("createDbCmd is [%s]\n", createDbCmd);
    int ret = mysql_query(&mDb, createDbCmd);
    if(ret != 0)
    {
        dbgError("mysql_query failed! ret=%d, errno=%d, errmsg=[%s], cmd=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb), createDbCmd);
        return -3;
    }
    dbgDebug("create database %s succeed.\n", mDbName.c_str());

    ret = mysql_select_db(&mDb, mDbName.c_str());
    if(ret != 0)
    {
        dbgError("mysql_select_db failed! ret=%d, errno=%d, errmsg=[%s], dbName=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb), mDbName.c_str());
        return -4;
    }

    char createCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(createCmd, SQL_CMD_MAXLEN, 
        "create table if not exists %s(%s bigint, %s bigint, %s char(%d)",
        mFaceRecTabName.c_str(),
        FACEREC_TABLE_ATTR_TIMESTAMP, 
        FACEREC_TABLE_ATTR_CAMERAIP, 
        FACEREC_TABLE_ATTR_CAMERANAME, MAX_CAMERA_NAME_LEN);

    char tmp[32] = {0x00};
    int i = 0;
    for(i = 0; i < MAX_FACE_NUM; i++)
    {
        memset(tmp, 0x00, 32);
        sprintf(tmp, ", %s%d char(%d)", FACEREC_TABLE_ATTR_PEOPLENAME, i, MAX_PEOPLE_NAME_LEN);
        strcat(createCmd, tmp);
    }
    strcat(createCmd, ");");
    
    createCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    dbgDebug("create facerecord table cmd is [%s]\n", createCmd);
    ret = mysql_query(&mDb, createCmd);
    if(ret != 0)
    {
        dbgError("create facerecord table failed! ret=%d, errno=%d, errmsg=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb));
        return -5;
    }
    dbgDebug("Create facerecord table succeed.\n");

    mIsLogin = true;

    return 0;
}

int DbMysql::logout()
{
    mIsLogin = false;

    return 0;
}

int DbMysql::query(const time_t startTime, const time_t endTime,
    const string & peopleName, list < DbItem > & dbItemList)
{
    pthread_mutex_lock(&mStateLock);

    int cnt = 0;
    for(cnt = 0; cnt < 3; cnt++)
    {
        if(mState != DB_MYSQL_STATE_IDLE)
        {
            sleep(1);
            continue;
        }
        mState = DB_MYSQL_STATE_QUERY;
        break;
    }
    
    if(mState != DB_MYSQL_STATE_QUERY)
    {
        dbgError("mState=%d, cannot do query now.\n", mState);
        pthread_mutex_unlock(&mStateLock);
        return -1;
    }
    
    pthread_mutex_unlock(&mStateLock);

    char queryCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(queryCmd, SQL_CMD_MAXLEN, 
        "select * from %s where %s>=%ld and %s<=%ld and (",
        mFaceRecTabName.c_str(), 
        FACEREC_TABLE_ATTR_TIMESTAMP, startTime,
        FACEREC_TABLE_ATTR_TIMESTAMP, endTime);
    char tmp[MAX_PEOPLE_NAME_LEN * 2] = {0X00};
    int i = 0;
    for(i = 0; i < MAX_FACE_NUM - 1; i++)
    {
        sprintf(tmp, "%s%d=\"%s\" or ", FACEREC_TABLE_ATTR_PEOPLENAME, i, peopleName.c_str());
        strcat(queryCmd, tmp);
    }
    sprintf(tmp, "%s%d=\"%s\");", FACEREC_TABLE_ATTR_PEOPLENAME, MAX_FACE_NUM - 1, peopleName.c_str());
    strcat(queryCmd, tmp);
    queryCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    dbgDebug("queryCmd=[%s]\n", queryCmd);

    int ret = mysql_query(&mDb, queryCmd);
    if(ret != 0)
    {
        dbgError("mysql_query failed! ret=%d, errno=%d, errmsg=[%s], queryCmd=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb), queryCmd);
        mState = DB_MYSQL_STATE_IDLE;
        return -2;
    }
    dbgDebug("mysql_query for query succeed.\n");

    dbItemList.clear();

    MYSQL_RES * pRes = mysql_use_result(&mDb);
    if(NULL == pRes)
    {
        dbgError("Donot find any file with cmd [%s]\n", queryCmd);
        mState = DB_MYSQL_STATE_IDLE;
        return 0;
    }
    dbgDebug("mysql_use_result succeed.\n");

    MYSQL_ROW row;
    DbItem curItem;
    while(NULL != (row = mysql_fetch_row(pRes)))
    {
        curItem.mTimestamp = atol(row[0]);
        curItem.mCameraIp = atol(row[1]);
        curItem.mCameraName = row[2];

        //people names
        curItem.mPeopleNameVector.clear();
        int i = 0;
        for(i = 0; i < MAX_FACE_NUM; i++)
        {
            if(strlen(row[3 + i]) != 0)
            {
                string tmp(row[3 + i]);
                curItem.mPeopleNameVector.push_back(tmp);
            }
        }
    
        dbItemList.push_back(curItem);
    }
    dbgDebug("get all items succeed.\n");
    mysql_free_result(pRes);
    pRes = NULL;
    
    mState = DB_MYSQL_STATE_IDLE;

    return 0;
}

/*
    insert a new item to db;
*/
int DbMysql::insert(DbItem & item)
{
    if(item.mPeopleNameVector.size() > MAX_FACE_NUM)
    {
        dbgError("Mostly, I allowed %d face in a picture, input is %d, too large.\n",
            MAX_FACE_NUM, item.mPeopleNameVector.size());
        return -1;
    }
 
    pthread_mutex_lock(&mStateLock);

    int cnt = 0;
    for(cnt = 0; cnt < 3; cnt++)
    {
        if(mState != DB_MYSQL_STATE_IDLE)
        {
            sleep(1);
            continue;
        }
        mState = DB_MYSQL_STATE_INSERT;
        break;
    }
    
    if(mState != DB_MYSQL_STATE_INSERT)
    {
        dbgError("mState=%d, cannot do insert now.\n", mState);
        pthread_mutex_unlock(&mStateLock);
        return -2;
    }
    
    pthread_mutex_unlock(&mStateLock);
    
    char insertCmd[SQL_CMD_MAXLEN] = {0x00};
    snprintf(insertCmd, SQL_CMD_MAXLEN, 
        "insert into %s values(%ld, %ld, \"%s\"", 
        mFaceRecTabName.c_str(), 
        item.mTimestamp, 
        item.mCameraIp, 
        item.mCameraName.c_str());
    char tmp[MAX_PEOPLE_NAME_LEN * 2] = {0x00};
    int i = 0;
    for(i = 0; i < item.mPeopleNameVector.size(); i++)
    {
        memset(tmp, 0x00, MAX_PEOPLE_NAME_LEN * 2);
        sprintf(tmp, ", \"%s\"", item.mPeopleNameVector[i].c_str());
        strcat(insertCmd, tmp);
    }
    for(i = 0; i < MAX_FACE_NUM - item.mPeopleNameVector.size(); i++)
    {
        memset(tmp, 0x00, MAX_PEOPLE_NAME_LEN * 2);
        strcpy(tmp, ", \"\"");
        strcat(insertCmd, tmp);
    }
    strcat(insertCmd, ");");
    insertCmd[SQL_CMD_MAXLEN - 1] = 0x00;
    dbgDebug("insertCmd is [%s]\n", insertCmd);

    int ret = mysql_query(&mDb, insertCmd);
    if(ret != 0)
    {
        dbgError("insert new item to db failed! ret=%d, errno=%d, errmsg=[%s], inertcmd=[%s]\n",
            ret, mysql_errno(&mDb), mysql_error(&mDb), insertCmd);
        mState = DB_MYSQL_STATE_IDLE;
        return -3;
    }
    dbgDebug("insert new item to db succeed.\n");
    
    mState = DB_MYSQL_STATE_IDLE;

    return 0;
}

void DbMysql::run()
{
    while(getRunState())
    {
        sleep(1);

        if(DB_SAVE_MAX_DAYS < 0)
            continue;

        time_t curTime = time(NULL);
        struct tm * pCurLocalTime = localtime(&curTime);
        if(pCurLocalTime->tm_hour == 0 && pCurLocalTime->tm_min == 0 && pCurLocalTime->tm_sec == 0)
        {
            dbgInfo("Current timestamp is %ld, I will clear old db records now.\n", curTime);

            //wait other operations being done before delete
            pthread_mutex_lock(&mStateLock);
            int i = 0;
            for(i = 0; i < 10; i++)
            {
                if(mState == DB_MYSQL_STATE_IDLE)
                {
                    mState = DB_MYSQL_STATE_DELETE;
                    break;
                }
                sleep(1);
            }
            pthread_mutex_unlock(&mStateLock);

            if(i >= 10)
            {
                dbgError("Donot get db control in 10 seconds! cannot delete records today!\n");
                continue;
            }

            time_t limitTimestamp = curTime - (DB_SAVE_MAX_DAYS * 24 * 60 * 60);
            char deleteCmd[SQL_CMD_MAXLEN] = {0x00};
            snprintf(deleteCmd, SQL_CMD_MAXLEN,
                "delete from %s where %s<%ld;",
                mFaceRecTabName.c_str(), FACEREC_TABLE_ATTR_TIMESTAMP, limitTimestamp);
            dbgDebug("deleteCmd = [%s]\n", deleteCmd);
            int ret = mysql_query(&mDb, deleteCmd);
            if(ret != 0)
            {
                dbgError("mysql_query to delete records failed! ret=%d, errno=%d, errmsg=[%s], cmd=[%s]\n",
                    ret, mysql_errno(&mDb), mysql_error(&mDb), deleteCmd);
            }
            else
                dbgDebug("mysql_query to delete records succeed!\n");
            mState = DB_MYSQL_STATE_IDLE;        
        }
    }
}


DbMysql * DbMysqlSingleton::pDbMysql = new DbMysql(MYSQL_DB_NAME, MYSQL_FACEREC_TABNAME);

DbMysql * DbMysqlSingleton::getInstance() 
{
    return pDbMysql;
}


