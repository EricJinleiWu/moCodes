#Compiler for linux
CXX:= g++
CC := gcc

VERNUM = $(shell cat VERINFO | grep MOCPS_CLIENTLIB | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

MO_SDK_RELEASE_DIR = ../../../moSdk/release
MO_SDK_INC_DIR = ../../../moSdk/inc

#moCloudUtils dir
UTILS_SRCDIR = ../moCloudUtils/src
UTILS_INCDIR = ../moCloudUtils/inc

#src
SRCDIR = ./src
INCDIR = ./inc

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I$(UTILS_INCDIR) \
		-I$(INCDIR) \
		-I$(MO_SDK_INC_DIR)

# System lib path
LDFLAGS = -lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(UTILS_SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(UTILS_SRCDIR)/*.c))
MAIN_OBJS += $(patsubst $(SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.c))

#target is a lib
TARGET_DIR = ./lib
TARGET = $(TARGET_DIR)/libMoCloudClient.so.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ -shared $(INCDIRS) $(LDFLAGS)
	ln -s libMoCloudClient.so.$(DVRGW_SUFFIX) $(TARGET_DIR)/libMoCloudClient.so
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(UTILS_SRCDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
