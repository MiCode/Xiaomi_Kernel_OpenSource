/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_PLAT_SERVICE_H__
#define __GPUEB_PLAT_SERVICE_H__

#define PLT_INIT           0x504C5401
#define PLT_LOG_ENABLE     0x504C5402
#define PLAT_IPI_TEST      0

struct plat_ipi_send_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int phys;
			unsigned int size;
		} ctrl;
		struct {
			unsigned int enable;
		} logger;
	} u;
};

int gpueb_plat_service_init(struct platform_device *pdev);

#endif /* __GPUEB_PLAT_SERVICE_H__ */
