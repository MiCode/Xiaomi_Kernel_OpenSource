#include "tomcrypt_private.h"

#include <linux/slab.h>

void * stick_kzalloc(size_t s) { return kzalloc(s, GFP_KERNEL); }

void stick_kfree(void *p) { kfree(p); }