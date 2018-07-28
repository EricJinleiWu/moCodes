#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "fileMgr.h"
#include "utils.h"

using namespace std;

#include <vector>
#include <string>

#include <algorithm>

CaptFileInfo::CaptFileInfo() : mWidth(0), mHeight(0), mTimestamp(0), mCameraIp(0)
{
    mFaceInfoList.clear();
}

CaptFileInfo::CaptFileInfo(
    const int width, const int height, const time_t timestamp, const unsigned long cameraIp) : 
    mWidth(width), mHeight(height), mTimestamp(timestamp), mCameraIp(cameraIp)
{
    mFaceInfoList.clear();
}

CaptFileInfo::CaptFileInfo(const CaptFileInfo & other) : 
    mWidth(other.mWidth), mHeight(other.mHeight), mTimestamp(other.mTimestamp),
    mCameraIp(other.mCameraIp)
{
    mFaceInfoList.clear();
}

CaptFileInfo::~CaptFileInfo()
{
    ;
}

CaptFileInfo & CaptFileInfo::operator = (const CaptFileInfo & other)
{
    if(this == &other)
        return *this;

    mWidth = other.mWidth;
    mHeight = other.mHeight;
    mTimestamp = other.mTimestamp;
    mCameraIp = other.mCameraIp;

    mFaceInfoList = other.mFaceInfoList;
    
    return *this;
}

/* 
    timestamp is the KEY of this class;
    different cameras can send capture pictures in the same time, so when we check equal or not,
    must cmp cameraIp, too.
*/
bool CaptFileInfo::operator == (const CaptFileInfo & cmp)
{
    return (mTimestamp == cmp.mTimestamp && mCameraIp == cmp.mCameraIp);
}
bool CaptFileInfo::operator > (const CaptFileInfo & cmp)
{
    return mTimestamp > cmp.mTimestamp;
}
bool CaptFileInfo::operator < (const CaptFileInfo & cmp)
{
    return mTimestamp < cmp.mTimestamp;
}

int CaptFileInfo::getWidth()
{
    return mWidth;
}

int CaptFileInfo::getHeight()
{
    return mHeight;
}

time_t CaptFileInfo::getTimestamp()
{
    return mTimestamp;
}

unsigned long CaptFileInfo::getCameraIp()
{
    return mCameraIp;
}

void CaptFileInfo::setWidth(const int width)
{
    mWidth = width;
}

void CaptFileInfo::setHeight(const int height)
{
    mHeight= height;
}

void CaptFileInfo::setTimestamp(const time_t timestamp)
{
    mTimestamp= timestamp;
}

void CaptFileInfo::setCameraIp(const unsigned long cameraIp)
{
    mCameraIp= cameraIp;
}

void CaptFileInfo::setFaceInfoList(list<FaceInfo> & l)
{
    mFaceInfoList.clear();
    mFaceInfoList = l;
}



FileMgr::FileMgr() : mCaptDirpath(DEFAULT_CAPT_DIRPATH), mBaseDirpath(DEFAULT_BASE_DIRPATH),
    mMaxDisplayJpgNum(DEFAULT_DISPLAY_MAX_NUM), mMaxTmpJpgNum(DEFAULT_DISPLAY_MAX_NUM * 2),
    mpFaceOper(NULL)
{
    mTmpCaptFileInfoVector.clear();
    mDisplayCaptFileInfoVector.clear();
    pthread_mutex_init(&mTmpCaptFileInfoVectorMutex, NULL);
    pthread_mutex_init(&mDisplayCaptFileInfoVectorMutex, NULL);
    sem_init(&mSemaphore, 0, 0);
    mBaseFaceInfoList.clear();
}


FileMgr::FileMgr(const string & captDirpath, const string & baseDirpath, const int maxDisplayJpgNum) : 
    mCaptDirpath(captDirpath), mBaseDirpath(baseDirpath),
    mMaxDisplayJpgNum(maxDisplayJpgNum), mMaxTmpJpgNum(maxDisplayJpgNum * 2),
    mpFaceOper(NULL)
{
    mTmpCaptFileInfoVector.clear();
    mDisplayCaptFileInfoVector.clear();
    sem_init(&mSemaphore, 0, 0);
    pthread_mutex_init(&mTmpCaptFileInfoVectorMutex, NULL);
    pthread_mutex_init(&mDisplayCaptFileInfoVectorMutex, NULL);
    mBaseFaceInfoList.clear();
}

FileMgr::FileMgr(const FileMgr & other) : 
    mCaptDirpath(other.mCaptDirpath), mBaseDirpath(other.mBaseDirpath), 
    mMaxDisplayJpgNum(other.mMaxDisplayJpgNum), mMaxTmpJpgNum(other.mMaxTmpJpgNum), 
    mTmpCaptFileInfoVector(other.mTmpCaptFileInfoVector), 
    mDisplayCaptFileInfoVector(other.mDisplayCaptFileInfoVector) 
{
    sem_init(&mSemaphore, 0, 0);
    pthread_mutex_init(&mTmpCaptFileInfoVectorMutex, NULL);
    pthread_mutex_init(&mDisplayCaptFileInfoVectorMutex, NULL);
    mpFaceOper = NULL;
    mBaseFaceInfoList = other.mBaseFaceInfoList;
}

FileMgr::~FileMgr()
{
    mBaseFaceInfoList.clear();
    mTmpCaptFileInfoVector.clear();
    mDisplayCaptFileInfoVector.clear();
    sem_destroy(&mSemaphore);
    pthread_mutex_destroy(&mTmpCaptFileInfoVectorMutex);
    pthread_mutex_destroy(&mDisplayCaptFileInfoVectorMutex);
}

void FileMgr::setParams(const string & captDirpath,const string & baseDirpath,const int maxDisplayJpgNum)
{
    mCaptDirpath = captDirpath;
    mBaseDirpath= baseDirpath;
    mMaxDisplayJpgNum = maxDisplayJpgNum;
    mMaxTmpJpgNum = mMaxDisplayJpgNum * 2;
}

void FileMgr::getParams(string & captDirpath, string & baseDirpath, int & maxDisplayJpgNum)
{
    captDirpath = mCaptDirpath;
    baseDirpath= mBaseDirpath;
    maxDisplayJpgNum = mMaxDisplayJpgNum;
}

void FileMgr::dumpParams()
{
    dbgInfo("Dump FileMgr info now : CaptDirpath=[%s], BaseDirpath=[%s], MaxDisplayJpgNum=%d, MaxTmpFileNum=%d\n",
        mCaptDirpath.c_str(), mBaseDirpath.c_str(), mMaxDisplayJpgNum, mMaxTmpJpgNum);
}

void FileMgr::setFaceOperPtr(FaceOper * p)
{
    if(mpFaceOper)
    {
        delete mpFaceOper;
        mpFaceOper = NULL;
    }

    mpFaceOper = p;
}

/*
    @filename in format like : timestamp_w%d_h%d_ip%lu_capt.jpg;
*/
int FileMgr::getCaptFileInfo(const string & filename,CaptFileInfo & fileInfo)
{
    time_t timestamp;
    int width;
    int height;
    unsigned long ip;
    int ret = sscanf(filename.c_str(), CAPT_FILE_FORMAT, &timestamp, &width, &height, &ip);
    if(ret != 4)
    {
        dbgError("filename=[%s], CATP_FILE_FORMAT=[%s], donot in right format! ret=%d\n",
            filename.c_str(), CAPT_FILE_FORMAT, ret);
        return -1;
    }

    fileInfo.setWidth(width);
    fileInfo.setHeight(height);
    fileInfo.setTimestamp(timestamp);
    fileInfo.setCameraIp(ip);
    
    dbgDebug("filename=[%s], fileInfo: timestamp=%ld, width=%d, height=%d, ip=%lu\n",
        filename.c_str(), fileInfo.getTimestamp(), fileInfo.getWidth(), fileInfo.getHeight(), 
        fileInfo.getCameraIp());

    return 0;
}

int FileMgr::getYuvFilename(CaptFileInfo & fileInfo,string & yuvname)
{
    char tmp[MAX_FILENAME_LEN] = {0x00};
    memset(tmp, 0x00, MAX_FILENAME_LEN);
    snprintf(tmp, MAX_FILENAME_LEN, YUV_FILE_FORMAT, 
        fileInfo.getTimestamp(), fileInfo.getWidth(),
        fileInfo.getHeight(), fileInfo.getCameraIp());
    tmp[MAX_FILENAME_LEN - 1] = 0x00;
    yuvname = tmp;
    dbgDebug("fileInfo: timestamp=%ld, width=%d, height=%d, ip=%lu; yuvname=[%s]\n",
        fileInfo.getTimestamp(), fileInfo.getWidth(), fileInfo.getHeight(), 
        fileInfo.getCameraIp(), yuvname.c_str());

    return 0;
}

int FileMgr::getCaptFilename(CaptFileInfo & fileInfo,string & filename)
{
    char tmp[MAX_FILENAME_LEN] = {0x00};
    memset(tmp, 0x00, MAX_FILENAME_LEN);
    snprintf(tmp, MAX_FILENAME_LEN, CAPT_FILE_FORMAT, 
        fileInfo.getTimestamp(), fileInfo.getWidth(),
        fileInfo.getHeight(), fileInfo.getCameraIp());
    tmp[MAX_FILENAME_LEN - 1] = 0x00;
    filename = tmp;
    dbgDebug("fileInfo: timestamp=%ld, width=%d, height=%d, ip=%lu; filename=[%s]\n",
        fileInfo.getTimestamp(), fileInfo.getWidth(), fileInfo.getHeight(), 
        fileInfo.getCameraIp(), filename.c_str());

    return 0;
}

int FileMgr::insertCaptFileTask(CaptFileInfo & fileInfo)
{
    pthread_mutex_lock(&mTmpCaptFileInfoVectorMutex);

    //If too many tasks in, we cannot insert new task
    if(mTmpCaptFileInfoVector.size() > mMaxTmpJpgNum)
    {
        dbgError(
            "cannot insert a new capture, current captVector size=%d, maxTmpJpgNum=%d, too many tasks wait for be done.\n",
            mTmpCaptFileInfoVector.size(), mMaxTmpJpgNum);
        pthread_mutex_unlock(&mTmpCaptFileInfoVectorMutex);
        return -1;
    }

    //Check this task being insert or not, if has in vector now, donot insert again.
    for(vector<CaptFileInfo>::iterator it = mTmpCaptFileInfoVector.begin(); 
        it != mTmpCaptFileInfoVector.end(); it++)
    {
        CaptFileInfo tmp = *it;
        if(tmp == fileInfo)
        {
            dbgError("this file task being insert yet! cannot insert again! " \
                "fileInfo.timestamp=%ld, cameraIp=%lu\n", 
                fileInfo.getTimestamp(), fileInfo.getCameraIp());
            pthread_mutex_unlock(&mTmpCaptFileInfoVectorMutex);
            return -2;
        }
    }

    //insert this task to vector
    mTmpCaptFileInfoVector.push_back(fileInfo);
    pthread_mutex_unlock(&mTmpCaptFileInfoVectorMutex);
    return 0;
}

int FileMgr::delCaptFileTask(CaptFileInfo & fileInfo)
{
    pthread_mutex_lock(&mTmpCaptFileInfoVectorMutex);

    //find this fileInfo in vector or not, if exist, erase it; or, cannot delete sure.
    bool isFind = false;
    for(vector<CaptFileInfo>::iterator it = mTmpCaptFileInfoVector.begin();
        it != mTmpCaptFileInfoVector.end(); it++)
    {
        CaptFileInfo tmp = *it;
        if(tmp == fileInfo)
        {
            dbgDebug("Find this fileInfo, will delete it now. fileInfo.cameraIp=%lu, timestamp=%ld\n",
                fileInfo.getCameraIp(), fileInfo.getTimestamp());
            mTmpCaptFileInfoVector.erase(it);
            isFind = true;
            break;
        }
    }

    pthread_mutex_unlock(&mTmpCaptFileInfoVectorMutex);
    return isFind ? 0 : -1;
}

int FileMgr::notifyCaptFileTask()
{
    //send a semaphore to main thread, thread will get tasks from tmpVector to do;
    sem_post(&mSemaphore);
    return 0;
}

void FileMgr::run()
{
    while(getRunState())
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        ts.tv_nsec += 0;    //nano second
        int ret = sem_timedwait(&mSemaphore, &ts);
        if(ret < 0)
        {
            if(errno == ETIMEDOUT)
            {
                dbgDebug("sem_timedwait timeout, do next loop now.\n");
                continue;
            }
            else
            {
                dbgError("sem_timedwait failed! ret=%d, errno=%d, desc=[%s]\n",
                    ret, errno, strerror(errno));
                continue;
            }
        }

        while(getRunState())
        {
            //recv a signal, should do all tasks
            CaptFileInfo curFileTaskInfo;
            pthread_mutex_lock(&mTmpCaptFileInfoVectorMutex);
            if(mTmpCaptFileInfoVector.size() == 0)
            {
                dbgInfo("All capt file task being done.\n");
                pthread_mutex_unlock(&mTmpCaptFileInfoVectorMutex);
                break;
            }
            curFileTaskInfo = mTmpCaptFileInfoVector.back();
            mTmpCaptFileInfoVector.pop_back();
            pthread_mutex_unlock(&mTmpCaptFileInfoVectorMutex);

            //jpg->yuv, arcSoft do yuv, is have face, get face features, add info to db
            //if donot have face, should delete jpg and yuv;
            //if have face, should delete yuv, then add this task to captFileInfoVector;
            //when add to captFileInfoVector, should check number, if too many files in, should 
            //  delete one, then add a new one.
            //  when delete one from captFileInfoVector, should delete file, too.
            ret = doCaptTask(curFileTaskInfo);
            if(ret != 0)
            {
                dbgError("doCaptTask failed! ret=%d, task ip=%lu, timestamp=%ld\n",
                    ret, curFileTaskInfo.getCameraIp(), curFileTaskInfo.getTimestamp());
                continue;
            }
            dbgDebug("doCaptTask succeed. task ip=%lu, timestamp=%ld\n", curFileTaskInfo.getCameraIp(), 
                curFileTaskInfo.getTimestamp());

            insertDisplayFileInfo(curFileTaskInfo);
        }
    }
}

int FileMgr::insertDisplayFileInfo(CaptFileInfo & fileInfo)
{
    pthread_mutex_lock(&mDisplayCaptFileInfoVectorMutex);
    
    //check vector size firstly
    if(mDisplayCaptFileInfoVector.size() >= mMaxDisplayJpgNum)
    {
        vector<CaptFileInfo>::iterator minIter = min_element(mDisplayCaptFileInfoVector.begin(), mDisplayCaptFileInfoVector.end());

        CaptFileInfo tmp = *minIter;
        string jpgFilename;
        getCaptFilename(tmp, jpgFilename);
        string jpgFilepath;
        jpgFilepath = mCaptDirpath + "/" + jpgFilename;
        //TODO, send some signal to QT, one file will be delete, and new one will be insert, should replace it?
        unlink(jpgFilepath.c_str());
        
        mDisplayCaptFileInfoVector.erase(minIter);
    }

    //insert the new task to the end of vector
    mDisplayCaptFileInfoVector.push_back(fileInfo);
    pthread_mutex_unlock(&mDisplayCaptFileInfoVectorMutex);
    return 0;
}

int FileMgr::doCaptTask(CaptFileInfo & fileInfo)
{
    if(mpFaceOper == NULL)
    {
        dbgError("Donont init FaceOper object, cannot do face operation!\n");
        return -1;
    }

    //get yuvfilepath firstly
    string jpgFilename;
    string yuvFilename;
    getCaptFilename(fileInfo, jpgFilename);
    getYuvFilename(fileInfo, yuvFilename);
    string jpgFilepath = mCaptDirpath + "/" + jpgFilename;
    string yuvFilepath = mCaptDirpath + "/" + yuvFilename;
    dbgDebug("getCaptFilename succeed. jpgFilename is [%s], jpgFilepath is [%s], yuvFilepath=[%s]\n", 
        jpgFilename.c_str(), jpgFilepath.c_str(), yuvFilepath.c_str());

    //convert jpg to yuv firstly    
    int ret = mJpg2yuv.convert(jpgFilepath, yuvFilepath);
    if(ret != 0)
    {
        dbgError("convJpg2Yuv failed! ret=%d, jpgFilepath=[%s], yuvFilepath=[%s]\n",
            ret, jpgFilepath.c_str(), yuvFilepath.c_str());
        unlink(jpgFilepath.c_str());
        return -2;
    }
    dbgDebug("convJpg2Yuv succeed. jpgFilepath=[%s], yuvFilepath=[%s]\n",
        jpgFilepath.c_str(), yuvFilepath.c_str());

    //set yuv info to FaceOper firstly
    ret = mpFaceOper->setYuvFile(yuvFilepath, fileInfo.getWidth(), fileInfo.getHeight());
    if(ret != 0)
    {
        dbgError("FaceOper set yuv file info failed! ret=%d, yuvFilepath=[%s]\n",
            ret, yuvFilepath.c_str());
        unlink(jpgFilepath.c_str());
        unlink(yuvFilepath.c_str());
        return -3;
    }
    dbgDebug("FaceOper set yuv file info succeed.\n");

    //check face in this capture or not
    int faceNum = 0;
    list<FaceInfo> faceInfoList;
    ret = mpFaceOper->getFaceInfo(faceNum, faceInfoList, mBaseFaceInfoList);
    if(ret != 0 || faceNum == 0)
    {
        dbgInfo("To yuvFilepath [%s], get faces info failed! ret=%d\n", yuvFilepath.c_str(), ret);
        unlink(jpgFilepath.c_str());
        unlink(yuvFilepath.c_str());
        mpFaceOper->clearYuvFile();
        return -4;
    }
    dbgInfo("To capture [%s], find face! faceInfo dump here.\n", yuvFilepath.c_str());
    dumpFaceInfo(faceInfoList);
    mpFaceOper->clearYuvFile();

    //set all info to @fileInfo, then we need to delete yuv file, it will not be used any more
    unlink(yuvFilepath.c_str());
    fileInfo.setFaceInfoList(faceInfoList);
    
    return 0;
}

void FileMgr::getCaptDirPath(string & dirpath)
{
    dirpath = mCaptDirpath;
}

void FileMgr::getDisplayCaptFileVector(vector < CaptFileInfo > & displayVector)
{
    displayVector = mDisplayCaptFileInfoVector;
}

void FileMgr::dumpFaceInfo(list<FaceInfo> & faceInfoList)
{
    dbgDebug("Dump face info list start.\n");
    for(list<FaceInfo>::iterator it = faceInfoList.begin(); it != faceInfoList.end(); it++)
    {
        dbgDebug("left=%d, right=%d, top=%d, bottom=%d, angle=%d, age=%d, gender=%d, name=[%s]\n",
            it->getLeftPos(), it->getRightPos(), it->getTopPos(), it->getBottomPos(),
            it->getAngle(), it->getAge(), it->getGender(), it->getName().c_str());
    }
    dbgDebug("Dump face info list stop.\n");
}

int FileMgr::getBaseFaceFeatures()
{
    DIR * pDir = opendir(mBaseDirpath.c_str());
    if(NULL == pDir)
    {
        dbgError("opendir failed! dirpath=[%s], errno=%d, desc=[%s]\n",
            mBaseDirpath.c_str(), errno, strerror(errno));
        return -1;
    }
    dbgDebug("Open dir [%s] succeed.\n", mBaseDirpath.c_str());

    list<string> delFilenamesList;
    delFilenamesList.clear();

    struct dirent * pCurFile = NULL;
    while((pCurFile = readdir(pDir)) != NULL)
    {
        if(0 == strcmp(pCurFile->d_name, ".") || 0 == strcmp(pCurFile->d_name, ".."))
            continue;

        string curFilename(pCurFile->d_name);
        BaseFaceInfo curBaseFaceInfo(mBaseDirpath, curFilename);
        int ret = curBaseFaceInfo.getFeatures(mpFaceOper);
        if(ret != 0)
        {
            dbgError("curBaseFaceInfo.getFeatures failed! ret=%d, dirpath=[%s], filename=[%s]\n",
                ret, mBaseDirpath.c_str(), curFilename.c_str());
            delFilenamesList.push_back(curFilename);
            continue;
        }
        dbgDebug("curBaseFaceInfo.getFeatures succeed. filename=[%s]\n", curFilename.c_str());

        mBaseFaceInfoList.push_back(curBaseFaceInfo);
    }
    dbgDebug("Finally, in directory [%s], we find %d valid base face files.\n",
        mBaseDirpath.c_str(), mBaseFaceInfoList.size());
    closedir(pDir);
    pDir = NULL;

    //should delete all invalid files
    for(list<string>::iterator it=delFilenamesList.begin(); it != delFilenamesList.end(); it++)
    {
        string curFilename = *it;
        string curFilepath = mBaseDirpath + "/" + curFilename;
        unlink(curFilepath.c_str());
    }
    return 0;
}



FileMgr * FileMgrSingleton::pFileMgr = new FileMgr();

FileMgr * FileMgrSingleton::getInstance() 
{
    return pFileMgr;
}

