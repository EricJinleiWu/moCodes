#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
SRC_DIR = ./src
TST_DIR = ./tst

# INC DIRS.
INCDIRS = -I./inc

# System lib path
LDFLAGS =	-lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.cpp))
MAIN_OBJS += $(patsubst $(TST_DIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(TST_DIR)/*.cpp))

# app or plugins obj
PROGRAM_DIR = ./bin
PROGRAM_BIN = $(PROGRAM_DIR)/tstMoString
          
# complete binary
APP: DEPENDS $(PROGRAM_BIN)
$(PROGRAM_BIN): $(MAIN_OBJS)
	@echo ">>> compiling $(PROGRAM_LIB) ..."
	$(CXX) -o $@ $^ $(LDFLAGS)
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(PROGRAM_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
$(PROGRAM_OBJ_DIR)/%.o: $(TST_DIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(PROGRAM_DIR)
		
