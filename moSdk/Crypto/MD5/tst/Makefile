CC = gcc
CXX = g++

INC_DIR = -I../../inc -I../inc -I../../../Logger/inc

SRC_DIR = ../src/
SRC_FILES = $(SRC_DIR)moCrypt_MD5.c

TST_DIR = ./
TST_FILES = $(TST_DIR)/tst.c

MOSDK_DIR = ../../../release

TAR = tstMD5

all:
	$(CC) -o $(TAR) $(SRC_FILES) $(TST_FILES) $(INC_DIR) -L$(MOSDK_DIR)/ -lMoLogger -lMoIniParser

clean:
	rm -rf $(TAR)
