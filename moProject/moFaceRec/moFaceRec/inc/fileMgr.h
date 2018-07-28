#ifndef __FILE_MGR_H__
#define __FILE_MGR_H__

#include <time.h>
#include <semaphore.h>
#include <pthread.h>

#include "moThread.h"
#include "jpg2yuv.h"

#include "faceOper.h"

using namespace std;

#include <string>
#include <vector>

class CaptFileInfo
{
public:
    CaptFileInfo();
    CaptFileInfo(const int width, const int height, const time_t timestamp, const unsigned long cameraIp);
    CaptFileInfo(const CaptFileInfo & other);
    virtual ~CaptFileInfo();

public:
    CaptFileInfo & operator = (const CaptFileInfo & other);
    /* timestamp being the standard */
    bool operator < (const CaptFileInfo & cmp);
    bool operator > (const CaptFileInfo & cmp);
    bool operator == (const CaptFileInfo & cmp);

public:
    virtual int getWidth();
    virtual int getHeight();
    virtual time_t getTimestamp();
    virtual unsigned long getCameraIp();

public:
    virtual void setWidth(const int width);
    virtual void setHeight(const int height);
    virtual void setTimestamp(const time_t timestamp);
    virtual void setCameraIp(const unsigned long cameraIp);

public:
    virtual void setFaceInfoList(list<FaceInfo> & l);

private:
    int mWidth;
    int mHeight;
    time_t mTimestamp;
    unsigned long mCameraIp;

    list<FaceInfo> mFaceInfoList;
};

class FileMgr : public MoThread
{
public:
    FileMgr();
    FileMgr(const string & captDirpath, const string & baseDirpath, const int maxDisplayJpgNum);
    FileMgr(const FileMgr & other);
    virtual ~FileMgr();

public:
    virtual void setParams(const string & captDirpath, const string & baseDirpath, const int maxDisplayJpgNum);
    virtual void getParams(string & captDirpath, string & baseDirpath, int & maxDisplayJpgNum);
    virtual void dumpParams();

    virtual void setFaceOperPtr(FaceOper * p);

    /*
        get each file from @mBaseDirpath;
        check it has face and only have one face or not;
        get its face features;
        set to local memory;
    */
    virtual int getBaseFaceFeatures();

public:
    /*
        Check how many tasks in our memory, if too more, insert cannot be done;
        If can be inserted, just insert to our memory, donot write file,
        write file will be done by outside, they can do this in multi threads;
    */
    virtual int insertCaptFileTask(CaptFileInfo & fileInfo);

    /*
        A file task have face, should display to user, then we add it to display vector;
        should check the size of this vector, if too large, should delete one task;
        we will delete the task has smallest timestamp;
    */
    virtual int insertDisplayFileInfo(CaptFileInfo & fileInfo);

    /*
        After write file to @mCaptDirpath, call this method to notify FileMgr to do this filetask;
    */
    virtual int notifyCaptFileTask();

    /*
        If needed, call this function to delete the capture file info from our memory;
        donot delete file, delete operation will be done in outside by caller itself;
    */
    virtual int delCaptFileTask(CaptFileInfo & fileInfo);

public:
    /*
        do this task;
        set gender & age & name to @fileInfo;
    */
    virtual int doCaptTask(CaptFileInfo & fileInfo);

public:
    virtual void getCaptDirPath(string & dirpath);
    virtual void getDisplayCaptFileVector(vector<CaptFileInfo> & displayVector);

public:
    /* convert fileinfo to filename in format like : timestamp_w%d_h%d_ip%lu_capt.jpg/.yuv; */
    virtual int getCaptFilename(CaptFileInfo & fileInfo, string & filename);
    virtual int getYuvFilename(CaptFileInfo & fileInfo, string & yuvname);
    /* convert filename in format like : timestamp_w%d_h%d_ip%lu_capt.jpg to fileinfo; */
    virtual int getCaptFileInfo(const string & filename, CaptFileInfo & fileInfo);

public:
    virtual void dumpFaceInfo(list<FaceInfo> & faceInfoList);

public:
    virtual void run();

private:
    string mCaptDirpath;
    string mBaseDirpath;
    int mMaxDisplayJpgNum;
    int mMaxTmpJpgNum;
    vector<CaptFileInfo> mTmpCaptFileInfoVector;
    pthread_mutex_t mTmpCaptFileInfoVectorMutex;
    vector<CaptFileInfo> mDisplayCaptFileInfoVector;
    pthread_mutex_t mDisplayCaptFileInfoVectorMutex;
    sem_t mSemaphore;

    Jpg2Yuv mJpg2yuv;

    FaceOper * mpFaceOper;

    list<BaseFaceInfo> mBaseFaceInfoList;
};

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
