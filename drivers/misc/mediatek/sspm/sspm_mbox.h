/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
