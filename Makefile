BER= .

include $(BER)/config

all clean: $T
	cd src; $(MAKE) $@
	cd src/asn; $(MAKE) $@
	cd src/pdu; $(MAKE) $@
	cd test/c; $(MAKE) $@
	cd test/z39; $(MAKE) $@
