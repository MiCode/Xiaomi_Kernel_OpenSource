/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#if !defined(_GSI_EMULATION_H_)
# define _GSI_EMULATION_H_

# include <linux/interrupt.h>

# include "gsi.h"
# include "gsi_reg.h"

#if defined(CONFIG_IPA_EMULATION)
# include "gsi_emulation_stubs.h"
#endif

# define gsi_emu_readl(c)     (readl_relaxed(c))
# define gsi_emu_writel(v, c) ({ __iowmb(); writel_relaxed((v), (c)); })

# define CNTRLR_BASE 0

/*
 * The following file contains definitions and declarations that are
 * germane only to the IPA emulation system, which is run from an X86
 * environment.  Declaration's for non-X86 (ie. arm) are merely stubs
 * to facilitate compile and link.
 *
 * Interrupt controller registers.
 * Descriptions taken from the EMULATION interrupt controller SWI.
 * - There is only one Master Enable register
 * - Each group of 32 interrupt lines (range) is controlled by 8 registers,
 *   which are consecutive in memory:
 *      GE_INT_ENABLE_n
 *      GE_INT_ENABLE_CLEAR_n
 *      GE_INT_ENABLE_SET_n
 *      GE_INT_TYPE_n
 *      GE_IRQ_STATUS_n
 *      GE_RAW_STATUS_n
 *      GE_INT_CLEAR_n
 *      GE_SOFT_INT_n
 * - After the above 8 registers, there are the registers of the next
 *   group (range) of 32 interrupt lines, and so on.
 */

/** @brief The interrupt controller version and interrupt count register.
 *         Specifies interrupt controller version (upper 16 bits) and the
 *         number of interrupt lines supported by HW (lower 16 bits).
 */
# define GE_INT_CTL_VER_CNT              \
	(CNTRLR_BASE + 0x0000)

/** @brief Enable or disable physical IRQ output signal to the system,
 *         not affecting any status registers.
 *
 *         0x0 : DISABLE IRQ output disabled
 *         0x1 : ENABLE  IRQ output enabled
 */
# define GE_INT_OUT_ENABLE               \
	(CNTRLR_BASE + 0x0004)

/** @brief The IRQ master enable register.
 *         Bit #0: IRQ_ENABLE, set 0 to disable, 1 to enable.
 */
# define GE_INT_MASTER_ENABLE            \
	(CNTRLR_BASE + 0x0008)

# define GE_INT_MASTER_STATUS            \
	(CNTRLR_BASE + 0x000C)

/** @brief Each bit disables (bit=0, default) or enables (bit=1) the
 *         corresponding interrupt source
 */
# define GE_INT_ENABLE_n(n)              \
	(CNTRLR_BASE + 0x0010 + 0x20 * (n))

/** @brief Write bit=1 to clear (to 0) the corresponding bit(s) in INT_ENABLE.
 *         Does nothing for bit=0
 */
# define GE_INT_ENABLE_CLEAR_n(n)        \
	(CNTRLR_BASE + 0x0014 + 0x20 * (n))

/** @brief Write bit=1 to set (to 1) the corresponding bit(s) in INT_ENABLE.
 *         Does nothing for bit=0
 */
# define GE_INT_ENABLE_SET_n(n)          \
	(CNTRLR_BASE + 0x0018 + 0x20 * (n))

/** @brief Select level (bit=0, default) or edge (bit=1) sensitive input
 *         detection logic for each corresponding interrupt source
 */
# define GE_INT_TYPE_n(n)                \
	(CNTRLR_BASE + 0x001C + 0x20 * (n))

/** @brief Shows the interrupt sources captured in RAW_STATUS that have been
 *         steered to irq_n by INT_SELECT. Interrupts must also be enabled by
 *         INT_ENABLE and MASTER_ENABLE. Read only register.
 *         Bit values: 1=active, 0=inactive
 */
# define GE_IRQ_STATUS_n(n)                      \
	(CNTRLR_BASE + 0x0020 + 0x20 * (n))

/** @brief Shows the interrupt sources that have been latched by the input
 *         logic of the Interrupt Controller. Read only register.
 *         Bit values: 1=active, 0=inactive
 */
# define GE_RAW_STATUS_n(n)                      \
	(CNTRLR_BASE + 0x0024 + 0x20 * (n))

/** @brief Write bit=1 to clear the corresponding bit(s) in RAW_STATUS.
 *         Does nothing for bit=0
 */
# define GE_INT_CLEAR_n(n)               \
	(CNTRLR_BASE + 0x0028 + 0x20 * (n))

/** @brief Write bit=1 to set the corresponding bit(s) in RAW_STATUS.
 *         Does nothing for bit=0.
 *  @note  Only functional for edge detected interrupts
 */
# define GE_SOFT_INT_n(n)                        \
	(CNTRLR_BASE + 0x002C + 0x20 * (n))

/** @brief Maximal number of ranges in SW. Each range supports 32 interrupt
 *         lines. If HW is extended considerably, increase this value
 */
# define DEO_IC_MAX_RANGE_CNT            8

/** @brief Size of the registers of one range in memory, in bytes */
# define DEO_IC_RANGE_MEM_SIZE           32  /* SWI: 8 registers, no gaps */

/** @brief Minimal Interrupt controller HW version */
# define DEO_IC_INT_CTL_VER_MIN          0x0102


#if defined(CONFIG_IPA_EMULATION) /* declarations to follow */

/*
 * *****************************************************************************
 * The following used to set up the EMULATION interrupt controller...
 * *****************************************************************************
 */
int setup_emulator_cntrlr(
	void __iomem *intcntrlr_base,
	u32           intcntrlr_mem_size);

/*
 * *****************************************************************************
 * The following for EMULATION hard irq...
 * *****************************************************************************
 */
irqreturn_t emulator_hard_irq_isr(
	int   irq,
	void *ctxt);

/*
 * *****************************************************************************
 * The following for EMULATION soft irq...
 * *****************************************************************************
 */
irqreturn_t emulator_soft_irq_isr(
	int   irq,
	void *ctxt);

# else /* #if !defined(CONFIG_IPA_EMULATION) then definitions to follow */

static inline int setup_emulator_cntrlr(
	void __iomem *intcntrlr_base,
	u32           intcntrlr_mem_size)
{
	return 0;
}

static inline irqreturn_t emulator_hard_irq_isr(
	int   irq,
	void *ctxt)
{
	return IRQ_NONE;
}

static inline irqreturn_t emulator_soft_irq_isr(
	int   irq,
	void *ctxt)
{
	return IRQ_HANDLED;
}

# endif /* #if defined(CONFIG_IPA_EMULATION) */

#endif /* #if !defined(_GSI_EMULATION_H_) */
