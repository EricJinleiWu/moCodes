#!/usr/bin/python3
# -*- coding : utf-8 *-*

""" 
    do capture in this module;
    two module can be choosed : raspistill, piccamera;
    all capture params being input by main module;
"""

__author__ = "EricWu"

import os
import time

gCaptMethodDict = {
    0 : "raspistill", 
    1 : "piccamera"
    }

gCaptFileFormat = {
    0 : "jpg",
    1 : "yuv"
    }

class MoCaptCapture(object):
    
    def __init__(self, captMethod = 0):
        """
        @captMethod = 0 : raspistill;
        @captMethod = 1 : piccamera;
        """
        self.captMethod = captMethod
    
    def __del__(self):
        pass
    
    def capture(self, dirPath = "./", width = 640, height = 480, fileFormat = 0, hostName="defaultCameraName"):
        """
        Do capture by raspistill or piccamera, use which method depends on self.captMethod;
        The file we captured will be save in @dirPath;
        capture file in name like : timestamp_width_height.format;
        
        return 0 if succeed;
        return 0- if failed:
             -1 : directory donot valid;
             -2 : invalid param;
        """
        
        #check the directory valid or not
        if not os.path.exists(dirPath):
            print("Error : Input dirPath [%s] donot exist! cannot capture a picture to it!" % (dirPath))
            return (-1, None)
        
        #check the params in valid range or not
        if fileFormat != 0 and fileFormat != 1:
            print("Error : fileFormat=%d, invalid value!" % (fileFormat))
            return (-2, None)
        
        #get capture file name
        curTimestamp = int(time.time())
        captFilename = str(curTimestamp) + "_w" + str(width) + "_h" + str(height) + "." + gCaptFileFormat[fileFormat]
        captFilepath = dirPath
        if not captFilepath.endswith("/"):
            captFilepath += "/"
        captFilepath += captFilename
        print("Debug : width=%d, height=%d, fileFormat=%d, dirPath=[%s], pictureFilename=[%s], pictureFilepath=[%s]" % (width, height, fileFormat, dirPath, captFilename, captFilepath))
        
        #if this file exist yet, delete firstly
        if os.path.exists(captFilepath):
            os.remove(captFilepath)
            if os.path.exists(captFilepath):
                print("Delete file [%s] failed!" % (captFilepath))
                return (-3, None)
        
        #do capture now
        if self.captMethod == 0:
            
            if fileFormat == 0:
                captCmd = "raspistill"
            else:
                captCmd = "raspiyuv"
            
            captCmd += " -w " + str(width) + " -h " + str(height) + " -t 100 -o " + captFilepath
            os.system(captCmd)
            if not os.path.exists(captFilepath):
                print("raspistill failed! filename=[%s], filepath=[%s]" % (captFilename, captFilepath))
                return (-4, None)
            else:
                print("raspistill succeed! filename=[%s], filepath=[%s]" % (captFilename, captFilepath))
                
                #filename should add length, this can help moFaceRecServer to do some other work.
                newFilename = str(curTimestamp) + "_w" + str(width) + "_h" + str(height) + "_l" + str(os.path.getsize(captFilepath)) + "_n" + hostName + "." + gCaptFileFormat[fileFormat]
                newFilepath = dirPath
                if not newFilepath.endswith("/"):
                    newFilepath += "/"
                newFilepath += newFilename
                os.renames(captFilepath, newFilepath)
                
                return (0, newFilepath)
        elif self.captMethod == 1:
            print("Currently, donot support piccamera to do capture.")
            #TODO, do capture by piccamera
            pass
        else:
            print("captMethod = %d, donot support it now.")
            pass
        
        print("Invalid operations to be here!")
        return (-5, None)
    