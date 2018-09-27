#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
SRCDIR = ./

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./ \
	  -I../include

# System lib path
LDFLAGS = -L../lib -lavcodec -lavformat -lavutil -lswresample
LDFLAGS += -lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRCDIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.cpp))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/ffmpegMyTest
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CXX) -o $@ $^ $(INCDIRS) $(LDFLAGS)
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
