
# This is what I use, uncomment if you know your arch and want to specify
# ARCH= -gencode arch=compute_52,code=compute_52

OPENCV=1
#SOCKET=1
NCS=1
CC=gcc
CPP=g++

#common to both compiler and linker
COMMON=-Iinclude/ -I.

#only for compiler
CFLAGS=-Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -O0 -g
#only for linker
LDFLAGS= -lmvnc -lm -ljpeg -lturbojpeg 
ifdef OPENCV
  COMMON += -DOPENCV `pkg-config --cflags opencv`
  LDFLAGS += `pkg-config --libs opencv` -lstdc++
endif
ifdef SOCKET
	COMMON += -DSOCKET
endif
ifdef NCS
	COMMON += -DNCS
endif

CFLAGS+=$(OPTS)

OBJDIR=./debug/objs/

EXEC=./debug/gengi
OBJ=ncs.o ny2.o  socket.o  #image_opencv.o

OBJS = $(addprefix $(OBJDIR), $(OBJ))
DEPS = $(wildcard ./*.h)


all: obj clean $(EXEC)
#all: obj  results $(SLIB) $(ALIB) $(EXEC)

$(info $(OBJS))

$(EXEC): $(OBJS)
	$(CC)  $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)%.o: %.cpp $(DEPS)
	$(CPP) $(COMMON) $(CFLAGS) -c $< -o $@

$(OBJDIR)%.o: %.c $(DEPS)
	$(CC)  $(COMMON) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p ./debug/objs/

.PHONY: clean
clean:
	rm -rf $(OBJS) $(EXEC)

