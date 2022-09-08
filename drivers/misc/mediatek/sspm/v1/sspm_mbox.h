/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __SSPM_MBOX_H__
#define __SSPM_MBOX_H__

#include <linux/platform_device.h>

enum SSPM_MBOX_SIZE {
	SSPM_MBOX_4BYTE = 0x80,
	SSPM_MBOX_8BYTE = 0x100,
};

extern struct platform_device *sspm_pdev;

typedef unsigned int (*sspm_ipi_isr)(unsigned int mbox, void __iomem *base,
	unsigned int irq);

unsigned int sspm_mbox_size(int mbox);
extern int sspm_mbox_send(unsigned int mbox, unsigned int slot,
	unsigned int irq, void *data, unsigned int len);
extern uint32_t *sspm_mbox_addr(unsigned int mbox, unsigned int slot);
extern int sspm_mbox_read(unsigned int mbox, unsigned int slot, void *data,
	unsigned int len);
extern int sspm_mbox_write(unsigned int mbox, unsigned int slot, void *data,
	unsigned int len);
extern unsigned int sspm_mbox_init(unsigned int mode, unsigned int count,
	sspm_ipi_isr ipi_isr_cb);
extern int sspm_mbox_polling(unsigned int mbox, unsigned int irq,
	unsigned int slot, unsigned int *retdata, unsigned int retlen,
	unsigned int retries);

#endif
