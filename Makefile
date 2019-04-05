


CC = gcc
CPP = g++

CFLAGS = -g -Wall -I.
CPPFLAGS = $(CFLAGS) #-std=c++11 
LIBRARIES = -L/usr/local/lib
LFLAGS = -lmvnc -lm -ljpeg -lturbojpeg# -lopencv_core -lopencv_imgproc -lopencv_highgui
LDFLAGS = $(LIBRARIES) $(LFLAGS)
#SRC = ./src
OBJ_DIR = debug/objs/

SOURCES_C = darknet_region_layer.c movidius.c socket.c
# SOURCES_CPP = $(wildcard yolo_ncs/*.cpp mov1.cpp) 
# SOURCES_CPP = mov1.cpp #$(wildcard mov1.cpp) 

OBJECTS_C  = $(addprefix $(OBJ_DIR), $(SOURCES_C:.c=.o))
OBJECTS_CPP = $(addprefix $(OBJ_DIR), $(SOURCES_CPP:.cpp=.o))

$(info $(OBJECTS_C))
# OBJECTS_CPP := $(OBJECTS_CPP:.cpp=.o)

# _OBJECTS_C = $(patsubst %.c,%.o,$(SOURCES_C))
# _OBJECTS_CPP = $(patsubst %.cpp,%.o,$(SOURCES_CPP))

# OBJECTS_C  = $(patsubst %.o, debug/objs/%.o,$(_OBJECTS_C))
# OBJECTS_CPP = $(patsubst %.o, debug/objs/%.o,$(_OBJECTS_CPP))

EXECUTABLE = debug/gengi


#all: $(OBJ_DIR) $(SOURCES_C) $(SOURCES_CPP) $(EXECUTABLE)
all: clean $(OBJ_DIR) $(SOURCES_C) $(EXECUTABLE)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)/yolo_ncs

$(OBJ_DIR)%.o : %.c
	@echo Compiling: $< to $@
	@$(CC) $(CFLAGS) -c $< -o $@

debug/objs/%.o : %.cpp
	@echo Compiling: $< 
	@$(CPP) $(CPPFLAGS) -c $< -o $@


$(EXECUTABLE): $(OBJECTS_C)
	@echo Linking: $@
	@$(CC) -g $(OBJECTS_C) $(LDFLAGS) -o $@

.PHONY: clean
clean: clean
	@echo "\nmaking clean";
	rm -f debug/mov2
	rm -f debug/objs/*.o
	rm -f debug/objs/yolo_ncs/*

