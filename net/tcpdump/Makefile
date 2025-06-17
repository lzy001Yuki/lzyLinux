CC=gcc
CFLAGS=-Wall -g
LIBS=-lpcap

all: test_custom_tcpdump

test_custom_tcpdump:  custom_tcpdump.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

custom_tcpdump.o: custom_tcpdump.c
	$(CC) $(CFLAGS) -c -o $@ $<
