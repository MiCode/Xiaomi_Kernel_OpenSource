/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/of_irq.h>

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


#define TAIKO_VERSION_1_0	0
#define TAIKO_IS_1_0(ver) \
	((ver == TAIKO_VERSION_1_0) ? 1 : 0)


enum {
	WCD9XXX_IRQ_SLIMBUS = 0,
	WCD9XXX_IRQ_MBHC_REMOVAL,
	WCD9XXX_IRQ_MBHC_SHORT_TERM,
	WCD9XXX_IRQ_MBHC_PRESS,
	WCD9XXX_IRQ_MBHC_RELEASE,
	WCD9XXX_IRQ_MBHC_POTENTIAL,
	WCD9XXX_IRQ_MBHC_INSERTION,
	WCD9XXX_IRQ_BG_PRECHARGE,
	WCD9XXX_IRQ_PA1_STARTUP,
	WCD9XXX_IRQ_PA2_STARTUP,
	WCD9XXX_IRQ_PA3_STARTUP,
	WCD9XXX_IRQ_PA4_STARTUP,
	WCD9XXX_IRQ_PA5_STARTUP,
	WCD9XXX_IRQ_MICBIAS1_PRECHARGE,
	WCD9XXX_IRQ_MICBIAS2_PRECHARGE,
	WCD9XXX_IRQ_MICBIAS3_PRECHARGE,
	WCD9XXX_IRQ_HPH_PA_OCPL_FAULT,
	WCD9XXX_IRQ_HPH_PA_OCPR_FAULT,
	WCD9XXX_IRQ_EAR_PA_OCPL_FAULT,
	WCD9XXX_IRQ_HPH_L_PA_STARTUP,
	WCD9XXX_IRQ_HPH_R_PA_STARTUP,
	WCD9XXX_IRQ_EAR_PA_STARTUP,
	WCD9XXX_NUM_IRQS,
};

enum {
	TABLA_NUM_IRQS = WCD9XXX_NUM_IRQS,
	SITAR_NUM_IRQS = WCD9XXX_NUM_IRQS,
	TAIKO_NUM_IRQS = WCD9XXX_NUM_IRQS,
};


#define MAX(X, Y) (((int)X) >= ((int)Y) ? (X) : (Y))
#define WCD9XXX_MAX_NUM_IRQS (MAX(MAX(TABLA_NUM_IRQS, SITAR_NUM_IRQS), \
				  TAIKO_NUM_IRQS))

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

	int reset_gpio;

	int (*read_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *dest, bool interface_reg);
	int (*write_dev)(struct wcd9xxx *wcd9xxx, unsigned short reg,
			 int bytes, void *src, bool interface_reg);

	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;

	enum wcd9xxx_pm_state pm_state;
	struct mutex pm_lock;
	/* pm_wq notifies change of pm_state */
	wait_queue_head_t pm_wq;
	struct pm_qos_request pm_qos_req;
	int wlock_holders;

	int num_rx_port;
	int num_tx_port;

	u8 idbyte[4];

	unsigned int irq_base;
	unsigned int irq;
	u8 irq_masks_cur[WCD9XXX_NUM_IRQ_REGS];
	u8 irq_masks_cache[WCD9XXX_NUM_IRQ_REGS];
	bool irq_level_high[WCD9XXX_MAX_NUM_IRQS];
	int num_irqs;
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

int wcd9xxx_request_irq(struct wcd9xxx *wcd9xxx, int irq,
			irq_handler_t handler, const char *name, void *data);

void wcd9xxx_free_irq(struct wcd9xxx *wcd9xxx, int irq, void *data);
void wcd9xxx_enable_irq(struct wcd9xxx *wcd9xxx, int irq);
void wcd9xxx_disable_irq(struct wcd9xxx *wcd9xxx, int irq);
void wcd9xxx_disable_irq_sync(struct wcd9xxx *wcd9xxx, int irq);
#ifdef CONFIG_OF
int __init wcd9xxx_irq_of_init(struct device_node *node,
			       struct device_node *parent);
#endif /* CONFIG_OF */
#endif
