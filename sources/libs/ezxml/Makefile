CFLAGS=-I.
LIB_OUT=libezxml.a
SOURCES=ezxml.c 
OBJECTS=$(SOURCES:.c=.o)

OS_NAME=$(shell uname -s)
ifeq ($(OS_NAME),FreeBSD)
    CC=cc
    LD=cc
    AR=ar rcs
else 
    CC=gcc
    LD=gcc
    AR=ar rcs
endif

all: $(SOURCES) $(LIB_OUT)

$(LIB_OUT): $(OBJECTS)
	$(AR) $@ $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

install:
	exit 0

clean:
	@rm -f *.o *.a

