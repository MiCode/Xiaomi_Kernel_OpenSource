// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUSYS_MNOC_DRV_H__
#define __APUSYS_MNOC_DRV_H__

/* struct for put mnoc relate information,
 * such as qos, pmu, register, etc.
 *
 * Set and get with dev_set_drvdata/dev_get_drvdata.
 */
struct apu_mnoc {
	struct device *dev;

	/* below is kboject for /sys/kernel/apumnoc */
	struct kobject *root_dir;

	/* Qos related datas */
	struct proc_dir_entry	*qos_dir;
};

#define APUSYS_MNOC_DEV_NAME "apusys_mnoc"

#define APUSYS_MNOC_LOG_PREFIX "[apusys][mnoc]"
#define LOG_ERR(x, args...) \
pr_info(APUSYS_MNOC_LOG_PREFIX "[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) \
pr_info(APUSYS_MNOC_LOG_PREFIX "[warn] %s " x, __func__, ##args)
#define LOG_INFO(x, args...) \
pr_info(APUSYS_MNOC_LOG_PREFIX "[info] %s " x, __func__, ##args)
#define DEBUG_TAG LOG_DEBUG("\n")

#define LOG_DEBUG(x, args...) \
	{ \
		if (mnoc_log_level > 0) \
			pr_info(APUSYS_MNOC_LOG_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define LOG_DETAIL(x, args...) \
	{ \
		if (mnoc_log_level > 1) \
			pr_info(APUSYS_MNOC_LOG_PREFIX "[detail] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define INT_STA_PRINTF(m, x, args...)\
	{ \
		if (m != NULL) \
			seq_printf(m, x, ##args); \
		else \
			pr_info(APUSYS_MNOC_LOG_PREFIX "[isr_work] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}


extern void __iomem *mnoc_base;
extern void __iomem *mnoc_int_base;
extern void __iomem *mnoc_apu_conn_base;
extern void __iomem *mnoc_slp_prot_base1;
extern void __iomem *mnoc_slp_prot_base2;
extern spinlock_t mnoc_spinlock;
extern bool mnoc_reg_valid;

extern int mnoc_log_level;

void infra2apu_sram_en(void);
void infra2apu_sram_dis(void);

#endif
