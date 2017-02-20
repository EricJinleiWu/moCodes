CC = gcc
CXX = g++

INC = ./
SRC = main.cpp multiThread.cpp

TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/multiThread

all:depends
	$(CXX) -o $(TARGET) -I$(INC) $(SRC) -lpthread

depends:
	mkdir -p $(TARGET_DIR)

clean:
	rm -rf $(TARGET_DIR)


