#ifndef __SERVER_H__
#define __SERVER_H__

#include "moThread.h"

#define DEF_HTTP_SERVER_PORT    8080
#define FACESERVER_CAPTURE_URL  "/FaceServer/captures"

class FaceServer : public MoThread
{
public:
    FaceServer();
    FaceServer(const int port = DEF_HTTP_SERVER_PORT);
    virtual ~FaceServer();

public:
    virtual int createServer();
    virtual void run();
    virtual void destroyServer();

private:
    int mPort;
};

#endif
