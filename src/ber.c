/* BER octets <-> Lua tables */

#include <string.h>	/* mem* */

#include <lauxlib.h>

#include "ber.h"


#define bytes_count(x)							\
    !(x) ? 0 : ((x) & 0xFF000000) ? 4 : ((x) & 0xFF0000) ? 3		\
     : ((x) & 0xFF00) ? 2 : 1


static int
ber_taglen (struct bers *bs);
static struct ber *
ber_add (struct bers *bs, const unsigned char opt);

/* <<========================================
 * BER (de|en)coders
 */

#define UNUSED(x)	((void) (x))

#define DEN_ENCODE	1
#define DEN_DECODE	2
#define DEN_SIMPLE	4

static int ber_oct (struct bers *bs, int len, unsigned char opt);
static int ber_bit (struct bers *bs, int len, unsigned char opt);
static int ber_oid (struct bers *bs, int len, unsigned char opt);
static int ber_int (struct bers *bs, int len, unsigned char opt);
static int ber_bool (struct bers *bs, int len, unsigned char opt);
static int ber_null (struct bers *bs, int len, unsigned char opt);
static int ber_ext_dref (struct bers *bs, int len, unsigned char opt);
static int ber_ext_asn (struct bers *bs, int len, unsigned char opt);
static int ber_oct_skip (struct bers *bs, int len, unsigned char opt);

static struct {
    int (*fun) (struct bers *bs, int len, unsigned char opt);
    unsigned char berlen_max;
    struct tmt tag;
} simples[] = {
    {ber_oct, 0,	{{4}, TAG_SIMPLE | TAG_COMPONENTS, COMP_START_NUM, FUN_OCT, 0, 0}},
    {ber_bit, 0,	{{3}, TAG_SIMPLE | TAG_COMPONENTS, COMP_START_NUM, FUN_BIT, 0, 0}},
    {ber_oid, OIDSIZ,	{{0}, 0, 0, 0, 0, 0}},
    {ber_int, sizeof (int),	{{0}, 0, 0, 0, 0, 0}},
    {ber_bool, 1,	{{0}, 0, 0, 0, 0, 0}},
    {ber_null, 1,	{{0}, 0, 0, 0, 0, 0}},
    {ber_ext_dref, OIDSIZ,	{{0}, 0, 0, 0, 0, 0}},
    {ber_ext_asn, 0,	{{0}, 0, 0, 0, 0, 0}},
    {ber_oct_skip, 0,	{{0}, TAG_SIMPLE, COMP_START_NUM, FUN_OCT_SKIP, 0, 0}},
};


static int
ber_encstr (struct bers *bs, int pad)
{
    struct ber *b = bs->top;
    int len;
    unsigned char chunk = b->opt & (BER_MORE | BER_INCOMPL);
    size_t string_len = 0;
    const char *string_ptr = lua_tolstring(bs->L, -1, &string_len);

    if (chunk & BER_MORE) {
	len = b->v.size;
	b->opt &= ~BER_MORE;
    } else {
	int lenpad;
	len = (int) string_len;
	if (chunk) len -= b->len;
	else b->len = 0;
	lenpad = len + pad;
#ifdef ENC_SIMPLESZ_MAX
	/* constructed simples */
	if (lenpad > ENC_SIMPLESZ_MAX) {
	    struct tmt *t;
	    lenpad = ENC_SIMPLESZ_MAX;
	    len = ENC_SIMPLESZ_MAX - pad;
	    if (!(chunk & BER_INCOMPL)) {
		tag_id_t cn;
		t = &simples[b->tag->subaddr].tag;
		cn = t->u.cn;
		b->v.bufp = bs->bp;
		{
		    tag_id_t pre = b->u.cn;
		    unsigned char *tagp = bs->bp;
		    tagp -= bytes_count(pre);
		    *tagp |= BER_CONSTR;
		}
		*bs->bp++ = 0x80;
		b->opt |= BER_CONSTR | BER_INCOMPL;
		b = ber_add (bs, DEN_ENCODE);
		b->opt = BER_INCOMPL;
		b->len = 0;
		*((tag_id_t *) bs->bp) = cn;
		bs->bp += bytes_count(cn);
		b->tag = t;
		b->next = NULL;
	    }
	} else if (chunk & BER_INCOMPL)
	    b->opt &= ~BER_INCOMPL;
#endif
	/* set length */
	if (lenpad < 0x80) *bs->bp++ = lenpad;
	else {
	    unsigned int num = lenpad, number = 0;
	    char llen = 0; /* length of length */
	    do {
		number <<= 8;
		number |= (unsigned char) num;
		++llen; num >>= 8;
	    } while (num);
	    *bs->bp++ = BER_INDEFIN | llen;
	    *((tag_id_t *) bs->bp) = number;
	    bs->bp += llen;
	}
	/* set padding bytes */
	if (pad) {
	    memset (bs->bp, 0, pad);
	    bs->bp += pad;
	}
    }
    /* cut to chunks? */
    {
	int bound = bs->endp - bs->bp;
	if (len > bound) {
	    b->v.size = len - bound;
	    len = bound;
	    b->opt |= BER_MORE;
	}
    }
    /* copy (sub)string */
    memcpy (bs->bp, string_ptr + b->len, len);
    bs->bp += len;

    if (b->opt & (BER_MORE | BER_INCOMPL)) {
	b->len += len;
	return BER_MORE;
    }
    return 0;
}

static int
ber_oct (struct bers *bs, int len, unsigned char opt)
{
    if (opt & DEN_DECODE) {
	lua_pushlstring (bs->L, (char *) bs->bp, len);
	bs->bp += len;
	return 0;
    }
    return ber_encstr (bs, 0);
}

static int
ber_bit (struct bers *bs, int len, unsigned char opt)
{
    /* unused bits ignored */
    if (opt & DEN_DECODE) {
	bs->bp[--len] &= 0xFF >> *bs->bp++;
	lua_pushlstring (bs->L, (char *) bs->bp, len);
	bs->bp += len;
	return 0;
    }
    return ber_encstr (bs, 1);
}

static int
ber_oid (struct bers *bs, int len, unsigned char opt)
{
    if (opt & DEN_DECODE) {
	lua_pushlstring (bs->L, (char *) bs->bp, len);
	bs->bp += len;
    } else {
	size_t slen = 0;
	const char *s = lua_tolstring(bs->L, -1, &slen);
	*bs->bp++ = slen;
	memcpy (bs->bp, s, slen);
	bs->bp += slen;
    }
    return 0;
}

static int
ber_int (struct bers *bs, int len, unsigned char opt)
{
    unsigned int i = 0, num = 0;

    if (opt & DEN_DECODE) {
        while (len--) {
	    i <<= 8;
	    i |= *bs->bp++;
	}
	lua_pushnumber (bs->L, i);
    } else {
	i = (int) lua_tonumber (bs->L, -1);
	do {
	    num <<= 8;
	    num |= (unsigned char) i;
	    i >>= 8;
	    ++len;
	} while (i);
	*bs->bp++ = len;
	for (; len--; num >>= 8)
	    *bs->bp++ = (unsigned char) num;
    }
    return 0;
}

static int
ber_bool (struct bers *bs, int len, unsigned char opt)
{
    UNUSED (len);
    if (opt & DEN_DECODE)
	lua_pushboolean (bs->L, *bs->bp++ != 0);
    else {
	*bs->bp++ = 1;
	*bs->bp++ = lua_toboolean (bs->L, -1);
    }
    return 0;
}

static int
ber_null (struct bers *bs, int len, unsigned char opt)
{
    UNUSED (len);
    if (opt & DEN_DECODE) lua_pushboolean (bs->L, 0);
    else *bs->bp++ = 0;
    return 0;
}

static int
ber_ext_dref (struct bers *bs, int len, unsigned char opt)
{
    struct module_id *mid;
    unsigned char oid[OIDSIZ], *oidp = bs->bp;
    int res;

    ber_oid (bs, len, opt);
    /* oidp points to len..content of oid */
    if (opt & DEN_DECODE) {
	if (bs->bp == bs->buf) {
	    memcpy (oid + 1, bs->bp, oid[0] = len);
	    oidp = oid;
	} else --oidp;
    } else len = *oidp;
    ++len;
    /* binary search of module by oid */
    {
	struct module_id *mbeg = bs->odr->modules;
	struct module_id *mend = mbeg + bs->odr->nmodules - 1;
	do {
	    mid = mbeg + ((mend - mbeg) >> 1);
	    res = *oidp - *mid->oid;
	    if (!res) res = memcmp (oidp, mid->oid, len);
	    if (!res) break;
	    if (res < 0) mend = mid - 1;
	    else mbeg = mid + 1;
	} while (mbeg <= mend);
    }
    bs->ext_mid = res ? NULL : mid;
    return 0;
}

static int
ber_ext_asn (struct bers *bs, int len, unsigned char opt)
{
    struct ber *b = bs->top;
    struct module_id *mid = bs->ext_mid;
    UNUSED (len);

    if (opt & DEN_DECODE) {
	b = ber_add (bs, DEN_DECODE);
	b->v.size = b->opt = 0;
	b->next = bs->odr->odrs;
	if (mid) b->next += mid->addr;
    } else {
	if (!mid) longjmp (*bs->jb, BER_ERREXTOID); /* Bad Ext.OID */
	*(bs->bp - 1) |= BER_CONSTR; /* Ext_ASN is constructed */
	b->opt |= BER_CONSTR;
	b->v.bufp = bs->bp;
	*bs->bp++ = 0x80;

	b = ber_add (bs, DEN_ENCODE);
	b->opt = 0;
	b->next = bs->odr->odrs + mid->addr;
	b->no = COMP_START_NUM - 1;
    }
    bs->ext_mid = NULL;
    return BER_INCOMPL;
}

/* Ignore input octets of unknown Ext.ASN */
static int
ber_oct_skip (struct bers *bs, int len, unsigned char opt)
{
    UNUSED (opt);
    lua_pushlstring (bs->L, NULL, 0);
    bs->bp += len;
    return 0;
}

/* ========================================>> */


/* Add ber to bers stack */
static struct ber *
ber_add (struct bers *bs, const unsigned char opt)
{
    if (!bs->top) bs->top = bs->stack;
    else if (++bs->top - bs->stack >= BERS_MAX)
	longjmp (*bs->jb, BER_ERRSTKO); /* Bers stack overflow */
    if ((opt & (DEN_DECODE | DEN_SIMPLE)) == DEN_DECODE)
	lua_newtable (bs->L);
    return bs->top;
}

/* Delete top ber's from stack */
static void
ber_del (struct bers *bs, unsigned char opt)
{
    struct ber *bpr = bs->top;
    int i;

    if (!bpr) longjmp (*bs->jb, BER_ERRSTKU); /* Bers stack underflow */
//fprintf (stderr, "- top=%d\n", bpr - bs->stack);
    if (bpr-- == bs->stack) {
	if (opt & DEN_DECODE)
	    lua_rawseti (bs->L, -2, bs->top->no);
	bs->top = NULL;
	return;
    }
    if (opt & DEN_DECODE) {
	do {
	    struct ber *b;
	    /* End Of Contents */
	    if (bpr->opt & opt & BER_INDEFIN) {
		opt &= ~BER_INDEFIN;
		bpr->v.size += bs->top->v.size;
		--bs->top, --bpr;
	    }
	    b = bs->top;
	    if (b->opt & BER_INCOMPL)
		lua_concat (bs->L, 2);
	    else lua_rawseti (bs->L, -2, b->no);
	    /* elements of type_of | cutted chunks */
	    if ((b->opt & TAG_TYPE_OF) && !b->next) {
		i = bpr->tag->subaddr;
		b->next = (b->opt & BER_INCOMPL)
		 ? &simples[i].tag : bs->odr->odrs + i;
		++b->no;
		b->opt &= BER_INCOMPL | TAG_TYPE_OF;
	    }
	    if (b->opt & TAG_CHOICE) {
		for (; bpr >= bs->stack && !bpr->u.cn; --bpr)
		    lua_rawseti (bs->L, -2, bpr->no);
		if (bpr < bs->stack) break;
		bs->top = bpr + 1;
		*bs->top = *b;
		b = bs->top;
		b->opt &= ~TAG_CHOICE;
	    }
	    i = bpr->len - b->v.size;
	    if (i > 0) return;
	    if (i < 0) {
		if (bpr->opt & BER_INDEFIN) return;
		longjmp (*bs->jb, BER_ERRTAGLEN); /* Bad length */
	    }
	    bpr->v.size += b->v.size;
	    --bs->top, --bpr;
	} while (bs->top > bs->stack);
    } else {
#ifdef ENC_SIMPLESZ_MAX
	if (bpr->opt & BER_INCOMPL)
	    bpr->opt &= ~BER_INCOMPL;
	else
#endif
	    lua_pop (bs->L, 1);
	/* set length */
	if (bpr->opt & BER_CONSTR) {
	    if (bpr->opt & BER_INDEFIN) {
		*bs->bp++ = '\0';
		*bs->bp++ = '\0';
	    } else {
		i = bs->bp - bpr->v.bufp - 1;
		if (i >= 0x80) {
		    struct ber *bi = bpr;
		    while (--bi >= bs->stack
		     && !(bi->opt & BER_INDEFIN))
			bi->opt |= BER_INDEFIN;
		    bpr->opt |= BER_INDEFIN;
    		    *bs->bp++ = '\0';
    		    *bs->bp++ = '\0';
		} else *bpr->v.bufp = i;
	    }
	}
	for (; bpr >= bs->stack && !bpr->u.cn; --bpr)
	    lua_pop (bs->L, 1);
	bs->top = bpr;
    }
    if (bpr < bs->stack) bs->top = NULL;
}


/* Find tmt from choice */
static char
ber_choice (struct bers *bs, const int cn, struct tmt *t)
{
    struct ber *b = bs->top;
    struct tmt *choices[CHOICES_MAX];
    int ch_i = 0;

    while (cn != t->u.cn) {
	if (!t->u.cn) {
	    choices[ch_i++] = t;
	    if (ch_i >= CHOICES_MAX)
		longjmp (*bs->jb, BER_ERRCHCSO); /* Choices stack overflow */
	    t = bs->odr->odrs + t->subaddr;
	    continue;
	}
	if (!t->comp_next) {
	    while (--ch_i >= 0 && !choices[ch_i]->comp_next)
		;
	    if (ch_i < 0) return 0;
	    t = choices[ch_i];
	}
	t = bs->odr->odrs + t->comp_next;
    }
    b->tag = t;
    if (ch_i) {
	struct ber bo = *b;
	unsigned char i;
	int next = choices[0]->comp_next;
	for (i = 0; i < ch_i; ++i) {
	    b->u.cn = 0;
	    b->no = choices[i]->comp_no;
	    b = ber_add (bs, DEN_DECODE);
//fprintf (stderr, "+ >%s addr=%d top=%d\n", bs->odr->names + choices[i]->nameaddr, choices[i] - bs->odr->odrs, b - bs->stack);
	}
	*b = bo;
	b->next = next ? bs->odr->odrs + next : NULL;
	b->opt |= TAG_CHOICE; /* end of choices */
    }
    return 1;
}

/* Find tmt in odrs area */
static void
ber_odr (struct bers *bs)
{
    struct ber *b = bs->top;
    struct tmt *t = b->next;
    const int cn = b->u.cn;
    char fnd = 0;

    if (t == bs->odr->odrs) {
	b->tag = &simples[b->len > 0 ? FUN_OCT_SKIP : FUN_EXT_ASN].tag;
	return;
    }
    b->tag = b->next = NULL;
    if (b->opt & TAG_CHOICE) {
	b->opt &= ~TAG_CHOICE;
	fnd = ber_choice (bs, cn, t);
    } else {
	while (t && cn != t->u.cn
	 && ((t->opt & TAG_OPTIONAL) || !t->u.cn)) {
	    if (!t->u.cn && (fnd = ber_choice (bs, cn, t))) break;
	    t = (t->comp_next) ? bs->odr->odrs + t->comp_next : NULL;
	}
	if (t && cn == t->u.cn) {
	    if (!fnd) {
		b->tag = t;
		fnd = 1;
	    }
	    /* use bs->top - may be added in ber_choice */
	    if (t->comp_next)
		bs->top->next = bs->odr->odrs + t->comp_next;
	}
    }
    if (!fnd) longjmp (*bs->jb, BER_ERRTAGODR); /* Missing odr */
}

/* Set tag and length of contents.
 * Return error code and later check bounds (bp >= endp ?)
 */
static int
ber_taglen (struct bers *bs)
{
    struct ber *b = bs->top;
    unsigned char c;
    unsigned int len;

    if (!(*bs->bp | *(bs->bp + 1))) {
	bs->bp += 2;
	return BER_INDEFIN;
    }
    /* tag & tclass */
    b->opt &= BER_INCOMPL | TAG_CHOICE | TAG_TYPE_OF;
    b->opt |= *bs->bp & BER_CONSTR;
    c = b->u.cn = 0;
    b->u.id.classnum = *bs->bp & ~BER_CONSTR;
    if ((*bs->bp & 0x1F) == 0x1F) {
	++bs->bp;
	for (; (*bs->bp & 0x80) && c < CLASS_NUMSIZ; ++c, ++bs->bp)
	    b->u.id.number[c] = *bs->bp;
        b->u.id.number[c] = *bs->bp;
    }
    ++bs->bp;
    if (c >= CLASS_NUMSIZ)
	return BER_ERRTAGNUM; /* Too long tag.number */

    /* length */
    len = 0;
    if (*bs->bp & BER_INDEFIN) {
	c = *bs->bp++ & ~BER_INDEFIN;
	if (c == 0x7F) c = 0;
	else if (c > sizeof (int))
	    return BER_ERRTAGLEN; /* Too long length of length */
	while (c--) {
	    len <<= 8;
	    len |= *bs->bp++;
	}
	if (!len) b->opt |= BER_INDEFIN;
    } else len = *bs->bp++;
    b->len = len;
    return 0;
}

/* Process input ber octets */
unsigned char
ber_decode (struct bers *bs)
{
    struct ber *b;
    struct tmt *t;
    int i, sub;
    unsigned char c, more;

    if (!bs->top) {
	b = ber_add (bs, DEN_DECODE);
	b->v.size = 0;
	b->opt = 0;
	b->next = bs->odr->start;
    }
    while (bs->top) {
	b = bs->top;
//fprintf (stderr, "bp=0x%x next=%d top=%d\n", bs->bp - bs->buf, b->next - bs->odr->odrs, b - bs->stack);
	more = b->opt & BER_MORE;
	if (more) b->opt &= ~BER_MORE;
	else {
	    unsigned char *bufp = bs->bp;
	    i = ber_taglen (bs);
	    if (bs->bp > bs->endp) {
		bs->bp = bufp;
		return BER_INCOMPL;
	    }
	    b->v.size += bs->bp - bufp;
	    if (i) {
		if (i == BER_INDEFIN) {
		    ber_del (bs, DEN_DECODE | BER_INDEFIN);
		    continue;
		} else longjmp (*bs->jb, i); /* BER_ERRTAG* */
	    }
	    ber_odr (bs);
	    b = bs->top; /* may be added in ber_odr */
	    if (!(b->len || (b->opt & BER_INDEFIN)
	     || b->tag->subaddr == FUN_NULL)) {
		lua_pushnil (bs->L);
		ber_del (bs, DEN_DECODE);
		continue;
	    }
	}
	t = b->tag;
	sub = t->subaddr;
//fprintf (stderr, " >%s addr=%d bp=0x%x\n", bs->odr->names + t->nameaddr, t - bs->odr->odrs, bs->bp - bs->buf);

	if (!(b->opt & TAG_TYPE_OF))
	    b->no = t->comp_no;
	if (t->opt & TAG_SIMPLE) {
	    if (more) /* concat of cutted octets? */
		more = (sub == FUN_OCT || sub == FUN_OCT_SKIP);
	    else /* constructed simples */
		if ((b->opt & BER_CONSTR)
		 && (simples[sub].tag.opt & TAG_COMPONENTS)) {
		    lua_pushlstring (bs->L, NULL, 0);
		    b = ber_add (bs, DEN_DECODE | DEN_SIMPLE);
		    b->v.size = 0;
		    b->opt = BER_INCOMPL | TAG_TYPE_OF;
		    b->next = &simples[sub].tag;
		    b->no = COMP_START_NUM;
		    continue;
		}
	    /* check length */
	    if (sub != FUN_EXT_ASN) {
		i = b->len;
		c = simples[sub].berlen_max;
		if (c && i > c)
		    longjmp (*bs->jb, BER_ERRTAGLEN); /* Too long length */
		if (i > bs->endp - bs->bp) {
		    b->opt |= BER_MORE;
		    /* gather only octet strings */
		    if (sub != FUN_OCT && sub != FUN_OCT_SKIP)
			return BER_INCOMPL;
		    i = bs->endp - bs->bp;
		    b->len -= i;
		}
		b->v.size += i;
	    } else i = 0;
	    c = simples[sub].fun (bs, i, DEN_DECODE);
	    if (more) lua_concat (bs->L, 2);
	    if (b->opt & BER_MORE) return BER_INCOMPL;
	    if (!c) ber_del (bs, DEN_DECODE);
	} else {
	    if (!(b->opt & BER_CONSTR))
		longjmp (*bs->jb, BER_ERRTAG); /* BER is primitive */
	    b = ber_add (bs, DEN_DECODE);
	    b->v.size = 0;
	    b->opt = t->opt & (TAG_CHOICE | TAG_TYPE_OF);
	    b->next = bs->odr->odrs + sub;
	    b->no = COMP_START_NUM;
//fprintf (stderr, "+ top=%d\n", bs->top - bs->stack);
	}
    }
    return (bs->bp < bs->endp) ? BER_MORE : 0;
}

/* Process output lua table */
unsigned char
ber_encode (struct bers *bs)
{
    struct ber *b;
    struct tmt *t;
    int sub, ltp = LUA_TNONE;
    unsigned char chunk, iscons;

    if (!bs->top) {
	b = ber_add (bs, DEN_ENCODE);
	b->opt = 0;
	b->next = bs->odr->start;
	b->no = COMP_START_NUM - 1;
    }
    while (bs->top) {
	b = bs->top;
	t = b->tag;
	chunk = b->opt & (BER_MORE | BER_INCOMPL);
	/* find tmt in odrs area */
	if (!chunk) {
	    lua_rawgeti (bs->L, -1, ++b->no);
	    ltp = lua_type (bs->L, -1);
	    t = b->next;
	    if (b->opt & TAG_TYPE_OF) {
		if (ltp != LUA_TNIL) {
		    b->opt = TAG_TYPE_OF;
		    t = bs->odr->odrs + (b - 1)->tag->subaddr;
		}
	    } else if (t && ltp == LUA_TNIL) {
		int i = t->comp_next;
		do {
		    lua_pop (bs->L, 1);
		    lua_rawgeti (bs->L, -1, ++b->no);
		    ltp = lua_type (bs->L, -1);
		} while (ltp == LUA_TNIL && (i = bs->odr->odrs[i].comp_next));
		t = (i) ? bs->odr->odrs + i : NULL;
	    }
	    if (!t || ltp == LUA_TNIL) {
		lua_pop (bs->L, 1);
		ber_del (bs, DEN_ENCODE);
		continue;
	    }
	    /* set next tmt */
	    b->tag = t;
	    b->next = (!(b->opt & TAG_CHOICE) && t->comp_next)
	     ? bs->odr->odrs + t->comp_next : NULL;
	}
	sub = t->subaddr;
	iscons = !(t->opt & TAG_SIMPLE);

	/* Tag */
	if (!(chunk & BER_MORE) && t->u.cn) {
	    tag_id_t cn = b->u.cn = t->u.cn;
	    cn |= (iscons) ? BER_CONSTR : 0;
	    *((tag_id_t *) bs->bp) = cn;
	    //for (; cn; cn >>= 8) ++bs->bp;
	    bs->bp += bytes_count(cn);
	    if (iscons) {
		b->v.bufp = bs->bp;
		*bs->bp++ = 0x80;
		b->opt |= BER_CONSTR;
	    }
	}
	/* Content */
//fprintf (stderr, ">%s\n", bs->odr->names + b->tag->nameaddr);
	if (iscons) {
	    if (ltp != LUA_TTABLE)
		longjmp (*bs->jb, BER_ERRLUAOUT); /* Bad PDU */
	    b = ber_add (bs, DEN_ENCODE);
	    b->opt = t->opt & (TAG_CHOICE | TAG_TYPE_OF);
	    b->next = bs->odr->odrs + sub;
	    b->no = COMP_START_NUM - 1;
//fprintf (stderr, "+ top=%d\n", b - bs->stack);
	} else
	    if (!simples[sub].fun (bs, 0, DEN_ENCODE))
		lua_pop (bs->L, 1);
	/* Buffer overflow? */
	if (bs->endp - bs->bp < ENC_BUFRESERVE) {
	    for (b = bs->top; b >= bs->stack
	     && !(b->opt & BER_INDEFIN); --b)
		b->opt |= BER_INDEFIN;
	    return BER_INCOMPL;
	}
    }
    return 0;
}

const char *
ber_errstr (const int no)
{
    switch (no) {
    case BER_ERRMEM:	return "memory";
    case BER_ERRTAG:	return "bad tag";
    case BER_ERRTAGNUM:	return "bad tag.number";
    case BER_ERRTAGLEN:	return "bad tag.length";
    case BER_ERRTAGODR:	return "bad tag.odr";
    case BER_ERROIDMID:	return "unknown ModuleID";
    case BER_ERROID:	return "bad OID";
    case BER_ERREXT:	return "bad External";
    case BER_ERREXTOID:	return "bad Ext.OID";
    case BER_ERRSTKO:	return "bers stack overflow";
    case BER_ERRSTKU:	return "bers stack underflow";
    case BER_ERRCHCSO:	return "choices stack overflow";
    case BER_ERRLUASTK:	return "bad Lua stack";
    case BER_ERRLUAOUT:	return "bad encode PDU";
    case BER_ERRSIZE:	return "transfer limit";
    case BER_ERRODR:	return "bad odr file";
    default:		return "unknown error";
    }
}
