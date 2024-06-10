PREFIX ?= /opt/whisperd

UNAME_S=$(shell uname -s)
MOD_PATH=$(PREFIX)/lib/mods/
MOD_OUT=mod-whisper-fast.so

WHISPERD_INC=-I/opt/whisperd/include/wstk -I/opt/whisperd/include/whisperd
WHISPERD_LIB=-L/opt/whisperd/lib -lwhisperd -lwstk

CFLAGS=-g -O2 -fPIC
LIBS=$(WHISPERD_LIB)
INCS=$(WHISPERD_INC) 

MOD_SOURCES=mod-whisper-fast.c
MOD_OBJECTS=$(MOD_SOURCES:.c=.o)


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


all: $(MOD_SOURCES) $(MOD_OUT)

$(MOD_OUT): $(MOD_OBJECTS)
	$(LD) -shared -o $@ $(MOD_OBJECTS)

.c.o:
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

install:
	cp $(MOD_OUT) $(MOD_PATH)

clean:
	@rm -f *.o *.so



