#Compiler for linux
CXX			:= g++
CC			:= gcc

#VERNUM = $(shell cat VERINFO | grep MoCryptFileTool | awk '{print $$2}' | cut -b 2-)
#DVRGW_SUFFIX = $(VERNUM)

MOCFT_BIN_DIR = ./lib

#The inc dir of utils
MOUTILS_INCDIR = ../../../moSdk/Utils/inc

#The inc dir of crypto
MOCRYPT_INCDIR = ../../../moSdk/Crypto/inc

#The inc dir of Logger
MOLOGGER_INCDIR = ../../../moSdk/Logger/inc

#local src and include dir
MOCFT_INCDIR = ./inc
MOCFT_SRCDIR = ./src

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I$(MOCFT_INCDIR) \
		  -I$(MOCFT_SRCDIR) \
		  -I$(MOUTILS_INCDIR) \
		  -I$(MOCRYPT_INCDIR) \
		  -I$(MOLOGGER_INCDIR)

# System lib path
LDFLAGS =	-lpthread -lrt -ldl
LDFLAGS += \
		   -L../../../moSdk/release -lMoLogger -lMoUtils -lMoCrypto -lMoIniParser

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(MOCFT_SRCDIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(MOCFT_SRCDIR)/*.cpp))

#target is a lib
TARGET = $(MOCFT_BIN_DIR)/mocftProtBg.so
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CXX) -o $@ $^ $(INCDIRS) $(LDFLAGS)
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(MOCFT_BIN_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(MOCFT_SRCDIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(MOCFT_BIN_DIR)
		
