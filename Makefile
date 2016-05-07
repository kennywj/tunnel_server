#
#   Module: Makefile for cvrdaemon
#   The cvrdaemon is a module on CVR server to handle the service request from device
#
DEBUG_ON=y

CC=gcc
ifeq ($(OS),Windows_NT)
	TARGET = cvrdaemon.exe
else
	TARGET = cvrdaemon
endif

ifeq ($(OS),Windows_NT)
    CFLAGS += -D WIN32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CFLAGS += -D LINUX
    endif
    ifeq ($(UNAME_S),Darwin)
        CFLAGS += -D DARWIN
    endif
endif

CFLAGS	+= -I. -Wall -s -O0 -g -D_XOPEN_SOURCE -D_GNU_SOURCE -g -DFD_SETSIZE=1024
LDFLAGS += -lpthread -Wl,-Map,$(TARGET).map -lm -lssl -lcrypto -lcurl

ifdef DEBUG_ON
	CFLAGS += -DDEBUG
endif

OBJS =  main.o tunnel.o tunnutil.o tunnmsg.o link.o cmd.o console.o parser.o 
		
.SUFFIXS: .c

%.o: %.c
	$(CC) $< -c $(CFLAGS)

all: $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	@rm -rf *.o $(TARGET) $(TARGET).map

install: $(TARGET)
	install -d $(TARGETDIR)/sbin
	install -m 755 $(TARGET) $(TARGETDIR)/sbin
	$(STRIP) $(TARGETDIR)/sbin/$(TARGET)
	
