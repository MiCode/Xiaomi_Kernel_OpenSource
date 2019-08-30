/*
 * dbmdx-customer.h  --  DBMDX customer api
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_CUSTOMER_API_H
#define _DBMDX_CUSTOMER_API_H

#include "dbmdx-interface.h"

unsigned long customer_dbmdx_clk_get_rate(struct dbmdx_private *p,
					enum dbmdx_clocks clk);
long customer_dbmdx_clk_set_rate(struct dbmdx_private *p,
				enum dbmdx_clocks clk);
int customer_dbmdx_clk_enable(struct dbmdx_private *p, enum dbmdx_clocks clk);
void customer_dbmdx_clk_disable(struct dbmdx_private *p, enum dbmdx_clocks clk);
void dbmdx_uart_clk_enable(struct dbmdx_private *p, bool enable);

#endif
