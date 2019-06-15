#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
SRCDIR = ./

#libJpeg directory
LIBJPEG_OUTPUT_DIR = /home/wujl/MO/jpeg-9c/output/

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./ \
	  -I../include \
	  -I$(LIBJPEG_OUTPUT_DIR)/include

# System lib path
LDFLAGS = -L../lib -lavcodec -lavformat -lavutil -lswresample
LDFLAGS += -L$(LIBJPEG_OUTPUT_DIR)/lib -ljpeg
LDFLAGS += -lpthread -lrt -ldl

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.c))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/ffmpegMyTest
          
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
		
