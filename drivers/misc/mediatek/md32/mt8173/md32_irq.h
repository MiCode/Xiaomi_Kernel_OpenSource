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

#ifndef __MD32_IRQ_H__
#define __MD32_IRQ_H__
#include <linux/interrupt.h>

#define MD32_PTCM_SIZE		0x10000	/* 64KB */
#define MD32_DTCM_SIZE		0x10000	/* 64KB */
#define MD32_DTCM_OFFSET	0x18000

#define MD32_BASE		(md32reg.cfg)
#define MD32_TO_HOST_ADDR	(md32reg.cfg + 0x001C)
#define MD32_TO_HOST_REG	MD32_TO_HOST_ADDR
#define MD32_TO_SPM_REG		(md32reg.cfg + 0x0020)
#define HOST_TO_MD32_REG	(md32reg.cfg + 0x0024)
#define MD32_DEBUG_PC_REG	(md32reg.cfg + 0x0060)
#define MD32_DEBUG_R14_REG	(md32reg.cfg + 0x0064)
#define MD32_DEBUG_R15_REG	(md32reg.cfg + 0x0068)
#define MD32_WDT_REG		(md32reg.cfg + 0x0084)

#define MD32_PTCM		(md32reg.sram)
#define MD32_DTCM		(md32reg.sram + MD32_DTCM_OFFSET)

struct md32_regs {
	void __iomem *sram;
	void __iomem *cfg;
	int irq;
};

void md32_ipi_handler(void);
void md32_ipi_init(void);
void md32_irq_init(void);
irqreturn_t md32_irq_handler(int irq, void *dev_id);
void md32_ocd_init(void);
ssize_t md32_get_log_buf(unsigned char *md32_log_buf, size_t b_len);
int reboot_load_md32(void);
void memcpy_from_md32(void *trg, const void __iomem *src, int size);

extern struct device_attribute dev_attr_md32_ocd;
extern unsigned long md32_log_buf_addr;
extern unsigned long md32_log_start_idx_addr;
extern unsigned long md32_log_end_idx_addr;
extern unsigned long md32_log_buf_len_addr;

#endif	/* __MD32_IRQ_H__ */
