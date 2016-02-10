/* LuaBER utilities */

#include <ctype.h>	/* isdigit */
#include <string.h>	/* memmove */

#include <lauxlib.h>


#define atod(cp, num)							\
    while (isdigit (*(cp)))						\
	(num) = ((num) << 3) + ((num) << 1) + (*((cp)++) & ~'0');

#define bytes_count(x)							\
    !(x) ? 0 : ((x) & 0xFF000000) ? 4 : ((x) & 0xFF0000) ? 3		\
     : ((x) & 0xFF00) ? 2 : 1


static unsigned int
reverse (register unsigned int x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    return x;
}

static int
dtoa (char *s, int len, int num)
{
    if (!len) return 0;
    if (!num) {
	*s = '0';
	len = 1;
    } else {
	int i = len;
	do {
	    s[--i] = (num % 10) | '0';
	    num /= 10;
	} while (num && i > 0);
	len -= i;
	memmove (s, &s[i], len);
    }
    return len;
}

/*
 * Arguments: string
 * Returns: string
 */
int
oid2str (lua_State *L)
{
#define OID_STRSIZE	32
    char oid[OID_STRSIZE];
    unsigned int num;
    unsigned char sub, oidi = 0;
    size_t len = 0;
    const char *oidp = lua_tolstring (L, -1, &len);

    if (!oidp) return 0;
    num = *oidp++;
    sub = num / 40;
    if (sub > 2) sub = 2;
    num = num - sub * 40;
    oidi += dtoa (&oid[oidi], OID_STRSIZE - oidi, sub);
    for (; ; ) {
	oid[oidi++] = '.';
	oidi += dtoa (&oid[oidi], OID_STRSIZE - oidi, num);
	if (!--len || oidi >= OID_STRSIZE) break;
    	for (num = 0; *oidp & 0x80; --len, ++oidp, num <<= 7)
	    num |= *oidp & 0x7F;
	num |= *oidp++;
    }
    lua_pushlstring (L, oid, oidi > OID_STRSIZE ? OID_STRSIZE : oidi);
    return 1;
}

/*
 * Arguments: string
 * Returns: string
 */
int
str2oid (lua_State *L)
{
#define OID_SIZE	24
    unsigned int num = 0, number;
    unsigned char sub = 0, oid[OID_SIZE], *oidp = oid;
    const char *strp = lua_tostring (L, -1);

    if (!strp) return 0;
    atod (strp, sub);
    if ((sub > 2) || (*strp++ != '.')) return 0;
    atod (strp, num);
    *oidp++ = sub * 40 + num;
    num = 0;
    while (*strp == '.') {
	++strp;
	atod (strp, num);
	number = num & 0x7F;
	num >>= 7;
	for (sub = 1; num; ++sub) {
	    number <<= 8;
	    number |= (unsigned char) num | 0x80;
	    num >>= 7;
	}
	/* buffer overflow? */
	if (oidp > oid + OID_SIZE - sub) return 0;
	for (; sub; --sub, number >>= 8)
	    *oidp++ = (unsigned char) number;
    }
    lua_pushlstring (L, (char *) oid, oidp - oid);
    return 1;
}


/*
 * Arguments: string
 * Returns: number
 */
int
bitstr2num (lua_State *L)
{
    lua_pushnumber (L,
     (lua_Number) reverse (*((unsigned int *) luaL_checkstring (L, 1))));
    return 1;
}

/*
 * Arguments: number
 * Returns: string
 */
int
num2bitstr (lua_State *L)
{
    unsigned int num = reverse ((int) lua_tonumber (L, 1));
    lua_pushlstring (L, (char *) &num, bytes_count(num));
    return 1;
}

