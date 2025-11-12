
SRC = $(wildcard *.c)
OBJS = ${SRC:.c=.o}

LDFLAGS  = -lwayland-client

all: pwayland_window

pwayland_window: $(OBJS)
	cc -g $(OBJS) -o pwayland_window $(LDFLAGS)

install:
	cp pwayland_window /usr/local/bin

clean:
	rm -f *.o
	rm -f pwayland_window

