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
	pr_notice("[mmdvfs]%s: "fmt"\n", __func__, ##args)

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

struct mmdvfs_ipi_data {
	uint8_t func_id;
	uint8_t user_id;
	uint8_t freq_opp;
	uint8_t data_ack;
};

enum {
	FUNC_SET_OPP,
	FUNC_DUMP_OPP,
	FUNC_NUM
};

enum {
	USER_DISP0, // uP
	USER_DISP0_AP,
	USER_DISP1, // uP
	USER_DISP1_AP,
	USER_MDP,
	USER_MML,
	USER_MMINFRA,
	USER_VENC, // uP
	USER_JPEGENC,
	USER_VDEC, // uP
	USER_VFMT,
	USER_CAM,
	USER_SENIF,
	USER_IMG,
	USER_NUM
};

#endif /* __DRV_CLK_MMDVFS_V3_H */
