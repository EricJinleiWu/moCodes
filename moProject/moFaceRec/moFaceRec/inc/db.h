#ifndef __DB_MYSQL_H__
#define __DB_MYSQL_H__

#include "moThread.h"

#include "mysql/mysql.h"

using namespace std;
#include <string>
#include <list>
#include <vector>

#define MYSQL_DB_NAME   "MoFaceRecMysqlDb"
#define MYSQL_FACEREC_TABNAME "faceRecordTable"

#define SQL_CMD_MAXLEN  512

#define FACEREC_TABLE_ATTR_PEOPLENAME   "peopleName"
#define FACEREC_TABLE_ATTR_TIMESTAMP    "timestamp"
#define FACEREC_TABLE_ATTR_CAMERAIP     "cameraIp"
#define FACEREC_TABLE_ATTR_CAMERANAME   "cameraName"

/*
    Each day, do one check;
    delete the records which too old;
    this can help us to control the size of db;
    if donot want delete them, just set this value to 0-;
*/
#define DB_SAVE_MAX_DAYS    7

class DbItem
{
public:
    DbItem();
    DbItem(const time_t timestamp, const unsigned long long cameraIp, 
        const string & cameraName, const vector<string> &peopleNameVector);
    DbItem(const DbItem & other);
    virtual ~DbItem();

public:
    DbItem & operator = (const DbItem & other);

public:
    time_t mTimestamp;
    unsigned long long mCameraIp;
    string mCameraName;
    vector<string> mPeopleNameVector;
};

typedef enum
{
    DB_MYSQL_STATE_IDLE, 
    DB_MYSQL_STATE_INSERT,
    DB_MYSQL_STATE_QUERY,
    DB_MYSQL_STATE_DELETE,
}DB_MYSQL_STATE;

class DbMysql : public MoThread
{
public:
    DbMysql();
    DbMysql(const string & dbName, const string & faceRecordTableName);
    virtual ~DbMysql();

public:
    /*
        init to mysql;
        if table donot exist, create it;
        must be called at first, then other db operations can be call;
    */
    virtual int login();
    /*
        uninit to mysql;
    */
    virtual int logout();

    /*
        get all result by the choices;
    */
    virtual int query(const time_t startTime, const time_t endTime, const string & peopleName,
        list<DbItem> & dbItemList);

    /*
        insert a new db item to db;
    */
    virtual int insert(DbItem & item);

public:
    virtual void run();

private:
    MYSQL mDb;  //the handle of mysql

    string mDbName;
    string mFaceRecTabName;

    bool mIsLogin;

    DB_MYSQL_STATE mState;
    pthread_mutex_t mStateLock;
};


class DbMysqlSingleton
{
public:    
    static DbMysql * getInstance();

private:
    DbMysqlSingleton() {}
    ~DbMysqlSingleton() {}

    static DbMysql * pDbMysql;
};


#endif
