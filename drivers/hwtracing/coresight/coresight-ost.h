/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CORESIGHT_CORESIGHT_OST_H
#define _CORESIGHT_CORESIGHT_OST_H

#include <linux/types.h>
#include <linux/coresight-stm.h>

#if CONFIG_CORESIGHT_OST
static inline bool stm_ost_configured(void) { return 1; }

extern ssize_t stm_ost_packet(struct stm_data *stm_data,
			      unsigned int size,
			      const unsigned char *buf);

extern int stm_set_ost_params(struct stm_drvdata *drvdata,
			      size_t bitmap_size);
#else
static inline bool stm_ost_configured(void) { return 0; }

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
