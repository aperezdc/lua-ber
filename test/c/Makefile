BER= ../..

include $(BER)/config

CFLAGS= $(MYCFLAGS) $(WARN) -I$(INC) -I$(BER)/src $(LUAINCS)

T= tst
OBJS= $(T).o
SRCS= $(T).c

all: $T

$T:	$(OBJS) $(HDRS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(BERLIBS) $(LUALIBS)

clean:
	rm -f $(OBJS) $T
