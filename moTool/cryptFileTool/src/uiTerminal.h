#ifndef __UI_TERMINAL_H__
#define __UI_TERMINAL_H__

#include "ui.h"
#include "conProtocol.h"

class UiTerminal : public UI
{
public:
    UiTerminal();
    UiTerminal(const UiTerminal & other);
    ~UiTerminal();

public:
    virtual void showHelp();

    virtual void run();

    virtual void showProgBar();

    virtual void setProgress(const int prog);

private:
    void showBasicInfo();
    int getRequest();

private:
    MOCFT_TASKINFO mTaskInfo;
    cpClient * mpCpClient;
};

#endif