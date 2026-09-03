// SPDX-License-Identifier: GPL-2.0-only
/*
 * Lightsleep config for Spreadtrum.
 *
 * Copyright (C) 2019 Spreadtrum Ltd.
 * Author: Jingchao Ye <jingchao.ye@spreadtrum.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regmap.h>
#include <linux/of_address.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>
#include <linux/of_device.h>
#include <linux/cpuidle-sprd.h>
#include <linux/tick.h>
#define SPRD_DIS_ALL			0xffffffff
#define SPRD_FRC_LIGHT			0x3ffff
#define	SPRD_INTCV_IRQ_EN		0x0008
#define	SPRD_INTCV_IRQ_DIS		0x000c

DEFINE_MUTEX(doze_mutex);

struct sprd_intc_group {
	u32 intc0;
	u32 intc1;
	u32 intc2;
	u32 intc3;
};

static struct sprd_intc_group g_intc;

static int light_sleep;
static int trace_debug;
static int test_power;
static int mcu_sleep_debug;
static int heavy_sleep;

struct regmap *cpuidle_syscon_apahb;
struct regmap *cpuidle_syscon_apapb;
struct regmap *cpuidle_syscon_aonapb;
struct regmap *cpuidle_syscon_pmuapb;

struct regmap *cpuidle_syscon_ap_intc0;
struct regmap *cpuidle_syscon_ap_intc1;
struct regmap *cpuidle_syscon_ap_intc2;
struct regmap *cpuidle_syscon_ap_intc3;

static u32 uart1_eb_temp;
static int doze_flag_en;
static void __iomem *sprd_ap_gic_base_vaddr;

/* Enable Lightsleep Function */
module_param_named(light_sleep, light_sleep, int, 0644);

/*
 * Used to trace32 debug,
 * because the value of trace32 is jumpy-changing
 * while DAP clock be gatenning
 */
module_param_named(trace_debug, trace_debug, int, 0644);

/*
 * Used to lightsleep state power test,
 * if test_power be set to 1,
 * the system will keep lightsleep state
 * until to press powerkey
 */
module_param_named(test_power, test_power, int, 0644);

/* Used to MCU_SYS_SLEEP debug */
module_param_named(mcu_sleep_debug, mcu_sleep_debug, int, 0644);


static void pm_ca7_core_auto_gate_enable(int enable)
{
	if (enable)
		regmap_update_bits(cpuidle_syscon_apahb,
				   REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
				   MASK_AP_AHB_CA7_CORE_AUTO_GATE_EN,
				   MASK_AP_AHB_CA7_CORE_AUTO_GATE_EN);
	else
		regmap_update_bits(cpuidle_syscon_apahb,
				   REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
				   MASK_AP_AHB_CA7_CORE_AUTO_GATE_EN,
				   ~MASK_AP_AHB_CA7_CORE_AUTO_GATE_EN);
}

static void pm_sync_gic_intc(void)
{
	u32 temp;

	temp = readl_relaxed(sprd_ap_gic_base_vaddr + 0x100 + 4);
	if (temp != g_intc.intc0) {
		regmap_write(cpuidle_syscon_ap_intc0,
			     SPRD_INTCV_IRQ_DIS, SPRD_DIS_ALL);
		regmap_write(cpuidle_syscon_ap_intc0, SPRD_INTCV_IRQ_EN, temp);
	}

	temp = readl_relaxed(sprd_ap_gic_base_vaddr + 0x100 + 8);
	if (temp != g_intc.intc1) {
		regmap_write(cpuidle_syscon_ap_intc1,
			     SPRD_INTCV_IRQ_DIS, SPRD_DIS_ALL);
		regmap_write(cpuidle_syscon_ap_intc1, SPRD_INTCV_IRQ_EN, temp);
	}

	temp = readl_relaxed(sprd_ap_gic_base_vaddr + 0x100 + 12);
	if (temp != g_intc.intc2) {
		regmap_write(cpuidle_syscon_ap_intc2,
			     SPRD_INTCV_IRQ_DIS, SPRD_DIS_ALL);
		regmap_write(cpuidle_syscon_ap_intc2, SPRD_INTCV_IRQ_EN, temp);
	}

	temp = readl_relaxed(sprd_ap_gic_base_vaddr + 0x100 + 16);
	if (temp != g_intc.intc3) {
		regmap_write(cpuidle_syscon_ap_intc3,
			     SPRD_INTCV_IRQ_DIS, SPRD_DIS_ALL);
		regmap_write(cpuidle_syscon_ap_intc3, SPRD_INTCV_IRQ_EN, temp);
	}
}

static bool apsys_master_slave_check(void)
{
	u32 aon_apb_eb0 = 0;
	u32 aon_apb_eb1 = 0;
	u32 ap_ahb_eb = 0;
	u32 ap_apb_eb = 0;

	regmap_read(cpuidle_syscon_apahb, REG_AP_AHB_AHB_EB, &ap_ahb_eb);
	if (ap_ahb_eb)
		return 1;

	regmap_read(cpuidle_syscon_apapb, REG_AP_APB_APB_EB, &ap_apb_eb);
	ap_apb_eb &= ~(MASK_AP_APB_UART1_EB |
			MASK_AP_APB_INTC3_EB |
			MASK_AP_APB_INTC2_EB |
			MASK_AP_APB_INTC1_EB |
			MASK_AP_APB_INTC0_EB |
			MASK_AP_APB_SIM0_32K_EB |
			MASK_AP_APB_SIM0_EB);
	if (ap_apb_eb)
		return 1;

	regmap_read(cpuidle_syscon_aonapb, REG_AON_APB_APB_EB0, &aon_apb_eb0);
	regmap_read(cpuidle_syscon_aonapb, REG_AON_APB_APB_EB1, &aon_apb_eb1);

	if ((aon_apb_eb0 & (MASK_AON_APB_MM_EB | MASK_AON_APB_GPU_EB)) |
			(aon_apb_eb1 & MASK_AON_APB_MM_VSP_EB))
		return 1;

	return 0;
}

static void doze_sleep_bak_restore_uarteb(int bak)
{
	if (bak) {
		u32 ap_apb_eb_temp = 0;

		regmap_read(cpuidle_syscon_apapb,
			    REG_AP_APB_APB_EB,
			    &ap_apb_eb_temp);
		uart1_eb_temp = MASK_AP_APB_UART1_EB & ap_apb_eb_temp;
	} else {
		if (uart1_eb_temp)
			regmap_update_bits(
					cpuidle_syscon_apapb,
					REG_AP_APB_APB_EB,
					MASK_AP_APB_UART1_EB,
					MASK_AP_APB_UART1_EB);
	}
}

static DEFINE_SPINLOCK(light_sleep_lock);
static cpumask_t cpus_lightsleep = CPU_MASK_NONE;
static unsigned int is_auto_gate_enable;
void sprd_pike2_light_en(void)
{
	u32 val = 0;
	unsigned long flags;
	u32 cpu;
	u32 is_allcpusleep = 0;

	cpu = smp_processor_id();
	spin_lock_irqsave(&light_sleep_lock, flags);
	cpumask_set_cpu(cpu, &cpus_lightsleep);
	if (cpumask_weight(&cpus_lightsleep) == cpumask_weight(cpu_online_mask))
		is_allcpusleep = 1;

	spin_unlock_irqrestore(&light_sleep_lock, flags);
	if (!light_sleep || is_allcpusleep == 0)
		return;

	regmap_read(cpuidle_syscon_apahb, REG_AP_AHB_MST_FRC_LSLP, &val);
	if (val == 0)
		regmap_write(cpuidle_syscon_apahb,
			     REG_AP_AHB_MST_FRC_LSLP,
			     SPRD_FRC_LIGHT);

	regmap_read(cpuidle_syscon_apahb, REG_AP_AHB_AHB_EB, &val);
	if (!val)
		regmap_update_bits(cpuidle_syscon_apahb,
				   REG_AP_AHB_MCU_PAUSE,
				   MASK_AP_AHB_MCU_SYS_SLEEP_EN,
				   MASK_AP_AHB_MCU_SYS_SLEEP_EN);

	pm_sync_gic_intc();
	if (is_allcpusleep) {
		pm_ca7_core_auto_gate_enable(1);
		is_auto_gate_enable = 1;
	}
	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_MCU_PAUSE,
			   MASK_AP_AHB_MCU_LIGHT_SLEEP_EN,
			   MASK_AP_AHB_MCU_LIGHT_SLEEP_EN);
}

void sprd_pike2_light_dis(void)
{
	u32 cpu;
	unsigned long flags;

	cpu = smp_processor_id();
	spin_lock_irqsave(&light_sleep_lock, flags);
	cpumask_clear_cpu(cpu, &cpus_lightsleep);
	spin_unlock_irqrestore(&light_sleep_lock, flags);
	if (!light_sleep)
		return;

	if (is_auto_gate_enable) {
		pm_sync_gic_intc();
		pm_ca7_core_auto_gate_enable(0);
		is_auto_gate_enable = 0;
	}

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_APB_EB2,
			   MASK_AON_APB_AP_DAP_EB,
			   MASK_AON_APB_AP_DAP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_MCU_PAUSE,
			   MASK_AP_AHB_MCU_SYS_SLEEP_EN |
			   MASK_AP_AHB_MCU_LIGHT_SLEEP_EN,
			   0);
}

static void sprd_doze_sleep_enter(void)
{
	if (!heavy_sleep || smp_processor_id() != 0)
		return;

	doze_flag_en = 1;

	if (!mutex_trylock(&doze_mutex))
		return;

	doze_sleep_bak_restore_uarteb(1);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PD_AP_SYS_CFG,
			   MASK_PMU_APB_PD_AP_SYS_AUTO_SHUTDOWN_EN,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PD_CA7_C0_CFG,
			   MASK_PMU_APB_PD_CA7_C0_AUTO_SHUTDOWN_EN |
			   MASK_PMU_APB_PD_CA7_C0_WFI_SHUTDOWN_EN,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PD_CA7_TOP_CFG,
			   MASK_PMU_APB_PD_CA7_TOP_AUTO_SHUTDOWN_EN,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PUB_PLL_CFG,
			   MASK_PMU_APB_PUB_PLL_AUTO_PD_EN4,
			   MASK_PMU_APB_PUB_PLL_AUTO_PD_EN4);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PLL_FRC_ON,
			   MASK_PMU_APB_XTLBUF0_FRC_ON,
			   MASK_PMU_APB_XTLBUF0_FRC_ON);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PUB_DSLP_CFG0,
			   0xff,
			   0);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_APB_EB2,
			   MASK_AON_APB_AP_DAP_EB,
			   0);

	regmap_update_bits(cpuidle_syscon_apapb,
			   REG_AP_APB_APB_EB,
			   MASK_AP_APB_UART1_EB |
			   MASK_AP_APB_SIM0_EB,
			   0);

	pm_sync_gic_intc();
	pm_ca7_core_auto_gate_enable(1);

	/* Set mtx_lp_disable to 0 (default value 0) */
	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
			   MASK_AP_AHB_AP_MAINMTX_LP_DISABLE,
			   0);

	/* To bypass LPC force channel by H/W (default value 0) */
	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
			   MASK_AP_AHB_LP_AUTO_CTRL_EN,
			   0);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_MCU_PAUSE,
			   MASK_AP_AHB_MCU_SYS_SLEEP_EN,
			   MASK_AP_AHB_MCU_SYS_SLEEP_EN);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_AP_DSLP_ENA,
			   MASK_PMU_APB_AP_DSLP_ENA,
			   MASK_PMU_APB_AP_DSLP_ENA);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_SYS_DOZE_DSLP_ENA,
			   MASK_PMU_APB_AP_DOZE_SLEEP_ENA,
			   MASK_PMU_APB_AP_DOZE_SLEEP_ENA);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_MCU_PAUSE,
			   MASK_AP_AHB_MCU_SLEEP_FOLLOW_CA7_EN |
			   MASK_AP_AHB_MCU_DEEP_SLEEP_EN,
			   MASK_AP_AHB_MCU_SLEEP_FOLLOW_CA7_EN |
			   MASK_AP_AHB_MCU_DEEP_SLEEP_EN);

	mutex_unlock(&doze_mutex);
}

static void sprd_doze_sleep_exit(void)
{
	if (!heavy_sleep || smp_processor_id() != 0)
		return;

	doze_flag_en = 0;

	doze_sleep_bak_restore_uarteb(0);
	regmap_update_bits(cpuidle_syscon_apapb,
			   REG_AP_APB_APB_EB,
			   MASK_AP_APB_SIM0_EB,
			   MASK_AP_APB_SIM0_EB);

	pm_sync_gic_intc();
	pm_ca7_core_auto_gate_enable(0);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_APB_EB2,
			   MASK_AON_APB_AP_DAP_EB,
			   MASK_AON_APB_AP_DAP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_MCU_PAUSE,
			   MASK_AP_AHB_MCU_SLEEP_FOLLOW_CA7_EN |
			   MASK_AP_AHB_MCU_DEEP_SLEEP_EN,
			   0);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_MCU_PAUSE,
			   MASK_AP_AHB_MCU_SYS_SLEEP_EN,
			   0);

	/* MASK_AP_AHB_AP_PERI_FORCE_SLP_EB */
	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG,
			   MASK_AP_AHB_AP_PERI_FORCE_SLP,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_AP_DSLP_ENA,
			   MASK_PMU_APB_AP_DSLP_ENA,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_SYS_DOZE_DSLP_ENA,
			   MASK_PMU_APB_AP_DOZE_SLEEP_ENA,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PD_AP_SYS_CFG,
			   MASK_PMU_APB_PD_AP_SYS_AUTO_SHUTDOWN_EN,
			   MASK_PMU_APB_PD_AP_SYS_AUTO_SHUTDOWN_EN);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PD_CA7_C0_CFG,
			   MASK_PMU_APB_PD_CA7_C0_AUTO_SHUTDOWN_EN |
			   MASK_PMU_APB_PD_CA7_C0_WFI_SHUTDOWN_EN,
			   MASK_PMU_APB_PD_CA7_C0_AUTO_SHUTDOWN_EN |
			   MASK_PMU_APB_PD_CA7_C0_WFI_SHUTDOWN_EN);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PD_CA7_TOP_CFG,
			   MASK_PMU_APB_PD_CA7_TOP_AUTO_SHUTDOWN_EN,
			   MASK_PMU_APB_PD_CA7_TOP_AUTO_SHUTDOWN_EN);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PUB_PLL_CFG,
			   MASK_PMU_APB_PUB_PLL_AUTO_PD_EN4,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PLL_FRC_ON,
			   MASK_PMU_APB_XTLBUF0_FRC_ON,
			   0);

	regmap_update_bits(cpuidle_syscon_pmuapb,
			   REG_PMU_APB_PUB_DSLP_CFG0,
			   0xff & 0xffffffff,
			   0xff & 0xffffffff);
}

void sprd_pike2_doze_en(void)
{

	if (apsys_master_slave_check() == 0 && cpumask_weight(cpu_online_mask) == 1)
		sprd_doze_sleep_enter();
	else
		sprd_pike2_light_en();
}

void sprd_pike2_doze_dis(void)
{
	if (doze_flag_en)
		sprd_doze_sleep_exit();
	else
		sprd_pike2_light_dis();
}

static void sprd_lpc_init(void)
{
	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
			   MASK_AP_AHB_LP_AUTO_CTRL_EN,
			   ~MASK_AP_AHB_LP_AUTO_CTRL_EN);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_CA7_TOP_M0_LPC,
			   MASK_AP_AHB_CA7_TOP_M0_LP_EB,
			   MASK_AP_AHB_CA7_TOP_M0_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_CA7_TOP_S1_LPC,
			   MASK_AP_AHB_CA7_TOP_S1_LP_EB,
			   MASK_AP_AHB_CA7_TOP_S1_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_CA7_TOP_S2_LPC,
			   MASK_AP_AHB_CA7_TOP_S2_LP_EB,
			   MASK_AP_AHB_CA7_TOP_S2_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_CA7_ABRG_S0_LPC,
			   MASK_AP_AHB_CA7_ABRG_S0_LP_EB,
			   MASK_AP_AHB_CA7_ABRG_S0_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_CA7_EMC_REG_SLICE_LPC,
			   MASK_AP_AHB_CA7_EMC_REG_SLICE_LP_EB,
			   MASK_AP_AHB_CA7_EMC_REG_SLICE_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_M0_LPC,
			   MASK_AP_AHB_M0_LP_EB,
			   MASK_AP_AHB_M0_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_M9_LPC,
			   MASK_AP_AHB_M9_LP_EB,
			   MASK_AP_AHB_M9_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_M_SYNC_LPC,
			   MASK_AP_AHB_M_SYNC_LP_EB,
			   MASK_AP_AHB_M_SYNC_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_S_SYNC_LPC,
			   MASK_AP_AHB_S_SYNC_LP_EB,
			   MASK_AP_AHB_S_SYNC_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_GSP_M0_LPC,
			   MASK_AP_AHB_AP_GSP_M0_LP_EB,
			   MASK_AP_AHB_AP_GSP_M0_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_GSP_M1_LPC,
			   MASK_AP_AHB_AP_GSP_M1_LP_EB,
			   MASK_AP_AHB_AP_GSP_M1_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_GSP_S0_LPC,
			   MASK_AP_AHB_AP_GSP_S0_LP_EB,
			   MASK_AP_AHB_AP_GSP_S0_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_DISP_MAIN_LPC,
			   MASK_AP_AHB_AP_DISP_MAIN_LP_EB,
			   MASK_AP_AHB_AP_DISP_MAIN_LP_EB);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_USB_AHBM2AXI_S0_LPC,
			   MASK_AP_AHB_USB_AHBM2AXI_S0_LP_EB,
			   MASK_AP_AHB_USB_AHBM2AXI_S0_LP_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX_EMC_LP_CTRL,
			   MASK_AON_APB_LP_EB_EMC,
			   MASK_AON_APB_LP_EB_EMC);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX_MAIN_LP_CTRL,
			   MASK_AON_APB_LP_EB_MAIN,
			   MASK_AON_APB_LP_EB_MAIN);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX_SW0_LP_CTRL,
			   MASK_AON_APB_LP_EB_SW0,
			   MASK_AON_APB_LP_EB_SW0);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX_SW1_LP_CTRL,
			   MASK_AON_APB_LP_EB_SW1,
			   MASK_AON_APB_LP_EB_SW1);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX__RF_LP_CTRL,
			   MASK_AON_APB_LP_EB_RF,
			   MASK_AON_APB_LP_EB_RF);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX_WCN_LP_CTRL,
			   MASK_AON_APB_LP_EB_WCN,
			   MASK_AON_APB_LP_EB_WCN);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_TOP_LPC0,
			   MASK_AON_APB_TOP_LPC0_EB,
			   MASK_AON_APB_TOP_LPC0_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_TOP_LPC1,
			   MASK_AON_APB_TOP_LPC1_EB,
			   MASK_AON_APB_TOP_LPC1_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_TOP_LPC2,
			   MASK_AON_APB_TOP_LPC2_EB,
			   MASK_AON_APB_TOP_LPC2_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_TOP_LPC3,
			   MASK_AON_APB_TOP_LPC3_EB,
			   MASK_AON_APB_TOP_LPC3_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_TOP_LPC4,
			   MASK_AON_APB_TOP_LPC4_EB,
			   MASK_AON_APB_TOP_LPC4_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_TOP_LPC5,
			   MASK_AON_APB_TOP_LPC5_EB,
			   MASK_AON_APB_TOP_LPC5_EB);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_AON_MTX_MAIN_LP_CTRL,
			   MASK_AON_APB_LP_EB_MAIN,
			   MASK_AON_APB_LP_EB_MAIN);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_RES_REG2,
			   0x7 & 0xffffffff,
			   0x7 & 0xffffffff);
}

static void sprd_ap_force_light(void)
{
	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG,
			   MASK_AP_AHB_MCU_LIGHT_SLEEP_EN_FORCE,
			   MASK_AP_AHB_MCU_LIGHT_SLEEP_EN_FORCE);

	regmap_update_bits(cpuidle_syscon_apahb,
			   REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
			   MASK_AP_AHB_CA7_DBG_AUTO_GATE_EN |
			   MASK_AP_AHB_AP_AHB_AUTO_GATE_EN,
			   MASK_AP_AHB_CA7_DBG_AUTO_GATE_EN |
			   MASK_AP_AHB_AP_AHB_AUTO_GATE_EN);

	regmap_update_bits(cpuidle_syscon_aonapb,
			   REG_AON_APB_EMC_AUTO_GATE_EN,
			   MASK_AON_APB_MAILBOX_PCLK_AUTO_GATE_EN |
			   MASK_AON_APB_WTLCP_PUB_AUTO_GATE_EN |
			   MASK_AON_APB_AP_PUB_AUTO_GATE_EN,
			   MASK_AON_APB_MAILBOX_PCLK_AUTO_GATE_EN |
			   MASK_AON_APB_WTLCP_PUB_AUTO_GATE_EN |
			   MASK_AON_APB_AP_PUB_AUTO_GATE_EN);
}

static int __init sprd_pike2_init(struct device *dev)
{
	struct device_node *node = NULL;
	struct device_node *np = of_find_node_by_name(NULL, "sprd-sleep");

	cpuidle_syscon_apahb = syscon_regmap_lookup_by_phandle(np,
							       "sprd,sys-ap-ahb");
	cpuidle_syscon_apapb = syscon_regmap_lookup_by_phandle(np,
							       "sprd,sys-ap-apb");
	cpuidle_syscon_aonapb = syscon_regmap_lookup_by_phandle(np,
								"sprd,sys-aon-apb");
	cpuidle_syscon_pmuapb = syscon_regmap_lookup_by_phandle(np,
								"sprd,sys-pmu-apb");

	if (IS_ERR(cpuidle_syscon_apahb) ||
	    IS_ERR(cpuidle_syscon_aonapb) ||
	    IS_ERR(cpuidle_syscon_pmuapb)) {
		dev_err(dev, "failed to find sprd,sys-syscon\n");
		return -EINVAL;
	}

	node = of_find_compatible_node(NULL, NULL, "arm,cortex-a9-gic");
	if (!node)
		dev_err(dev, "failed to get gic node\n");

	sprd_ap_gic_base_vaddr = of_iomap(node, 0);
	if (!sprd_ap_gic_base_vaddr) {
		dev_err(dev, "failed to map sprd_ap_gic_base_vaddr\n");
		return -EINVAL;
	}

	cpuidle_syscon_ap_intc0 = syscon_regmap_lookup_by_phandle(np,
								  "sprd,sys-ap-intc0");
	cpuidle_syscon_ap_intc1 = syscon_regmap_lookup_by_phandle(np,
								  "sprd,sys-ap-intc1");
	cpuidle_syscon_ap_intc2 = syscon_regmap_lookup_by_phandle(np,
								  "sprd,sys-ap-intc2");
	cpuidle_syscon_ap_intc3 = syscon_regmap_lookup_by_phandle(np,
								  "sprd,sys-ap-intc3");

	if (IS_ERR(cpuidle_syscon_ap_intc0) ||
	    IS_ERR(cpuidle_syscon_ap_intc1) ||
	    IS_ERR(cpuidle_syscon_ap_intc2) ||
	    IS_ERR(cpuidle_syscon_ap_intc1)) {
		dev_err(dev, "failed to find sprd,sys-ap-intc\n");
		return -EINVAL;
	}

	sprd_lpc_init();
	sprd_ap_force_light();

	light_sleep = 1;
	trace_debug = 0;
	test_power = 0;
	mcu_sleep_debug = 0;
	heavy_sleep = 1;
	return 0;
}

static struct sprd_cpuidle_operations sprd_sleep_ops = {
	.light_en = sprd_pike2_light_en,
	.light_dis = sprd_pike2_light_dis,
	.doze_en = sprd_pike2_doze_en,
	.doze_dis = sprd_pike2_doze_dis,
};

static int __init sprd_sleep_probe(struct platform_device *pdev)
{
	int ret;

	ret = sprd_pike2_init(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize pike2_sleep\n");
		return ret;
	}

	sprd_cpuidle_ops_init(&sprd_sleep_ops);

	return ret;
}

static struct platform_driver sprd_sleep_platdrv = {
	.driver = {
		.name	= "sprd-sleep",
	},
	.probe		= sprd_sleep_probe,
};

/* List of machines supported by this driver */
static const struct of_device_id sprd_sleep_dt_match[] __initconst = {
	{ .compatible = "sprd,pike2-sleep", },
	{ }
};

static int __init sprd_pike2_sleep_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;
	struct platform_device *pdev;
	int err;

	np = of_find_node_by_name(NULL, "sprd-sleep");
	if (!np)
		return -ENODEV;

	match = of_match_node(sprd_sleep_dt_match, np);
	of_node_put(np);
	if (!match) {
		pr_warn("Machine is not compatible with sprd-sleep\n");
		return -ENODEV;
	}

	err = platform_driver_register(&sprd_sleep_platdrv);

	pdev = platform_device_register_simple("sprd-sleep", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		dev_err(&pdev->dev, "failed to register sprd_sleep platform device\n");
		return PTR_ERR(pdev);
	}

	return err;
}

device_initcall(sprd_pike2_sleep_init);
MODULE_DESCRIPTION("Sprd Sleep Driver");
MODULE_LICENSE("GPL v2");
