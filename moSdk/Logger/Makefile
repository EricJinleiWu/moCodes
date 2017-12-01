#Compiler for linux
CXX			:= g++
#CC			:= arm-hisiv400-linux-gcc
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep MOSDK_Logger | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

MO_SDK_RELEASE_DIR = ../release

#src dir
SRC_DIR = ./src

# INC DIRS.
INCDIRS = -I./inc \
	  -I../inc

# System lib path
LDFLAGS = -L../
#LDFLAGS =	-lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

#target is a lib
TARGET_DIR = ./lib
TARGET = $(TARGET_DIR)/libMoLogger.so.$(DVRGW_SUFFIX)

# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ -shared $(INCDIRS) $(LDFLAGS)
	ln -s libMoLogger.so.$(DVRGW_SUFFIX) $(TARGET_DIR)/libMoLogger.so
	cp $(TARGET_DIR)/* $(MO_SDK_RELEASE_DIR) -rfad
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(MO_SDK_RELEASE_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
$(PROGRAM_OBJ_DIR)/%.o: $(TST_DIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
	@rm -rf $(MO_SDK_RELEASE_DIR)/libMoLogger.so*
		
