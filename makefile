include config.mk

CC=gcc
CFLAGS=-g -Wall -Werror $(LIBUSB_CFLAGS)
LDFLAGS=
LIBS=$(LIBUSB_LIBS)
BINS=nrfdude

all: $(BINS)

nrfdude: nrfdude.o ihex.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: tags clean

tags:
	ctags .

clean:
	-rm $(BINS) *.o tags
