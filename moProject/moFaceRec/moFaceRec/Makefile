#Compiler for linux
CXX			:= g++
CC			:= gcc

#src dir
SRCDIR = ./src

VERNUM = $(shell cat VERINFO | grep MOFACEREC | awk '{print $$2}' | cut -b 2-)
VER_SUFFIX = $(VERNUM)

# INC DIRS. All algorithm should set header file in this directory.
INCDIRS = -I./inc \
	  -I./src \
	  -I./arcSoft/inc \
	  -I./ffmpeg/inc

# System lib path
LDFLAGS = -L./arcSoft/lib -larcsoft_fsdk_age_estimation -larcsoft_fsdk_face_recognition -larcsoft_fsdk_face_tracking -larcsoft_fsdk_gender_estimation -larcsoft_fsdk_face_detection
LDFLAGS += -L./ffmpeg/lib -lavcodec -lavformat -lavutil -lswresample
LDFLAGS += -lpthread -lrt -ldl
LDFLAGS += -lmysqlclient

#Cflags
CFLAGS	= -g -ggdb -MMD -Wall -W

#obj dir
PROGRAM_OBJ_DIR = ./obj

# source file obj
MAIN_OBJS = $(patsubst $(SRCDIR)/%.c, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.c))
MAIN_OBJS += $(patsubst $(SRCDIR)/%.cpp, $(PROGRAM_OBJ_DIR)/%.o, $(wildcard $(SRCDIR)/*.cpp))

#target is a lib
TARGET_DIR = ./bin
TARGET = $(TARGET_DIR)/moFaceRec.$(VER_SUFFIX)
          
# complete binary
APP: DEPENDS $(TARGET)
$(TARGET): $(MAIN_OBJS)
	@echo ">>> compiling $(TARGET) ..."
	$(CXX) -o $@ $^ $(INCDIRS) $(LDFLAGS)
	ln -s moFaceRec.$(VER_SUFFIX) $(TARGET_DIR)/moFaceRec
      
DEPENDS:
	@echo "::: compiling depends ..."
	@mkdir -p $(PROGRAM_OBJ_DIR)
	@mkdir -p $(TARGET_DIR)

# src --> obj.
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.c
	@echo ">>> compiling $< ..."
	$(CC) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
$(PROGRAM_OBJ_DIR)/%.o: $(SRCDIR)/%.cpp
	@echo ">>> compiling $< ..."
	$(CXX) $(CFLAGS) $(INCDIRS) -fPIC -c $< -o $@
	
clean:
	@rm -rf $(PROGRAM_OBJ_DIR)
	@rm -rf $(TARGET_DIR)
		
