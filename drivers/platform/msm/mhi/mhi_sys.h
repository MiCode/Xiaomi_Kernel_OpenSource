/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _H_MHI_SYS_
#define _H_MHI_SYS_

#include <linux/mutex.h>
#include <linux/ipc_logging.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/device.h>

#include "mhi.h"

extern enum MHI_DEBUG_LEVEL mhi_msg_lvl;
extern enum MHI_DEBUG_LEVEL mhi_ipc_log_lvl;
extern unsigned int mhi_log_override;
extern u32 m3_timer_val_ms;

extern enum MHI_DEBUG_LEVEL mhi_xfer_db_interval;
extern enum MHI_DEBUG_LEVEL tx_mhi_intmodt;
extern enum MHI_DEBUG_LEVEL rx_mhi_intmodt;
extern void *mhi_ipc_log;

#define MHI_ASSERT(_x, _msg)\
	do {\
		if (!(_x)) {\
			pr_err("ASSERT- %s : Failure in %s:%d/%s()!\n",\
				_msg, __FILE__, __LINE__, __func__); \
			panic("ASSERT"); \
		} \
	} while (0)

#define mhi_log(_msg_lvl, _msg, ...) do { \
		DEFINE_DYNAMIC_DEBUG_METADATA(descriptor, _msg);	 \
		if ((mhi_log_override ||				 \
		    unlikely(descriptor.flags & _DPRINTK_FLAGS_PRINT)) &&\
			(_msg_lvl) >= mhi_msg_lvl)			 \
			pr_alert("[%s] " _msg, __func__, ##__VA_ARGS__); \
		if ((mhi_log_override ||				      \
			unlikely(descriptor.flags & _DPRINTK_FLAGS_PRINT)) && \
			mhi_ipc_log && ((_msg_lvl) >= mhi_ipc_log_lvl))  \
			ipc_log_string(mhi_ipc_log,			 \
				"[%s] " _msg, __func__, ##__VA_ARGS__);  \
} while (0)

irqreturn_t mhi_msi_handlr(int msi_number, void *dev_id);

struct mhi_meminfo {
	struct device *dev;
	uintptr_t pa_aligned;
	uintptr_t pa_unaligned;
	uintptr_t va_aligned;
	uintptr_t va_unaligned;
	uintptr_t size;
};

enum MHI_STATUS mhi_mallocmemregion(struct mhi_meminfo *meminfo, size_t size);

uintptr_t mhi_get_phy_addr(struct mhi_meminfo *meminfo);
void *mhi_get_virt_addr(struct mhi_meminfo *meminfo);
uintptr_t mhi_p2v_addr(struct mhi_meminfo *meminfo, phys_addr_t pa);
phys_addr_t mhi_v2p_addr(struct mhi_meminfo *meminfo, uintptr_t va);
u64 mhi_get_memregion_len(struct mhi_meminfo *meminfo);
void mhi_freememregion(struct mhi_meminfo *meminfo);

void print_ring(struct mhi_ring *local_chan_ctxt, u32 ring_id);
int mhi_init_debugfs(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_probe(struct pci_dev *mhi_device,
		const struct pci_device_id *mhi_device_id);
ssize_t sysfs_init_m3(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);
ssize_t sysfs_init_m0(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);
ssize_t sysfs_init_mhi_reset(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);

#endif
