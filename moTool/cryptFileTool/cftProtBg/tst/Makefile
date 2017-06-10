CC = gcc
CXX = g++

INC_DIR = \
		  -I../../../../moSdk/Logger/inc/ \
		  -I../../../../moSdk/Utils/inc/ \
		  -I../../../../moSdk/Crypto/inc/ \
		  -I../inc

LDFLAGS = \
		  -L../lib -lMocftProtBg \
		  -L../../../../moSdk/release -lMoCrypto -lMoIniParser -lMoLogger -lMoUtils

SRC_FILES = ./main.cpp
TARGET = ./tstCftProtBg

all:
	$(CXX) -o $(TARGET) $(SRC_FILES) $(INC_DIR) $(LDFLAGS)

clean:

