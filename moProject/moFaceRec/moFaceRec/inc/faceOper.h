#ifndef __FACE_OPER_H__
#define __FACE_OPER_H__

#include "arcsoft_fsdk_face_detection.h"
#include "arcsoft_fsdk_face_recognition.h"

using namespace std;
#include <string>
#include <list>

class BaseFaceInfo;


class FaceInfo
{
public:
    FaceInfo();
    FaceInfo(int leftPos, int rightPos, int topPos, int bottomPos, int angle);
    FaceInfo(const FaceInfo & other);
    virtual ~FaceInfo();

public:
    FaceInfo  & operator = (const FaceInfo & other);

public:
    virtual int getLeftPos();
    virtual int getRightPos();
    virtual int getTopPos();
    virtual int getBottomPos();
    virtual int getAngle();
    virtual void getAll(int &leftPos, int &rightPos, int &topPos, int &bottomPos, int &angle,
        char & gender, int & age, string & name);

    virtual void setLeftPos(const int pos);
    virtual void setRightPos(const int pos);
    virtual void setTopPos(const int pos);
    virtual void setBottomPos(const int pos);
    virtual void setAngle(const int angle);
    virtual void setAll(int leftPos, int rightPos, int topPos, int bottomPos, int angle,
        char gender, int age, string & name);

public:
    virtual char getGender();
    virtual int getAge();
    virtual string getName();

    virtual void setGender(const char gender);
    virtual void setAge(const int age);
    virtual void setName(const string & name);

private:
    int mLeftPos;
    int mRightPos;
    int mTopPos;
    int mBottomPos;
    int mAngle;

    char mGender;       //gender, 0-man, 1-woman;
    int mAge;   //age
    string mName;   //who being matched, if being matched, then its real man name; if donot matched to base face, its "UnKnown";
};

class FaceOper
{
public:
    FaceOper();
    FaceOper(string & yuvFilepath);
    virtual ~FaceOper();

public:
    /*
        before we do face operation, register is needed;
        this will give the key to ARC SOFT SDK, then we can do face oper by it;
        after we do all what we wanted, we should unregister to it.
    */
    virtual int register2Arc();
    virtual void unRegister2Arc();

public:
    /*
        Set yuv filepath to local memory, then read this file data, set to memory, get age, gender, faceNum, will use this memory, all;
        we will parse this file when do face oper;

        return 0 if succeed, else 0- return;
    */
    virtual int setYuvFile(const string & yuvFilepath, const int width, const int height);

    /*
        clear the memory being used, set all params to default.
    */
    virtual int clearYuvFile();

public:
    /*
        get all face info for @mCurYuvFilepath;
        In one picture, may be have more than 1 faces, I set the face number to @faceNum;
        And each face info will be insert into @faceInfoList;
        
        return 0 if ok, 0- else;
        If donot have face in this picture, return 0-, too.
    */
    virtual int getFaceInfo(int & faceNum, list<FaceInfo> & faceInfoList, list<BaseFaceInfo> & baseFaceinfoList);

    /* 
        just get face detect result;
    */
    virtual int getFaceResult(LPAFD_FSDK_FACERES & faceResult);

    /*
        get the features by this faceResult;
    */
    virtual int getFaceFeature(AFR_FSDK_FACEINPUT & faceInput, int &featureLen, unsigned char *& pFeatures);

private:
    virtual int mallocWorkMem();
    virtual int freeWorkMem();
    virtual int initAllHandles();

private:
    /*
        read @mCurYuvFilepath, get all data, set to @mpCurYuvData, size set to @mCurYuvSize;
    */
    virtual int readYuvFileData();

    virtual int setArcImgInfo(ASVLOFFSCREEN * pImgInfo);

private:
    string mCurYuvFilepath;
    bool mIsRegisterd;

    void * mpFaceDetectHdl;
    void * mpFaceAgeHdl;
    void * mpFaceGenderHdl;
    void * mpFaceRecoHdl;

    unsigned char * mpFaceDetectMem;
    unsigned char * mpFaceAgeMem;
    unsigned char * mpFaceGenderMem;
    unsigned char * mpFaceRecoMem;

    unsigned char * mpCurYuvData;
    size_t mCurYuvSize;
    int mCurWidth;
    int mCurHeight;
};



class BaseFaceInfo
{
public:
    BaseFaceInfo();
    BaseFaceInfo(const string & dirpath, const string & jpgFilename);
    BaseFaceInfo(const BaseFaceInfo & other);
    ~BaseFaceInfo();

public:
    BaseFaceInfo & operator = (const BaseFaceInfo & other);

public:
    virtual int getFeatures(FaceOper * pFaceOper);

public:
    virtual int getFeatureLen();
    virtual unsigned char * getFeatureValue();
    virtual string getName();

private:
    int mWidth;
    int mHeight;
    string mName;

    int mFeatureLen;
    unsigned char * mpFeature;

    string mDirpath;
    string mJpgFilename;
};

#endif
