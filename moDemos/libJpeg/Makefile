#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
LIBJPEG_OUTPUT_DIR=../output/
SRCDIR = ./src

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./inc \
	  -I$(LIBJPEG_OUTPUT_DIR)/include

# System lib path
LDFLAGS = -L$(LIBJPEG_OUTPUT_DIR)/lib -ljpeg
LDFLAGS += -lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.c))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/libjpegMyTest
          
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
		
