/* Print about PDU's from odr file */

#include <stdio.h>
#include <stdlib.h>

#define BER_FUN_NAMES
#include "../mmodr.h"


static const char usage[] = "Usage: odr2pdu FILE\n";

static struct mmodr odr;

static void
err_quit (const char *msg)
{
    fprintf (stderr, msg);
    exit (EXIT_FAILURE);
}

/* Print information about components */
static void
odrNames (struct tmt *t)
{
#define ODR_STACKSIZ 40
    struct tmt *stack[ODR_STACKSIZ];
    int level = 0, i, recurs;

    for (; ; ) {
	recurs = 0;
	putchar ('\n');
	for (i = level; i; --i) putchar ('\t');
	printf ("^%d [0x%x] %s ",
	 t->comp_no, t->u.cn, odr.names + t->nameaddr);
	if (t->opt & TAG_OPTIONAL) printf ("OPT. ");
	if (t->opt & TAG_TYPE_OF) printf ("OF ");
	if (!(t->opt & TAG_SIMPLE)) {
	    if (level)
		for (i = level - 1; i; --i)
		    if (t == stack[i]) {
			recurs = 1;
			break;
		    }
	    if (!recurs) {
	        if (!t->u.cn || (t->opt & TAG_CHOICE))
		    printf ("CHOICE ");
	        putchar ('{');
		if (level >= ODR_STACKSIZ)
		    err_quit ("Odrs stack overflow\n");
		stack[level++] = t;
		t = odr.odrs + t->subaddr;
		continue;
	    } else printf ("LOOP (level %d)", i);
	} else
	    if (t->subaddr < sizeof (ber_fun_names))
		printf ("[%s]", ber_fun_names[t->subaddr]);
	    else printf ("[%d?] tag.opt=%d", t->subaddr, t->opt);
	if (!t->comp_next || recurs) {
	    do
		if (--level >= 0) putchar ('}');
		else return;
	    while (!stack[level]->comp_next);
	    t = stack[level];
	}
	t = odr.odrs + t->comp_next;
    }
}

static void
odrModules ()
{
    struct module_id *mid;
    int i, num, sub;

    for (mid = odr.modules; (void *) mid != odr.names; ++mid) {
	printf ("%c%d %s", (odr.odrs + mid->addr == odr.start) ? '>' : '~',
	 mid - odr.modules, odr.names + mid->nameaddr);
	if (mid->oid[0] > 1) {
	    num = mid->oid[1];
	    sub = num / 40;
	    if (sub > 2) sub = 2;
	    num = num - sub * 40;
	    printf (" {%d.%d", sub, num);
	    for (i = 2, sub = 0; i <= mid->oid[0]; ++i) {
		for (num = 0; mid->oid[i] & 0x80; ++i, num <<= 7)
		    num |= mid->oid[i] & 0x7F;
		num |= mid->oid[i];
		printf (".%d", num);
	    }
	    putchar ('}');
	}
	odrNames (odr.odrs + mid->addr);
	printf ("\n\n");
    }
}

static int
file_odr_open (struct mmodr *mo, const char *file)
{
    FILE *fodr = fopen (file, "rb");
    if (fodr && !fseek (fodr, 0, SEEK_END)) {
	int len = ftell (fodr);
	if (len != -1 && !fseek (fodr, 0, SEEK_SET)) {
	    void *mp = malloc (len);
	    if (mp) {
		len = fread (mp, 1, len, fodr);
		fclose (fodr);
		if (len) return mmodr_set (mo, mp, len);
		free (mp);
	    }
	}
    }
    return -1;
}

int
main (int argc, char *argv[])
{
    if (argc < 2) err_quit (usage);
    if (file_odr_open (&odr, argv[1]))
	err_quit ("bad odr file\n");
    odrModules ();
    free (odr.odrs);
    return EXIT_SUCCESS;
}
