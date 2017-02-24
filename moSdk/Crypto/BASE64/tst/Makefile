CC = gcc
CXX = g++

INC_DIR = ../../inc

SRC_DIR = ../src/
SRC_FILES = $(SRC_DIR)moCrypt_BASE64.c

TST_DIR = ./
TST_FILES = $(TST_DIR)/tst.c

TAR = tstBase64

all:
	$(CC) -o $(TAR) $(SRC_FILES) $(TST_FILES) -I $(INC_DIR)

clean:
	rm -rf $(TAR)
