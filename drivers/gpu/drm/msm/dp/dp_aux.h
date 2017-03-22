/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DP_AUX_H_
#define _DP_AUX_H_

#include "dp_catalog.h"

enum dp_aux_error {
	DP_AUX_ERR_NONE	= 0,
	DP_AUX_ERR_ADDR	= -1,
	DP_AUX_ERR_TOUT	= -2,
	DP_AUX_ERR_NACK	= -3,
	DP_AUX_ERR_DEFER	= -4,
	DP_AUX_ERR_NACK_DEFER	= -5,
};

enum aux_tx_mode {
	AUX_NATIVE,
	AUX_I2C,
};

enum aux_exe_mode {
	AUX_WRITE,
	AUX_READ,
};

struct aux_cmd {
	enum aux_exe_mode ex_mode;
	enum aux_tx_mode tx_mode;
	u32 addr;
	u32 len;
	u8 *buf;
	bool next;
};

struct dp_aux {
	int (*process)(struct dp_aux *aux, struct aux_cmd *cmd);
	int (*write)(struct dp_aux *aux, u32 addr, u32 len,
			enum aux_tx_mode mode, u8 *buf);
	int (*read)(struct dp_aux *aux, u32 addr, u32 len,
			enum aux_tx_mode mode, u8 **buf);
	bool (*ready)(struct dp_aux *aux);
	void (*isr)(struct dp_aux *aux);
	void (*init)(struct dp_aux *aux, u32 *aux_cfg);
	void (*deinit)(struct dp_aux *aux);
};

struct dp_aux *dp_aux_get(struct device *dev, struct dp_catalog_aux *catalog);
void dp_aux_put(struct dp_aux *aux);

#endif /*__DP_AUX_H_*/
