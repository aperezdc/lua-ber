#ifndef MAP_H
#define MAP_H

int odr_add (const struct tmt *t);
int name_add (const char *s);
void names_del (void);
struct module *module_add (const int opt, const char *mname);
void module_del (struct module *mdel);
struct def *def_add (struct module *m, const int opt, const char *dname);
void def_del (struct module *m, struct def *dend, const unsigned char leave);
void def_req (struct def *fdef, const union comp_addr u, const unsigned char opt);
void *find (const char *name, void *i, const void *end);

#endif
