#Compiler for linux
CXX:= g++
CC := gcc

MO_SDK_RELEASE_DIR = ../../../../moSdk/release
MO_SDK_INC_DIR = ../../../../moSdk/inc

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I$(MO_SDK_INC_DIR) \
		-I../../moCloudUtils/inc \
		-I../inc

# System lib path
LDFLAGS = -lpthread -lrt -ldl \
		-L$(MO_SDK_RELEASE_DIR) -lMoCrypto -lMoLogger -lMoUtils \
		-L../lib -lMoCloudClient


#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

SRC_DIR = ./

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/tstMoCloudClient
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ $(INCDIRS) $(LDFLAGS)
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
