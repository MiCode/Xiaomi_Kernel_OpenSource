/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MFD_TABLA_CORE_H__
#define __MFD_TABLA_CORE_H__

#include <linux/interrupt.h>
#include <linux/pm_qos.h>

#define WCD9XXX_NUM_IRQ_REGS 3

#define WCD9XXX_SLIM_NUM_PORT_REG 3

#define WCD9XXX_INTERFACE_TYPE_SLIMBUS	0x00
#define WCD9XXX_INTERFACE_TYPE_I2C	0x01

#define TABLA_VERSION_1_0	0
#define TABLA_VERSION_1_1	1
#define TABLA_VERSION_2_0	2
#define TABLA_IS_1_X(ver) \
	(((ver == TABLA_VERSION_1_0) || (ver == TABLA_VERSION_1_1)) ? 1 : 0)
#define TABLA_IS_2_0(ver) ((ver == TABLA_VERSION_2_0) ? 1 : 0)

#define SITAR_VERSION_1P0 0
#define SITAR_VERSION_1P1 1
#define SITAR_IS_1P0(ver) \
	((ver == SITAR_VERSION_1P0) ? 1 : 0)
#define SITAR_IS_1P1(ver) \
	((ver == SITAR_VERSION_1P1) ? 1 : 0)

enum {
	TABLA_IRQ_SLIMBUS = 0,
	TABLA_IRQ_MBHC_REMOVAL,
	TABLA_IRQ_MBHC_SHORT_TERM,
	TABLA_IRQ_MBHC_PRESS,
	TABLA_IRQ_MBHC_RELEASE,
	TABLA_IRQ_MBHC_POTENTIAL,
	TABLA_IRQ_MBHC_INSERTION,
	TABLA_IRQ_BG_PRECHARGE,
	TABLA_IRQ_PA1_STARTUP,
	TABLA_IRQ_PA2_STARTUP,
	TABLA_IRQ_PA3_STARTUP,
	TABLA_IRQ_PA4_STARTUP,
	TABLA_IRQ_PA5_STARTUP,
	TABLA_IRQ_MICBIAS1_PRECHARGE,
	TABLA_IRQ_MICBIAS2_PRECHARGE,
	TABLA_IRQ_MICBIAS3_PRECHARGE,
	TABLA_IRQ_HPH_PA_OCPL_FAULT,
	TABLA_IRQ_HPH_PA_OCPR_FAULT,
	TABLA_IRQ_EAR_PA_OCPL_FAULT,
	TABLA_IRQ_HPH_L_PA_STARTUP,
	TABLA_IRQ_HPH_R_PA_STARTUP,
	TABLA_IRQ_EAR_PA_STARTUP,
	TABLA_NUM_IRQS,
};

enum {
	SITAR_IRQ_SLIMBUS = 0,
	SITAR_IRQ_MBHC_REMOVAL,
	SITAR_IRQ_MBHC_SHORT_TERM,
	SITAR_IRQ_MBHC_PRESS,
	SITAR_IRQ_MBHC_RELEASE,
	SITAR_IRQ_MBHC_POTENTIAL,
	SITAR_IRQ_MBHC_INSERTION,
	SITAR_IRQ_BG_PRECHARGE,
	SITAR_IRQ_PA1_STARTUP,
	SITAR_IRQ_PA2_STARTUP,
	SITAR_IRQ_PA3_STARTUP,
	SITAR_IRQ_PA4_STARTUP,
	SITAR_IRQ_PA5_STARTUP,
	SITAR_IRQ_MICBIAS1_PRECHARGE,
	SITAR_IRQ_MICBIAS2_PRECHARGE,
	SITAR_IRQ_MICBIAS3_PRECHARGE,
	SITAR_IRQ_HPH_PA_OCPL_FAULT,
	SITAR_IRQ_HPH_PA_OCPR_FAULT,
	SITAR_IRQ_EAR_PA_OCPL_FAULT,
	SITAR_IRQ_HPH_L_PA_STARTUP,
	SITAR_IRQ_HPH_R_PA_STARTUP,
	SITAR_IRQ_EAR_PA_STARTUP,
	SITAR_NUM_IRQS,
};


enum {
	TAIKO_IRQ_SLIMBUS = 0,
	TAIKO_IRQ_MBHC_REMOVAL,
	TAIKO_IRQ_MBHC_SHORT_TERM,
	TAIKO_IRQ_MBHC_PRESS,
	TAIKO_IRQ_MBHC_RELEASE,
	TAIKO_IRQ_MBHC_POTENTIAL,
	TAIKO_IRQ_MBHC_INSERTION,
	TAIKO_IRQ_BG_PRECHARGE,
	TAIKO_IRQ_PA1_STARTUP,
	TAIKO_IRQ_PA2_STARTUP,
	TAIKO_IRQ_PA3_STARTUP,
	TAIKO_IRQ_PA4_STARTUP,
	TAIKO_IRQ_PA5_STARTUP,
	TAIKO_IRQ_MICBIAS1_PRECHARGE,
	TAIKO_IRQ_MICBIAS2_PRECHARGE,
	TAIKO_IRQ_MICBIAS3_PRECHARGE,
	TAIKO_IRQ_HPH_PA_OCPL_FAULT,
	TAIKO_IRQ_HPH_PA_OCPR_FAULT,
	TAIKO_IRQ_EAR_PA_OCPL_FAULT,
	TAIKO_IRQ_HPH_L_PA_STARTUP,
	TAIKO_IRQ_HPH_R_PA_STARTUP,
	TAIKO_IRQ_EAR_PA_STARTUP,
	TAIKO_NUM_IRQS,
};

enum wcd9xxx_pm_state {
	WCD9XXX_PM_SLEEPABLE,
	WCD9XXX_PM_AWAKE,
	WCD9XXX_PM_ASLEEP,
};

struct wcd9xxx {
	struct device *dev;
	struct slim_device *slim;
	struct slim_device *slim_slave;
	struct mutex io_lock;
	struct mutex xfer_lock;
	struct mutex irq_lock;
	u8 version;

	unsigned int irq_base;
	unsigned int irq;
	u8 irq_masks_cur[WCD9XXX_NUM_IRQ_REGS];
	u8 irq_masks_cache[WCD9XXX_NUM_IRQ_REGS];
	u8 irq_level[WCD9XXX_NUM_IRQ_REGS];

	int reset_gpio;

	int (*read_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *dest, bool interface_reg);
	int (*write_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			 int bytes, void *src, bool interface_reg);

	struct regulator_bulk_data *supplies;

	enum wcd9xxx_pm_state pm_state;
	struct mutex pm_lock;
	/* pm_wq notifies change of pm_state */
	wait_queue_head_t pm_wq;
	struct pm_qos_request pm_qos_req;
	int wlock_holders;

	int num_rx_port;
	int num_tx_port;
};

int wcd9xxx_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg);
int wcd9xxx_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		u8 val);
int wcd9xxx_interface_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg);
int wcd9xxx_interface_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		u8 val);
int wcd9xxx_bulk_read(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int count, u8 *buf);
int wcd9xxx_bulk_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int count, u8 *buf);
int wcd9xxx_irq_init(struct wcd9xxx *wcd9xxx);
void wcd9xxx_irq_exit(struct wcd9xxx *wcd9xxx);
int wcd9xxx_get_logical_addresses(u8 *pgd_la, u8 *inf_la);
int wcd9xxx_get_intf_type(void);

bool wcd9xxx_lock_sleep(struct wcd9xxx *wcd9xxx);
void wcd9xxx_unlock_sleep(struct wcd9xxx *wcd9xxx);
enum wcd9xxx_pm_state wcd9xxx_pm_cmpxchg(struct wcd9xxx *wcd9xxx,
				enum wcd9xxx_pm_state o,
				enum wcd9xxx_pm_state n);

static inline int wcd9xxx_request_irq(struct wcd9xxx *wcd9xxx, int irq,
				     irq_handler_t handler, const char *name,
				     void *data)
{
	if (!wcd9xxx->irq_base)
		return -EINVAL;
	return request_threaded_irq(wcd9xxx->irq_base + irq, NULL, handler,
				    IRQF_TRIGGER_RISING, name,
				    data);
}
static inline void wcd9xxx_free_irq(struct wcd9xxx *wcd9xxx,
				int irq, void *data)
{
	if (!wcd9xxx->irq_base)
		return;
	free_irq(wcd9xxx->irq_base + irq, data);
}
static inline void wcd9xxx_enable_irq(struct wcd9xxx *wcd9xxx, int irq)
{
	if (!wcd9xxx->irq_base)
		return;
	enable_irq(wcd9xxx->irq_base + irq);
}
static inline void wcd9xxx_disable_irq(struct wcd9xxx *wcd9xxx, int irq)
{
	if (!wcd9xxx->irq_base)
		return;
	disable_irq_nosync(wcd9xxx->irq_base + irq);
}
static inline void wcd9xxx_disable_irq_sync(struct wcd9xxx *wcd9xxx, int irq)
{
	if (!wcd9xxx->irq_base)
		return;
	disable_irq(wcd9xxx->irq_base + irq);
}

#endif
