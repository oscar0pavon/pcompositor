
SRC = $(wildcard *.c)
OBJS = ${SRC:.c=.o}

LDFLAGS  = -lwayland-server

all: pcompositor

pcompositor: $(OBJS) ../pcompositor
	cc -g $(OBJS) -o ../pcompositor $(LDFLAGS)

drm: drm.c buffer.c
	cc -g drm.c -I/usr/include/libdrm -ldrm -lgbm -o drm
	cc -g buffer.c -I/usr/include/libdrm -ldrm -lgbm -o buffer

clean:
	rm -f *.o


