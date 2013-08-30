/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __MFD_CORE_RESOURCE_H__
#define __MFD_CORE_RESOURCE_H__

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>

#define WCD9XXX_MAX_IRQ_REGS 4
#define WCD9XXX_MAX_NUM_IRQS (WCD9XXX_MAX_IRQ_REGS * 8)

struct intr_data {
	int intr_num;
	bool clear_first;
};

enum wcd9xxx_pm_state {
	WCD9XXX_PM_SLEEPABLE,
	WCD9XXX_PM_AWAKE,
	WCD9XXX_PM_ASLEEP,
};

enum wcd9xxx_intf_status {
	WCD9XXX_INTERFACE_TYPE_PROBING,
	WCD9XXX_INTERFACE_TYPE_SLIMBUS,
	WCD9XXX_INTERFACE_TYPE_I2C,
};

struct wcd9xxx_core_resource {
	struct mutex irq_lock;
	struct mutex nested_irq_lock;

	enum wcd9xxx_pm_state pm_state;
	struct mutex pm_lock;
	/* pm_wq notifies change of pm_state */
	wait_queue_head_t pm_wq;
	struct pm_qos_request pm_qos_req;
	int wlock_holders;


	/* holds the table of interrupts per codec */
	const struct intr_data *intr_table;
	int intr_table_size;
	unsigned int irq_base;
	unsigned int irq;
	u8 irq_masks_cur[WCD9XXX_MAX_IRQ_REGS];
	u8 irq_masks_cache[WCD9XXX_MAX_IRQ_REGS];
	bool irq_level_high[WCD9XXX_MAX_NUM_IRQS];
	int num_irqs;
	int num_irq_regs;

	/* Callback functions to read/write codec registers */
	int (*codec_reg_read) (struct wcd9xxx_core_resource *,
				unsigned short);
	int (*codec_reg_write) (struct wcd9xxx_core_resource *,
				unsigned short, u8);
	int (*codec_bulk_read) (struct wcd9xxx_core_resource *,
				unsigned short, int, u8 *);

	/* Pointer to parent container data structure */
	void *parent;

	struct device *dev;
};

extern int wcd9xxx_core_res_init(
	struct wcd9xxx_core_resource*,
	int, int,
	int (*codec_read)(struct wcd9xxx_core_resource *, unsigned short),
	int (*codec_write)(struct wcd9xxx_core_resource *, unsigned short, u8),
	int (*codec_bulk_read) (struct wcd9xxx_core_resource *, unsigned short,
							int, u8 *));

extern void wcd9xxx_core_res_deinit(
	struct wcd9xxx_core_resource *);

extern int wcd9xxx_core_res_suspend(
	struct wcd9xxx_core_resource *,
	pm_message_t);

extern int wcd9xxx_core_res_resume(
	struct wcd9xxx_core_resource *);

extern int wcd9xxx_core_irq_init(
	struct wcd9xxx_core_resource*);

extern int wcd9xxx_initialize_irq(
	struct wcd9xxx_core_resource*,
	unsigned int,
	unsigned int);

enum wcd9xxx_intf_status wcd9xxx_get_intf_type(void);
void wcd9xxx_set_intf_type(enum wcd9xxx_intf_status);

bool wcd9xxx_lock_sleep(struct wcd9xxx_core_resource *);
void wcd9xxx_unlock_sleep(struct wcd9xxx_core_resource *);
void wcd9xxx_nested_irq_lock(struct wcd9xxx_core_resource *);
void wcd9xxx_nested_irq_unlock(struct wcd9xxx_core_resource *);
enum wcd9xxx_pm_state wcd9xxx_pm_cmpxchg(
			struct wcd9xxx_core_resource *,
			enum wcd9xxx_pm_state,
			enum wcd9xxx_pm_state);

int wcd9xxx_request_irq(struct wcd9xxx_core_resource *, int,
			irq_handler_t, const char *, void *);

void wcd9xxx_free_irq(struct wcd9xxx_core_resource *, int, void*);
void wcd9xxx_enable_irq(struct wcd9xxx_core_resource *, int);
void wcd9xxx_disable_irq(struct wcd9xxx_core_resource *, int);
void wcd9xxx_disable_irq_sync(struct wcd9xxx_core_resource *, int);
int wcd9xxx_reg_read(struct wcd9xxx_core_resource *,
					 unsigned short);
int wcd9xxx_reg_write(struct wcd9xxx_core_resource *,
					  unsigned short, u8);
int wcd9xxx_bulk_read(struct wcd9xxx_core_resource *,
					unsigned short, int, u8 *);
int wcd9xxx_bulk_write(struct wcd9xxx_core_resource*,
					 unsigned short, int, u8*);
int wcd9xxx_irq_init(struct wcd9xxx_core_resource *);
void wcd9xxx_irq_exit(struct wcd9xxx_core_resource *);
int wcd9xxx_core_res_resume(
	struct wcd9xxx_core_resource *);
int wcd9xxx_core_res_suspend(
	struct wcd9xxx_core_resource *,
	pm_message_t);
#endif
