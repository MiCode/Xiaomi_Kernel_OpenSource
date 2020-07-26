/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef MSM_GSI_H
#define MSM_GSI_H
#include <linux/types.h>
#include <linux/interrupt.h>

enum gsi_chan_dir {
	GSI_CHAN_DIR_FROM_GSI = 0x0,
	GSI_CHAN_DIR_TO_GSI = 0x1
};

/**
 * @GSI_USE_PREFETCH_BUFS: Channel will use normal prefetch buffers if possible
 * @GSI_ESCAPE_BUF_ONLY: Channel will always use escape buffers only
 * @GSI_SMART_PRE_FETCH: Channel will work in smart prefetch mode.
 *	relevant starting GSI 2.5
 * @GSI_FREE_PRE_FETCH: Channel will work in free prefetch mode.
 *	relevant starting GSI 2.5
 */
enum gsi_prefetch_mode {
	GSI_USE_PREFETCH_BUFS = 0x0,
	GSI_ESCAPE_BUF_ONLY = 0x1,
	GSI_SMART_PRE_FETCH = 0x2,
	GSI_FREE_PRE_FETCH = 0x3,
};

#endif
