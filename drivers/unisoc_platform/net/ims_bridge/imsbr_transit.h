/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_TRANSIT_H
#define _IMSBR_TRANSIT_H

#include <linux/sipc.h>

void imsbr_transit_process(struct imsbr_sipc *sipc, struct sblock *blk,
			   bool freeit);
#endif
