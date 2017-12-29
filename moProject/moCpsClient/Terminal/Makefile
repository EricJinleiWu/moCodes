CC = gcc
SRC = ./main.c
TARGET = moCpsClient
INCDIR = ../../../moSdk/inc/
LDFLAGS = -lpthread \
	-L../../../moSdk/release/ -lMoCpsCli -lMoCpsServ -lMoCrypto -lMoLogger -lMoSort -lMoUtils

all:
	$(CC) -o $(TARGET) $(SRC) -I$(INCDIR) $(LDFLAGS)

clean:
	rm -rf $(TARGET)
