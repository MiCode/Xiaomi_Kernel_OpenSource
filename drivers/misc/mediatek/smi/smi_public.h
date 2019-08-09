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

#ifndef __SMI_PUBLIC_H__
#define __SMI_PUBLIC_H__

#define SMI_LARB0_REG_INDX 0
#define SMI_LARB1_REG_INDX 1
#define SMI_LARB2_REG_INDX 2
#define SMI_LARB3_REG_INDX 3
#define SMI_LARB4_REG_INDX 4
#define SMI_LARB5_REG_INDX 5
#define SMI_LARB6_REG_INDX 6
#define SMI_LARB7_REG_INDX 7
#define SMI_LARB8_REG_INDX 8
#define SMI_LARB9_REG_INDX 9
#define SMI_LARB10_REG_INDX 10
#define SMI_LARB11_REG_INDX 11
#define SMI_PARAM_BUS_OPTIMIZATION	(0xFFF)

struct smi_bwc_scen_cb {
	struct list_head list;
	char *name;
	void (*smi_bwc_scen_cb_handle)(const unsigned int scen);
};

#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
bool smi_mm_clk_first_get(void);
void __iomem *smi_base_addr_get(const unsigned int reg_indx);
int smi_bus_prepare_enable(const unsigned int reg_indx,
	const char *user_name, const bool mtcmos);
int smi_bus_disable_unprepare(const unsigned int reg_indx,
	const char *user_name, const bool mtcmos);
int smi_debug_bus_hang_detect(unsigned int reg_indx, const bool dump,
	const bool gce, const bool m4u);
struct smi_bwc_scen_cb *smi_bwc_scen_cb_register(struct smi_bwc_scen_cb *cb);
#else
#define smi_mm_clk_first_get(void) ((void)0)
#define smi_base_addr_get(reg_indx) ((void)0)
#define smi_bus_prepare_enable(reg_indx, user_name, mtcmos) ((void)0)
#define smi_bus_disable_unprepare(reg_indx, user_name, mtcmos) ((void)0)
#define smi_debug_bus_hang_detect(larb_indx, dump, gce, m4u) ((void)0)
#define smi_bwc_scen_cb_register(cb) ((void)0)
#endif

#endif
