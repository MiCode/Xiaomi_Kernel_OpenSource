/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef QPNP_CLKDIV_H
#define QPNP_CLKDIV_H

enum q_clkdiv_cfg {
	Q_CLKDIV_NO_CLK = 0,
	Q_CLKDIV_XO_DIV_1,
	Q_CLKDIV_XO_DIV_2,
	Q_CLKDIV_XO_DIV_4,
	Q_CLKDIV_XO_DIV_8,
	Q_CLKDIV_XO_DIV_16,
	Q_CLKDIV_XO_DIV_32,
	Q_CLKDIV_XO_DIV_64,
	Q_CLKDIV_INVALID,
};

struct q_clkdiv;

struct q_clkdiv *qpnp_clkdiv_get(struct device *dev, const char *name);
int qpnp_clkdiv_enable(struct q_clkdiv *q_clkdiv);
int qpnp_clkdiv_disable(struct q_clkdiv *q_clkdiv);
int qpnp_clkdiv_config(struct q_clkdiv *q_clkdiv,
				enum q_clkdiv_cfg cfg);
#endif
