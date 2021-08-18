/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUSYS_CORE_H__
#define __APUSYS_CORE_H__
struct apusys_core_info {
	struct dentry *dbg_root;
};
/* declare init/exit func at other module */
int mdw_init(struct apusys_core_info *info);
void mdw_exit(void);
int edma_init(struct apusys_core_info *info);
void edma_exit(void);
int mdla_init(struct apusys_core_info *info);
void mdla_exit(void);
int vpu_init(struct apusys_core_info *info);
void vpu_exit(void);
int mvpu_init(struct apusys_core_info *info);
void mvpu_exit(void);
int mnoc_init(struct apusys_core_info *info);
void mnoc_exit(void);
int apu_power_drv_init(struct apusys_core_info *info);
void apu_power_drv_exit(void);
int apupwr_init_tags(struct apusys_core_info *info);
void apupwr_exit_tags(void);
int debug_init(struct apusys_core_info *info);
void debug_exit(void);
int reviser_init(struct apusys_core_info *info);
void reviser_exit(void);
int devapc_init(struct apusys_core_info *info);
void devapc_exit(void);
int apumem_init(struct apusys_core_info *info);
void apumem_exit(void);
int sw_logger_init(struct apusys_core_info *info);
void sw_logger_exit(void);
int hw_logger_init(struct apusys_core_info *info);
void hw_logger_exit(void);
int apu_rproc_init(struct apusys_core_info *info);
void apu_rproc_exit(void);

/*
 * init function at other modulses
 * call init function in order at apusys.ko init stage
 */
static int (*apusys_init_func[])(struct apusys_core_info *) = {
	sw_logger_init,
	hw_logger_init,
//	apupwr_init_tags,
	apu_power_drv_init,
	devapc_init,
	mnoc_init,
	reviser_init,
	apumem_init,
	mdw_init,
	edma_init,
	mdla_init,
#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU)
	vpu_init,
#endif
#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
	debug_init,
#endif
	mvpu_init,
	apu_rproc_init,
};

/*
 * exit function at other modulses
 * call exit function in order at apusys.ko exit stage
 */
static void (*apusys_exit_func[])(void) = {
	apu_rproc_exit,
#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
	mvpu_exit,
	debug_exit,
#endif
#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU)
	vpu_exit,
#endif
	mdla_exit,
	edma_exit,
	mdw_exit,
	apumem_exit,
	reviser_exit,
	mnoc_exit,
	devapc_exit,
	apu_power_drv_exit,
//	apupwr_exit_tags,
	hw_logger_exit,
	sw_logger_exit,
};
#endif
