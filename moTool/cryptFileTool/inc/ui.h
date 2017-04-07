#ifndef __UI_H__
#define __UI_H__

typedef enum
{
    UI_CRYPT_STATE_IDLE,
    UI_CRYPT_STATE_CRYPTING
}UI_CRYPT_STATE;

class UI
{
public:
    UI() {}
    virtual ~UI() {}

public:
    /*
        Show some infomation of this tool;
        Show the usage of this tool;
    */
    virtual void showHelp() = 0;

    /*
        Start ui;
    */
    virtual void run() = 0;

    /*
        When need progress, should use this function to show progress bar;
    */
    virtual void showProgBar() = 0;

    /*
        Set the progress, when needed, can show it in progress bar;
    */
    virtual void setProgress(const int prog) = 0;

protected:
    UI_CRYPT_STATE mState;
    int mProgress;
};


typedef void (UI::*pSetProgFunc)(const int);


#endif
