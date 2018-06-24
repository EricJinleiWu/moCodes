#!/usr/bin/python3
# -*- coding : utf-8 *-*

""" 
    Start do moCaptCapture for moFaceRec project;
    Logger module being init firstly;
    Cfg module will tell us all infomation about server info and moCaptCapture info and others;
    Capture module will do moCaptCapture by raspistill or piccamera, use what just depends on your settings;
    and this main module will send the picture(jpg/yuv) to moFaceRecServer by connect protocol(http);
    this main module also should start a http server, when moFaceRecServer want to refresh config info, should send request to this server.
"""

__author__ = "EricWu"

import requests
import time
import os

from moCaptCapture import MoCaptCapture
from moCaptCfg import MoCaptCfg

def sendPic2Server(captFilepath, serverIp, serverPort):
    """
    send the picture(maybe jpg or yuv) to server;
    """
    if True:
        print("sendPic2Server stub")
        return 0
    else:
        ret = 0
        fd = open(captFilepath, "rb")
        reqBody = {"picture" : fd}
        serverUrl = "http://"
        serverUrl += serverIp
        serverUrl += ":"
        serverUrl += str(serverPort)
        serverUrl += "/captureFile"
        try:
            resp = requests.post(serverUrl, data=reqBody)
        except requests.exceptions.ConnectionError:
            print("catch an exception : ConnectionError! check for it! serverUrl=", serverUrl)
            ret = -1
        except requests.exceptions.ConnectTimeout:
            print("catch an exception : ConnectTimeout! check for it! serverUrl=", serverUrl)
            ret = -2
        except:
            print("catch an exception : Unknown! serverUrl=", serverUrl)
            ret = -3
        finally:
            fd.close()
            if ret != 0:
                return ret
        
        #parse response
        if resp.status_code != 200:
            print("resp.status_code=%d, donot statusOk." % (resp.status_code))
            return -4
        return 0    

def startHttpServer():
    print("startHttpServer stub")
    return 0

    #TODO, do it later
    
    

def main():
    moCaptCfg = MoCaptCfg("./moCapture.cfg")
    captureParam = moCaptCfg.getCaptInfo()
    if captureParam == None:
        print("getCaptInfo failed!")
        return -1
    captureInteval = captureParam["Inteval"]
    
    moCapture = MoCaptCapture(0)    #0 means use raspistill to do capture
    
    #TODO, start our http server, we should have ability to stop it when error ocurred
    startHttpServer()
    
    while(True):
        if moCaptCfg.isUpdating():
            print("Updating config file, we will wait for it util update being done.")
            time.sleep(captureInteval)
            continue
        
        captureParam = moCaptCfg.getCaptInfo()
        if captureParam == None:
            print("getCaptInfo failed!")
            return -2
        captureWidth = captureParam["Width"]
        captureHeight = captureParam["Height"]
        captureFormatStr = captureParam["Format"]
        if captureFormatStr != "jpg":
            captureFormat = 1
        else:
            captureFormat = 0
        captureDirpath = captureParam["CaptureDirpath"]
        captureInteval = captureParam["Inteval"]
        
        #do capture
        ret, captFilepath = moCapture.capture(captureDirpath, captureWidth, captureHeight, captureFormat)
        if ret != 0:
            print("capture failed! ret=", ret)
            return -3
        
        #send to server
        serverInfo = moCaptCfg.getServInfo()
        serverIp = serverInfo["Ip"]
        serverPort = serverInfo["Port"]
        ret = sendPic2Server(captFilepath, serverIp, serverPort)
        if ret != 0:
            print("sendPic2Server failed. serverIp=[%s], serverPort=%d" % (serverIp, serverPort))
        
        #delete capture file
        os.remove(captFilepath)
        
        time.sleep(captureInteval)
    
    return 0

if __name__ == "__main__":
    main()
