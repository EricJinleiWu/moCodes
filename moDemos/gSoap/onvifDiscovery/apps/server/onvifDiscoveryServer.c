#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/socket.h>

#include "RemoteDiscoveryBinding.nsmap"
#include "soapH.h"
#include "soapStub.h"

#include "wsaapi.h"
#include "wsddapi.h"

#define dbgDebug(format, ...) printf("[%s, %d--debug] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define dbgError(format, ...) printf("[%s, %d--error] : "format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ONVIF_GROUP_IP  "239.255.255.250"
#define ONVIF_GROUP_PORT 3702

#define DEVICE_UUID_MAXLEN  64
#define DEVICE_UUID_FORMAT  "urn:uuid:%8.8x-%4.4x-%4.4x-%4.4x-%4.4x%8.8x"
#define DEVICE_UUID_STUB_NUM0   0x4a696e4c  //JinL
#define DEVICE_UUID_STUB_NUM1   0x6569      //ei
#define DEVICE_UUID_STUB_NUM2   0x5775      //Wu
#define DEVICE_UUID_STUB_NUM3   0x4572      //Er
#define DEVICE_UUID_STUB_NUM4   0x6963      //ic

#define DEVICE_TYPE     "tdn:NetworkVideoTransmitter"
#define DEVICE_SCOPE    "onvif://www.onvif.org/type/video_encoder " \
                        "onvif://www.onvif.org/type/ptz " \
                        "onvif://www.onvif.org/type/audio_encoder " \
                        "onvif://www.onvif.org/type/video_analytics " \
                        "onvif://www.onvif.org/location/country/China " \
                        "onvif://www.onvif.org/location/city/beijing " \
                        "onvif://www.onvif.org/hardware/EricWu " \
                        "onvif://www.onvif.org/name/IpCamera"

#define DEVICE_SERVICE_ADDRESS_MAXLEN   64
#define DEVICE_SERVICE_ADDRESS_FORMAT   "http://%s/onvif/device_service"
#define DEVICE_USING_IP "192.168.11.154"


static struct soap gOnvifServerSoap;
static int gSockId = -1;


/** Web service operation 'SOAP_ENV__Fault' (returns SOAP_OK or error code) */
int SOAP_ENV__Fault(struct soap* pSoap, char *faultcode, char *faultstring, char *faultactor, struct SOAP_ENV__Detail *detail, struct SOAP_ENV__Code *SOAP_ENV__Code, struct SOAP_ENV__Reason *SOAP_ENV__Reason, char *SOAP_ENV__Node, char *SOAP_ENV__Role, struct SOAP_ENV__Detail *SOAP_ENV__Detail)
{
    //just a stub
    return 0;
}

/** Web service operation '__tdn__Hello' (returns SOAP_OK or error code) */
int __tdn__Hello(struct soap* pSoap, struct wsdd__HelloType tdn__Hello, struct wsdd__ResolveType *tdn__HelloResponse)
{
    //just a stub
    return 0;
}

/** Web service operation '__tdn__Bye' (returns SOAP_OK or error code) */
int __tdn__Bye(struct soap* pSoap, struct wsdd__ByeType tdn__Bye, struct wsdd__ResolveType *tdn__ByeResponse)
{
    //just a stub
    return 0;
}

/** Web service operation '__tdn__Probe' (returns SOAP_OK or error code) */
int __tdn__Probe(struct soap* pSoap, struct wsdd__ProbeType tdn__Probe, struct wsdd__ProbeMatchesType *tdn__ProbeResponse)
{
    //just a stub
    return 0;
}

void wsdd_event_ProbeMatches(
    struct soap *soap, unsigned int InstanceId, const char *SequenceId, unsigned int MessageNumber, 
    const char *MessageID, const char *RelatesTo, struct wsdd__ProbeMatchesType *matches)
{
    ;
}

soap_wsdd_mode wsdd_event_Resolve(
    struct soap *soap, const char *MessageID, const char *ReplyTo, const char *EndpointReference, struct wsdd__ResolveMatchType *match)
{
    return SOAP_WSDD_MANAGED;
}

void wsdd_event_ResolveMatches(
    struct soap *soap, unsigned int InstanceId, const char *SequenceId, unsigned int MessageNumber, 
    const char *MessageID, const char *RelatesTo, struct wsdd__ResolveMatchType *match)
{
    ;
}

void wsdd_event_Hello(
    struct soap *soap, unsigned int InstanceId, const char *SequenceId, unsigned int MessageNumber,
    const char *MessageID, const char *RelatesTo, const char *EndpointReference, const char *Types, 
    const char *Scopes, const char *MatchBy, const char *XAddrs, unsigned int MetadataVersion)
{
    ;
}

void wsdd_event_Bye(
    struct soap *soap, unsigned int InstanceId, const char *SequenceId, unsigned int MessageNumber, 
    const char *MessageID, const char *RelatesTo, const char *EndpointReference, const char *Types, 
    const char *Scopes, const char *MatchBy, const char *XAddrs, unsigned int *MetadataVersion)
{
    ;
}

static char * getUuid()
{
    static char uuid[DEVICE_UUID_MAXLEN] = {0x00};
    if(strlen(uuid) == 0)
    {
        srandom(time(NULL));
        snprintf(
            uuid, DEVICE_UUID_MAXLEN, DEVICE_UUID_FORMAT, 
            DEVICE_UUID_STUB_NUM0, DEVICE_UUID_STUB_NUM1,
            DEVICE_UUID_STUB_NUM2, DEVICE_UUID_STUB_NUM3,
            DEVICE_UUID_STUB_NUM4, random());
        uuid[DEVICE_UUID_MAXLEN - 1] = 0x00;
        dbgDebug("uuid = [%s]\n", uuid);
    }
    return uuid;
}

static char * getServiceAddress()
{
    static char servAddr[DEVICE_SERVICE_ADDRESS_MAXLEN] = {0x00};
    if(strlen(servAddr) == 0)
    {
        snprintf(servAddr, DEVICE_SERVICE_ADDRESS_MAXLEN, DEVICE_SERVICE_ADDRESS_FORMAT, DEVICE_USING_IP);
        servAddr[DEVICE_SERVICE_ADDRESS_MAXLEN - 1] = 0x00;
        dbgDebug("servAddr = [%s]\n", servAddr);
    }
    return servAddr;
}

soap_wsdd_mode wsdd_event_Probe(
    struct soap *soap, const char *MessageID, const char *ReplyTo, const char *Types, const char *Scopes, 
    const char *MatchBy, struct wsdd__ProbeMatchesType *matches) 
{ 
#if 0
    dbgDebug("MessageID:%s\n", MessageID); 
    dbgDebug("ReplyTo:%s\n", ReplyTo); 
    dbgDebug("Types:%s\n", Types); 
    dbgDebug("Scopes:%s\n", Scopes); 
    dbgDebug("MatchBy:%s\n", MatchBy); 
#endif

    soap_wsdd_init_ProbeMatches(soap, matches); 

    int ret = soap_wsa_add_RelatesTo(soap, MessageID);
    if (SOAP_OK != ret)
    { 
        dbgDebug("soap_wsa_add_RelatesTo failed! ret=%d\n", ret);
        return -1;
    }

    soap_wsdd_add_ProbeMatch(soap, matches, 
        getUuid(),
        DEVICE_TYPE, 
        DEVICE_SCOPE,
        NULL, 
        getServiceAddress(),
        10); 
    return SOAP_WSDD_MANAGED; 
}


/*
    soap_bind to a port, this will return a socket id to us;
    create socket;
    add to group;
*/
static int initSoapSocket()
{
    //bind to port, and get sockId
#if 0
    /*
        donot use this function
        #define soap_init(soap) soap_init1(soap, SOAP_IO_DEFAULT)
        this will init soap to default IO protocol, if it is tcp, we cannot do groupcast because it based on udp;
    */
    soap_init(pSoap);
#else
    soap_init1(&gOnvifServerSoap, SOAP_IO_UDP);
#endif
    gOnvifServerSoap.bind_flags = SO_REUSEADDR;

    gOnvifServerSoap.connect_timeout   = 0;
    gOnvifServerSoap.recv_timeout      = 0;
    gOnvifServerSoap.send_timeout      = 0;
    // 打开调试信息
    soap_set_recv_logfile(&gOnvifServerSoap, "./log/recv.xml");
    soap_set_sent_logfile(&gOnvifServerSoap, "./log/send.xml");
    soap_set_test_logfile(&gOnvifServerSoap, "./log/test.log");

    soap_set_namespaces(&gOnvifServerSoap, namespaces);
    
//    soap_register_plugin(&gOnvifServerSoap, soap_wsa);  //这个很重要，我分析了很久才得出的
    
    int sockId = soap_bind(&gOnvifServerSoap, NULL, ONVIF_GROUP_PORT, 100);
    if(!soap_valid_socket(sockId))
    {
        dbgError("soap_bind failed! ret=%d\n", sockId);
        soap_print_fault(&gOnvifServerSoap, stderr);
        soap_end(&gOnvifServerSoap);
        soap_done(&gOnvifServerSoap);
        return -2;
    }
    dbgDebug("soap_bind succeed. sockId=%d\n", sockId);

    /**
     * 指定外出多播数据报的TTL
     */
    unsigned char ttl = 32;
    int ret = setsockopt(sockId, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    if(-1 == ret)
    {
        dbgError("setsockopt to TTL failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        soap_end(&gOnvifServerSoap);
        soap_done(&gOnvifServerSoap);
        return -3;
    }
    dbgDebug("setsockopt to TTL succeed.\n");
    
    //join to group
    struct ip_mreq mreq;
    memset(&mreq, 0x00, sizeof(struct ip_mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(ONVIF_GROUP_IP);
#if 1
    mreq.imr_interface.s_addr = inet_addr(DEVICE_USING_IP);
#else
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#endif
    ret = setsockopt(sockId, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if(ret != 0)
    {
        dbgError("setsockopt failed! ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
        soap_end(&gOnvifServerSoap);
        soap_done(&gOnvifServerSoap);
        return -4;
    }
    dbgDebug("setsockopt succeed, we add to group %s succeed.\n", ONVIF_GROUP_IP);

    return 0;
}


int main(int argc, char** argv)
{
    printf("IP_ADD_MEMBERSHIP=%d\n", IP_ADD_MEMBERSHIP);
    
//    struct soap onvifServerSoap;
    int ret = initSoapSocket(/*&onvifServerSoap*/);
    if(ret < 0)
    {
        dbgError("initSoapSocket failed! ret=%d\n", ret);
        return -1;
    }
    dbgDebug("initSoapSocket succeed.\n");

    while(1)
    {
#if 0
        ret = soap_accept(&gOnvifServerSoap);
        if(ret < 0)
        {
            soap_print_fault(&gOnvifServerSoap, stderr);
            break;
        }
        dbgDebug("accept a client.\n");
        soap_serve(&gOnvifServerSoap);
        soap_end(&gOnvifServerSoap);
#else
        soap_wsdd_listen(&gOnvifServerSoap, 0);
        soap_destroy(&gOnvifServerSoap);
        soap_end(&gOnvifServerSoap);
#endif
    }
    
    soap_done(&gOnvifServerSoap);
    
    return 0;
}

