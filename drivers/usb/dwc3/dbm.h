/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __DBM_H
#define __DBM_H

#include <linux/types.h>
#include <linux/usb/gadget.h>

/**
 *  USB DBM  Hardware registers bitmask.
 *
 */
/* DBM_EP_CFG */
#define DBM_EN_EP		0x00000001
#define USB3_EPNUM		0x0000003E
#define DBM_BAM_PIPE_NUM	0x000000C0
#define DBM_PRODUCER		0x00000100
#define DBM_DISABLE_WB		0x00000200
#define DBM_INT_RAM_ACC		0x00000400

/* DBM_DATA_FIFO_SIZE */
#define DBM_DATA_FIFO_SIZE_MASK	0x0000ffff

/* DBM_GEVNTSIZ */
#define DBM_GEVNTSIZ_MASK	0x0000ffff

/* DBM_DBG_CNFG */
#define DBM_ENABLE_IOC_MASK	0x0000000f

/* DBM_SOFT_RESET */
#define DBM_SFT_RST_EP0		0x00000001
#define DBM_SFT_RST_EP1		0x00000002
#define DBM_SFT_RST_EP2		0x00000004
#define DBM_SFT_RST_EP3		0x00000008
#define DBM_SFT_RST_EPS_MASK	0x0000000F
#define DBM_SFT_RST_MASK	0x80000000
#define DBM_EN_MASK		0x00000002

/* DBM TRB configurations */
#define DBM_TRB_BIT		0x80000000
#define DBM_TRB_DATA_SRC	0x40000000
#define DBM_TRB_DMA		0x20000000
#define DBM_TRB_EP_NUM(ep)	(ep<<24)


struct dbm {
	struct device		*dev;
	struct list_head	head;

	/* Reset the DBM registers upon initialization */
	int (*soft_reset)(bool reset);

	/* Configure a USB DBM ep to work in BAM mode */
	int (*ep_config)(u8 usb_ep, u8 bam_pipe,
			bool producer, bool disable_wb,
			bool internal_mem, bool ioc);

	/* Configure a USB DBM ep to work in normal mode */
	int (*ep_unconfig)(u8 usb_ep);

	/* Return number of configured DBM endpoints */
	int (*get_num_of_eps_configured)(void);

	/* Configure the DBM with the USB3 core event buffer */
	int (*event_buffer_config)(u32 addr_lo, u32 addr_hi, int size);

	/* Configure the DBM with the BAM's data fifo */
	int (*data_fifo_config)(u8 dep_num, phys_addr_t addr,
			 u32 size, u8 dst_pipe_idx);

	/* Configure DBM speed : hs/ss */
	void (*set_speed)(bool speed);

	/* Enable DBM */
	void (*enable)(void);

	/* Reset a USB DBM ep */
	int (*ep_soft_reset)(u8 dbm_ep, bool enter_reset);

	/* Check whether the USB DBM requires ep reset after lpm suspend */
	bool (*reset_ep_after_lpm)(void);

	/*
	 * Indicates whether the DBM notifies the software about the need to
	 * come out of L1 state by interrupt
	 */
	bool (*l1_lpm_interrupt)(void);

};

struct dbm *usb_get_dbm_by_phandle(struct device *dev,
	const char *phandle, u8 index);
int usb_add_dbm(struct dbm *x);



#define CHECK_DBM_PTR_INT(dbm, func) do {				   \
	if (!(dbm) || !((dbm)->func)) {					   \
		pr_err("Can't call %s, dbp pointer == NULL\n", __func__);  \
		return -EPERM;						   \
	}								   \
} while (0)

#define CHECK_DBM_PTR_VOID(dbm, func) do {				   \
	if (!(dbm) || !((dbm)->func)) {					   \
		pr_err("Can't call %s, dbp pointer == NULL\n", __func__);  \
		return;							   \
	}								   \
} while (0)

#define CHECK_DBM_PTR_BOOL(dbm, func, ret) do {				   \
	if (!(dbm) || !((dbm)->func)) {					   \
		pr_err("Can't call %s, dbp pointer == NULL\n", __func__);  \
		return ret;						   \
	}								   \
} while (0)

static inline int dbm_soft_reset(struct dbm *dbm, bool enter_reset)
{
	CHECK_DBM_PTR_INT(dbm, soft_reset);
	return dbm->soft_reset(enter_reset);
}

static inline int dbm_ep_config(struct dbm  *dbm, u8 usb_ep, u8 bam_pipe,
			bool producer, bool disable_wb, bool internal_mem,
			bool ioc)
{
	CHECK_DBM_PTR_INT(dbm, ep_config);
	return dbm->ep_config(usb_ep, bam_pipe, producer, disable_wb,
				  internal_mem, ioc);

}

static inline int dbm_ep_unconfig(struct dbm *dbm, u8 usb_ep)
{
	CHECK_DBM_PTR_INT(dbm, ep_unconfig);
	return dbm->ep_unconfig(usb_ep);
}

static inline int dbm_get_num_of_eps_configured(struct dbm *dbm)
{
	CHECK_DBM_PTR_INT(dbm, get_num_of_eps_configured);
	return dbm->get_num_of_eps_configured();
}

static inline int dbm_event_buffer_config(struct dbm *dbm, u32 addr_lo,
					u32 addr_hi, int size)
{
	CHECK_DBM_PTR_INT(dbm, event_buffer_config);
	return dbm->event_buffer_config(addr_lo, addr_hi, size);
}

static inline int dbm_data_fifo_config(struct dbm *dbm, u8 dep_num,
				phys_addr_t addr, u32 size, u8 dst_pipe_idx)
{
	CHECK_DBM_PTR_INT(dbm, data_fifo_config);
	return dbm->data_fifo_config(dep_num, addr, size, dst_pipe_idx);
}

static inline void dbm_set_speed(struct dbm *dbm, bool speed)
{
	CHECK_DBM_PTR_VOID(dbm, set_speed);
	dbm->set_speed(speed);
}

static inline void dbm_enable(struct dbm *dbm)
{
	CHECK_DBM_PTR_VOID(dbm, enable);
	dbm->enable();
}

static inline int dbm_ep_soft_reset(struct dbm *dbm, u8 usb_ep,
					bool enter_reset)
{
	CHECK_DBM_PTR_INT(dbm, ep_soft_reset);
	return dbm->ep_soft_reset(usb_ep, enter_reset);
}

static inline bool dbm_reset_ep_after_lpm(struct dbm *dbm)
{
	/* Default (backward compatible) setting is false */
	CHECK_DBM_PTR_BOOL(dbm, reset_ep_after_lpm, false);
	return dbm->reset_ep_after_lpm();
}

static inline bool dbm_l1_lpm_interrupt(struct dbm *dbm)
{
	/* Default (backward compatible) setting is false */
	CHECK_DBM_PTR_BOOL(dbm, l1_lpm_interrupt, false);
	return dbm->l1_lpm_interrupt();
}

#endif /* __DBM_H */
