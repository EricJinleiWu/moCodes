#Compiler for linux
CXX			:= g++
#CC			:= arm-hisiv400-linux-gcc
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep moIniParser | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

MO_SDK_RELEASE_DIR = ../release

#src dir
SRC_DIR = ./

# INC DIRS.
INCDIRS = -I./inc

# System lib path
#LDFLAGS =	-lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

# app or plugins obj
PROGRAM_DIR = ./lib
PROGRAM_LIB = $(PROGRAM_DIR)/libMoIniParser.so.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(PROGRAM_LIB)
$(PROGRAM_LIB): $(MAIN_OBJS)
	@echo ">>> compiling $(PROGRAM_LIB) ..."
	$(CC) -o $@ $^ $(LDFLAGS) -shared
	ln -s libMoIniParser.so.$(DVRGW_SUFFIX) $(PROGRAM_DIR)/libMoIniParser.so
	cp $(PROGRAM_DIR)/* $(MO_SDK_RELEASE_DIR) -rfad
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(PROGRAM_DIR)
	@mkdir -p $(MO_SDK_RELEASE_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(PROGRAM_DIR)
	@rm -rf $(MO_SDK_RELEASE_DIR)/libMoIniParser.so*
		
