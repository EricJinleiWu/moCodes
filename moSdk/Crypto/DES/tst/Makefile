CC = gcc
CXX = g++

MOSDK_RELEASE_DIR = ../../../
UTILS_DIR = ../../../Utils/

INC_DIR = -I../../inc -I$(UTILS_DIR)/inc -I../../../Logger/inc/

SRC_DIR = ../src/
SRC_FILES = $(SRC_DIR)moCrypt_DES.c

TST_DIR = ./
TST_FILES = $(TST_DIR)/tst.c

TAR = tstDes

all:
	$(CC) -o $(TAR) $(SRC_FILES) $(TST_FILES) $(INC_DIR) -L$(MOSDK_RELEASE_DIR)/release -lMoUtils -lMoCrypto -lMoLogger -lMoIniParser

clean:
	rm -rf $(TAR)
