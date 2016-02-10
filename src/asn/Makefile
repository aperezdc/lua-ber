BER= ../..

include $(BER)/config

CFLAGS= $(MYCFLAGS) $(WARN)

T= $(BIN)/asn2odr
OBJS= asn.o map.o
SRCS= asn.c map.c
HDRS= odr.h asn.h map.h

all: $T

$T:	$(OBJS) $(HDRS)
	$(CC) -o $@ $(CFLAGS) $(OBJS)

clean:
	rm -f $(OBJS) $T
