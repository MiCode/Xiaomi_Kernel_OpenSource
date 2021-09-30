/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SLBC_IPI_H_
#define _SLBC_IPI_H_

enum {
	IPI_SLBC_ENABLE,
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
	IPI_SLBC_BUFFER_POWER_ON,
	IPI_SLBC_BUFFER_POWER_OFF,
	IPI_SLBC_FORCE,
	IPI_SLBC_MIC_NUM,
	IPI_SLBC_INNER,
	IPI_SLBC_OUTER,
	IPI_SLBC_MEM_BARRIER,
	IPI_SLB_DISABLE,
	IPI_SLC_DISABLE,
	NR_IPI_SLBC,
};

struct slbc_ipi_data {
	unsigned int cmd;
	unsigned int arg;
};

struct slbc_ipi_ops {
	int (*slbc_request_acp)(void *ptr);
	int (*slbc_release_acp)(void *ptr);
	void (*slbc_mem_barrier)(void);
};

extern int slbc_scmi_set(void *buffer, int slot);
extern int slbc_scmi_get(void *buffer, int slot, void *ptr);

#define SLBC_IPI(x, y)			((x) & 0xffff | ((y) & 0xffff) << 16)
#define SLBC_IPI_CMD_GET(x)		((x) & 0xffff)
#define SLBC_IPI_UID_GET(x)		((x) >> 16 & 0xffff)

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
extern int slbc_suspend_resume_notify(int suspend);
extern int slbc_scmi_init(void);
extern int slbc_sspm_slb_disable(int disable);
extern int slbc_sspm_slc_disable(int disable);
extern int slbc_sspm_enable(int enable);
extern int slbc_get_scmi_enable(void);
extern void slbc_set_scmi_enable(int enable);
extern int slbc_force_scmi_cmd(unsigned int force);
extern int slbc_mic_num_cmd(unsigned int num);
extern int slbc_inner_cmd(unsigned int inner);
extern int slbc_outer_cmd(unsigned int outer);
extern int _slbc_request_cache_scmi(void *ptr);
extern int _slbc_release_cache_scmi(void *ptr);
extern int _slbc_request_buffer_scmi(void *ptr);
extern int _slbc_release_buffer_scmi(void *ptr);
extern void slbc_register_ipi_ops(struct slbc_ipi_ops *ops);
extern void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops);
#else
__attribute__ ((weak)) int slbc_suspend_resume_notify(int) {}
__attribute__ ((weak)) int slbc_scmi_init(void) { return 0; }
__attribute__ ((weak)) int slbc_sspm_slb_disable(int disable) {}
__attribute__ ((weak)) int slbc_sspm_slc_disable(int disable) {}
__attribute__ ((weak)) int slbc_sspm_enable(int enable) {}
__attribute__ ((weak)) int slbc_get_scmi_enable(void) { return 0; }
__attribute__ ((weak)) void slbc_set_scmi_enable(int enable) {}
__attribute__ ((weak)) int slbc_force_scmi_cmd(unsigned int force) {}
__attribute__ ((weak)) int slbc_mic_num_cmd(unsigned int num) {}
__attribute__ ((weak)) int slbc_inner_cmd(unsigned int inner) {}
__attribute__ ((weak)) int slbc_outer_cmd(unsigned int outer) {}
__attribute__ ((weak)) int _slbc_request_cache_scmi(void *ptr) {}
__attribute__ ((weak)) int _slbc_release_cache_scmi(void *ptr) {}
__attribute__ ((weak)) int _slbc_request_buffer_scmi(void *ptr) {}
__attribute__ ((weak)) int _slbc_release_buffer_scmi(void *ptr) {}
__attribute__ ((weak)) void slbc_register_ipi_ops(struct slbc_ipi_ops *ops) {}
__attribute__ ((weak)) void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops) {}
#endif /* CONFIG_MTK_SLBC_IPI */

#endif /* _SLBC_IPI_H_ */

