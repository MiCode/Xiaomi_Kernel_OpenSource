/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MVPU_IPI_H__
#define __MVPU_IPI_H__

#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#define mvpu_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("MVPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define mvpu_aee_warn(key, format, args...)
#endif


enum MVPU_IPI_TYPE {
	MVPU_LOG_LEVEL,

	/* uP to kernel */
	MVPU_IPI_MICROP_MSG
};


enum MVPU_IPI_DIR_TYPE {
	MVPU_IPI_READ,
	MVPU_IPI_WRITE,
};


int mvpu_ipi_send(int type_0, u64 val);
int mvpu_ipi_recv(int type_0, u64 *val);
int mvpu_ipi_init(void);
void mvpu_ipi_deinit(void);

#endif /* __MDLA_IPI_H__ */

