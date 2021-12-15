/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef GT9XX_MID_H
#define GT9XX_MID_H

struct ctp_cfg {
	unsigned char lens[6];
	unsigned char *info[6];
};

struct mid_cfg_data {
	char name[32];
	struct ctp_cfg *cfg;
};

#endif
