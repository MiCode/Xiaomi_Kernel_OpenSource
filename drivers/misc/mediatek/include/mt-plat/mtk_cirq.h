/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CIRQ_SYS_H__
#define __CIRQ_SYS_H__

#define MT_EDGE_SENSITIVE 0
#define MT_LEVEL_SENSITIVE 1

/*
 * Define hardware register
 */
#define  CIRQ_STA_BASE         (SYS_CIRQ_BASE + 0x000)
#define  CIRQ_ACK_BASE         (SYS_CIRQ_BASE + 0x040)
#define  CIRQ_MASK_BASE        (SYS_CIRQ_BASE + 0x080)
#define  CIRQ_MASK_SET_BASE    (SYS_CIRQ_BASE + 0x0C0)
#define  CIRQ_MASK_CLR_BASE    (SYS_CIRQ_BASE + 0x100)
#define  CIRQ_SENS_BASE        (SYS_CIRQ_BASE + 0x140)
#define  CIRQ_SENS_SET_BASE    (SYS_CIRQ_BASE + 0x180)
#define  CIRQ_SENS_CLR_BASE    (SYS_CIRQ_BASE + 0x1C0)
#define  CIRQ_POL_BASE         (SYS_CIRQ_BASE + 0x200)
#define  CIRQ_POL_SET_BASE     (SYS_CIRQ_BASE + 0x240)
#define  CIRQ_POL_CLR_BASE     (SYS_CIRQ_BASE + 0x280)
#define  CIRQ_CON              (SYS_CIRQ_BASE + 0x300)

/*
 * Register placement
 */
#define  CIRQ_CON_EN_BITS           (0)
#define  CIRQ_CON_EDGE_ONLY_BITS    (1)
#define  CIRQ_CON_FLUSH_BITS        (2)
#define  CIRQ_CON_EVENT_BITS        (31)
#define  CIRQ_CON_BITS_MASK         (0x7)

/*
 * Register setting
 */
#define  CIRQ_CON_EN            (0x1)
#define  CIRQ_CON_EDGE_ONLY     (0x1)
#define  CIRQ_CON_FLUSH         (0x1)

/*
 * Define constant
 */
#define  CIRQ_CTRL_REG_NUM      ((CIRQ_IRQ_NUM + 31) / 32)

#define  MT_CIRQ_POL_NEG        (0)
#define  MT_CIRQ_POL_POS        (1)

/*
 * Define macro
 */
#define  IRQ_TO_CIRQ_NUM(irq)       \
	((irq) - (GIC_PRIVATE_SIGNALS + CIRQ_SPI_START))
#define  CIRQ_TO_IRQ_NUM(cirq)      \
	((cirq) + (GIC_PRIVATE_SIGNALS + CIRQ_SPI_START))

#define print_func() pr_debug("[CIRQ] in %s\n", __func__)


/* #define  __CHECK_IRQ_TYPE */

extern void __iomem *GIC_DIST_BASE;
extern void __iomem *GIC_CPU_BASE;
extern void __iomem *INT_POL_CTL0;

#ifndef GIC_PRIVATE_SIGNALS
#define GIC_PRIVATE_SIGNALS     (32)
#endif

/*
 * Define function prototypes.
 */
void mt_cirq_enable(void);
void mt_cirq_disable(void);
void mt_cirq_clone_gic(void);
void mt_cirq_sw_reset(void);
void mt_cirq_flush(void);
int mt_cirq_test(void);
void mt_cirq_dump_reg(void);
void set_wakeup_sources(u32 *list, u32 num_events);

#endif  /*!__CIRQ_H__ */
