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

#ifndef _PMIC_MT6397_H_
#define _PMIC_MT6397_H_

extern void kpd_pwrkey_pmic_handler(unsigned long pressed);
extern void kpd_pmic_rstkey_handler(unsigned long pressed);
extern int accdet_irq_handler(void);
extern void accdet_auxadc_switch(int enable);

struct mt6397_event_stat {
	u64 last;
	int count;
	bool blocked:1;
	bool wake_blocked:1;
};

struct mt6397_chip_priv {
	struct device *dev;
	struct irq_domain *domain;
	unsigned long event_mask;
	unsigned long wake_mask;
	unsigned int saved_mask;
	u16 int_con[2];
	u16 int_stat[2];
	int irq;
	int irq_base;
	int num_int;
	int irq_hw_id;
	bool suspended:1;
	unsigned int wakeup_event;
	struct mt6397_event_stat stat[32];
};

struct mt6397_irq_data {
	const char *name;
	unsigned int irq_id;
	 irqreturn_t (*action_fn)(int irq, void *dev_id);
	bool enabled:1;
	bool wake_src:1;
};

#ifdef CONFIG_MTK_BATTERY_PROTECT
extern void low_battery_protect_init(struct device *device);
extern void low_battery_protect_enable(void);
extern void low_battery_protect_disable(void);
extern irqreturn_t bat_l_int_handler(int irq, void *dev_id);
extern irqreturn_t bat_h_int_handler(int irq, void *dev_id);
extern void lbat_min_en_setting_nolock(int en_val);
extern void lbat_max_en_setting_nolock(int en_val);
extern void lbat_min_en_setting(int en_val);
extern void lbat_max_en_setting(int en_val);
extern void toggle_lbat_irq_src(bool enable, u16 hw_irq);
#endif

#endif				/* _PMIC_MT6397_H_ */
