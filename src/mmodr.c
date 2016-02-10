/* Set mmodr */

#include <string.h>	/* strcmp */

#include "mmodr.h"

int
mmodr_set (struct mmodr *mo, const void * const p, int len)
{
    if (len > (int) sizeof (struct odr_info)) {
	const struct odr_info * const info = p;

	mo->odrs = (struct tmt *) info;
	mo->start = mo->odrs + info->start;
	mo->modules = (struct module_id *) (mo->odrs + info->nodrs);
	mo->names = (char *) (mo->modules + info->nmodules);
	mo->nmodules = (char) info->nmodules;

	return
	 !(len >= (mo->names - (char *) info) + (int) sizeof (ODR_NAME_STUB)
	 && !strcmp (mo->names, ODR_NAME_STUB));
    }
    return -1;
}

