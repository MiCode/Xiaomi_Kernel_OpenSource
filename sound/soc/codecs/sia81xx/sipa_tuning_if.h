/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#ifndef _SIPA_TUNING_IF_H
#define _SIPA_TUNING_IF_H

#define SIXTH_SIPA_RX_MODULE			(0x1000E900)/* module id */
#define SIXTH_SIPA_RX_ENABLE			(0x1000EA01)/* parameter id */

struct sipa_cal_opt {
	int (*init)(void);
	void (*exit)(void);
	unsigned long (*open)(uint32_t cal_id);
	int (*close)(unsigned long handle);
	int (*read)(unsigned long handle, uint32_t mode_id, uint32_t param_id,
		uint32_t size, uint8_t *payload);
	int (*write)(unsigned long  handle, uint32_t mode_id, uint32_t param_id,
		uint32_t size, uint8_t *payload);
};

extern struct sipa_cal_opt tuning_if_opt;

#endif /* _SIPA_TUNING_IF_H */
