CC = gcc
DEBUG = -g
INC = -I./include/ 
LIBS = -lpthread -llog -lnet
LIBDIR = -L./lib/ -Wl,-rpath=./lib
CFLAGS = 
#-D__FILENAME__='"$(subst $(dir $<), ,$<)"'
SHARED = -fpic -shared


SRC_DIR = . 

SRC = $(foreach dir,$(SRC_DIR), $(wildcard $(dir)/*.c))

BIN = ftpclient 

SHAREDBIN = libftpclient.so

all:$(SRC)
	$(CC) $(SRC) $(CFLAGS) $(DEBUG) $(INC) $(LIBDIR) $(LIBS) -o $(BIN)

shared:$(SRC)
	$(CC) $(SRC) $(CFLAGS) $(INC) $(LIBDIR) $(LIBS) $(SHARED) -o $(SHAREDBIN)

.PHONY:

clean:
	rm -rf $(BIN) $(SHAREDBIN)
