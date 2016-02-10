#ifndef MMODR_H
#define MMODR_H

#include "asn/odr.h"

struct mmodr {
    struct tmt *odrs, *start;
    struct module_id *modules;
    char *names;
    unsigned char nmodules;
};

int mmodr_set (struct mmodr *mo, const void *info, int len);

#endif
