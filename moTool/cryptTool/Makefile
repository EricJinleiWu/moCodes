#Compiler for linux
CXX			:= g++
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep MOTOOL_Crypt | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

RELEASE_DIR = ../release

#src dir, When add a new algorithm, add its dir here.
SRC_DIR = ./src
INC_DIR = ./inc

MOSDK_CRYPT_DIR = ../../moSdk/Crypto/
MOSDK_CRYPT_INC = $(MOSDK_CRYPT_DIR)inc
MOSDK_CRYPT_LIB = $(MOSDK_CRYPT_DIR)lib

MOSDK_LOGGER_DIR = ../../moSdk/Logger/
MOSDK_LOGGER_INC = $(MOSDK_LOGGER_DIR)inc

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I$(INC_DIR) \
		  -I$(MOSDK_CRYPT_INC) \
		  -I$(MOSDK_LOGGER_INC)

# System lib path
LDFLAGS =	-lpthread -lrt -ldl
LDFLAGS += -L$(MOSDK_CRYPT_LIB) -lMoCrypto

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/moCryptTool.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ $(INCDIRS) $(LDFLAGS)
	cp $(TARGET_DIR)/* $(RELEASE_DIR) -rfad
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(RELEASE_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
