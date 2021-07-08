/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SLBC_IPI_H_
#define _SLBC_IPI_H_

enum {
	IPI_SLBC_ENABLE,
	IPI_SLBC_MEM_INIT,
	IPI_SLBC_SYNC_FROM_AP,
	IPI_SLBC_SYNC_TO_AP,
	IPI_SLBC_CACHE_REQUEST_FROM_AP,
	IPI_SLBC_CACHE_REQUEST_TO_AP,
	IPI_SLBC_CACHE_RELEASE_FROM_AP,
	IPI_SLBC_CACHE_RELEASE_TO_AP,
	IPI_SLBC_BUFFER_REQUEST_FROM_AP,
	IPI_SLBC_BUFFER_REQUEST_TO_AP,
	IPI_SLBC_BUFFER_RELEASE_FROM_AP,
	IPI_SLBC_BUFFER_RELEASE_TO_AP,
	IPI_SLBC_ACP_REQUEST_FROM_AP,
	IPI_SLBC_ACP_REQUEST_TO_AP,
	IPI_SLBC_ACP_RELEASE_FROM_AP,
	IPI_SLBC_ACP_RELEASE_TO_AP,
	IPI_SLBC_SUSPEND_RESUME_NOTIFY,
	NR_IPI_SLBC,
};

struct slbc_ipi_data {
	unsigned int cmd;
	unsigned int arg;
};

extern unsigned int slbc_ipi_to_sspm_command(void *buffer, int slot);

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
extern void slbc_suspend_resume_notify(int suspend);
extern int slbc_ipi_init(void);
extern void slbc_sspm_enable(int enable);
extern int slbc_get_ipi_enable(void);
extern void slbc_set_ipi_enable(int enable);
#else
__weak void slbc_suspend_resume_notify(int) { }
__weak int slbc_ipi_init(void) { return 0; }
__weak void slbc_sspm_enable(int enable) { }
__weak int slbc_get_ipi_enable(void) { return 0; }
__weak void slbc_set_ipi_enable(int enable) { }
#endif /* CONFIG_MTK_SLBC_IPI */

#endif /* _SLBC_IPI_H_ */

