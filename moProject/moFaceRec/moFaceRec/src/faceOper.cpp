#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

#include "arcsoft_fsdk_face_detection.h"
#include "arcsoft_fsdk_age_estimation.h"
#include "arcsoft_fsdk_face_recognition.h"
#include "arcsoft_fsdk_face_tracking.h"
#include "arcsoft_fsdk_gender_estimation.h"
#include "utils.h"
#include "faceOper.h"
#include "jpg2yuv.h"

using namespace std;
#include <string>
#include <list>
#include <vector>

#define ARCSOFT_APPID                   "9MiPVe9YtczBhGZyUweHowHFWWmRSZtL2qKNKL9c1KtX"

#define ARCSOFT_FACE_DETECTION_SDKKEY   "4abwaxfukC5Q341JDKW8PHCjLkmcY9fNVHYu9c2Kcr2J"
#define ARCSOFT_FACE_DETECTION_WORKMEM_SIZE     (40 * 1024 * 1024)
#define ARCSOFT_FACE_DETECTION_IMG_DIR      "./faceDetectionTstFiles"

#define ARCSOFT_FACE_RECO_SDKKEY        "4abwaxfukC5Q341JDKW8PHCrWA2kMiZ3pSfvwtg7zVye"
#define ARCSOFT_FACE_RECO_WORKMEM_SIZE          (40 * 1024 * 1024)
#define ARCSOFT_FACE_RECO_IMG_DIR      "./faceRecoTstFiles"

#define ARCSOFT_FACE_TRACKING_SDKKEY    "4abwaxfukC5Q341JDKW8PHCcBMWUvTh5Tq4ax3wwNff3"
#define ARCSOFT_FACE_TRACKING_WORKMEM_SIZE     (40 * 1024 * 1024)
#define ARCSOFT_FACE_TRACKING_IMG_DIR      "./faceTrackingTstFiles"

#define ARCSOFT_AGE_SDKKEY              "4abwaxfukC5Q341JDKW8PHDUKALcpV7yAsVaNL629k2e"
#define ARCSOFT_FACE_AGE_WORKMEM_SIZE           (30 * 1024 * 1024)
#define ARCSOFT_FACE_AGE_IMG_DIR      "./faceAgeEstimationTstFiles"

#define ARCSOFT_GENDER_SDKKEY           "4abwaxfukC5Q341JDKW8PHDbUZbnf1cY2GdfsZWnUcXy"
#define ARCSOFT_FACE_GENDER_WORKMEM_SIZE        (30 * 1024 * 1024)
#define ARCSOFT_FACE_GENDER_IMG_DIR      "./faceGenderEstimationTstFiles"

#define MAX_FACE_NUM    16


BaseFaceInfo::BaseFaceInfo() : mWidth(0), mHeight(0), mName(UNKNOWN_PEOPLE_NAME),
    mFeatureLen(0), mpFeature(NULL), mDirpath("./"), mJpgFilename("")
{
    ;
}

BaseFaceInfo::BaseFaceInfo(const string & dirpath, const string & jpgFilename) : 
    mWidth(0), mHeight(0), mName(UNKNOWN_PEOPLE_NAME),
    mFeatureLen(0), mpFeature(NULL), 
    mDirpath(dirpath), mJpgFilename(jpgFilename)
{
    ;
}

BaseFaceInfo::BaseFaceInfo(const BaseFaceInfo & other) : 
    mWidth(other.mWidth), mHeight(other.mHeight), mName(other.mName),
    mFeatureLen(other.mFeatureLen), mDirpath(other.mDirpath), mJpgFilename(other.mJpgFilename)
{
    mpFeature = NULL;
    mpFeature = (unsigned char *)malloc(sizeof(unsigned char) * mFeatureLen);
    if(mpFeature != NULL)
        memcpy(mpFeature, other.mpFeature, mFeatureLen);
}

BaseFaceInfo::~BaseFaceInfo()
{
    if(mpFeature)
    {
        free(mpFeature);
        mpFeature = NULL;
    }
}

BaseFaceInfo & BaseFaceInfo::operator=(const BaseFaceInfo & other)
{
    if(this == &other)
        return *this;

    mWidth = other.mWidth;
    mHeight = other.mHeight;
    mName = other.mName;
    mFeatureLen = other.mFeatureLen;
    mDirpath = other.mDirpath;
    mJpgFilename = other.mJpgFilename;
    if(mpFeature)
    {
        free(mpFeature);
        mpFeature = NULL;
    }
    mpFeature = (unsigned char * )malloc(sizeof(unsigned char) * mFeatureLen);
    if(mpFeature)
    {
        memcpy(mpFeature, other.mpFeature, mFeatureLen);
    }
    return *this;
}

int BaseFaceInfo::getFeatureLen()
{
    return mFeatureLen;
}

unsigned char * BaseFaceInfo::getFeatureValue()
{
    return mpFeature;
}

string BaseFaceInfo::getName()
{
    return mName;
}

/*
    Check @mDirpath and @mJpgFilename is valid or not, include : exist or not, name in right format or not, and so on;
    convert to yuv, do face detect, check detect result;
    get features from this face result;
*/
int BaseFaceInfo::getFeatures(FaceOper * pFaceOper)
{
    if(NULL == pFaceOper)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }
    //check dirpath exist or not
    if(access(mDirpath.c_str(), 0) != 0)
    {
        dbgError("dirpath = [%s], donot exist!\n", mDirpath.c_str());
        return -2;
    }
    //check filename format
    char tmp[MAX_FILENAME_LEN] = {0x00};
    int ret = sscanf(mJpgFilename.c_str(), BASE_FILE_FORMAT, &mWidth, &mHeight, tmp);
    if(ret != BASE_FILE_PARAM_NUM)
    {
        dbgError("mJpgFilename=[%s], donot in right base face file format!\n", mJpgFilename.c_str());
        return -3;
    }
    //tmp is include suffix like "a.jpg", we should convert it to "a"
    if(strlen(tmp) <= strlen(JPG_SUFFIX) || 0 != strcmp(tmp + (strlen(tmp) - strlen(JPG_SUFFIX)), JPG_SUFFIX))
    {
        dbgError("tmp=[%s], donot in right jpg file name format, it should ends with [%s] as suffix.\n",
            tmp, JPG_SUFFIX);
        return -3;
    }
    tmp[strlen(tmp) - strlen(JPG_SUFFIX)] = 0x00;
    dbgDebug("dirpath=[%s], jpgFilename=[%s], get params : width=%d, height=%d, name=[%s]\n",
        mDirpath.c_str(), mJpgFilename.c_str(), mWidth, mHeight, tmp);
    mName = tmp;
    //get tmp yuv file by Jpg2Yuv
    string jpgFilepath = mDirpath + "/" + mJpgFilename;
    char tmpYuvFilename[MAX_FILENAME_LEN] = {0x00};
    snprintf(tmpYuvFilename, MAX_FILENAME_LEN, BASE_TMPYUV_FILENAME_FORMAT, mWidth, mHeight, mName.c_str());
    tmpYuvFilename[MAX_FILENAME_LEN - 1] = 0x00;
    string tmpYuvFilepath = mDirpath + "/";
    tmpYuvFilepath += tmpYuvFilename;
    Jpg2Yuv jpg2yuv;
    ret = jpg2yuv.convert(jpgFilepath, tmpYuvFilepath);
    if(ret != 0)
    {
        dbgError("convert jpg to yuv failed! ret=%d, jpgFilepath=[%s]\n", ret, jpgFilepath.c_str());
        return -4;
    }
    dbgDebug("convert jpg to yuv succeed. jpgFilepath=[%s], yuvFileapth=[%s]\n",
        jpgFilepath.c_str(), tmpYuvFilepath.c_str());
    //get face result by ARC-FACE-DETECT
    ret = pFaceOper->setYuvFile(tmpYuvFilepath, mWidth, mHeight);
    if(ret != 0)
    {
        dbgError("set yuv file params to FaceOper failed! ret=%d, yuvFilepath=[%s], width=%d, height=%d\n",
            ret, tmpYuvFilepath.c_str(), mWidth, mHeight);
        unlink(tmpYuvFilepath.c_str());
        return -5;
    }
    dbgDebug("set yuv file params to FaceOper succeed.\n");
    LPAFD_FSDK_FACERES faceResult;
    ret = pFaceOper->getFaceResult(faceResult);
    if(ret != 0)
    {
        dbgError("getFaceResult failed! ret=%d\n", ret);
        pFaceOper->clearYuvFile();
        unlink(tmpYuvFilepath.c_str());
        return -6;
    }
    dbgDebug("getFaceResult succeed.\n");
    //check just one face in or not
    if(faceResult->nFace != 1)
    {
        dbgError("In file [%s], we find %d faces, to be a base face picture, must have one face only!\n",
            tmpYuvFilepath.c_str(), faceResult->nFace);
        pFaceOper->clearYuvFile();
        unlink(tmpYuvFilepath.c_str());
        return -7;
    }
    //get this face features
    if(mpFeature)
    {
        dbgInfo("mpFeature donot NULL, should reMalloc it for this jpg file.\n");
        free(mpFeature);
        mpFeature = NULL;
    }
    AFR_FSDK_FACEINPUT faceInput;
    faceInput.rcFace.left = faceResult->rcFace[0].left;
    faceInput.rcFace.right = faceResult->rcFace[0].right;
    faceInput.rcFace.top = faceResult->rcFace[0].top;
    faceInput.rcFace.bottom = faceResult->rcFace[0].bottom;
    faceInput.lOrient = faceResult->lfaceOrient[0];
    ret = pFaceOper->getFaceFeature(faceInput, mFeatureLen, mpFeature);
    if(ret != 0)
    {
        dbgError("getFaceFeature failed! ret=%d\n", ret);
        pFaceOper->clearYuvFile();
        unlink(tmpYuvFilepath.c_str());
        return -8;
    }
    dbgDebug("getFaceFeatures succeed.\n");
    pFaceOper->clearYuvFile();
    unlink(tmpYuvFilepath.c_str());
    return 0;
}


FaceInfo::FaceInfo() : mLeftPos(0), mRightPos(0), mTopPos(0), mBottomPos(0), mAngle(0),
    mGender(0), mAge(0), mName(UNKNOWN_PEOPLE_NAME)
{
    ;
}

FaceInfo::FaceInfo(int leftPos, int rightPos, int topPos, int bottomPos, int angle) :
    mLeftPos(leftPos), mRightPos(rightPos), mTopPos(topPos), mBottomPos(bottomPos), mAngle(angle),
    mGender(0), mAge(0), mName(UNKNOWN_PEOPLE_NAME)
{
    ;
}

FaceInfo::FaceInfo(const FaceInfo & other) :
    mLeftPos(other.mLeftPos), mRightPos(other.mRightPos), 
    mTopPos(other.mTopPos), mBottomPos(other.mBottomPos), 
    mAngle(mAngle),
    mGender(other.mGender), mAge(other.mAge), mName(other.mName)
{
    ;
}

FaceInfo::~FaceInfo()
{
    ;
}

FaceInfo & FaceInfo::operator = (const FaceInfo & other)
{
    if(this == &other)
        return *this;

    mLeftPos = other.mLeftPos;
    mRightPos = other.mRightPos;
    mTopPos = other.mTopPos;
    mBottomPos = other.mBottomPos;
    mAngle = other.mAngle;

    mGender = other.mGender;
    mAge = other.mAge;
    mName = other.mName;
    return *this;
}

int FaceInfo::getLeftPos()
{
    return mLeftPos;
}
int FaceInfo::getRightPos()
{
    return mRightPos;
}
int FaceInfo::getTopPos()
{
    return mTopPos;
}
int FaceInfo::getBottomPos()
{
    return mBottomPos;
}
int FaceInfo::getAngle()
{
    return mAngle;
}
char FaceInfo::getGender()
{
    return mGender;
}
int FaceInfo::getAge()
{
    return mAge;
}
string FaceInfo::getName()
{
    return mName;
}

void FaceInfo::getAll(int & leftPos, int & rightPos, int & topPos, int & bottomPos, int & angle,
    char & gender, int & age, string & name)
{
    leftPos = mLeftPos;
    rightPos = mRightPos;
    topPos = mTopPos;
    bottomPos = mBottomPos;
    angle = mAngle;

    gender = mGender;
    age = mAge;
    name = mName;
}

void FaceInfo::setLeftPos(const int pos)
{
    mLeftPos = pos;
}
void FaceInfo::setRightPos(const int pos)
{
    mRightPos = pos;
}
void FaceInfo::setTopPos(const int pos)
{
    mTopPos = pos;
}
void FaceInfo::setBottomPos(const int pos)
{
    mBottomPos = pos;
}
void FaceInfo::setAngle(const int angle)
{
    mAngle= angle;
}
void FaceInfo::setGender(const char gender)
{
    mGender = gender;
}
void FaceInfo::setAge(const int age)
{
    mAge = age;
}
void FaceInfo::setName(const string & name)
{
    mName = name;
}

void FaceInfo::setAll(int leftPos, int rightPos, int topPos, int bottomPos, int angle,
    char gender, int age, string & name)
{
    mLeftPos = leftPos;
    mRightPos = rightPos;
    mTopPos = topPos;
    mBottomPos = bottomPos;
    mAngle = angle;

    mGender = gender;
    mAge = age;
    mName = name;
}

FaceOper::FaceOper(): 
    mCurYuvFilepath(""), mIsRegisterd(false),
    mpFaceDetectHdl(NULL), mpFaceAgeHdl(NULL),
    mpFaceGenderHdl(NULL), mpFaceRecoHdl(NULL),
    mpFaceDetectMem(NULL), mpFaceAgeMem(NULL),
    mpFaceGenderMem(NULL), mpFaceRecoMem(NULL),
    mpCurYuvData(NULL), mCurYuvSize(0),
    mCurWidth(0), mCurHeight(0)
{
    ;
}


FaceOper::FaceOper(string & yuvFilepath): 
    mCurYuvFilepath(yuvFilepath), mIsRegisterd(false),
    mpFaceDetectHdl(NULL), mpFaceAgeHdl(NULL),
    mpFaceGenderHdl(NULL), mpFaceRecoHdl(NULL),
    mpFaceDetectMem(NULL), mpFaceAgeMem(NULL),
    mpFaceGenderMem(NULL), mpFaceRecoMem(NULL),
    mpCurYuvData(NULL), mCurYuvSize(0),
    mCurWidth(0), mCurHeight(0)
{
    ;
}

FaceOper::~FaceOper()
{
    unRegister2Arc();

    if(mpCurYuvData)
    {
        free(mpCurYuvData);
        mpCurYuvData = NULL;
    }
}

int FaceOper::register2Arc()
{
    if(mpFaceDetectHdl != NULL || mpFaceAgeHdl != NULL || mpFaceGenderHdl != NULL || 
        mpFaceRecoHdl != NULL)
    {
        dbgError("All handles of face oper must be NULL!\n");
        return -1;
    }

    int ret = mallocWorkMem();
    if(ret != 0)
    {
        dbgError("get memory for ARC working failed! ret=%d\n", ret);
        return -2;
    }
    dbgDebug("get memory for ARC working succeed.\n");

    //init to each handle
    ret = initAllHandles();
    if(ret != 0)
    {
        dbgError("initAllHandles failed! ret=%d\n", ret);
        freeWorkMem();
        return -3;
    }
    dbgError("initAllHandles succeed.\n");

    mIsRegisterd = true;

    return 0;
}

int FaceOper::mallocWorkMem()
{
    if(mpFaceDetectMem != NULL || mpFaceAgeMem != NULL || mpFaceGenderMem != NULL || 
        mpFaceRecoMem != NULL)
    {
        dbgError("All work memories of face oper must be NULL!\n");
        return -1;
    }
    
    mpFaceDetectMem = (unsigned char *)malloc(sizeof(unsigned char) * ARCSOFT_FACE_DETECTION_WORKMEM_SIZE);
    if(mpFaceDetectMem == NULL)
    {
        dbgError("malloc for mpFaceDetectMem failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        return -2;
    }
    mpFaceAgeMem = (unsigned char *)malloc(sizeof(unsigned char) * ARCSOFT_FACE_AGE_WORKMEM_SIZE);
    if(mpFaceAgeMem == NULL)
    {
        dbgError("malloc for mpFaceAgeMem failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        free(mpFaceDetectMem);
        mpFaceDetectMem = NULL;
        return -3;
    }
    mpFaceGenderMem = (unsigned char *)malloc(sizeof(unsigned char) * ARCSOFT_FACE_GENDER_WORKMEM_SIZE);
    if(mpFaceGenderMem == NULL)
    {
        dbgError("malloc for mpFaceGenderMem failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        free(mpFaceDetectMem);
        mpFaceDetectMem = NULL;
        free(mpFaceAgeMem);
        mpFaceAgeMem = NULL;
        return -4;
    }
    mpFaceRecoMem = (unsigned char *)malloc(sizeof(unsigned char) * ARCSOFT_FACE_RECO_WORKMEM_SIZE);
    if(mpFaceRecoMem == NULL)
    {
        dbgError("malloc for mpFaceRecoMem failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        free(mpFaceDetectMem);
        mpFaceDetectMem = NULL;
        free(mpFaceAgeMem);
        mpFaceAgeMem = NULL;
        free(mpFaceGenderMem);
        mpFaceGenderMem = NULL;
        return -5;
    }
    dbgDebug("Malloc memory for ARC working succeed.\n");
    return 0;
}

int FaceOper::freeWorkMem()
{
    if(mpFaceDetectMem)
    {
        free(mpFaceDetectMem);
        mpFaceDetectMem = NULL;
    }
    if(mpFaceAgeMem)
    {
        free(mpFaceAgeMem);
        mpFaceAgeMem = NULL;
    }
    if(mpFaceGenderMem)
    {    
        free(mpFaceGenderMem);
        mpFaceGenderMem = NULL;
    }
    if(mpFaceRecoMem)
    {
        free(mpFaceRecoMem);
        mpFaceRecoMem = NULL;
    }
    return 0;
}

int FaceOper::initAllHandles()
{
    //Face Detection
    int ret = AFD_FSDK_InitialFaceEngine(
        ARCSOFT_APPID, ARCSOFT_FACE_DETECTION_SDKKEY, mpFaceDetectMem,
        ARCSOFT_FACE_DETECTION_WORKMEM_SIZE, &mpFaceDetectHdl, 
        AFD_FSDK_OPF_0_HIGHER_EXT, 16, MAX_FACE_NUM);
    if(ret != 0)
    {
        dbgError("AFD_FSDK_InitialFaceEngine failed, ret=%d\n", ret);
        return -1;
    }
    const AFD_FSDK_Version * pFdVersionInfo = AFD_FSDK_GetVersion(mpFaceDetectHdl);
    dbgDebug("ARC-FaceDetect version : \n\tVersion=[%s], BuildDate=[%s], CopyRight=[%s]\n\t" \
        "CodeBase=%d, major=%d, minor=%d, build=%d\nArc soft version dump over.\n",
        pFdVersionInfo->Version, pFdVersionInfo->BuildDate, pFdVersionInfo->CopyRight,
        pFdVersionInfo->lCodebase, pFdVersionInfo->lMajor, pFdVersionInfo->lMinor, 
        pFdVersionInfo->lBuild);

    ret = ASAE_FSDK_InitAgeEngine(ARCSOFT_APPID, ARCSOFT_AGE_SDKKEY, mpFaceAgeMem, 
        ARCSOFT_FACE_AGE_WORKMEM_SIZE, &mpFaceAgeHdl);
    if (ret != 0) {
        dbgError("ASAE_FSDK_InitAgeEngine failed: 0x%x\r\n", ret);
        return -2;
    }
    const ASAE_FSDK_Version * pAgeVersionInfo = ASAE_FSDK_GetVersion(mpFaceAgeHdl);
    dbgDebug("ARC-Age version : \n\tVersion=[%s], BuildDate=[%s], CopyRight=[%s]\n\t" \
        "CodeBase=%d, major=%d, minor=%d, build=%d\nArc soft version dump over.\n",
        pAgeVersionInfo->Version, pAgeVersionInfo->BuildDate, pAgeVersionInfo->CopyRight,
        pAgeVersionInfo->lCodebase, pAgeVersionInfo->lMajor, pAgeVersionInfo->lMinor, 
        pAgeVersionInfo->lBuild);

    ret = ASGE_FSDK_InitGenderEngine(ARCSOFT_APPID, ARCSOFT_GENDER_SDKKEY, mpFaceGenderMem, 
        ARCSOFT_FACE_GENDER_WORKMEM_SIZE, &mpFaceGenderHdl);
    if (ret != 0) {
        dbgError("ASGE_FSDK_InitGenderEngine failed! ret = 0x%x\n", ret);
        return -3;
    }
    const ASGE_FSDK_Version * pGenderVersionInfo = ASGE_FSDK_GetVersion(mpFaceGenderHdl);
    dbgDebug("ARC-Gender version : \n\tVersion=[%s], BuildDate=[%s], CopyRight=[%s]\n\t" \
        "CodeBase=%d, major=%d, minor=%d, build=%d\nArc soft version dump over.\n",
        pGenderVersionInfo->Version, pGenderVersionInfo->BuildDate, pGenderVersionInfo->CopyRight,
        pGenderVersionInfo->lCodebase, pGenderVersionInfo->lMajor, pGenderVersionInfo->lMinor, 
        pGenderVersionInfo->lBuild);

    ret = AFR_FSDK_InitialEngine(ARCSOFT_APPID, ARCSOFT_FACE_RECO_SDKKEY, mpFaceRecoMem, 
        ARCSOFT_FACE_RECO_WORKMEM_SIZE, &mpFaceRecoHdl);
    if(ret != 0)
    {
        dbgError("AFR_FSDK_InitialEngine failed! ret=0x%x\n", ret);
        return -4;
    }
    const AFR_FSDK_Version * pFrVersionInfo = AFR_FSDK_GetVersion(mpFaceRecoHdl);
    dbgDebug("ARC-Gender version : \n\tVersion=[%s], BuildDate=[%s], CopyRight=[%s]\n\t" \
        "CodeBase=%d, major=%d, minor=%d, build=%d\nArc soft version dump over.\n",
        pFrVersionInfo->Version, pFrVersionInfo->BuildDate, pFrVersionInfo->CopyRight,
        pFrVersionInfo->lCodebase, pFrVersionInfo->lMajor, pFrVersionInfo->lMinor, 
        pFrVersionInfo->lBuild);

    dbgDebug("all handles for ARC SOFT being inited! canbe used now.\n");
    return 0;
}

void FaceOper::unRegister2Arc()
{
    if(mpFaceDetectHdl != NULL)
    {
        AFD_FSDK_UninitialFaceEngine(mpFaceDetectHdl);
        mpFaceDetectHdl = NULL;
    }
    if(mpFaceAgeHdl != NULL)
    {
        ASAE_FSDK_UninitAgeEngine(mpFaceAgeHdl);
        mpFaceAgeHdl = NULL;
    }
    if(mpFaceGenderHdl != NULL)
    {
        ASGE_FSDK_UninitGenderEngine(mpFaceGenderHdl);
        mpFaceGenderHdl = NULL;
    }
    if(mpFaceRecoHdl != NULL)
    {
        AFR_FSDK_UninitialEngine(mpFaceRecoHdl);
        mpFaceRecoHdl = NULL;
    }
    

    if(mpFaceDetectMem != NULL)
    {
        free(mpFaceDetectMem);
        mpFaceDetectMem = NULL;
    }
    if(mpFaceAgeMem != NULL)
    {
        free(mpFaceAgeMem);
        mpFaceAgeMem = NULL;
    }
    if(mpFaceGenderMem!= NULL)
    {
        free(mpFaceGenderMem);
        mpFaceGenderMem = NULL;
    }
    if(mpFaceRecoMem!= NULL)
    {
        free(mpFaceRecoMem);
        mpFaceRecoMem = NULL;
    }

    mIsRegisterd = false;
}

int FaceOper::setYuvFile(const string & yuvFilepath, const int width, const int height)
{
    mCurYuvFilepath = yuvFilepath;
    mCurHeight = height;
    mCurWidth = width;

    return readYuvFileData();
}

int FaceOper::clearYuvFile()
{
    free(mpCurYuvData);
    mpCurYuvData = NULL;
    mCurYuvSize = 0;
    mCurYuvFilepath = "";
    mCurWidth = 0;
    mCurHeight = 0;
    return 0;
}

int FaceOper::readYuvFileData()
{
    if(mpCurYuvData)
    {
        free(mpCurYuvData);
        mpCurYuvData = NULL;
    }

    struct stat curStat;
    int ret = stat(mCurYuvFilepath.c_str(), &curStat);
    if(ret != 0)
    {
        dbgError("stat failed! ret=%d, filepath=[%s], errno=%d, desc=[%s]\n",
            ret, mCurYuvFilepath.c_str(), errno, strerror(errno));
        return -1;
    }
    mCurYuvSize = curStat.st_size;

    mpCurYuvData = (unsigned char *)malloc(sizeof(unsigned char ) * mCurYuvSize);
    if(mpCurYuvData == NULL)
    {
        dbgError("malloc for mpCurYuvData failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        return -2;
    }

    FILE * fp = NULL;
    fp = fopen(mCurYuvFilepath.c_str(), "rb");
    if(fp == NULL)
    {
        dbgError("open file [%s] for read failed. errno=%d, desc=[%s]\n",
            mCurYuvFilepath.c_str(), errno, strerror(errno));
        free(mpCurYuvData);
        mpCurYuvData = NULL;
        return -3;
    }

    size_t readLen = fread(mpCurYuvData, 1, mCurYuvSize, fp);
    if(readLen != mCurYuvSize)
    {
        dbgError("read file to data failed! readLen=%d, mCurYuvsize=%d\n", readLen, mCurYuvSize);
        fclose(fp);
        fp = NULL;
        free(mpCurYuvData);
        mpCurYuvData = NULL;
        return -4;
    }
    dbgDebug("read yuv file data to memory succeed.\n");
    fclose(fp);
    fp = NULL;

    return 0;
}

int FaceOper::setArcImgInfo(ASVLOFFSCREEN * pImgInfo)
{
    if(NULL == pImgInfo)
    {
        dbgError("Input param is NULL.\n");
        return -1;
    }

    if(NULL == mpCurYuvData)
    {
        dbgError("mpCurYuvData == NULL, cannot set to img info!\n");
        return -2;
    }
    
    memset(pImgInfo, 0x00, sizeof(ASVLOFFSCREEN));
    pImgInfo->u32PixelArrayFormat = ASVL_PAF_I420;
    pImgInfo->i32Width = mCurWidth;
    pImgInfo->i32Height = mCurHeight;
    pImgInfo->ppu8Plane[0] = mpCurYuvData;
    pImgInfo->pi32Pitch[0] = pImgInfo->i32Width;
    pImgInfo->pi32Pitch[1] = pImgInfo->i32Width/2;
    pImgInfo->pi32Pitch[2] = pImgInfo->i32Width/2;
    pImgInfo->ppu8Plane[1] = pImgInfo->ppu8Plane[0] + pImgInfo->pi32Pitch[0] * pImgInfo->i32Height;
    pImgInfo->ppu8Plane[2] = pImgInfo->ppu8Plane[1] + pImgInfo->pi32Pitch[1] * pImgInfo->i32Height/2;    
    return 0;
}

int FaceOper::getFaceResult(LPAFD_FSDK_FACERES & faceResult)
{
    if(!mIsRegisterd)
    {
        dbgError("Donot registered to ARC yet, cannot get face info!\n");
        return -1;
    }

    ASVLOFFSCREEN imgInfo;
    int ret = setArcImgInfo(&imgInfo);
    if (ret != 0) 
    {
        dbgError("setArcImgInfo failed, ret = %d\n", ret);
        return -2;
    }
    dbgDebug("setArcImgInfo for ARC-FACE-DETECT succeed.\n");

    ret = AFD_FSDK_StillImageFaceDetection(mpFaceDetectHdl, &imgInfo, &faceResult);
    if (ret != 0) 
    {
        dbgError("AFD_FSDK_StillImageFaceDetection failed, ret = 0x%x\n", ret);
        return -3;
    }
    dbgDebug("AFD_FSDK_StillImageFaceDetection succeed.\n");
    return 0;    
}

int FaceOper::getFaceFeature(AFR_FSDK_FACEINPUT & faceInput, int &featureLen, unsigned char *& pFeatures)
{
    if(pFeatures)
    {
        dbgError("pFeatures is not NULL, cannot recv new features value.\n");
        return -1;
    }
    
    ASVLOFFSCREEN imgInfo;
    int ret = setArcImgInfo(&imgInfo);
    if (ret != 0) 
    {
        dbgError("setArcImgInfo failed, ret = %d\n", ret);
        return -2;
    }
    dbgDebug("setArcImgInfo for ARC-FACE-DETECT succeed.\n");

    AFR_FSDK_FACEMODEL localFaceModel;
    ret = AFR_FSDK_ExtractFRFeature(mpFaceRecoHdl, &imgInfo, &faceInput, &localFaceModel);
    if(ret != 0)
    {
        dbgError("AFR_FSDK_ExtractFRFeature failed! ret=0x%x\n", ret);
        return -3;
    }
    dbgDebug("AFR_FSDK_ExtractFRFeature succeed.\n");

    //should malloc a new memory block, then set values to it
    featureLen = localFaceModel.lFeatureSize;
    pFeatures = (unsigned char *)malloc(sizeof(unsigned char) * featureLen);
    if(pFeatures == NULL)
    {
        dbgError("malloc for features failed! errno=%d, desc=[%s], length=%d\n",
            errno, strerror(errno), featureLen);
        return -4;
    }
    memcpy(pFeatures, localFaceModel.pbFeature, featureLen);
    dbgDebug("copy features from local to malloc memory succeed.\n");
    return 0;
}

/*
    get all faces positions;
    get their ages;
    get their gender;
    get their name;
    return 0 if succeed, 0- else;
*/
int FaceOper::getFaceInfo(int & faceNum, list < FaceInfo > & faceInfoList, list<BaseFaceInfo> & baseFaceinfoList)
{
    if(!mIsRegisterd)
    {
        dbgError("Donot registered to ARC yet, cannot get face info!\n");
        return -1;
    }

    ASVLOFFSCREEN imgInfo;
    int ret = setArcImgInfo(&imgInfo);
    if (ret != 0) 
    {
        dbgError("setArcImgInfo failed, ret = %d\n", ret);
        return -2;
    }
    dbgDebug("setArcImgInfo for ARC-FACE-DETECT succeed.\n");

    LPAFD_FSDK_FACERES faceResult;
    memset(&faceResult, 0x00, sizeof(LPAFD_FSDK_FACERES));
    ret = AFD_FSDK_StillImageFaceDetection(mpFaceDetectHdl, &imgInfo, &faceResult);
    if (ret != 0) 
    {
        dbgError("AFD_FSDK_StillImageFaceDetection failed, ret = 0x%x\n", ret);
        return -3;
    }
    if(faceResult->nFace == 0)
    {
        dbgWarn("Current yuv file [%s] donot detect face!\n", mCurYuvFilepath.c_str());
        return -4;
    }
    dbgDebug("AFD_FSDK_StillImageFaceDetection succeed, get %d faces in yuvFile [%s]\n",
        faceResult->nFace, mCurYuvFilepath.c_str());
    
    //get ages
    ASAE_FSDK_AGEFACEINPUT ageFaceInput;
    ageFaceInput.lFaceNumber = faceResult->nFace;
    ageFaceInput.pFaceRectArray = faceResult->rcFace;
    ageFaceInput.pFaceOrientArray = faceResult->lfaceOrient;
    
    ASAE_FSDK_AGERESULT ageResult;
    ret = ASAE_FSDK_AgeEstimation_StaticImage(mpFaceAgeHdl, &imgInfo, &ageFaceInput, &ageResult);
    if(ret != 0)
    {
        dbgError("ASAE_FSDK_AgeEstimation_StaticImage failed! ret=%d\n", ret);
        return -5;
    }
    dbgDebug("ASAE_FSDK_AgeEstimation_StaticImage succeed.\n");

    //get genders
    ASGE_FSDK_GENDERFACEINPUT genderFaceInput;
    genderFaceInput.lFaceNumber = faceResult->nFace;
    genderFaceInput.pFaceRectArray = faceResult->rcFace;
    genderFaceInput.pFaceOrientArray = faceResult->lfaceOrient;
    ASGE_FSDK_GENDERRESULT  genderResult;
    ret = ASGE_FSDK_GenderEstimation_StaticImage(mpFaceGenderHdl, &imgInfo, &genderFaceInput, &genderResult);   //0 male(man), 1 female(woman), -1 donot knows
    if(ret != 0)
    {
        dbgError("ASGE_FSDK_GenderEstimation_StaticImage failed! ret=%d\n", ret);
        return -5;
    }
    dbgDebug("ASGE_FSDK_GenderEstimation_StaticImage succeed.\n");

    //get name, base face pictures being needed now.
    vector<string> nameVector;
    string unknownName(UNKNOWN_PEOPLE_NAME);
    int i = 0;
    for(i = 0; i < faceResult->nFace; i++)
    {
        AFR_FSDK_FACEINPUT faceInput;
        faceInput.rcFace.left = faceResult->rcFace[i].left;
        faceInput.rcFace.right = faceResult->rcFace[i].right;
        faceInput.rcFace.top = faceResult->rcFace[i].top;
        faceInput.rcFace.bottom = faceResult->rcFace[i].bottom;
        faceInput.lOrient = faceResult->lfaceOrient[i];

        int featureLen = 0;
        unsigned char * pFeature = NULL;
        ret = getFaceFeature(faceInput, featureLen, pFeature);
        if(ret != 0)
        {
            dbgWarn("getFaceFeatures failed! ret=%d, yuvFilepath=[%s], pos:[%d, %d, %d, %d], angle=%d\n",
                ret, mCurYuvFilepath.c_str(), faceResult->rcFace[i].left, faceResult->rcFace[i].right,
                faceResult->rcFace[i].top, faceResult->rcFace[i].bottom, faceResult->lfaceOrient[i]);
            nameVector.push_back(unknownName);
        }
        else
        {
            AFR_FSDK_FACEMODEL curFaceModel;
            curFaceModel.lFeatureSize = featureLen;
            curFaceModel.pbFeature = pFeature;
            float simi, max = 0.0;
            string name = unknownName;
            for(list<BaseFaceInfo>::iterator it = baseFaceinfoList.begin(); 
                it != baseFaceinfoList.end(); it++)
            {
                AFR_FSDK_FACEMODEL baseFaceModel;
                baseFaceModel.lFeatureSize = it->getFeatureLen();
                baseFaceModel.pbFeature = it->getFeatureValue();
                ret = AFR_FSDK_FacePairMatching(mpFaceRecoHdl, &curFaceModel, &baseFaceModel, &simi);
                if(ret != 0)
                {
                    dbgWarn("AFR_FSDK_FacePairMatching failed! ret=0x%x\n", ret);
                    free(pFeature);
                    pFeature = NULL;
                    continue;
                }
                dbgDebug("Cmp to face(name=[%s]) with simi=%.6f, current max=%.6f\n",
                    it->getName().c_str(), simi, max);
                if(simi > max)
                {
                    max = simi;
                    name = it->getName();
                }
            }
            dbgInfo("Face rec over, maxSimi=%.6f, we get its name=[%s]\n", max, name.c_str());

            if(max < SIMI_LIMIT_VALUE)
            {
                dbgInfo("maxSimi=%.6f, less than SIMI_LIMIT_VALUE(%.6f), we donot think its a face we know.\n",
                    max, SIMI_LIMIT_VALUE);
                name = unknownName;
            }
            
            nameVector.push_back(name);

            free(pFeature);
            pFeature = NULL;
        }
    }
    
    faceInfoList.clear();
    for(i = 0; i < faceResult->nFace; i++)
    {
        FaceInfo curFaceInfo(faceResult->rcFace[i].left, faceResult->rcFace[i].right,
            faceResult->rcFace[i].top, faceResult->rcFace[i].bottom, faceResult->lfaceOrient[i]);
        curFaceInfo.setAge(ageResult.pAgeResultArray[i]);
        curFaceInfo.setGender(genderResult.pGenderResultArray[i]);
        curFaceInfo.setName(nameVector[i]);
        
        faceInfoList.push_back(curFaceInfo);
    }
    faceNum = faceResult->nFace;
    
    return 0;
}

