CC = gcc
CXX = g++

SRC = ./tst.c
TARGET = ./tstMoNvrCli

all:
	$(CC) -o $(TARGET) $(SRC) -I../inc -I../../../../moSdk/Crypto/inc/ -I../../../../moSdk/Logger/inc/ -I../../../../moSdk/Utils/inc -L../lib -lMoNvrClient -L../../../../moSdk/release/ -lMoCrypto -lMoLogger -lMoUtils

clean:
	rm -rf $(TARGET)
