#!/usr/bin/python3
# -*- coding : utf-8 *-*

""" 
    config file management module;
    set default config file if not exist or in wrong format;
    get config info from file for using;
    when needed, refresh config info and file;
"""

__author__ = "EricWu"

import os
import json

gCfgState = {0 : "idle", 1 : "updating"}

class MoCaptCfg(object):
    
    def __init__(self, cfgFilepath):
        self.cfgFilepath = cfgFilepath
        self.lastParseTimestamp = 0
        self.state = 0
    
    def __del__(self):
        pass

    def setDefaultCfg(self):
        self.hostInfo = {"Ip" : "127.0.0.1", "Port" : 8080, "Name" : "EricWu"}
        self.serverInfo = {"Ip" : "127.0.0.1", "Port" : 8081}
        self.captParam = {"Width" : 640, "Height" : 480, "Format" : "jpg", "Inteval" : 10, "CaptureDirpath" : "./"}
        #write these config info to file
        try:
            fd = open(self.cfgFilepath, "w")
        except IOError as err:
            print("open file [%s] for write to default config info failed!" % (self.cfgFilepath))
            return False
        cfgDict = {"HostInfo" : self.hostInfo, "ServerInfo" : self.serverInfo, "CaptureParam" : self.captParam}
        json.dump(cfgDict, fd, indent = 4, separators = (",", ":"))
        fd.close()
        return True
    
    def parse(self):
        """
        check @self.cfgFilepath exist or not;
        check @self.lastParseTimestamp equal with current timestamp or not;
            if equal, donot need refresh config file, just return OK;
            else, goto below;
        parse config file: 
            if in wrong format, set default value to it, and refresh to file;
            else, set to memory;
            
        return 0 if succeed;
        return 0- if failed, if failed, means config is invalid, caller must do it seriaslly; 
        """
        
        if not os.path.exists(self.cfgFilepath):
            print("config file [%s] donot exist, create default one now!" % (self.cfgFilepath))
            if self.setDefaultCfg():
                print("set default config file succeed.")
                return 0
            else:
                print("set default config file failed.")
                return -1
        
        #check file exist or not
        if not os.path.isfile(self.cfgFilepath):
            print("config file [%s] exist, but donot a regular file!" % (self.cfgFilepath))
            return -1
        
        cfgFileStat = os.stat(self.cfgFilepath)
        cfgFileLastModTime = int(cfgFileStat.st_mtime)
        if self.lastParseTimestamp == cfgFileLastModTime:
            #config file donot changed in this period
            return 0
        
        print("self.lastParsTimestamp=[%d], cfgFileLastModTime=[%d], we will get new config info now." % 
              (self.lastParseTimestamp, cfgFileLastModTime))
        
        try:
            fd = open(self.cfgFilepath, "r")
        except IOError as err:
            print("open config file [%s] failed! err is [%s]" % (self.cfgFilepath, str(err)))
            return -2
        
        jsonHdl = json.load(fd)
        if not self.isRightFormatCfg(jsonHdl):
            fd.close()
            print("config file donot in right format! we will set default value to it now!")
            if not self.setDefaultCfg():
                print("setDefaultCfg failed!!!")
                return -3
            else:
                print("setDefaultCfg succeed.")
                self.lastParseTimestamp = cfgFileLastModTime
                return 0
        else:
            print("config file in right format, we get it to memory now.")
            self.getCfgFromJson(jsonHdl)
            fd.close()
            self.lastParseTimestamp = cfgFileLastModTime
            return 0
        
    def getCfgFromJson(self, jsonHdl):
        self.hostInfo = jsonHdl["HostInfo"]
        self.serverInfo = jsonHdl["ServerInfo"]
        self.captParam  = jsonHdl["CaptureParam"]

    def isRightFormatCfg(self, jsonHdl):
        """
        Json file as our config file must in right format, or we cannot use it!
        """
        
        if "HostInfo" not in jsonHdl:
            print("HostInfo donot exist! Invalid config file!")
            return False
        if "ServerInfo" not in jsonHdl:
            print("ServerInfo donot exist! Invalid config file!")
            return False
        if "CaptureParam" not in jsonHdl:
            print("CaptureParam donot exist! Invalid config file!")
            return False
        
        #check HostInfo firstly
        hostInfoDict = jsonHdl["HostInfo"]
        if "Ip" not in hostInfoDict or "Port" not in hostInfoDict or "Name" not in hostInfoDict:
            print("In our config file - HostInfo, we must define [Ip, Port, Name], but currently hostInfoDict=", hostInfoDict)
            return False
        
        #check ServerInfo secondly
        serverInfoDict = jsonHdl["ServerInfo"]
        if "Ip" not in serverInfoDict or "Port" not in serverInfoDict:
            print("In our config file - ServerInfo, we must define [Ip, Port], but currently ServerInfoDict=", serverInfoDict)
            return False
        
        #check CaptureParam then
        captParamDict = jsonHdl["CaptureParam"]
        if "Width" not in captParamDict or "Height" not in captParamDict or "Format" not in captParamDict or "Inteval" not in captParamDict or "CaptureDirpath" not in captParamDict:
            print("In our config file - HostInfo, we must define [width, height, format, inteval, captureDirpath], but currently captParamDict=", captParamDict)
            return False
        
        return True
    
    def getHostInfo(self):
        """
        return a dictionary;
        """
        ret = self.parse()
        if ret != 0:
            print("Parse config file by function parse failed! ret = ", ret)
            return None
        
        return self.hostInfo
    
    def getServInfo(self):
        """
        return a dictionary;
        """
        ret = self.parse()
        if ret != 0:
            print("Parse config file by function parse failed! ret = ", ret)
            return None
        
        return self.serverInfo
    
    def getCaptInfo(self):
        """
        return a dictionary;
        """
        ret = self.parse()
        if ret != 0:
            print("Parse config file by function parse failed! ret = ", ret)
            return None
        
        return self.captParam
    
    def getAllInfo(self):
        """
        return a lsit;
        """
        ret = self.parse()
        if ret != 0:
            print("Parse config file by function parse failed! ret = ", ret)
            return None
        
        return [self.hostInfo, self.serverInfo, self.captParam]
    
    def isUpdating(self):
        if self.state == 0: #idle
            return False
        else:   #updating
            return True
    
    def updateCfgInfo(self, cfgInfo):
        """
        update the config info to memory and file;
        when updating, donot use its config info, because it will not be security;
        """
        self.state = 1
        
        if self.isRightFormatCfg(cfgInfo):
            print("input config info donot in right format, cannot do update!")
            self.state = 0
            return -1
        
        try:
            fd = open(self.cfgFilepath, "w")
        except IOError as err:
            print("open file [%s] for update config info failed!" % (self.cfgFilepath))
            return -2
        cfgDict = {"HostInfo" : cfgInfo.hostInfo, "ServerInfo" : cfgInfo.serverInfo, "CaptureParam" : cfgInfo.captParam}
        json.dump(cfgDict, fd, indent = 4, separators = (",", ":"))
        fd.close()
        
        self.hostInfo = cfgInfo.hostInfo
        self.serverInfo = cfgInfo.serverInfo
        self.captParam = cfgInfo.captParam
        
        self.state = 0
        return 0