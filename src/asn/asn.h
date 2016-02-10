#ifndef ASN_H
#define ASN_H

#include "odr.h"

#define NAMESIZ 36

struct tmt *odrs;
int odrs_size, odrs_next;

char *names;
int names_size, names_next;

struct oid {
    char name[NAMESIZ];
    struct oid *next;
    unsigned char oid[OIDSIZ]; /* oid[0] - length */
};

union comp_addr {
    int addr;
    struct def *ncdef;
};

/* used to sort component names and odr */
struct comp {
    struct comp *next;
    unsigned char opt; /* DEF_INCOMPL -> u.ncdef */
    union comp_addr u;
} *comp_names; /* head of component names */

struct def {
    char name[NAMESIZ];
    struct def *next;
    struct tmt tag;
    unsigned short int addr;
#define DEF_EXPORT	1
#define DEF_IMPORT	2
#define DEF_INCOMPL	4
#define FORWARD		8
    unsigned char opt;
    struct def *type;
    struct comp *compn;
};

struct module {
    char name[NAMESIZ];
    struct module *next;
    struct module_id id;
    unsigned char opt;
    struct def *defn, *imports, *exports;
} *modules; /* head of modules */


void asnError (const char *fmt, ...);

#endif
