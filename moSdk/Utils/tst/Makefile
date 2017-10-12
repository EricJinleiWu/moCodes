CC = gcc
CXX = g++

INC_DIR = -I../inc -I../../Logger/inc

SRC_DIR = ./
SRC_FILES = $(SRC_DIR)tst.c

MOUTILS_LIBDIR = ../../release/

TAR = tstMoUtils

all:
	$(CC) -o $(TAR) $(SRC_FILES) $(INC_DIR) -lpthread -L$(MOUTILS_LIBDIR) -lMoUtils  -lMoLogger 

clean:
	rm -rf $(TAR)
