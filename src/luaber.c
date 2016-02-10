/* Lua BER library */

#include <setjmp.h>
#include <string.h>
#include <stdlib.h>

#include <lauxlib.h>
#include <lua.h>

#include "ber.h"
#include "ber_util.h"

typedef struct bers *p_bers;
typedef struct mmodr *p_mmodr;

#define BERHANDLE	"bers*"
#define ODRHANDLE	"mmodr*"

#define BUF_SIZ		BUFSIZ /* encode out chunk size */

/*
 * Arguments: odr_udata
 * Returns: ber_udata, thread
 */
static int
lber_ber (lua_State *L)
{
    p_mmodr mo = lua_touserdata (L, 1); /* ODRHANDLE */
    p_bers bs = lua_newuserdata (L, sizeof (struct bers));
    luaL_getmetatable (L, BERHANDLE);
    lua_setmetatable (L, -2);
    memset (bs, 0, sizeof (struct bers));
    bs->odr = mo;
    bs->L = lua_newthread (L);
    return 2;
}

/*
 * Arguments: ber_udata
 */
static int
lber_clear (lua_State *L)
{
    p_bers bs = lua_touserdata (L, 1); /* BERHANDLE */
    p_mmodr mo = bs->odr;
    lua_State *bsL = bs->L;
    lua_settop (bsL, 0);
    memset (bs, 0, sizeof (struct bers));
    bs->odr = mo;
    bs->L = bsL;
    return 0;
}

/*
 * Arguments: ber_udata, string
 * Returns: tail (string), table
 *          nil, errcode
 */
static int
lber_decode (lua_State *L)
{
    p_bers bs = lua_touserdata (L, 1); /* BERHANDLE */
    void *strp = NULL;
    size_t str_len;
    jmp_buf jb;
    unsigned char c;
    int res;

    luaL_checktype (L, 2, LUA_TSTRING);
    strp = (void *) lua_tolstring (L, 2, &str_len);

    bs->jb = &jb;
    bs->bp = bs->buf = strp;
    bs->endp = (unsigned char *) strp + str_len;
    res = setjmp (jb);
    if (!res) c = ber_decode (bs);
    else {
	lua_pushnil (L);
	lua_pushinteger (L, res);
	return 2;
    }
    /* tail */
    res = bs->endp - bs->bp;
    if (res < 0) res = 0;
    lua_pushlstring (L, (char *) bs->bp, res);
    /* complete? */
    if (!c || c == BER_MORE) {
	lua_xmove (bs->L, L, 1);
	return 2;
    }
    return 1;
}

/*
 * Arguments: ber_udata, table
 * Returns: string, [boolean (complete?)]
 *          nil, errcode
 */
static int
lber_encode (lua_State *L)
{
    p_bers bs = lua_touserdata (L, 1); /* BERHANDLE */
    unsigned char buffer[BUF_SIZ];
    jmp_buf jb;
    int res;

    if (!lua_istable (L, 2))
	luaL_argerror (L, 2, "Table_out expected");
    /* set table in thread */
    if (!lua_gettop (bs->L))
	lua_xmove (L, bs->L, 1);

    bs->jb = &jb;
    bs->bp = bs->buf = buffer;
    bs->endp = buffer + BUF_SIZ;
    res = setjmp (jb);
    if (!res) {
	unsigned char c = ber_encode (bs);
	lua_pushlstring (L, (char *) buffer, bs->bp - buffer);
	lua_pushboolean (L, !c);
    } else {
        lua_pushnil (L);
        lua_pushinteger (L, res);
    }
    return 2;
}

/*
 * Returns: odr_udata
 */
static int
lodr_new (lua_State *L)
{
    p_mmodr mo = lua_newuserdata (L, sizeof (struct mmodr));
    luaL_getmetatable (L, ODRHANDLE);
    lua_setmetatable (L, -2);
    mo->odrs = NULL;
    return 1;
}

/*
 * Arguments: odr_udata, string
 * Returns: boolean
 *          nil, errcode
 */
static int
lodr_set (lua_State *L)
{
    p_mmodr mo = lua_touserdata (L, 1); /* ODRHANDLE */
    size_t str_len = 0;
    const char *str = luaL_checklstring (L, 2, &str_len);

    if (!mmodr_set (mo, str, str_len)) {
	lua_pushboolean (L, 1);
	return 1;
    } else {
        lua_pushnil (L);
        lua_pushinteger (L, BER_ERRODR);
        return 2;
    }
}

/*
 * Arguments: odr_udata
 * Returns: table {name => oid}
 */
static int
lodr_names (lua_State *L)
{
    struct module_id *mid;
    p_mmodr mo = lua_touserdata (L, 1); /* ODRHANDLE */
    lua_newtable (L);
    for (mid = mo->modules; (void *) mid != mo->names; ++mid)
	if (mid->oid[0] > 1) {
	    lua_pushstring (L, mo->names + mid->nameaddr);
	    lua_pushlstring (L, (char *) mid->oid + 1, mid->oid[0]);
	    lua_rawset (L, -3);
	}
    return 1;
}

/*
 * Arguments: odr_udata, oid
 * Returns: string
 *          nil, errcode
 */
static int
lodr_oid2name (lua_State *L)
{
    p_mmodr mo = lua_touserdata (L, 1); /* ODRHANDLE */
    size_t len = 0;
    const char *oidp = luaL_checklstring (L, 2, &len);
    unsigned char oid[OIDSIZ];
    struct module_id *mid;
    int res;

    if (len > OIDSIZ - 1) {
        lua_pushnil (L);
        lua_pushinteger (L, BER_ERROID);
	return 2;
    }
    memcpy (oid + 1, oidp, oid[0] = len++);
    /* binary search of module by oid */
    {
	struct module_id *mbeg = mo->modules;
	struct module_id *mend = mbeg + mo->nmodules - 1;
	do {
	    mid = mbeg + ((mend - mbeg) >> 1);
	    res = *oid - *mid->oid;
	    if (!res) res = memcmp (oid, mid->oid, len);
	    if (!res) break;
	    if (res < 0) mend = mid - 1;
	    else mbeg = mid + 1;
	} while (mbeg <= mend);
    }
    if (!res) {
	lua_pushstring (L, mo->names + mid->nameaddr);
	return 1;
    }
    return 0;
}

/*
 * Arguments: [number]
 * Returns: string
 */
static int
lber_strerror (lua_State *L)
{
    int err = luaL_checkinteger (L, 1);
    lua_pushstring (L, ber_errstr (err));
    return 1;
}


static luaL_Reg odrmeth[] = {
    {"set",		lodr_set},
    {"ber",		lber_ber},
    {"names",		lodr_names},
    {"oid2name",	lodr_oid2name},
    {NULL, NULL}
};

static luaL_Reg bermeth[] = {
    {"clear",		lber_clear},
    {"decode",		lber_decode},
    {"encode",  	lber_encode},
    {NULL, NULL}
};

static luaL_Reg berlib[] = {
    {"odr",		lodr_new},
    {"oid2str",		oid2str},
    {"str2oid",		str2oid},
    {"num2bitstr",	num2bitstr},
    {"bitstr2num",	bitstr2num},
    {"strerror",	lber_strerror},
    {NULL, NULL}
};


static void
register_functions (lua_State *L, const luaL_Reg *l)
{
    for (; l->name; l++) {
        lua_pushcfunction (L, l->func);
        lua_setfield (L, -2, l->name);
    }
}


static void
createmeta (lua_State *L)
{
    luaL_newmetatable (L, ODRHANDLE);
    lua_pushliteral (L, "__index");
    lua_pushvalue (L, -2);  /* push metatable */
    lua_rawset (L, -3);  /* metatable.__index = metatable */
    register_functions (L, odrmeth);
    lua_pop (L, 1);

    luaL_newmetatable (L, BERHANDLE);
    lua_pushliteral (L, "__index");
    lua_pushvalue (L, -2);  /* push metatable */
    lua_rawset (L, -3);  /* metatable.__index = metatable */
    register_functions (L, bermeth);
    lua_pop (L, 1);
}

/* Open BER library */
LUALIB_API int
luaopen_ber (lua_State *L)
{
    lua_newtable (L);
    register_functions (L, berlib);
    createmeta (L);
    return 1;
}
