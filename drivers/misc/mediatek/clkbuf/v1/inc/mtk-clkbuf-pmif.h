/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#ifndef CLKBUF_PMIF_H
#define CLKBUF_PMIF_H

#include "mtk_clkbuf_common.h"

enum PMIF_INF {
	PMIF_CONN_INF,
	PMIF_NFC_INF,
	PMIF_RC_INF,
	PMIF_INF_MAX,
};

struct pmif_hw {
	struct base_hw hw;
	struct reg_t _conn_inf_en;
	struct reg_t _nfc_inf_en;
	struct reg_t _rc_inf_en;
	struct reg_t _conn_clr_addr;
	struct reg_t _conn_set_addr;
	struct reg_t _conn_clr_cmd;
	struct reg_t _conn_set_cmd;
	struct reg_t _nfc_clr_addr;
	struct reg_t _nfc_set_addr;
	struct reg_t _nfc_clr_cmd;
	struct reg_t _nfc_set_cmd;
	struct reg_t _mode_ctrl;
	struct reg_t _slp_ctrl;
};

struct clkbuf_pmif_hw {
	struct mutex lock;
	struct pmif_hw **pmif;
	struct xo_buf_ctl_t conn_inf_ctl;
	struct xo_buf_ctl_t nfc_inf_ctl;
	u32 pmif_num;
	u8 xo_conn_id;
	u8 xo_nfc_id;
	bool conn_inf_init;
	bool nfc_inf_init;
	bool rc_inf_init;
	bool rc_enable;
};

int __clk_buf_pmif_rc_inf_store(const char *cmd);

int clkbuf_pmif_hw_init(struct platform_device *pdev);
int clkbuf_pmif_post_init(void);
int clkbuf_pmif_get_inf_en(enum PMIF_INF inf, u32 *en);
int clkbuf_pmif_get_inf_data(enum PMIF_INF inf, u32 *clr_addr, u32 *set_addr,
		u32 *clr_cmd, u32 *set_cmd);
int clkbuf_pmif_get_misc_reg(u32 *mode_ctl, u32 *sleep_ctl, u32 pmif_idx);
u32 clkbuf_pmif_get_pmif_cnt(void);

/* PMIF V1 */
#define RC_INF_EN_V1_ADDR		(0x24)
#define RC_INF_EN_V1_MASK		(0x1)
#define RC_INF_EN_V1_SHIFT		(4)
#define CONN_INF_EN_V1_ADDR		(0x28)
#define CONN_INF_EN_V1_MASK		(0x1)
#define CONN_INF_EN_V1_SHIFT		(0)
#define NFC_INF_EN_V1_ADDR		(0x28)
#define NFC_INF_EN_V1_MASK		(0x1)
#define NFC_INF_EN_V1_SHIFT		(1)
#define CONN_CMD_DEST_V1_ADDR		(0x5C)
#define CONN_CLR_CMD_DEST_V1_ADDR	(CONN_CMD_DEST_V1_ADDR)
#define CONN_CLR_CMD_DEST_V1_MASK	(0xFFFF)
#define CONN_CLR_CMD_DEST_V1_SHIFT	(0)
#define CONN_SET_CMD_DEST_V1_ADDR	(CONN_CMD_DEST_V1_ADDR)
#define CONN_SET_CMD_DEST_V1_MASK	(0xFFFF)
#define CONN_SET_CMD_DEST_V1_SHIFT	(16)
#define CONN_CMD_V1_ADDR		(0x60)
#define CONN_CLR_CMD_V1_ADDR		(CONN_CMD_V1_ADDR)
#define CONN_CLR_CMD_V1_MASK		(0xFFFF)
#define CONN_CLR_CMD_V1_SHIFT		(0)
#define CONN_SET_CMD_V1_ADDR		(CONN_CMD_V1_ADDR)
#define CONN_SET_CMD_V1_MASK		(0xFFFF)
#define CONN_SET_CMD_V1_SHIFT		(16)
#define NFC_CMD_DEST_V1_ADDR		(0x64)
#define NFC_CLR_CMD_DEST_V1_ADDR	(NFC_CMD_DEST_V1_ADDR)
#define NFC_CLR_CMD_DEST_V1_MASK	(0xFFFF)
#define NFC_CLR_CMD_DEST_V1_SHIFT	(0)
#define NFC_SET_CMD_DEST_V1_ADDR	(NFC_CMD_DEST_V1_ADDR)
#define NFC_SET_CMD_DEST_V1_MASK	(0xFFFF)
#define NFC_SET_CMD_DEST_V1_SHIFT	(16)
#define NFC_CMD_V1_ADDR			(0x68)
#define NFC_CLR_CMD_V1_ADDR		(NFC_CMD_V1_ADDR)
#define NFC_CLR_CMD_V1_MASK		(0xFFFF)
#define NFC_CLR_CMD_V1_SHIFT		(0)
#define NFC_SET_CMD_V1_ADDR		(NFC_CMD_V1_ADDR)
#define NFC_SET_CMD_V1_MASK		(0xFFFF)
#define NFC_SET_CMD_V1_SHIFT		(16)
#define SLP_PROTECT_V1_ADDR		(0x3E8)
#define SLP_PROTECT_V1_MASK		(0xFFFFFFFF)
#define SLP_PROTECT_V1_SHIFT		(0)
#define MODE_CTRL_V1_ADDR		(0x400)
#define MODE_CTRL_V1_MASK		(0xFFFFFFFF)
#define MODE_CTRL_V1_SHIFT		(0)

#define RC_INF_EN_V2_ADDR		(0x24)
#define RC_INF_EN_V2_MASK		(0x1)
#define RC_INF_EN_V2_SHIFT		(4)
#define CONN_INF_EN_V2_ADDR		(0x28)
#define CONN_INF_EN_V2_MASK		(0x1)
#define CONN_INF_EN_V2_SHIFT		(0)
#define NFC_INF_EN_V2_ADDR		(0x28)
#define NFC_INF_EN_V2_MASK		(0x1)
#define NFC_INF_EN_V2_SHIFT		(1)
#define CONN_CMD_DEST_V2_ADDR		(0x5C)
#define CONN_CLR_CMD_DEST_V2_ADDR	(CONN_CMD_DEST_V2_ADDR)
#define CONN_CLR_CMD_DEST_V2_MASK	(0xFFFF)
#define CONN_CLR_CMD_DEST_V2_SHIFT	(0)
#define CONN_SET_CMD_DEST_V2_ADDR	(CONN_CMD_DEST_V2_ADDR)
#define CONN_SET_CMD_DEST_V2_MASK	(0xFFFF)
#define CONN_SET_CMD_DEST_V2_SHIFT	(16)
#define CONN_CMD_V2_ADDR		(0x60)
#define CONN_CLR_CMD_V2_ADDR		(CONN_CMD_V2_ADDR)
#define CONN_CLR_CMD_V2_MASK		(0xFFFF)
#define CONN_CLR_CMD_V2_SHIFT		(0)
#define CONN_SET_CMD_V2_ADDR		(CONN_CMD_V2_ADDR)
#define CONN_SET_CMD_V2_MASK		(0xFFFF)
#define CONN_SET_CMD_V2_SHIFT		(16)
#define NFC_CMD_DEST_V2_ADDR		(0x64)
#define NFC_CLR_CMD_DEST_V2_ADDR	(NFC_CMD_DEST_V2_ADDR)
#define NFC_CLR_CMD_DEST_V2_MASK	(0xFFFF)
#define NFC_CLR_CMD_DEST_V2_SHIFT	(0)
#define NFC_SET_CMD_DEST_V2_ADDR	(NFC_CMD_DEST_V2_ADDR)
#define NFC_SET_CMD_DEST_V2_MASK	(0xFFFF)
#define NFC_SET_CMD_DEST_V2_SHIFT	(16)
#define NFC_CMD_V2_ADDR			(0x68)
#define NFC_CLR_CMD_V2_ADDR		(NFC_CMD_V2_ADDR)
#define NFC_CLR_CMD_V2_MASK		(0xFFFF)
#define NFC_CLR_CMD_V2_SHIFT		(0)
#define NFC_SET_CMD_V2_ADDR		(NFC_CMD_V2_ADDR)
#define NFC_SET_CMD_V2_MASK		(0xFFFF)
#define NFC_SET_CMD_V2_SHIFT		(16)
#define SLP_PROTECT_V2_ADDR		(0x3F0)
#define SLP_PROTECT_V2_MASK		(0xFFFFFFFF)
#define SLP_PROTECT_V2_SHIFT		(0)
#define MODE_CTRL_V2_ADDR		(0x408)
#define MODE_CTRL_V2_MASK		(0xFFFFFFFF)
#define MODE_CTRL_V2_SHIFT		(0)

enum CLKBUF_PMIF_VERSION {
	CLKBUF_PMIF_NONE,
	CLKBUF_PMIF_VERSION_1,
	CLKBUF_PMIF_VERSION_2,
	CLKBUF_PMIF_VER_MAX,
};

#endif /* CLKBUF_PMIF_H */
