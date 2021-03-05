/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __HELIO_DVFSRC_QOS_H
#define __HELIO_DVFSRC_QOS_H

#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/io.h>
#include "helio-dvfsrc-smc-control.h"

#define dvfsrc_wait_for_completion(condition, timeout)			\
({								\
	int ret = 0;						\
	if (is_dvfsrc_enabled())				\
		ret = 1;					\
	while (!(condition) && ret > 0) {			\
		if (ret++ >= timeout)				\
			ret = -EBUSY;				\
		udelay(1);					\
	}							\
	ret;							\
})

enum {
	DVFSRC_QOS_RESERVED = 0,
	DVFSRC_QOS_MEMORY_BANDWIDTH,
	DVFSRC_QOS_CPU_MEMORY_BANDWIDTH,
	DVFSRC_QOS_GPU_MEMORY_BANDWIDTH,
	DVFSRC_QOS_MM_MEMORY_BANDWIDTH,
	DVFSRC_QOS_OTHER_MEMORY_BANDWIDTH,
	DVFSRC_QOS_DDR_OPP,
	DVFSRC_QOS_VCORE_OPP,
	DVFSRC_QOS_SCP_VCORE_REQUEST,
	DVFSRC_QOS_POWER_MODEL_DDR_REQUEST,
	DVFSRC_QOS_POWER_MODEL_VCORE_REQUEST,
	DVFSRC_QOS_VCORE_DVFS_FORCE_OPP,
	DVFSRC_QOS_ISP_HRT_BANDWIDTH,
	DVFSRC_QOS_APU_MEMORY_BANDWIDTH,
	/* insert new ID */
	DVFSRC_QOS_NUM_CLASSES,
};

/* met profile table index */
enum met_info_index {
	INFO_OPP_IDX = 0,
	INFO_FREQ_IDX,
	INFO_VCORE_IDX,
	INFO_SPM_LEVEL_IDX,
	INFO_MAX,
};

struct reg_config {
	u32 offset;
	u32 val;
};

struct helio_dvfsrc {
	int irq;
	struct device *dev;
	bool qos_enabled;
	bool dvfsrc_enabled;
	int dvfsrc_flag;

	void __iomem		*regs;
	void __iomem		*spm_regs;
	void __iomem		*sram_regs;

	struct notifier_block	pm_qos_memory_bw_nb;
	struct notifier_block	pm_qos_cpu_memory_bw_nb;
	struct notifier_block	pm_qos_gpu_memory_bw_nb;
	struct notifier_block	pm_qos_mm_memory_bw_nb;
	struct notifier_block	pm_qos_other_memory_bw_nb;
	struct notifier_block	pm_qos_ddr_opp_nb;
	struct notifier_block	pm_qos_vcore_opp_nb;
	struct notifier_block	pm_qos_scp_vcore_request_nb;
	struct notifier_block	pm_qos_power_model_ddr_request_nb;
	struct notifier_block	pm_qos_power_model_vcore_request_nb;
	struct notifier_block	pm_qos_vcore_dvfs_force_opp_nb;
	struct notifier_block	pm_qos_isp_hrt_bw_nb;
	struct notifier_block	pm_qos_apu_memory_bw_nb;

	struct reg_config	*init_config;

	bool opp_forced;
	char force_start[20];
	char force_end[20];
	int (*suspend)(struct helio_dvfsrc *dvfsrc_dev);
	int (*resume)(struct helio_dvfsrc *dvfsrc_dev);
};

extern int is_dvfsrc_enabled(void);
extern int is_dvfsrc_qos_enabled(void);
extern void helio_dvfsrc_qos_init_done(void);
extern void helio_dvfsrc_enable(int dvfsrc_en);
extern void helio_dvfsrc_flag_set(int flag);
extern int helio_dvfsrc_flag_get(void);
extern u32 dvfsrc_dump_reg(char *ptr, u32 count);
extern u32 dvfsrc_read(u32 offset);
extern u32 spm_reg_read(u32 offset);
extern void dvfsrc_write(u32 offset, u32 val);
extern void dvfsrc_opp_table_init(void);
extern int helio_dvfsrc_add_interface(struct device *dev);
extern void helio_dvfsrc_sspm_ipi_init(int dvfsrc_en);
extern void helio_dvfsrc_remove_interface(struct device *dev);
extern void dvfsrc_opp_level_mapping(void);
extern void get_opp_info(char *p);
extern void get_dvfsrc_reg(char *p);
extern void get_dvfsrc_record(char *p);

extern void vcorefs_trace_qos(void);
extern void helio_dvfsrc_platform_pre_init(struct helio_dvfsrc *dvfsrc);
extern int helio_dvfsrc_config(struct helio_dvfsrc *dvfsrc);
extern int commit_data(int type, int data, int check_spmfw);
extern void dvfsrc_enable_level_intr(int en);

extern void get_spm_reg(char *p);

extern int vcore_pmic_to_uv(int pmic_val);
extern int vcore_uv_to_pmic(int vcore_uv);
extern int helio_dvfsrc_level_mask_get(void);
extern int helio_dvfsrc_level_mask_set(bool en, int level);
extern void pm_qos_trace_dbg_show_request(int pm_qos_class);
extern void pm_qos_trace_dbg_dump(int pm_qos_class);

#endif /* __HELIO_DVFSRC_QOS_H */

