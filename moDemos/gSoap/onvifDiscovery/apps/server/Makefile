#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
SRCDIR = ./
UTILSDIR = ../utils
GENFILESDIR = ../../genFiles/server
LIBDIR = ../libs

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I$(UTILSDIR) -I$(GENFILESDIR)

# System lib path
LDFLAGS = -lpthread -lrt -ldl
LDFLAGS += -L$(LIBDIR) -lgsoap -lgsoapck

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(UTILSDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(UTILSDIR)/*.c))
MAIN_OBJS += $(patsubst $(GENFILESDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(GENFILESDIR)/*.c))
MAIN_OBJS += $(patsubst $(SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.c))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/onvifDiscoveryServer
          
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
$(PROGRAM_OBJ_DIR)/%.o: $(UTILSDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(GENFILESDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@

clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
	
