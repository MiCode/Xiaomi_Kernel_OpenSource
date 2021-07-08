/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_MPU_H__
#define __GPUEB_MPU_H__

/* per slot 4 bytes */
#define MPU_IPI_SEND_DATA_LEN \
		(sizeof(struct mpu_ipi_send_data) / sizeof(unsigned int))

enum mpu_ipi_send_cmd {
	CMD_INIT_MPU_TABLE = 2197,
};

struct mpu_ipi_send_data {
	unsigned int cmd;
	union {
		struct {
			u64 phys_base;
			u64 size;
		} mpu_table;
	} u;
};

int gpueb_mpu_init(struct platform_device *pdev);

#endif /* __GPUEB_MPU_H__ */
