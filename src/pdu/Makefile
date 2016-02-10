BER= ../..

include $(BER)/config

CFLAGS= $(MYCFLAGS) $(WARN)

T= $(BIN)/odr2pdu
OBJS= pdu.o
SRCS= pdu.c

all: $T

$T:	$(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(BERLIBS)

clean:
	rm -f $(OBJS) $T
