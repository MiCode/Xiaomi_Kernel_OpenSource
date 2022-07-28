/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _ktz8866_SW_H_
#define _ktz8866_SW_H_

extern int lcd_set_bias(int enable);
extern int lcd_set_bl_bias_reg(struct device *pdev, int enable);
#endif
