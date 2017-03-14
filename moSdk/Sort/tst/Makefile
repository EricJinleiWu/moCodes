CC = gcc
CXX = g++

INC_DIR = -I../inc -I../../Logger/inc

SRC_DIR = ./
SRC_FILES = $(SRC_DIR)tst.c

MOUTILS_LIBDIR = ../../release/

TAR = tstMoSort

all:
	$(CC) -o $(TAR) $(SRC_FILES) $(INC_DIR) -lpthread -L$(MOUTILS_LIBDIR) -lMoSort  -lMoLogger -lMoIniParser

clean:
	rm -rf $(TAR)
