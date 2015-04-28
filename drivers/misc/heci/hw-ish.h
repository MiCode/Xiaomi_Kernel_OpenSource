/*
 * H/W layer of HECI provider device (ISS)
 *
 * Copyright (c) 2014-2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _HECI_HW_ISH_H_
#define _HECI_HW_ISH_H_

#include <linux/pci.h>
#include "hw-ish-regs.h"
#include "heci_dev.h"

extern int	suspend_flag;
extern wait_queue_head_t	suspend_wait;

struct ipc_rst_payload_type {
	u16            reset_id;
	u16            reserved;
};

struct ish_hw {
	void __iomem *mem_addr;
};

#define to_ish_hw(dev) (struct ish_hw *)((dev)->hw)


struct heci_device *ish_dev_init(struct pci_dev *pdev);

irqreturn_t ish_irq_handler(int irq, void *dev_id);

void ish_clr_host_rdy(struct heci_device *dev);
void ish_set_host_rdy(struct heci_device *dev);
bool ish_hw_is_ready(struct heci_device *dev);
void ish_intr_enable(struct heci_device *dev);
void ish_intr_disable(struct heci_device *dev);

int	write_ipc_from_queue(struct heci_device *dev);

static int	ipc_send_mng_msg(struct heci_device *dev, uint32_t msg_code,
	void *msg, size_t size);

static int	ipc_send_heci_msg(struct heci_device *dev,
	struct heci_msg_hdr *hdr, void *msg, void(*ipc_send_compl)(void *),
	void *ipc_send_compl_prm);

static u32	ish_read_hdr(const struct heci_device *dev);

void g_ish_print_log(char *format, ...);

#endif /* _HECI_HW_ISH_H_ */

