/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_INTERRUPTS_H
#define _SDE_HW_INTERRUPTS_H

#include <linux/types.h>

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_util.h"
#include "sde_hw_mdss.h"

#define IRQ_SOURCE_MDP		BIT(0)
#define IRQ_SOURCE_DSI0		BIT(4)
#define IRQ_SOURCE_DSI1		BIT(5)
#define IRQ_SOURCE_HDMI		BIT(8)
#define IRQ_SOURCE_EDP		BIT(12)
#define IRQ_SOURCE_MHL		BIT(16)

/**
 * sde_intr_type - HW Interrupt Type
 * @SDE_IRQ_TYPE_WB_ROT_COMP:		WB rotator done
 * @SDE_IRQ_TYPE_WB_WFD_COMP:		WB WFD done
 * @SDE_IRQ_TYPE_PING_PONG_COMP:	PingPong done
 * @SDE_IRQ_TYPE_PING_PONG_RD_PTR:	PingPong read pointer
 * @SDE_IRQ_TYPE_PING_PONG_WR_PTR:	PingPong write pointer
 * @SDE_IRQ_TYPE_PING_PONG_AUTO_REF:	PingPong auto refresh
 * @SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK:	PingPong Tear check
 * @SDE_IRQ_TYPE_PING_PONG_TE_CHECK:	PingPong TE detection
 * @SDE_IRQ_TYPE_INTF_UNDER_RUN:	INTF underrun
 * @SDE_IRQ_TYPE_INTF_VSYNC:		INTF VSYNC
 * @SDE_IRQ_TYPE_CWB_OVERFLOW:		Concurrent WB overflow
 * @SDE_IRQ_TYPE_HIST_VIG_DONE:		VIG Histogram done
 * @SDE_IRQ_TYPE_HIST_VIG_RSTSEQ:	VIG Histogram reset
 * @SDE_IRQ_TYPE_HIST_DSPP_DONE:	DSPP Histogram done
 * @SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ:	DSPP Histogram reset
 * @SDE_IRQ_TYPE_WD_TIMER:		Watchdog timer
 * @SDE_IRQ_TYPE_SFI_VIDEO_IN:		Video static frame INTR into static
 * @SDE_IRQ_TYPE_SFI_VIDEO_OUT:		Video static frame INTR out-of static
 * @SDE_IRQ_TYPE_SFI_CMD_0_IN:		DSI CMD0 static frame INTR into static
 * @SDE_IRQ_TYPE_SFI_CMD_0_OUT:		DSI CMD0 static frame INTR out-of static
 * @SDE_IRQ_TYPE_SFI_CMD_1_IN:		DSI CMD1 static frame INTR into static
 * @SDE_IRQ_TYPE_SFI_CMD_1_OUT:		DSI CMD1 static frame INTR out-of static
 * @SDE_IRQ_TYPE_SFI_CMD_2_IN:		DSI CMD2 static frame INTR into static
 * @SDE_IRQ_TYPE_SFI_CMD_2_OUT:		DSI CMD2 static frame INTR out-of static
 * @SDE_IRQ_TYPE_PROG_LINE:		Programmable Line interrupt
 * @SDE_IRQ_TYPE_AD4_BL_DONE:		AD4 backlight
 * @SDE_IRQ_TYPE_CTL_START:		Control start
 * @SDE_IRQ_TYPE_RESERVED:		Reserved for expansion
 */
enum sde_intr_type {
	SDE_IRQ_TYPE_WB_ROT_COMP,
	SDE_IRQ_TYPE_WB_WFD_COMP,
	SDE_IRQ_TYPE_PING_PONG_COMP,
	SDE_IRQ_TYPE_PING_PONG_RD_PTR,
	SDE_IRQ_TYPE_PING_PONG_WR_PTR,
	SDE_IRQ_TYPE_PING_PONG_AUTO_REF,
	SDE_IRQ_TYPE_PING_PONG_TEAR_CHECK,
	SDE_IRQ_TYPE_PING_PONG_TE_CHECK,
	SDE_IRQ_TYPE_INTF_UNDER_RUN,
	SDE_IRQ_TYPE_INTF_VSYNC,
	SDE_IRQ_TYPE_CWB_OVERFLOW,
	SDE_IRQ_TYPE_HIST_VIG_DONE,
	SDE_IRQ_TYPE_HIST_VIG_RSTSEQ,
	SDE_IRQ_TYPE_HIST_DSPP_DONE,
	SDE_IRQ_TYPE_HIST_DSPP_RSTSEQ,
	SDE_IRQ_TYPE_WD_TIMER,
	SDE_IRQ_TYPE_SFI_VIDEO_IN,
	SDE_IRQ_TYPE_SFI_VIDEO_OUT,
	SDE_IRQ_TYPE_SFI_CMD_0_IN,
	SDE_IRQ_TYPE_SFI_CMD_0_OUT,
	SDE_IRQ_TYPE_SFI_CMD_1_IN,
	SDE_IRQ_TYPE_SFI_CMD_1_OUT,
	SDE_IRQ_TYPE_SFI_CMD_2_IN,
	SDE_IRQ_TYPE_SFI_CMD_2_OUT,
	SDE_IRQ_TYPE_PROG_LINE,
	SDE_IRQ_TYPE_AD4_BL_DONE,
	SDE_IRQ_TYPE_CTL_START,
	SDE_IRQ_TYPE_RESERVED,
};

struct sde_hw_intr;

/**
 * Interrupt operations.
 */
struct sde_hw_intr_ops {
	/**
	 * set_mask - Programs the given interrupt register with the
	 *            given interrupt mask. Register value will get overwritten.
	 * @intr:	HW interrupt handle
	 * @reg_off:	MDSS HW register offset
	 * @irqmask:	IRQ mask value
	 */
	void (*set_mask)(
			struct sde_hw_intr *intr,
			uint32_t reg,
			uint32_t irqmask);

	/**
	 * irq_idx_lookup - Lookup IRQ index on the HW interrupt type
	 *                 Used for all irq related ops
	 * @intr_type:		Interrupt type defined in sde_intr_type
	 * @instance_idx:	HW interrupt block instance
	 * @return:		irq_idx or -EINVAL for lookup fail
	 */
	int (*irq_idx_lookup)(
			enum sde_intr_type intr_type,
			u32 instance_idx);

	/**
	 * enable_irq - Enable IRQ based on lookup IRQ index
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @return:	0 for success, otherwise failure
	 */
	int (*enable_irq)(
			struct sde_hw_intr *intr,
			int irq_idx);

	/**
	 * disable_irq - Disable IRQ based on lookup IRQ index
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @return:	0 for success, otherwise failure
	 */
	int (*disable_irq)(
			struct sde_hw_intr *intr,
			int irq_idx);

	/**
	 * disable_irq_nolock - Disable IRQ based on IRQ index without lock
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @return:	0 for success, otherwise failure
	 */
	int (*disable_irq_nolock)(
			struct sde_hw_intr *intr,
			int irq_idx);

	/**
	 * clear_all_irqs - Clears all the interrupts (i.e. acknowledges
	 *                  any asserted IRQs). Useful during reset.
	 * @intr:	HW interrupt handle
	 * @return:	0 for success, otherwise failure
	 */
	int (*clear_all_irqs)(
			struct sde_hw_intr *intr);

	/**
	 * disable_all_irqs - Disables all the interrupts. Useful during reset.
	 * @intr:	HW interrupt handle
	 * @return:	0 for success, otherwise failure
	 */
	int (*disable_all_irqs)(
			struct sde_hw_intr *intr);

	/**
	 * dispatch_irqs - IRQ dispatcher will call the given callback
	 *                 function when a matching interrupt status bit is
	 *                 found in the irq mapping table.
	 * @intr:	HW interrupt handle
	 * @cbfunc:	Callback function pointer
	 * @arg:	Argument to pass back during callback
	 */
	void (*dispatch_irqs)(
			struct sde_hw_intr *intr,
			void (*cbfunc)(void *arg, int irq_idx),
			void *arg);

	/**
	 * get_interrupt_statuses - Gets and store value from all interrupt
	 *                          status registers that are currently fired.
	 * @intr:	HW interrupt handle
	 */
	void (*get_interrupt_statuses)(
			struct sde_hw_intr *intr);

	/**
	 * clear_interrupt_status - Clears HW interrupt status based on given
	 *                          lookup IRQ index.
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 */
	void (*clear_interrupt_status)(
			struct sde_hw_intr *intr,
			int irq_idx);

	/**
	 * clear_intr_status_nolock() - clears the HW interrupts without lock
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 */
	void (*clear_intr_status_nolock)(
			struct sde_hw_intr *intr,
			int irq_idx);

	/**
	 * clear_intr_status_force_mask() - clear the HW interrupts
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @irq_mask:	irq mask to clear
	 */
	void (*clear_intr_status_force_mask)(
			struct sde_hw_intr *intr,
			int irq_idx,
			u32 irq_mask);

	/**
	 * get_interrupt_status - Gets HW interrupt status, and clear if set,
	 *                        based on given lookup IRQ index.
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @clear:	True to clear irq after read
	 */
	u32 (*get_interrupt_status)(
			struct sde_hw_intr *intr,
			int irq_idx,
			bool clear);

	/**
	 * get_intr_status_nolock - nolock version of get_interrupt_status
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @clear:	True to clear irq after read
	 */
	u32 (*get_intr_status_nolock)(
			struct sde_hw_intr *intr,
			int irq_idx,
			bool clear);

	/**
	 * get_intr_status_nomask - nolock version of get_interrupt_status
	 * @intr:	HW interrupt handle
	 * @irq_idx:	Lookup irq index return from irq_idx_lookup
	 * @clear:	True to clear irq after read
	 */
	u32 (*get_intr_status_nomask)(
			struct sde_hw_intr *intr,
			int irq_idx,
			bool clear);

	/**
	 * get_valid_interrupts - Gets a mask of all valid interrupt sources
	 *                        within SDE. These are actually status bits
	 *                        within interrupt registers that specify the
	 *                        source of the interrupt in IRQs. For example,
	 *                        valid interrupt sources can be MDP, DSI,
	 *                        HDMI etc.
	 * @intr:	HW interrupt handle
	 * @mask:	Returning the interrupt source MASK
	 * @return:	0 for success, otherwise failure
	 */
	int (*get_valid_interrupts)(
			struct sde_hw_intr *intr,
			uint32_t *mask);

	/**
	 * get_interrupt_sources - Gets the bitmask of the SDE interrupt
	 *                         source that are currently fired.
	 * @intr:	HW interrupt handle
	 * @sources:	Returning the SDE interrupt source status bit mask
	 * @return:	0 for success, otherwise failure
	 */
	int (*get_interrupt_sources)(
			struct sde_hw_intr *intr,
			uint32_t *sources);
};

/**
 * struct sde_hw_intr: hw interrupts handling data structure
 * @hw:               virtual address mapping
 * @ops:              function pointer mapping for IRQ handling
 * @cache_irq_mask:   array of IRQ enable masks reg storage created during init
 * @save_irq_status:  array of IRQ status reg storage created during init
 * @irq_idx_tbl_size: total number of irq_idx mapped in the hw_interrupts
 * @irq_lock:         spinlock for accessing IRQ resources
 */
struct sde_hw_intr {
	struct sde_hw_blk_reg_map hw;
	struct sde_hw_intr_ops ops;
	u32 *cache_irq_mask;
	u32 *save_irq_status;
	u32 irq_idx_tbl_size;
	spinlock_t irq_lock;
};

/**
 * sde_hw_intr_init(): Initializes the interrupts hw object
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_intr *sde_hw_intr_init(void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_intr_destroy(): Cleanup interrutps hw object
 * @intr: pointer to interrupts hw object
 */
void sde_hw_intr_destroy(struct sde_hw_intr *intr);
#endif
