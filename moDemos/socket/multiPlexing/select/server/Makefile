CC = gcc
CXX = g++

INC = ./
SRC = main.cpp server.cpp

TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/selectServer

all:depends
	$(CXX) -o $(TARGET) -I$(INC) $(SRC) -lpthread

depends:
	mkdir -p $(TARGET_DIR)

clean:
	rm -rf $(TARGET_DIR)


