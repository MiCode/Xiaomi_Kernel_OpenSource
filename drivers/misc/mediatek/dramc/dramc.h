/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#ifndef __DRAMC_COMMON_H__
#define __DRAMC_COMMON_H__

extern void __iomem *SLEEP_BASE_ADDR;

int acquire_dram_ctrl(void);
int release_dram_ctrl(void);
void __iomem *get_dbg_info_base(unsigned int key);
unsigned int get_dbg_info_size(unsigned int key);

#endif /* __DRAMC_COMMON_H__ */
