/* Test BER */

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lauxlib.h"
#include "lualib.h"

#include "ber.h"

#define BUF_SIZ		BUFSIZ

static lua_State *L;
static struct bers bs;
static struct mmodr odr;

static const char usage[] = "Usage: %s -f FILE -l FILE\n"
    "\t-f - odr file\n"
    "\t-l - lua file\n";
static char *progname, *odrfile, *luafile;

static void
err_quit (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
    putc ('\n', stderr);
    _exit (EXIT_FAILURE);
}

static void
event_loop ()
{
    unsigned char buffer[BUF_SIZ];
    int i, tl = 0;

    bs.L = L;
    bs.bp = bs.buf = buffer;

#if LUA_VERSION_NUM < 503
    lua_pushstring (bs.L, "QI");
#endif

    do {
	i = read (0, bs.bp, BUF_SIZ - tl);
	if (!i) return;
	bs.endp = bs.bp + i;
	bs.bp = bs.buf;

	i = ber_decode (&bs);
	if (bs.bp < bs.endp) {
	    tl = bs.endp - bs.bp;
	    memcpy (bs.buf, bs.bp, tl);
	    bs.bp = bs.buf + tl;
	} else {
	    tl = 0;
	    bs.bp = bs.buf;
	}
    } while (i == BER_INCOMPL);

#if LUA_VERSION_NUM >= 503
    lua_setglobal (bs.L, "QI");
#else
    lua_rawset (bs.L, LUA_GLOBALSINDEX);
#endif

    luaL_dofile (L, luafile);
    lua_settop (L, 0);

    bs.endp = bs.buf + BUF_SIZ;

#if LUA_VERSION_NUM >= 503
    lua_getglobal(bs.L, "QO");
#else
    lua_pushstring (bs.L, "QO");
    lua_rawget (bs.L, LUA_GLOBALSINDEX);
#endif

    while ((i = ber_encode (&bs)) == BER_INCOMPL) {
	if (!(i = write (1, bs.buf, bs.bp - bs.buf)))
	    return;
	bs.bp = bs.buf;
    }
    write (1, bs.buf, bs.bp - bs.buf);
}

/* Initialize Lua */
static void
lua_init ()
{
    L = luaL_newstate ();
    if (!L) err_quit ("Cannot init Lua");
    luaL_openlibs (L);
    lua_settop (L, 0);
}

/* Parse command line */
static void
get_args (int argc, char *argv[])
{
    int i;

    progname = argv[0];
    if (argc > 1) for (i = 1; i < argc; ++i) {
	if (argv[i][0] == '-')
	    switch (argv[i][1]) {
	    case 'f':
		if (argv[i][2]) odrfile = &argv[i][2];
		else if (++i < argc) odrfile = argv[i];
		break;
	    case 'l':
		if (argv[i][2]) luafile = &argv[i][2];
		else if (++i < argc) luafile = argv[i];
		break;
	    }
	else err_quit (usage, progname);
    }
    if (!(odrfile && luafile))
	err_quit (usage, progname);
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
    jmp_buf jb;
    int ret;

    get_args (argc, argv);
    if (file_odr_open (&odr, odrfile))
	err_quit ("bad odr file");
    bs.odr = &odr;
    lua_init ();

    bs.jb = &jb;
    ret = setjmp (jb);
    if (!ret) event_loop ();
    else err_quit ("Error: %s", ber_errstr (ret));

    free (odr.odrs);
    lua_close (L);
    return EXIT_SUCCESS;
}
