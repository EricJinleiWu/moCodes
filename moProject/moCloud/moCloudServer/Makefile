#Compiler for linux
CXX:= g++
CC := gcc

VERNUM = $(shell cat VERINFO | grep MOCPS_SERVER | awk '{print $$2}' | cut -b 2-)
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
LDFLAGS = -lpthread -lrt -ldl \
          -L$(MO_SDK_RELEASE_DIR) -lMoCrypto -lMoLogger -lMoUtils


#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(UTILS_SRCDIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(UTILS_SRCDIR)/*.cpp))
MAIN_OBJS += $(patsubst $(SRCDIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.cpp))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/moCloudServer.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CXX) -o $@ $^ $(INCDIRS) $(LDFLAGS)
	ln -s moCloudServer.$(DVRGW_SUFFIX) $(TARGET_DIR)/moCloudServer
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(UTILS_SRCDIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
