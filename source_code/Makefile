
SRC = $(wildcard *.c)
OBJS = ${SRC:.c=.o}

LDFLAGS  = -lwayland-server

all: pcompositor

pcompositor: $(OBJS)
	cc -g $(OBJS) -o pcompositor $(LDFLAGS)

clean:
	rm -f *.o
	rm -f pcompositor


