#Compiler for linux
CXX			:= g++
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep MOSDK_Crypto | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

MO_SDK_RELEASE_DIR = ../release

#The inc dir of utils
MOUTILS_DIR = ../Utils/
MOUTILS_INCDIR = $(MOUTILS_DIR)inc

#src dir, When add a new algorithm, add its dir here.
RC4_DIR = ./RC4
RC4_SRC_DIR = $(RC4_DIR)/src
RC4_INC_DIR = $(RC4_DIR)/inc
MD5_DIR = ./MD5
MD5_SRC_DIR = $(MD5_DIR)/src
MD5_INC_DIR = $(MD5_DIR)/inc
BASE64_DIR = ./BASE64
BASE64_SRC_DIR = $(BASE64_DIR)/src
BASE64_INC_DIR = $(BASE64_DIR)/inc

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./inc \
	      -I$(RC4_INC_DIR) \
		  -I$(MD5_INC_DIR) \
		  -I$(BASE64_INC_DIR) \
		  -I$(MOUTILS_INCDIR)

# System lib path
#LDFLAGS =	-lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(RC4_SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(RC4_SRC_DIR)/*.c))
MAIN_OBJS += $(patsubst $(MD5_SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(MD5_SRC_DIR)/*.c))
MAIN_OBJS += $(patsubst $(BASE64_SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(BASE64_SRC_DIR)/*.c))

#target is a lib
TARGET_DIR = ./lib
TARGET = $(TARGET_DIR)/libMoCrypto.so.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ -shared $(INCDIRS) $(LDFLAGS)
	ln -s libMoCrypto.so.$(DVRGW_SUFFIX) $(TARGET_DIR)/libMoCrypto.so
	cp $(TARGET_DIR)/* $(MO_SDK_RELEASE_DIR) -rfad
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(MO_SDK_RELEASE_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(RC4_SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(MD5_SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(BASE64_SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
	@rm -rf $(MO_SDK_RELEASE_DIR)/libMoCrypto.so*
		
