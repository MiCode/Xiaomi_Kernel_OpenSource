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
int sample_init(struct apusys_core_info *info);
void sample_exit(void);
int edma_init(struct apusys_core_info *info);
void edma_exit(void);
int mdla_init(struct apusys_core_info *info);
void mdla_exit(void);
int vpu_init(struct apusys_core_info *info);
void vpu_exit(void);
int mnoc_init(struct apusys_core_info *info);
void mnoc_exit(void);
int mdw_mem_drv_init(struct apusys_core_info *info);
void mdw_mem_drv_exit(void);
int apu_power_drv_init(struct apusys_core_info *info);
void apu_power_drv_exit(void);
int debug_init(struct apusys_core_info *info);
void debug_exit(void);


/*
 * init function at other modulses
 * call init function in order at apusys.ko init stage
 */
static int (*apusys_init_func[])(struct apusys_core_info *) = {
	apu_power_drv_init,
	mnoc_init,
	mdw_mem_drv_init,
	mdw_init,
	sample_init,
	edma_init,
	mdla_init,
#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU)
	vpu_init,
#endif
	debug_init,
};

/*
 * exit function at other modulses
 * call exit function in order at apusys.ko exit stage
 */
static void (*apusys_exit_func[])(void) = {
	debug_exit,
#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU)
	vpu_exit,
#endif
	mdla_exit,
	edma_exit,
	sample_exit,
	mdw_exit,
	mdw_mem_drv_exit,
	mnoc_exit,
	apu_power_drv_exit,
};
#endif
