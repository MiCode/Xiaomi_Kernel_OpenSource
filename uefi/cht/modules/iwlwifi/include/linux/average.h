/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_BUILD_AVERAGE
#include_next <linux/average.h>
#else
#undef ewma_init
#define ewma_init LINUX_BACKPORT(ewma_init)
#undef ewma_add
#define ewma_add LINUX_BACKPORT(ewma_add)
#include <linux/backport-average.h>
#endif /* CPTCFG_BPAUTO_BUILD_AVERAGE */
