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
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>

#define  IPV4_VERSION           0x40
#define  IPV6_VERSION           0x60

#define DPMAIF_CAP_LRO		(1 << 0)
#define DPMAIF_CAP_2RXQ		(1 << 1)
#define DPMAIF_CAP_PIT_DEG	(1 << 2)

struct dpmaif_clk_node {
	struct clk    *clk_ref;
	unsigned char *clk_name;
};

struct ccci_dpmaif_platform_ops {
	void (*hw_reset)(void);

};


extern unsigned int g_dpmaif_ver;
extern unsigned int g_chip_info;
extern struct ccci_dpmaif_platform_ops g_plt_ops;

int ccci_dpmaif_init_clk(struct device *dev,
		struct dpmaif_clk_node *clk);
void ccci_dpmaif_set_clk(unsigned int on,
		struct dpmaif_clk_node *clk);

extern int ccci_dpmaif_hif_init_v1(struct platform_device *pdev);
extern int ccci_dpmaif_hif_init_v2(struct platform_device *pdev);
extern int ccci_dpmaif_hif_init_v3(struct platform_device *pdev);

extern int ccci_dpmaif_suspend_noirq_v1(struct device *dev);
extern int ccci_dpmaif_resume_noirq_v1(struct device *dev);
extern int ccci_dpmaif_suspend_noirq_v2(struct device *dev);
extern int ccci_dpmaif_resume_noirq_v2(struct device *dev);
extern int ccci_dpmaif_suspend_noirq_v3(struct device *dev);
extern int ccci_dpmaif_resume_noirq_v3(struct device *dev);

#endif				/* __MODEM_DPMA_COMM_H__ */
