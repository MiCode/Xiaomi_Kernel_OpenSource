/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/irqdomain.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/reboot.h>
#include <linux/wakeup_reason.h>

#include <mt-plat/mt_pmic_wrap.h>
#include <mt-plat/upmu_common.h>
#include "pmic_mt6397.h"
#include <../base/power/mt8173/mt_spm_sleep.h>

#include <mt-plat/mt_reboot.h>
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_boot_common.h>
#endif

#if defined(CONFIG_MTK_RTC)
#include <mtk_rtc.h>
#endif

static void deferred_restart(struct work_struct *dummy);

static DEFINE_MUTEX(pmic_lock_mutex);
static DEFINE_MUTEX(pmic_access_mutex);
static DEFINE_SPINLOCK(pmic_smp_spinlock);
static DECLARE_WORK(restart_work, deferred_restart);

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static bool long_pwrkey_press;
static unsigned long timer_pre;
static unsigned long timer_pos;
#define LONG_PWRKEY_PRESS_TIME		(500*1000000)	/* 500ms */
#endif

static struct hrtimer check_pwrkey_release_timer;
#define LONG_PRESS_PWRKEY_SHUTDOWN_TIME		(3)	/* 3sec */
#define PWRKEY_INITIAL_STATE (0)

static struct mt6397_chip_priv *mt6397_chip;

static struct mt_wake_event mt6397_event = {
	.domain = "PMIC"
};

static struct mt_wake_event_map pwrkey_wev = {
	.domain = "PMIC",
	.code = MT6397_IRQ_PWRKEY,
	.we = WEV_PWR,
};

static struct mt_wake_event_map rtc_wev = {
	.domain = "PMIC",
	.code = MT6397_IRQ_RTC,
	.we = WEV_RTC,
};

static struct mt_wake_event_map charger_wev = {
	.domain = "PMIC",
	.code = MT6397_IRQ_CHRDET,
	.we = WEV_CHARGER,
};

/* ============================================================================== */
/* PMIC lock/unlock APIs */
/* ============================================================================== */
void pmic_lock(void)
{
	mutex_lock(&pmic_lock_mutex);
}

void pmic_unlock(void)
{
	mutex_unlock(&pmic_lock_mutex);
}

void pmic_smp_lock(void)
{
	spin_lock(&pmic_smp_spinlock);
}

void pmic_smp_unlock(void)
{
	spin_unlock(&pmic_smp_spinlock);
}

unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	mutex_lock(&pmic_access_mutex);

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);

	mutex_unlock(&pmic_access_mutex);

	return return_value;
}

unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	mutex_lock(&pmic_access_mutex);

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	return_value = pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}

	mutex_unlock(&pmic_access_mutex);

	return return_value;
}

/* ============================================================================== */
/* PMIC read/write APIs : nolock */
/* ============================================================================== */
unsigned int pmic_read_interface_nolock(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);

	return return_value;
}

unsigned int pmic_config_interface_nolock(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	return_value = pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	return return_value;
}

unsigned int upmu_get_reg_value(unsigned int reg)
{
	unsigned int ret = 0;
	unsigned int reg_val = 0;

	ret = pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}

void upmu_set_reg_value(unsigned int reg, unsigned int reg_val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

/* ============================================================================== */
/* pmic dev_attr APIs */
/* ============================================================================== */
unsigned int g_reg_value = 0;
static ssize_t show_pmic_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_warn("[Power/PMIC][show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%04X\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = "\n";
	char temp_buf[32];
	char *addr = NULL;
	unsigned long reg_value = 0;
	unsigned long reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	pr_warn("[Power/PMIC][store_pmic_access]\n");
	if (size != 0) {
		pr_warn("[Power/PMIC]" "[store_pmic_access] buf is %s and size is %lu\n", buf, size);
		addr = strsep(&pvalue, " ");

		if (kstrtoul(addr, 16, &reg_address))
			return -EINVAL;

#ifdef CONFIG_PM_DEBUG
		if ((size >= 10) && (strncmp(buf, "hard_reset", 10) == 0)) {
			pr_warn("[Power/PMIC]" "[store_pmic_access] Simulate long press Power Key\n");
			arch_reset(0, NULL);
		} else
#endif
		if (size > 7) {
			if (kstrtoul(pvalue, 16, &reg_value))
				return -EINVAL;
			pr_warn("[Power/PMIC]" "[store_pmic_access] write PMU reg 0x%lx with value 0x%lx !\n",
				reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_warn("[Power/PMIC]" "[store_pmic_access] read PMU reg 0x%lx with value 0x%x !\n",
				reg_address, g_reg_value);
			pr_warn("[Power/PMIC]" "[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);	/* 664 */

static void deferred_restart(struct work_struct *dummy)
{
	unsigned int pwrkey_deb = 0;

	/* Double check if pwrkey is still pressed */
	pwrkey_deb = upmu_get_pwrkey_deb();
	if (pwrkey_deb == 1) {
		pr_warn("[check_pwrkey_release_timer] Release pwrkey\n");
		kpd_pwrkey_pmic_handler(0x0);
	} else
		pr_warn("[check_pwrkey_release_timer] Still press pwrkey, do nothing\n");

}

/* mt6397 irq chip clear event status for given event mask. */
static void mt6397_ack_events_locked(struct mt6397_chip_priv *chip, unsigned int event_mask)
{
	unsigned int val;

	pwrap_write(chip->int_stat[0], event_mask & 0xFFFF);
	pwrap_write(chip->int_stat[1], (event_mask >> 16) & 0xFFFF);
	pwrap_read(chip->int_stat[0], &val);
	pwrap_read(chip->int_stat[1], &val);
}

/* mt6397 irq chip event read. */
static unsigned int mt6397_get_events_locked(struct mt6397_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	unsigned int events[2] = { 0 };

	pwrap_read(chip->int_stat[0], &events[0]);
	pwrap_read(chip->int_stat[1], &events[1]);

	return (events[1] << 16) | (events[0] & 0xFFFF);
}

static unsigned int mt6397_get_events(struct mt6397_chip_priv *chip)
{
	unsigned int event;

	pmic_lock();
	event = mt6397_get_events_locked(chip);
	pmic_unlock();

	return event;
}


/* mt6397 irq chip event mask read: debugging only */
static unsigned int mt6397_get_event_mask_locked(struct mt6397_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	unsigned int event_mask[2] = { 0 };

	pwrap_read(chip->int_con[0], &event_mask[0]);
	pwrap_read(chip->int_con[1], &event_mask[1]);

	return (event_mask[1] << 16) | (event_mask[0] & 0xFFFF);
}

static unsigned int mt6397_get_event_mask(struct mt6397_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	unsigned int res;

	pmic_lock();
	res = mt6397_get_event_mask_locked(chip);
	pmic_unlock();

	return res;
}

/* mt6397 irq chip event mask write: initial setup */
static void mt6397_set_event_mask_locked(struct mt6397_chip_priv *chip, unsigned int event_mask)
{
	unsigned int val;

	pwrap_write(chip->int_con[0], event_mask & 0xFFFF);
	pwrap_write(chip->int_con[1], (event_mask >> 16) & 0xFFFF);
	pwrap_read(chip->int_con[0], &val);
	pwrap_read(chip->int_con[1], &val);
	chip->event_mask = event_mask;
}

static void mt6397_set_event_mask(struct mt6397_chip_priv *chip, unsigned int event_mask)
{
	pmic_lock();
	mt6397_set_event_mask_locked(chip, event_mask);
	pmic_unlock();
}

/* this function is only called by generic IRQ framework, and it is always
 * called with pmic_lock held by IRQ framework. */
static void mt6397_irq_mask_unmask_locked(struct irq_data *d, bool enable)
{
	struct mt6397_chip_priv *mt_chip = d->chip_data;
	int hw_irq = d->hwirq;
	u16 port = (hw_irq >> 4) & 1;
	unsigned int val;

	if (enable)
		set_bit(hw_irq, (unsigned long *)&mt_chip->event_mask);
	else
		clear_bit(hw_irq, (unsigned long *)&mt_chip->event_mask);

	if (port) {
		pwrap_write(mt_chip->int_con[1], (mt_chip->event_mask >> 16) & 0xFFFF);
		pwrap_read(mt_chip->int_con[1], &val);
	} else {
		pwrap_write(mt_chip->int_con[0], mt_chip->event_mask & 0xFFFF);
		pwrap_read(mt_chip->int_con[0], &val);
#if defined(CONFIG_MTK_BATTERY_PROTECT)
		/* lbat irq src need toggle to enable again. */
		toggle_lbat_irq_src(enable, hw_irq);
#endif
	}
}

static void mt6397_irq_mask_locked(struct irq_data *d)
{
	mt6397_irq_mask_unmask_locked(d, false);
	mdelay(5);
}

static void mt6397_irq_unmask_locked(struct irq_data *d)
{
	mt6397_irq_mask_unmask_locked(d, true);
}

static void mt6397_irq_ack_locked(struct irq_data *d)
{
	struct mt6397_chip_priv *chip = irq_data_get_irq_chip_data(d);

	mt6397_ack_events_locked(chip, 1 << d->hwirq);
}

#ifdef CONFIG_PMIC_IMPLEMENT_UNUSED_EVENT_HANDLERS
static irqreturn_t pwrkey_rstb_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_int_en_pwrkey_rstb(0);

	return IRQ_HANDLED;
}

static irqreturn_t hdmi_sifm_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_int_en_hdmi_sifm(0);

#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
	vMhlTriggerIntTask();
#endif

	return IRQ_HANDLED;
}

static irqreturn_t hdmi_cec_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_int_en_hdmi_cec(0);

	return IRQ_HANDLED;
}

static irqreturn_t vsrmca15_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vsrmca15(0);

	return IRQ_HANDLED;
}

static irqreturn_t vcore_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vcore(0);

	return IRQ_HANDLED;
}

static irqreturn_t vio18_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vio18(0);

	return IRQ_HANDLED;
}

static irqreturn_t vpca7_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vpca7(0);

	return IRQ_HANDLED;
}

static irqreturn_t vsrmca7_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vsrmca7(0);

	return IRQ_HANDLED;
}

static irqreturn_t vdrm_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vdrm(0);

	return IRQ_HANDLED;
}

static irqreturn_t vca15_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vca15(0);

	return IRQ_HANDLED;
}

static irqreturn_t vgpu_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vgpu(0);

	return IRQ_HANDLED;
}

#endif

enum hrtimer_restart check_pwrkey_release_timer_func(struct hrtimer *timer)
{
	queue_work(system_highpri_wq, &restart_work);
	return HRTIMER_NORESTART;
}

static irqreturn_t pwrkey_int_handler(int irq, void *dev_id)
{
	unsigned int pwrkey_deb = 0;
	static int key_down = PWRKEY_INITIAL_STATE;
	ktime_t ktime;

	pr_warn("%s:\n", __func__);

	pwrkey_deb = upmu_get_pwrkey_deb();

	if (pwrkey_deb == 1) {
		pr_warn("[Power/PMIC]" "[pwrkey_int_handler] Release pwrkey\n");
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
			timer_pos = sched_clock();
			if (timer_pos - timer_pre >= LONG_PWRKEY_PRESS_TIME)
				long_pwrkey_press = true;

			pr_warn("[Power/PMIC]" "pos = %ld, pre = %ld, pos-pre = %ld, long_pwrkey_press = %d\r\n",
				timer_pos, timer_pre, timer_pos - timer_pre, long_pwrkey_press);
			if (long_pwrkey_press) {	/* 500ms */
				pr_warn("[Power/PMIC]" "Pwrkey Pressed during kpoc, reboot OS\r\n");
				if (system_state == SYSTEM_BOOTING)
					arch_reset(0, NULL);
				else
					orderly_reboot(true);
			}
		}
#endif
		hrtimer_cancel(&check_pwrkey_release_timer);

		if (key_down == 0) {
			kpd_pwrkey_pmic_handler(0x1);
			kpd_pwrkey_pmic_handler(0x0);
		} else {
			kpd_pwrkey_pmic_handler(0x0);
		}

		key_down = 0;
	} else {
		key_down = 1;
		pr_warn("[Power/PMIC][pwrkey_int_handler] Press pwrkey\n");
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT)
			timer_pre = sched_clock();

#endif
		ktime = ktime_set(LONG_PRESS_PWRKEY_SHUTDOWN_TIME, 0);
		hrtimer_start(&check_pwrkey_release_timer, ktime, HRTIMER_MODE_REL);

		kpd_pwrkey_pmic_handler(0x1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t homekey_int_handler(int irq, void *dev_id)
{
	pr_warn("%s:\n", __func__);

	if (upmu_get_homekey_deb() == 1) {
		pr_warn("[Power/PMIC]" "[homekey_int_handler] Release HomeKey\r\n");

		kpd_pmic_rstkey_handler(0x0);

	} else {
		pr_warn("[Power/PMIC]" "[homekey_int_handler] Press HomeKey\r\n");

		kpd_pmic_rstkey_handler(0x1);

	}

	return IRQ_HANDLED;
}

static irqreturn_t rtc_int_handler(int irq, void *dev_id)
{
	rtc_irq_handler();
	mdelay(100);

	return IRQ_HANDLED;
}

#ifdef CONFIG_MTK_ACCDET
static irqreturn_t accdet_int_handler(int irq, void *dev_id)
{
	int ret = 0;

	pr_warn("%s:\n", __func__);

	ret = accdet_irq_handler();
	if (0 == ret)
		pr_debug("[Power/PMIC]" "[accdet_int_handler] don't finished\n");
	return IRQ_HANDLED;
}
#endif

static struct mt6397_irq_data mt6397_irqs[] = {
	{
	 .name = "mt6397_pwrkey",
	 .irq_id = MT6397_IRQ_PWRKEY,
	 .action_fn = pwrkey_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
	{
	 .name = "mt6397_homekey",
	 .irq_id = MT6397_IRQ_HOMEKEY,
	 .action_fn = homekey_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6397_rtc",
	 .irq_id = MT6397_IRQ_RTC,
	 .action_fn = rtc_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
#ifdef CONFIG_MTK_ACCDET
	{
	 .name = "mt6397_accdet",
	 .irq_id = MT6397_IRQ_ACCDET,
	 .action_fn = accdet_int_handler,
	 .enabled = true,
	 },
#endif
#if defined(CONFIG_MTK_BATTERY_PROTECT)
	{
	 .name = "mt6397_bat_l",
	 .irq_id = MT6397_IRQ_BAT_L,
	 .action_fn = bat_l_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6397_bat_h",
	 .irq_id = MT6397_IRQ_BAT_H,
	 .action_fn = bat_h_int_handler,
	 .enabled = true,
	 },
#endif
#ifdef CONFIG_PMIC_IMPLEMENT_UNUSED_EVENT_HANDLERS
	{
	 .name = "mt6397_vca15",
	 .irq_id = MT6397_IRQ_VCA15,
	 .action_fn = vca15_int_handler,
	 },
	{
	 .name = "mt6397_vgpu",
	 .irq_id = MT6397_IRQ_VGPU,
	 .action_fn = vgpu_int_handler,
	 },
	{
	 .name = "mt6397_pwrkey_rstb",
	 .irq_id = MT6397_IRQ_PWRKEY_RSTB,
	 .action_fn = pwrkey_rstb_int_handler,
	 },
	{
	 .name = "mt6397_hdmi_sifm",
	 .irq_id = MT6397_IRQ_HDMI_SIFM,
	 .action_fn = hdmi_sifm_int_handler,
	 },
	{
	 .name = "mt6397_hdmi_cec",
	 .irq_id = MT6397_IRQ_HDMI_CEC,
	 .action_fn = hdmi_cec_int_handler,
	 },
	{
	 .name = "mt6397_srmvca15",
	 .irq_id = MT6397_IRQ_VSRMCA15,
	 .action_fn = vsrmca15_int_handler,
	 },
	{
	 .name = "mt6397_vcore",
	 .irq_id = MT6397_IRQ_VCORE,
	 .action_fn = vcore_int_handler,
	 },
	{
	 .name = "mt6397_vio18",
	 .irq_id = MT6397_IRQ_VIO18,
	 .action_fn = vio18_int_handler,
	 },
	{
	 .name = "mt6397_vpca7",
	 .irq_id = MT6397_IRQ_VPCA7,
	 .action_fn = vpca7_int_handler,
	 },
	{
	 .name = "mt6397_vsram7",
	 .irq_id = MT6397_IRQ_VSRMCA7,
	 .action_fn = vsrmca7_int_handler,
	 },
	{
	 .name = "mt6397_vdrm",
	 .irq_id = MT6397_IRQ_VDRM,
	 .action_fn = vdrm_int_handler,
	 },
#endif
};

static inline void mt6397_do_handle_events(struct mt6397_chip_priv *chip, unsigned int events)
{
	int event_hw_irq;

	for (event_hw_irq = __ffs(events); events;
	     events &= ~(1 << event_hw_irq), event_hw_irq = __ffs(events)) {
		int event_irq = irq_find_mapping(chip->domain, 0) + event_hw_irq;

		pr_debug("%s: event=%d\n", __func__, event_hw_irq);

		{
			unsigned long flags;
			/* simulate HW irq */
			local_irq_save(flags);
			generic_handle_irq(event_irq);
			local_irq_restore(flags);
		}
	}
}

static inline void mt6397_set_suspended(struct mt6397_chip_priv *chip, bool suspended)
{
	chip->suspended = suspended;
	smp_wmb();		/* matching barrier is in mt6397_is_suspended */
}

static inline bool mt6397_is_suspended(struct mt6397_chip_priv *chip)
{
	smp_rmb();		/* matching barrier is in mt6397_set_suspended */
	return chip->suspended;
}

static inline bool mt6397_do_irq(struct mt6397_chip_priv *chip)
{
	unsigned int events = mt6397_get_events(chip);

	if (!events)
		return false;

	/* if event happens when it is masked, it is a HW bug,
	 * unless it is a wakeup interrupt */
	if (events & ~(chip->event_mask | chip->wake_mask)) {
		pr_err("%s: PMIC is raising events %08X which are not enabled\n"
		       "\t(mask 0x%lx, wakeup 0x%lx). HW BUG. Stop\n",
		       __func__, events, chip->event_mask, chip->wake_mask);
		pr_err("int ctrl: %08x, status: %08x\n",
		       mt6397_get_event_mask_locked(chip), mt6397_get_events(chip));
		pr_err("int ctrl: %08x, status: %08x\n",
		       mt6397_get_event_mask_locked(chip), mt6397_get_events(chip));
		BUG();
	}

	mt6397_do_handle_events(chip, events);

	return true;
}

static irqreturn_t mt6397_irq(int irq, void *d)
{
	struct mt6397_chip_priv *chip = (struct mt6397_chip_priv *)d;

	while (!mt6397_is_suspended(chip) && mt6397_do_irq(chip))
		continue;

	return IRQ_HANDLED;
}

static void mt6397_irq_bus_lock(struct irq_data *d)
{
	pmic_lock();
}

static void mt6397_irq_bus_sync_unlock(struct irq_data *d)
{
	pmic_unlock();
}

static void mt6397_irq_chip_suspend(struct mt6397_chip_priv *chip)
{
	chip->saved_mask = mt6397_get_event_mask_locked(chip);
	pr_debug("%s: read event mask=%08X\n", __func__, chip->saved_mask);
	mt6397_set_event_mask_locked(chip, chip->wake_mask);
	pr_debug("%s: write event mask= 0x%lx\n", __func__, chip->wake_mask);
}

static void mt6397_irq_chip_resume(struct mt6397_chip_priv *chip)
{
	struct mt_wake_event *we = spm_get_wakeup_event();
	u32 events = mt6397_get_events_locked(chip);
	int event = 0;
	int i, irq;
	struct irq_desc *desc;

	if (events)
		event = __ffs(events);

	for (i = 0; i < ARRAY_SIZE(mt6397_irqs); i++) {
		struct mt6397_irq_data *data = &mt6397_irqs[i];
		if (event == data->irq_id) {
			irq = irq_find_mapping(chip->domain, event);
			desc = irq_to_desc(irq);
			if (desc && desc->action && desc->action->name) {
				printk("PM: wakeup by pmic irq: %d, name: %s\n", irq, desc->action->name);
			}
			log_wakeup_reason(irq);
		}
	}

	mt6397_set_event_mask_locked(chip, chip->saved_mask);

	if (events && we && we->domain && !strcmp(we->domain, "EINT") && we->code == chip->irq_hw_id)
		spm_report_wakeup_event(&mt6397_event, event);

	if (events)
		chip->wakeup_event = events;
}

static int mt6397_irq_set_wake_locked(struct irq_data *d, unsigned int on)
{
	struct mt6397_chip_priv *chip = irq_data_get_irq_chip_data(d);

	if (on)
		set_bit(d->hwirq, (unsigned long *)&chip->wake_mask);
	else
		clear_bit(d->hwirq, (unsigned long *)&chip->wake_mask);
	return 0;
}

static struct irq_chip mt6397_irq_chip = {
	.name = "mt6397-irqchip",
	.irq_ack = mt6397_irq_ack_locked,
	.irq_mask = mt6397_irq_mask_locked,
	.irq_unmask = mt6397_irq_unmask_locked,
	.irq_set_wake = mt6397_irq_set_wake_locked,
	.irq_bus_lock = mt6397_irq_bus_lock,
	.irq_bus_sync_unlock = mt6397_irq_bus_sync_unlock,
};

static int mt6397_irq_init(struct mt6397_chip_priv *chip)
{
	int i;
	int ret;

	chip->domain = irq_domain_add_linear(chip->dev->of_node,
		MT6397_IRQ_NR, &irq_domain_simple_ops, chip);
	if (!chip->domain) {
		dev_err(chip->dev, "could not create irq domain\n");
		return -ENOMEM;
	}

	for (i = 0; i < chip->num_int; i++) {
		int virq = irq_create_mapping(chip->domain, i);

		irq_set_chip_and_handler(virq, &mt6397_irq_chip,
			handle_level_irq);
		irq_set_chip_data(virq, chip);
		set_irq_flags(virq, IRQF_VALID);

	};

	mt6397_set_event_mask(chip, 0);
	pr_debug("%s: PMIC: event_mask=%08X; events=%08X\n",
		 __func__, mt6397_get_event_mask(chip), mt6397_get_events(chip));

	ret = request_threaded_irq(chip->irq, NULL, mt6397_irq,
				    IRQF_ONESHOT, mt6397_irq_chip.name, chip);
	if (ret < 0) {
		pr_debug("%s: PMIC master irq request err: %d\n", __func__, ret);
		goto err_free_domain;
	}

	irq_set_irq_wake(chip->irq, true);
	return 0;
 err_free_domain:
	irq_domain_remove(chip->domain);
	return ret;
}

static int mt6397_irq_handler_init(struct mt6397_chip_priv *chip)
{
	int i;
	/*AP:
	 * I register all the non-default vectors,
	 * and disable all vectors that were not enabled by original code;
	 * threads are created for all non-default vectors.
	 */
	for (i = 0; i < ARRAY_SIZE(mt6397_irqs); i++) {
		int ret, irq;
		struct mt6397_irq_data *data = &mt6397_irqs[i];

		irq = irq_find_mapping(chip->domain, data->irq_id);
		ret = request_threaded_irq(irq, NULL, data->action_fn,
					   IRQF_TRIGGER_HIGH | IRQF_ONESHOT, data->name, chip);
		if (ret) {
			pr_debug("%s: failed to register irq=%d (%d); name='%s'; err: %d\n",
				__func__, irq, data->irq_id, data->name, ret);
			continue;
		}
		if (!data->enabled)
			disable_irq(irq);
		if (data->wake_src)
			irq_set_irq_wake(irq, 1);
		pr_warn("%s: registered irq=%d (%d); name='%s'; enabled: %d\n",
			__func__, irq, data->irq_id, data->name, data->enabled);
	}
	return 0;
}

static int mt6397_syscore_suspend(void)
{
	mt6397_irq_chip_suspend(mt6397_chip);
	return 0;
}

static void mt6397_syscore_resume(void)
{
	mt6397_irq_chip_resume(mt6397_chip);
}

static struct syscore_ops mt6397_syscore_ops = {
	.suspend = mt6397_syscore_suspend,
	.resume = mt6397_syscore_resume,
};

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int chip_version = 0;
	unsigned int ret = 0;

	/* [0:0]: RG_VCDT_HV_EN; Disable HV. Only compare LV threshold. */
	ret = pmic_config_interface(0x0, 0x0, 0x1, 0);

	/* put init setting from DE/SA */
	chip_version = upmu_get_cid();

	switch (chip_version & 0xFF) {
	case 0x91:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xC, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x0, 0x3, 10);
		break;
	case 0x97:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xB, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x1, 0x3, 10);
		break;
	default:
		pr_err("[Power/PMIC] Error chip ID %d\r\n", chip_version);
		break;
	}

	ret = pmic_config_interface(0xC, 0x1, 0x7, 1);	/* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.3V */
	ret = pmic_config_interface(0x24, 0x1, 0x1, 1);	/* [1:1]: RG_BC11_RST; */
	ret = pmic_config_interface(0x2A, 0x0, 0x7, 4);	/* [6:4]: RG_CSDAC_STP; align to 6250's setting */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 7);	/* [7:7]: RG_ULC_DET_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 6);	/* [6:6]: RG_HWCV_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 2);	/* [2:2]: RG_CSDAC_MODE; */
	ret = pmic_config_interface(0x102, 0x0, 0x1, 3);	/* [3:3]: RG_PWMOC_CK_PDN; For OC protection */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 9);	/* [9:9]: RG_SRCVOLT_HW_AUTO_EN; */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 8);	/* [8:8]: RG_OSC_SEL_AUTO; */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 6);	/* [6:6]: RG_SMPS_DIV2_SRC_AUTOFF_DIS; */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 5);	/* [5:5]: RG_SMPS_AUTOFF_DIS; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 7);	/* [7:7]: VDRM_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 6);	/* [6:6]: VSRMCA7_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 5);	/* [5:5]: VPCA7_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 4);	/* [4:4]: VIO18_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 3);	/* [3:3]: VGPU_DEG_EN; For OC protection */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 2);	/* [2:2]: VCORE_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 1);	/* [1:1]: VSRMCA15_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 0);	/* [0:0]: VCA15_DEG_EN; */
	ret = pmic_config_interface(0x206, 0x600, 0x0FFF, 0);	/* [12:0]: BUCK_RSV; for OC protection */
	/* [7:6]: QI_VSRMCA7_VSLEEP; sleep mode only (0.85V) */
	ret = pmic_config_interface(0x210, 0x0, 0x3, 6);
	/* [5:4]: QI_VSRMCA15_VSLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x210, 0x1, 0x3, 4);
	/* [3:2]: QI_VPCA7_VSLEEP; sleep mode only (0.85V) */
	ret = pmic_config_interface(0x210, 0x0, 0x3, 2);
	/* [1:0]: QI_VCA15_VSLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x210, 0x1, 0x3, 0);
	/* [13:12]: RG_VCA15_CSL2; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 12);
	/* [11:10]: RG_VCA15_CSL1; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 10);
	/* [15:15]: VCA15_SFCHG_REN; soft change rising enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 15);
	/* [14:8]: VCA15_SFCHG_RRATE; soft change rising step=0.5us */
	ret = pmic_config_interface(0x224, 0x5, 0x7F, 8);
	/* [7:7]: VCA15_SFCHG_FEN; soft change falling enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 7);
	/* [6:0]: VCA15_SFCHG_FRATE; soft change falling step=2us */
	ret = pmic_config_interface(0x224, 0x17, 0x7F, 0);
	/* [6:0]: VCA15_VOSEL_SLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x22A, 0x0, 0x7F, 0);
	/* [8:8]: VCA15_VSLEEP_EN; set sleep mode reference voltage from R2R to V2V */
	ret = pmic_config_interface(0x238, 0x1, 0x1, 8);
	/* [5:4]: VCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x238, 0x3, 0x3, 4);
	ret = pmic_config_interface(0x244, 0x1, 0x1, 5);	/* [5:5]: VSRMCA15_TRACK_SLEEP_CTRL; */
	ret = pmic_config_interface(0x246, 0x0, 0x3, 4);	/* [5:4]: VSRMCA15_VOSEL_SEL; */
	ret = pmic_config_interface(0x24A, 0x1, 0x1, 15);	/* [15:15]: VSRMCA15_SFCHG_REN; */
	ret = pmic_config_interface(0x24A, 0x5, 0x7F, 8);	/* [14:8]: VSRMCA15_SFCHG_RRATE; */
	ret = pmic_config_interface(0x24A, 0x1, 0x1, 7);	/* [7:7]: VSRMCA15_SFCHG_FEN; */
	ret = pmic_config_interface(0x24A, 0x17, 0x7F, 0);	/* [6:0]: VSRMCA15_SFCHG_FRATE; */
	/* [6:0]: VSRMCA15_VOSEL_SLEEP; Sleep mode setting only (0.7V) */
	ret = pmic_config_interface(0x250, 0x00, 0x7F, 0);
	/* [8:8]: VSRMCA15_VSLEEP_EN; set sleep mode reference voltage from R2R to V2V */
	ret = pmic_config_interface(0x25E, 0x1, 0x1, 8);
	/* [5:4]: VSRMCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x25E, 0x3, 0x3, 4);
	/* [1:1]: VCORE_VOSEL_CTRL; sleep mode voltage control follow SRCLKEN */
	ret = pmic_config_interface(0x270, 0x1, 0x1, 1);
	ret = pmic_config_interface(0x272, 0x0, 0x3, 4);	/* [5:4]: VCORE_VOSEL_SEL; */
	ret = pmic_config_interface(0x276, 0x1, 0x1, 15);	/* [15:15]: VCORE_SFCHG_REN; */
	ret = pmic_config_interface(0x276, 0x5, 0x7F, 8);	/* [14:8]: VCORE_SFCHG_RRATE; */
	ret = pmic_config_interface(0x276, 0x17, 0x7F, 0);	/* [6:0]: VCORE_SFCHG_FRATE; */
	/* [6:0]: VCORE_VOSEL_SLEEP; Sleep mode setting only (0.7V) */
	ret = pmic_config_interface(0x27C, 0x0, 0x7F, 0);
	/* [8:8]: VCORE_VSLEEP_EN; Sleep mode HW control  R2R to VtoV */
	ret = pmic_config_interface(0x28A, 0x1, 0x1, 8);
	/* [5:4]: VCORE_VOSEL_TRANS_EN; Follows MT6320 VCORE setting. */
	ret = pmic_config_interface(0x28A, 0x0, 0x3, 4);
	ret = pmic_config_interface(0x28A, 0x3, 0x3, 0);	/* [1:0]: VCORE_TRANSTD; */
	ret = pmic_config_interface(0x28E, 0x1, 0x3, 8);	/* [9:8]: RG_VGPU_CSL; for OC protection */
	ret = pmic_config_interface(0x29C, 0x1, 0x1, 15);	/* [15:15]: VGPU_SFCHG_REN; */
	ret = pmic_config_interface(0x29C, 0x5, 0x7F, 8);	/* [14:8]: VGPU_SFCHG_RRATE; */
	ret = pmic_config_interface(0x29C, 0x17, 0x7F, 0);	/* [6:0]: VGPU_SFCHG_FRATE; */
	ret = pmic_config_interface(0x2B0, 0x0, 0x3, 4);	/* [5:4]: VGPU_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x2B0, 0x3, 0x3, 0);	/* [1:0]: VGPU_TRANSTD; */
	ret = pmic_config_interface(0x332, 0x0, 0x3, 4);	/* [5:4]: VPCA7_VOSEL_SEL; */
	ret = pmic_config_interface(0x336, 0x1, 0x1, 15);	/* [15:15]: VPCA7_SFCHG_REN; */
	ret = pmic_config_interface(0x336, 0x5, 0x7F, 8);	/* [14:8]: VPCA7_SFCHG_RRATE; */
	ret = pmic_config_interface(0x336, 0x1, 0x1, 7);	/* [7:7]: VPCA7_SFCHG_FEN; */
	ret = pmic_config_interface(0x336, 0x17, 0x7F, 0);	/* [6:0]: VPCA7_SFCHG_FRATE; */
	ret = pmic_config_interface(0x33C, 0x18, 0x7F, 0);	/* [6:0]: VPCA7_VOSEL_SLEEP; */
	ret = pmic_config_interface(0x34A, 0x1, 0x1, 8);	/* [8:8]: VPCA7_VSLEEP_EN; */
	ret = pmic_config_interface(0x34A, 0x3, 0x3, 4);	/* [5:4]: VPCA7_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x356, 0x0, 0x1, 5);	/* [5:5]: VSRMCA7_TRACK_SLEEP_CTRL; */
	ret = pmic_config_interface(0x358, 0x0, 0x3, 4);	/* [5:4]: VSRMCA7_VOSEL_SEL; */
	ret = pmic_config_interface(0x35C, 0x1, 0x1, 15);	/* [15:15]: VSRMCA7_SFCHG_REN; */
	ret = pmic_config_interface(0x35C, 0x5, 0x7F, 8);	/* [14:8]: VSRMCA7_SFCHG_RRATE; */
	ret = pmic_config_interface(0x35C, 0x1, 0x1, 7);	/* [7:7]: VSRMCA7_SFCHG_FEN; */
	ret = pmic_config_interface(0x35C, 0x17, 0x7F, 0);	/* [6:0]: VSRMCA7_SFCHG_FRATE; */
	ret = pmic_config_interface(0x362, 0x18, 0x7F, 0);	/* [6:0]: VSRMCA7_VOSEL_SLEEP; */
	ret = pmic_config_interface(0x370, 0x1, 0x1, 8);	/* [8:8]: VSRMCA7_VSLEEP_EN; */
	ret = pmic_config_interface(0x370, 0x3, 0x3, 4);	/* [5:4]: VSRMCA7_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x39C, 0x1, 0x1, 8);	/* [8:8]: VDRM_VSLEEP_EN; */
	ret = pmic_config_interface(0x440, 0x1, 0x1, 2);	/* [2:2]: VIBR_THER_SHEN_EN; */
	ret = pmic_config_interface(0x500, 0x1, 0x1, 5);	/* [5:5]: THR_HWPDN_EN; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 3);	/* [3:3]: RG_RST_DRVSEL; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 2);	/* [2:2]: RG_EN_DRVSEL; */
	ret = pmic_config_interface(0x508, 0x1, 0x1, 1);	/* [1:1]: PWRBB_DEB_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 12);	/* [12:12]: VSRMCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 11);	/* [11:11]: VPCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 10);	/* [10:10]: VCORE_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 9);	/* [9:9]: VSRMCA7_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 8);	/* [8:8]: VPCA7_PG_H2L_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1, 1);	/* [1:1]: STRUP_PWROFF_PREOFF_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1, 0);	/* [0:0]: STRUP_PWROFF_SEQ_EN; */
	ret = pmic_config_interface(0x55E, 0xFC, 0xFF, 8);	/* [15:8]: RG_ADC_TRIM_CH_SEL; */
	ret = pmic_config_interface(0x560, 0x1, 0x1, 1);	/* [1:1]: FLASH_THER_SHDN_EN; */
	ret = pmic_config_interface(0x566, 0x1, 0x1, 1);	/* [1:1]: KPLED_THER_SHDN_EN; */
	ret = pmic_config_interface(0x600, 0x1, 0x1, 9);	/* [9:9]: SPK_THER_SHDN_L_EN; */
	ret = pmic_config_interface(0x604, 0x1, 0x1, 0);	/* [0:0]: RG_SPK_INTG_RST_L; */
	ret = pmic_config_interface(0x606, 0x1, 0x1, 9);	/* [9:9]: SPK_THER_SHDN_R_EN; */
	ret = pmic_config_interface(0x60A, 0x1, 0xF, 11);	/* [14:11]: RG_SPKPGA_GAINR; */
	ret = pmic_config_interface(0x612, 0x1, 0xF, 8);	/* [11:8]: RG_SPKPGA_GAINL; */
	ret = pmic_config_interface(0x632, 0x1, 0x1, 8);	/* [8:8]: FG_SLP_EN; */
	ret = pmic_config_interface(0x638, 0xFFC2, 0xFFFF, 0);	/* [15:0]: FG_SLP_CUR_TH; */
	ret = pmic_config_interface(0x63A, 0x14, 0xFF, 0);	/* [7:0]: FG_SLP_TIME; */
	ret = pmic_config_interface(0x63C, 0xFF, 0xFF, 8);	/* [15:8]: FG_DET_TIME; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 7);	/* [7:7]: RG_LCLDO_ENC_REMOTE_SENSE_VA28; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 4);	/* [4:4]: RG_LCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 1);	/* [1:1]: RG_HCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x71A, 0x1, 0x1, 15);	/* [15:15]: RG_NCP_REMOTE_SENSE_VA18; */
	ret = pmic_config_interface(0x260, 0x10, 0x7F, 8);	/* [14:8]: VSRMCA15_VOSEL_OFFSET; set offset=100mV */
	ret = pmic_config_interface(0x260, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA15_VOSEL_DELTA; set delta=0mV */
	ret = pmic_config_interface(0x262, 0x48, 0x7F, 8);	/* [14:8]: VSRMCA15_VOSEL_ON_HB; set HB=1.15V */
	ret = pmic_config_interface(0x262, 0x25, 0x7F, 0);	/* [6:0]: VSRMCA15_VOSEL_ON_LB; set LB=0.93125V */
	ret = pmic_config_interface(0x264, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA15_VOSEL_SLEEP_LB; set sleep LB=0.7V */
	ret = pmic_config_interface(0x372, 0x4, 0x7F, 8);	/* [14:8]: VSRMCA7_VOSEL_OFFSET; set offset=25mV */
	ret = pmic_config_interface(0x372, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA7_VOSEL_DELTA; set delta=0mV */
	ret = pmic_config_interface(0x374, 0x48, 0x7F, 8);	/* [14:8]: VSRMCA7_VOSEL_ON_HB; set HB=1.15V */
	ret = pmic_config_interface(0x374, 0x25, 0x7F, 0);	/* [6:0]: VSRMCA7_VOSEL_ON_LB; set LB=0.93125V */
	ret = pmic_config_interface(0x376, 0x18, 0x7F, 0);	/* [6:0]: set sleep LB=0.85000V */
	ret = pmic_config_interface(0x21E, 0x3, 0x3, 0);	/* [1:1]: DVS HW control by SRCLKEN */
	ret = pmic_config_interface(0x244, 0x3, 0x3, 0);	/* [1:1]: VSRMCA15_VOSEL_CTRL, VSRMCA15_EN_CTRL; */
	ret = pmic_config_interface(0x330, 0x0, 0x1, 1);	/* [1:1]: VPCA7_VOSEL_CTRL; */
	ret = pmic_config_interface(0x356, 0x0, 0x1, 1);	/* [1:1]: VSRMCA7_VOSEL_CTRL; */
	ret = pmic_config_interface(0x21E, 0x1, 0x1, 4);	/* [4:4]: VCA15_TRACK_ON_CTRL; DVFS tracking enable */
	ret = pmic_config_interface(0x244, 0x1, 0x1, 4);	/* [4:4]: VSRMCA15_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x330, 0x0, 0x1, 4);	/* [4:4]: VPCA7_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x356, 0x0, 0x1, 4);	/* [4:4]: VSRMCA7_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x134, 0x3, 0x3, 14);	/* [15:14]: VGPU OC; */
	ret = pmic_config_interface(0x134, 0x3, 0x3, 2);	/* [3:2]: VCA15 OC; */
	ret = pmic_config_interface(0x750, 0x0, 0x1, 0);     /*long press reboot */
}

void PMIC_CUSTOM_SETTING_V1(void)
{
	/* enable HW control DCXO 26MHz on-off, request by SPM module */
	upmu_set_rg_srcvolt_hw_auto_en(1);
	upmu_set_rg_dcxo_ldo_dbb_reg_en(0);

	/* enable HW control DCXO RF clk on-off, request by low power module task */
	upmu_set_rg_srclkperi_hw_auto_en(1);
	upmu_set_rg_dcxo_ldo_rf1_reg_en(0);

#ifndef CONFIG_MTK_PMIC_RF2_26M_ALWAYS_ON
	/* disable RF2 26MHz clock */
	upmu_set_rg_dcxo_ldo_rf2_reg_en(0x1);
	upmu_set_rg_dcxo_s2a_ldo_rf2_en(0x0);	/* clock off for internal 32K */
	upmu_set_rg_dcxo_por2_ldo_rf2_en(0x0);	/* clock off for external 32K */
#else
	/* enable RF2 26MHz clock */
	upmu_set_rg_dcxo_ldo_rf2_reg_en(0x1);
	upmu_set_rg_dcxo_s2a_ldo_rf2_en(0x1);	/* clock on for internal 32K */
	upmu_set_rg_dcxo_por2_ldo_rf2_en(0x1);	/* clock on for external 32K */
#endif
}

void pmic_low_power_setting(void)
{
	unsigned int ret = 0;

	pr_warn("[Power/PMIC]" "[pmic_low_power_setting]\n");

	upmu_set_vio18_vsleep_en(1);
	/* top */
	ret = pmic_config_interface(0x102, 0x8080, 0x8080, 0);
	ret = pmic_config_interface(0x108, 0x0882, 0x0882, 0);
	ret = pmic_config_interface(0x12a, 0x0000, 0x8c00, 0);	/* reg_ck:24MHz */
	ret = pmic_config_interface(0x206, 0x0060, 0x0060, 0);
	ret = pmic_config_interface(0x402, 0x0001, 0x0001, 0);

	/* chip_version > PMIC6397_E1_CID_CODE*/
	ret = pmic_config_interface(0x128, 0x0000, 0x0060, 0);

	/* VTCXO control */
	/* chip_version > PMIC6397_E1_CID_CODE*/
	/* enter low power mode when suspend */
	ret = pmic_config_interface(0x400, 0x4400, 0x6c01, 0);
	ret = pmic_config_interface(0x446, 0x0100, 0x0100, 0);
	pr_debug("[Power/PMIC][pmic_low_power_setting] Done\n");
}

void pmic_setting_depends_rtc(void)
{
	unsigned int ret = 0;

#if (!defined(CONFIG_POWER_EXT) && defined(CONFIG_MTK_RTC))
	if (crystal_exist_status()) {
#else
	if (0) {
#endif
		/* with 32K */
		ret = pmic_config_interface(ANALDO_CON1, 3, 0x7, 12);	/* [14:12]=3(VTCXO_SRCLK_EN_SEL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 11);	/* [11]=1(VTCXO_ON_CTRL), */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 1);	/* [1]=0(VTCXO_LP_SET), */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 0);	/* [0]=0(VTCXO_LP_SEL), */

		pr_warn("[Power/PMIC]" "[pmic_setting_depends_rtc] With 32K. Reg[0x%x]=0x%x\n", ANALDO_CON1,
			upmu_get_reg_value(ANALDO_CON1));
	} else {
		/* without 32K */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 11);	/* [11]=0(VTCXO_ON_CTRL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 10);	/* [10]=1(RG_VTCXO_EN), */
		ret = pmic_config_interface(ANALDO_CON1, 3, 0x7, 4);	/* [6:4]=3(VTCXO_SRCLK_MODE_SEL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 0);	/* [0]=1(VTCXO_LP_SEL), */

		pr_warn("[Power/PMIC]" "[pmic_setting_depends_rtc] Without 32K. Reg[0x%x]=0x%x\n", ANALDO_CON1,
			upmu_get_reg_value(ANALDO_CON1));
	}
}

void pmic_charger_watchdog_enable(bool enable)
{
	int arg = enable ? 1 : 0;

	upmu_set_rg_chrwdt_en(arg);
	upmu_set_rg_chrwdt_int_en(arg);
	upmu_set_rg_chrwdt_wr(arg);
	upmu_set_rg_chrwdt_flag_wr(1);
}

static int pmic_mt6397_probe(struct platform_device *pdev)
{
	unsigned int ret_val;
	int ret_device_file = 0;
	struct mt6397_chip_priv *chip;
	int ret;
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);

	pr_debug("[Power/PMIC] ******** MT6397 pmic driver probe!! ********\n");

	ret_device_file = device_create_file(&(pdev->dev), &dev_attr_pmic_access);
	/* get PMIC CID */
	ret_val = upmu_get_cid();
	pr_debug("[Power/PMIC] MT6397 PMIC CID=0x%x\n", ret_val);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_INIT_SETTING_V1] Done\n");
	PMIC_CUSTOM_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_CUSTOM_SETTING_V1] Done\n");

	/* pmic low power setting */
	pmic_low_power_setting();

	/* enable PWRKEY/HOMEKEY posedge detected interrupt */
	upmu_set_rg_pwrkey_int_sel(1);
	upmu_set_rg_homekey_int_sel(1);
	upmu_set_rg_homekey_puen(1);

	pmic_charger_watchdog_enable(false);

	chip = kzalloc(sizeof(struct mt6397_chip_priv), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	mt6397->irq = platform_get_irq(pdev, 0);

	if (mt6397->irq <= 0) {
		dev_err(&pdev->dev, "failed to get irq: %d\n", mt6397->irq);
		return -EINVAL;
	}
	chip->irq = mt6397->irq; /* hw irq of EINT */
	chip->irq_hw_id = (int)irqd_to_hwirq(irq_get_irq_data(mt6397->irq)); /* EINT num */

	chip->num_int = 32;
	chip->int_con[0] = INT_CON0;
	chip->int_con[1] = INT_CON1;
	chip->int_stat[0] = INT_STATUS0;
	chip->int_stat[1] = INT_STATUS1;

	dev_set_drvdata(chip->dev, chip);

	device_init_wakeup(chip->dev, true);
	pr_debug("[Power/PMIC][PMIC_EINT_SETTING] Done\n");

	ret = mt6397_irq_init(chip);
	if (ret)
		return ret;

	ret = mt6397_irq_handler_init(chip);
	if (ret)
		return ret;

	pwrkey_wev.irq = irq_find_mapping(chip->domain, MT6397_IRQ_PWRKEY);
	rtc_wev.irq = irq_find_mapping(chip->domain, MT6397_IRQ_RTC);
	charger_wev.irq = irq_find_mapping(chip->domain, MT6397_IRQ_CHRDET);
	spm_register_wakeup_event(&pwrkey_wev);
	spm_register_wakeup_event(&rtc_wev);
	spm_register_wakeup_event(&charger_wev);

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	low_battery_protect_init(&(pdev->dev));
#else
	pr_warn("[Power/PMIC][PMIC] no define LOW_BATTERY_PROTECT\n");
#endif

	mt6397_chip = chip;
	register_syscore_ops(&mt6397_syscore_ops);

	hrtimer_init(&check_pwrkey_release_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	check_pwrkey_release_timer.function = check_pwrkey_release_timer_func;

	return 0;
}

static int pmic_mt6397_remove(struct platform_device *dev)
{
	pr_debug("[Power/PMIC] " "******** MT6397 pmic driver remove!! ********\n");

	return 0;
}

static void pmic_mt6397_shutdown(struct platform_device *dev)
{
	pr_debug("[Power/PMIC] " "******** MT6397 pmic driver shutdown!! ********\n");
}

static int pmic_mt6397_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mt6397_chip_priv *chip = dev_get_drvdata(&pdev->dev);
	u32 ret = 0;
	u32 events;

	hrtimer_cancel(&check_pwrkey_release_timer);

	mt6397_set_suspended(chip, true);
	disable_irq(chip->irq);

	events = mt6397_get_events(chip);
	if (events)
		dev_err(&pdev->dev, "%s: PMIC events: %08X\n", __func__, events);

	/* Set PMIC CA7, CA15 TRANS_EN to disable(0x0) before system into sleep mode. */
	ret =
	    pmic_config_interface(VCA15_CON18, 0x0, PMIC_VCA15_VOSEL_TRANS_EN_MASK,
				  PMIC_VCA15_VOSEL_TRANS_EN_SHIFT);
	ret =
	    pmic_config_interface(VPCA7_CON18, 0x0, PMIC_VPCA7_VOSEL_TRANS_EN_MASK,
				  PMIC_VPCA7_VOSEL_TRANS_EN_SHIFT);
	pr_warn("[Power/PMIC] Suspend: Reg[0x%x]=0x%x\n", VCA15_CON18,
		upmu_get_reg_value(VCA15_CON18));
	pr_warn("[Power/PMIC] Suspend: Reg[0x%x]=0x%x\n", VPCA7_CON18,
		upmu_get_reg_value(VPCA7_CON18));

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	low_battery_protect_disable();
#endif

	return 0;
}

static int pmic_mt6397_resume(struct platform_device *pdev)
{
	struct mt6397_chip_priv *chip = dev_get_drvdata(&pdev->dev);
	u32 ret = 0;
	int i;

	/* pr_warn("[Power/PMIC] ******** MT6397 pmic driver resume!! ********\n" ); */

	/* Set PMIC CA7, CA15 TRANS_EN to falling enable(0x1) after system resume. */
	ret =
	    pmic_config_interface(VCA15_CON18, 0x1, PMIC_VCA15_VOSEL_TRANS_EN_MASK,
				  PMIC_VCA15_VOSEL_TRANS_EN_SHIFT);
	ret =
	    pmic_config_interface(VPCA7_CON18, 0x1, PMIC_VPCA7_VOSEL_TRANS_EN_MASK,
				  PMIC_VPCA7_VOSEL_TRANS_EN_SHIFT);
	pr_warn("[Power/PMIC] Resume: Reg[0x%x]=0x%x\n", VCA15_CON18,
		upmu_get_reg_value(VCA15_CON18));
	pr_warn("[Power/PMIC] Resume: Reg[0x%x]=0x%x\n", VPCA7_CON18,
		upmu_get_reg_value(VPCA7_CON18));

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	low_battery_protect_enable();
#endif

	mt6397_set_suspended(chip, false);

	/* amnesty for all blocked events on resume */
	for (i = 0; i < ARRAY_SIZE(chip->stat); ++i) {
		if (chip->stat[i].blocked) {
			chip->stat[i].blocked = false;
			enable_irq(i + chip->irq_base);
			pr_debug("%s: restored blocked PMIC event%d\n", __func__, i);
		}
		if (chip->stat[i].wake_blocked) {
			chip->stat[i].wake_blocked = false;
			irq_set_irq_wake(i + chip->irq_base, true);
			pr_debug("%s: restored blocked PMIC wake src %d\n", __func__, i);
		}
	}

	if (chip->wakeup_event) {
		mt6397_do_handle_events(chip, chip->wakeup_event);
		chip->wakeup_event = 0;
	}

	enable_irq(chip->irq);

	return 0;
}


static const struct of_device_id mt6397_pmic_of_match[] = {
	{ .compatible = "mediatek,mt6397-pmic", },
	{ }
};

static struct platform_driver pmic_mt6397_driver = {
	.driver = {
		.name = "mt6397-pmic",
		.owner = THIS_MODULE,
		.of_match_table = mt6397_pmic_of_match,
	},
	.probe	= pmic_mt6397_probe,
	.remove = pmic_mt6397_remove,
	.shutdown = pmic_mt6397_shutdown,
	.suspend = pmic_mt6397_suspend,
	.resume = pmic_mt6397_resume,
};

module_platform_driver(pmic_mt6397_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("PMIC Common Driver for MediaTek MT6397 PMIC");
MODULE_ALIAS("platform:mt6397-pmic");
