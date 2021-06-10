/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_DPMA_COMM_H__
#define __MODEM_DPMA_COMM_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>



struct ccci_dpmaif_platform_ops {
	void (*hw_reset)(void);

};


extern unsigned int g_md_gen;
extern unsigned int g_ap_palt;
extern struct ccci_dpmaif_platform_ops g_plt_ops;



extern int ccci_dpmaif_hif_init_v2(struct platform_device *pdev);
extern int ccci_dpmaif_hif_init_v3(struct platform_device *pdev);
extern void mtk_ccci_affinity_rta_v2(u32 irq_cpus,
	u32 push_cpus, int cpu_nr);
extern void mtk_ccci_affinity_rta_v3(u32 irq_cpus,
	u32 push_cpus, int cpu_nr);

extern int ccci_dpmaif_suspend_noirq_v2(struct device *dev);
extern int ccci_dpmaif_resume_noirq_v2(struct device *dev);
extern int ccci_dpmaif_suspend_noirq_v3(struct device *dev);
extern int ccci_dpmaif_resume_noirq_v3(struct device *dev);

#endif				/* __MODEM_DPMA_COMM_H__ */
