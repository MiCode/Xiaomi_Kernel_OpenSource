/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APS_IPI_H__
#define __APS_IPI_H__

#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#define aps_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("APS", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define aps_aee_warn(key, format, args...)
#endif


enum APS_IPI_TYPE {
	APS_LOG_LEVEL,

	/* uP to kernel */
	APS_IPI_MICROP_MSG
};

enum APS_IPI_DIR_TYPE {
	APS_IPI_READ,
	APS_IPI_WRITE,
};


int aps_ipi_send(int type_0, u64 val);
int aps_ipi_recv(int type_0, u64 *val);
int aps_ipi_init(void);
void aps_ipi_deinit(void);

#endif /* __APS_IPI_H__ */
