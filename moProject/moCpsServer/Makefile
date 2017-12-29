#Compiler for linux
CXX			:= g++
CC			:= gcc

MO_SDK_RELEASE_DIR = ../../moSdk/release
MO_SDK_INC_DIR = ../../moSdk/inc

#src dir
SRCDIR = ./src

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./inc \
		-I$(MO_SDK_INC_DIR)

# System lib path
LDFLAGS =	-lpthread -lrt -ldl \
		-L../../moSdk/release/ -lMoCpsCli -lMoCpsServ -lMoCrypto -lMoLogger -lMoSort -lMoUtils

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.c))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/moCpsServer
          
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
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
