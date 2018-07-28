#ifndef __MO_THREAD_H__
#define __MO_THREAD_H__

#include <pthread.h>

using namespace std;

#include <string>

class MoThread
{
public:
    MoThread();
    MoThread(const string & thrName);
    virtual ~MoThread();

public:
    virtual pthread_t getThrId();
    virtual string & getThrName();
    virtual bool getRunState();

protected:
    static void * runProxy(void * args);
    virtual void run() = 0;

public:
    int start();
    //just stop thread running, must call @join to know thread being stopped succeed or not;
    int stop();
    int join();

private:
    pthread_t mThrId;
    string mThrName;
    bool mIsRun;
};

#endif