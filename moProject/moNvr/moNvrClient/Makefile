#Compiler for linux
CXX			:= g++
CC			:= gcc

VERNUM = $(shell cat VERINFO | grep MoNvrClient | awk '{print $$2}' | cut -b 2-)
DVRGW_SUFFIX = $(VERNUM)

#src dir
SRC_DIR = ./

# INC DIRS. 
INCDIRS = -I./inc \
	      -I./ \
		  -I../../../moSdk/Logger/inc/ \
		  -I../../../moSdk/Crypto/inc/ \
		  -I../../../moSdk/Utils/inc/

# System lib path
LDFLAGS =	-lpthread -lrt -ldl \
			-L../../../moSdk/release/ -lMoCrypto -lMoLogger -lMoUtils

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

#target is a lib
TARGET_DIR = ./lib
TARGET = $(TARGET_DIR)/libMoNvrClient.so.$(DVRGW_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CC) -o $@ $^ -shared $(INCDIRS) $(LDFLAGS)
	ln -s libMoNvrClient.so.$(DVRGW_SUFFIX) $(TARGET_DIR)/libMoNvrClient.so
      
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
		
