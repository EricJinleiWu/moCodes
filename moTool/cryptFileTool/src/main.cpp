#include <stdio.h>

#include "ui.h"
#include "uiTerminal.h"
#include "conProtocol.h"
#include "cpSocket.h"
#include "taskMgr.h"

int main(int argc, char **argv)
{
    moLoggerInit("./");
    
    //Signal should check?

    //Init server firstly
    int ret = cpServerInit();
    if(ret != 0)
    {
        printf("cpServerInit failed! ret = %d\n", ret);
        return -1;
    }

    cpServerRun();
    
//    cpServer * pServ = new CpServerSocket();
//    dynamic_cast<CpServerSocket *>(pServ)->run();

//    //Init taskMgr as singleton
//    TaskMgrSingleton t(dynamic_cast<CpServerSocket *>(pServ), &CpServerSocket::setProg);


    //Start UI
    UI *pUi = new UiTerminal();
    pUi->run();

    cpServerUnInit();

    moLoggerUnInit();
    
    return 0;
}





































