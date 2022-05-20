/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __DRV_CLK_MMDVFS_V3_H
#define __DRV_CLK_MMDVFS_V3_H

#define MAX_MMDVFS_FREQ_NUM	(6)
#define MAX_PWR_NUM		(3)

#define MAX_OPP		(0xff)

#define SPECIAL_NONE		(0)
#define SPECIAL_INDEPENDENCE	(1)

#define IPI_MMUP	(0)
#define IPI_CCU		(1)

#define IPI_TIMEOUT_MS 200U

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs][dbg]%s: "fmt"\n", __func__, ##args)
#define MMDVFS_ERR(fmt, args...) \
	pr_notice("[mmdvfs][err]%s: "fmt"\n", __func__, ##args)

struct mtk_mmdvfs_clk {
	const char *name;
	u8 clk_id;
	u8 user_id;
	u8 pwr_id;
	u8 special_type;
	u8 ipi_type;
	u8 freq_num;
	u8 opp;
	u64 freqs[MAX_MMDVFS_FREQ_NUM];
	struct clk_hw clk_hw;
};
enum {
	ARG_PWR_ID,
	ARG_USER_ID,
	ARG_SPECIAL_TYPE,
	ARG_IPI_TYPE,
	ARG_OPP_TABLE,
	ARG_NUM
};

/* sync with vcp */

enum {
	USER_DISP, // uP
	USER_DISP_AP,
	USER_MDP,
	USER_MML,
	USER_MMINFRA,
	USER_VENC, // uP
	USER_JPEGENC,
	USER_VDEC, // uP
	USER_VFMT,
	// USER_CAM,
	// USER_SENIF,
	// USER_IMG,
	USER_VCORE,
	// USER_VMM,
	USER_NUM
};

enum {
	POWER_VCORE,
	POWER_VMM,
	POWER_NUM
};

enum {
	FUNC_GET_OPP, /* user, user, user */
	FUNC_SET_OPP, /* user, opp */
	FUNC_FORCE_OPP, /* power, opp */
	FUNC_CAMERA_ON, /* on */
	FUNC_LOG,
	FUNC_NUM
};

enum {
	LOG_DEBUG_ON,
	LOG_IPI_IN,
	LOG_IPI_OUT,
	LOG_CLOCK,
	LOG_POWER,
	LOG_HOPPING,
	LOG_NUM
};

struct mmdvfs_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t opp;
	uint8_t ack;
};

#endif /* __DRV_CLK_MMDVFS_V3_H */
