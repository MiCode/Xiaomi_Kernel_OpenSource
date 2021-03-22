/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_CLK_QCOM_H_
#define __LINUX_CLK_QCOM_H_

#include <linux/clk.h>
#include <linux/regulator/consumer.h>

enum branch_mem_flags {
	CLKFLAG_RETAIN_PERIPH,
	CLKFLAG_NORETAIN_PERIPH,
	CLKFLAG_RETAIN_MEM,
	CLKFLAG_NORETAIN_MEM,
	CLKFLAG_PERIPH_OFF_SET,
	CLKFLAG_PERIPH_OFF_CLEAR,
};

void qcom_clk_dump(struct clk *clk, struct regulator *regulator,
			bool calltrace);
void qcom_clk_bulk_dump(int num_clks, struct clk_bulk_data *clks,
			struct regulator *regulator, bool calltrace);

#endif  /* __LINUX_CLK_QCOM_H_ */
