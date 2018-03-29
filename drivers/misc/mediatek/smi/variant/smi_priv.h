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

#ifndef __SMI_PRIV_H__
#define __SMI_PRIV_H__

#include "smi_reg.h"

#define SMI_LARB_PORT_NR_MAX  21/* Max port num in current platform.*/
struct mtk_smi_priv;

enum mtk_platform {
	MTK_PLAT_MT8173,
	MTK_PLAT_MT8163,
	MTK_PLAT_MT8127,
	MTK_PLAT_MT8167,
	MTK_PLAT_MAX
};

struct mtk_smi_data {
	unsigned int larb_nr;
	struct device *larb[SMI_LARB_NR_MAX];
	struct device *smicommon;
	const struct mtk_smi_priv *smi_priv;
	unsigned long smi_common_base;
	unsigned long larb_base[SMI_LARB_NR_MAX];
	int larbref[SMI_LARB_NR_MAX];

	/*record the larb port register, please use the max value*/
	unsigned short int larb_port_backup[SMI_LARB_PORT_NR_MAX * SMI_LARB_NR_MAX];
};

struct mtk_smi_priv {
	enum mtk_platform plat;
	unsigned int larb_port_num[SMI_LARB_NR_MAX]; /* the port number in each larb */
	unsigned char larb_vc_setting[SMI_LARB_NR_MAX];
	void (*init_setting)(struct mtk_smi_data *, bool *,
				u32 *, unsigned int);
	void (*vp_setting)(struct mtk_smi_data *);
	void (*vp_wfd_setting)(struct mtk_smi_data *);
	void (*vr_setting)(struct mtk_smi_data *);
	void (*hdmi_setting)(struct mtk_smi_data *);
	void (*hdmi_4k_setting)(struct mtk_smi_data *);
};

extern const struct mtk_smi_priv smi_mt8173_priv;
extern const struct mtk_smi_priv smi_mt8127_priv;
extern const struct mtk_smi_priv smi_mt8163_priv;
extern const struct mtk_smi_priv smi_mt8167_priv;
extern bool smi_clk_always_on;

#endif
