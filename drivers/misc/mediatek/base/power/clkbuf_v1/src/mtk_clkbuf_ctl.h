/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_ctl.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_CTL_H__
#define __MTK_CLK_BUF_CTL_H__

#include <linux/kernel.h>
#include <linux/mutex.h>

#include "mtk_clkbuf_hw.h"

enum CLK_BUF_TYPE {
	CLK_BUF_SW_DISABLE = 0,
	CLK_BUF_SW_ENABLE  = 1,
};

int mtk_register_clk_buf(struct device *dev, struct clk_buf_op *ops);

#endif

