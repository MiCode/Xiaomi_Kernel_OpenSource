/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
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
	unsigned short int
		larb_port_backup[SMI_LARB_PORT_NR_MAX * SMI_LARB_NR_MAX];
};

struct mtk_smi_priv {
	enum mtk_platform plat;
	/* the port number in each larb */
	unsigned int larb_port_num[SMI_LARB_NR_MAX];
	unsigned char larb_vc_setting[SMI_LARB_NR_MAX];
	void (*init_setting)(struct mtk_smi_data *smidev, bool *default_saved,
		u32 *default_smi_val, unsigned int larbid);
	void (*vp_setting)(struct mtk_smi_data *smidev);
	void (*vp_wfd_setting)(struct mtk_smi_data *smidev);
	void (*vr_setting)(struct mtk_smi_data *smidev);
	void (*hdmi_setting)(struct mtk_smi_data *smidev);
	void (*hdmi_4k_setting)(struct mtk_smi_data *smidev);
};

extern const struct mtk_smi_priv smi_mt8173_priv;

#endif
