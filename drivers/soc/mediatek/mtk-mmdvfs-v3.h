/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __DRV_CLK_MMDVFS_V3_H
#define __DRV_CLK_MMDVFS_V3_H

#include <dt-bindings/clock/mmdvfs-clk.h>

#define MAX_OPP		(6)
#define IPI_TIMEOUT_MS	(200U)

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs][dbg]%s: "fmt"\n", __func__, ##args)
#define MMDVFS_ERR(fmt, args...) \
	pr_notice("[mmdvfs][err]%s: "fmt"\n", __func__, ##args)

struct mtk_mmdvfs_clk {
	const char *name;
	u8 clk_id;
	u8 pwr_id;
	u8 user_id;
	u8 ipi_type;
	u8 spec_type;
	u8 opp;
	u8 freq_num;
	u32 freqs[MAX_OPP];
	struct clk_hw clk_hw;
};

/* vcp/.../mmdvfs_public.h */
enum {
	USER_DISP,
	USER_DISP_AP,
	USER_MDP,
	USER_MML,
	USER_MMINFRA,
	USER_VENC,
	USER_VENC_AP,
	USER_VDEC,
	USER_VDEC_AP,
	USER_IMG,
	USER_CAM,
	USER_VCORE,
	USER_VMM,
	USER_NUM
};

/* vcp/.../mmdvfs_private.h */
enum {
	LOG_DEBUG_ON,
	LOG_IPI_IN,
	LOG_IPI_OUT,
	LOG_CLOCK,
	LOG_POWER,
	LOG_HOPPING,
	LOG_NUM
};

enum {
	FUNC_GET_OPP, /* user, user, user */
	FUNC_SET_OPP, /* user, opp */
	FUNC_FORCE_OPP, /* power, opp */
	FUNC_CAMERA_ON, /* on */
	FUNC_LOG,
	FUNC_NUM
};

struct mmdvfs_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t opp;
	uint8_t ack;
};

#endif /* __DRV_CLK_MMDVFS_V3_H */
