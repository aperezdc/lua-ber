BER= ../..

include $(BER)/config

ASN= z3950v3.asn charneg-3.asn datetime.asn esadmin.asn esupdate.asn\
 mterm2.asn oclcui.asn univres.asn

T= z3950.odr
PDU= $(T).pdu

all: $T

$T:	$(ASN)
	$(BIN)/asn2odr useful.asn -s $(ASN)
	mv asn.odr $(T)

pdu:	$(T)
	$(BIN)/odr2pdu $(T) > $(PDU)

clean:
	rm -f $(T) $(PDU)
