#ifndef __FILE_MGR_H__
#define __FILE_MGR_H__

using namespace std;

#include <string>
#include <list>
#include <map>

#include "moCloudUtilsTypes.h"

#define SQLITE_DBNAME       "mocloudserver_sqlite3.db"
#define MYSQL_DBNAME        "mocloudserver_mysql.db"
#define USERINFO_TABLENAME  "UserinfoTable"
#define FILEINFO_TABLENAME  "FileinfoTable"

typedef enum
{
    DB_USER_ROLE_ORDINARY,
    DB_USER_ROLE_ADMIN
}DB_USER_ROLE;

typedef struct
{
    string username;
    string password;
    DB_USER_ROLE role;
    time_t lastLogInTime;
    time_t signUpTime;
}DB_USERINFO;

typedef struct
{
    //When init, we should check the files in database is exist or not;
    bool isInited;

    //We can show this value to client
    MOCLOUD_BASIC_FILEINFO basicInfo;
    
    //These value are ctrl info, being used with ourselves
    int readHdr;    //The read handler for this file
    int readerNum;  //how many clients read this file
    
    int writeHdr;   //The write handler for this file, just when uploading we should use it.
}DB_FILEINFO;

class DbCtrl
{
public:
    DbCtrl() {}
    virtual ~DbCtrl() {}

public:
    //@info.username will be the key, if exist yet, insert will not succeed.
    virtual int insertUserinfo(const DB_USERINFO & info) = 0;
    //@username is the key of this table
    virtual int deleteUserinfo(const string & username) = 0;
    //@info.username should valid, will get all info for this user
    virtual int getUserinfo(const string & username, DB_USERINFO & info) = 0;
    //@username should valid, will modify its value to @info
    virtual int modifyUserinfo(const string & username, DB_USERINFO & info) = 0;

    //@info.type & @info.name to be the key of this table
    virtual int insertFileinfo(const DB_FILEINFO & info) = 0;
    virtual int deleteFileinfo(const MOCLOUD_FILETYPE & filetype, const string & filename) = 0;
    //@info.type & @info.name should valid
    virtual int getFileinfo(DB_FILEINFO & info) = 0;
    //username should valid, will modify its value to @info
    virtual int modifyFileinfo(const MOCLOUD_FILETYPE & filetype, const string & filename, DB_FILEINFO & info) = 0;  

    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, list<DB_FILEINFO> & filelist) = 0;
    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap) = 0;
};

class DbCtrlSqlite : public DbCtrl
{
public:
    DbCtrlSqlite();
    DbCtrlSqlite(const DbCtrlSqlite & other);
    virtual ~DbCtrlSqlite();

public:
    DbCtrlSqlite & operator = (const DbCtrlSqlite & other);

public:
    //@info.username will be the key, if exist yet, insert will not succeed.
    virtual int insertUserinfo(const DB_USERINFO & info);
    //@username is the key of this table
    virtual int deleteUserinfo(const string & username);
    //@info.username should valid, will get all info for this user
    virtual int getUserinfo(const string & username, DB_USERINFO & info);
    //@username should valid, will modify its value to @info
    virtual int modifyUserinfo(const string & username, DB_USERINFO & info);

    //@info.type & @info.name to be the key of this table
    virtual int insertFileinfo(const DB_FILEINFO & info);
    virtual int deleteFileinfo(const MOCLOUD_FILEINFO_KEY & key);
    //@info.type & @info.name should valid
    virtual int getFileinfo(DB_FILEINFO & info);
    //username should valid, will modify its value to @info
    virtual int modifyFileinfo(const MOCLOUD_FILEINFO_KEY & key, DB_FILEINFO & info);

    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, list<DB_FILEINFO> & filelist);
    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap);
    
public:
    bool isUserExist(const string & username);
    bool isFileExist(const MOCLOUD_FILEINFO_KEY & key);

private:
    string mDbName;
    string mUserinfoTableName;
    string mFileinfoTableName;
};

class DbCtrlMysql : public DbCtrl
{
public:
    DbCtrlMysql();
    DbCtrlMysql(const DbCtrlMysql & other);
    virtual ~DbCtrlMysql();

public:
    DbCtrlMysql & operator = (const DbCtrlMysql & other);

public:
    //@info.username will be the key, if exist yet, insert will not succeed.
    virtual int insertUserinfo(const DB_USERINFO & info);
    //@username is the key of this table
    virtual int deleteUserinfo(const string & username);
    //@info.username should valid, will get all info for this user
    virtual int getUserinfo(const string & username, DB_USERINFO & info);
    //@username should valid, will modify its value to @info
    virtual int modifyUserinfo(const string & username, DB_USERINFO & info);

    //@info.type & @info.name to be the key of this table
    virtual int insertFileinfo(const DB_FILEINFO & info);
    virtual int deleteFileinfo(const MOCLOUD_FILEINFO_KEY & key);
    //@info.type & @info.name should valid
    virtual int getFileinfo(DB_FILEINFO & info);
    //username should valid, will modify its value to @info
    virtual int modifyFileinfo(const MOCLOUD_FILEINFO_KEY & key, DB_FILEINFO & info);

    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, list<DB_FILEINFO> & filelist);
    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap);
    
public:
    bool isUserExist(const string & username);
    bool isFileExist(const MOCLOUD_FILEINFO_KEY & key);

private:
    string mDbName;
    string mUserinfoTableName;
    string mFileinfoTableName;
};

class FileMgr
{
public:
    FileMgr();
    //@dirpath is the path of our files
    FileMgr(const string & dirpath);
    FileMgr(const FileMgr & other);
    virtual ~FileMgr();

public:
    FileMgr & operator = (const FileMgr & other);

public:
    virtual int refreshFileinfoTable();
    
    virtual int readFile(const MOCLOUD_FILEINFO_KEY & key, const size_t & offset,
        const size_t length, char * pData);

    //just uploading file will use this function
    virtual int writeFile(const MOCLOUD_FILEINFO_KEY & key, const size_t & offset,
        const size_t length, char * pData);

    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, list<DB_FILEINFO> & filelist);
    virtual int getFilelist(const MOCLOUD_FILETYPE & filetype, 
        map<MOCLOUD_FILETYPE, list<DB_FILEINFO> > & filelistMap);

    virtual int modifyDirpath(const string & newDirpath);

//public:
//    static FileMgr * getInstance() {return FileMgr::pFileMgr;}

private:
    string mDirpath;
};

#if 1
class FileMgrSingleton
{
public:    
    static FileMgr * getInstance();

private:
    FileMgrSingleton() {}
    ~FileMgrSingleton() {}

    static FileMgr * pFileMgr;
};


#endif

#endif
