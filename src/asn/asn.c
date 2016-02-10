/* ASN.1 Compiler
 * Thanks to Index Data/YAZ
 *
 * Syntax for the ASN.1 supported:
 * file   -> file module
 *         | module
 * module -> name skip DEFINITIONS ::= BEGIN mbody END
 * mbody  -> EXPORTS nlist ;
 *         | EXPORTS ALL ;
 *         | IMPORTS imlist ;
 *         | name ::= tmt
 *         | skip
 * tmt    -> tag mod type
 * type   -> SEQUENCE { sqlist }
 *         | SEQUENCE OF type
 *         | CHOICE { chlist }
 *         | simple { enlist }
 *
 * simple -> INTEGER
 *         | BOOLEAN
 *         | OCTET STRING
 *         | BIT STRING
 *         | EXTERNAL
 *         | name
 * sqlist -> sqlist , name tmt opt
 *         | name tmt opt
 * chlist -> chlist , name tmt 
 *         | name tmt 
 * enlist -> enlist , name (n)
 *         | name (n)
 * imlist -> nlist FROM name
 *           imlist nlist FROM name
 * nlist  -> name
 *         | nlist , name
 * mod    -> IMPLICIT | EXPLICIT | e
 * tag    -> [tclass n] | [n] | e
 * opt    -> OPTIONAL | e
 *
 * name    identifier/token 
 * e       epsilon/empty 
 * skip    tokens skipped
 * n       number
 * tclass  UNIVERSAL | APPLICATION | PRIVATE | e
 */

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "asn.h"
#include "map.h"


static const char usage[] = "Usage: asn2odr [-n] [FILE ...] -s FILE ...\n"
		"\t-n - don't add names (global)\n"
		"\t-s - start file\n";

static FILE *fi, *fo;
static char *file, str[BUFSIZ], *val, type;
static int lineno;
static char implicit_tags, exports_all;

static struct module *mcur; /* current module */
static struct odr_info info;

static char is_sfile; /* start from current file? */
static char is_names = 1; /* add names */


/* Universal tags (simple types) */
static struct def simple_defn[] = {
{"BOOLEAN",		&simple_defn[1],  {{1}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_BOOL, 0, 0}, 0, 0, NULL, NULL},
{"INTEGER",		&simple_defn[2],  {{2}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_INT, 0, 0}, 0, 0, NULL, NULL},
{"BIT\0STRING",		&simple_defn[3],  {{3}, TAG_IMPLICIT | TAG_SIMPLE | TAG_TWO_WORDS, COMP_START_NUM, FUN_BIT, 0, 0}, 0, 0, NULL, NULL},
{"OCTET\0STRING",	&simple_defn[4],  {{4}, TAG_IMPLICIT | TAG_SIMPLE | TAG_TWO_WORDS, COMP_START_NUM, FUN_OCT, 0, 0}, 0, 0, NULL, NULL},
{"ANY",			&simple_defn[5],  {{4}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_OCT, 0, 0}, 0, 0, NULL, NULL},
{"NULL",		&simple_defn[6],  {{5}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_NULL, 0, 0}, 0, 0, NULL, NULL},
{"OBJECT\0IDENTIFIER",	&simple_defn[7],  {{6}, TAG_IMPLICIT | TAG_SIMPLE | TAG_TWO_WORDS, COMP_START_NUM, FUN_OID, 0, 0}, 0, 0, NULL, NULL},
{"REAL",		&simple_defn[8],  {{9}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_OCT, 0, 0}, 0, 0, NULL, NULL},
{"SEQUENCE",		&simple_defn[9],  {{16}, TAG_IMPLICIT | TAG_COMPONENTS, COMP_START_NUM, 0, 0, 0}, 0, 0, NULL, NULL},
{"SET",			&simple_defn[10], {{17}, TAG_IMPLICIT | TAG_COMPONENTS, COMP_START_NUM, 0, 0, 0}, 0, 0, NULL, NULL},
{"EXT_DREF",		&simple_defn[11], {{6}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_EXT_DREF, 0, 0}, 0, 0, NULL, NULL},
{"EXT_ASN",		&simple_defn[12], {{4}, TAG_IMPLICIT | TAG_SIMPLE, COMP_START_NUM, FUN_EXT_ASN, 0, 0}, 0, 0, NULL, NULL},
{"CHOICE",		NULL,		  {{0}, TAG_CHOICE | TAG_COMPONENTS, COMP_START_NUM, 0, 0, 0}, 0, 0, NULL, NULL}
};
static struct def *simple_defn_end = simple_defn
 + sizeof (simple_defn) / sizeof (struct def) - 1;

/* Z39-50 OID classes */
static struct oid oids[] = {
/* Z39-50 {0x2A, 0x86, 0x48, 0xCE, 0x13} */
{"Z39-50-attributeSet",		&oids[1],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 3}},
{"Z39-50-diagnosticFormat",	&oids[2],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 4}},
{"Z39-50-recordSyntax",		&oids[3],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 5}},
{"Z39-50-resourceReport",	&oids[4],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 7}},
{"Z39-50-accessControl",	&oids[5],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 8}},
{"Z39-50-extendedService",	&oids[6],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 9}},
{"Z39-50-userInfoFormat",	&oids[7],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 10}},
{"Z39-50-elementSpec",		&oids[8],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 11}},
{"Z39-50-variantSet",		&oids[9],  {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 12}},
{"Z39-50-schema",		&oids[10], {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 13}},
{"Z39-50-tagSet",		&oids[11], {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 14}},
{"Z39-50-negotiation",		&oids[12], {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 15}},
{"Z39-50-query",		NULL,	   {6, 0x2A, 0x86, 0x48, 0xCE, 0x13, 16}}
};

static struct def *asnType (struct tmt *t);


/* Report error and die */
void
asnError (const char *fmt, ...)
{
    va_list ap;

    fprintf (stderr, "%s:%d: Error in module '%s'\n> ",
     file, lineno, mcur->name);
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
    if (errno) perror ("Error");
    exit (EXIT_FAILURE);
}

/* Report warning and return */
static void
asnWarning (const char *msg)
{
    fprintf (stderr, "%s:%d: Warning: %s\n", file, lineno, msg);
}

/* Moves input file pointer.
 * The globals type and val are set.
 * val holds name, if token is normal identifier name.
 * Sets type to one of:
 *   \0   end-of-file
 *   {    left curly brace 
 *   }    right curly brace
 *   ,    comma
 *   ;    semicolon
 *   (    (n)
 *   [    [n]
 *   :    ::=
 *   n    other token n
 */
static void
lex ()
{
    static char *valend, endchar;

    if (!val) return;
    if (type == 'n' && valend) {
	*valend = endchar;
	val = valend;
    }
    while (isspace (*val)) ++val;
    while (!*val) {
	val = fgets (str, BUFSIZ, fi);
	if (!val) {
	    type = '\0';
	    return;
	}
	++lineno;
	if ((valend = strstr (str, "--"))) *valend = '\0';
	while (isspace (*val)) ++val;
    }
    switch (type = *val) {
    case '{': case '}': case ',': case '[': case ']':
    case ';': case '(': case ')': val++; break;
    case ':': val += 3; break; /* ::= */
    default:
	if (!(valend = strpbrk (val, "\f\r\n\t\v ,:;{}()[]")))
	    valend = val + strlen (val);
	endchar = *valend;
	*valend = '\0';
	if (valend - val > NAMESIZ - 1) {
	    val[NAMESIZ - 1] = '\0';
	    val[NAMESIZ - 2] = '|';
	    asnWarning ("Long name cutted");
	}
	type = 'n';
    }
}

/* Move pointer and expect token t */
static void
lex_expect (const char t)
{
    lex ();
    if (t != type)
	asnError ("Expected %c type, got %c\n", t, type);
}

/* See if token is name; moves pointer and
 * returns 1 if it is; returns 0 otherwise */
static unsigned char
lex_name_move (const char *name)
{
    if (type == 'n' && !strcmp (val, name)) {
	lex ();
	return 1;
    }
    return 0;
}
			    
/* Parses enumerated list - { name1 (n), name2 (n), ... } */
static void
asnEnum ()
{
    if (type != '{') return;
    for (; ; ) {
	lex_expect ('n');
	lex_expect ('(');
	lex_expect ('n');
	lex_expect (')');
	lex ();
	if (type != ',') break;
    }
    if (type != '}')
	asnError ("Missing } in enum list, got %c '%s'\n", type, val);
    lex ();
}

/* Convert values to network byte order */
static int
hton7 (int num, unsigned char *dest, unsigned int max)
{
    unsigned int i, len, number;

    number = num & 0x7F;
    num >>= 7;
    for (len = 1; num; ++len) {
	number <<= 8;
	number |= (unsigned char) num | 0x80;
	num >>= 7;
    }
    if (len > max)
	 asnWarning ("Too long converting number");

    for (i = 0; len; ++i, --len, number >>= 8)
	dest[i] = number;
    return i;
}

/* Parses tag and modifier */
static void
asnMod (struct tmt *t)
{
    t->u.cn = 0;
    if (type == '[') {
	lex ();
	t->u.id.classnum = CLASS_CONTEXT;
	if (type == 'n' && isalpha (*val)) {
	    switch (*val) {
	    case 'U': t->u.id.classnum = CLASS_UNIVERSAL; break;
	    case 'A': t->u.id.classnum = CLASS_APPLICATION; break;
	    case 'P': t->u.id.classnum = CLASS_PRIVATE; break;
	    default:
		asnError ("Bad tag.class: '%s'\n", val);
	    }
	    lex ();
	}
	if (type == 'n' && isdigit (*val)) {
	    int i = atoi (val);
	    if (i >= 31) {
		t->u.id.classnum |= 31;
		hton7 (i, t->u.id.number, CLASS_NUMSIZ);
	    } else t->u.id.classnum |= i;
	} else
	    asnError ("Bad tag.number: '%s'\n", val);
	lex_expect (']');
	lex ();
    }
    t->opt = implicit_tags;
    if (lex_name_move ("EXPLICIT")) t->opt &= ~TAG_IMPLICIT;
    else if (lex_name_move ("IMPLICIT")) t->opt |= TAG_IMPLICIT;
}

/* Parses optional modifier */
static unsigned char
asnOptional ()
{
    if (lex_name_move ("OPTIONAL")) return TAG_OPTIONAL;
    else if (lex_name_move ("DEFAULT")) {
	lex ();
	return TAG_OPTIONAL;
    }
    return 0;
}

/* Parses the Subtype specification.
 * We now it's balanced, i.e. (... ( ... ) .. )
 */
static void
asnSubtypeSpec ()
{
    int level = 1;

    if (type != '(') return;
    lex ();
    while (type && level) {
	if (type == '(') ++level;
	else if (type == ')') --level;
	lex ();
    }
    if (!type) asnError ("Missing ) in SubtypeSpec\n");
}

/* Parses the optional SizeConstraint */
static void
asnSizeConstraint ()
{
    if (lex_name_move ("SIZE"))	asnSubtypeSpec ();
}

/* Set type dependencies */
static struct tmt *
asnTypeDep (struct tmt *t, struct def *d)
{
    unsigned char opt_imask = 0;

    if (!t->u.cn) {
	t->u = d->tag.u;
	t->subaddr = d->tag.subaddr;
    }
    else {
	if ((t->opt & TAG_IMPLICIT) || !d->tag.u.cn)
	    t->subaddr = d->tag.subaddr;
	else {
	    if (d->tag.opt & (TAG_DEFINITION | TAG_SIMPLE)) {
		if (!d->addr) {
		    if (is_names)
			d->tag.nameaddr = name_add (d->name);
		    d->addr = odr_add (&d->tag);
		}
		t->subaddr = d->addr;
	    } else {
		if (is_names)
		    d->tag.nameaddr = name_add (d->name);
		t->subaddr = odr_add (&d->tag);
		t = odrs + t->subaddr;
	    }
	    opt_imask = TAG_IMPLICIT | TAG_SIMPLE;
	}
    }
    t->opt |= d->tag.opt & ~opt_imask;
    return t;
}

/* Parses components.
 * Return address of first component
 */
static int
asnSub ()
{
    static struct def *fdef;
    struct tmt *t;
    /* paddr - address of previous component to set next */
    int paddr = 0, faddr = 0, addr, comp_no = COMP_START_NUM;

    if (type != '{')
	asnError ("Expects { specifier, but got %c\n", type);
    lex ();
    while (type == 'n') {
	union comp_addr ca;
	addr = odr_add (NULL);
	t = odrs + addr;
	if (!paddr) faddr = addr;
	else (odrs + paddr)->comp_next = addr;
	paddr = addr;
	if (is_names)
	    t->nameaddr = name_add (val);
	t->comp_no = comp_no++;
	lex ();
	asnMod (t);
	fdef = asnType (t);
	ca.addr = addr;
	if (fdef) def_req (fdef, ca, 0);
	t->opt |= asnOptional ();
	if (type != ',') break;
	lex ();
    }
    if (type != '}')
	asnError ("Missing } after COMPONENTS list"
	 ", got %c '%s'\n", type, val);
    lex ();
    return faddr;
}

/* Parses ASN.1 type.
 * Return forward definition
 */
static struct def *
asnType (struct tmt *t)
{
    static struct def *d;
    struct def *fdef = NULL;

    if (type != 'n')
	asnError ("Expects type specifier, but got %c\n", type);
    if (!(d = find (val, simple_defn, NULL))) {
	d = def_add (mcur, FORWARD | exports_all, val);
	if (d->opt & DEF_IMPORT) d = d->type;
    }
    if (d->opt & (FORWARD | DEF_INCOMPL)) fdef = d;
    else t = asnTypeDep (t, d);
    if (t->opt & TAG_TWO_WORDS) {
	lex ();
	t->opt &= ~TAG_TWO_WORDS;
    } else if (t->opt & TAG_COMPONENTS) asnSizeConstraint ();
    if (lex_name_move ("DEFINED")) lex_name_move ("BY");
    lex ();
    asnSubtypeSpec ();
    if ((t->opt & TAG_COMPONENTS) && lex_name_move ("OF")) {
	t->opt &= ~TAG_COMPONENTS;
	if (!(t->opt & TAG_IMPLICIT)) {
	    t->subaddr = odr_add (&d->tag);
	    t = odrs + t->subaddr;
	};
	t->opt = (t->opt & ~TAG_IMPLICIT) | TAG_TYPE_OF;
	return asnType (t);
    }
    if (!(t->opt & TAG_COMPONENTS)) asnEnum ();
    else if (!(t->opt & TAG_DEFINITION)) t->subaddr = asnSub ();
    return fdef;
}

/* Parses type definition */
static void
asnForwardTypes (struct def *d)
{
    static struct def *ncdef;
    static struct comp *c;
    static struct tmt *t;

    while ((c = d->compn)) {
	ncdef = NULL;
	if (c->opt & DEF_INCOMPL) {
	    ncdef = c->u.ncdef;
	    t = &ncdef->tag;
	    ncdef->opt &= ~DEF_INCOMPL;
	} else t = odrs + c->u.addr;
	t = asnTypeDep (t, d);
	d->compn = c->next;
	free (c);
	if (ncdef) {
	    if (ncdef->addr) *(odrs + ncdef->addr) = *t;
	    asnForwardTypes (ncdef);
	}
    }
}

/* Parses type definition (top-level).
 * On entry name holds the type we are defining
 */
static void
asnDef (const char *name)
{
    struct def *d, *fdef;
    union comp_addr ca;

    d = def_add (mcur, exports_all | DEF_INCOMPL, name);
    asnMod (&d->tag);
    if (!mcur->id.addr && mcur->id.oid[0]) {
	if (is_names)
	    d->tag.nameaddr = name_add (d->name);
	mcur->id.addr = d->addr = odr_add (&d->tag);
    }
    fdef = asnType (&d->tag);
    d->tag.opt |= TAG_DEFINITION; /* asnType may set to simple type */
    if (d->addr) *(odrs + d->addr) = d->tag;
    ca.ncdef = d;
    if (fdef) def_req (fdef, ca, DEF_INCOMPL);
    else d->opt &= ~DEF_INCOMPL;
    if (d->compn && !fdef) asnForwardTypes (d);
}

/* Parses i-list in "IMPORTS {i-list};" */
static void
asnImports ()
{
    struct module *m;
    struct def *d, *imports_end;

    imports_end = mcur->imports = mcur->exports;
    if (!lex_name_move ("IMPORTS")) return;
    if (type != 'n')
	asnError ("Missing name in IMPORTS list\n");
    while (type == 'n') {
	def_add (mcur, DEF_IMPORT, val);
	lex ();
	if (lex_name_move ("FROM")) {
	    d = mcur->imports;
	    m = module_add (FORWARD, val);
	    while (d != imports_end) {
	        d->type = def_add (m, DEF_EXPORT | FORWARD, d->name);
	        d = d->next;
	    }
	    imports_end = mcur->imports;
	} else if (type != ',') break;
	lex ();
    }
    if (imports_end != mcur->imports)
	asnError ("Missing FROM in IMPORTS list\n");
    else if (type != ';')
	asnError ("Missing ; after IMPORTS list, got %c '%s'\n",
	 type, val);
    lex ();
}

/* Parses e-list in "EXPORTS {e-list};" */
static void
asnExports ()
{
    exports_all = 0;
    if (!lex_name_move ("EXPORTS")) return;
    if (type != 'n')
	asnError ("Missing name in EXPORTS list\n");
    while (type == 'n') {
	if (lex_name_move ("ALL")) {
	    exports_all |= DEF_EXPORT;
	    break;
	}
	def_add (mcur, DEF_EXPORT | FORWARD, val);
	lex ();
	if (type != ',') break;
	lex ();
    }
    if (type != ';')
	asnError ("Missing ; after EXPORTS list, got %c '%s'\n",
	 type, val);
    lex ();
}

/* Parses a module specification.
 * Exports lists, imports lists, and type definitions are handled;
 * other things are silently ignored
 */
static void
asnModuleBody ()
{
    char oval[NAMESIZ];

    asnExports ();
    asnImports ();
    while (type) {
	if (type != 'n') {
	    lex ();
	    continue;
	}
	if (!strcmp (val, "END")) break;
	strcpy (oval, val);
	lex ();
	if (type == ':') {
	    lex ();
	    asnDef (oval);
	} else if (type == 'n') {
	    lex ();
	    if (type) lex ();
	}
    }
}

/* Parses TagDefault section */
static void
asnTagDefault ()
{
    implicit_tags = 0;
    if ((lex_name_move ("IMPLICIT") && (implicit_tags |= TAG_IMPLICIT))
     || lex_name_move ("EXPLICIT"))
	if (!lex_name_move ("TAGS"))
	    asnError ("Bad TagDefault specification");
}

/* Parses Module Identifier section */
static void
asnModuleId (unsigned char *oid)
{
    struct oid *o;
    int i = 0;

    if (type != '{') return;
    lex ();
    if (type == 'n') {
	if (!(o = find (val, oids, NULL)))
	    asnError ("Bad ModuleID Class '%s'\n", val);
	memcpy (oid + 1, o->oid + 1, i = o->oid[0]);
	lex ();
    } else
	asnError ("Bad Module Identifier specification\n");
    while (type == 'n') {
	lex_expect ('(');
	lex ();
	i += hton7 (atoi (val), oid + i + 1, OIDSIZ - i);
	if (i >= OIDSIZ)
	    asnError ("Too long Module Identifier\n");
	lex_expect (')');
	lex ();
    }
    oid[0] = i;
    info.nmodules++;
    if (type != '}')
	asnError ("Missing } after ModuleID, got %c '%s'\n", type, val);
    lex ();
}

/* Parses a collection of module specifications */
static void
asnModules ()
{
    char oval[NAMESIZ];

    lex ();
    while (type == 'n') {
	mcur = module_add (0, val);
	strcpy (oval, val);
	lex ();
	asnModuleId (mcur->id.oid);
	if (is_sfile) {
	    if (!mcur->id.oid[0]) {
		mcur->id.oid[0] = 1;
		info.nmodules++;
	    }
	    is_sfile = 0; /* start from the first module in file */
	}
	if (mcur->id.oid[0] && is_names)
	    mcur->id.nameaddr = name_add (oval);
	while (!lex_name_move ("DEFINITIONS")) {
	    lex ();
	    if (!type) return;
	}
	asnTagDefault ();
	if (type != ':')
	    asnError ("::= expected, got %c '%s'\n", type, val);
	lex ();
	if (!lex_name_move ("BEGIN"))
	    asnError ("BEGIN expected\n");
	asnModuleBody ();
	if (!exports_all)
	    /* defn -> imports -> exports */
	    def_del (mcur, mcur->exports, DEF_INCOMPL | FORWARD);
	else if (mcur->imports) {
	    /* defn -> exports -> imports */
	    mcur->defn = mcur->imports;
	    def_del (mcur, NULL, DEF_INCOMPL | FORWARD);
	    mcur->defn = mcur->exports;
	}
	if (!strcmp (mcur->name, "_USE")) {
	    simple_defn_end->next = mcur->exports;
	    while (simple_defn_end->next)
		simple_defn_end = simple_defn_end->next;
	}
	if (!(mcur->exports || mcur->id.oid[0]))
	    module_del (mcur);
	lex ();
    }
}

/* Parses an ASN.1 specification file */
static void
asnFile ()
{
    lineno = str[0] = 0;
    val = str;
    fi = fopen (file, "r");
    if (!fi) asnError ("Open ASN.1 file '%s'\n", file);
    asnModules ();
    fclose (fi);
}


/* Compare modules */
static int
module_cmp (const void *m1, const void *m2)
{
    return memcmp (((struct module_id *) m1)->oid,
     ((struct module_id *) m2)->oid, OIDSIZ);
}

/* Write output file */
static void
asnOut ()
{
    int i;

    info.nodrs = odrs_next;
    *((struct odr_info *) odrs) = info;
    fo = fopen ("asn.odr", "wb");
    if (!fo) asnError ("Create odr file\n");
    fwrite (odrs, sizeof (struct tmt), odrs_next, fo);
    /* reuse odrs area to sort modules.oid */
    for (i = 0; modules; module_del (modules))
	if (modules->id.oid[0])
	    ((struct module_id *) odrs)[i++] = modules->id;
    /* i == info.nmodules */
    qsort (odrs, i, sizeof (struct module_id), module_cmp);
    fwrite (odrs, sizeof (struct module_id), i, fo);
    fwrite (names, sizeof (char), names_next, fo);
    fclose (fo);
    free (odrs);
    names_del ();
}


int
main (int argc, char *argv[])
{
    int i = 0;

    if (argc < 2)
	fprintf (stderr, usage), exit (EXIT_FAILURE);
    odr_add (NULL); /* module->id.addr > 0! */
    name_add (ODR_NAME_STUB); /* stub */
    while (++i < argc) {
	file = argv[i];
	if (*file == '-') {
	    switch (file[1]) {
	    case 'n':
		is_names = 0;
		break;
	    case 's':
		info.start = odrs_next;
		is_sfile = 1;
		break;
	    }
	    continue;
	}
	asnFile ();
    }
    if (!info.start)
	fprintf (stderr, usage), exit (EXIT_FAILURE);
    asnOut ();
    return EXIT_SUCCESS;
}
