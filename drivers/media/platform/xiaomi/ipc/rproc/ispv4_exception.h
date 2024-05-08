#ifndef __ISPV4_HANDEXP_H_
#define __ISPV4_HANDEXP_H_

#include "ispv4_rproc.h"

int ispv4_mbox_excep_boot(struct xm_ispv4_rproc *rp);
void ispv4_mbox_excep_deboot(struct xm_ispv4_rproc *rp);

void register_exception_cb(struct xm_ispv4_rproc *rp,
			   int (*cb)(void *, void *, int),
			   void *priv);
#endif

