CC = gcc

LIBNSOCK_PATH = libnsock

# CFLAGS = -I$(LIBNSOCK_PATH) -O2 -Wall
CFLAGS = -I$(LIBNSOCK_PATH) -ggdb -Wall

LDFLAGS = -lnsock -L$(LIBNSOCK_PATH)
# EXTLDFLAGS = -s

all: srcs.mk nsc

srcs.mk:
	$(CC) $(CFLAGS) -MM *.c > srcs.mk

nsc: Makefile nsc.o
	$(CC) $(CFLAGS) -o nsc nsc.o $(EXTLDFLAGS) $(LDFLAGS)

clean:
	rm -f *.o nsc

distclean: clean
	rm -f srcs.mk

.c.o:
	$(CC) $(CFLAGS) -c $<
