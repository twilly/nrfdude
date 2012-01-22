include config.mk

CC=gcc
CFLAGS=-g -Wall -Werror $(LIBUSB_CFLAGS)
LDFLAGS=
LIBS=$(LIBUSB_LIBS)
BINS=nrfdude

all: $(BINS)

nrfdude: nrfdude.o ihex.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBS)

release: CFLAGS=-O2 -Wall -Werror $(LIBUSB_CFLAGS)
release: $(BINS)

.PHONY: tags clean

tags:
	ctags .

clean:
	-rm $(BINS) *.o tags
