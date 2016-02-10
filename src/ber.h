#ifndef BER_H
#define BER_H

#include <setjmp.h>	/* jmp_buf */

#include <lua.h>

#include "mmodr.h"


#define BERS_MAX	40	/* deep of bers stack */
#define CHOICES_MAX	8	/* maximum immediately choices */
#define ENC_BUFRESERVE	BERS_MAX * 2		/* sizeof "\0\0" */
#define ENC_LLEN_MAX	sizeof (int) + 1	/* encode length of length */
/*#define ENC_SIMPLESZ_MAX	1000*/		/* CER */


struct ber {
    union tag_id u;
    int len;		/* defined size */
    union {
	int size;	/* total size (decode) */
	unsigned char *bufp;	/* pointer to length (encode) */
    } v;
#define BER_INCOMPL	1
#define BER_MORE	2
#define BER_CONSTR	32
#define BER_INDEFIN	128
    unsigned char opt;		/* concurrent to tag.opt (TAG_...) */
    unsigned short int no;	/* tag->comp_no | occurence of TYPE_OF */
    struct tmt *tag, *next;
};

struct bers {
    struct mmodr *odr;
    lua_State *L;
    jmp_buf *jb;
    struct ber stack[BERS_MAX], *top;
    unsigned char *buf, *bp, *endp;
    struct module_id *ext_mid; /* EXTERNAL */
};


/* Error codes */
#define BER_ERRMEM	-1
#define BER_ERRTAG	-10
#define BER_ERRTAGLEN	-11
#define BER_ERRTAGNUM	-12
#define BER_ERRTAGODR	-13
#define BER_ERROID	-20
#define BER_ERROIDMID	-21
#define BER_ERREXT	-30
#define BER_ERREXTOID	-31
#define BER_ERRSTKO	-40
#define BER_ERRSTKU	-41
#define BER_ERRCHCSO	-50
#define BER_ERRLUASTK	-60
#define BER_ERRLUAOUT	-61
#define BER_ERRSIZE	-70
#define BER_ERRODR	-80

const char *
ber_errstr (const int no);
unsigned char
ber_decode (struct bers *bs);
unsigned char
ber_encode (struct bers *bs);

#endif
