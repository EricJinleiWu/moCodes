#!/usr/bin/python3
# -*- coding : utf-8 *-*

""" 
    log to file or stdout;
    if loggerFile being create succeed, write log info to this file looply;
    else, output to stdout;
"""

__author__ = "EricWu"

LOGGER_FILEPATH = "../logs/moCapture.log"
LOGGER_FILE_MAXSIZE = 1024  #in killo bytes

#TODO, should modify here to an enum
LOGGER_MODE_STDOUT = 0
LOGGER_MODE_FILE = 1
LOGGER_MODE = LOGGER_MODE_STDOUT

class MoCaptLogger(object):
    
    def __init__(self, loggerFilepath, loggerMode, loggerFileMaxsize):
        self.loggerFilepath = loggerFilepath
        self.loggerFileMaxsize = loggerFileMaxsize
        self.errNo = 0
        
        self.loggerMode = LOGGER_MODE_STDOUT
        if loggerMode == LOGGER_MODE_FILE:
            self.fd = None
            self.openLogFile()
            self.loggerMode = LOGGER_MODE_FILE
            
    def __del__(self):
        if self.loggerMode == LOGGER_MODE_FILE and self.fd != None:
            self.fd.close()
    
    def openLogFile(self):
        """
        open loggerFile, if open succeed, then write log to this file;
        else, write log to stdout;
        """
        try:
            self.fd = open(self.loggerFilepath, "a")
        except IOError as err:
            print("Open file failed: [" + str(err) + "]")
            self.fd = None
            self.errNo = -1
        
    
    def getLogCfg(self):
        """
        loggerFilepath is a string;
        loggerFileMaxSize is the maxsize of this file, in KilloBytes;
        loggerMode is the mode of output, 0 is output to stdout, 1 is output to file;
        """
        return (self.loggerFilepath, self.loggerFileMaxsize, self.loggerMode)
    
    def getErrStr(self, errNo):
        errDescDict = {
            0 : "No Error",
            -1 : "Log File open failed!",
            #TODO, others
            }
        if errDescDict.has_key(errNo):
            return errDescDict[errNo]
        else:
            return "Unknown errNo"
    
    def getErr(self):
        """
        get the last error No and description;
        """
        return (self.errNo, self.getErrStr(self.errNo))
    
    def logger(self):
        """
        If output to file, and file open succeed, then write to file looply, should not larger than maxSize!
        If output to file, and file open failed, just output to stdout;
        If output to stdout, just output to stdout is OK.
        """
        #TODO, do it, currently, just a stub
        
        pass
    
moCaptLogger = MoCaptLogger(LOGGER_FILEPATH, LOGGER_MODE, LOGGER_FILE_MAXSIZE)