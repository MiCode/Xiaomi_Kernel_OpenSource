/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __BWL_H__
#define __BWL_H__

struct scn_name_t {
	char *name;
};

struct scn_reg_t {
	unsigned int offset;
	unsigned int value;
};

extern unsigned int decode_bwl_env(
	unsigned int dram_type, unsigned int ch_num, unsigned int rk_num);
extern unsigned int acquire_bwl_ctrl(void __iomem *LAST_EMI_BASE);
extern void release_bwl_ctrl(void __iomem *LAST_EMI_BASE);

#endif /* __BWL_H__ */
