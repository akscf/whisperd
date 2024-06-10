PREFIX ?= /opt/whisperd

UNAME_S=$(shell uname -s)
LIB_OUT=libwhisperd.so
BIN_OUT=whisperd

MPG123_INC=-I/opt/whisperd/include/libmpg123 -I/opt/whisperd/include/libsyn123
MPG123_LIB=-L/opt/whisperd/lib -lmpg123 -lsyn123

WSTK_INC=-I/opt/whisperd/include/wstk
WSTK_LIB=-L/opt/whisperd/lib -lwstk

WHSD_INC=-I./include
WHSD_LIB=-L. -lwhisperd

CFLAGS=-g -O2 -fPIC
LIBS=-lc -lm -lpthread $(WHSD_LIB) $(WSTK_LIB) $(MPG123_LIB)
INCS=$(WHSD_INC) $(WSTK_INC) $(MPG123_INC)

BIN_SOURCES=./src/main.c
LIB_SOURCES=./src/whsd-main.c ./src/whsd-misc.c ./src/whsd-config.c ./src/whsd-http-server.c ./src/whsd-form-decoder.c ./src/whsd-codecs.c ./src/whsd-modules.c

BIN_OBJECTS=$(BIN_SOURCES:.c=.o)
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)

ifeq ($(UNAME_S),Linux)
	CC=gcc
	LD=gcc
	CFLAGS+=-DWSTK_OS_LINUX
endif

ifeq ($(UNAME_S),FreeBSD)
	CC=cc
	LD=cc
	CFLAGS+=-DWSTK_OS_FREEBSD
endif

ifeq ($(UNAME_S),Dawrin)
	CC=cc
	LD=cc
	CFLAGS+=-DWSTK_OS_DARWIN
endif


all: $(LIB_SOURCES) $(BIN_SOURCE) $(LIB_OUT) $(BIN_OUT)

$(LIB_OUT): $(LIB_OBJECTS)
	$(LD) -shared -o $@ $(LIB_OBJECTS)

$(BIN_OUT): $(BIN_OBJECTS)
	$(LD) -o $@ $(BIN_OBJECTS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

install:
	mkdir -p $(PREFIX)/bin/
	mkdir -p $(PREFIX)/lib/
	mkdir -p $(PREFIX)/lib/mods/
	mkdir -p $(PREFIX)/include/
	mkdir -p $(PREFIX)/configs/
	cp $(LIB_OUT) $(PREFIX)/lib/
	cp $(BIN_OUT) $(PREFIX)/bin/
	cp misc/whisperd-run.sh $(PREFIX)/bin/
	cp misc/whisperd-conf.xml $(PREFIX)/configs/
	rm -drf $(PREFIX)/include/whisperd
	cp -r ./include $(PREFIX)/include/whisperd

clean:
	@rm -f src/*.o
	@rm -f $(BIN_OUT)
	@rm -f $(LIB_OUT)


