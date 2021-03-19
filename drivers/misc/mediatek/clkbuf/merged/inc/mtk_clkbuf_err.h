/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#ifndef CLKBUF_ERR_H
#define CLKBUF_ERR_H

enum clkbuf_err_code {
	EREG_NOT_SUPPORT = 1000,
	EHW_NOT_SUPPORT,
	EHW_ALREADY_INIT,
	EFIND_DTS_ERR,
	EGET_BASE_FAILED,
	ECHIP_NOT_FOUND = 1005,
	ENO_PMIC_REGMAP_FOUND,
	EXO_NUM_CONFIG_ERR,
	ERC_INIT_TIMEOUT,
	EXO_NOT_SW_CTRL,
	EXO_NOT_FOUND = 1010,
};

#endif /* CLKBUF_ERR_H */
