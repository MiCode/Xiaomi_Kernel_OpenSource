/*
 * arch/arm/mach-tegra/sleep.h
 *
 * Declarations for power state transition code
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_SLEEP_H
#define __MACH_TEGRA_SLEEP_H

#include <mach/iomap.h>

#ifndef CONFIG_TRUSTED_FOUNDATIONS
/* FIXME: The code associated with this should be removed if our change to
   save the diagnostic regsiter in the CPU context is accepted. */
#define USE_TEGRA_DIAG_REG_SAVE	1
#else
#define USE_TEGRA_DIAG_REG_SAVE	0
#endif

#define TEGRA_POWER_LP1_AUDIO		(1 << 25) /* do not turn off pll-p in LP1 */

#ifdef CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE

/* Power gate CRAIL partition */
#define TEGRA_POWER_CLUSTER_PART_CRAIL	(1 << 25)

/* Power gate CxNC partition */
#define TEGRA_POWER_CLUSTER_PART_NONCPU	(1 << 24)

#define TEGRA_POWER_CLUSTER_PART_MASK	(TEGRA_POWER_CLUSTER_PART_CRAIL | \
						TEGRA_POWER_CLUSTER_PART_NONCPU)
#define TEGRA_POWER_CLUSTER_PART_DEFAULT TEGRA_POWER_CLUSTER_PART_CRAIL
#else
#define TEGRA_POWER_CLUSTER_PART_DEFAULT 0
#endif
#define TEGRA_POWER_CLUSTER_PART_SHIFT	24
#define TEGRA_POWER_CLUSTER_FORCE_SHIFT	2
#define TEGRA_POWER_CLUSTER_FORCE_MASK	(1 << TEGRA_POWER_CLUSTER_FORCE_SHIFT)

#define TEGRA_POWER_SDRAM_SELFREFRESH	(1 << 26) /* SDRAM is in self-refresh */
#define TEGRA_POWER_HOTPLUG_SHUTDOWN	(1 << 27) /* Hotplug shutdown */
#define TEGRA_POWER_CLUSTER_G		(1 << 28) /* G CPU */
#define TEGRA_POWER_CLUSTER_LP		(1 << 29) /* LP CPU */
#define TEGRA_POWER_CLUSTER_MASK	(TEGRA_POWER_CLUSTER_G | \
						TEGRA_POWER_CLUSTER_LP)
#define TEGRA_POWER_CLUSTER_IMMEDIATE	(1 << 30) /* Immediate wake */
#define TEGRA_POWER_CLUSTER_FORCE	(1 << 31) /* Force switch */

#define TEGRA_IRAM_CODE_AREA		(TEGRA_IRAM_BASE + SZ_4K)

/* PMC_SCRATCH2 is used for PLLM boot state if PLLM auto-restart is enabled */
#define PMC_SCRATCH2			0x58
/* PMC_SCRATCH37-39 and 41 are used for tegra_pen_lock in Tegra2 idle */
#define PMC_SCRATCH37                   0x130
#define PMC_SCRATCH38                   0x134
/* PMC_SCRATCH39 stores the reset vector of the AVP (always 0) after LP0 */
#define PMC_SCRATCH39                   0x138
/* PMC_SCRATCH41 stores the reset vector of the CPU after LP0 and LP1 */
#define PMC_SCRATCH41                   0x140

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define CPU_RESETTABLE			2
#define CPU_RESETTABLE_SOON		1
#define CPU_NOT_RESETTABLE		0
#endif

#define FLOW_CTRL_HALT_CPU0_EVENTS	0x0
#define   FLOW_CTRL_WAITEVENT		(2 << 29)
#define   FLOW_CTRL_WAIT_FOR_INTERRUPT	(4 << 29)
#define   FLOW_CTRL_JTAG_RESUME		(1 << 28)
#define   FLOW_CTRL_HALT_CPU_IRQ	(1 << 10)
#define   FLOW_CTRL_HALT_CPU_FIQ	(1 << 8)
#define   FLOW_CTRL_HALT_LIC_IRQ	(1 << 11)
#define   FLOW_CTRL_HALT_LIC_FIQ	(1 << 10)
#define   FLOW_CTRL_HALT_GIC_IRQ	(1 << 9)
#define   FLOW_CTRL_HALT_GIC_FIQ	(1 << 8)
#define   FLOW_CTRL_IMMEDIATE_WAKE	(1 << 3)
#define FLOW_CTRL_CPU0_CSR		0x8
#define   FLOW_CTRL_CSR_INTR_FLAG		(1 << 15)
#define   FLOW_CTRL_CSR_EVENT_FLAG		(1 << 14)
#define   FLOW_CTRL_CSR_ENABLE_EXT_NONE	(0)
#define   FLOW_CTRL_CSR_ENABLE_EXT_CRAIL	(1<<13)
#define   FLOW_CTRL_CSR_ENABLE_EXT_NCPU	(1<<12)
#define   FLOW_CTRL_CSR_ENABLE_EXT_MASK	( \
					FLOW_CTRL_CSR_ENABLE_EXT_NCPU | \
					FLOW_CTRL_CSR_ENABLE_EXT_CRAIL )
#define   FLOW_CTRL_CSR_ENABLE_EXT_EMU FLOW_CTRL_CSR_ENABLE_EXT_MASK
#define   FLOW_CTRL_CSR_IMMEDIATE_WAKE		(1<<3)
#define   FLOW_CTRL_CSR_SWITCH_CLUSTER		(1<<2)
#define   FLOW_CTRL_CSR_ENABLE			(1 << 0)

#define FLOW_CTRL_HALT_CPU1_EVENTS	0x14
#define FLOW_CTRL_CPU1_CSR		0x18

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define FLOW_CTRL_CSR_WFE_CPU0		(1 << 4)
#define FLOW_CTRL_CSR_WFE_BITMAP	(3 << 4)
#define FLOW_CTRL_CSR_WFI_BITMAP	0
#else
#define FLOW_CTRL_CSR_WFE_BITMAP	(0xF << 4)
#define FLOW_CTRL_CSR_WFI_CPU0		(1 << 8)
#define FLOW_CTRL_CSR_WFI_BITMAP	(0xF << 8)
#endif

#ifdef CONFIG_CACHE_L2X0
#define TEGRA_PL310_VIRT (TEGRA_ARM_PL310_BASE - IO_CPU_PHYS + IO_CPU_VIRT)
#endif
#ifdef CONFIG_HAVE_ARM_SCU
#define TEGRA_ARM_PERIF_VIRT (TEGRA_ARM_PERIF_BASE - IO_CPU_PHYS + IO_CPU_VIRT)
#endif
#define TEGRA_FLOW_CTRL_VIRT (TEGRA_FLOW_CTRL_BASE - IO_PPSB_PHYS + IO_PPSB_VIRT)
#define TEGRA_CLK_RESET_VIRT (TEGRA_CLK_RESET_BASE - IO_PPSB_PHYS + IO_PPSB_VIRT)

#ifdef __ASSEMBLY__

/* Macro to exit SMP coherency. */
.macro exit_smp, tmp1, tmp2
	mrc	p15, 0, \tmp1, c1, c0, 1	@ ACTLR
	bic	\tmp1, \tmp1, #(1<<6) | (1<<0)	@ clear ACTLR.SMP | ACTLR.FW
#ifdef CONFIG_ARM_ERRATA_799270
	ldr	\tmp2, =TEGRA_CLK_RESET_VIRT
	ldr	\tmp2, [\tmp2, #0x70]		@ BOND_OUT_L
	and	\tmp2, \tmp2, #0
	orr	\tmp1, \tmp1, \tmp2
#endif
	mcr	p15, 0, \tmp1, c1, c0, 1	@ ACTLR
	isb
	dsb
#ifdef CONFIG_HAVE_ARM_SCU
	cpu_id	\tmp1
	mov	\tmp1, \tmp1, lsl #2
	mov	\tmp2, #0xf
	mov	\tmp2, \tmp2, lsl \tmp1
	mov32	\tmp1, TEGRA_ARM_PERIF_VIRT + 0xC
	str	\tmp2, [\tmp1]			@ invalidate SCU tags for CPU
	dsb
#endif
.endm

#define DEBUG_CONTEXT_STACK	0

/* pops a debug check token from the stack */
.macro	pop_stack_token tmp1, tmp2
#if DEBUG_CONTEXT_STACK
	mov32	\tmp1, 0xBAB1F00D
	ldmfd	sp!, {\tmp2}
	cmp	\tmp1, \tmp2
	movne	pc, #0
#endif
.endm

/* pushes a debug check token onto the stack */
.macro	push_stack_token tmp1
#if DEBUG_CONTEXT_STACK
	mov32	\tmp1, 0xBAB1F00D
	stmfd	sp!, {\tmp1}
#endif
.endm

#else	/* !defined(__ASSEMBLY__) */

#define FLOW_CTRL_HALT_CPU(cpu)	(IO_ADDRESS(TEGRA_FLOW_CTRL_BASE) +	\
	((cpu) ? (FLOW_CTRL_HALT_CPU1_EVENTS + 8 * ((cpu) - 1)) :	\
	 FLOW_CTRL_HALT_CPU0_EVENTS))

#define FLOW_CTRL_CPU_CSR(cpu)	(IO_ADDRESS(TEGRA_FLOW_CTRL_BASE) +	\
	((cpu) ? (FLOW_CTRL_CPU1_CSR + 8 * ((cpu) - 1)) :	\
	 FLOW_CTRL_CPU0_CSR))

static inline void flowctrl_writel(unsigned long val, void __iomem *addr)
{
	writel(val, addr);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	wmb();
#endif
	(void)__raw_readl(addr);
}

void tegra_pen_lock(void);
void tegra_pen_unlock(void);
int tegra_sleep_cpu_finish(unsigned long v2p);
void tegra_resume(void);
void tegra_flush_l1_cache(void);
void tegra_flush_cache(void);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
extern unsigned int tegra2_iram_start;
extern unsigned int tegra2_iram_end;
int  tegra2_cpu_is_resettable_soon(void);
void tegra2_cpu_reset(int cpu);
void tegra2_cpu_set_resettable_soon(void);
void tegra2_cpu_clear_resettable(void);
int tegra2_sleep_core_finish(unsigned long int);
void tegra2_hotplug_shutdown(void);
void tegra2_sleep_wfi(unsigned long v2p);
int tegra2_finish_sleep_cpu_secondary(unsigned long int);
#else
extern unsigned int tegra3_iram_start;
extern unsigned int tegra3_iram_end;
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
extern unsigned int lp1_register_pmuslave_addr;
extern unsigned int lp1_register_i2c_base_addr;
extern unsigned int lp1_register_core_lowvolt;
extern unsigned int lp1_register_core_highvolt;
#endif
int tegra3_sleep_core_finish(unsigned long int);
int tegra3_sleep_cpu_secondary_finish(unsigned long int);
void tegra3_hotplug_shutdown(void);
#endif

#ifdef CONFIG_TRUSTED_FOUNDATIONS
extern unsigned long tegra_resume_timestamps_start;
extern unsigned long tegra_resume_timestamps_end;
#ifndef CONFIG_ARCH_TEGRA_11x_SOC
extern unsigned long tegra_resume_smc_entry_time;
extern unsigned long tegra_resume_smc_exit_time;
#endif
extern unsigned long tegra_resume_entry_time;
#endif

static inline void *tegra_iram_start(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return &tegra2_iram_start;
#else
	return &tegra3_iram_start;
#endif
}

static inline void *tegra_iram_end(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return &tegra2_iram_end;
#else
	return &tegra3_iram_end;
#endif
}

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
static inline void *tegra_lp1_register_pmuslave_addr(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return NULL;
#else
	return &lp1_register_pmuslave_addr;
#endif
}

static inline void *tegra_lp1_register_i2c_base_addr(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return NULL;
#else
	return &lp1_register_i2c_base_addr;
#endif
}

static inline void *tegra_lp1_register_core_lowvolt(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return NULL;
#else
	return &lp1_register_core_lowvolt;
#endif
}

static inline void *tegra_lp1_register_core_highvolt(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return NULL;
#else
	return &lp1_register_core_highvolt;
#endif
}
#endif /* For CONFIG_TEGRA_LP1_LOW_COREVOLTAGE */
#endif
#endif
