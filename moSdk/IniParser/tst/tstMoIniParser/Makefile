#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
SRC_DIR = ./

# INC DIRS.
INCDIRS = -I../../inc

# System lib path
LIB_SIMS_DIR = ../../lib/
LDFLAGS = -L$(LIB_SIMS_DIR) -lMoIniParser
#LDFLAGS +=	-lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))

# app or plugins obj
PROGRAM_DIR = ./bin
PROGRAM_BIN = $(PROGRAM_DIR)/tstSimsIniParser
          
# complete binary
APP: DEPENDS $(PROGRAM_BIN)
$(PROGRAM_BIN): $(MAIN_OBJS)
	@echo ">>> compiling $(PROGRAM_BIN) ..."
	$(CC) -o $@ $^ $(LDFLAGS)
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(PROGRAM_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(PROGRAM_DIR)
		
