#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#include "client.h"

#define NOBLOCK 1

Client::Client() : mIp("127.0.0.1"), mMode(SOCKET_MODE_TCP), mSocketId(-1), mServerSockId(-1)
{
    initModeDescMap();
}

Client::Client(const Client & other) :
    mIp(other.mIp), mMode(other.mMode), mSocketId(other.mSocketId), mServerSockId(other.mServerSockId)
{
    initModeDescMap();
}

Client::Client(const string & ip, const SOCKET_MODE mode) : mIp(ip), mMode(mode), mSocketId(-1), mServerSockId(-1)
{
    initModeDescMap();
}

Client::~Client()
{
    if(mSocketId >= 0)
    {
        close(mSocketId);
        mSocketId = -1;
    }
}

Client & Client::operator = (const Client &other)
{
    if(this == &other)
    {
        return *this;
    }

    mIp = other.mIp;
    mMode = other.mMode;
    mSocketId = other.mSocketId;
    mServerSockId = other.mServerSockId;
    
    return *this;
}

void Client::dump()
{
    printf("In select client, ip = %s, mode = [%s]\n", mIp.c_str(), mModeDescMap[mMode].c_str());
}

void Client::initModeDescMap()
{
    mModeDescMap.insert(map<int, string>::value_type(SOCKET_MODE_TCP, "TCP"));
    mModeDescMap.insert(map<int, string>::value_type(SOCKET_MODE_UDP, "UDP"));
}

int Client::initTcpSocket()
{
    if(mSocketId > 0)
    {
        close(mSocketId);
        mSocketId = -1;
    }

    //Create socket
    mSocketId = socket(AF_INET, SOCK_STREAM, 0);
    if(mSocketId < 0)
    {
        fprintf(stderr, "create tcp socket failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        return -1;
    }

    //Set this socket can be reused
    int on = 1;
    int ret = setsockopt(mSocketId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret != 0)
    {
        fprintf(stderr, "setsockopt failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        close(mSocketId);
        mSocketId = -1;
        return -2;
    }
#if NOBLOCK
    //Set this socket to nonblock
    int flags = fcntl(mSocketId, F_GETFL, 0);
    if(flags < 0)
    {
        fprintf(stderr, "fcntl failed when get flags! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        close(mSocketId);
        mSocketId = -1;
        return -3;
    }
    ret = fcntl(mSocketId, F_SETFL, flags | O_NONBLOCK);
    if(ret < 0)
    {
        fprintf(stderr, "fcntl failed when set flags! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        close(mSocketId);
        mSocketId = -1;
        return -4;
    }

    //do connect in noblock mode
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0x00, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8082);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ret = connect(mSocketId, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr));
    if(0 == ret)
    {
        printf("Connect succeed once!\n");
    }
    else
    {
        if(errno == EINPROGRESS)
        {
            fprintf(stderr, "Socket being blocked yet, we have no ret.\n");
            //Do select here.
            fd_set wFdSet;
            FD_ZERO(&wFdSet);
            FD_SET(mSocketId, &wFdSet);
            fd_set rFdSet;
            FD_ZERO(&rFdSet);
            FD_SET(mSocketId, &rFdSet);
            struct timeval timeout;
            timeout.tv_sec = 3;
            timeout.tv_usec = 0;
            ret = select(mSocketId + 1, &rFdSet, &wFdSet, NULL, &timeout);
            if(ret < 0)
            {
                fprintf(stderr, "select failed! ret = %d, errno = %d, desc = [%s]\n", 
                    ret, errno, strerror(errno));
                close(mSocketId);
                mSocketId = -1;
                return -5;
            }
            else if(ret == 0)
            {
                fprintf(stderr, "select timeout!\n");
                close(mSocketId);
                mSocketId = -1;
                return -6;
            }
            else
            {
                if(FD_ISSET(mSocketId, &wFdSet))
                {
                    if(FD_ISSET(mSocketId, &rFdSet))
                    {
                        printf("select ok, but wFdSet and rFdSet all being set, we should check!\n");
                        //check this socket is OK or not.
                        int err = 0;
                        socklen_t errLen;
                        ret = getsockopt(mSocketId, SOL_SOCKET, SO_ERROR, (void *)&err, &errLen);
                        if(0 != ret)
                        {
                            fprintf(stderr, "getsockopt failed! ret = %d, errno = %d, desc = [%s]\n", 
                                ret, errno, strerror(errno));
                            close(mSocketId);
                            mSocketId = -1;
                            return -7;
                        }
                        else
                        {
                            if(err != 0)
                            {
                                fprintf(stderr, "getsockopt ok, but err = %d, donot mean OK, just mean some error!\n",
                                    err);
                                close(mSocketId);
                                mSocketId = -1;
                                return -8;
                            }
                            else
                            {
                                printf("getsockopt ok, and err = %d, means connect succeed!\n", err);
                            }
                        }
                    }
                    else
                    {
                        printf("select ok, and just wFdSet being set, means connect being OK.\n");
                    }
                }
                else
                {
                    //Just this socket being set to wFdSet, if not this socket, error ocurred!
                    fprintf(stderr, "select ok, but the fd is not client socket!\n");
                    close(mSocketId);
                    mSocketId = -1;
                    return -7;
                }
            }
        }
        else
        {
            fprintf(stderr, "Connect failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            close(mSocketId);
            mSocketId = -1;
            return -5;
        }
    }
    printf("Connect succeed at the end!\n");

    //set to block mode
    ret = fcntl(mSocketId, F_SETFL, flags);
    if(ret < 0)
    {
        fprintf(stderr, "fcntl failed when set flags! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        close(mSocketId);
        mSocketId = -1;
        return -6;
    }
    printf("socket has been set to block mode!\n");
    
#else
    //do connect in block mode directly;
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0x00, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8082);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ret = connect(mSocketId, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
        fprintf(stderr, "Connect to server failed! ret = %d, errno = %d, desc = [%s]\n", 
            ret, errno, strerror(errno));
        close(mSocketId);
        mSocketId = -1;
        return -5;
    }
    printf("Client being connect to server now!\n");

#endif

    return 0;
}


int Client::initUdpSocket()
{
    if(mSocketId >= 0)
    {
        close(mSocketId);
        mSocketId = -1;
    }

    //Create socket
    mSocketId = socket(AF_INET, SOCK_DGRAM, 0);
    if(mSocketId < 0)
    {
        fprintf(stderr, "create socket for client failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return -1;
    }

    //set this socket to be reused
    int on = 1;
    int ret = setsockopt(mSocketId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret < 0)
    {
        fprintf(stderr, "setsockopt to client failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(mSocketId);
        mSocketId = -1;
        return -2;
    }

    return 0;
}


int Client::initSocket()
{
    int ret = 0;
    if(mSocketId < 0)
    {
        switch(mMode)
        {
            case SOCKET_MODE_TCP:
                ret = initTcpSocket();
                break;
            case SOCKET_MODE_UDP:
                ret = initUdpSocket();
                break;
            default:
                ret = -1;
                fprintf(stderr, "mMode = %d, invalid value! cannot init socket!\n");
                break;
        }
    }

    return ret;
}

int Client::sendMsg(const string & msg)
{
    if(mSocketId < 0)
    {
        fprintf(stderr, "socket donot valid! check for it!\n");
        return -1;
    }

    int ret = 0;
    switch(mMode)
    {
        case SOCKET_MODE_TCP:
            ret = send(mSocketId, msg.c_str(), msg.length(), 0);
            if(ret < 0)
            {
                if(errno = EPIPE)
                {
                    fprintf(stderr, "Server donot running yet! cannot send msg to it!\n");
                    ret = -3;
                }
                else
                {
                    fprintf(stderr, "send failed! ret = %d, errno = %d, desc = [%s]\n",
                        ret, errno, strerror(errno));
                    ret = -4;
                }
            }
            else
            {
                printf("send [%s] to server succeed, ret = %d.\n", msg.c_str(), ret);
                ret = 0;
            }
            break;
        case SOCKET_MODE_UDP:
            struct sockaddr_in serverAddr;
            memset(&serverAddr, 0x00, sizeof(struct sockaddr_in));
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(8082);
            serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            ret = sendto(mSocketId, msg.c_str(), msg.length(), 0, 
                (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
            if(ret < 0)
            {
                if(errno = EPIPE)
                {
                    fprintf(stderr, "Server donot running yet! cannot sendto msg!");
                    ret = -3;
                }
                else
                {
                    fprintf(stderr, "sendto failed! ret = %d, errno = %d, desc = [%s]\n",
                        ret, errno, strerror(errno));
                    ret = -4;
                }
            }
            else
            {
                printf("sendto msg [%s] succeed!\n", msg.c_str());
                ret = 0;
            }
            break;
        default:
            ret = -2;
            fprintf(stderr, "mMode = %d, invalid!\n");
            break;
    }
    return ret;
}

int Client::recvMsg(string & msg)
{
    if(mSocketId < 0)
    {
        fprintf(stderr, "mSocketId = %d, donot valid, cannot recv msg!\n", mSocketId);
        return -1;
    }

    int ret = 0;
    switch(mMode)
    {
        case SOCKET_MODE_TCP:
            {
                char msgRecved[512];
                memset(msgRecved, 0x00, 512);
                ret = recv(mSocketId, msgRecved, 512, 0);
                if(ret < 0)
                {
                    fprintf(stderr, "recv failed! ret = %d, errno = %d, desc = [%s]\n",
                        ret, errno, strerror(errno));
                    ret = -3;
                }
                else
                {
                    msgRecved[511] = 0x00;
                    printf("recv succeed! msg is [%s]\n", msgRecved);
                    msg = msgRecved;
                    ret = 0;
                }
            }
            break;
        case SOCKET_MODE_UDP:
            {
                struct sockaddr_in serverAddr;
                memset(&serverAddr, 0x00, sizeof(struct sockaddr_in));
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(8082);
                serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                char msgRecved[512];
                memset(msgRecved, 0x00, 512);

                socklen_t len = 0;
                ret = recvfrom(mSocketId, msgRecved, 512, 0, (struct sockaddr *)&serverAddr, &len);
                if(ret < 0)
                {
                    fprintf(stderr, "recvfrom failed! ret = %d, errno = %d, desc = [%s]\n",
                        ret, errno, strerror(errno));
                    ret = 03;
                }
                else
                {
                    msgRecved[511] = 0x00;
                    printf("recvfrom succeed! msg is [%s]\n", msgRecved);
                    msg = msgRecved;
                    ret = 0;
                }
            }
            break;
        default:
            break;
    }
    return ret;
}



