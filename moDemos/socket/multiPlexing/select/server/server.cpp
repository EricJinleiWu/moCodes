#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#include "server.h"

Server::Server() : mServIp("127.0.0.1"), mServPort(8082), mServSockId(-1),
    mMode(SOCKET_MODE_TCP)
{
    pthread_mutex_init(&mCliSockIdArrMutex, NULL);
    initModeDescMap();
    initCliSockIdArr();
}

Server::Server(const Server & other) : mServIp(other.mServIp), mServPort(other.mServPort),
    mServSockId(other.mServSockId), mMode(other.mMode)
{
    pthread_mutex_init(&mCliSockIdArrMutex, NULL);
    initModeDescMap();
    initCliSockIdArr();
}

Server::Server(const string & ip, const unsigned int port, const SOCKET_MODE mode) : mServIp(ip),
    mServPort(port), mMode(mode), mServSockId(-1)
{
    pthread_mutex_init(&mCliSockIdArrMutex, NULL);
    initModeDescMap();
    initCliSockIdArr();
}

Server::~Server()
{
    mModeDescMap.clear();

    if(mServSockId >= 0)
    {
        close(mServSockId);
        mServSockId = -1;
    }

    pthread_mutex_destroy(&mCliSockIdArrMutex);
}

void Server::initModeDescMap()
{
    mModeDescMap.insert(map<int, string>::value_type(SOCKET_MODE_TCP, "TCP"));
    mModeDescMap.insert(map<int, string>::value_type(SOCKET_MODE_UDP, "UDP"));
}

void Server::initCliSockIdArr()
{
    int i = 0;
    for(i = 0; i < MAX_LISTEN_NUM; i++)
    {
        mCliSockIdArr[i] = INVALID_SOCK_ID;
    }
}

int Server::insertCliSockId(const unsigned int id)
{
    int ret = -1;
    int i = 0;
    pthread_mutex_lock(&mCliSockIdArrMutex);
    for(i = 0; i < MAX_LISTEN_NUM; i++)
    {
        if(mCliSockIdArr[i] == INVALID_SOCK_ID)
        {
            break;
        }
        else
        {
            if(id == mCliSockIdArr[i])
            {
                printf("client socket id = %d, has been exist in mCliSockIdArr, index = %d, will not insert again.\n",
                    id, i);
                ret = 0;
                pthread_mutex_unlock(&mCliSockIdArrMutex);
                return ret;
            }
            else
            {
                //do nothing, just to next index
            }
        }
    }

    if(i >= MAX_LISTEN_NUM)
    {
        fprintf(stderr, "mCliSockIdArr has full! cannot insert other client socket id!\n");
        ret = -1;
    }
    else
    {
        mCliSockIdArr[i] = id;
        ret = 0;
    }
    
    pthread_mutex_unlock(&mCliSockIdArrMutex);
    
    return ret;
}

int Server::delCliSockId(const unsigned int id)
{
    int ret = 0;
    bool isExist = false;

    pthread_mutex_lock(&mCliSockIdArrMutex);

    int i = 0;
    for(i = 0; i < MAX_LISTEN_NUM; i++)
    {
        if(mCliSockIdArr[i] == INVALID_SOCK_ID)
        {
            break;
        }
        else if(mCliSockIdArr[i] == id)
        {
            isExist = true;
            break;
        }
        else
        {
            //do nothing, just to next index
        }
    }

    if(isExist)
    {
        int j = i;
        for(j = i; j < MAX_LISTEN_NUM - 1; j++)
        {
            mCliSockIdArr[j] = mCliSockIdArr[j + 1];
            if(mCliSockIdArr[j] == INVALID_SOCK_ID)
                break;
        }
        mCliSockIdArr[MAX_LISTEN_NUM - 1] = INVALID_SOCK_ID;
    }
    else
    {
        //input @id donot exist in mCliSockIdArr
        ret = 0;
    }

    pthread_mutex_unlock(&mCliSockIdArrMutex);

    return ret;
}

void Server::dumpCliSockIds()
{
    int i = 0;

    pthread_mutex_lock(&mCliSockIdArrMutex);

    printf("dump all client socket id here.\n");
    for(i = 0; i < MAX_LISTEN_NUM; i++)
    {
        printf("%d, ", mCliSockIdArr[i]);
    }
    printf("\n\n");
    
    pthread_mutex_unlock(&mCliSockIdArrMutex);
}

int Server::initSocket()
{
    if(mServSockId >= 0)
    {
        printf("Server socket has been inited, not need init again.\n");
        return 0;
    }

    int ret = 0;
    switch(mMode)
    {
        case SOCKET_MODE_TCP:
            ret = initTcpSocket();
            break;
        case SOCKET_MODE_UDP:
            ret = initUdpSocket();
            break;
        default:
            fprintf(stderr, "mMode = %d, its invalid!\n", mMode);
            ret = -1;
            break;
    }
    
    return ret;
}

int Server::initTcpSocket()
{
    //create socket
    mServSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(mServSockId < 0)
    {
        fprintf(stderr, "create socket for server in TCP mode failed! errno = %d, desc = [%s]\n", 
            errno, strerror(errno));
        return -1;
    }

    //Set this socket to reused
    int on = 1;
    int ret = setsockopt(mServSockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret != 0)
    {
        fprintf(stderr, "set server socket to reused failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -2;
    }

    //set this socket to nonblocked
    int flags = fcntl(mServSockId, F_GETFL, 0); 
    if(flags < 0)
    {
        fprintf(stderr, "Get socket flags failed! ret = %d, errno = %d, desc = [%s]\n",
            flags, errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -3;
    }
    ret = fcntl(mServSockId, F_SETFL, flags | O_NONBLOCK);
    if(ret < 0)
    {
        fprintf(stderr, "set socket to nonblock failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -4;
    }

    //bind to port
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(mServPort);
    servAddr.sin_addr.s_addr = inet_addr(mServIp.c_str());
    ret = bind(mServSockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
        fprintf(stderr, "bind failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -5;
    }

    //listen
    ret = listen(mServSockId, MAX_LISTEN_NUM);
    if(ret != 0)
    {
        fprintf(stderr, "listen failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -6;
    }

    printf("Init server socket succeed!\n");
    return 0;
}

int Server::initUdpSocket()
{
    //create socket
    mServSockId = socket(AF_INET, SOCK_DGRAM, 0);
    if(mServSockId < 0)
    {
        fprintf(stderr, "create socket failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return -1;
    }

    //set this socket to reused
    int on = 1;
    int ret = setsockopt(mServSockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret != 0)
    {
        fprintf(stderr, "set server socket to reused in udp mode failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -2;
    }

    //set this socket to nonblock mode
    ioctl(mServSockId, FIONBIO, &on);

    //bind to port
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(mServPort);
    servAddr.sin_addr.s_addr = inet_addr(mServIp.c_str());
    ret = bind(mServSockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
        fprintf(stderr, "bind failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        close(mServSockId);
        mServSockId = -1;
        return -3;
    }

    printf("init server socket in UDP mode succeed.\n");
    return 0;
}


void Server::run()
{
    //If donot init yet, init firstly
    if(mServSockId < 0)
    {
        int ret = initSocket();
        if(ret < 0)
        {
            fprintf(stderr, "initSocket failed! ret = %d.\n", ret);
            return ;
        }
    }

    //Recv clients looply
    switch(mMode)
    {
        case SOCKET_MODE_TCP:
            runInTcpMode();
            break;
        case SOCKET_MODE_UDP:
            runInUdpMode();
            break;
        default:
            fprintf(stderr, "mMode = %d, invalid!\n", mMode);
            break;
    }
}


ssize_t Server::readn (int fd, void *buf, size_t num)
{
    ssize_t res;
    size_t n;
    char *ptr;

    n = num;
    ptr = (char *)buf;
    while (n > 0) 
    {
        if ((res = read (fd, ptr, n)) == -1) 
        {
        if (errno == EINTR)
            res = 0;
        else
            return (-1);
        }
        else if (res == 0)
            break;

        ptr += res;
        n -= res;
    }

    return (num - n);
}


void Server::runInTcpMode()
{
    int maxFdId = 0;

    fd_set rFdSet, wFdSet;

    struct timeval timeout;

    printf("mServSockId = %d \n", mServSockId);
    
    while(true)
    {
        FD_ZERO(&rFdSet);
        FD_ZERO(&wFdSet);

        FD_SET(mServSockId, &rFdSet);
        maxFdId = mServSockId;
        
        int i = 0;
        for(i = 0; i < MAX_LISTEN_NUM; i++)
        {
            if(mCliSockIdArr[i] == -1)
                break;
            FD_SET(mCliSockIdArr[i], &rFdSet);
            FD_SET(mCliSockIdArr[i], &wFdSet);
            maxFdId = (maxFdId >= mCliSockIdArr[i]) ? maxFdId : mCliSockIdArr[i];
        }
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(maxFdId + 1, &rFdSet, &wFdSet, NULL, &timeout);
        if(ret < 0)
        {
            fprintf(stderr, "select failed! ret = %d, errno = %d, desc = [%s]\n", 
                ret, errno, strerror(errno));
            break;
        }
        else if (ret == 0)
        {
//            printf("select timeout, just continue.\n");
            continue;
        }
        else
        {
            //Check all clients
            for(i = 0; i < MAX_LISTEN_NUM; i++)
            {
                if(mCliSockIdArr[i] == INVALID_SOCK_ID)
                {
                    break;
                }
                if(FD_ISSET(mCliSockIdArr[i], &rFdSet))
                {
                    //recv currently, should convert to recvn
                    char recvMsg[128] = {0x00};
                    memset(recvMsg, 0x00, 128);
                    int len = 128;
                    ret = recv(mCliSockIdArr[i], recvMsg, len, 0);
                    if(ret < 0)
                    {
                        fprintf(stderr, "recv failed! i = %d, cliSockId = %d, ret = %d, errno = %d, desc = [%s]\n",
                            i, mCliSockIdArr[i], ret, errno, strerror(errno));
                        fprintf(stderr, "client maybe not exist now, delete it from local.\n");
                        close(mCliSockIdArr[i]);
                        delCliSockId(mCliSockIdArr[i]);
                        FD_CLR(mCliSockIdArr[i], &rFdSet);                   
                        continue;
                    }
                    else if(ret == 0)
                    {
                        fprintf(stderr, "recv ret = %d, errno = %d, desc = [%s], recvMsg = [%s], means client not exist now, delete client from local.\n",
                            ret, errno, strerror(errno), recvMsg);
//                        dumpCliSockIds();
                        close(mCliSockIdArr[i]);
                        delCliSockId(mCliSockIdArr[i]);
                        FD_CLR(mCliSockIdArr[i], &rFdSet);
                        continue;
                    }   
                    else
                    {
                        recvMsg[127] = 0x00;
                        printf("ret = %d, recv msg = [%s], len = %d,  from cliSockId = %d\n", 
                            ret, recvMsg, len, mCliSockIdArr[i]);
                    }
                }
//                if(FD_ISSET(mCliSockIdArr[i], &wFdSet))
//                {
//                    //send
//                    char msg[32] = "response from select server.";
//                    ret = send(mCliSockIdArr[i], msg, 32, 0);
//                    if(ret < 0)
//                    {
//                        fprintf(stderr, "send failed! i = %d, cliSockId = %d, ret = %d, errno = %d, desc = [%s]\n", 
//                            i, mCliSockIdArr[i], ret, errno, strerror(errno));
//                        continue;
//                    }
//                    else
//                    {
//                        printf("send msg [%s] to client OK.\n", msg);
//                    }
//                }
            }

            
            if(FD_ISSET(mServSockId, &rFdSet))  //some client send connect request
            {
                struct sockaddr_in clientAddr;
                memset(&clientAddr, 0x00, sizeof(struct sockaddr_in));
                socklen_t len = sizeof(struct sockaddr_in);
                int cliSockId = accept(mServSockId, (struct sockaddr *)&clientAddr, &len);
                if(cliSockId < 0)
                {
                    fprintf(stderr, "accept failed! errno = %d, desc = [%s]\n", 
                        errno, strerror(errno));
                    continue;
                }
                else
                {
                    printf("accept a new client connect request. client ip = [%s], port = [%d], id = %d.\n", 
                        inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), cliSockId);

                    //add this client addr to local
                    ret = insertCliSockId(cliSockId);
                    if(ret != 0)
                    {
                        fprintf(stderr, "insertCliSockId failed! ret = %d, cliSockId = %d\n",
                            ret, cliSockId);
                        dumpCliSockIds();
                        continue;
                    }
                    else
                    {
                        //do nothing;
                    }
                }
            }

        }
    }
}

void Server::runInUdpMode()
{
    fd_set rFdSet;
    struct timeval timeout;
    while(true)
    {
        FD_ZERO(&rFdSet);
        FD_SET(mServSockId, &rFdSet);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(mServSockId + 1, &rFdSet, NULL, NULL, &timeout);
        if(ret == 0)
        {
            //timeout
            continue;
        }
        else if(ret < 0)
        {
            fprintf(stderr, "select failed!\n");
            break;
        }
        else
        {
            if(FD_ISSET(mServSockId, &rFdSet))
            {
                char recvMsg[128] = {0x00};
                struct sockaddr_in clientAddr;
                socklen_t cliLen = sizeof(struct sockaddr_in);
                ret = recvfrom(mServSockId, recvMsg, 128, 0, (struct sockaddr *)&clientAddr, &cliLen);
                if(ret <= 0)
                {
                    fprintf(stderr, "recvFrom failed! ret = %d, errno = %d, desc = [%s]\n",
                        ret, errno, strerror(errno));
                    break;
                }
                else
                {
                    recvMsg[127] = 0x00;
                    printf("recv msg is [%s]\n", recvMsg);
                }
            }
        }
    }
}

