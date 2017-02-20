CC = gcc
CXX = g++

INC = ./
SRC = main.cpp server.cpp

TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/epollServer

all:depends
	$(CXX) -o $(TARGET) -I$(INC) $(SRC)

depends:
	mkdir -p $(TARGET_DIR)

clean:
	rm -rf $(TARGET_DIR)


