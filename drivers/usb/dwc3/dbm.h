/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2012-2015, 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __DBM_H
#define __DBM_H

#include <linux/device.h>
#include <linux/types.h>

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

struct dbm;

struct dbm *dwc3_init_dbm(struct device *dev, void __iomem *base);

int dbm_soft_reset(struct dbm *dbm, bool enter_reset);
int dbm_ep_config(struct dbm  *dbm, u8 usb_ep, u8 bam_pipe, bool producer,
			bool disable_wb, bool internal_mem, bool ioc);
int dbm_ep_unconfig(struct dbm *dbm, u8 usb_ep);
int dbm_get_num_of_eps_configured(struct dbm *dbm);
int dbm_event_buffer_config(struct dbm *dbm, u32 addr_lo, u32 addr_hi,
				int size);
int dbm_data_fifo_config(struct dbm *dbm, u8 dep_num, unsigned long addr,
				u32 size, u8 dst_pipe_idx);
int dwc3_dbm_disable_update_xfer(struct dbm *dbm, u8 usb_ep);
void dbm_set_speed(struct dbm *dbm, bool speed);
void dbm_enable(struct dbm *dbm);
int dbm_ep_soft_reset(struct dbm *dbm, u8 usb_ep, bool enter_reset);
bool dbm_reset_ep_after_lpm(struct dbm *dbm);
bool dbm_l1_lpm_interrupt(struct dbm *dbm);

#endif /* __DBM_H */
