#Compiler for linux
CXX			:= g++
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep MOSDK_Utils | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

MO_SDK_RELEASE_DIR = ../release

#src dir
ThreadPool_DIR = ./ThreadPool
ThreadPool_SRC_DIR = $(ThreadPool_DIR)/src
ThreadPool_INC_DIR = $(ThreadPool_DIR)/inc

# INC DIRS.
INCDIRS = -I./inc \
	      -I$(ThreadPool_INC_DIR)

# System lib path
LDFLAGS =	-lpthread
#LDFLAGS += -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(ThreadPool_SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(ThreadPool_SRC_DIR)/*.c))

#target is a lib
TARGET_DIR = ./lib
TARGET = $(TARGET_DIR)/libMoUtils.so.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ -shared $(INCDIRS) $(LDFLAGS)
	ln -s libMoUtils.so.$(DVRGW_SUFFIX) $(TARGET_DIR)/libMoUtils.so
	cp $(TARGET_DIR)/* $(MO_SDK_RELEASE_DIR) -rfad
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(MO_SDK_RELEASE_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(ThreadPool_SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
	@rm -rf $(MO_SDK_RELEASE_DIR)/libMoUtils.so*
		
