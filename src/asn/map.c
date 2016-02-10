/* Utils for ASN.1 compiler */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "asn.h"
#include "map.h"

/* Add type or component to odrs area.
 * Return addr
 */
int
odr_add (const struct tmt *t)
{
    int cur = odrs_next++;

    if (odrs_next >= odrs_size) {
	odrs = realloc (odrs, (odrs_size += 1024) * sizeof (struct tmt));
	if (!odrs) perror ("Odr realloc");
    }
    if (t) odrs[cur] = *t;
    return cur;
}

/* Add string to names area (not duplicate) */
int
name_add (const char *s)
{
    struct comp *cprev = NULL, *c = comp_names, *cnew;
    int i, cur = names_next;

    while (c && (i = strcmp (s, names + c->u.addr)) > 0) {
	cprev = c;
	c = c->next;
    }
    if (c && !i) return c->u.addr;
    cnew = calloc (1, sizeof (struct comp));
    if (!cnew) perror ("Component name malloc");
    cnew->u.addr = cur;
    if (cprev) {
	cnew->next = c;
	cprev->next = cnew;
    } else {
	cnew->next = comp_names;
	comp_names = cnew;
    }
    i = strlen (s);
    names_next += ++i;
    if ((names_next > names_size)
     && !(names = realloc (names, names_size += 4096)))
	perror ("Names realloc");
    strncpy (names + cur, s, i);
    return cur;
}

void
names_del (void)
{
    struct comp *c;

    while ((c = comp_names)) {
	comp_names = c->next;
	free (c);
    }
    free (names);
}

struct module *
module_add (const int opt, const char *mname)
{
    struct module *m;

    if ((m = find (mname, modules, NULL))) {
	if (opt & FORWARD) return m;
	if (m->opt & FORWARD) {
	    m->opt = (m->opt & ~FORWARD) | opt;
	    return m;
	}
	asnError ("Duplicate module '%s'\n", mname);
    }
    m = calloc (1, sizeof (struct module));
    if (!m) perror ("Module malloc");
    strcpy (m->name, mname);
    m->opt = opt;
    m->next = modules;
    modules = m;
    return m;
}

void
module_del (struct module *mdel)
{
    struct module *m = modules;

    def_del (mdel, NULL, 0);
    if (mdel == modules) {
	modules = mdel->next;
    } else {
	while (m->next && m->next != mdel) m = m->next;
	m->next = m->next->next;
	m = m->next;
    }
    if (mdel->opt & FORWARD)
	fprintf (stderr, "Missing module '%s'\n", mdel->name);
    free (mdel);
}

/* Add definition to module */
struct def *
def_add (struct module *m, const int opt, const char *dname)
{
    struct def *d = NULL;

    if ((d = find (dname, (opt & DEF_EXPORT) ? m->exports : m->defn, NULL))) {
	if (opt & FORWARD) return d;
	if (d->opt & FORWARD) {
	    d->opt = (d->opt & ~FORWARD) | opt;
	    return d;
	}
	asnError ("Duplicate definition '%s' in module '%s'\n",
	 dname, m->name);
    }
    d = calloc (1, sizeof (struct def));
    if (!d) perror ("Definition malloc");
    strcpy (d->name, dname);
    d->opt = opt;
#if COMP_START_NUM
    d->tag.comp_no = COMP_START_NUM;
#endif
    if (opt & DEF_EXPORT) m->exports = d;
    else if (opt & DEF_IMPORT) m->imports = d;
    d->next = m->defn;
    m->defn = d;
    return d;
}

/* Delete definition from module.
 * (Module is neccesary to info)
 */
void
def_del (struct module *m, struct def *dend, const unsigned char leave)
{
    struct def *d = m->defn, *ddel, *dhead = NULL, *dprev = NULL;
    struct comp *c;

    while (d != dend) {
	if (d->opt & leave) {
	    if (!dhead) dhead = d;
	    dprev = d;
	    d = d->next;
	    continue;
	}
	while ((c = d->compn)) {
	    d->compn = c->next;
	    free (c);
	}
	if (d->opt & FORWARD)
	    fprintf (stderr, "Missing definition '%s' in module '%s'"
	     " (not [defined | exported])\n", d->name, m->name);
	else if (d->opt & DEF_INCOMPL)
	    fprintf (stderr, "Incomplete definition '%s' in module '%s'\n",
	     d->name, m->name);
	ddel = d;
	d = d->next;
	if (dprev) dprev->next = d;
        free (ddel);
    }
    m->defn = (dhead) ? dhead : dend;
}

/* Set required component */
void
def_req (struct def *fdef, const union comp_addr u, const unsigned char opt)
{
    struct comp *c;

    c = calloc (1, sizeof (struct comp));
    if (!c) perror ("Req.comp malloc");
    c->u = u;
    c->opt = opt;
    c->next = fdef->compn;
    fdef->compn = c;
}

/* Generic search of definitions and modules */
void *
find (const char *name, void *i, const void *end)
{
    if (!i) return NULL;
    while ((i != end) && strcmp ((char *) i, name))
	i = ((struct def *) i)->next;
    return (i == end) ? NULL : i;
}
