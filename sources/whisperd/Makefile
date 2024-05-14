PREFIX ?= /opt/whisperd

UNAME_M=$(shell uname -m)
UNAME_S=$(shell uname -s)
BIN_OUT=whisperd

MPG123_INC=-I../libs/mpg123-1.32.3/src/libmpg123 -I../libs/mpg123-1.32.3/src/libsyn123
MPG123_LIB=../libs/mpg123-1.32.3/src/libmpg123/.libs/libmpg123.a ../libs/mpg123-1.32.3/src/libsyn123/.libs/libsyn123.a

RE_INC=-I../libs/libre/dist/include
RE_LIB=../libs/libre/dist/lib/libre.a

ifeq ($(UNAME_S),Darwin)
SSL_INC=-I/opt/homebrew/opt/openssl@3/include
SSL_LIB=/opt/homebrew/opt/openssl@3/lib/libcrypto.a /opt/homebrew/opt/openssl@3/lib/libssl.a
endif

#DFLAGS+=-DWD_DEBUG_ENABLE_RE_MEMDEBUG -DWD_DEBUG_ENABLE_RE_TMRDEBUG

ifeq ($(UNAME_S),Darwin)
OFLAGS=-O3
else
OFLAGS=-O3 -march=westmere
endif
CFLAGS=-g ${OFLAGS} ${DFLAGS}
ifeq ($(UNAME_S),Darwin)
CFLAGS+=-I./include -I../libs/ezxml -I../libs/multipart -I./whisper_cpp ${RE_INC} ${MPG123_INC} ${SSL_INC}
else
CFLAGS+=-I./include -I../libs/ezxml -I../libs/multipart -I./whisper_cpp ${RE_INC} ${MPG123_INC}
endif
LIBS+=../libs/ezxml/libezxml.a
LIBS+=../libs/multipart/libmultipart.a
ifeq ($(UNAME_S),Darwin)
LIBS+=${RE_LIB} ${MPG123_LIB} ${SSL_LIB}
LIBS+=-lc -lm -lpthread -lz
else
LIBS+=${RE_LIB} ${MPG123_LIB}
LIBS+=-lc -lm -lpthread -lssl -lcrypto -lz
endif



OBJECTS=$(SOURCES:.c=.o) $(CXXSOURCES:.cpp=.o)

SOURCES=./src/whisperd.c ./src/whisperd-misc.c ./src/whisperd-file.c ./src/whisperd-config.c ./src/whisperd-srvc-http.c ./src/whisperd-srvc-cluster.c ./src/whisperd-audio.c
SOURCES+=./src/whisperd-parser.c ./src/whisperd-model.c ./src/whisperd-hashtable.c ./src/whisperd-list.c ./src/whisperd-whisper.c
SOURCES+=./whisper_cpp/ggml.c

CXXFLAGS=${OFLAGS} ${DFLAGS} -std=c++11 -fpermissive -I./include -I./whisper_cpp ${RE_INC}
ifeq ($(UNAME_S),Darwin)
CXXLIBS=-lstdc++
else
CXXLIBS=-lstdc++ -lgcc
endif
CXXSOURCES=./whisper_cpp/common-ggml.cpp ./whisper_cpp/whisper.cpp ./src/whisperd-whisper-wrp.cpp

ifeq ($(UNAME_S),FreeBSD)
    CXX=cc
    CC=cc
    LD=cc
    LIBS+=-lexecinfo
else
    CXX=g++
    CC=gcc
    LD=gcc
endif

ifeq ($(UNAME_M),$(filter $(UNAME_M),x86_64 i686))
	ifeq ($(UNAME_S),FreeBSD)
	    OFLAGS += -mfma -mf16c -mavx -mavx2
	else ifeq ($(UNAME_S),Linux)
	    AVX2_M := $(shell grep "avx2 " /proc/cpuinfo)
	    ifneq (,$(findstring avx2,$(AVX2_M)))
		OFLAGS += -mavx2
	    endif
	    FMA_M := $(shell grep "fma " /proc/cpuinfo)
	    ifneq (,$(findstring fma,$(FMA_M)))
	        OFLAGS += -mfma
	    endif
	    F16C_M := $(shell grep "f16c " /proc/cpuinfo)
	    ifneq (,$(findstring f16c,$(F16C_M)))
	        OFLAGS += -mf16c
	        AVX1_M := $(shell grep "avx " /proc/cpuinfo)
	        ifneq (,$(findstring avx,$(AVX1_M)))
		    OFLAGS += -mavx
		endif
	    endif
	    SSE3_M := $(shell grep "sse3 " /proc/cpuinfo)
	    ifneq (,$(findstring sse3,$(SSE3_M)))
	        OFLAGS += -msse3
	    endif
	else
	    OFLAGS += -mfma -mf16c -mavx -mavx2
	endif
endif

all: $(SOURCES) $(CXXSOURCES) $(BIN_OUT)

$(BIN_OUT): $(OBJECTS)
	$(LD) -o $@ $(OBJECTS) $(LIBS) $(CXXLIBS)
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

install:
	mkdir -p $(PREFIX)/bin/
	mkdir -p $(PREFIX)/configs/
	mkdir -p $(PREFIX)/models/
	cp $(BIN_OUT) $(PREFIX)/bin/
	cp misc/whisperd.sh $(PREFIX)/bin/
	cp misc/whisperd-conf.xml $(PREFIX)/configs/
	cp misc/server.pem $(PREFIX)/configs/

clean:
	@rm -f src/*.o
	@rm -f whisper_cpp/*.o
	@rm -f $(BIN_OUT)
