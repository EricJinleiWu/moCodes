CC = gcc
CXX = g++

INC_DIR = -I../../inc -I../inc

SRC_DIR = ../src/
SRC_FILES = $(SRC_DIR)moCrypt_MD5.c

TST_DIR = ./
TST_FILES = $(TST_DIR)/tst.c

TAR = tstMD5

all:
	$(CC) -o $(TAR) $(SRC_FILES) $(TST_FILES) $(INC_DIR)

clean:
	rm -rf $(TAR)
