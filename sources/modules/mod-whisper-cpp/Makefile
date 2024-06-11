PREFIX ?= /opt/whisperd

UNAME_S=$(shell uname -s)

MOD_PATH=$(PREFIX)/lib/mods/
CFG_PATH=$(PREFIX)/configs/

MOD_OUT=mod-whisper-cpp.so

LIBWHISPER_INC=-I/opt/whisper_cpp/include
LIBWHISPER_LIB=-L/opt/whisper_cpp/lib -lwhisper

WHISPERD_INC=-I/opt/whisperd/include/wstk -I/opt/whisperd/include/whisperd
WHISPERD_LIB=-L/opt/whisperd/lib -lwhisperd -lwstk

CFLAGS=-g -O2 -fPIC
LIBS=$(WHISPERD_LIB) $(LIBWHISPER_LIB)
INCS=-I. $(WHISPERD_INC) $(LIBWHISPER_INC)

MOD_SOURCES=mod-whisper-cpp.c mod-whisper-cpp-misc.c
MOD_OBJECTS=mod-whisper-cpp.o mod-whisper-cpp-misc.o

ifeq ($(UNAME_S),Linux)
	CPP=g++
	CC=gcc
	LD=gcc
	CFLAGS+=-DWSTK_OS_LINUX
endif

ifeq ($(UNAME_S),FreeBSD)
	CPP=cc
	CC=cc
	LD=cc
	CFLAGS+=-DWSTK_OS_FREEBSD
endif

ifeq ($(UNAME_S),Dawrin)
	CPP=cc
	CC=cc
	LD=cc
	CFLAGS+=-DWSTK_OS_DARWIN
endif


all: $(MOD_SOURCES) $(MOD_OUT)

$(MOD_OUT): $(MOD_OBJECTS)
	$(LD) -shared -o $@ $(MOD_OBJECTS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@
.cpp.o:
	$(CPP) $(CFLAGS) $(INCS) -c $< -o $@

install:
	cp $(MOD_OUT) $(MOD_PATH)
	cp mod-whisper-cpp-conf.xml $(CFG_PATH)

clean:
	@rm -f *.o *.so



