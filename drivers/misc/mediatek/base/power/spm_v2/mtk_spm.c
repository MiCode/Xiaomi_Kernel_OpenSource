/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include "mtk_spm_idle.h"
/* #include <mach/irqs.h> */
#include <mt-plat/upmu_common.h>
#include "mtk_spm_vcore_dvfs.h"
#include "mtk_vcorefs_governor.h"
#include "mtk_spm_internal.h"
#include "mtk_spm_resource_req_internal.h"
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
/* #include <mach/eint.h> */
/* #include <mach/mt_boot.h> */
#ifdef CONFIG_MTK_WD_KICKER
#include <mach/wd_api.h>
#endif

#define ENABLE_DYNA_LOAD_PCM
#ifdef ENABLE_DYNA_LOAD_PCM	/* for dyna_load_pcm */
/* for request_firmware */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/dcache.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include "mtk_spm_misc.h"
#include <linux/suspend.h>
#if defined(CONFIG_MTK_LEGACY)
#include <cust_gpio_usage.h>
#endif
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#include "mtk_dramc.h"
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
#include "mt6355/mtk_pmic_api_buck.h"
#include "mt-plat/mtk_rtc.h"
#endif
#endif
#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif

#include <mt-plat/mtk_cirq.h>

static struct dentry *spm_dir;
static struct dentry *spm_file;
struct platform_device *pspmdev;
static int dyna_load_pcm_done __nosavedata;
static int dyna_load_pcm_progress;
static int dyna_load_pcm_addr_2nd;
#define LOAD_FW_BY_DEV 0
#define LOAD_FW_BY_HIB 1
#define LOAD_FW_BY_AEE 2

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
static char *dyna_load_pcm_path[] = {
	[DYNA_LOAD_PCM_SUSPEND] = "pcm_suspend_mt6355.bin",
	[DYNA_LOAD_PCM_SUSPEND_BY_MP1] = "pcm_suspend_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_SUSPEND_LPDDR4] = "pcm_suspend_lpddr4_mt6355.bin",
	[DYNA_LOAD_PCM_SUSPEND_LPDDR4_BY_MP1] = "pcm_suspend_lpddr4_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_SODI] = "pcm_sodi_ddrdfs_mt6355.bin",
	[DYNA_LOAD_PCM_SODI_BY_MP1] = "pcm_sodi_ddrdfs_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4] = "pcm_sodi_ddrdfs_lpddr4_mt6355.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4_BY_MP1] = "pcm_sodi_ddrdfs_lpddr4_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_DEEPIDLE] = "pcm_deepidle_mt6355.bin",
	[DYNA_LOAD_PCM_DEEPIDLE_BY_MP1] = "pcm_deepidle_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_DEEPIDLE_LPDDR4] = "pcm_deepidle_lpddr4_mt6355.bin",
	[DYNA_LOAD_PCM_DEEPIDLE_LPDDR4_BY_MP1] = "pcm_deepidle_lpddr4_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4_2400] = "pcm_sodi_ddrdfs_lpddr4_2400_mt6355.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4_2400_BY_MP1] = "pcm_sodi_ddrdfs_lpddr4_2400_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_MAX] = "pcm_path_max",
};
#else
static char *dyna_load_pcm_path[] = {
	[DYNA_LOAD_PCM_SUSPEND] = "pcm_suspend.bin",
	[DYNA_LOAD_PCM_SUSPEND_BY_MP1] = "pcm_suspend_by_mp1.bin",
	[DYNA_LOAD_PCM_SUSPEND_LPDDR4] = "pcm_suspend_lpddr4.bin",
	[DYNA_LOAD_PCM_SUSPEND_LPDDR4_BY_MP1] = "pcm_suspend_lpddr4_by_mp1.bin",
	[DYNA_LOAD_PCM_SODI] = "pcm_sodi_ddrdfs.bin",
	[DYNA_LOAD_PCM_SODI_BY_MP1] = "pcm_sodi_ddrdfs_by_mp1.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4] = "pcm_sodi_ddrdfs_lpddr4.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4_BY_MP1] = "pcm_sodi_ddrdfs_lpddr4_by_mp1.bin",
	[DYNA_LOAD_PCM_DEEPIDLE] = "pcm_deepidle.bin",
	[DYNA_LOAD_PCM_DEEPIDLE_BY_MP1] = "pcm_deepidle_by_mp1.bin",
	[DYNA_LOAD_PCM_DEEPIDLE_LPDDR4] = "pcm_deepidle_lpddr4.bin",
	[DYNA_LOAD_PCM_DEEPIDLE_LPDDR4_BY_MP1] = "pcm_deepidle_lpddr4_by_mp1.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4_2400] = "pcm_sodi_ddrdfs_lpddr4_2400_mt6355.bin",
	[DYNA_LOAD_PCM_SODI_LPDDR4_2400_BY_MP1] = "pcm_sodi_ddrdfs_lpddr4_2400_by_mp1_mt6355.bin",
	[DYNA_LOAD_PCM_MAX] = "pcm_path_max",
};
#endif

MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SUSPEND]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SUSPEND_BY_MP1]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SUSPEND_LPDDR4]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SUSPEND_LPDDR4_BY_MP1]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI_BY_MP1]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI_LPDDR4]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI_LPDDR4_BY_MP1]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_DEEPIDLE]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_DEEPIDLE_BY_MP1]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_DEEPIDLE_LPDDR4]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_DEEPIDLE_LPDDR4_BY_MP1]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI_LPDDR4_2400]);
MODULE_FIRMWARE(dyna_load_pcm_path[DYNA_LOAD_PCM_SODI_LPDDR4_2400_BY_MP1]);
#endif
struct dyna_load_pcm_t dyna_load_pcm[DYNA_LOAD_PCM_MAX];

/* add char device for spm */
#include <linux/cdev.h>
#define SPM_DETECT_MAJOR 159	/* FIXME */
#define SPM_DETECT_DEV_NUM 1
#define SPM_DETECT_DRVIER_NAME "spm"
#define SPM_DETECT_DEVICE_NAME "spm"

struct class *pspmDetectClass;
struct device *pspmDetectDev;
static int gSPMDetectMajor = SPM_DETECT_MAJOR;
static struct cdev gSPMDetectCdev;

#endif				/* ENABLE_DYNA_LOAD_PCM */

void __iomem *spm_base;
void __iomem *spm_infracfg_ao_base;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
void __iomem *spm_dramc_ch0_top0_base;
void __iomem *spm_dramc_ch0_top1_base;
void __iomem *spm_dramc_ch1_top0_base;
void __iomem *spm_dramc_ch1_top1_base;
#else
void __iomem *spm_ddrphy_base;
#endif
void __iomem *spm_cksys_base;
void __iomem *spm_mcucfg;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
void __iomem *spm_bsi1cfg;
#endif
u32 gpio_base_addr;
struct clk *i2c3_clk_main;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
u32 spm_irq_0 = 197;
u32 spm_irq_1 = 198;
u32 spm_irq_2 = 199;
u32 spm_irq_3 = 200;
u32 spm_irq_4 = 201;
u32 spm_irq_5 = 202;
u32 spm_irq_6 = 203;
u32 spm_irq_7 = 204;
#else
u32 spm_irq_0 = 180;
#endif

#define	NF_EDGE_TRIG_IRQS		5
static u32 edge_trig_irqs[NF_EDGE_TRIG_IRQS];

/**************************************
 * Config and Parameter
 **************************************/
#define SPM_MD_DDR_EN_OUT	0


/**************************************
 * Define and Declare
 **************************************/
struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

static twam_handler_t spm_twam_handler;

void __attribute__((weak)) spm_sodi3_init(void)
{

}

void __attribute__((weak)) spm_sodi_init(void)
{

}

void __attribute__((weak)) spm_mcdi_init(void)
{

}

void __attribute__((weak)) spm_mcsodi_init(void)
{

}

void __attribute__((weak)) spm_deepidle_init(void)
{

}

void __attribute__((weak)) mt_power_gs_dump_suspend(void)
{

}

int __attribute__((weak)) spm_fs_init(void)
{
	return 0;
}

#if defined(CONFIG_MACH_KIBOPLUS) /* temporarily fix build fail */
int __attribute__((weak)) vcorefs_late_init_dvfs(void)
{
	return 0;
}
#endif

/**************************************
 * Init and IRQ Function
 **************************************/
static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr;
	unsigned long flags;
	struct twam_sig twamsig;

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_IRQ_STA);
	if (isr & ISRS_TWAM) {
		twamsig.sig0 = spm_read(SPM_TWAM_LAST_STA0);
		twamsig.sig1 = spm_read(SPM_TWAM_LAST_STA1);
		twamsig.sig2 = spm_read(SPM_TWAM_LAST_STA2);
		twamsig.sig3 = spm_read(SPM_TWAM_LAST_STA3);
		udelay(40); /* delay 1T @ 32K to wait twam_state_d2t down */
	}

	/* clean ISR status */
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_IRQ_STA, isr);
	if (isr & ISRS_TWAM)
		while (ISRS_TWAM & spm_read(SPM_IRQ_STA))
			;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	spm_write(SPM_SWINT_CLR, PCM_SW_INT0);
#else
	spm_write(SPM_SW_INT_CLEAR, PCM_SW_INT0);
#endif
	spin_unlock_irqrestore(&__spm_lock, flags);

	if ((isr & ISRS_TWAM) && spm_twam_handler)
		spm_twam_handler(&twamsig);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		spm_err("IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n", isr);

	return IRQ_HANDLED;
}

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
static irqreturn_t spm_irq_aux_handler(u32 irq_id)
{
	u32 isr;
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	isr = spm_read(SPM_IRQ_STA);
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	spm_write(SPM_SWINT_CLR, (1U << irq_id));
#else
	spm_write(SPM_SW_INT_CLEAR, (1U << irq_id));
#endif
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_err("IRQ%u HANDLER SHOULD NOT BE EXECUTED (0x%x)\n", irq_id, isr);

	return IRQ_HANDLED;
}

static irqreturn_t spm_irq1_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(1);
}

static irqreturn_t spm_irq2_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(2);
}

static irqreturn_t spm_irq3_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(3);
}

static irqreturn_t spm_irq4_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(4);
}

static irqreturn_t spm_irq5_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(5);
}

static irqreturn_t spm_irq6_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(6);
}

static irqreturn_t spm_irq7_handler(int irq, void *dev_id)
{
	return spm_irq_aux_handler(7);
}
#endif

static int spm_irq_register(void)
{
	int i, err, r = 0;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	struct spm_irq_desc irqdesc[] = {
		{.irq = 0, .handler = spm_irq0_handler,},
		{.irq = 0, .handler = spm_irq1_handler,},
		{.irq = 0, .handler = spm_irq2_handler,},
		{.irq = 0, .handler = spm_irq3_handler,},
		{.irq = 0, .handler = spm_irq4_handler,},
		{.irq = 0, .handler = spm_irq5_handler,},
		{.irq = 0, .handler = spm_irq6_handler,},
		{.irq = 0, .handler = spm_irq7_handler,}
	};
#endif
	irqdesc[0].irq = SPM_IRQ0_ID;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	irqdesc[1].irq = SPM_IRQ1_ID;
	irqdesc[2].irq = SPM_IRQ2_ID;
	irqdesc[3].irq = SPM_IRQ3_ID;
	irqdesc[4].irq = SPM_IRQ4_ID;
	irqdesc[5].irq = SPM_IRQ5_ID;
	irqdesc[6].irq = SPM_IRQ6_ID;
	irqdesc[7].irq = SPM_IRQ7_ID;
#endif
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		if (cpu_present(i)) {
			err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
					IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND | IRQF_PERCPU, "SPM", NULL);
			if (err) {
				spm_err("FAILED TO REQUEST IRQ%d (%d)\n", i, err);
				r = -EPERM;
			}
		}
	}
	return r;
}

static void spm_register_init(void)
{
	unsigned long flags;
	struct device_node *node;
#if !defined(CONFIG_MACH_MT6757) && !defined(CONFIG_MACH_KIBOPLUS)
	int ret = -1;
#endif

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		spm_err("find SLEEP node failed\n");
	spm_base = of_iomap(node, 0);
	if (!spm_base) {
		spm_err("base spm_base failed\n");
		return;
	}

	spm_irq_0 = irq_of_parse_and_map(node, 0);
	if (!spm_irq_0)
		spm_err("get spm_irq_0 failed\n");
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	spm_irq_1 = irq_of_parse_and_map(node, 1);
	if (!spm_irq_1)
		spm_err("get spm_irq_1 failed\n");
	spm_irq_2 = irq_of_parse_and_map(node, 2);
	if (!spm_irq_2)
		spm_err("get spm_irq_2 failed\n");
	spm_irq_3 = irq_of_parse_and_map(node, 3);
	if (!spm_irq_3)
		spm_err("get spm_irq_3 failed\n");
	spm_irq_4 = irq_of_parse_and_map(node, 4);
	if (!spm_irq_4)
		spm_err("get spm_irq_4 failed\n");
	spm_irq_5 = irq_of_parse_and_map(node, 5);
	if (!spm_irq_5)
		spm_err("get spm_irq_5 failed\n");
	spm_irq_6 = irq_of_parse_and_map(node, 6);
	if (!spm_irq_6)
		spm_err("get spm_irq_6 failed\n");
	spm_irq_7 = irq_of_parse_and_map(node, 7);
	if (!spm_irq_7)
		spm_err("get spm_irq_7 failed\n");
#endif
	/* cksys_base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
	if (!node)
		spm_err("[CLK_CKSYS] find node failed\n");
	spm_cksys_base = of_iomap(node, 0);
	if (!spm_cksys_base)
		spm_err("[CLK_CKSYS] base failed\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (!node)
		spm_err("[CLK_INFRACFG_AO] find node failed\n");
	spm_infracfg_ao_base = of_iomap(node, 0);
	if (!spm_infracfg_ao_base)
		spm_err("[CLK_INFRACFG_AO] base failed\n");

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	if (!node)
		spm_err("[MCUCFG] find node failed\n");
	spm_mcucfg = of_iomap(node, 0);
	if (!spm_mcucfg)
		spm_err("[MCUCFG] base failed\n");
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	/* bsi1cfg */
	node = of_find_compatible_node(NULL, NULL, "mediatek,bpi_bsi_slv1");
	if (!node)
		spm_err("[bsi1] find node failed\n");
	spm_bsi1cfg = of_iomap(node, 0);
	if (!spm_bsi1cfg)
		spm_err("[bsi1] base failed\n");
#endif
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	node = of_find_compatible_node(NULL, NULL, "mediatek,dramc_ch0_top0");
	if (!node)
		spm_err("find dramc_ch0_top0 node failed\n");
	spm_dramc_ch0_top0_base = of_iomap(node, 0);
	if (!spm_dramc_ch0_top0_base)
		spm_err("[dramc_ch0_top0] base failed\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,dramc_ch0_top1");
	if (!node)
		spm_err("find dramc_ch0_top1 node failed\n");
	spm_dramc_ch0_top1_base = of_iomap(node, 0);
	if (!spm_dramc_ch0_top1_base)
		spm_err("[dramc_ch0_top1] base failed\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,dramc_ch1_top0");
	if (!node)
		spm_err("find dramc_ch1_top0 node failed\n");
	spm_dramc_ch1_top0_base = of_iomap(node, 0);
	if (!spm_dramc_ch1_top0_base)
		spm_err("[dramc_ch1_top0] base failed\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,dramc_ch1_top1");
	if (!node)
		spm_err("find dramc_ch1_top1 node failed\n");
	spm_dramc_ch1_top1_base = of_iomap(node, 0);
	if (!spm_dramc_ch1_top1_base)
		spm_err("[dramc_ch1_top1] base failed\n");
#else
	node = of_find_compatible_node(NULL, NULL, "mediatek,ddrphy");
	if (!node)
		spm_err("find DDRPHY node failed\n");
	spm_ddrphy_base = of_iomap(node, 0);
	if (!spm_ddrphy_base)
		spm_err("[DDRPHY] base failed\n");
#endif

	spm_err("spm_base = %p, spm_irq_0 = %d\n", spm_base, spm_irq_0);
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	spm_err("spm_irq_1 = %d, spm_irq_2 = %d, spm_irq_3 = %d\n",
		spm_irq_1, spm_irq_2, spm_irq_3);
	spm_err("spm_irq_4 = %d, spm_irq_5 = %d, spm_irq_6 = %d, spm_irq_7 = %d\n", spm_irq_4,
		spm_irq_5, spm_irq_6, spm_irq_7);
#endif
	spm_err("cksys_base = %p, infracfg_ao_base = %p, spm_mcucfg = %p\n", spm_cksys_base,
		spm_infracfg_ao_base, spm_mcucfg);

	/* GPIO */
	node = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
	if (!node)
		spm_err("find mediatek,GPIO failed\n");
	if (of_property_read_u32_array(node, "reg", &gpio_base_addr, 1))
		spm_err("mediatek,GPIO base addr can NOT found!\n");

	/* edge trigger irqs that have to lateched by CIRQ */
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6757-toprgu");
	if (!node) {
		spm_err("find toprgu node failed\n");
	} else {
		edge_trig_irqs[0] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[0])
			spm_err("get wdt_irq failed\n");
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6757-keypad");
	if (!node) {
		spm_err("find keypad node failed\n");
	} else {
		edge_trig_irqs[1] = irq_of_parse_and_map(node, 0);
		if (!edge_trig_irqs[1])
			spm_err("get kp_irq failed\n");
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,ap2c2k_ccif");
	if (!node) {
		spm_err("find ap2c2k_ccif node failed\n");
	} else {
		edge_trig_irqs[2] = irq_of_parse_and_map(node, 1);
		if (!edge_trig_irqs[2])
			spm_err("get c2k_wdt_irq failed\n");
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdcldma");
	if (!node) {
		spm_err("find mdcldma node failed\n");
	} else {
		edge_trig_irqs[3] = irq_of_parse_and_map(node, 2);
		if (!edge_trig_irqs[3])
			spm_err("get md_wdt_irq failed\n");
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6757-consys");
	if (!node) {
		spm_err("find consys node failed\n");
	} else {
		edge_trig_irqs[4] = irq_of_parse_and_map(node, 1);
		if (!edge_trig_irqs[4])
			spm_err("get conn_wdt_irq failed\n");
	}

	spm_err("edge trigger irqs: %d, %d, %d, %d, %d\n",
		 edge_trig_irqs[0],
		 edge_trig_irqs[1],
		 edge_trig_irqs[2],
		 edge_trig_irqs[3],
		 edge_trig_irqs[4]);
#else /* Dilapidate, needs to remove */
	if (!node)
		node = of_find_compatible_node(NULL, NULL, "mediatek,sys_cirq");

	if (!node) {
		spm_err("find mediatek,sys_cirq failed\n");
		/* WARN_ON(1); */
	} else {
		ret = of_property_read_u32_array(node, "mediatek,edge_trig_irqs_for_spm",
					   edge_trig_irqs, NF_EDGE_TRIG_IRQS);
		if (!ret) {
			/* Returns 0 on success */
			spm_err("edge trigger irqs: %d, %d, %d, %d, %d\n",
						edge_trig_irqs[0],
						edge_trig_irqs[1],
						edge_trig_irqs[2],
						edge_trig_irqs[3],
						edge_trig_irqs[4]
			);
		} else {
			/* Can not get value of edge_trig_irqs */
			spm_err("find mediatek,edge_trig_irqs_for_spm failed\n");
			/* WARN_ON(1); */
		}
	}
#endif

	spin_lock_irqsave(&__spm_lock, flags);

	/* md resource request selection */

#ifdef CONFIG_MTK_MD3_SUPPORT
#if CONFIG_MTK_MD3_SUPPORT /* Using this to check > 0 */
	if (spm_infracfg_ao_base)
		spm_write(SPM_INFRA_MISC,
				(spm_read(SPM_INFRA_MISC) &
				 ~(0xff << MD_SRC_REQ_BIT)) |
				(0x6d << MD_SRC_REQ_BIT));
#else /* CONFIG_MTK_MD3_SUPPORT is 0 */
	if (spm_infracfg_ao_base)
		spm_write(SPM_INFRA_MISC,
				(spm_read(SPM_INFRA_MISC) &
					~(0xff << MD_SRC_REQ_BIT)) |
				(0x29 << MD_SRC_REQ_BIT));
#endif
#else /* CONFIG_MTK_MD3_SUPPORT is not defined */
	if (spm_infracfg_ao_base)
		spm_write(SPM_INFRA_MISC,
				(spm_read(SPM_INFRA_MISC) &
					~(0xff << MD_SRC_REQ_BIT)) |
				(0x29 << MD_SRC_REQ_BIT));
#endif

	/* enable register control */
	spm_write(POWERON_CONFIG_EN, SPM_REGWR_CFG_KEY | BCLK_CG_EN_LSB);

	/* init power control register */
	/* dram will set this register */
	/* spm_write(SPM_POWER_ON_VAL0, 0); */
	spm_write(SPM_POWER_ON_VAL1, POWER_ON_VAL1_DEF);
	spm_write(PCM_PWR_IO_EN, 0);

	/* reset PCM */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | PCM_SW_RESET_LSB);
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
	WARN_ON((spm_read(PCM_FSM_STA) & 0x7fffff) != PCM_FSM_STA_DEF);	/* PCM reset failed */

	/* init PCM control register */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | EN_IM_SLEEP_DVS_LSB);
	spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | EVENT_LOCK_EN_LSB |
		  SPM_SRAM_ISOINT_B_LSB | SPM_SRAM_SLEEP_B_LSB | MIF_APBEN_LSB);
	spm_write(PCM_IM_PTR, 0);
	spm_write(PCM_IM_LEN, 0);

	/*
	 * SRCLKENA0: POWER_ON_VAL1 (PWR_IO_EN[7]=0) or
	 *            E1: r7|SRCLKENAI0|SRCLKENAI1|MD1_SRCLKENA (PWR_IO_EN[7]=1)
	 *            E2: r7|SRCLKENAI0 (PWR_IO_EN[7]=1)
	 * CLKSQ0_OFF: POWER_ON_VAL0 (PWR_IO_EN[0]=0) or r0 (PWR_IO_EN[0]=1)
	 * SRCLKENA1: MD2_SRCLKENA
	 * CLKSQ1_OFF: !MD2_SRCLKENA
	 */
	/* bit 2, 3, 6, 12, 27 = 1 */
	spm_write(SPM_CLK_CON, spm_read(SPM_CLK_CON) | CC_SYSCLK1_EN_0 |
		  CC_SYSCLK1_EN_1 | CC_SRCLKENA_MASK_0 |
		  CLKSQ1_SEL_CTRL_LSB | SYSCLK0_SRC_MASK_B_LSB |
		  CC_SYSCLK1_SRC_MASK_B_MD2_SRCCLKENA);

	/* clean wakeup event raw status */
	spm_write(SPM_WAKEUP_EVENT_MASK, SPM_WAKEUP_EVENT_MASK_DEF);

	/* clean ISR status */
	spm_write(SPM_IRQ_MASK, ISRM_ALL);
	spm_write(SPM_IRQ_STA, ISRC_ALL);
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	spm_write(SPM_SWINT_CLR, PCM_SW_INT_ALL);
#else
	spm_write(SPM_SW_INT_CLEAR, PCM_SW_INT_ALL);
#endif

	/* output md_ddr_en if needed for debug */
#if SPM_MD_DDR_EN_OUT
	__spm_dbgout_md_ddr_en(true);
#endif

	/* init r7 with POWER_ON_VAL1 */
	spm_write(PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(PCM_PWR_IO_EN, 0);

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	/* init cg status for MC-SODI */
	spm_write(SPM_BSI_CLK_SR, 1);
#endif

	spin_unlock_irqrestore(&__spm_lock, flags);
}

int __init spm_module_init(void)
{
	int r = 0;
	int i;
	unsigned int irq_type;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	u32 reg_val;
#endif
#endif
#if 0
#ifdef CONFIG_MTK_WD_KICKER
	struct wd_api *wd_api;
#endif
#endif
	spm_register_init();
	if (spm_irq_register() != 0)
		r = -EPERM;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_PM)
	if (spm_fs_init() != 0)
		r = -EPERM;
#endif

	/* Note: Initialize irq type to avoid pending irqs */
	for (i = 0; i < NF_EDGE_TRIG_IRQS; i++) {
		if (edge_trig_irqs[i]) {
			irq_type = irq_get_trigger_type(edge_trig_irqs[i]);
			irq_set_irq_type(edge_trig_irqs[i], irq_type);
		}
	}

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
	set_wakeup_sources(edge_trig_irqs, NF_EDGE_TRIG_IRQS);
#endif


#if 0
#ifdef CONFIG_MTK_WD_KICKER
	get_wd_api(&wd_api);
	if (wd_api->wd_spmwdt_mode_config) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);
	} else {
		spm_err("FAILED TO GET WD API\n");
		r = -ENODEV;
	}
#endif
#endif
	spm_sodi3_init();
	spm_sodi_init();
	spm_mcdi_init();
	spm_mcsodi_init();
	spm_deepidle_init();
#if 1				/* FIXME: wait for DRAMC golden setting enable */
	if (spm_golden_setting_cmp(1) != 0) {
		/* r = -EPERM; */
		/* aee_kernel_warning("SPM Warring", "dram golden setting mismach"); */
	}
#endif

	spm_set_dummy_read_addr();

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	/* TODO: needs to add support of MT6355 */
#else
	/* debug code */
	r = pmic_read_interface_nolock(MT6351_WDTDBG_CON1, &reg_val, 0xffff, 0);
	spm_crit("[PMIC]wdtdbg_con1 : 0x%x\n", reg_val);
	r = pmic_read_interface_nolock(MT6351_BUCK_VCORE_CON0, &reg_val, 0xffff, 0);
	spm_crit("[PMIC]vcore vosel_ctrl=0x%x\n", reg_val);
	r = pmic_read_interface_nolock(MT6351_BUCK_VCORE_CON4, &reg_val, 0xffff, 0);
	spm_crit("[PMIC]vcore vosel=0x%x\n", reg_val);
	r = pmic_read_interface_nolock(MT6351_BUCK_VCORE_CON5, &reg_val, 0xffff, 0);
	spm_crit("[PMIC]vcore vosel_on=0x%x\n", reg_val);
	r = pmic_read_interface_nolock(MT6351_WDTDBG_CON1, &reg_val, 0xffff, 0);
	spm_crit("[PMIC]wdtdbg_con1-after : 0x%x\n", reg_val);
#endif
#endif
#endif
/* set Vcore DVFS bootup opp by ddr shuffle opp */
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	reg_val = get_shuffle_status();
	spm_vcorefs_init_state(reg_val);
	spm_crit2("[VcoreFS] SPM_SW_RSV_5: 0x%x, dramc shuffle status: 0x%x\n", spm_read(SPM_SW_RSV_5), reg_val);
#endif
#endif

	return r;
}

/* arch_initcall(spm_module_init); */

#ifdef ENABLE_DYNA_LOAD_PCM	/* for dyna_load_pcm */
static char *local_buf;
static dma_addr_t local_buf_dma;
static const struct firmware *spm_fw[DYNA_LOAD_PCM_MAX];

int spm_fw_count;

/*Reserved memory by device tree!*/
int reserve_memory_spm_fn(struct reserved_mem *rmem)
{
	pr_info(" name: %s, base: 0x%llx, size: 0x%llx\n", rmem->name,
			   (unsigned long long)rmem->base, (unsigned long long)rmem->size);
	WARN_ON(rmem->size < PCM_FIRMWARE_SIZE * DYNA_LOAD_PCM_MAX);

	local_buf_dma = rmem->base;
	return 0;
}
RESERVEDMEM_OF_DECLARE(reserve_memory_test, "mediatek,spm-reserve-memory", reserve_memory_spm_fn);

int spm_load_pcm_firmware(struct platform_device *pdev)
{
	const struct firmware *fw;
	int err = 0;
	int i;
	int offset = 0;
	int addr_2nd = 0;

	if (!pdev)
		return err;

	if (dyna_load_pcm_done)
		return err;

	if (local_buf == NULL) {
		local_buf = (char *)ioremap_nocache(local_buf_dma, PCM_FIRMWARE_SIZE * DYNA_LOAD_PCM_MAX);
		if (!local_buf) {
			pr_debug("Failed to dma_alloc_coherent(), %d.\n", err);
			return -ENOMEM;
		}
	}

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
		u16 firmware_size = 0;
		int copy_size = 0;
		struct pcm_desc *pdesc = &(dyna_load_pcm[i].desc);
		int j = 0;

		spm_fw[i] = NULL;
		do {
			j++;
			pr_debug("try to request_firmware() %s - %d\n", dyna_load_pcm_path[i], j);
			err = request_firmware(&fw, dyna_load_pcm_path[i], &pdev->dev);
			if (err)
				pr_debug("Failed to load %s, err = %d.\n", dyna_load_pcm_path[i], err);
		} while (err == -EAGAIN && j < 5);
		if (err) {
			pr_debug("Failed to load %s, err = %d.\n", dyna_load_pcm_path[i], err);
			continue;
		}
		spm_fw[i] = fw;

		/* Do whatever it takes to load firmware into device. */
		/* start of binary size */
		offset = 0;
		copy_size = 2;
		memcpy(&firmware_size, fw->data, copy_size);

		/* start of binary */
		offset += copy_size;
		copy_size = firmware_size * 4;
		dyna_load_pcm[i].buf = local_buf + i * PCM_FIRMWARE_SIZE;
		dyna_load_pcm[i].buf_dma = local_buf_dma + i * PCM_FIRMWARE_SIZE;
		memcpy_toio(dyna_load_pcm[i].buf, fw->data + offset, copy_size);
		/* dmac_map_area((void *)dyna_load_pcm[i].buf, PCM_FIRMWARE_SIZE, DMA_TO_DEVICE); */

		/* start of pcm_desc without pointer */
		offset += copy_size;
		copy_size = sizeof(struct pcm_desc) - offsetof(struct pcm_desc, size);
		memcpy((void *)&(dyna_load_pcm[i].desc.size), fw->data + offset, copy_size);
		/* get minimum addr_2nd */
		if (pdesc->addr_2nd) {
			if (addr_2nd)
				addr_2nd = min_t(int, (int)pdesc->addr_2nd, (int)addr_2nd);
			else
				addr_2nd = pdesc->addr_2nd;
		}

		/* start of pcm_desc version */
		offset += copy_size;
		copy_size = fw->size - offset;
		snprintf(dyna_load_pcm[i].version, PCM_FIRMWARE_VERSION_SIZE - 1,
				"%s", fw->data + offset);
		pdesc->version = dyna_load_pcm[i].version;
		pdesc->base = (u32 *) dyna_load_pcm[i].buf;
		pdesc->base_dma = dyna_load_pcm[i].buf_dma;

		dyna_load_pcm[i].ready = 1;
		spm_fw_count++;
	}

	dyna_load_pcm_addr_2nd = addr_2nd;

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	/* check addr_2nd */
	if (spm_fw_count == DYNA_LOAD_PCM_MAX) {
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
			struct pcm_desc *pdesc = &(dyna_load_pcm[i].desc);

			if (!pdesc->version)
				continue;

			if (pdesc->addr_2nd == 0) {
				if (addr_2nd == (pdesc->size - 3)) {
					*(u16 *) &pdesc->size = addr_2nd;
				} else {
					pr_debug("check addr_2nd fail, %d %d\n", addr_2nd, pdesc->size);
					WARN_ON(1);
				}
			}
		}
	}
#endif
	if (spm_fw_count == DYNA_LOAD_PCM_MAX) {
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
		__spm_pmic_low_iq_mode(0);
#endif
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
		vcorefs_late_init_dvfs();
#endif
		dyna_load_pcm_done = 1;
		spm_crit("SPM firmware is ready, dyna_load_pcm_done = %d\n", dyna_load_pcm_done);
	}

	return err;
}

int spm_load_pcm_firmware_nodev(int src)
{
	int i;

	spm_crit("%s by src %d\n", __func__, src);

	if (spm_fw_count == 0)
		spm_load_pcm_firmware(pspmdev);
	else
		spm_crit("spm_fw_count = %d\n", spm_fw_count);

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
		struct pcm_desc *pdesc = &(dyna_load_pcm[i].desc);

		if (dyna_load_pcm[i].ready &&
		    (pdesc->addr_2nd == 0) &&
		    (dyna_load_pcm_addr_2nd == (pdesc->size - 3))) {
			spm_crit("recover addr_2nd (%d), %d %d\n", i, dyna_load_pcm_addr_2nd, pdesc->size);
			*(u16 *) &pdesc->size = dyna_load_pcm_addr_2nd;
		}
	}

	return 0;
}

int spm_load_firmware_status(void)
{
	return dyna_load_pcm_done;
}

void *get_spm_firmware_version(uint32_t index)
{
	void *ptr = NULL;
#if 0
	int loop = 30;

	while (dyna_load_pcm_done == 0 && loop > 0) {
		loop--;
		msleep(100);
	}
#endif

	if (!dyna_load_pcm_done) {
		dyna_load_pcm_progress = 1;
		spm_load_pcm_firmware_nodev(LOAD_FW_BY_AEE);
	}

	if (dyna_load_pcm_done) {
		if (index == 0) {
			ptr = (void *)&spm_fw_count;
			spm_crit("SPM firmware version count = %d\n", spm_fw_count);
		} else if (index <= DYNA_LOAD_PCM_MAX) {
			ptr = dyna_load_pcm[index - 1].version;
			spm_crit("SPM firmware version(0x%x) = %s\n", index - 1, (char *)ptr);
		}
	} else {
		spm_crit("SPM firmware is not ready, dyna_load_pcm_done = %d\n", dyna_load_pcm_done);
	}

	return ptr;
}
EXPORT_SYMBOL(get_spm_firmware_version);

static int spm_dbg_show_firmware(struct seq_file *s, void *unused)
{
	int i;
	struct pcm_desc *pdesc = NULL;

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
		pdesc = &(dyna_load_pcm[i].desc);
		seq_printf(s, "#@# %s\n", dyna_load_pcm_path[i]);

		if (pdesc->version) {
			seq_printf(s, "#@#  version = %s\n", pdesc->version);
			seq_printf(s, "#@#  base = 0x%p\n", pdesc->base);
			seq_printf(s, "#@#  size = %u\n", pdesc->size);
			seq_printf(s, "#@#  sess = %u\n", pdesc->sess);
			seq_printf(s, "#@#  replace = %u\n", pdesc->replace);
			seq_printf(s, "#@#  addr_2nd = %u\n", pdesc->addr_2nd);
			seq_printf(s, "#@#  vec0 = 0x%x\n", pdesc->vec0);
			seq_printf(s, "#@#  vec1 = 0x%x\n", pdesc->vec1);
			seq_printf(s, "#@#  vec2 = 0x%x\n", pdesc->vec2);
			seq_printf(s, "#@#  vec3 = 0x%x\n", pdesc->vec3);
			seq_printf(s, "#@#  vec4 = 0x%x\n", pdesc->vec4);
			seq_printf(s, "#@#  vec5 = 0x%x\n", pdesc->vec5);
			seq_printf(s, "#@#  vec6 = 0x%x\n", pdesc->vec6);
			seq_printf(s, "#@#  vec7 = 0x%x\n", pdesc->vec7);
			seq_printf(s, "#@#  vec8 = 0x%x\n", pdesc->vec8);
			seq_printf(s, "#@#  vec9 = 0x%x\n", pdesc->vec9);
			seq_printf(s, "#@#  vec10 = 0x%x\n", pdesc->vec10);
			seq_printf(s, "#@#  vec11 = 0x%x\n", pdesc->vec11);
			seq_printf(s, "#@#  vec12 = 0x%x\n", pdesc->vec12);
			seq_printf(s, "#@#  vec13 = 0x%x\n", pdesc->vec13);
			seq_printf(s, "#@#  vec14 = 0x%x\n", pdesc->vec14);
			seq_printf(s, "#@#  vec15 = 0x%x\n", pdesc->vec15);
		}
	}
	seq_puts(s, "\n\n");

	return 0;
}

static int spm_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, spm_dbg_show_firmware, &inode->i_private);
}

static const struct file_operations spm_debug_fops = {
	.open = spm_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int SPM_detect_open(struct inode *inode, struct file *file)
{
	pr_debug("open major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
	if (dyna_load_pcm_progress == 0)
		spm_load_pcm_firmware_nodev(LOAD_FW_BY_DEV);

	return 0;
}

static int SPM_detect_close(struct inode *inode, struct file *file)
{
	pr_debug("close major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	return 0;
}

static ssize_t SPM_detect_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	pr_debug(" ++\n");
	pr_debug(" --\n");

	return 0;
}

ssize_t SPM_detect_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	pr_debug(" ++\n");
	pr_debug(" --\n");

	return 0;
}

const struct file_operations gSPMDetectFops = {
	.open = SPM_detect_open,
	.release = SPM_detect_close,
	.read = SPM_detect_read,
	.write = SPM_detect_write,
};

static int spm_sleep_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, get_spm_sleep_count, &inode->i_private);
}

static const struct file_operations spm_sleep_count_fops = {
	.open = spm_sleep_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int spm_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	int i = 0;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++) {
			if (spm_fw[i])
				release_firmware(spm_fw[i]);
		}
		dyna_load_pcm_done = 0;
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++)
			dyna_load_pcm[i].ready = 0;
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		dyna_load_pcm_done = 0;
		for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++)
			dyna_load_pcm[i].ready = 0;
		if (local_buf) {
			iounmap(local_buf);
			local_buf = NULL;
		}
		spm_load_pcm_firmware_nodev(LOAD_FW_BY_HIB);

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block spm_pm_notifier_func = {
	.notifier_call = spm_pm_event,
	.priority = 0,
};

int spm_module_late_init(void)
{
	int i = 0;
	dev_t devID = MKDEV(gSPMDetectMajor, 0);
	int cdevErr = -1;
	int ret = -1;

	pspmdev = platform_device_register_simple("spm", 0, NULL, 0);
	if (IS_ERR(pspmdev)) {
		pr_debug("Failed to register platform device.\n");
		return -EINVAL;
	}

	ret = register_chrdev_region(devID, SPM_DETECT_DEV_NUM, SPM_DETECT_DRVIER_NAME);
	if (ret) {
		pr_debug("fail to register chrdev\n");
		return ret;
	}

	cdev_init(&gSPMDetectCdev, &gSPMDetectFops);
	gSPMDetectCdev.owner = THIS_MODULE;

	cdevErr = cdev_add(&gSPMDetectCdev, devID, SPM_DETECT_DEV_NUM);
	if (cdevErr) {
		pr_debug("cdev_add() fails (%d)\n", cdevErr);
		goto err1;
	}

	pspmDetectClass = class_create(THIS_MODULE, SPM_DETECT_DEVICE_NAME);
	if (IS_ERR(pspmDetectClass)) {
		pr_debug("class create fail, error code(%ld)\n", PTR_ERR(pspmDetectClass));
		goto err1;
	}

	pspmDetectDev = device_create(pspmDetectClass, NULL, devID, NULL, SPM_DETECT_DEVICE_NAME);
	if (IS_ERR(pspmDetectDev)) {
		pr_debug("device create fail, error code(%ld)\n", PTR_ERR(pspmDetectDev));
		goto err2;
	}

	pr_debug("driver(major %d) installed success\n", gSPMDetectMajor);

	spm_dir = debugfs_create_dir("spm", NULL);
	if (spm_dir == NULL) {
		pr_debug("Failed to create spm dir in debugfs.\n");
		return -EINVAL;
	}

	spm_file = debugfs_create_file("firmware", 0444, spm_dir, NULL, &spm_debug_fops);

	for (i = DYNA_LOAD_PCM_SUSPEND; i < DYNA_LOAD_PCM_MAX; i++)
		dyna_load_pcm[i].ready = 0;

	spm_file = debugfs_create_file("spm_sleep_count", 0444, spm_dir, NULL, &spm_sleep_count_fops);
	spm_resource_req_debugfs_init(spm_dir);

	ret = register_pm_notifier(&spm_pm_notifier_func);
	if (ret) {
		pr_debug("Failed to register PM notifier.\n");
		goto err2;
	}

	return 0;

err2:

	if (pspmDetectClass) {
		class_destroy(pspmDetectClass);
		pspmDetectClass = NULL;
	}

err1:

	if (cdevErr == 0)
		cdev_del(&gSPMDetectCdev);

	if (ret == 0) {
		unregister_chrdev_region(devID, SPM_DETECT_DEV_NUM);
		gSPMDetectMajor = -1;
	}

	pr_debug("fail\n");

	return -1;

}
late_initcall(spm_module_late_init);
#endif				/* ENABLE_DYNA_LOAD_PCM */

/**************************************
 * PLL Request API
 **************************************/
void spm_mainpll_on_request(const char *drv_name)
{
	int req;

	req = atomic_inc_return(&__spm_mainpll_req);
	spm_debug("%s request MAINPLL on (%d)\n", drv_name, req);
}
EXPORT_SYMBOL(spm_mainpll_on_request);

void spm_mainpll_on_unrequest(const char *drv_name)
{
	int req;

	req = atomic_dec_return(&__spm_mainpll_req);
	spm_debug("%s unrequest MAINPLL on (%d)\n", drv_name, req);
}
EXPORT_SYMBOL(spm_mainpll_on_unrequest);


/**************************************
 * TWAM Control API
 **************************************/
static unsigned int idle_sel;
void spm_twam_set_idle_select(unsigned int sel)
{
	idle_sel = sel & 0x3;
}
EXPORT_SYMBOL(spm_twam_set_idle_select);

static unsigned int window_len;
void spm_twam_set_window_length(unsigned int len)
{
	window_len = len;
}
EXPORT_SYMBOL(spm_twam_set_window_length);

static struct twam_sig mon_type;
void spm_twam_set_mon_type(struct twam_sig *mon)
{
	if (mon) {
		mon_type.sig0 = mon->sig0 & 0x3;
		mon_type.sig1 = mon->sig1 & 0x3;
		mon_type.sig2 = mon->sig2 & 0x3;
		mon_type.sig3 = mon->sig3 & 0x3;
	}
}
EXPORT_SYMBOL(spm_twam_set_mon_type);

void spm_twam_register_handler(twam_handler_t handler)
{
	spm_twam_handler = handler;
}
EXPORT_SYMBOL(spm_twam_register_handler);

void spm_twam_enable_monitor(const struct twam_sig *twamsig, bool speed_mode)
{
	u32 sig0 = 0, sig1 = 0, sig2 = 0, sig3 = 0;
	u32 mon0 = 0, mon1 = 0, mon2 = 0, mon3 = 0;
	unsigned int sel;
	unsigned int length;
	unsigned long flags;
	unsigned int spm_twam_con_val;

	if (twamsig) {
		sig0 = twamsig->sig0 & 0x1f;
		sig1 = twamsig->sig1 & 0x1f;
		sig2 = twamsig->sig2 & 0x1f;
		sig3 = twamsig->sig3 & 0x1f;
	}

	/* Idle selection */
	sel = idle_sel;
	/* Window length */
	length = window_len;
	/* Monitor type */
	mon0 = mon_type.sig0 & 0x3;
	mon1 = mon_type.sig1 & 0x3;
	mon2 = mon_type.sig2 & 0x3;
	mon3 = mon_type.sig3 & 0x3;

	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) & ~ISRM_TWAM);
	/* Signal Select */
	spm_write(SPM_TWAM_IDLE_SEL, sel);
	/* Monitor Control */
	spm_twam_con_val = ((sig3 << 27) |
			(sig2 << 22) |
			(sig1 << 17) |
			(sig0 << 12) |
			(mon3 << 10) |
			(mon2 << 8) |
			(mon1 << 6) |
			(mon0 << 4) | TWAM_ENABLE_LSB);
	if (speed_mode)
		spm_twam_con_val |= TWAM_SPEED_MODE_ENABLE_LSB;
	else
		spm_twam_con_val &= ~TWAM_SPEED_MODE_ENABLE_LSB;

	spm_write(SPM_TWAM_CON, spm_twam_con_val);

	/* Window Length */
	/* 0x13DDF0 for 50ms, 0x65B8 for 1ms, 0x1458 for 200us, 0xA2C for 100us */
	/* in speed mode (26 MHz) */
	spm_write(SPM_TWAM_WINDOW_LEN, length);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("enable TWAM for signal %u, %u, %u, %u (%u)\n",
		  sig0, sig1, sig2, sig3, speed_mode);
}
EXPORT_SYMBOL(spm_twam_enable_monitor);

void spm_twam_disable_monitor(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_TWAM_CON, spm_read(SPM_TWAM_CON) & ~TWAM_ENABLE_LSB);
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_TWAM);
	spm_write(SPM_IRQ_STA, ISRC_TWAM);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_debug("disable TWAM\n");
}
EXPORT_SYMBOL(spm_twam_disable_monitor);

/**************************************
 * SPM Golden Seting API(MEMPLL Control, DRAMC)
 **************************************/
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
struct ddrphy_golden_cfg {
	u32 addr;
	u32 value;
};

static struct ddrphy_golden_cfg ddrphy_setting[] = {
	{0x088, 0x0},
	{0x08c, 0x2e800},
	{0x108, 0x0},
	{0x10c, 0x2e800},
	{0x188, 0x800},
	{0x18c, 0xba000},
	{0x274, 0xffffffff},
	{0x278, 0x0},
	{0x27c, 0xfe3fffff},
};

int spm_golden_setting_cmp(bool en)
{
	u32 val;
	int i, ddrphy_num, r = 0;

	if (!en)
		return r;

	/* DRAMC0/DRAMC1 AO */
	val = spm_read(spm_dramc_ch0_top1_base + 0x038);
	if ((val & 0xc4000027) != 0xc0000007) {
		spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
			spm_dramc_ch0_top1_base + 0x038, val);
		r = -EPERM;
	}

	val = spm_read(spm_dramc_ch1_top1_base + 0x038);
	if ((val & 0xc4000027) != 0xc0000007) {
		spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
			spm_dramc_ch1_top1_base + 0x038, val);
		r = -EPERM;
	}

	/* DDRPHY0/DDRPHY1 DCM */
	val = spm_read(spm_infracfg_ao_base + 0x078);
	if ((val & 0x000000c0) != 0x0) {
		spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
			spm_infracfg_ao_base + 0x078, val);
		r = -EPERM;
	}

	val = spm_read(spm_dramc_ch0_top0_base + 0x284);
	if ((val & 0x000bff00) != 0x0) {
		spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
			spm_dramc_ch0_top0_base + 0x284, val);
		r = -EPERM;
	}

	val = spm_read(spm_dramc_ch1_top0_base + 0x284);
	if ((val & 0x000bff00) != 0x0) {
		spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
			spm_dramc_ch1_top0_base + 0x284, val);
		r = -EPERM;
	}
	if (get_ddr_type() == TYPE_LPDDR3) {
		if ((val & 0x00100000) != 0x100000) {
			spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
				 spm_dramc_ch1_top0_base + 0x284, val);
			r = -EPERM;
		}
	} else {
		if ((val & 0x001b0000) != 0x0) {
			spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
				spm_dramc_ch1_top0_base + 0x284, val);
			r = -EPERM;
		}
	}

	/* ANA_DDRPHY low power */
	ddrphy_num = ARRAY_SIZE(ddrphy_setting);
	for (i = 0; i < ddrphy_num; i++) {
		if (spm_read(spm_dramc_ch0_top0_base + ddrphy_setting[i].addr) != ddrphy_setting[i].value) {
			spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
				spm_dramc_ch0_top0_base + ddrphy_setting[i].addr,
				spm_read(spm_dramc_ch0_top0_base + ddrphy_setting[i].addr));
			r = -EPERM;
		}
		if (spm_read(spm_dramc_ch1_top0_base + ddrphy_setting[i].addr) != ddrphy_setting[i].value) {
			spm_err("dramc setting mismatch: addr = %p, val = 0x%x\n",
				spm_dramc_ch1_top0_base + ddrphy_setting[i].addr,
				spm_read(spm_dramc_ch1_top0_base + ddrphy_setting[i].addr));
			r = -EPERM;
		}
	}

	if (!r)
		pr_debug("SPM Notice: dram golden setting comparison is pass\n");

	return r;
}
#else
struct ddrphy_golden_cfg {
	u32 addr;
	u32 value;
	u32 value1;
};

static struct ddrphy_golden_cfg ddrphy_setting[] = {
	{0x5c0, 0x21271b1b, 0xffffffff},
	{0x5c4, 0x5096001e, 0xffffffff},
	{0x5c8, 0x9010f010, 0xffffffff},
	{0x5cc, 0x50101010, 0xffffffff},
	{0x640, 0x000220b1, 0x00022091},
	{0x650, 0x00000018, 0xffffffff},
	{0x698, 0x00011e00, 0x00018030},
};

int spm_golden_setting_cmp(bool en)
{

	int i, ddrphy_num, r = 0;

	if (!en)
		return r;

	/*Compare Dramc Goldeing Setting */
	ddrphy_num = ARRAY_SIZE(ddrphy_setting);
	for (i = 0; i < ddrphy_num; i++) {
		if ((spm_read(spm_ddrphy_base + ddrphy_setting[i].addr) != ddrphy_setting[i].value)
		    && ((ddrphy_setting[i].value1 == 0xffffffff)
			|| (spm_read(spm_ddrphy_base + ddrphy_setting[i].addr) !=
			    ddrphy_setting[i].value1))) {
			spm_err("dramc setting mismatch addr: %p, val: 0x%x\n",
				spm_ddrphy_base + ddrphy_setting[i].addr,
				spm_read(spm_ddrphy_base + ddrphy_setting[i].addr));
			r = -EPERM;
		}
	}

	return r;

}
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
/* for PMIC power settings */
#define RG_VCORE_SLEEP_VOLTAGE_0P568	0x0	/* 2'b00: 0.568V,default */
#define RG_VCORE_SLEEP_VOLTAGE_0P625	0x1	/* 2'b01: 0.625V */
#define RG_VCORE_SLEEP_VOLTAGE_0P65	0x2	/* 2'b10: 0.65V */
#define RG_VCORE_SLEEP_VOLTAGE_0P75	0x3	/* 2'b11: 0.75V */
static void spm_pmic_set_rg_vcore(int vcore, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(PMIC_RG_VCORE_SLEEP_VOLTAGE_ADDR,
					     vcore,
					     PMIC_RG_VCORE_SLEEP_VOLTAGE_MASK,
					     PMIC_RG_VCORE_SLEEP_VOLTAGE_SHIFT);
	} else {
		pmic_config_interface(PMIC_RG_VCORE_SLEEP_VOLTAGE_ADDR,
				      vcore,
				      PMIC_RG_VCORE_SLEEP_VOLTAGE_MASK,
				      PMIC_RG_VCORE_SLEEP_VOLTAGE_SHIFT);
	}
}

#define RG_BUCK_VCORE_VOSEL_SLEEP_0P65	0x27	/* 7'b0100111 */
#define RG_BUCK_VCORE_VOSEL_SLEEP_0P75	0x37	/* 7'b0110111 */
static void spm_pmic_set_vcore(int vcore, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_ADDR,
					     vcore,
					     PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_MASK,
					     PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_SHIFT);
	} else {
		pmic_config_interface(PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_ADDR,
				      vcore,
				      PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_MASK,
				      PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_SHIFT);
	}
}
#else /* not MT6355 */
/* for PMIC power settings */
#define VCORE_VOSEL_SLEEP_0P6	0x00	/* 7'b0000000 */
#define VCORE_VOSEL_SLEEP_0P65	0x08	/* 7'b0001000 */
#define VCORE_VOSEL_SLEEP_0P7	0x10	/* 7'b0010000 */
#define VCORE_VOSEL_SLEEP_0P77	0x1C	/* 7'b0011100 */
#define VCORE_VOSEL_SLEEP_0P8	0x20	/* 7'b0100000 */
#define VCORE_VOSEL_SLEEP_0P9	0x30	/* 7'b0110000 */
static void spm_pmic_set_vcore(int vcore, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_ADDR,
					     vcore,
					     MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_MASK,
					     MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_SHIFT);
	} else {
		pmic_config_interface(MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_ADDR,
				      vcore,
				      MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_MASK,
				      MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_SHIFT);
	}
}

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#define VSRAM_MD_VOSEL_SLEEP_0P6	0x00	/* 7'b0000000 */
#define VSRAM_MD_VOSEL_SLEEP_0P8	0x20	/* 7'b0100000 */
static void spm_pmic_set_vsram_md(int vsram_md, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP_ADDR,
					     vsram_md,
					     MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP_MASK,
					     MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP_SHIFT);
	} else {
		pmic_config_interface(MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP_ADDR,
				      vsram_md,
				      MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP_MASK,
				      MT6351_PMIC_BUCK_VSRAM_MD_VOSEL_SLEEP_SHIFT);
	}
}

#define VGPU_VOSEL_SLEEP_0P6	0x00	/* 7'b0000000 */
static void spm_pmic_set_vgpu(int vgpu, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VGPU_VOSEL_SLEEP_ADDR,
					     vgpu,
					     MT6351_PMIC_BUCK_VGPU_VOSEL_SLEEP_MASK,
					     MT6351_PMIC_BUCK_VGPU_VOSEL_SLEEP_SHIFT);
	} else {
		pmic_config_interface(MT6351_PMIC_BUCK_VGPU_VOSEL_SLEEP_ADDR,
				      vgpu,
				      MT6351_PMIC_BUCK_VGPU_VOSEL_SLEEP_MASK,
				      MT6351_PMIC_BUCK_VGPU_VOSEL_SLEEP_SHIFT);
	}
}

#define VMD1_VOSEL_SLEEP_0P6	0x00	/* 7'b0000000 */
static void spm_pmic_set_vmd1(int vmd1, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VMD1_VOSEL_SLEEP_ADDR,
					     vmd1,
					     MT6351_PMIC_BUCK_VMD1_VOSEL_SLEEP_MASK,
					     MT6351_PMIC_BUCK_VMD1_VOSEL_SLEEP_SHIFT);
	} else {
		pmic_config_interface(MT6351_PMIC_BUCK_VMD1_VOSEL_SLEEP_ADDR,
				      vmd1,
				      MT6351_PMIC_BUCK_VMD1_VOSEL_SLEEP_MASK,
				      MT6351_PMIC_BUCK_VMD1_VOSEL_SLEEP_SHIFT);
	}
}

#define VMODEM_VOSEL_SLEEP_0P6	0x00	/* 7'b0000000 */
static void spm_pmic_set_vmodem(int vmodem, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VMODEM_VOSEL_SLEEP_ADDR,
					     vmodem,
					     MT6351_PMIC_BUCK_VMODEM_VOSEL_SLEEP_MASK,
					     MT6351_PMIC_BUCK_VMODEM_VOSEL_SLEEP_SHIFT);
	} else {
		pmic_config_interface(MT6351_PMIC_BUCK_VMODEM_VOSEL_SLEEP_ADDR,
				      vmodem,
				      MT6351_PMIC_BUCK_VMODEM_VOSEL_SLEEP_MASK,
				      MT6351_PMIC_BUCK_VMODEM_VOSEL_SLEEP_SHIFT);
	}
}
#endif

static void spm_pmic_set_buck(u32 addr, int en_ctrl, int en, int vsleep_en, int vosel_sel, int lock)
{
	if (lock == 0) {
		if (en_ctrl != -1)
			pmic_config_interface_nolock(addr,	/* EN_CTRL_ADDR:        CON0 */
						     en_ctrl, 0x1,	/* EN_CTRL_MASK */
						     0);	/* EN_CTRL_SHIFT */
		if (en != -1)
			pmic_config_interface_nolock(addr + 0x4,	/* EN_ADDR:             CON2 */
						     en, 0x1,	/* EN_MASK */
						     0);	/* EN_SHIFT */
		if (vsleep_en != -1)
			pmic_config_interface_nolock(addr + 0x12,	/* VSLEEP_EN_ADDR:      CON9 */
						     vsleep_en, 0x1,	/* VSLEEP_EN_MASK */
						     8);	/* VSLEEP_EN_SHIFT */
		if (vosel_sel != -1)
			pmic_config_interface_nolock(addr + 0x2,	/* VOSEL_SEL_ADDR       CON1 */
						     vosel_sel, 0x7,	/* VOSEL_SEL_MASK */
						     3);	/* VOSEL_SEL_SHIFT */
	} else {
		if (en_ctrl != -1)
			pmic_config_interface(addr,	/* EN_CTRL_ADDR:        CON0 */
					      en_ctrl, 0x1,	/* EN_CTRL_MASK */
					      0);	/* EN_CTRL_SHIFT */
		if (en != -1)
			pmic_config_interface(addr + 0x4,	/* EN_ADDR:             CON2 */
					      en, 0x1,	/* EN_MASK */
					      0);	/* EN_SHIFT */
		if (vsleep_en != -1)
			pmic_config_interface(addr + 0x12,	/* VSLEEP_EN_ADDR:      CON9 */
					      vsleep_en, 0x1,	/* VSLEEP_EN_MASK */
					      8);	/* VSLEEP_EN_SHIFT */
		if (vosel_sel != -1)
			pmic_config_interface(addr + 0x2,	/* VOSEL_SEL_ADDR       CON1 */
					      vosel_sel, 0x7,	/* VOSEL_SEL_MASK */
					      3);	/* VOSEL_SEL_SHIFT */
	}
}

static void spm_pmic_set_ldo(u32 addr, int on_ctrl, int en, int mode_ctrl,
			     int secclk_mode_sel, int lock)
{
	if (lock == 0) {
		if (on_ctrl != -1)
			pmic_config_interface_nolock(addr, on_ctrl, 0x1,	/* ON_CTRL_MSAK */
						     3);	/* ON_CTRL_SHIFT */
		if (en != -1)
			pmic_config_interface_nolock(addr, en, 0x1,	/* EN_MASK */
						     1);	/* EN_SHIFT */
		if (mode_ctrl != -1)
			pmic_config_interface_nolock(addr, mode_ctrl, 0x1,	/* MODE_CTRL_MASK */
						     2);	/* MODE_CTRL_SHIFT */
		if (secclk_mode_sel != -1)
			pmic_config_interface_nolock(addr, secclk_mode_sel, 0x7,	/* SRCLK_MODE_SEL_MASK */
						     5);	/* SRCLK_MODE_SEL_SHIFT */
	} else {
		if (on_ctrl != -1)
			pmic_config_interface(addr, on_ctrl, 0x1,	/* ON_CTRL_MSAK */
					      3);	/* ON_CTRL_SHIFT */
		if (en != -1)
			pmic_config_interface(addr, en, 0x1,	/* EN_MASK */
					      1);	/* EN_SHIFT */
		if (mode_ctrl != -1)
			pmic_config_interface(addr, mode_ctrl, 0x1,	/* MODE_CTRL_MASK */
					      2);	/* MODE_CTRL_SHIFT */
		if (secclk_mode_sel != -1)
			pmic_config_interface(addr, secclk_mode_sel, 0x7,	/* SRCLK_MODE_SEL_MASK */
					      5);	/* SRCLK_MODE_SEL_SHIFT */
	}
}

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#define RG_VCORE_SLEEP_0P6	0x3	/* 2'b11 */
#define RG_VCORE_SLEEP_0P65	0x2	/* 2'b10 */
#define RG_VCORE_SLEEP_0P7	0x0	/* 2'b00 */
#define RG_VCORE_SLEEP_0P9	0x1	/* 2'b01 */
static void spm_pmic_set_rg_vcore(int vcore, int lock)
{
	if (lock == 0) {
		pmic_config_interface_nolock(MT6351_PMIC_RG_VCORE_VSLEEP_SEL_ADDR,
					     vcore,
					     MT6351_PMIC_RG_VCORE_VSLEEP_SEL_MASK,
					     MT6351_PMIC_RG_VCORE_VSLEEP_SEL_SHIFT);
	} else {
		pmic_config_interface(MT6351_PMIC_RG_VCORE_VSLEEP_SEL_ADDR,
				      vcore,
				      MT6351_PMIC_RG_VCORE_VSLEEP_SEL_MASK,
				      MT6351_PMIC_RG_VCORE_VSLEEP_SEL_SHIFT);
	}
}

static void spm_pmic_set_extra_low_power_mode(int lock)
{
	if (lock == 0) {
		/*
		 * BUCK_VCORE_CON9(0x612):
		 * BUCK_VCORE_R2R_PDN[10], BUCK_VCORE_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VCORE_R2R_PDN_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VCORE_R2R_PDN_MASK,
					     MT6351_PMIC_BUCK_VCORE_R2R_PDN_SHIFT);
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VCORE_VSLEEP_SEL_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VCORE_VSLEEP_SEL_MASK,
					     MT6351_PMIC_BUCK_VCORE_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VGPU_CON9(0x626):
		 * BUCK_VGPU_R2R_PDN[10], BUCK_VGPU_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VGPU_R2R_PDN_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VGPU_R2R_PDN_MASK,
					     MT6351_PMIC_BUCK_VGPU_R2R_PDN_SHIFT);
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VGPU_VSLEEP_SEL_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VGPU_VSLEEP_SEL_MASK,
					     MT6351_PMIC_BUCK_VGPU_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VMODEM_CON9(0x63A):
		 * BUCK_VMODEM_R2R_PDN[10], BUCK_VMODEM_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VMODEM_R2R_PDN_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VMODEM_R2R_PDN_MASK,
					     MT6351_PMIC_BUCK_VMODEM_R2R_PDN_SHIFT);
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VMODEM_VSLEEP_SEL_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VMODEM_VSLEEP_SEL_MASK,
					     MT6351_PMIC_BUCK_VMODEM_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VMD1_CON9(0x64E):
		 * BUCK_VMD1_R2R_PDN[10], BUCK_VMD1_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VMD1_R2R_PDN_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VMD1_R2R_PDN_MASK,
					     MT6351_PMIC_BUCK_VMD1_R2R_PDN_SHIFT);
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_MASK,
					     MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VSRAM_MD_CON9(0x662):
		 * BUCK_VSRAM_MD_R2R_PDN[10], BUCK_VSRAM_MD_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VSRAM_MD_R2R_PDN_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VSRAM_MD_R2R_PDN_MASK,
					     MT6351_PMIC_BUCK_VSRAM_MD_R2R_PDN_SHIFT);
		pmic_config_interface_nolock(MT6351_PMIC_BUCK_VSRAM_MD_VSLEEP_SEL_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_MASK,
					     MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_SHIFT);

		/* LDO_VXO22_CON0(0xA64): RG_VXO22_MODE_CTRL set to be 1 (0xA64[2] = 1?b1) */
		pmic_config_interface_nolock(MT6351_PMIC_RG_VXO22_MODE_CTRL_ADDR,
					     0x1,
					     MT6351_PMIC_RG_VXO22_MODE_CTRL_MASK,
					     MT6351_PMIC_RG_VXO22_MODE_CTRL_SHIFT);
	} else {
		/*
		 * BUCK_VCORE_CON9(0x612):
		 * BUCK_VCORE_R2R_PDN[10], BUCK_VCORE_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface(MT6351_PMIC_BUCK_VCORE_R2R_PDN_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VCORE_R2R_PDN_MASK,
					     MT6351_PMIC_BUCK_VCORE_R2R_PDN_SHIFT);
		pmic_config_interface(MT6351_PMIC_BUCK_VCORE_VSLEEP_SEL_ADDR,
					     0x1,
					     MT6351_PMIC_BUCK_VCORE_VSLEEP_SEL_MASK,
					     MT6351_PMIC_BUCK_VCORE_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VGPU_CON9(0x626):
		 * BUCK_VGPU_R2R_PDN[10], BUCK_VGPU_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface(MT6351_PMIC_BUCK_VGPU_R2R_PDN_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VGPU_R2R_PDN_MASK,
				      MT6351_PMIC_BUCK_VGPU_R2R_PDN_SHIFT);
		pmic_config_interface(MT6351_PMIC_BUCK_VGPU_VSLEEP_SEL_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VGPU_VSLEEP_SEL_MASK,
				      MT6351_PMIC_BUCK_VGPU_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VMODEM_CON9(0x63A):
		 * BUCK_VMODEM_R2R_PDN[10], BUCK_VMODEM_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface(MT6351_PMIC_BUCK_VMODEM_R2R_PDN_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VMODEM_R2R_PDN_MASK,
				      MT6351_PMIC_BUCK_VMODEM_R2R_PDN_SHIFT);
		pmic_config_interface(MT6351_PMIC_BUCK_VMODEM_VSLEEP_SEL_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VMODEM_VSLEEP_SEL_MASK,
				      MT6351_PMIC_BUCK_VMODEM_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VMD1_CON9(0x64E):
		 * BUCK_VMD1_R2R_PDN[10], BUCK_VMD1_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface(MT6351_PMIC_BUCK_VMD1_R2R_PDN_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VMD1_R2R_PDN_MASK,
				      MT6351_PMIC_BUCK_VMD1_R2R_PDN_SHIFT);
		pmic_config_interface(MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_MASK,
				      MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_SHIFT);
		/*
		 * BUCK_VSRAM_MD_CON9(0x662):
		 * BUCK_VSRAM_MD_R2R_PDN[10], BUCK_VSRAM_MD_VSLEEP_SEL[11] should both set to 1
		 */
		pmic_config_interface(MT6351_PMIC_BUCK_VSRAM_MD_R2R_PDN_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VSRAM_MD_R2R_PDN_MASK,
				      MT6351_PMIC_BUCK_VSRAM_MD_R2R_PDN_SHIFT);
		pmic_config_interface(MT6351_PMIC_BUCK_VSRAM_MD_VSLEEP_SEL_ADDR,
				      0x1,
				      MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_MASK,
				      MT6351_PMIC_BUCK_VMD1_VSLEEP_SEL_SHIFT);

		/* LDO_VXO22_CON0(0xA64): RG_VXO22_MODE_CTRL set to be 1 (0xA64[2] = 1?b1) */
		pmic_config_interface(MT6351_PMIC_RG_VXO22_MODE_CTRL_ADDR,
				      0x1,
				      MT6351_PMIC_RG_VXO22_MODE_CTRL_MASK,
				      MT6351_PMIC_RG_VXO22_MODE_CTRL_SHIFT);
	}
}
#endif

#define PMIC_SW_MODE (0)
#define PMIC_HW_MODE (1)

#define PMIC_BUCK_SRCLKEN_NA	-1
#define PMIC_BUCK_SRCLKEN0	0
#define PMIC_BUCK_SRCLKEN2	4

#define PMIC_LDO_SRCLKEN_NA	-1
#define PMIC_LDO_SRCLKEN0	0
#define PMIC_LDO_SRCLKEN2	2
#endif
#endif

void spm_pmic_power_mode(int mode, int force, int lock)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	static int prev_mode = -1;

	if (mode < PMIC_PWR_NORMAL || mode >= PMIC_PWR_NUM) {
		pr_debug("wrong spm pmic power mode");
		return;
	}

	if (force == 0 && mode == prev_mode)
		return;

	switch (mode) {
	case PMIC_PWR_NORMAL:
		/* nothing */
		break;
	case PMIC_PWR_DEEPIDLE:
		spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_VOLTAGE_0P75, lock);
		spm_pmic_set_vcore(RG_BUCK_VCORE_VOSEL_SLEEP_0P75, lock);
		pmic_buck_vcore_lp(SRCLKEN2, 0, HW_LP);
		pmic_buck_vcore_lp(SPM, 1, SPM_ON);
		break;
	case PMIC_PWR_SODI3:
		spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_VOLTAGE_0P75, lock);
		spm_pmic_set_vcore(RG_BUCK_VCORE_VOSEL_SLEEP_0P75, lock);

		pmic_ldo_vldo28_lp(SRCLKEN0, 0, HW_LP);
		pmic_ldo_vldo28_lp(SW, 1, SW_ON);
		pmic_ldo_vbif28_lp(SRCLKEN0, 1, HW_LP);
		break;
	case PMIC_PWR_SODI:
		/* nothing */
		break;
	case PMIC_PWR_SUSPEND:
		spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_VOLTAGE_0P65, lock);
		spm_pmic_set_vcore(RG_BUCK_VCORE_VOSEL_SLEEP_0P65, lock);

		pmic_ldo_vldo28_lp(SRCLKEN0, 1, HW_LP);
		pmic_ldo_vbif28_lp(SRCLKEN0, 1, HW_OFF);
		break;
	default:
		pr_debug("spm pmic power mode (%d) is not configured\n", mode);
	}

	prev_mode = mode;
#else
	static int prev_mode = -1;

	if (mode < PMIC_PWR_NORMAL || mode >= PMIC_PWR_NUM) {
		pr_debug("wrong spm pmic power mode");
		return;
	}

	if (force == 0 && mode == prev_mode)
		return;

	switch (mode) {
	case PMIC_PWR_NORMAL:
		/* nothing */
		break;
	case PMIC_PWR_DEEPIDLE:
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
		if (can_spm_pmic_set_vcore_voltage()) {
			spm_pmic_set_vcore(VCORE_VOSEL_SLEEP_0P7, lock);
		} else {
			pr_debug("Not set vcore voltage\n");
			spm_pmic_set_vcore(VCORE_VOSEL_SLEEP_0P8, lock);
		}
		spm_pmic_set_vsram_md(VSRAM_MD_VOSEL_SLEEP_0P8, lock);
		spm_pmic_set_buck(MT6351_BUCK_VCORE_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN2, lock);
		spm_pmic_set_buck(MT6351_BUCK_VSRAM_MD_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN2, lock);
		spm_pmic_set_buck(MT6351_BUCK_VS1_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN2, lock);
		spm_pmic_set_buck(MT6351_BUCK_VS2_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN2, lock);
		spm_pmic_set_buck(MT6351_BUCK_VGPU_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN2, lock);
		if (get_ddr_type() == TYPE_LPDDR3)
			spm_pmic_set_ldo(MT6351_LDO_VDRAM_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN2, lock);
		else
			spm_pmic_set_ldo(MT6351_LDO_VDRAM_CON0, 0, 0, 0, PMIC_LDO_SRCLKEN_NA, lock);
		spm_pmic_set_ldo(MT6351_LDO_VUSB33_CON0, 0, 1, 0, PMIC_LDO_SRCLKEN_NA, lock);	/* For Audio MP3 */
		spm_pmic_set_ldo(MT6351_LDO_VIO28_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN2, lock);
		spm_pmic_set_ldo(MT6351_LDO_VLDO28_CON0, 0, -1, 1, PMIC_LDO_SRCLKEN2, lock);
		spm_pmic_set_ldo(MT6351_LDO_VIO18_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN2, lock);
		spm_pmic_set_ldo(MT6351_LDO_VA18_CON0, 0, 1, 0, PMIC_LDO_SRCLKEN_NA, lock); /* For Audio MP3 */
		spm_pmic_set_ldo(MT6351_LDO_VA10_CON0, 0, -1, 1, PMIC_LDO_SRCLKEN2, lock);
		spm_pmic_set_ldo(MT6351_LDO_VXO22_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN2, lock);
#endif
		break;
	case PMIC_PWR_SODI3:
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
		if (can_spm_pmic_set_vcore_voltage()) {
			spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_0P7, lock);
			spm_pmic_set_vcore(VCORE_VOSEL_SLEEP_0P7, lock);
		} else {
			pr_debug("SODI3 set vcore sleep voltage to be 0p7\n");
			spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_0P7, lock);
			spm_pmic_set_vcore(VCORE_VOSEL_SLEEP_0P8, lock);
		}
		spm_pmic_set_vsram_md(VSRAM_MD_VOSEL_SLEEP_0P6, lock);
		spm_pmic_set_buck(MT6351_BUCK_VCORE_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VSRAM_MD_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VS1_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VS2_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VGPU_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		if (get_ddr_type() == TYPE_LPDDR3)
			spm_pmic_set_ldo(MT6351_LDO_VDRAM_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		else
			spm_pmic_set_ldo(MT6351_LDO_VDRAM_CON0, 0, 0, 0, PMIC_LDO_SRCLKEN_NA, lock);
		spm_pmic_set_ldo(MT6351_LDO_VUSB33_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VIO28_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VIO18_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VA18_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VA10_CON0, 0, -1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VLDO28_CON0, 0, -1, 0, PMIC_LDO_SRCLKEN_NA, lock);	/* For Panel */
#endif
		break;
	case PMIC_PWR_SODI:
		/* nothing */
		break;
	case PMIC_PWR_SUSPEND:
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
		if (can_spm_pmic_set_vcore_voltage()) {
			pr_debug("Set vcore sleep voltage to be 0p65\n");
			spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_0P65, lock);
			spm_pmic_set_vcore(VCORE_VOSEL_SLEEP_0P65, lock);
		} else {
			pr_debug("Set vcore sleep voltage to be 0p7\n");
			spm_pmic_set_rg_vcore(RG_VCORE_SLEEP_0P7, lock);
			spm_pmic_set_vcore(VCORE_VOSEL_SLEEP_0P8, lock);
		}
		spm_pmic_set_vsram_md(VSRAM_MD_VOSEL_SLEEP_0P6, lock);
		spm_pmic_set_vgpu(VGPU_VOSEL_SLEEP_0P6, lock);
		spm_pmic_set_vmd1(VMD1_VOSEL_SLEEP_0P6, lock);
		spm_pmic_set_vmodem(VMODEM_VOSEL_SLEEP_0P6, lock);
		spm_pmic_set_buck(MT6351_BUCK_VCORE_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VSRAM_MD_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VGPU_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VMD1_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VMODEM_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VS1_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		spm_pmic_set_buck(MT6351_BUCK_VS2_CON0, 0, 1, 1, PMIC_BUCK_SRCLKEN0, lock);
		if (get_ddr_type() == TYPE_LPDDR3)
			spm_pmic_set_ldo(MT6351_LDO_VDRAM_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		else
			spm_pmic_set_ldo(MT6351_LDO_VDRAM_CON0, 0, 0, 0, PMIC_LDO_SRCLKEN_NA, lock);
		spm_pmic_set_ldo(MT6351_LDO_VUSB33_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VIO28_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VLDO28_CON0, 0, -1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VIO18_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VA18_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VA10_CON0, 0, -1, 1, PMIC_LDO_SRCLKEN0, lock);
		spm_pmic_set_ldo(MT6351_LDO_VXO22_CON0, 0, 1, 1, PMIC_LDO_SRCLKEN0, lock);
		/* TODO: do not hardcode pmic operation */
		pmic_config_interface_nolock(0xA6E, 0x020E, 0xffff, 0); /* LDO_VA10_CON0 */

		spm_pmic_set_extra_low_power_mode(lock);
#endif
		break;
	default:
		pr_debug("spm pmic power mode (%d) is not configured\n", mode);
	}

	prev_mode = mode;
#endif
#endif
}

void spm_bypass_boost_gpio_set(void)
{
#if 0
	u32 gpio_nf = 0;
	u32 gpio_dout_nf = 0;
	u32 gpio_dout_bit = 0;
	u32 gpio_dout_addr = 0;

	gpio_nf = 20;
#if 0
#if defined(CONFIG_MTK_LEGACY)
	/* TODO: get GPIO # from header */
	gpio_nf = (GPIO_BYPASS_BOOST_PIN & 0x0000FFFF);
#else
	/* TODO: get GPIO # from dtsi */
#endif
#endif

	gpio_dout_nf = gpio_nf / 32;
	gpio_dout_bit = gpio_nf % 32;
	gpio_dout_addr = gpio_base_addr + 0x100;
	gpio_dout_addr += gpio_dout_nf * 0x10;

#if 0
	pr_debug("bypass-boost: addr = 0x%x, bit = %d\n", gpio_dout_addr, gpio_dout_bit);
#endif

	spm_write(SPM_BSI_EN_SR, gpio_dout_addr);
	spm_write(SPM_BSI_CLK_SR, gpio_dout_bit);
#endif
}

u32 spm_get_register(void __force __iomem *offset)
{
	return spm_read(offset);
}

void spm_set_register(void __force __iomem *offset, u32 value)
{
	spm_write(offset, value);
}

void *mt_spm_base_get(void)
{
	return spm_base;
}
EXPORT_SYMBOL(mt_spm_base_get);

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
void unmask_edge_trig_irqs_for_cirq(void)
{
	int i;

	for (i = 0; i < NF_EDGE_TRIG_IRQS; i++) {
		if (edge_trig_irqs[i]) {
			/* unmask edge trigger irqs */
			mt_irq_unmask_for_sleep_ex(edge_trig_irqs[i]);
		}
	}
}
#else /* Dilapidate, needs to remove */
void unmask_edge_trig_irqs_for_cirq(void)
{
	int i;

	for (i = 0; i < NF_EDGE_TRIG_IRQS; i++)
		mt_irq_unmask_for_sleep(edge_trig_irqs[i]);
}
#endif

MODULE_DESCRIPTION("SPM Driver v0.1");
