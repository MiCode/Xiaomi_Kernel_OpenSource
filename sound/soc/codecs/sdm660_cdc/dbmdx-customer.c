/*
 * DSPG DBMDX codec driver customer interface
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "dbmdx-customer.h"

unsigned long customer_dbmdx_clk_get_rate(
		struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMDX_CLK_CONSTANT ? "constant" : "master");
	return 0;
}
EXPORT_SYMBOL(customer_dbmdx_clk_get_rate);

long customer_dbmdx_clk_set_rate(
		struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMDX_CLK_CONSTANT ? "constant" : "master");
	return 0;
}
EXPORT_SYMBOL(customer_dbmdx_clk_set_rate);

int customer_dbmdx_clk_enable(struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMDX_CLK_CONSTANT ? "constant" : "master");
	return 0;
}
EXPORT_SYMBOL(customer_dbmdx_clk_enable);

void customer_dbmdx_clk_disable(struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMDX_CLK_CONSTANT ? "constant" : "master");
}
EXPORT_SYMBOL(customer_dbmdx_clk_disable);

