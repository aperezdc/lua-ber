BER_SRCS     := src/ber.c src/ber_util.c src/luaber.c src/mmodr.c
BER_OBJS     := $(BER_SRCS:.c=.o)
ASN2ODR_SRCS := src/asn/asn.c src/asn/map.c
ASN2ODR_OBJS := $(ASN2ODR_SRCS:.c=.o)
ODR2PDU_SRCS := src/pdu/pdu.c src/mmodr.c
ODR2PDU_OBJS := $(ODR2PDU_SRCS:.c=.o)

# Makeing a Makefile compatible with LuaRocks:
#   https://github.com/keplerproject/luarocks/wiki/Creating-a-Makefile-that-plays-nice-with-LuaRocks

LUA          = lua
LUA_VERSION  =
DESTDIR      =
INST_PREFIX  = /usr/local
INST_BINDIR  = $(INST_PREFIX)/bin
INST_LIBDIR  = $(INST_PREFIX)/lib/lua/$(LUA_VERSION)
INST_LUADIR  = $(INST_PREFIX)/share/lua/$(LUA_VERSION)
INST_CONFDIR = $(INST_PREFIX)/etc

ifeq ($(strip $(LUA_VERSION)),)
  LUA_VERSION = $(shell $(LUA) -e 'print(string.sub(_VERSION, 5))')
endif

ifeq ($(strip $(LIBFLAG)),)
  # XXX May not work everywhere
  LIBFLAG = -shared
  CFLAGS += -fPIC
endif

ifneq ($(strip $(LUA_INCDIR)),)
  CPPFLAGS += -I$(LUA_INCDIR)
endif
ifneq ($(strip $(LUA_LIBDIR)),)
  LDFLAGS += -L$(LUA_LIBDIR)
endif

all: print-vars ber.so asn2odr odr2pdu

print-vars:
	@echo "CC           = $(CC)"
	@echo "CFLAGS       = $(CFLAGS)"
	@echo "CPPFLAGS     = $(CPPFLAGS)"
	@echo "LDFLAGS      = $(LDFLAGS)"
	@echo "LIBFLAG      = $(LIBFLAG)"
	@echo "LUA          = $(LUA)"
	@echo "LUA_VERSION  = $(LUA_VERSION)"
	@echo "LUA_INCDIR   = $(LUA_INCDIR)"
	@echo "DESTDIR      = $(DESTDIR)"
	@echo "INST_PREFIX  = $(INST_PREFIX)"
	@echo "INST_BINDIR  = $(INST_BINDIR)"
	@echo "INST_LIBDIR  = $(INST_LIBDIR)"
	@echo "INST_LUADIR  = $(INST_LUADIR)"
	@echo "INST_CONFDIR = $(INST_CONFDIR)"

.PHONY: print-vars

ber.so: $(BER_OBJS)
	$(CC) $(LIBFLAG) -o $@ $(LDFLAGS) $^

asn2odr: $(ASN2ODR_OBJS)
	$(CC) -o $@ $(LDFLAGS) $^

odr2pdu: $(ODR2PDU_OBJS)
	$(CC) -o $@ $(LDFLAGS) $^

clean:
	$(RM) ber.so $(BER_OBJS)
	$(RM) asn2odr $(ASN2ODR_OBJS)
	$(RM) odr2pdu $(ODR2PDU_OBJS)

install: all
	install -Dm755 ber.so $(DESTDIR)$(INST_LIBDIR)/ber.so
	install -Dm755 asn2odr $(DESTDIR)$(INST_BINDIR)/asn2odr
	install -Dm755 odr2pdu $(DESTDIR)$(INST_BINDIR)/odr2pdu

.PHONY: install


TEST_ASN := $(filter-out test/useful.asn,$(wildcard test/*.asn))
TEST_BER := $(wildcard test/*.ber)

test/z3950.odr: test/useful.asn $(TEST_ASN) | asn2odr
	./asn2odr test/useful.asn -s $(TEST_ASN)
	mv asn.odr $@

test/z3950.pdu: test/z3950.odr | odr2pdu
	./odr2pdu $< > $@

test/check: CPPFLAGS += -Isrc
test/check: test/check.o $(BER_OBJS)
	$(CC) -o $@ $(CFLAGS) $^ -llua

check: test/z3950.odr test/check test/check.lua
	@for i in $(TEST_BER) ; do \
		echo "=== ./test/check -ftest/z3950.odr -ltest/check.lua < $$i ===" ; \
		./test/check -ftest/z3950.odr -ltest/check.lua < $$i ; \
	done

.PHONY: check
