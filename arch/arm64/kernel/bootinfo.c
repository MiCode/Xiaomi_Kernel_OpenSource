/*
 * bootinfo.c
 *
 * Copyright (C) 2011 Xiaomi Ltd.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <linux/bitops.h>
#include <linux/input/qpnp-power-on.h>

#define PMIC_NUM (16)
static int pmic_v[PMIC_NUM];
static const char * const poweroff_reasons[POFF_REASON_MAX] = {
	[POFF_REASON_EVENT_SOFT]		= "soft",
	[POFF_REASON_EVENT_PS_HOLD]		= "ps_hold",
	[POFF_REASON_EVENT_PMIC_WD]		= "pmic_wd",
	[POFF_REASON_EVENT_GP1_KPD1]		= "keypad_reset1",
	[POFF_REASON_EVENT_GP2_KPD2]		= "keypad_reset2",
	[POFF_REASON_EVENT_KPDPWR_AND_RESIN]	= "kpdpwr_resin",
	[POFF_REASON_EVENT_RESIN_N]		= "resin_n",
	[POFF_REASON_EVENT_KPDPWR_N]		= "kpdpwr_n",
	[POFF_REASON_EVENT_RESEVER1]		= "resever1",
	[POFF_REASON_EVENT_RESEVER2]		= "resever2",
	[POFF_REASON_EVENT_RESEVER3]		= "resever3",
	[POFF_REASON_EVENT_CHARGER]		= "charger",
	[POFF_REASON_EVENT_TFT]			= "tft",
	[POFF_REASON_EVENT_UVLO]		= "uvlo",
	[POFF_REASON_EVENT_OTST3]		= "otst3",
	[POFF_REASON_EVENT_STAGE3]		= "stage3",
	[POFF_REASON_EVENT_GP_FAULT0]		= "gp_fault0",
	[POFF_REASON_EVENT_GP_FAULT1]		= "gp_fault1",
	[POFF_REASON_EVENT_GP_FAULT2]		= "gp_fault2",
	[POFF_REASON_EVENT_GP_FAULT3]		= "gp_fault3",
	[POFF_REASON_EVENT_MBG_FAULT]		= "mbg_fault",
	[POFF_REASON_EVENT_OVLO]		= "ovlo",
	[POFF_REASON_EVENT_GEN2_UVLO]		= "gen2_uvlo",
	[POFF_REASON_EVENT_AVDD_RB]		= "avdd_rb",
	[POFF_REASON_EVENT_RESEVER4]		= "resever4",
	[POFF_REASON_EVENT_RESEVER5]		= "resever5",
	[POFF_REASON_EVENT_RESEVER6]		= "resever6",
	[POFF_REASON_EVENT_FAULT_FAULT_N]	= "fault_n",
	[POFF_REASON_EVENT_FAULT_PBS_WATCHDOG_TO] = "fault_pbs_watchdog",
	[POFF_REASON_EVENT_FAULT_PBS_NACK]	= "fault_pbs_nack",
	[POFF_REASON_EVENT_FAULT_RESTART_PON]	= "fault_restart_pon",
	[POFF_REASON_EVENT_GEN2_OTST3]		= "otst3",
	[POFF_REASON_EVENT_RESEVER7]		= "resever7",
	[POFF_REASON_EVENT_RESEVER8]		= "resever8",
	[POFF_REASON_EVENT_RESEVER9]		= "resever9",
	[POFF_REASON_EVENT_RESEVER10]		= "resever10",
	[POFF_REASON_EVENT_S3_RESET_FAULT_N]	= "s3_reset_fault_n",
	[POFF_REASON_EVENT_S3_RESET_PBS_WATCHDOG_TO] = "s3_reset_pbs_watchdog",
	[POFF_REASON_EVENT_S3_RESET_PBS_NACK]	= "s3_reset_pbs_nack",
	[POFF_REASON_EVENT_S3_RESET_KPDPWR_ANDOR_RESIN] = "s3_reset_kpdpwr_andor_resin",
};

static const char * const powerup_reasons[PU_REASON_MAX] = {
	[PU_REASON_EVENT_KPD]		= "keypad",
	[PU_REASON_EVENT_RTC]		= "rtc",
	[PU_REASON_EVENT_CABLE]		= "cable",
	[PU_REASON_EVENT_SMPL]		= "smpl",
	[PU_REASON_EVENT_PON1]		= "pon1",
	[PU_REASON_EVENT_USB_CHG]	= "usb_chg",
	[PU_REASON_EVENT_DC_CHG]	= "dc_chg",
	[PU_REASON_EVENT_HWRST]		= "hw_reset",
	[PU_REASON_EVENT_LPK]		= "long_power_key",
};

static const char * const reset_reasons[RS_REASON_MAX] = {
	[RS_REASON_EVENT_WDOG]          = "wdog",
	[RS_REASON_EVENT_KPANIC]        = "kpanic",
	[RS_REASON_EVENT_NORMAL]        = "reboot",
	[RS_REASON_EVENT_OTHER]         = "other",
};

static struct kobject *bootinfo_kobj;
static powerup_reason_t powerup_reason;

#define bootinfo_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= NULL,				\
}

#define bootinfo_func_init(type, name, initval)	\
static type name = (initval);			\
type get_##name(void)				\
{						\
	return name;				\
}						\
void set_##name(type __##name)			\
{						\
	name = __##name;			\
}

int is_abnormal_powerup(void)
{
	u32 pu_reason = get_powerup_reason();

	return (pu_reason & (RESTART_EVENT_KPANIC | RESTART_EVENT_WDOG)) |
		(pu_reason & BIT(PU_REASON_EVENT_HWRST) & RESTART_EVENT_OTHER);
}

static ssize_t powerup_reason_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	u32 pu_reason;
	int pu_reason_index = PU_REASON_MAX;
	u32 reset_reason;
	int reset_reason_index = RS_REASON_MAX;

	pu_reason = get_powerup_reason();
	if (((pu_reason & BIT(PU_REASON_EVENT_HWRST))
		&& qpnp_pon_is_ps_hold_reset()) ||
		(pu_reason & BIT(PU_REASON_EVENT_WARMRST))) {
		reset_reason = pu_reason >> 16;
		reset_reason_index = find_first_bit((unsigned long *)&reset_reason,
				sizeof(reset_reason)*BITS_PER_BYTE);
		if (reset_reason_index < RS_REASON_MAX && reset_reason_index >= 0) {
			s += snprintf(s, strlen(reset_reasons[reset_reason_index]) + 2,
					"%s\n", reset_reasons[reset_reason_index]);
			pr_debug("%s: rs_reason [0x%x], first non-zero bit %d\n",
				__func__, reset_reason, reset_reason_index);
			goto out;
		};
	}
	if (qpnp_pon_is_lpk() &&
		(pu_reason & BIT(PU_REASON_EVENT_HWRST)))
		pu_reason_index = PU_REASON_EVENT_LPK;
	else if (pu_reason & BIT(PU_REASON_EVENT_HWRST))
		pu_reason_index = PU_REASON_EVENT_HWRST;
	else if (pu_reason & BIT(PU_REASON_EVENT_SMPL))
		pu_reason_index = PU_REASON_EVENT_SMPL;
	else if (pu_reason & BIT(PU_REASON_EVENT_RTC))
		pu_reason_index = PU_REASON_EVENT_RTC;
	else if (pu_reason & BIT(PU_REASON_EVENT_USB_CHG))
		pu_reason_index = PU_REASON_EVENT_USB_CHG;
	else if (pu_reason & BIT(PU_REASON_EVENT_DC_CHG))
		pu_reason_index = PU_REASON_EVENT_DC_CHG;
	else if (pu_reason & BIT(PU_REASON_EVENT_KPD))
		pu_reason_index = PU_REASON_EVENT_KPD;
	else if (pu_reason & BIT(PU_REASON_EVENT_PON1))
		pu_reason_index = PU_REASON_EVENT_PON1;
	if (pu_reason_index < PU_REASON_MAX && pu_reason_index >= 0) {
		s += snprintf(s, strlen(powerup_reasons[pu_reason_index]) + 2,
				"%s\n", powerup_reasons[pu_reason_index]);
		pr_debug("%s: pu_reason [0x%x] index %d\n",
			__func__, pu_reason, pu_reason_index);
		goto out;
	}
	s += snprintf(s, 15, "unknown reboot\n");
out:
	return (s - buf);
}

static ssize_t powerup_reason_details_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	u32 pu_reason;

	pu_reason = get_powerup_reason();

	return snprintf(buf, 11, "0x%x\n", pu_reason);
}

void set_poweroff_reason(int pmicv)
{
	int i = 0;

	while (i < PMIC_NUM) {
		if (pmic_v[i] != -1)
			i++;
		else {
			pmic_v[i] = pmicv;
			break;
		}
	}
}

static ssize_t poweroff_reason_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int i = 0;
	int l = 0;
	int v = pmic_v[0];

	if (v == -1)
		return snprintf(buf, 10, " unknown \n");

	while ((i < PMIC_NUM) && (pmic_v[i] != -1)) {
		v = pmic_v[i];
		i++;
		if (v >= 0 && v < POFF_REASON_MAX)
			l += snprintf(buf + l,
				(strlen(poweroff_reasons[v]) + 10),
				" PNo.%d-%s ", i - 1, poweroff_reasons[v]);
		else
			l += snprintf(buf + l, 17, " PNo.%d-%s ", i - 1, "unknown");
	}
	l += snprintf(buf + l, 2, "\n");

	return l;
}

bootinfo_attr(poweroff_reason);
bootinfo_attr(powerup_reason);
bootinfo_attr(powerup_reason_details);
bootinfo_func_init(u32, powerup_reason, 0);

static struct attribute *g[] = {
	&poweroff_reason_attr.attr,
	&powerup_reason_attr.attr,
	&powerup_reason_details_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (bootinfo_kobj == NULL) {
		pr_err("bootinfo_init: subsystem_register failed\n");
		goto fail;
	}

	memset(pmic_v, -1, sizeof(pmic_v));
	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		pr_err("bootinfo_init: subsystem_register failed\n");
		goto sys_fail;
	}

	return ret;

sys_fail:
	kobject_del(bootinfo_kobj);
fail:
	return ret;

}

static void __exit bootinfo_exit(void)
{
	if (bootinfo_kobj) {
		sysfs_remove_group(bootinfo_kobj, &attr_group);
		kobject_del(bootinfo_kobj);
	}
}

core_initcall(bootinfo_init);
module_exit(bootinfo_exit);
