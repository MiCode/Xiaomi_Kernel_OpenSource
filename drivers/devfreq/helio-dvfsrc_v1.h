/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef __HELIO_DVFSRC_V1_H
#define __HELIO_DVFSRC_V1_H

#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/io.h>

#if defined(CONFIG_MACH_MT6775)
#include <helio-dvfsrc-mt6775.h>
#elif defined(CONFIG_MACH_MT6771)
#include <helio-dvfsrc-mt6771.h>
#include <helio-dvfsrc-opp.h>
#elif defined(CONFIG_MACH_MT6739)
#include <helio-dvfsrc-opp.h>
#endif

struct reg_config {
	u32 offset;
	u32 val;
};

struct helio_dvfsrc {
	struct devfreq		*devfreq;

	void __iomem		*regs;
	void __iomem		*sram_regs;
	struct mutex		lock;

	int enable;
	int skip;
	int flag;
	int dram_type;
	unsigned int log_mask;

	int curr_vcore_uv;
	int curr_ddr_khz;

	bool vcore_dvs;
	bool ddr_dfs;
	bool mm_clk;

	struct notifier_block	pm_qos_memory_bw_nb;
	struct notifier_block	pm_qos_cpu_memory_bw_nb;
	struct notifier_block	pm_qos_gpu_memory_bw_nb;
	struct notifier_block	pm_qos_mm_memory_bw_nb;
	struct notifier_block	pm_qos_md_peri_memory_bw_nb;
	struct notifier_block	pm_qos_emi_opp_nb;
	struct notifier_block	pm_qos_vcore_opp_nb;
	struct notifier_block	pm_qos_vcore_dvfs_fixed_opp_nb;

	struct reg_config	*init_config;
	struct reg_config	*suspend_config;
	struct reg_config	*resume_config;
};

extern spinlock_t helio_dvfsrc_lock;

extern int helio_dvfsrc_add_interface(struct device *dev);
extern void helio_dvfsrc_remove_interface(struct device *dev);
extern void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc);
extern void spm_check_status_before_dvfs(void);
extern int dvfsrc_transfer_to_dram_level(int data);
extern int dvfsrc_transfer_to_vcore_level(int data);

#if !defined(CONFIG_MACH_MT6771)
extern int vcorefs_get_curr_vcore(void);
extern int vcorefs_get_curr_ddr(void);
extern int dvfsrc_get_vcore_by_steps(u32 opp);
extern int dvfsrc_get_ddr_by_steps(u32 opp);
#endif
extern int is_qos_can_work(void);
extern int spm_dvfs_flag_init(void);
extern void dvfsrc_update_opp_table(void);
extern char *dvfsrc_get_opp_table_info(char *p);

extern u32 vcore_to_vcore_dvfs_level[];
extern u32 emi_to_vcore_dvfs_level[];
extern u32 vcore_dvfs_to_vcore_dvfs_level[];

extern void dvfsrc_update_sspm_vcore_opp_table(int opp, unsigned int vcore_uv);
extern void dvfsrc_update_sspm_ddr_opp_table(int opp, unsigned int ddr_khz);

extern int dvfsrc_get_bw(int type);
extern int get_cur_vcore_dvfs_opp(void);

extern int is_dvfsrc_opp_fixed(void);

#define DVFSRC_REG(dvfsrc, offset) (dvfsrc->regs + offset)
#define DVFSRC_SRAM_REG(dvfsrc, offset) (dvfsrc->sram_regs + offset)

#define DVFSRC_TIMEOUT		1000

#define is_dvfsrc_in_progress(dvfsrc)	\
	(dvfsrc_read(dvfsrc, DVFSRC_LEVEL) & 0xFFFF)
#define get_dvfsrc_level(dvfsrc)	\
	(dvfsrc_read(dvfsrc, DVFSRC_LEVEL) >> 16)

/* PMIC */
#define vcore_pmic_to_uv(pmic)	\
	(((pmic) * VCORE_STEP_UV) + VCORE_BASE_UV)
#define vcore_uv_to_pmic(uv)	/* pmic >= uv */	\
	((((uv) - VCORE_BASE_UV) + (VCORE_STEP_UV - 1)) / VCORE_STEP_UV)

#define wait_for_completion(condition, timeout)			\
({								\
	int i = 0;						\
	while (!(condition)) {					\
		if (i >= (timeout)) {				\
			i = -EBUSY;				\
			break;					\
		}						\
		udelay(1);					\
		i++;						\
	}							\
	i;							\
})

#define dvfsrc_write(dvfsrc, offset, val) \
	writel(val, DVFSRC_REG(dvfsrc, offset))

#define dvfsrc_read(dvfsrc, offset) \
	readl(DVFSRC_REG(dvfsrc, offset))

#define dvfsrc_sram_write(dvfsrc, offset, val) \
	writel(val, DVFSRC_SRAM_REG(dvfsrc, offset))

#define dvfsrc_sram_read(dvfsrc, offset) \
	readl(DVFSRC_SRAM_REG(dvfsrc, offset))

extern struct helio_dvfsrc *dvfsrc;

#endif /* __HELIO_DVFSRC_H */
