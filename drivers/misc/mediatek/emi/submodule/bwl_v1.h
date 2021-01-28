/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
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
