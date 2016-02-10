BER= ..

include $(BER)/config

CFLAGS= $(MYCFLAGS) $(WARN) -I$(INC) $(LUAINCS)

T= $(LIB)/libluaber.a
OBJS= luaber.o ber.o ber_util.o mmodr.o
SRCS= luaber.c ber.c ber_util.c mmodr.c
HDRS= $(INC)/luaber.h asn/odr.h ber.h ber_util.h mmodr.h

all: $T

$T:	$(OBJS) $(HDRS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $T
