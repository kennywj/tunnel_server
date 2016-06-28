#
#   Module: Makefile for cvrdaemon
#   The cvrdaemon is a module on CVR server to handle the service request from device
#
#DEBUG_ON=y
#
# pure tunnel, not do registry
#
#DO_TEST_TUNNEL=y
#
# raw data tx test
#
#DO_RAW_TUNN_TEST=y
#
# enable secure tunnel
#
SUPPORT_SSL_TUNNEL=y

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

OBJS =  main.o tunnel.o tunnutil.o tunnmsg.o link.o cmd.o console.o parser.o cJSON.o clouds.o
	
ifdef SUPPORT_SSL_TUNNEL
CFLAGS += -D_OPEN_SECURED_CONN
OBJS += sslsock_server.o
endif
	
ifdef DEBUG_ON
	CFLAGS += -DDEBUG
endif

ifdef DO_TEST_TUNNEL
	CFLAGS += -DTEST_TUNNEL
endif

ifdef DO_RAW_TUNN_TEST
	CFLAGS += -DRAW_TUNN_TEST
endif
	
	
	
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
	
