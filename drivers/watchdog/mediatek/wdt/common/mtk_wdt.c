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

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>
#include <linux/irqchip/mtk-gic-extend.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include "mt_wdt.h"
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <mt-plat/aee.h>
#include <ext_wd_drv.h>

#include <mach/wd_api.h>

#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/reset.h>

void __iomem *toprgu_base = 0;
unsigned int wdt_irq_id = 0;
#define AP_RGU_WDT_IRQ_ID    wdt_irq_id

#define DRV_NAME "mtk-wdt"

static const struct of_device_id rgu_of_match[] = {
	{.compatible = "mediatek,mt2701-rgu"},
	{.compatible = "mediatek,mt8127-rgu"},
	{.compatible = "mediatek,mt8163-rgu"},
	{.compatible = "mediatek,mt8173-rgu"},
	{}
};

MODULE_DEVICE_TABLE(of, rgu_of_match);

/*
 * internal variables
 */
static DEFINE_SPINLOCK(rgu_reg_operation_spinlock);
static unsigned int timeout;

static int g_last_time_time_out_value;
static int g_wdt_enable = 1;

struct toprgu_reset {
	spinlock_t lock;
	void __iomem *toprgu_swrst_base;
	int regofs;
	struct reset_controller_dev rcdev;
};

static int toprgu_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	unsigned int tmp;
	unsigned long flags;
	struct toprgu_reset *data = container_of(rcdev, struct toprgu_reset, rcdev);

	spin_lock_irqsave(&data->lock, flags);

	tmp = __raw_readl(data->toprgu_swrst_base + data->regofs);
	tmp |= BIT(id);
	tmp |= MTK_WDT_SWSYS_RST_KEY;
	writel(tmp, data->toprgu_swrst_base + data->regofs);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int toprgu_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	unsigned int tmp;
	unsigned long flags;
	struct toprgu_reset *data = container_of(rcdev, struct toprgu_reset, rcdev);

	spin_lock_irqsave(&data->lock, flags);

	tmp = __raw_readl(data->toprgu_swrst_base + data->regofs);
	tmp &= ~BIT(id);
	tmp |= MTK_WDT_SWSYS_RST_KEY;
	writel(tmp, data->toprgu_swrst_base + data->regofs);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int toprgu_reset(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	int ret;

	ret = toprgu_reset_assert(rcdev, id);
	if (ret)
		return ret;

	return toprgu_reset_deassert(rcdev, id);
}

static struct reset_control_ops toprgu_reset_ops = {
	.assert = toprgu_reset_assert,
	.deassert = toprgu_reset_deassert,
	.reset = toprgu_reset,
};

static void toprgu_register_reset_controller(struct device_node *np,
		void __iomem *toprgu_base, int regofs)
{
	struct toprgu_reset *data;
	int ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return;

	spin_lock_init(&data->lock);

	data->toprgu_swrst_base = toprgu_base;
	data->regofs = regofs;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = 15;
	data->rcdev.ops = &toprgu_reset_ops;
	data->rcdev.of_node = np;

	ret = reset_controller_register(&data->rcdev);
	if (ret) {
		pr_err("could not register toprgu reset controller: %d\n", ret);
		kfree(data);
		return;
	}
}

#ifndef __USING_DUMMY_WDT_DRV__	/* FPGA will set this flag */
/*
    this function set the timeout value.
    value: second
*/
void mtk_wdt_set_time_out_value(unsigned int value)
{
	/*
	 * TimeOut = BitField 15:5
	 * Key     = BitField  4:0 = 0x08
	 */
	spin_lock(&rgu_reg_operation_spinlock);

	/* 1 tick means 512 * T32K -> 1s = T32/512 tick = 64 */
	/* --> value * (1<<6) */
	timeout = (unsigned int)(value * (1 << 6));
	timeout = timeout << 5;
	writel((timeout | MTK_WDT_LENGTH_KEY), MTK_WDT_LENGTH);

	spin_unlock(&rgu_reg_operation_spinlock);
}

/*
    watchdog mode:
    debug_en:   debug module reset enable.
    irq:        generate interrupt instead of reset
    ext_en:     output reset signal to outside
    ext_pol:    polarity of external reset signal
    wdt_en:     enable watch dog timer
*/
void mtk_wdt_mode_config(bool dual_mode_en, bool irq, bool ext_en, bool ext_pol, bool wdt_en)
{
	unsigned int tmp;

	spin_lock(&rgu_reg_operation_spinlock);

	/* pr_debug(" mtk_wdt_mode_config  mode value=%x,pid=%d\n",DRV_Reg32(MTK_WDT_MODE),current->pid); */
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

	/* Bit 4: WDT_Auto_restart, this is a reserved bit, we use it as bypass powerkey flag. */
	/* Because HW reboot always need reboot to kernel, we set it always. */
	tmp |= MTK_WDT_MODE_AUTO_RESTART;

	writel(tmp, MTK_WDT_MODE);
	/* dual_mode(1); //always dual mode */
	/* mdelay(100); */
	pr_debug(" mtk_wdt_mode_config  mode value=%x, tmp:%x,pid=%d\n", __raw_readl(MTK_WDT_MODE), tmp, current->pid);

	spin_unlock(&rgu_reg_operation_spinlock);
}

/* EXPORT_SYMBOL(mtk_wdt_mode_config); */

int mtk_wdt_enable(enum wk_wdt_en en)
{
	unsigned int tmp;

	spin_lock(&rgu_reg_operation_spinlock);

	tmp = __raw_readl(MTK_WDT_MODE);

	tmp |= MTK_WDT_MODE_KEY;
	if (WK_WDT_EN == en) {
		tmp |= MTK_WDT_MODE_ENABLE;
		g_wdt_enable = 1;
	} else if (WK_WDT_DIS == en) {
		tmp &= ~MTK_WDT_MODE_ENABLE;
		g_wdt_enable = 0;
	}
	pr_debug("mtk_wdt_enable value=%x,pid=%d\n", tmp, current->pid);
	writel(tmp, MTK_WDT_MODE);
	spin_unlock(&rgu_reg_operation_spinlock);
	return 0;
}

int mtk_wdt_confirm_hwreboot(void)
{
	/* aee need confirm wd can hw reboot */
	/* pr_debug("mtk_wdt_probe : Initialize to dual mode\n"); */
	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
	return 0;
}


void mtk_wdt_restart(enum wd_restart_type type)
{
	/* pr_debug("WDT:[mtk_wdt_restart] type  =%d, pid=%d\n",type,current->pid); */

	if (type == WD_TYPE_NORMAL) {
		spin_lock(&rgu_reg_operation_spinlock);
		writel(MTK_WDT_RESTART_KEY, MTK_WDT_RESTART);
		spin_unlock(&rgu_reg_operation_spinlock);
	} else if (type == WD_TYPE_NOLOCK) {
		*(u32 *)MTK_WDT_RESTART = MTK_WDT_RESTART_KEY;
	} else
		pr_debug("WDT:[mtk_wdt_restart] type=%d error pid =%d\n", type, current->pid);
}

void wdt_dump_reg(void)
{
	pr_alert("****************dump wdt reg start*************\n");
	pr_alert("MTK_WDT_MODE:0x%x\n", __raw_readl(MTK_WDT_MODE));
	pr_alert("MTK_WDT_LENGTH:0x%x\n", __raw_readl(MTK_WDT_LENGTH));
	pr_alert("MTK_WDT_RESTART:0x%x\n", __raw_readl(MTK_WDT_RESTART));
	pr_alert("MTK_WDT_STATUS:0x%x\n", __raw_readl(MTK_WDT_STATUS));
	pr_alert("MTK_WDT_INTERVAL:0x%x\n", __raw_readl(MTK_WDT_INTERVAL));
	pr_alert("MTK_WDT_SWRST:0x%x\n", __raw_readl(MTK_WDT_SWRST));
	pr_alert("MTK_WDT_NONRST_REG:0x%x\n", __raw_readl(MTK_WDT_NONRST_REG));
	pr_alert("MTK_WDT_NONRST_REG2:0x%x\n", __raw_readl(MTK_WDT_NONRST_REG2));
	pr_alert("MTK_WDT_REQ_MODE:0x%x\n", __raw_readl(MTK_WDT_REQ_MODE));
	pr_alert("MTK_WDT_REQ_IRQ_EN:0x%x\n", __raw_readl(MTK_WDT_REQ_IRQ_EN));
	pr_alert("MTK_WDT_DRAMC_CTL:0x%x\n", __raw_readl(MTK_WDT_DRAMC_CTL));
	pr_alert("****************dump wdt reg end*************\n");
}

void wdt_arch_reset(char mode)
{
	unsigned int wdt_mode_val;
	struct device_node *np_rgu = NULL;
	int i;

	pr_debug("wdt_arch_reset called@Kernel mode =%c\n", mode);

	for (i = 0; rgu_of_match[i].compatible; i++) {
		np_rgu = of_find_compatible_node(NULL, NULL, rgu_of_match[i].compatible);
		if (np_rgu)
			break;
	}

	if (!toprgu_base) {
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_err("RGU iomap failed\n");
		pr_debug("RGU base: 0x%p  RGU irq: %d\n", toprgu_base, wdt_irq_id);
		}

	spin_lock(&rgu_reg_operation_spinlock);
	/* Watchdog Rest */
	writel(MTK_WDT_RESTART_KEY, MTK_WDT_RESTART);
	wdt_mode_val = __raw_readl(MTK_WDT_MODE);
	pr_debug("wdt_arch_reset called MTK_WDT_MODE =%x\n", wdt_mode_val);
	/* clear autorestart bit: autoretart: 1, bypass power key, 0: not bypass power key */
	wdt_mode_val &= ~MTK_WDT_MODE_AUTO_RESTART;
	/* make sure WDT mode is hw reboot mode, can not config isr mode  */
	wdt_mode_val &= ~(MTK_WDT_MODE_IRQ | MTK_WDT_MODE_ENABLE | MTK_WDT_MODE_DUAL_MODE);
	if (mode) {
		/* mode != 0 means by pass power key reboot, We using auto_restart bit as by pass power key flag */
		wdt_mode_val = wdt_mode_val | (MTK_WDT_MODE_KEY|MTK_WDT_MODE_EXTEN|MTK_WDT_MODE_AUTO_RESTART);
	} else
		wdt_mode_val = wdt_mode_val | (MTK_WDT_MODE_KEY | MTK_WDT_MODE_EXTEN);

	writel(wdt_mode_val, MTK_WDT_MODE);
	pr_debug("wdt_arch_reset called end MTK_WDT_MODE =%x\n", wdt_mode_val);
	udelay(100);
	writel(MTK_WDT_SWRST_KEY, MTK_WDT_SWRST);
	pr_debug("wdt_arch_reset: SW_reset happen\n");
	spin_unlock(&rgu_reg_operation_spinlock);

	while (1) {
		wdt_dump_reg();
		pr_err("wdt_arch_reset error\n");
	}

}

int mtk_rgu_dram_reserved(int enable)
{
	pr_debug("mtk_rgu_dram_reserved:MTK_WDT_MODE(0x%x)\n", __raw_readl(MTK_WDT_MODE));
	return 0;
}

int mtk_wdt_swsysret_config(int bit, int set_value)
{
	unsigned int wdt_sys_val;

	spin_lock(&rgu_reg_operation_spinlock);
	wdt_sys_val = __raw_readl(MTK_WDT_SWSYSRST);
	pr_debug("fwq2 before set wdt_sys_val =%x\n", wdt_sys_val);
	wdt_sys_val |= MTK_WDT_SWSYS_RST_KEY;
	switch (bit) {
	case MTK_WDT_SWSYS_RST_MD_RST:
		if (1 == set_value)
			wdt_sys_val |= MTK_WDT_SWSYS_RST_MD_RST;
		else if (0 == set_value)
			wdt_sys_val &= ~MTK_WDT_SWSYS_RST_MD_RST;
		break;
	case MTK_WDT_SWSYS_RST_MD_LITE_RST:
		if (1 == set_value)
			wdt_sys_val |= MTK_WDT_SWSYS_RST_MD_LITE_RST;
		else if (0 == set_value)
			wdt_sys_val &= ~MTK_WDT_SWSYS_RST_MD_LITE_RST;
		break;
	}
	writel(wdt_sys_val, MTK_WDT_SWSYSRST);
	spin_unlock(&rgu_reg_operation_spinlock);

	mdelay(10);
	pr_debug("after set wdt_sys_val =%x,wdt_sys_val=%x\n", __raw_readl(MTK_WDT_SWSYSRST), wdt_sys_val);
	return 0;
}

int mtk_wdt_request_en_set(int mark_bit, WD_REQ_CTL en)
{
	int res = 0;
	unsigned int tmp;

	spin_lock(&rgu_reg_operation_spinlock);
	tmp = __raw_readl(MTK_WDT_REQ_MODE);
	tmp |= MTK_WDT_REQ_MODE_KEY;

	if (MTK_WDT_REQ_MODE_SPM_SCPSYS == mark_bit) {
		if (WD_REQ_EN == en)
			tmp |= (MTK_WDT_REQ_MODE_SPM_SCPSYS);
		else if (WD_REQ_DIS == en)
			tmp &= ~MTK_WDT_REQ_MODE_SPM_SCPSYS;
	} else if (MTK_WDT_REQ_MODE_SPM_THERMAL == mark_bit) {
		if (WD_REQ_EN == en)
			tmp |= (MTK_WDT_REQ_MODE_SPM_THERMAL);
		else if (WD_REQ_DIS == en)
			tmp &= ~MTK_WDT_REQ_MODE_SPM_THERMAL;
	} else if (MTK_WDT_REQ_MODE_THERMAL == mark_bit) {
		if (WD_REQ_EN == en)
			tmp |= (MTK_WDT_REQ_MODE_THERMAL);
		else if (WD_REQ_DIS == en)
			tmp &= ~MTK_WDT_REQ_MODE_THERMAL;
	} else
		res = -1;

	writel(tmp, MTK_WDT_REQ_MODE);
	spin_unlock(&rgu_reg_operation_spinlock);
	return res;
}

int mtk_wdt_request_mode_set(int mark_bit, WD_REQ_MODE mode)
{
	int res = 0;
	unsigned int tmp;

	spin_lock(&rgu_reg_operation_spinlock);
	tmp = __raw_readl(MTK_WDT_REQ_IRQ_EN);
	tmp |= MTK_WDT_REQ_IRQ_KEY;

	if (MTK_WDT_REQ_MODE_SPM_SCPSYS == mark_bit) {
		if (WD_REQ_IRQ_MODE == mode)
			tmp |= (MTK_WDT_REQ_IRQ_SPM_SCPSYS_EN);
		else if (WD_REQ_RST_MODE == mode)
			tmp &= ~(MTK_WDT_REQ_IRQ_SPM_SCPSYS_EN);
	} else if (MTK_WDT_REQ_MODE_SPM_THERMAL == mark_bit) {
		if (WD_REQ_IRQ_MODE == mode)
			tmp |= (MTK_WDT_REQ_IRQ_SPM_THERMAL_EN);
		else if (WD_REQ_RST_MODE == mode)
			tmp &= ~MTK_WDT_REQ_IRQ_SPM_THERMAL_EN;
	} else if (MTK_WDT_REQ_MODE_THERMAL == mark_bit) {
		if (WD_REQ_IRQ_MODE == mode)
			tmp |= (MTK_WDT_REQ_IRQ_THERMAL_EN);
		else if (WD_REQ_RST_MODE == mode)
			tmp &= ~MTK_WDT_REQ_IRQ_THERMAL_EN;
	} else
		res = -1;
	writel(tmp, MTK_WDT_REQ_IRQ_EN);
	spin_unlock(&rgu_reg_operation_spinlock);
	return res;
}

#else
/* ------------------------------------------------------------------------------------------------- */
/* Dummy functions */
/* ------------------------------------------------------------------------------------------------- */
void mtk_wdt_set_time_out_value(unsigned int value) {}
static void mtk_wdt_set_reset_length(unsigned int value) {}
void mtk_wdt_mode_config(bool dual_mode_en, bool irq,	bool ext_en, bool ext_pol, bool wdt_en) {}
int mtk_wdt_enable(enum wk_wdt_en en) { return 0; }
void mtk_wdt_restart(enum wd_restart_type type) {}
static void mtk_wdt_sw_trigger(void){}
static unsigned char mtk_wdt_check_status(void){ return 0; }
void wdt_arch_reset(char mode) {}
int  mtk_wdt_confirm_hwreboot(void){return 0; }
void mtk_wd_suspend(void){}
void mtk_wd_resume(void){}
void wdt_dump_reg(void){}
int mtk_wdt_swsysret_config(int bit, int set_value) { return 0; }
int mtk_wdt_request_mode_set(int mark_bit, WD_REQ_MODE mode) {return 0; }
int mtk_wdt_request_en_set(int mark_bit, WD_REQ_CTL en) {return 0; }
int mtk_rgu_dram_reserved(int enable) {return 0; }

#endif				/* #ifndef __USING_DUMMY_WDT_DRV__ */

#ifndef CONFIG_FIQ_GLUE
static void wdt_report_info(void)
{
	struct task_struct *task;

	task = &init_task;
	pr_debug("Qwdt: -- watchdog time out\n");

	for_each_process(task) {
		if (task->state == 0) {
			pr_debug("PID: %d, name: %s\n backtrace:\n", task->pid, task->comm);
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
	writel(wdt_mode_val, MTK_WDT_NONRST_REG);
#ifdef	CONFIG_MTK_WD_KICKER
	aee_wdt_printf("\n kick=0x%08x,check=0x%08x,STA=%x\n", wd_api->wd_get_kick_bit(),
		wd_api->wd_get_check_bit(), wdt_mode_val);
#endif

#ifdef	CONFIG_MTK_AEE_FEATURE
	aee_wdt_fiq_info(arg, regs, svc_sp);
#endif
}
#else				/* CONFIG_FIQ_GLUE */
static irqreturn_t mtk_wdt_isr(int irq, void *dev_id)
{
	writel(__raw_readl(MTK_WDT_STATUS), MTK_WDT_NONRST_REG);
	pr_err("fwq mtk_wdt_isr\n");

#ifndef __USING_DUMMY_WDT_DRV__	/* FPGA will set this flag */

	wdt_report_info();
	BUG();

#endif
	return IRQ_HANDLED;
}
#endif				/* CONFIG_FIQ_GLUE */

/*
 * Device interface
 */
static int mtk_wdt_probe(struct platform_device *dev)
{
	int ret = 0;
	unsigned int interval_val;

	if (!toprgu_base) {
		toprgu_base = of_iomap(dev->dev.of_node, 0);
		if (!toprgu_base) {
			pr_err("RGU iomap failed\n");
			return -ENODEV;
		}
	}
	if (!wdt_irq_id) {
		wdt_irq_id = irq_of_parse_and_map(dev->dev.of_node, 0);
		if (!wdt_irq_id) {
			pr_err("RGU get IRQ ID failed\n");
			return -ENODEV;
		}
	}
	pr_debug("RGU base: 0x%p  RGU irq: %d\n", toprgu_base, wdt_irq_id);

#ifndef __USING_DUMMY_WDT_DRV__	/* FPGA will set this flag */
#ifndef CONFIG_FIQ_GLUE
	pr_err("*** MTK WDT register irq ***\n");
	ret = request_irq(AP_RGU_WDT_IRQ_ID, (irq_handler_t)mtk_wdt_isr,
			  IRQF_TRIGGER_NONE, DRV_NAME, NULL);
#else
	wdt_irq_id = get_hardware_irq(wdt_irq_id);
	pr_err("*** MTK WDT register fiq: fiq number is %d ***\n", wdt_irq_id);
	ret = request_fiq(AP_RGU_WDT_IRQ_ID, wdt_fiq, IRQF_TRIGGER_FALLING, NULL);
#endif

	if (ret != 0) {
		pr_err("mtk_wdt_probe : failed to request irq (%d)\n", ret);
		return ret;
	}

	/* Set timeout vale and restart counter */
	g_last_time_time_out_value = 30;
	mtk_wdt_set_time_out_value(g_last_time_time_out_value);

	mtk_wdt_restart(WD_TYPE_NORMAL);

	/**
	 * Set the reset length: we will set a special magic key.
	 * For Power off and power on reset, the INTERVAL default value is 0x7FF.
	 * We set Interval[1:0] to different value to distinguish different stage.
	 * Enter pre-loader, we will set it to 0x0
	 * Enter u-boot, we will set it to 0x1
	 * Enter kernel, we will set it to 0x2
	 * And the default value is 0x3 which means reset from a power off and power on reset
	 */
#define POWER_OFF_ON_MAGIC	(0x3)
#define PRE_LOADER_MAGIC	(0x0)
#define U_BOOT_MAGIC		(0x1)
#define KERNEL_MAGIC		(0x2)
#define MAGIC_NUM_MASK		(0x3)

#ifdef CONFIG_MTK_WD_KICKER	/* Initialize to dual mode */
	pr_debug("mtk_wdt_probe : Initialize to dual mode\n");
	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
#else				/* Initialize to disable wdt */
	pr_debug("mtk_wdt_probe : Initialize to disable wdt\n");
	mtk_wdt_mode_config(FALSE, FALSE, TRUE, FALSE, FALSE);
	g_wdt_enable = 0;
#endif

	/* Update interval register value and check reboot flag */
	interval_val = __raw_readl(MTK_WDT_INTERVAL);
	interval_val &= ~(MAGIC_NUM_MASK);
	interval_val |= (KERNEL_MAGIC);
	/* Write back INTERVAL REG */
	writel(interval_val, MTK_WDT_INTERVAL);
#endif
	udelay(100);
	pr_err("mtk_wdt_probe: WDT_MODE(%x),MTK_WDT_NONRST_REG(%x)\n",
		__raw_readl(MTK_WDT_MODE), __raw_readl(MTK_WDT_NONRST_REG));

	toprgu_register_reset_controller(dev->dev.of_node, toprgu_base, 0x18);

	return ret;
}

static int mtk_wdt_remove(struct platform_device *dev)
{
	pr_debug("******** MTK wdt driver remove!! ********\n");

#ifndef __USING_DUMMY_WDT_DRV__	/* FPGA will set this flag */
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

void mtk_wd_suspend(void)
{
	mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, FALSE);

	mtk_wdt_restart(WD_TYPE_NORMAL);

	/*aee_sram_printk("[WDT] suspend\n");*/
	pr_debug("[WDT] suspend\n");
}

void mtk_wd_resume(void)
{
	if (g_wdt_enable == 1) {
		mtk_wdt_set_time_out_value(g_last_time_time_out_value);
		mtk_wdt_mode_config(TRUE, TRUE, TRUE, FALSE, TRUE);
		mtk_wdt_restart(WD_TYPE_NORMAL);
	}

	/*aee_sram_printk("[WDT] resume(%d)\n", g_wdt_enable);*/
	pr_debug("[WDT] resume(%d)\n", g_wdt_enable);
}

int mtk_wd_SetNonResetReg2(unsigned int offset, bool value)
{
	unsigned int tmp;

	spin_lock(&rgu_reg_operation_spinlock);

	tmp = __raw_readl(MTK_WDT_NONRST_REG2);
	if (value)
		tmp |= 1 << offset;
	else
		tmp &= ~(1 << offset);
	writel(tmp, MTK_WDT_NONRST_REG2);

	spin_unlock(&rgu_reg_operation_spinlock);

	return __raw_readl(MTK_WDT_NONRST_REG2);
}
static struct platform_driver mtk_wdt_driver = {
	.probe = mtk_wdt_probe,
	.remove = mtk_wdt_remove,
	.shutdown = mtk_wdt_shutdown,
	/*.suspend = mtk_wdt_suspend,
	.resume = mtk_wdt_resume,*/
	.driver = {
		   .name = DRV_NAME,
		   .of_match_table = rgu_of_match,
	},
};

/* this function is for those user who need WDT APIs before WDT driver's probe */
static int __init mtk_wdt_get_base_addr(void)
{
	struct device_node *np_rgu = NULL;
	int i;

	for (i = 0; rgu_of_match[i].compatible; i++) {
		np_rgu = of_find_compatible_node(NULL, NULL, rgu_of_match[i].compatible);
		if (np_rgu)
			break;
	}

	if (!toprgu_base) {
		toprgu_base = of_iomap(np_rgu, 0);
		if (!toprgu_base)
			pr_err("RGU iomap failed\n");

		pr_debug("RGU base: 0x%p\n", toprgu_base);
	}

	return 0;
}

core_initcall(mtk_wdt_get_base_addr);
module_platform_driver(mtk_wdt_driver);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK Watchdog Device Driver");
MODULE_LICENSE("GPL");
