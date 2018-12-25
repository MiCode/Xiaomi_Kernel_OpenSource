/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifdef CONFIG_FPGA_EARLY_PORTING
#define __USING_DUMMY_WDT_DRV__
#endif

#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>      /* For MODULE_ marcros  */
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>

#include <linux/uaccess.h>
#include <linux/types.h>
#include <mtk_wdt.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <mt-plat/sync_write.h>
#include <ext_wd_drv.h>

#ifndef __USING_DUMMY_WDT_DRV__
#include <mt-plat/upmu_common.h>
#endif

void __iomem *toprgu_base;
int	wdt_irq_id;
int wdt_sspm_irq_id;
int ext_debugkey_io_eint = -1;

static const struct of_device_id rgu_of_match[] = {
	{ .compatible = "mediatek,mt6765-toprgu", },
	{},
};

/**---------------------------------------------------------------------
 * Sub feature switch region
 *----------------------------------------------------------------------
 */
#define NO_DEBUG 1

/*
 *----------------------------------------------------------------------
 *   IRQ ID
 *--------------------------------------------------------------------
 */
#define AP_RGU_WDT_IRQ_ID       wdt_irq_id
#define AP_RGU_SSPM_WDT_IRQ_ID  wdt_sspm_irq_id

#ifdef CONFIG_KICK_SPM_WDT
#include <mach/mt_spm.h>
static void spm_wdt_init(void);
#endif

#ifndef __USING_DUMMY_WDT_DRV__ /* FPGA will set this flag */

static DEFINE_SPINLOCK(rgu_reg_operation_spinlock);
static int wdt_last_timeout_val;
static int wdt_enable = 1;
static bool wdt_intr_has_trigger; /* For test use */

#ifndef CONFIG_KICK_SPM_WDT
static unsigned int timeout;
#endif

static void mtk_wdt_mark_stage(unsigned int stage)
{
	unsigned int reg = __raw_readl(MTK_WDT_NONRST_REG2);

	reg = (reg & ~(RGU_STAGE_MASK << MTK_WDT_NONRST2_STAGE_OFS))
		| (stage << MTK_WDT_NONRST2_STAGE_OFS);

	mt_reg_sync_writel(reg, MTK_WDT_NONRST_REG2);
}

/*
 *   this function set the timeout value.
 *   value: second
 */
void mtk_wdt_set_time_out_value(unsigned int value)
{
	/*
	 * TimeOut = BitField 15:5
	 * Key	   = BitField  4:0 = 0x08
	 */
	spin_lock(&rgu_reg_operation_spinlock);

    #ifdef CONFIG_KICK_SPM_WDT
	spm_wdt_set_timeout(value);
    #else

	/* 1 tick means 512 * T32K -> 1s = T32/512 tick = 64 */
	/* --> value * (1<<6) */
	timeout = (unsigned int)(value * (1 << 6));
	timeout = timeout << 5;
	mt_reg_sync_writel((timeout | MTK_WDT_LENGTH_KEY), MTK_WDT_LENGTH);
    #endif
	spin_unlock(&rgu_reg_operation_spinlock);
}
/*
 *   watchdog mode:
 *   debug_en:   debug module reset enable.
 *   irq:        generate interrupt instead of reset
 *   ext_en:     output reset signal to outside
 *   ext_pol:    polarity of external reset signal
 *   wdt_en:     enable watch dog timer
 */
void mtk_wdt_mode_config(bool dual_mode_en,
					bool irq,
					bool ext_en,
					bool ext_pol,
					bool wdt_en)
{
	#ifndef CONFIG_KICK_SPM_WDT
	unsigned int tmp;
	#endif
	spin_lock(&rgu_reg_operation_spinlock);
	#ifdef CONFIG_KICK_SPM_WDT
	if (wdt_en == TRUE) {
		pr_debug("wdt enable spm timer.....\n");
		spm_wdt_enable_timer();
	} else {
		pr_debug("wdt disable spm timer.....\n");
		spm_wdt_disable_timer();
	}
    #else
	tmp = __raw_readl(MTK_WDT_MODE);
	tmp |= MTK_WDT_MODE_KEY;

	/* Bit 0 : Whether enable watchdog or not */
	if (wdt_en == TRUE)
		tmp |= MTK_WDT_MODE_ENABLE;
	else
		tmp &= ~MTK_WDT_MODE_ENABLE;

	/* Bit 1 : Configure extern reset signal polarity. */
	if (ext_pol == TRUE)
		tmp |= MTK_WDT_MODE_EXT_POL;
	else
		tmp &= ~MTK_WDT_MODE_EXT_POL;

	/* Bit 2 : Whether enable external reset signal */
	if (ext_en == TRUE)
		tmp |= MTK_WDT_MODE_EXTEN;
	else
		tmp &= ~MTK_WDT_MODE_EXTEN;

	/* Bit 3 : Whether generating interrupt instead of reset signal */
	if (irq == TRUE)
		tmp |= MTK_WDT_MODE_IRQ;
	else
		tmp &= ~MTK_WDT_MODE_IRQ;

	/* Bit 6 : Whether enable debug module reset */
	if (dual_mode_en == TRUE)
		tmp |= MTK_WDT_MODE_DUAL_MODE;
	else
		tmp &= ~MTK_WDT_MODE_DUAL_MODE;

	/* Bit 4: WDT_Auto_restart, this is a reserved bit,
	 *we use it as bypass powerkey flag.
	 */
	/* Because HW reboot always need reboot to kernel, we set it always. */
	tmp |= MTK_WDT_MODE_AUTO_RESTART;

	mt_reg_sync_writel(tmp, MTK_WDT_MODE);
	/* dual_mode(1); //always dual mode */
	/* mdelay(100); */
	pr_debug("mode change to 0x%x (write 0x%x), pid: %d\n",
		__raw_readl(MTK_WDT_MODE), tmp, current->pid);
    #endif
	spin_unlock(&rgu_reg_operation_spinlock);
}
/* EXPORT_SYMBOL(mtk_wdt_mode_config); */

int mtk_wdt_enable(enum wk_wdt_en en)
{
	unsigned int tmp = 0;

	spin_lock(&rgu_reg_operation_spinlock);
    #ifdef CONFIG_KICK_SPM_WDT
	if (en == WK_WDT_EN) {
		spm_wdt_enable_timer();
		pr_debug("wdt enable spm timer\n");

		tmp = __raw_readl(MTK_WDT_REQ_MODE);
		tmp |=  MTK_WDT_REQ_MODE_KEY;
		tmp |= (MTK_WDT_REQ_MODE_SPM_SCPSYS);
		mt_reg_sync_writel(tmp, MTK_WDT_REQ_MODE);
		wdt_enable = 1;
	}
	if (en == WK_WDT_DIS) {
		spm_wdt_disable_timer();
		pr_debug("wdt disable spm timer\n ");
		tmp = __raw_readl(MTK_WDT_REQ_MODE);
		tmp |=  MTK_WDT_REQ_MODE_KEY;
		tmp &= ~(MTK_WDT_REQ_MODE_SPM_SCPSYS);
		mt_reg_sync_writel(tmp, MTK_WDT_REQ_MODE);
		wdt_enable = 0;
	}
    #else

	tmp = __raw_readl(MTK_WDT_MODE);

	tmp |= MTK_WDT_MODE_KEY;
	if (en == WK_WDT_EN) {
		tmp |= MTK_WDT_MODE_ENABLE;
		wdt_enable = 1;
	}
	if (en == WK_WDT_DIS) {
		tmp &= ~MTK_WDT_MODE_ENABLE;
		wdt_enable = 0;
	}
	pr_debug("mtk_wdt_enable value=%x,pid=%d\n", tmp, current->pid);
	mt_reg_sync_writel(tmp, MTK_WDT_MODE);
	#endif
	spin_unlock(&rgu_reg_operation_spinlock);
	return 0;
}
int  mtk_wdt_confirm_hwreboot(void)
{
    /* aee need confirm wd can hw reboot */
    /* pr_debug("mtk_wdt_probe : Initialize to dual mode\n"); */
	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
	return 0;
}

void mtk_wdt_restart(enum wd_restart_type type)
{
	struct device_node *np_rgu;

	np_rgu = of_find_compatible_node(NULL, NULL,
		rgu_of_match[0].compatible);

	if (!toprgu_base) {
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_debug("RGU iomap failed\n");
	}

	if (type == WD_TYPE_NORMAL) {
		spin_lock(&rgu_reg_operation_spinlock);
	#ifdef CONFIG_KICK_SPM_WDT
		spm_wdt_restart_timer();
	#else
		mt_reg_sync_writel(MTK_WDT_RESTART_KEY, MTK_WDT_RESTART);
	#endif
		spin_unlock(&rgu_reg_operation_spinlock);
	} else if (type == WD_TYPE_NOLOCK) {
	#ifdef CONFIG_KICK_SPM_WDT
		spm_wdt_restart_timer_nolock();
	#else
		mt_reg_sync_writel(MTK_WDT_RESTART_KEY, MTK_WDT_RESTART);
	#endif
	} else
		pr_debug("WDT:[mtk_wdt_restart] type=%d error pid =%d\n",
			type, current->pid);
}

void mtk_wd_suspend(void)
{
	/* mtk_wdt_ModeSelection(KAL_FALSE, KAL_FALSE, KAL_FALSE); */
	/* en debug, dis irq, dis ext, low pol, dis wdt */
	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, FALSE);

	mtk_wdt_restart(WD_TYPE_NORMAL);

	/*aee_sram_printk("[WDT] suspend\n");*/
	pr_debug("[WDT] suspend\n");
}

void mtk_wd_resume(void)
{

	if (wdt_enable == 1) {
		mtk_wdt_set_time_out_value(wdt_last_timeout_val);
		mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
		mtk_wdt_restart(WD_TYPE_NORMAL);
	}

	/*aee_sram_printk("[WDT] resume(%d)\n", wdt_enable);*/
	pr_debug("[WDT] resume(%d)\n", wdt_enable);
}

void wdt_dump_reg(void)
{
	pr_info("****************dump wdt reg start*************\n");
	pr_info("MTK_WDT_MODE:0x%x\n", __raw_readl(MTK_WDT_MODE));
	pr_info("MTK_WDT_LENGTH:0x%x\n", __raw_readl(MTK_WDT_LENGTH));
	pr_info("MTK_WDT_RESTART:0x%x\n", __raw_readl(MTK_WDT_RESTART));
	pr_info("MTK_WDT_STATUS:0x%x\n", __raw_readl(MTK_WDT_STATUS));
	pr_info("MTK_WDT_INTERVAL:0x%x\n", __raw_readl(MTK_WDT_INTERVAL));
	pr_info("MTK_WDT_SWRST:0x%x\n", __raw_readl(MTK_WDT_SWRST));
	pr_info("MTK_WDT_NONRST_REG:0x%x\n", __raw_readl(MTK_WDT_NONRST_REG));
	pr_info("MTK_WDT_NONRST_REG2:0x%x\n",
		__raw_readl(MTK_WDT_NONRST_REG2));
	pr_info("MTK_WDT_REQ_MODE:0x%x\n", __raw_readl(MTK_WDT_REQ_MODE));
	pr_info("MTK_WDT_REQ_IRQ_EN:0x%x\n", __raw_readl(MTK_WDT_REQ_IRQ_EN));
	pr_info("MTK_WDT_EXT_REQ_CON:0x%x\n",
		__raw_readl(MTK_WDT_EXT_REQ_CON));
	pr_info("MTK_WDT_DEBUG_CTL:0x%x\n", __raw_readl(MTK_WDT_DEBUG_CTL));
	pr_info("MTK_WDT_LATCH_CTL:0x%x\n", __raw_readl(MTK_WDT_LATCH_CTL));
	pr_info("MTK_WDT_DEBUG_CTL2:0x%x\n", __raw_readl(MTK_WDT_DEBUG_CTL2));
	pr_info("MTK_WDT_COUNTER:0x%x\n", __raw_readl(MTK_WDT_COUNTER));
	pr_info("****************dump wdt reg end*************\n");

}

void aee_wdt_dump_reg(void)
{
/*
 *	aee_wdt_printf("***dump wdt reg start***\n");
 *	aee_wdt_printf("MODE:0x%x\n", __raw_readl(MTK_WDT_MODE));
 *	aee_wdt_printf("LENGTH:0x%x\n", __raw_readl(MTK_WDT_LENGTH));
 *	aee_wdt_printf("RESTART:0x%x\n", __raw_readl(MTK_WDT_RESTART));
 *	aee_wdt_printf("STATUS:0x%x\n", __raw_readl(MTK_WDT_STATUS));
 *	aee_wdt_printf("INTERVAL:0x%x\n", __raw_readl(MTK_WDT_INTERVAL));
 *	aee_wdt_printf("SWRST:0x%x\n", __raw_readl(MTK_WDT_SWRST));
 *	aee_wdt_printf("NONRST_REG:0x%x\n", __raw_readl(MTK_WDT_NONRST_REG));
 *	aee_wdt_printf("NONRST_REG2:0x%x\n", __raw_readl(MTK_WDT_NONRST_REG2));
 *	aee_wdt_printf("REQ_MODE:0x%x\n", __raw_readl(MTK_WDT_REQ_MODE));
 *	aee_wdt_printf("REQ_IRQ_EN:0x%x\n", __raw_readl(MTK_WDT_REQ_IRQ_EN));
 *	aee_wdt_printf("DRAMC_CTL:0x%x\n", __raw_readl(MTK_WDT_DEBUG_2_REG));
 *	aee_wdt_printf("LATCH_CTL:0x%x\n", __raw_readl(MTK_WDT_LATCH_CTL));
 *	aee_wdt_printf("***dump wdt reg end***\n");
 */
}

void wdt_arch_reset(char mode)
{
	unsigned int wdt_mode_val;
	struct device_node *np_rgu;

	pr_debug("%s: mode=0x%x\n", __func__, mode);
	np_rgu = of_find_compatible_node(NULL, NULL,
		rgu_of_match[0].compatible);

	if (!toprgu_base) {
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_info("RGU iomap failed\n");
		pr_debug("RGU base: 0x%p  RGU irq: %d\n",
			toprgu_base, wdt_irq_id);
	}

	spin_lock(&rgu_reg_operation_spinlock);

	/* Watchdog Rest */
	mt_reg_sync_writel(MTK_WDT_RESTART_KEY, MTK_WDT_RESTART);

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_MTK_PMIC
	/*
	 * Dump all PMIC registers value AND clear all PMIC
	 * exception registers before SW trigger WDT for easier
	 * issue debugging.
	 *
	 * This is added since X20 but shall be common in all future platforms.
	 *
	 * This shall be executed when WDT mechanism is enabled since
	 * it may hang inside, for example, IPI hang due to SSPM issue.
	 */
	pmic_pre_wdt_reset();
#endif
#endif

	wdt_mode_val = __raw_readl(MTK_WDT_MODE);

	pr_debug("%s: wdt_mode=0x%x\n", __func__, wdt_mode_val);

	/* clear autorestart bit: autoretart: 1, bypass power key,
	 * 0: not bypass power key
	 */
	wdt_mode_val &= (~MTK_WDT_MODE_AUTO_RESTART);

	/* make sure WDT mode is hw reboot mode, can not config isr mode */
	wdt_mode_val &= (~(MTK_WDT_MODE_IRQ | MTK_WDT_MODE_IRQ_LEVEL_EN |
			MTK_WDT_MODE_ENABLE | MTK_WDT_MODE_DUAL_MODE));

	if (mode & WD_SW_RESET_BYPASS_PWR_KEY) {
		/* Bypass power key reboot, We using auto_restart bit
		 * as by pass power key flag
		 */
		wdt_mode_val = wdt_mode_val | (MTK_WDT_MODE_KEY |
			MTK_WDT_MODE_EXTEN |
			MTK_WDT_MODE_AUTO_RESTART);
	} else
		wdt_mode_val = wdt_mode_val |
			(MTK_WDT_MODE_KEY | MTK_WDT_MODE_EXTEN);

	/*set latch register to 0 for SW reset*/
	/* mt_reg_sync_writel((MTK_WDT_LENGTH_CTL_KEY | 0x0),
	 *	MTK_WDT_LATCH_CTL);
	 */

	mt_reg_sync_writel(wdt_mode_val, MTK_WDT_MODE);

	/*
	 * disable ddr reserve mode if we are doing normal
	 * reboot to avoid unexpected dram issue.
	 * exception types:
	 *   0: normal
	 *   1: HWT
	 *   2: KE
	 *   3: nested panic
	 *   4: mrdump key
	 */
	if (!(mode & WD_SW_RESET_KEEP_DDR_RESERVE))
		mtk_rgu_dram_reserved(0);

	udelay(100);

	pr_debug("%s: sw reset happen!\n", __func__);

	__inner_flush_dcache_all();

	/* dump RGU registers */
	wdt_dump_reg();

	/* delay awhile to make above dump as complete as possible */
	udelay(100);

	/* trigger SW reset */
	mt_reg_sync_writel(MTK_WDT_SWRST_KEY, MTK_WDT_SWRST);

	spin_unlock(&rgu_reg_operation_spinlock);

	while (1) {
		/* check if system is alive for debugging */
		mdelay(100);
		pr_info("wdt_arch_reset: still alive\n");
		wdt_dump_reg();
		cpu_relax();
	}

}

int mtk_rgu_dram_reserved(int enable)
{
	unsigned int tmp;

	if (enable == 1) {
		/* enable ddr reserved mode */
		tmp = __raw_readl(MTK_WDT_MODE);
		tmp |= (MTK_WDT_MODE_DDR_RESERVE | MTK_WDT_MODE_KEY);
		mt_reg_sync_writel(tmp, MTK_WDT_MODE);
	} else if (enable == 0) {
		/* disable ddr reserved mode, set reset mode, */
		/* disable watchdog output reset signal */
		tmp = __raw_readl(MTK_WDT_MODE);
		tmp &= (~MTK_WDT_MODE_DDR_RESERVE);
		tmp |= MTK_WDT_MODE_KEY;
		mt_reg_sync_writel(tmp, MTK_WDT_MODE);
	}
	pr_info("%s: MTK_WDT_MODE(0x%x)\n",
		__func__, __raw_readl(MTK_WDT_MODE));

	return 0;
}

int mtk_rgu_cfg_emi_dcs(int enable)
{
	unsigned int tmp;

	tmp = __raw_readl(MTK_WDT_DEBUG_CTL2);

	if (enable == 1) {
		/* enable emi dcs */
		tmp |= MTK_WDT_DEBUG_CTL_EMI_DCS_EN;
	} else if (enable == 0) {
		/* disable emi dcs */
		tmp &= (~MTK_WDT_DEBUG_CTL_EMI_DCS_EN);
	} else
		return -1;

	tmp |= MTK_WDT_DEBUG_CTL2_KEY;
	mt_reg_sync_writel(tmp, MTK_WDT_DEBUG_CTL2);

	pr_info("%s: MTK_WDT_DEBUG_CTL2(0x%x)\n",
		__func__, __raw_readl(MTK_WDT_DEBUG_CTL2));

	return 0;
}

int mtk_rgu_cfg_dvfsrc(int enable)
{
	unsigned int dbg_ctl, latch;

	dbg_ctl = __raw_readl(MTK_WDT_DEBUG_CTL2);
	latch = __raw_readl(MTK_WDT_LATCH_CTL);

	if (enable == 1) {
		/* enable dvfsrc_en */
		dbg_ctl |= MTK_WDT_DEBUG_CTL_DVFSRC_EN;

		/* set dvfsrc_latch */
		latch |= MTK_WDT_LATCH_CTL_DVFSRC;
	} else {
		/* disable is not allowed */
		return -1;
	}

	dbg_ctl |= MTK_WDT_DEBUG_CTL2_KEY;
	mt_reg_sync_writel(dbg_ctl, MTK_WDT_DEBUG_CTL2);

	latch |= MTK_WDT_LATCH_CTL_KEY;
	mt_reg_sync_writel(latch, MTK_WDT_LATCH_CTL);

	pr_info("%s: MTK_WDT_DEBUG_CTL2(0x%x)\n",
		__func__, __raw_readl(MTK_WDT_DEBUG_CTL2));
	pr_info("%s: MTK_WDT_LATCH_CTL(0x%x)\n",
		__func__, __raw_readl(MTK_WDT_LATCH_CTL));

	return 0;
}

/*
 * Query if SYSRST has happened.
 *
 * Return:
 * 1: Happened.
 * 0: Not happened.
 */
int mtk_rgu_status_is_sysrst(void)
{
	return
	(__raw_readl(MTK_WDT_STATUS) & MTK_WDT_STATUS_SYSRST_RST) ? 1 : 0;
}

/*
 * Query if EINTRST has happened.
 *
 * Return:
 * 1: Happened.
 * 0: Not happened.
 */
int mtk_rgu_status_is_eintrst(void)
{
	return (__raw_readl(MTK_WDT_STATUS) & MTK_WDT_STATUS_EINT_RST) ? 1 : 0;
}

int mtk_rgu_mcu_cache_preserve(int enable)
{
	unsigned int tmp;

	if (enable == 1) {
		/* enable cache retention */
		tmp = __raw_readl(MTK_WDT_DEBUG_CTL);
		tmp |= (MTK_RG_MCU_CACHE_PRESERVE | MTK_WDT_DEBUG_CTL_KEY);
		mt_reg_sync_writel(tmp, MTK_WDT_DEBUG_CTL);
	} else if (enable == 0) {
		/* disable cache retention */
		tmp = __raw_readl(MTK_WDT_DEBUG_CTL);
		tmp &= (~MTK_RG_MCU_CACHE_PRESERVE);
		tmp |= MTK_WDT_DEBUG_CTL_KEY;
		mt_reg_sync_writel(tmp, MTK_WDT_DEBUG_CTL);
	}

	pr_info("%s: MTK_WDT_DEBUG_CTL(0x%x)\n",
		__func__, __raw_readl(MTK_WDT_DEBUG_CTL));

	return 0;
}

int mtk_wdt_swsysret_config(int bit, int set_value)
{
	unsigned int wdt_sys_val;

	spin_lock(&rgu_reg_operation_spinlock);
	wdt_sys_val = __raw_readl(MTK_WDT_SWSYSRST);
	pr_info("%s: before set wdt_sys_val =%x\n", __func__, wdt_sys_val);
	wdt_sys_val |= MTK_WDT_SWSYS_RST_KEY;
	switch (bit) {
	case MTK_WDT_SWSYS_RST_MD_RST:
		if (set_value == 1)
			wdt_sys_val |= MTK_WDT_SWSYS_RST_MD_RST;
		if (set_value == 0)
			wdt_sys_val &= ~MTK_WDT_SWSYS_RST_MD_RST;
		break;
	case MTK_WDT_SWSYS_RST_MFG_RST: /* MFG reset */
		if (set_value == 1)
			wdt_sys_val |= MTK_WDT_SWSYS_RST_MFG_RST;
		if (set_value == 0)
			wdt_sys_val &= ~MTK_WDT_SWSYS_RST_MFG_RST;
		break;
	case MTK_WDT_SWSYS_RST_C2K_RST: /* c2k reset */
		if (set_value == 1)
			wdt_sys_val |= MTK_WDT_SWSYS_RST_C2K_RST;
		if (set_value == 0)
			wdt_sys_val &= ~MTK_WDT_SWSYS_RST_C2K_RST;
		break;
	case MTK_WDT_SWSYS_RST_CONMCU_RST:
		if (set_value == 1)
			wdt_sys_val |= MTK_WDT_SWSYS_RST_CONMCU_RST;
		if (set_value == 0)
			wdt_sys_val &= ~MTK_WDT_SWSYS_RST_CONMCU_RST;
		break;
	}
	mt_reg_sync_writel(wdt_sys_val, MTK_WDT_SWSYSRST);
	spin_unlock(&rgu_reg_operation_spinlock);

	/* mdelay(10); */
	pr_info("%s: after set wdt_sys_val =%x,wdt_sys_val=%x\n", __func__,
		__raw_readl(MTK_WDT_SWSYSRST), wdt_sys_val);

	return 0;
}
EXPORT_SYMBOL(mtk_wdt_swsysret_config);

int mtk_wdt_request_en_set(int mark_bit, enum wk_req_en en)
{
	int res = 0;
	unsigned int tmp, ext_req_con;
	struct device_node *np_rgu;

	if (!toprgu_base) {
		np_rgu = of_find_compatible_node(NULL, NULL,
				rgu_of_match[0].compatible);
		toprgu_base = of_iomap(np_rgu, 0);

		if (!toprgu_base)
			pr_info("RGU iomap failed\n");

		pr_info("RGU base: 0x%p, RGU irq: %d\n",
			toprgu_base, wdt_irq_id);
	}

	spin_lock(&rgu_reg_operation_spinlock);
	tmp = __raw_readl(MTK_WDT_REQ_MODE);
	tmp |=  MTK_WDT_REQ_MODE_KEY;

	if (mark_bit == MTK_WDT_REQ_MODE_SPM_SCPSYS) {
		if (en == WD_REQ_EN)
			tmp |= (MTK_WDT_REQ_MODE_SPM_SCPSYS);
		if (en == WD_REQ_DIS)
			tmp &=  ~(MTK_WDT_REQ_MODE_SPM_SCPSYS);
	} else if (mark_bit == MTK_WDT_REQ_MODE_EINT) {
		if (en == WD_REQ_EN) {
			if (ext_debugkey_io_eint != -1) {
				pr_info("RGU ext_debugkey_io_eint is %d\n",
					ext_debugkey_io_eint);
				ext_req_con = (ext_debugkey_io_eint << 4) |
					0x01;
				mt_reg_sync_writel(ext_req_con,
					MTK_WDT_EXT_REQ_CON);
				tmp |= (MTK_WDT_REQ_MODE_EINT);
			} else {
				tmp &= ~(MTK_WDT_REQ_MODE_EINT);
				res = -1;
			}
		}
		if (en == WD_REQ_DIS)
			tmp &= ~(MTK_WDT_REQ_MODE_EINT);
	} else if (mark_bit == MTK_WDT_REQ_MODE_SYSRST) {
		if (en == WD_REQ_EN) {
			mt_reg_sync_writel(MTK_WDT_SYSDBG_DEG_EN1_KEY,
				MTK_WDT_SYSDBG_DEG_EN1);
			mt_reg_sync_writel(MTK_WDT_SYSDBG_DEG_EN2_KEY,
				MTK_WDT_SYSDBG_DEG_EN2);
			tmp |= (MTK_WDT_REQ_MODE_SYSRST);
		}
		if (en == WD_REQ_DIS)
			tmp &= ~(MTK_WDT_REQ_MODE_SYSRST);
	} else if (mark_bit == MTK_WDT_REQ_MODE_THERMAL) {
		if (en == WD_REQ_EN)
			tmp |= (MTK_WDT_REQ_MODE_THERMAL);
		if (en == WD_REQ_DIS)
			tmp &=  ~(MTK_WDT_REQ_MODE_THERMAL);
	} else
		res =  -1;

	mt_reg_sync_writel(tmp, MTK_WDT_REQ_MODE);
	spin_unlock(&rgu_reg_operation_spinlock);
	return res;
}

int mtk_wdt_request_mode_set(int mark_bit, enum wk_req_mode mode)
{
	int res = 0;
	unsigned int tmp;
	struct device_node *np_rgu;

	if (!toprgu_base) {
		np_rgu = of_find_compatible_node(NULL, NULL,
				rgu_of_match[0].compatible);
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_info("RGU iomap failed\n");
		pr_debug("RGU base: 0x%p  RGU irq: %d\n",
			toprgu_base, wdt_irq_id);
	}

	spin_lock(&rgu_reg_operation_spinlock);
	tmp = __raw_readl(MTK_WDT_REQ_IRQ_EN);
	tmp |= MTK_WDT_REQ_IRQ_KEY;

	if (mark_bit == MTK_WDT_REQ_MODE_SPM_SCPSYS) {
		if (mode == WD_REQ_IRQ_MODE)
			tmp |= (MTK_WDT_REQ_IRQ_SPM_SCPSYS_EN);
		if (mode == WD_REQ_RST_MODE)
			tmp &=  ~(MTK_WDT_REQ_IRQ_SPM_SCPSYS_EN);
	} else if (mark_bit == MTK_WDT_REQ_MODE_EINT) {
		if (mode == WD_REQ_IRQ_MODE)
			tmp |= (MTK_WDT_REQ_IRQ_EINT_EN);
		if (mode == WD_REQ_RST_MODE)
			tmp &= ~(MTK_WDT_REQ_IRQ_EINT_EN);
	} else if (mark_bit == MTK_WDT_REQ_MODE_SYSRST) {
		if (mode == WD_REQ_IRQ_MODE)
			tmp |= (MTK_WDT_REQ_IRQ_SYSRST_EN);
		if (mode == WD_REQ_RST_MODE)
			tmp &= ~(MTK_WDT_REQ_IRQ_SYSRST_EN);
	} else if (mark_bit == MTK_WDT_REQ_MODE_THERMAL) {
		if (mode == WD_REQ_IRQ_MODE)
			tmp |= (MTK_WDT_REQ_IRQ_THERMAL_EN);
		if (mode == WD_REQ_RST_MODE)
			tmp &=  ~(MTK_WDT_REQ_IRQ_THERMAL_EN);
	} else
		res =  -1;
	mt_reg_sync_writel(tmp, MTK_WDT_REQ_IRQ_EN);
	spin_unlock(&rgu_reg_operation_spinlock);
	return res;
}

/*this API is for C2K only
 * flag: 1 is to clear;0 is to set
 * shift: which bit need to do set or clear
 */
void mtk_wdt_set_c2k_sysrst(unsigned int flag, unsigned int shift)
{
	struct device_node *np_rgu;
	unsigned int ret;

	np_rgu = of_find_compatible_node(NULL, NULL,
			rgu_of_match[0].compatible);

	if (!toprgu_base) {
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_info("mtk_wdt_set_c2k_sysrst RGU iomap failed\n");
		pr_debug("mtk_wdt_set_c2k_sysrst RGU base: 0x%p  RGU irq: %d\n",
			toprgu_base, wdt_irq_id);
	}

	if (flag == 1) {
		ret = __raw_readl(MTK_WDT_SWSYSRST);
		ret &= (~(1 << shift));
		mt_reg_sync_writel((ret|MTK_WDT_SWSYS_RST_KEY),
			MTK_WDT_SWSYSRST);
	} else { /* means set x bit */
		ret = __raw_readl(MTK_WDT_SWSYSRST);
		ret |= ((1 << shift));
		mt_reg_sync_writel((ret|MTK_WDT_SWSYS_RST_KEY),
			MTK_WDT_SWSYSRST);
	}
}

int mtk_wdt_dfd_count_en(int value)
{
	unsigned int tmp;

	/* dfd_count_en is obsolete, enable dfd_en only here */

	if (value == 1) {
		/* enable dfd_en */
		tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
		tmp |= (MTK_WDT_DFD_EN|MTK_WDT_LATCH_CTL2_KEY);
		mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);
	} else if (value == 0) {
		/* disable dfd_en */
		tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
		tmp &= (~MTK_WDT_DFD_EN);
		tmp |= MTK_WDT_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);
	}
	pr_debug("mtk_wdt_dfd_en:MTK_WDT_LATCH_CTL2(0x%x)\n",
		__raw_readl(MTK_WDT_LATCH_CTL2));

	return 0;
}

int mtk_wdt_dfd_thermal1_dis(int value)
{
	unsigned int tmp;

	if (value == 1) {
		/* enable dfd count */
		tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
		tmp |= (MTK_WDT_DFD_THERMAL1_DIS | MTK_WDT_LATCH_CTL2_KEY);
		mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);
	} else if (value == 0) {
		/* disable dfd count */
		tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
		tmp &= (~MTK_WDT_DFD_THERMAL1_DIS);
		tmp |= MTK_WDT_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);
	}
	pr_debug("mtk_wdt_dfd_thermal1_dis:MTK_WDT_LATCH_CTL2(0x%x)\n",
		__raw_readl(MTK_WDT_LATCH_CTL2));

	return 0;
}

int mtk_wdt_dfd_thermal2_dis(int value)
{
	unsigned int tmp;

	if (value == 1) {
		/* enable dfd count */
		tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
		tmp |= (MTK_WDT_DFD_THERMAL2_DIS|MTK_WDT_LATCH_CTL2_KEY);
		mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);
	} else if (value == 0) {
		/* disable dfd count */
		tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
		tmp &= (~MTK_WDT_DFD_THERMAL2_DIS);
		tmp |= MTK_WDT_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);
	}
	pr_debug("mtk_wdt_dfd_thermal2_dis:MTK_WDT_LATCH_CTL2(0x%x)\n",
		__raw_readl(MTK_WDT_LATCH_CTL2));

	return 0;
}

int mtk_wdt_dfd_timeout(int value)
{
	unsigned int tmp;

	value = value << MTK_WDT_DFD_TIMEOUT_SHIFT;
	value = value & MTK_WDT_DFD_TIMEOUT_MASK;

	/* enable dfd count */
	tmp = __raw_readl(MTK_WDT_LATCH_CTL2);
	tmp &= (~MTK_WDT_DFD_TIMEOUT_MASK);
	tmp |= (value|MTK_WDT_LATCH_CTL2_KEY);
	mt_reg_sync_writel(tmp, MTK_WDT_LATCH_CTL2);

	pr_debug("mtk_wdt_dfd_timeout:MTK_WDT_LATCH_CTL2(0x%x)\n",
		__raw_readl(MTK_WDT_LATCH_CTL2));

	return 0;
}

#ifndef CONFIG_FIQ_GLUE
static void wdt_report_info(void)
{
	/* extern struct task_struct *wk_tsk; */
	struct task_struct *task;

	task = &init_task;
	pr_debug("Qwdt: -- watchdog time out\n");

	for_each_process(task) {
		if (task->state == 0) {
			pr_debug("PID: %d, name: %s\n backtrace:\n",
				task->pid, task->comm);
			show_stack(task, NULL);
			pr_debug("\n");
		}
	}

	pr_debug("backtrace of current task:\n");
	show_stack(NULL, NULL);
	pr_debug("Qwdt: -- watchdog time out\n");
}
#endif

#ifdef CONFIG_FIQ_GLUE
static void wdt_fiq(void *arg, void *regs, void *svc_sp)
{
	unsigned int wdt_mode_val;
	struct wd_api *wd_api = NULL;
get_wd_api(&wd_api);
	wdt_mode_val = __raw_readl(MTK_WDT_STATUS);
	mt_reg_sync_writel(wdt_mode_val, MTK_WDT_NONRST_REG);
    #ifdef	CONFIG_MTK_WD_KICKER
	aee_wdt_printf("\n kick=0x%08x,check=0x%08x,STA=%x\n",
		wd_api->wd_get_kick_bit(),
		wd_api->wd_get_check_bit(), wdt_mode_val);
	aee_wdt_dump_reg();
    #endif

	aee_wdt_fiq_info(arg, regs, svc_sp);
}
#else /* CONFIG_FIQ_GLUE */
static irqreturn_t mtk_wdt_isr(int irq, void *dev_id)
{
	pr_info("mtk_wdt_isr\n");

#ifndef __USING_DUMMY_WDT_DRV__ /* FPGA will set this flag */
	wdt_intr_has_trigger = 1;
	wdt_report_info();
	WARN_ON(1);
#endif

	return IRQ_HANDLED;
}
#endif /* CONFIG_FIQ_GLUE */

static irqreturn_t mtk_wdt_sspm_isr(int irq, void *dev_id)
{
	unsigned int reg;

	/*
	 * In those platforms SSPM WDT reset not connecting to AP RGU
	 * directly, we need an interrupt to handle SSPM WDT.
	 *
	 * Steps in interrupt handler:
	 *
	 *   1. Set SSPM flag in non-reset register (use NONRST_REG2).
	 *   2. Trigger AP RGU SW reset.
	 */

	pr_info("%s: SSPM reset!\n", __func__);

	reg = __raw_readl(MTK_WDT_NONRST_REG2);
	reg |= MTK_WDT_NONRST2_SSPM_RESET;
	mt_reg_sync_writel(reg, MTK_WDT_NONRST_REG2);

	wdt_arch_reset(1);

	return IRQ_HANDLED;
}


#else
/* ------------------------------------------------------------------------- */
/* Dummy functions */
/* ------------------------------------------------------------------------- */
void mtk_wdt_set_time_out_value(unsigned int value) {}
void mtk_wdt_mode_config(bool dual_mode_en, bool irq,
	bool ext_en, bool ext_pol, bool wdt_en) {}
int mtk_wdt_enable(enum wk_wdt_en en) { return 0; }
void mtk_wdt_restart(enum wd_restart_type type) {}
void wdt_arch_reset(char mode) {}
int  mtk_wdt_confirm_hwreboot(void){return 0; }
void mtk_wd_suspend(void){}
void mtk_wd_resume(void){}
void wdt_dump_reg(void){}
int mtk_wdt_swsysret_config(int bit, int set_value) { return 0; }
EXPORT_SYMBOL(mtk_wdt_swsysret_config);
int mtk_wdt_request_mode_set(int mark_bit, enum wk_req_mode mode) {return 0; }
int mtk_wdt_request_en_set(int mark_bit, enum wk_req_en en) {return 0; }
void mtk_wdt_set_c2k_sysrst(unsigned int flag) {}
int mtk_rgu_dram_reserved(int enable) {return 0; }
int mtk_rgu_cfg_emi_dcs(int enable) {return 0; }
int mtk_rgu_cfg_dvfsrc(int enable) {return 0; }
int mtk_rgu_mcu_cache_preserve(int enable) {return 0; }
int mtk_wdt_dfd_count_en(int value) {return 0; }
int mtk_wdt_dfd_thermal1_dis(int value) {return 0; }
int mtk_wdt_dfd_thermal2_dis(int value) {return 0; }
int mtk_wdt_dfd_timeout(int value) {return 0; }

#endif /* #ifndef __USING_DUMMY_WDT_DRV__ */

static int mtk_wdt_probe(struct platform_device *dev)
{
	int ret = 0;
	struct device_node *node;
	u32 ints[2] = { 0, 0 };

	pr_info("mtk wdt driver probe ..\n");

	if (!toprgu_base) {
		toprgu_base = of_iomap(dev->dev.of_node, 0);
		if (!toprgu_base) {
			pr_info("iomap failed\n");
			return -ENODEV;
		}
	}

	mtk_wdt_mark_stage(RGU_STAGE_KERNEL);

	/* get irq for AP WDT */
	if (!wdt_irq_id) {
		wdt_irq_id = irq_of_parse_and_map(dev->dev.of_node, 0);
		if (!wdt_irq_id) {
			pr_info("get wdt_irq_id failed, ret: %d\n", wdt_irq_id);
			return -ENODEV;
		}
	}

	/* get irq for SSPM WDT which informs AP */
	if (!wdt_sspm_irq_id) {
		wdt_sspm_irq_id = irq_of_parse_and_map(dev->dev.of_node, 1);
		if (!wdt_sspm_irq_id) {
			/*
			 * bypass fail of getting SSPM IRQ because not all
			 * platforms need this feature.
			 */
			pr_info("wdt_sspm_irq_id is not found\n");
			wdt_sspm_irq_id = 0;
		}
	}

	pr_debug("base: 0x%p, wdt_irq_id: %d, wdt_sspm_irq_id: %d\n",
		toprgu_base, wdt_irq_id, wdt_sspm_irq_id);

	node = of_find_compatible_node(NULL, NULL,
			"mediatek, mrdump_ext_rst-eint");

	if (node) {
		ret = of_property_read_u32_array(node, "interrupts",
			ints, ARRAY_SIZE(ints));
		if (!ret)
			ext_debugkey_io_eint = ints[0];
		else
			pr_info("failed to get interrupt mrdump_ext_rst-eint node\n");
	}

	pr_info("ext_debugkey_eint=%d\n", ext_debugkey_io_eint);

#ifndef __USING_DUMMY_WDT_DRV__ /* FPGA will set this flag */

#ifndef CONFIG_FIQ_GLUE
	pr_debug("!CONFIG_FIQ_GLUE: request IRQ\n");
    #ifdef CONFIG_KICK_SPM_WDT
	ret = spm_wdt_register_irq((irq_handler_t)mtk_wdt_isr);
    #else
	ret = request_irq(AP_RGU_WDT_IRQ_ID, (irq_handler_t)mtk_wdt_isr,
			IRQF_TRIGGER_NONE, "mt_wdt", NULL);
    #endif		/* CONFIG_KICK_SPM_WDT */
#else
	pr_debug("CONFIG_FIQ_GLUE: request FIQ\n");
    #ifdef CONFIG_KICK_SPM_WDT
	ret = spm_wdt_register_fiq(wdt_fiq);
    #else
	ret = request_fiq(AP_RGU_WDT_IRQ_ID, wdt_fiq,
			IRQF_TRIGGER_FALLING, NULL);
    #endif		/* CONFIG_KICK_SPM_WDT */
#endif

	if (ret != 0) {
		pr_info("failed to request wdt_irq_id %d, ret %d\n",
			wdt_irq_id, ret);
		return ret;
	}

	if (wdt_sspm_irq_id) {
		ret = request_irq(AP_RGU_SSPM_WDT_IRQ_ID,
			(irq_handler_t)mtk_wdt_sspm_isr,
			IRQF_TRIGGER_HIGH, "mt_sspm_wdt", NULL);

		if (ret != 0) {
			pr_info("failed to request wdt_sspm_irq_id %d, ret %d\n",
				wdt_sspm_irq_id, ret);

			/* bypass fail of SSPM IRQ related behavior
			 *because this is not critical
			 */
		}
	}

    #ifdef CONFIG_KICK_SPM_WDT
	spm_wdt_init();
    #endif

	/* Set timeout vale and restart counter */
	wdt_last_timeout_val = 30;
	mtk_wdt_set_time_out_value(wdt_last_timeout_val);

	mtk_wdt_restart(WD_TYPE_NORMAL);

    #ifdef CONFIG_MTK_WD_KICKER	/* Initialize to dual mode */
	pr_debug("WDT (dual mode) enabled.\n");
	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
	#else				/* Initialize to disable wdt */
	pr_debug("WDT disabled.\n");
	mtk_wdt_mode_config(FALSE, FALSE, TRUE, FALSE, FALSE);
	wdt_enable = 0;
	#endif

	/* Reset External debug key */
	mtk_wdt_request_en_set(MTK_WDT_REQ_MODE_SYSRST, WD_REQ_DIS);
	mtk_wdt_request_en_set(MTK_WDT_REQ_MODE_EINT, WD_REQ_DIS);
	mtk_wdt_request_mode_set(MTK_WDT_REQ_MODE_SYSRST, WD_REQ_IRQ_MODE);
	mtk_wdt_request_mode_set(MTK_WDT_REQ_MODE_EINT, WD_REQ_IRQ_MODE);

#else /* __USING_DUMMY_WDT_DRV__ */

	/* dummy assignment */

#endif /* __USING_DUMMY_WDT_DRV__ */

	udelay(100);
	pr_debug("WDT_MODE(0x%x), WDT_NONRST_REG(0x%x)\n",
		__raw_readl(MTK_WDT_MODE), __raw_readl(MTK_WDT_NONRST_REG));
	pr_debug("WDT_REQ_MODE(0x%x)\n", __raw_readl(MTK_WDT_REQ_MODE));
	pr_debug("WDT_REQ_IRQ_EN(0x%x)\n", __raw_readl(MTK_WDT_REQ_IRQ_EN));

	return ret;
}

static int mtk_wdt_remove(struct platform_device *dev)
{
	pr_debug("******** MTK wdt driver remove!! ********\n");

#ifndef __USING_DUMMY_WDT_DRV__ /* FPGA will set this flag */
	free_irq(AP_RGU_WDT_IRQ_ID, NULL);
#endif
	return 0;
}

static void mtk_wdt_shutdown(struct platform_device *dev)
{
	pr_debug("******** MTK WDT driver shutdown!! ********\n");

	/* mtk_wdt_ModeSelection(KAL_FALSE, KAL_FALSE, KAL_FALSE); */
	/* kick external wdt */
	/* mtk_wdt_mode_config(TRUE, FALSE, FALSE, FALSE, FALSE); */

	mtk_wdt_restart(WD_TYPE_NORMAL);
	pr_debug("******** MTK WDT driver shutdown done ********\n");
}

static struct platform_driver mtk_wdt_driver = {

	.driver     = {
		.name	= "mtk-wdt",
		.of_match_table = rgu_of_match,
	},
	.probe	= mtk_wdt_probe,
	.remove	= mtk_wdt_remove,
	.shutdown	= mtk_wdt_shutdown,
/* .suspend	= mtk_wdt_suspend, */
/* .resume	= mtk_wdt_resume, */
};

#ifdef CONFIG_KICK_SPM_WDT
static void spm_wdt_init(void)
{
	unsigned int tmp;
	/* set scpsys reset mode , not trigger irq */
	/* #ifndef CONFIG_ARM64 */
	/*6795 Macro*/
	tmp = __raw_readl(MTK_WDT_REQ_MODE);
	tmp |=  MTK_WDT_REQ_MODE_KEY;
	tmp |= (MTK_WDT_REQ_MODE_SPM_SCPSYS);
	mt_reg_sync_writel(tmp, MTK_WDT_REQ_MODE);

	tmp = __raw_readl(MTK_WDT_REQ_IRQ_EN);
	tmp |= MTK_WDT_REQ_IRQ_KEY;
	tmp &= ~(MTK_WDT_REQ_IRQ_SPM_SCPSYS_EN);
	mt_reg_sync_writel(tmp, MTK_WDT_REQ_IRQ_EN);
	/* #endif */

	pr_debug("mtk_wdt_init [MTK_WDT] not use RGU WDT use_SPM_WDT!!n");

	tmp = __raw_readl(MTK_WDT_MODE);
	tmp |= MTK_WDT_MODE_KEY;
	/* disable wdt */
	tmp &= (~(MTK_WDT_MODE_IRQ|MTK_WDT_MODE_ENABLE|MTK_WDT_MODE_DUAL_MODE));

	/* Bit 4: WDT_Auto_restart, this is a reserved bit,
	 *we use it as bypass powerkey flag.
	 */
	/* Because HW reboot always need reboot to kernel, we set it always. */
	tmp |= MTK_WDT_MODE_AUTO_RESTART;
	/* BIt2  ext signal */
	tmp |= MTK_WDT_MODE_EXTEN;
	mt_reg_sync_writel(tmp, MTK_WDT_MODE);

}
#endif

/*
 * init and exit function
 */
static int __init mtk_wdt_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_wdt_driver);
	if (ret) {
		pr_info("[mtk_wdt_driver] Unable to register driver (%d)\n",
			ret);
		return ret;
	}
	pr_info("mtk_wdt_init ok\n");
	return 0;
}

static void __exit mtk_wdt_exit(void)
{
}

/*
 * this function is for those user who need WDT APIs before WDT driver's probe
 */
static int __init mtk_wdt_get_base_addr(void)
{
	struct device_node *np_rgu;

	for_each_matching_node(np_rgu, rgu_of_match) {
		pr_info("%s: compatible node found: %s\n",
			__func__, np_rgu->name);
		break;
	}

	if (!toprgu_base) {
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_info("%s: rgu iomap failed\n", __func__);

		pr_debug("rgu base: 0x%p\n", toprgu_base);
	}

	return 0;
}
core_initcall(mtk_wdt_get_base_addr);
postcore_initcall(mtk_wdt_init);
module_exit(mtk_wdt_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("Watchdog Device Driver");
MODULE_LICENSE("GPL");
