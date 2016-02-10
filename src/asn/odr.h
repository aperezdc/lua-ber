#ifndef ODR_H
#define ODR_H

#define ODR_NAME_STUB	"NIL"

#define COMP_START_NUM	1 /* for Lua arrays */

enum ber_fun {FUN_OCT, FUN_BIT, FUN_OID, FUN_INT, FUN_BOOL,
		FUN_NULL, FUN_EXT_DREF, FUN_EXT_ASN, FUN_OCT_SKIP};
#ifdef BER_FUN_NAMES
char *ber_fun_names[] = {"Oct", "Bit", "OID", "Int", "Bool",
		"Null", "Ext_DRef", "Ext_ASN"};
#endif

struct odr_info {
    unsigned short start; /* first searching tmt in odrs area */
    unsigned short nodrs, nmodules; /* count of modules have ModuleId */
};

typedef int	tag_id_t;
union tag_id {
    tag_id_t cn;
    struct {
#define CLASS_UNIVERSAL		0
#define CLASS_APPLICATION	64
#define CLASS_CONTEXT		128
#define CLASS_PRIVATE		192
	unsigned char classnum;
#define CLASS_NUMSIZ	sizeof (tag_id_t) - sizeof (char)
	unsigned char number[CLASS_NUMSIZ];
    } id;
};

struct tmt {
    union tag_id u;
#define TAG_TWO_WORDS	1
#define TAG_IMPLICIT	2
#define TAG_SIMPLE	4
#define TAG_DEFINITION	8
#define TAG_CHOICE	16
#define TAG_COMPONENTS	32
#define TAG_TYPE_OF	64
#define TAG_OPTIONAL	128
    unsigned char opt, comp_no;
    unsigned short subaddr, comp_next, nameaddr;
};

struct module_id {
#define OIDSIZ		12	/* with length byte */
    unsigned char oid[OIDSIZ];	/* oid[0] - length */
    unsigned short addr, nameaddr;
};

#endif
