
# This is what I use, uncomment if you know your arch and want to specify
# ARCH= -gencode arch=compute_52,code=compute_52


CC=gcc
CPP=g++

ENV=

#common to both compiler and linker
COMMON=-Iinclude/ -I. -DOPENCV `pkg-config --cflags opencv` 
#only for compiler
CFLAGS=-Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -O0 -g
#only for linker
LDFLAGS= -lmvnc -lm -ljpeg -lturbojpeg `pkg-config --libs opencv` -lstdc++ 

CFLAGS+=$(OPTS)

OBJDIR=./debug/objs/

EXEC=./debug/gengi
OBJ=movidius.o yolov2.o  socket.o  image_opencv.o

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

