#Compiler for linux
CXX			:= g++
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep MOSDK_Sort | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

MO_SDK_RELEASE_DIR = ../release

#The inc dir of Logger
MOLOGGER_DIR = ../Logger
MOLOGGER_INCDIR = $(MOLOGGER_DIR)/inc

#src dir, When add a new algorithm, add its dir here.
DIRECTINSERT_DIR = ./DirectInsertSort
DIRECTINSERT_INC = $(DIRECTINSERT_DIR)/inc
DIRECTINSERT_SRC = $(DIRECTINSERT_DIR)/src

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./inc \
		  -I$(MOLOGGER_INCDIR) \
	      -I$(DIRECTINSERT_INC)

# System lib path
#LDFLAGS =	-lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(DIRECTINSERT_SRC)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(DIRECTINSERT_SRC)/*.c))

#target is a lib
TARGET_DIR = ./lib
TARGET = $(TARGET_DIR)/libMoSort.so.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ -shared $(INCDIRS) $(LDFLAGS)
	ln -s libMoSort.so.$(DVRGW_SUFFIX) $(TARGET_DIR)/libMoSort.so
	cp $(TARGET_DIR)/* $(MO_SDK_RELEASE_DIR) -rfad
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(MO_SDK_RELEASE_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(DIRECTINSERT_SRC)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
	@rm -rf $(MO_SDK_RELEASE_DIR)/libMoSort.so*
		
