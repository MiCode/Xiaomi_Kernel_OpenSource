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

#ifndef __DRAMC_COMMON_H__
#define __DRAMC_COMMON_H__

extern void __iomem *SLEEP_BASE_ADDR;

int acquire_dram_ctrl(void);
int release_dram_ctrl(void);
void __iomem *get_dbg_info_base(unsigned int key);
unsigned int get_dbg_info_size(unsigned int key);

#endif /* __DRAMC_COMMON_H__ */
