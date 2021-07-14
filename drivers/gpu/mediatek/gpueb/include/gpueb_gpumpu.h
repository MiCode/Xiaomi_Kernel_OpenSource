/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_GPUMPU_H__
#define __GPUEB_GPUMPU_H__

/**************************************************
 * IMPORTANT:
 * This file must be aligned with GPUEB gpumpu_ipi.h
 **************************************************/

/* per slot 4 bytes */
#define GPUMPU_IPI_SEND_DATA_LEN \
		(sizeof(struct gpumpu_ipi_send_data) / sizeof(unsigned int))

enum gpumpu_ipi_send_cmd {
	CMD_INIT_PAGE_TABLE = 2197,
	CMD_UPDATE_PAGE_TABLE,
};

struct gpumpu_ipi_send_data {
	unsigned int cmd;
	unsigned int mpu_info;
	union {
		struct {
			unsigned long long phys_base;
			unsigned long long size;
		} mpu_table;
		struct {
			unsigned long long phys_addr;
			unsigned long long size;
		} mpu_scatterlist;
	} u;
};

int gpueb_gpumpu_init(struct platform_device *pdev);

#endif /* __GPUEB_GPUMPU_H__ */
