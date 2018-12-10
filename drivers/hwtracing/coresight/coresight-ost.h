/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 */

#ifndef _CORESIGHT_CORESIGHT_OST_H
#define _CORESIGHT_CORESIGHT_OST_H

#include <linux/types.h>
#include <linux/coresight-stm.h>

#if CONFIG_CORESIGHT_OST
static inline bool stm_ost_configured(void) { return true; }

extern ssize_t stm_ost_packet(struct stm_data *stm_data,
			      unsigned int size,
			      const unsigned char *buf);

extern int stm_set_ost_params(struct stm_drvdata *drvdata,
			      size_t bitmap_size);
#else
static inline bool stm_ost_configured(void) { return false; }

static inline ssize_t stm_ost_packet(struct stm_data *stm_data,
				     unsigned int size,
				     const unsigned char *buf)
{
	return 0;
}

static inline int stm_set_ost_params(struct stm_drvdata *drvdata,
				     size_t bitmap_size)
{
	return 0;
}
#endif
#endif
