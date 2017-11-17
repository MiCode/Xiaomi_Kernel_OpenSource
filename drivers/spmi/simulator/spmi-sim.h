/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __SPMI_SIMULATOR_H_
#define __SPMI_SIMULATOR_H_

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/kernel.h>

struct spmi_sim;

/**
 * struct spmi_sim_ops - spmi register read/write hook operations
 *
 * @pre_read:	Called before a consumer SPMI register read occurs.
 * @post_read:	Called after a consumer SPMI register read has occurred.  val
 *		corresponds to the register value that will be passed back to
 *		the consumer.  It may be modified as desired without affecting
 *		the stored register state.
 * @pre_write:	Called before a consumer SPMI register write has occurred.  val
 *		corresponds to the consumer register value that is about to be
 *		written.  It may be modified as desired before it is stored as
 *		the register state.
 * @post_write:	Called after a consumer SPMI register write has occurred.
 *
 * These callbacks can be used by PMIC peripheral simulator drivers to hook into
 * SPMI consumer register read and write calls.  They are triggered before and
 * after each SPMI read or write transaction.  The addr value provided
 * corresponds to the full 20-bit SPMI address of the register being accessed.
 * spmi_sim_read() and spmi_sim_write() can be used from within these callbacks
 * in order to manage the simulated PMIC peripheral register state as needed.
 */
struct spmi_sim_ops {
	int (*pre_read)(struct spmi_sim *sim_ctrl, u32 addr);
	int (*post_read)(struct spmi_sim *sim_ctrl, u32 addr, u8 *val);
	int (*pre_write)(struct spmi_sim *sim_ctrl, u32 addr, u8 *val);
	int (*post_write)(struct spmi_sim *sim_ctrl, u32 addr);
};

/* Permissions to use in spmi_sim_init_register() calls */
#define SPMI_SIM_PERM_RW	0
#define SPMI_SIM_PERM_W		BIT(0)
#define SPMI_SIM_PERM_R		BIT(1)

/**
 * struct spmi_sim_register_init - register value and permission initializer
 *
 * @addr:		Full 20-bit SPMI register address
 * @value:		SPMI register value
 * @permissions:	Read/write permissions for the register.  Must use
 *			one of SPMI_SIM_PERM_[RW|R|W].
 */
struct spmi_sim_register_init {
	u32			addr;
	u8			value;
	u8			permissions;
};

/**
 * struct spmi_sim_register_ops_init - register operator initializer
 *
 * @addr:		Full 20-bit SPMI register address
 * @ops:		Pointer to register operators
 */
struct spmi_sim_register_ops_init {
	u32			addr;
	struct spmi_sim_ops	*ops;
};

#ifdef CONFIG_SPMI_SIMULATOR

struct spmi_sim *spmi_sim_get(struct device *dev);
int spmi_sim_init_register(struct spmi_sim *sim,
			const struct spmi_sim_register_init *regs,
			size_t count,
			u32 base_addr);
int spmi_sim_register_ops(struct spmi_sim *sim,
			const struct spmi_sim_register_ops_init *reg_ops,
			size_t count,
			u32 base_addr);
int spmi_sim_unregister_ops(struct spmi_sim *sim,
			const struct spmi_sim_register_ops_init *reg_ops,
			size_t count,
			u32 base_addr);
int spmi_sim_read(struct spmi_sim *sim, u32 addr, u8 *val);
int spmi_sim_write(struct spmi_sim *sim, u32 addr, u8 val);
int spmi_sim_masked_write(struct spmi_sim *sim, u32 addr, u8 mask, u8 val);
int spmi_sim_trigger_irq(struct spmi_sim *sim, u8 sid, u8 per, u8 irq);
int spmi_sim_set_irq_rt_status(struct spmi_sim *sim, u8 sid, u8 per, u8 irq,
			u8 rt_status);

#else

static inline struct spmi_sim *spmi_sim_get(struct device *dev)
{ return ERR_PTR(-ENODEV); }
static inline int spmi_sim_init_register(struct spmi_sim *sim,
			const struct spmi_sim_register_init *reg,
			size_t count,
			u32 base_addr)
{ return -ENODEV; }
static inline int spmi_sim_register_ops(struct spmi_sim *sim,
			const struct spmi_sim_register_ops_init *reg_ops,
			size_t count,
			u32 base_addr)
{ return -ENODEV; }
static inline int spmi_sim_unregister_ops(struct spmi_sim *sim,
			const struct spmi_sim_register_ops_init *reg_ops,
			size_t count,
			u32 base_addr)
{ return -ENODEV; }
static inline int spmi_sim_read(struct spmi_sim *sim, u32 addr, u8 *val)
{ return -ENODEV; }
static inline int spmi_sim_write(struct spmi_sim *sim, u32 addr, u8 val)
{ return -ENODEV; }
static inline int spmi_sim_masked_write(struct spmi_sim *sim, u32 addr, u8 mask,
			u8 val)
{ return -ENODEV; }
static inline int spmi_sim_trigger_irq(struct spmi_sim *sim, u8 sid, u8 per,
			u8 irq)
{ return -ENODEV; }
static inline int spmi_sim_set_irq_rt_status(struct spmi_sim *sim, u8 sid,
			u8 per, u8 irq, u8 rt_status)
{ return -ENODEV; }

#endif
#endif
