/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __CIRQ_SYS_H__
#define __CIRQ_SYS_H__

#define MT_EDGE_SENSITIVE 0
#define MT_LEVEL_SENSITIVE 1

#if !defined(CONFIG_OF)
#define SYS_CIRQ_BASE (0xF0202000)
#endif

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
#define  CIRQ_CON_SW_RST_BITS       (20)
#define  CIRQ_CON_EVENT_BITS        (31)
#define  CIRQ_CON_BITS_MASK         (0x7)

/*
 * Register setting
 */
#define  CIRQ_CON_EN            (0x1)
#define  CIRQ_CON_EDGE_ONLY     (0x1)
#define  CIRQ_CON_FLUSH         (0x1)
#define  CIRQ_SW_RESET          (0x1)

/*
 * Define constant
 */
#define  CIRQ_CTRL_REG_NUM      ((CIRQ_IRQ_NUM + 31) / 32)

#define  MT_CIRQ_POL_NEG        (0)
#define  MT_CIRQ_POL_POS        (1)

/*
 * Define macro
 */
#define IRQ_TO_CIRQ_NUM(irq)  ((irq) - (GIC_PRIVATE_SIGNALS + CIRQ_SPI_START))
#define CIRQ_TO_IRQ_NUM(cirq) ((cirq) + (GIC_PRIVATE_SIGNALS + CIRQ_SPI_START))

#define print_func() pr_debug("[CIRQ] in %s\n", __func__)


/* #define  __CHECK_IRQ_TYPE */

extern void __iomem *GIC_DIST_BASE;
extern void __iomem *GIC_CPU_BASE;
extern void __iomem *INT_POL_CTL0;

#ifndef GIC_PRIVATE_SIGNALS
#define GIC_PRIVATE_SIGNALS     (32)
#endif

/* GIC sensitive */
#define SENS_EDGE	(0x2)
#define SENS_LEVEL	(0x0)

/*
 * Define function prototypes.
 */
void mt_cirq_enable(void);
void mt_cirq_disable(void);
void mt_cirq_clone_gic(void);
void mt_cirq_flush(void);
int mt_cirq_test(void);
void mt_cirq_dump_reg(void);
#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
static void cirq_fast_sw_flush(void);
struct cirq_reg {
	unsigned int reg_num;
	unsigned int used;
	unsigned long mask;
	unsigned int pol;
	unsigned int sen;
	unsigned long pending;
	struct list_head the_link;
};

struct cirq_events {
	unsigned int num_reg;
	unsigned int spi_start;
	unsigned int num_of_events;
	unsigned int *wakeup_events;
	struct cirq_reg *table;
	void __iomem *dist_base;
	void __iomem *cirq_base;
	struct list_head used_reg_head;
};

extern unsigned int mt_irq_get_sens(unsigned int irq);

/*#define FAST_CIRQ_DEBUG*/
/*#define LATENCY_CHECK*/
#ifdef FAST_CIRQ_DEBUG
void debug_setting_dump(void);
#endif
#endif
#endif  /*!__CIRQ_H__ */
