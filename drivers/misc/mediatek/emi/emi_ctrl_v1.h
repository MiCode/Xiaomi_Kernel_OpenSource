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

#ifndef __EMI_CTRL_H__
#define __EMI_CTRL_H__

struct emi_info_t {
	unsigned int dram_type;
	unsigned int ch_num;
	unsigned int rk_num;
	unsigned int rank_size[MAX_RK];
};

extern void bwl_init(struct platform_driver *emi_ctrl);
extern void mpu_init(
	struct platform_driver *emi_ctrl, struct platform_device *pdev);
extern void elm_init(
	struct platform_driver *emi_ctrl, struct platform_device *pdev);

extern unsigned int get_dram_type(void);
extern unsigned int get_dram_mr(unsigned int index);
extern unsigned int get_ch_num(void);
extern unsigned int get_rk_num(void);
extern unsigned int get_rank_size(unsigned int rank_index);
extern void __iomem *mt_cen_emi_base_get(void);
extern void __iomem *mt_emi_base_get(void);
extern void __iomem *mt_chn_emi_base_get(unsigned int channel_index);
extern void __iomem *mt_emi_mpu_base_get(void);
extern void resume_decs(void __iomem *CEN_EMI_BASE);

#endif /* __EMI_CTRL_H__ */
