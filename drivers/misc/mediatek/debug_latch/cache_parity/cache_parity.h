/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CACHE_PARITY_H__
#define __CACHE_PARITY_H__

struct parity_record_t {
	unsigned int check_offset;
	unsigned int check_mask;
	unsigned int dump_offset;
	unsigned int dump_length;
	unsigned int clear_offset;
	unsigned int clear_mask;
};

struct parity_irq_record_t {
	int irq;
	struct parity_record_t parity_record;
};

struct parity_irq_config_t {
	unsigned int target_cpu;
	struct parity_record_t parity_record;
};

#endif /* __CACHE_PARITY_H__ */
