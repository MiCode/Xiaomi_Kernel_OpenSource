/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/const.h>
#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/cache.h>
#include <asm/domain.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/glue-cache.h>
#include <asm/glue-proc.h>
#include <asm/cp15.h>

#include "sleep.h"
#include "clock.h"
#include "reset.h"

#define EMC_CFG				0xc
#define EMC_ADR_CFG			0x10
#define EMC_TIMING_CONTROL		0x28
#define EMC_REFRESH			0x70
#define EMC_NOP				0xdc
#define EMC_SELF_REF			0xe0
#define EMC_MRW				0xe8
#define EMC_REQ_CTRL			0x2b0
#define EMC_EMC_STATUS			0x2b4
#define EMC_FBIO_CFG5			0x104
#define EMC_AUTO_CAL_CONFIG		0x2a4
#define EMC_AUTO_CAL_INTERVAL		0x2a8
#define EMC_AUTO_CAL_STATUS		0x2ac
#define EMC_CFG_DIG_DLL			0x2bc
#define EMC_ZCAL_INTERVAL		0x2e0
#define EMC_ZQ_CAL			0x2ec
#define EMC_XM2VTTGENPADCTRL		0x310
#define EMC_XM2VTTGENPADCTRL2		0x314

#define PMC_CTRL			0x0
#define PMC_CTRL_SIDE_EFFECT_LP0	(1 << 14)  /* enter LP0 when CPU pwr gated */

#define PMC_PWRGATE_TOGGLE		0x30
#define PMC_REMOVE_CLAMPING_CMD		0x34
#define PMC_PWRGATE_STATUS		0x38

#define PMC_PWRGATE_PARTID_L2C		(0x5)

#define PMC_IO_DPD_REQ			0x1b8
#define PMC_IO_DPD_STATUS		0x1bc
#define PMC_IO_DPD2_REQ			0x1c0
#define PMC_IO_DPD2_REQ_CODE_DPD_OFF	(1 << 30)
#define PMC_IO_DPD2_REQ_DISC_BIAS	(1 << 27)
#define PMC_SCRATCH1_ECO		0x264
#define PMC_POR_DPD_CTRL		0x264
#define PMC_IO_DPD3_REQ			0x45c
#define PMC_IO_DPD3_STATUS		0x460

#define EMC_SEL_DPD_CTRL		0x3d8

#define PMC_POR_DPD_CTRL		0x264

#define FLOW_IPC_STS			0x500
#define FLOW_IPC_STS_AP2BB_MSC_STS_0	(1 << 4)

#define CLK_RESET_CCLK_BURST		0x20
#define CCLK_BURST_PLLX_DIV2_BYPASS_LP	(1<<16)
#define CLK_RESET_CCLK_DIVIDER		0x24
#define CLK_RESET_SCLK_BURST		0x28
#define CLK_RESET_SCLK_DIVIDER		0x2c

#define CLK_RESET_PLLC_BASE		0x80
#define CLK_RESET_PLLM_BASE		0x90
#define CLK_RESET_PLLP_BASE		0xa0
#define CLK_RESET_PLLA_BASE		0xb0
#define CLK_RESET_PLLX_BASE		0xe0

#define CLK_RESET_PLLP_RESHIFT		0x528
#define CLK_RESET_PLLP_RESHIFT_DEFAULT	0x3b
#define CLK_RESET_PLLP_RESHIFT_ENABLE	0x3

#define CLK_RESET_PLLC_MISC		0x8c
#define CLK_RESET_PLLM_MISC		0x9c
#define CLK_RESET_PLLP_MISC		0xac
#define CLK_RESET_PLLA_MISC		0xbc
#define CLK_RESET_PLLX_MISC		0xe4
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define CLK_RESET_PLLX_MISC3		0x518
#define CLK_RESET_PLLM_MISC_IDDQ	5
#define CLK_RESET_PLLC_MISC_IDDQ	26
#define CLK_RESET_PLLX_MISC3_IDDQ	3
#endif

#define CLK_RESET_PLLP_OUTA		0xa4
#define CLK_RESET_PLLP_OUTB		0xa8

#define PMC_PLLP_WB0_OVERRIDE		0xf8
#define PMC_PLLM_WB0_OVERRIDE		0x1dc

#define CLK_RESET_CLK_SOURCE_EMC	0x19c
#define CLK_RESET_CLK_SOURCE_MSELECT	0x3b4
#define CLK_RESET_CLK_ENB_H_SET		0x328
#define CLK_RESET_CLK_ENB_H_CLR		0x32c
#define CLK_RESET_CLK_RST_DEV_H_SET		0x308
#define CLK_RESET_CLK_RST_DEV_H_CLR		0x30c
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define CLK_RESET_CLK_ENB_W_SET		0x448
#define CLK_RESET_CLK_ENB_W_CLR		0x44c
#endif

#define I2C_CNFG	0x0
#define I2C_ADDR0	0x4
#define I2C_DATA1	0xc
#define I2C_DATA2	0x10
#define I2C_STATUS	0x1c

#define MSELECT_CLKM			(0x3 << 30)

#define FLOW_CONTROL_CLUSTER_CONTROL	0x2c

#define TEGRA_RTC_MSEC	0x10

#if USE_PLL_LOCK_BITS
#define LOCK_DELAY 		PLL_POST_LOCK_DELAY
#else
#define LOCK_DELAY 		0xff /* 255uS delay for PLL stabilization */
#endif

#define USE_PLLP_ON_SLEEP_ENTRY 0

#define TEGRA30_POWER_HOTPLUG_SHUTDOWN	(1 << 27) /* Hotplug shutdown */

.macro emc_device_mask, rd, base
	ldr	\rd, [\base, #EMC_ADR_CFG]
	tst     \rd, #0x1
	moveq	\rd, #(0x1<<8)		@ just 1 device
	movne	\rd, #(0x3<<8)		@ 2 devices
.endm

.macro emc_timing_update, rd, base
	mov	\rd, #1
	str	\rd, [\base, #EMC_TIMING_CONTROL]
1001:
	ldr	\rd, [\base, #EMC_EMC_STATUS]
	tst	\rd, #(0x1<<23)		@ wait until EMC_STATUS_TIMING_UPDATE_STALLED is clear
	bne	1001b
.endm

#if defined(CONFIG_HOTPLUG_CPU) || defined(CONFIG_PM_SLEEP)
/*
 * tegra30_hotplug_shutdown(void)
 *
 * Powergates the current CPU.
 * Should never return.
 */
ENTRY(tegra30_hotplug_shutdown)
	mov	r6, lr
	bl	tegra_cpu_exit_coherency

	/* Powergate this CPU */
	mov	r0, #TEGRA30_POWER_HOTPLUG_SHUTDOWN
	bl	tegra30_cpu_shutdown
	mov	pc, r6			@ should never get here
ENDPROC(tegra30_hotplug_shutdown)

/*
 * tegra30_cpu_shutdown(unsigned long flags)
 *
 * Puts the current CPU in wait-for-event mode on the flow controller
 * and powergates it -- flags (in R0) indicate the request type.
 * Must never be called for CPU 0.
 *
 * corrupts r0-r4, r12
 */
ENTRY(tegra30_cpu_shutdown)
	cpu_id	r3
#ifndef CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE
	cmp	r3, #0
	moveq	pc, lr		@ Must never be called for CPU 0
#endif

	ldr	r12, =TEGRA_FLOW_CTRL_VIRT
	cpu_to_csr_reg r1, r3
	add	r1, r1, r12	@ virtual CSR address for this CPU
	cpu_to_halt_reg r2, r3
	add	r2, r2, r12	@ virtual HALT_EVENTS address for this CPU

	/*
	 * Clear this CPU's "event" and "interrupt" flags and power gate
	 * it when halting but not before it is in the "WFE" state.
	 */
	movw	r12, \
		FLOW_CTRL_CSR_INTR_FLAG | FLOW_CTRL_CSR_EVENT_FLAG | \
		FLOW_CTRL_CSR_ENABLE
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	mov	r4, #(1 << 8)			@ wfi bitmap
#else
	mov	r4, #(1 << 4)			@ wfe bitmap
#endif
	orr	r12, r12, r4, lsl r3
	str	r12, [r1]

	/* Halt this CPU. */
	mov	r3, #0x400
delay_1:
	subs	r3, r3, #1			@ delay as a part of wfe war.
	bge	delay_1;
	cpsid	a				@ disable imprecise aborts.
	ldr	r3, [r1]			@ read CSR
	str	r3, [r1]			@ clear CSR
	tst	r0, #TEGRA30_POWER_HOTPLUG_SHUTDOWN
#ifdef CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE
	mov	r3, #FLOW_CTRL_WAITEVENT
	orreq	r3, r3, #FLOW_CTRL_HALT_GIC_IRQ
	orreq	r3, r3, #FLOW_CTRL_HALT_GIC_FIQ
#else
	moveq	r3, #FLOW_CTRL_WAIT_FOR_INTERRUPT	@ For LP2
	movne	r3, #FLOW_CTRL_WAITEVENT		@ For hotplug
#endif
	str	r3, [r2]
	ldr	r0, [r2]
	b	wfe_war

__cpu_reset_again:
	dsb
	.align 5
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	wfi					@ CPU should be power gated here
#else
	wfe					@ CPU should be power gated here
#endif
wfe_war:
	b	__cpu_reset_again

	/*
	 * 38 nop's, which fills reset of wfe cache line and
	 * 4 more cachelines with nop
	 */
	.rept 38
	nop
	.endr
	b	.				@ should never get here

ENDPROC(tegra30_cpu_shutdown)
#endif

#ifdef CONFIG_PM_SLEEP
/*
 * tegra3_sleep_core_finish(unsigned long int)
 *
 * enters suspend in LP0 or LP1 by turning off the mmu and jumping to
 * tegra3_tear_down_core in IRAM
 */
ENTRY(tegra3_sleep_core_finish)
	mov	r4, r0
	bl	tegra_flush_cache
	mov	r0, r4
	bl	tegra_cpu_exit_coherency

	/* preload all the address literals that are needed for the
	 * CPU power-gating process, to avoid loads from SDRAM (which are
	 * not supported once SDRAM is put into self-refresh.
	 * LP0 / LP1 use physical address, since the MMU needs to be
	 * disabled before putting SDRAM into self-refresh to avoid
	 * memory access due to page table walks */
	mov32	r4, TEGRA_PMC_BASE
	mov32	r5, TEGRA_CLK_RESET_BASE
	mov32	r6, TEGRA_FLOW_CTRL_BASE
	mov32	r7, TEGRA_TMRUS_BASE

	mov32	r1, tegra3_tear_down_core
	mov32	r2, tegra3_iram_start
	sub	r1, r1, r2
	mov32	r2, TEGRA_IRAM_CODE_AREA
	add	r1, r1, r2
	mov	r11, #0
	b	tegra_turn_off_mmu
ENDPROC(tegra3_sleep_core_finish)

ENTRY(tegra3_stop_mc_clk_finish)
	mov	r4, r0
	bl	tegra_flush_cache
	mov	r0, r4
	bl	tegra_cpu_exit_coherency

	/* preload all the address literals that are needed for the
	 * CPU power-gating process, to avoid loads from SDRAM (which are
	 * not supported once SDRAM is put into self-refresh.
	 * LP0 / LP1 use physical address, since the MMU needs to be
	 * disabled before putting SDRAM into self-refresh to avoid
	 * memory access due to page table walks */
	mov32	r4, TEGRA_PMC_BASE
	mov32	r5, TEGRA_CLK_RESET_BASE
	mov32	r6, TEGRA_FLOW_CTRL_BASE
	mov32	r7, TEGRA_TMRUS_BASE

	mov32	r1, tegra3_stop_mc_clk
	mov32	r2, tegra3_iram_start
	sub	r1, r1, r2
	mov32	r2, TEGRA_IRAM_CODE_AREA
	add	r1, r1, r2
	mov	r11, #1
	b	tegra_turn_off_mmu
ENDPROC(tegra3_stop_mc_clk_finish)

/*
 * tegra3_sleep_cpu_secondary_finish(unsigned long v2p)
 *
 * Enters LP2 on secondary CPU by exiting coherency and powergating the CPU.
 */
ENTRY(tegra3_sleep_cpu_secondary_finish)
	mov	r6, lr

	bl	tegra_flush_l1_cache
	bl	tegra_cpu_exit_coherency

	/* Powergate this CPU. */
	mov	r0, #0				@ power mode flags (!hotplug)
	bl	tegra30_cpu_shutdown
	mov	r0, #1				@ never return here
	mov	pc, r6
ENDPROC(tegra3_sleep_cpu_secondary_finish)

/*
 * tegra3_tear_down_cpu
 *
 * Switches the CPU cluster to PLL-P and enters sleep.
 */
ENTRY(tegra3_tear_down_cpu)
	mov32	r4, TEGRA_PMC_BASE
	mov32	r5, TEGRA_CLK_RESET_BASE
	mov32	r6, TEGRA_FLOW_CTRL_BASE
	mov32	r7, TEGRA_TMRUS_BASE
#if USE_PLLP_ON_SLEEP_ENTRY
	bl	tegra_cpu_pllp
#endif
	b	tegra3_enter_sleep
ENDPROC(tegra3_tear_down_cpu)

/* START OF ROUTINES COPIED TO IRAM */
	.align L1_CACHE_SHIFT
	.globl tegra3_iram_start
tegra3_iram_start:

/*
 * tegra3_lp1_reset
 *
 * reset vector for LP1 restore; copied into IRAM during suspend.
 * brings the system back up to a safe starting point (SDRAM out of
 * self-refresh, PLLC, PLLM and PLLP reenabled, CPU running on PLLP,
 * system clock running on the same PLL that it suspended at), and
 * jumps to tegra_lp2_startup to restore PLLX and virtual addressing.
 * physical address of tegra_lp2_startup expected to be stored in
 * PMC_SCRATCH41
 *
 * NOTE: THIS *MUST* BE RELOCATED TO TEGRA_IRAM_CODE_AREA AND MUST BE FIRST.
 */
.macro pll_enable, rd, car, base, misc
	ldr	\rd, [\car, #\base]
	tst	\rd, #(1<<30)
	orreq	\rd, \rd, #(1<<30)
	streq	\rd, [\car, #\base]
#if USE_PLL_LOCK_BITS
	.if	\misc
	ldr	\rd, [\car, #\misc]
	bic	\rd, \rd, #(1<<18)
	str	\rd, [\car, #\misc]
	ldr	\rd, [\car, #\misc]
	ldr	\rd, [\car, #\misc]
	orr	\rd, \rd, #(1<<18)
	str	\rd, [\car, #\misc]
	.endif
#endif
.endm

.macro pll_locked, rd, car, base
#if USE_PLL_LOCK_BITS
1:
	ldr	\rd, [\car, #\base]
	tst	\rd, #(1<<27)
	beq	1b
#endif
.endm

.macro pll_iddq_exit, rd, car, iddq, iddq_bit
	ldr	\rd, [\car, #\iddq]
	bic	\rd, \rd, #(1<<\iddq_bit)
	str	\rd, [\car, #\iddq]
.endm

.macro pll_iddq_entry, rd, car, iddq, iddq_bit
	ldr	\rd, [\car, #\iddq]
	orr	\rd, \rd, #(1<<\iddq_bit)
	str	\rd, [\car, #\iddq]
.endm

ENTRY(tegra3_lp1_reset)
	/* the CPU and system bus are running from CLKM and executing from
	 * IRAM when this code is executed
	 * switch all SCLK/CCLK clocks to CLKM and set non STDBY clock source
	 * enable PLLP, PLLM, PLLC, and PLLX. */

	/* If resuming from MC clock stop state then enable EMC clock
	 * other PLLs are not disabled */
	ldr	r8, [r12, #RESET_DATA(MASK_MC_CLK)]
	tst	r8, r11		@ if memory clock stopped
	mov32	r2, TEGRA_PMC_BASE
	beq	resume_lp1

	/* If PLLM was enabled before entering MC clock stop state
	 * then enable it else continue enabling EMC clock */
	mov32	r0, TEGRA_CLK_RESET_BASE
	ldr	r1, pllm_state
	tst	r1, #0
	bne	enable_mc

	pll_iddq_exit r1, r0, CLK_RESET_PLLM_MISC, CLK_RESET_PLLM_MISC_IDDQ

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r1, [r7]
	add	r1, r1, #2
	wait_until r1, r7, r3
	/* enable PLLM via PMC */
	ldr	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]
	orr	r1, r1, #(1<<12)
	str	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]

	pll_enable r1, r0, CLK_RESET_PLLM_BASE, 0
	pll_locked r1, r0, CLK_RESET_PLLM_BASE

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r1, [r7]
	add	r1, r1, #LOCK_DELAY
	wait_until r1, r7, r3

enable_mc:
	mov32	r0, TEGRA_CLK_RESET_BASE
	mov	r1, #(1 << 25)
	str	r1, [r0, #CLK_RESET_CLK_ENB_H_SET]
	b	emc_exit_selfrefresh

resume_lp1:
	mov32	r0, TEGRA_CLK_RESET_BASE
#if !defined(CONFIG_TEGRA_USE_SECURE_KERNEL)
	/* secure code handles 32KHz to CLKM/OSC clock switch */
	mov	r1, #(1<<28)
	str	r1, [r0, #CLK_RESET_SCLK_BURST]
	str	r1, [r0, #CLK_RESET_CCLK_BURST]
	mov	r1, #0
	str	r1, [r0, #CLK_RESET_SCLK_DIVIDER]
	str	r1, [r0, #CLK_RESET_CCLK_DIVIDER]
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* enable PLLM via PMC */
	mov32	r2, TEGRA_PMC_BASE
	ldr	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]
	orr	r1, r1, #(1<<12)
	str	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]

	pll_enable r1, r0, CLK_RESET_PLLM_BASE, CLK_RESET_PLLM_MISC
	pll_enable r1, r0, CLK_RESET_PLLC_BASE, CLK_RESET_PLLC_MISC
	pll_enable r1, r0, CLK_RESET_PLLX_BASE, CLK_RESET_PLLX_MISC
#else
	pll_iddq_exit r1, r0, CLK_RESET_PLLM_MISC, CLK_RESET_PLLM_MISC_IDDQ
	pll_iddq_exit r1, r0, CLK_RESET_PLLC_MISC, CLK_RESET_PLLC_MISC_IDDQ
	pll_iddq_exit r1, r0, CLK_RESET_PLLX_MISC3, CLK_RESET_PLLX_MISC3_IDDQ

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r1, [r7]
	add	r1, r1, #2
	wait_until r1, r7, r3

	/* enable PLLM via PMC */
	mov32	r2, TEGRA_PMC_BASE
	ldr	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]
	orr	r1, r1, #(1<<12)
	str	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]

	pll_enable r1, r0, CLK_RESET_PLLM_BASE, 0
	pll_enable r1, r0, CLK_RESET_PLLC_BASE, 0
	pll_enable r1, r0, CLK_RESET_PLLX_BASE, 0
#endif
	pll_enable r1, r0, CLK_RESET_PLLP_BASE, CLK_RESET_PLLP_MISC

	pll_locked r1, r0, CLK_RESET_PLLM_BASE
	pll_locked r1, r0, CLK_RESET_PLLP_BASE
	pll_locked r1, r0, CLK_RESET_PLLC_BASE
	pll_locked r1, r0, CLK_RESET_PLLX_BASE

#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	ldr	r1, [r0, #CLK_RESET_PLLP_BASE]
	bic	r1, r1, #(1<<31)                /* disable PllP bypass */
	str	r1, [r0, #CLK_RESET_PLLP_BASE]

	mov	r1, #CLK_RESET_PLLP_RESHIFT_DEFAULT
	str	r1, [r0, #CLK_RESET_PLLP_RESHIFT]
#endif

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r1, [r7]
	add	r1, r1, #LOCK_DELAY
	wait_until r1, r7, r3

#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* re-enable cl_dvfs logic clock (if dfll running, it's in open loop) */
	mov	r4, #(1 << 27)
	str	r4, [r0, #CLK_RESET_CLK_ENB_W_SET]
#endif

#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* re-enable cl_dvfs logic clock (if dfll running, it's in open loop) */
	mov	r4, #(1 << 27)
	str	r4, [r0, #CLK_RESET_CLK_ENB_W_SET]
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	add	r5, pc, #tegra3_sdram_pad_save-(.+8)	@ r5 --> saved data
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	add	r5, pc, #tegra11_sdram_pad_save-(.+8)	@ r5 --> saved data
#endif

	ldr	r4, [r5, #0x18]
	str	r4, [r0, #CLK_RESET_CLK_SOURCE_MSELECT]

	ldr	r4, [r5, #0x1C]
	str	r4, [r0, #CLK_RESET_SCLK_BURST]
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	mov32	r4, ((1<<28) | (8))	@ burst policy is PLLX
	str	r4, [r0, #CLK_RESET_CCLK_BURST]
#else
	/* first restore PLLX div2 state, 2us delay, then CPU clock source */
	ldr	r4, [r5, #0x20]
	tst	r4, #CCLK_BURST_PLLX_DIV2_BYPASS_LP
	ldr	r1, [r0, #CLK_RESET_CCLK_BURST]
	biceq	r1, r1, #CCLK_BURST_PLLX_DIV2_BYPASS_LP
	orrne	r1, r1, #CCLK_BURST_PLLX_DIV2_BYPASS_LP
	str	r1, [r0, #CLK_RESET_CCLK_BURST]
	ldr	r1, [r7]
	add	r1, r1, #2
	wait_until r1, r7, r3
	str	r4, [r0, #CLK_RESET_CCLK_BURST]
#endif

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
lp1_voltset:
	/* Restore the Core voltage to high on LP1 resume */
	/* Reset(Enable/Disable) the DVC-I2C Controller*/
	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_RST_DEV_H_SET]

	/* Wait for 2us */
	mov32	r7, TEGRA_TMRUS_BASE
	wait_for_us r1, r7, r9
	add r1, r1, #2
	wait_until r1, r7, r9

	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_RST_DEV_H_CLR]

	/* Enable the DVC-I2C Controller */
	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_ENB_H_SET]


	/* Same I2C transaction protocol as suspend */
	ldr r1, lp1_register_pmuslave_addr
	cmp r1, #0
	beq lp1_voltskip_resume

	ldr r4, lp1_register_i2c_base_addr
	str r1, [r4, #I2C_ADDR0]

	mov32 r1, 0x2
	str r1, [r4, #I2C_CNFG]

	ldr r1, lp1_register_core_highvolt
	str r1, [r4, #I2C_DATA1]

	mov32 r1, 0
	str r1, [r4, #I2C_DATA2]

	mov32 r1, 0xA02
	str r1, [r4, #I2C_CNFG]

	wait_for_us r1, r7, r9
	mov32 r3, 0x7D0   /* Wait for 2ms and try transaction again */
	add r3, r1, r3
loop_i2c_status_resume:
	add r1, r1, #0xFA /* Check status every 250us */
	wait_until r1, r7, r9
	cmp r3, r1
	beq lp1_voltset

	ldr r3, [r4, #I2C_STATUS]
	cmp r3, #0
	bne loop_i2c_status_resume

lp1_voltskip_resume:
	/* Disable the DVC-I2C Controller */
	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_ENB_H_CLR]
#endif

#if defined (CONFIG_CACHE_L2X0)
	/* power up L2 */
	ldr	r0, [r2, #PMC_PWRGATE_STATUS]
	tst	r0, #(1<<PMC_PWRGATE_PARTID_L2C)
	bne	powerup_l2_done
	movw	r0, #(1<<8) | PMC_PWRGATE_PARTID_L2C
	str	r0, [r2, #PMC_PWRGATE_TOGGLE]
powerup_l2_wait:
	ldr	r0, [r2, #PMC_PWRGATE_STATUS]
	tst	r0, #(1<<PMC_PWRGATE_PARTID_L2C)
	beq	powerup_l2_wait
powerup_l2_done:
	mov	r0, #PMC_PWRGATE_PARTID_L2C
	str	r0, [r2, #PMC_REMOVE_CLAMPING_CMD]
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)

	/* If we are waking up from LP1, unconditionally continue
	 * resume.
	 */
	mov32	r4, TEGRA_PMC_BASE
	ldr	r0, [r4, #PMC_LP_STATE_SCRATCH_REG]
	mov	r0, r0, lsr #PMC_LP_STATE_BIT_OFFSET
	and	r0, r0, #PMC_LP_STATE_BIT_MASK
	cmp	r0, #PMC_LP_STATE_LP1BB
	beq	self_refresh_skip
#endif

emc_exit_selfrefresh:
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	mov32	r0, TEGRA_EMC_BASE		@ r0 reserved for emc base
	add	r5, pc, #tegra3_sdram_pad_save-(.+8)	@ r5 --> saved data
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	mov32	r0, TEGRA_EMC0_BASE		@ r0 reserved for emc base
	add	r5, pc, #tegra11_sdram_pad_save-(.+8)	@ r5 --> saved data
#endif

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	/* Take BGBIAS pads out of DPD */
	mov32	r1, 0x40020000
	str	r1, [r2, #PMC_IO_DPD3_REQ]

dram_exit_sr_wait4:
	ldr	r1, [r2, #PMC_IO_DPD3_STATUS]
	tst	r1, #(1 << 17)
	bne	dram_exit_sr_wait4

	/* Take VTTGEN pads out of DPD */
	mov32	r1, 0x4CD00000
	str	r1, [r2, #PMC_IO_DPD3_REQ]

dram_exit_sr_wait3:
	ldr	r1, [r2, #PMC_IO_DPD3_STATUS]
	tst	r1, #(1 << 20)
	bne	dram_exit_sr_wait3

	/* Take func pads out of dpd explicitly */
	mov32	r1, 0x430DFFFF
	str	r1, [r2, #PMC_IO_DPD3_REQ]

dram_exit_sr_wait2:
	ldr	r1, [r2, #PMC_IO_DPD3_STATUS]
	tst	r1, #(1 << 18)
	bne	dram_exit_sr_wait2

	mov32	r1, 0x40400000
	str	r1, [r2, #PMC_IO_DPD_REQ]

dram_exit_sr_wait1:
	ldr	r1, [r2, #PMC_IO_DPD_STATUS]
	tst	r1, #(1 << 22)
	bne	dram_exit_sr_wait1

	ldr	r1, [r2, #PMC_POR_DPD_CTRL]
	bic	r1, r1, #0x80000003
	str	r1, [r2, #PMC_POR_DPD_CTRL]
#endif

	ldr	r1, [r5, #0x14]	@ PMC_IO_DPD_STATUS
	mvn	r1, r1
	bic	r1, r1, #(0x1<<31)
	orr	r1, r1, #(0x1<<30)
	str	r1, [r2, #PMC_IO_DPD_REQ]

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	ldr	r1, [r5, #0x24]	@ PMC_IO_DPD3_STATUS
	mvn	r1, r1
	bic	r1, r1, #(0x1<<31)
	orr	r1, r1, #(0x1<<30)
	str	r1, [r2, #PMC_IO_DPD3_REQ]
#endif

exit_self_refresh:
	ldr	r1, [r5, #0xC]
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL]
	ldr	r1, [r5, #0x10]
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL2]
	ldr	r1, [r5, #0x8]
	str	r1, [r0, #EMC_AUTO_CAL_INTERVAL]

	ldr	r1, [r0, #EMC_CFG_DIG_DLL]
	orr	r1, r1, #(0x1<<30)		@ set DLL_RESET
	str	r1, [r0, #EMC_CFG_DIG_DLL]

	emc_timing_update r1, r0

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	mov32	r1, TEGRA_EMC1_BASE
	cmp	r0, r1
#endif
	ldr	r1, [r0, #EMC_AUTO_CAL_CONFIG]
	orr	r1, r1, #(0x1<<31)		@ set AUTO_CAL_ACTIVE
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	orreq	r1, r1, #(0x1<<27)		@ set slave mode for channel 1
#endif
	str	r1, [r0, #EMC_AUTO_CAL_CONFIG]

emc_wait_audo_cal_onetime:
	ldr	r1, [r0, #EMC_AUTO_CAL_STATUS]
	tst	r1, #(0x1<<31)		@ wait until AUTO_CAL_ACTIVE is clear
	bne	emc_wait_audo_cal_onetime

	ldr	r1, [r0, #EMC_CFG]
	bic	r1, r1, #(1<<31)	@ disable DRAM_CLK_STOP
	str	r1, [r0, #EMC_CFG]

	mov	r1, #0
	str	r1, [r0, #EMC_SELF_REF]	@ take DRAM out of self refresh
	mov	r1, #1
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	str	r1, [r0, #EMC_NOP]
	str	r1, [r0, #EMC_NOP]
#endif
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	str	r1, [r0, #EMC_REFRESH]
#endif

	emc_device_mask r1, r0

exit_selfrefresh_loop:
	ldr	r2, [r0, #EMC_EMC_STATUS]
	ands	r2, r2, r1
	bne	exit_selfrefresh_loop

	lsr	r1, r1, #8		@ devSel, bit0:dev0 bit1:dev1

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r2, [r0, #EMC_FBIO_CFG5]

	and	r2, r2, #3
	cmp	r2, #2
	beq	emc_lpddr2

	mov32	r2, 0x80000011
	str	r2, [r0, #EMC_ZQ_CAL]
	ldr	r2, [r7]
	add	r2, r2, #10
	wait_until r2, r7, r3

	tst	r1, #2
	beq zcal_done

	mov32	r2, 0x40000011
	str	r2, [r0, #EMC_ZQ_CAL]
	ldr	r2, [r7]
	add	r2, r2, #10
	wait_until r2, r7, r3
	b zcal_done

emc_lpddr2:

	mov32	r2, 0x800A00AB
	str	r2, [r0, #EMC_MRW]
	ldr	r2, [r7]
	add	r2, r2, #1
	wait_until r2, r7, r3

	tst	r1, #2
	beq zcal_done

	mov32	r2, 0x400A00AB
	str	r2, [r0, #EMC_MRW]
	ldr	r2, [r7]
	add	r2, r2, #1
	wait_until r2, r7, r3

zcal_done:

	mov	r1, #0
	str	r1, [r0, #EMC_REQ_CTRL]
	ldr	r1, [r5, #0x4]
	str	r1, [r0, #EMC_ZCAL_INTERVAL]
	ldr	r1, [r5, #0x0]
	str	r1, [r0, #EMC_CFG]

	emc_timing_update r1, r0

	ldr	r2, [r7]
	add	r2, r2, #5
	wait_until r2, r7, r3

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	mov32	r1, TEGRA_EMC1_BASE
	cmp	r0, r1
	movne	r0, r1
	addne	r5, r5, #0x24
	bne	exit_self_refresh
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	/* In the LP1 case, we need to set the Memory status from
	 * AP to BB, so that memory transactions can take place
	 */
	mov32	r4, TEGRA_PMC_BASE
	mov	r1, #PMC_IPC_SET_MEM_STS
	str	r1, [r4, #PMC_IPC_SET]
self_refresh_skip:
#endif
	mov32	r0, TEGRA_PMC_BASE
	ldr	r0, [r0, #PMC_SCRATCH41]
	mov	pc, r0
ENDPROC(tegra3_lp1_reset)

#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	.align	L1_CACHE_SHIFT
	.type	tegra3_sdram_pad_save, %object
tegra3_sdram_pad_save:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

tegra3_sdram_pad_address:
	.word	TEGRA_EMC_BASE + EMC_CFG				@0x0
	.word	TEGRA_EMC_BASE + EMC_ZCAL_INTERVAL			@0x4
	.word	TEGRA_EMC_BASE + EMC_AUTO_CAL_INTERVAL			@0x8
	.word	TEGRA_EMC_BASE + EMC_XM2VTTGENPADCTRL			@0xc
	.word	TEGRA_EMC_BASE + EMC_XM2VTTGENPADCTRL2			@0x10
	.word	TEGRA_PMC_BASE + PMC_IO_DPD_STATUS			@0x14
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_CLK_SOURCE_MSELECT	@0x18
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_SCLK_BURST		@0x1c
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_CCLK_BURST		@0x20
	.word	TEGRA_PMC_BASE + PMC_IO_DPD3_STATUS			@0x24
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	.align	L1_CACHE_SHIFT
	.type	tegra11_sdram_pad_save, %object
tegra11_sdram_pad_save:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

tegra11_sdram_pad_address:
	.word	TEGRA_EMC0_BASE + EMC_CFG				@0x0
	.word	TEGRA_EMC0_BASE + EMC_ZCAL_INTERVAL			@0x4
	.word	TEGRA_EMC0_BASE + EMC_AUTO_CAL_INTERVAL			@0x8
	.word	TEGRA_EMC0_BASE + EMC_XM2VTTGENPADCTRL			@0xc
	.word	TEGRA_EMC0_BASE + EMC_XM2VTTGENPADCTRL2			@0x10
	.word	TEGRA_PMC_BASE + PMC_IO_DPD_STATUS			@0x14
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_CLK_SOURCE_MSELECT	@0x18
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_SCLK_BURST		@0x1c
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_CCLK_BURST		@0x20
	.word	TEGRA_EMC1_BASE + EMC_CFG				@0x24
	.word	TEGRA_EMC1_BASE + EMC_ZCAL_INTERVAL			@0x28
	.word	TEGRA_EMC1_BASE + EMC_AUTO_CAL_INTERVAL			@0x2c
	.word	TEGRA_EMC1_BASE + EMC_XM2VTTGENPADCTRL			@0x30
	.word	TEGRA_EMC1_BASE + EMC_XM2VTTGENPADCTRL2			@0x34
#endif

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	.globl lp1_register_pmuslave_addr
	.globl lp1_register_i2c_base_addr
	.globl lp1_register_core_lowvolt
	.globl lp1_register_core_highvolt
lp1_register_pmuslave_addr:
	.word	0
lp1_register_i2c_base_addr:
	.word	0
lp1_register_core_lowvolt:
	.word	0
lp1_register_core_highvolt:
	.word	0
#endif

pllm_state:
	.word	0

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
lp_enter_state:
	.word	0
#endif

/* tegra3_tear_down_core
 *
 * LP0 entry check conditions w.r.t BB take place here
 */
tegra3_tear_down_core:
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	/* Checking for BB-idle or Paging case */
	ldr	r0, [r4, #PMC_IPC_STS]
	tst	r0, #PMC_IPC_STS_MEM_REQ | PMC_IPC_STS_MEM_REQ_SOON
	bne	lp1bb_entry

	/* Write PMC_IPC_CLR[mem_sts] = 1 */
	mov	r1, #PMC_IPC_CLR_MEM_STS
	str	r1, [r4, #PMC_IPC_CLR]

	/* Clear FLOW_IPC_STS[AP2BB_MSC_STS[0]] */
	ldr	r1, [r6, #FLOW_IPC_STS]
	bic	r1, #FLOW_IPC_STS_AP2BB_MSC_STS_0
	str	r1, [r6, #FLOW_IPC_STS]

	b	tegra3_lp0_tear_down_core

/* lp1bb_entry
 * Set up mem_req active low to be a wake event.
 * Configure the EVP reset vector.
 * Set up LIC to accept pmc wake events as interrupts.
 * Clear previously set warmboot and side_effect bits
 * Invoke remaining LP routines.
 */
lp1bb_entry:
	bl	tegra148_lp1bb_clear_warmboot_flag
	mov	r0, #PMC_LP_STATE_LP1BB
	str	r0, lp_enter_state
	bl	tegra148_set_lp_state
	bl	tegra148_set_mem_req_interrupt
	bl	tegra3_save_config
	bl	tegra3_cpu_clk32k
	b	tegra3_enter_sleep

/* Based on LP state being entered, sets mem_req=0
 * or mem_req=1 as a wake interrupt
 */
tegra148_set_mem_req_interrupt:
	/* Clear the PMC_CTRL2_WAKE_DET_EN bit */
	ldr	r0, [r4, #PMC_CTRL2]
	bic	r0, r0, #PMC_CTRL2_WAKE_DET_EN
	str	r0, [r4, #PMC_CTRL2]

	/* Program the wake_level2 registers */
	ldr     r0, [r4, #PMC_WAKE2_LEVEL]
	ldr	r1, lp_enter_state
	cmp	r1, #PMC_LP_STATE_LP1BB
	biceq	r0, r0, #PMC_WAKE2_BB_MEM_REQ
	orrne	r0, r0, #PMC_WAKE2_BB_MEM_REQ
	str	r0, [r4, #PMC_WAKE2_LEVEL]

	/* Wait for 1ms for write to take effect */
	mov32	r7, TEGRA_TMRUS_BASE
	wait_for_us r1, r7, r9
	add r1, r1, #100
	wait_until r1, r7, r9

	/* Program the auto_wake_lvl regsiters */
	ldr	r0, [r4, #PMC_AUTO_WAKE_LVL]
	orr	r0, r0, #1
	str	r0, [r4, #PMC_AUTO_WAKE_LVL]

	/* Wait for 1ms for write to take effect */
	mov32	r7, TEGRA_TMRUS_BASE
	wait_for_us r1, r7, r9
	add r1, r1, #100
	wait_until r1, r7, r9

	/* Configure mem_req active low to be wake event */
	ldr	r0, [r4, #PMC_WAKE2_MASK]
	orr	r0, r0, #PMC_WAKE2_BB_MEM_REQ
	str	r0, [r4, #PMC_WAKE2_MASK]

	ldr	r0, [r4, #PMC_CTRL2]
	orr	r0, r0, #PMC_CTRL2_WAKE_DET_EN
	str	r0, [r4, #PMC_CTRL2]

	/* Set up the LIC to accept pmc_wake events as interrupts */
	ldr	r8, =TEGRA_TERTIARY_ICTLR_BASE
	ldr	r0, =TRI_ICTLR_PMC_WAKE_INT
	str	r0, [r8, #TRI_ICTLR_CPU_IER_SET]
	mov	pc, lr

/*
 * tegra148_lp1bb_clear_warmboot_flag
 * Clears side effect bit in case it was set during
 * suspend entry. Also clears Warmboot0 flag.
 */
tegra148_lp1bb_clear_warmboot_flag:
	ldr	r0, [r4, #PMC_SCRATCH0]
	bic	r0, r0, #1
	str	r0, [r4, #PMC_SCRATCH0]
	ldr	r0, [r4, #PMC_CTRL]
	bic	r0, r0, #PMC_CTRL_SIDE_EFFECT_LP0
	str	r0, [r4, #PMC_CTRL]
	mov	pc, lr

/* Based on value of lp_enter_state, update LP state
 * scratch register
 */
tegra148_set_lp_state:
	ldr	r0, lp_enter_state
	mov	r0, r0, lsl #PMC_LP_STATE_BIT_OFFSET
	ldr	r1, [r4, #PMC_LP_STATE_SCRATCH_REG]
	mov	r2, #PMC_LP_STATE_BIT_MASK
	bic	r1, r2, lsl #PMC_LP_STATE_BIT_OFFSET
	orr	r1, r0
	str	r1, [r4, #PMC_LP_STATE_SCRATCH_REG]
	mov	pc, lr

/* tegra3_lp0_tear_down_core
 *
 * copied into and executed from IRAM
 * puts memory in self-refresh for LP0 and LP1
 */
tegra3_lp0_tear_down_core:
	ldr	r0, [r4, #PMC_CTRL]
	tst	r0, #PMC_CTRL_SIDE_EFFECT_LP0
	moveq	r0, #PMC_LP_STATE_LP1
	movne	r0, #PMC_LP_STATE_LP0
	str	r0, lp_enter_state
	bleq	tegra148_set_mem_req_interrupt
	bl	tegra148_set_lp_state
#endif
	bl	tegra3_save_config
	bl	tegra3_sdram_self_refresh
	bl	tegra3_cpu_clk32k
	b	tegra3_enter_sleep

tegra3_stop_mc_clk:
	bl	tegra3_save_config
	bl	tegra3_sdram_self_refresh
	mov	r1, #(1 << 25)
	str	r1, [r5, #CLK_RESET_CLK_ENB_H_CLR]

	/* Store PLLM state
	 * If PLLP_WB0_OVERRIDE(Bit 11) is set then store
	 * value OVERRIDE_ENABLE(Bit 12) else store CAR value */
	ldr	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	and	r1, r0, #(1 << 12)
	lsr	r1, #12
	and	r0, r0, #(1 << 11)
	lsr	r0, #11
	and	r1, r1, r0

	ldr	r2, [r5, #CLK_RESET_PLLM_BASE]
	and	r2, r2, #(1 << 30)
	lsr	r2, #30
	bic	r2, r2, r0

	orr	r1, r1, r2

	str	r1, pllm_state

	/* disable PLLM via PMC in LP1 */
	ldr	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	bic	r0, r0, #(1 << 12)
	str	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]

	/* disable PLLM in CAR */
	ldr	r0, [r5, #CLK_RESET_PLLM_BASE]
	bic	r0, r0, #(1 << 30)
	str	r0, [r5, #CLK_RESET_PLLM_BASE]

	b	tegra3_enter_sleep

/*
 * tegra3_cpu_clk32k
 *
 * In LP0 and LP1 all plls will be turned off.  Switch the system clock
 * to the 32khz clock (clks) and CPU to clkm.
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTRL_BASE
 * r7 = TEGRA_TMRUS_BASE
 */
tegra3_cpu_clk32k:
	ldr	r0, [r4, #PMC_CTRL]
	tst	r0, #PMC_CTRL_SIDE_EFFECT_LP0
	beq	lp1_clocks_prepare

	/* enable PLLM auto-restart via PMC in LP0; restore override settings */
	ldr	r0, [r4, #PMC_SCRATCH2]
	str	r0, [r4, #PMC_PLLM_WB0_OVERRIDE]
	ldr	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	orr	r0, r0, #((1 << 12) | (1 << 11))
	str	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	ldr r1, [r4, #PMC_SCRATCH1_ECO]
	orr r1, r1, #0x3F
	str r1, [r4, #PMC_SCRATCH1_ECO]
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	ldr r1, [r4, #PMC_POR_DPD_CTRL]
	orr r1, r1, #0x47
	str r1, [r4, #PMC_POR_DPD_CTRL]
#endif

	mov	pc, lr

lp1_clocks_prepare:
	/* Prepare to set the Core to the lowest voltage if supported.
	 * Start by setting the I2C clocks to make the I2C transfer */
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	/* Set up the PWR I2C GPIOs with the right masks*/

	/* Reset(Set/Clr) the DVC-I2C Controller*/
	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_RST_DEV_H_SET]

	/* Wait for 2us */
	wait_for_us r1, r7, r9
	mov32 r0, 0x7D0
	add r1, r1, r0
	wait_until r1, r7, r9

	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_RST_DEV_H_CLR]

	/* Enable the DVC-I2C Controller */
	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_ENB_H_SET]

	/* I2C transfer protocol:
	 * 4 packets: Slaveaddr + WriteConfigure + Data1 + Data2 */
	ldr r0, lp1_register_pmuslave_addr
	cmp r0, #0
	beq lp1_volt_skip
	ldr r1, lp1_register_i2c_base_addr
	str r0, [r1, #I2C_ADDR0]

	mov32 r0, 0x2
	str r0, [r1, #I2C_CNFG]

	ldr r0, lp1_register_core_lowvolt
	str r0, [r1, #I2C_DATA1]

	mov32 r0, 0
	str r0, [r1, #I2C_DATA2]

	/* Send I2C transaction */
	mov32 r0, 0xA02
	str r0, [r1, #I2C_CNFG]

	/* Check the transaction status before proceeding */
	wait_for_us r2, r7, r9
	mov32 r3, 0x7D0 /* Wait for 2ms for I2C transaction */
	add r3, r2, r3
loop_i2c_status_suspend:
	add r2, r2, #0xFA /* Check status every 250us */
	cmp r3, r2
	beq lp1_volt_skip  /* Waited for 2ms, I2C transaction didn't take place */
	wait_until r2, r7, r9

	ldr r0, [r1, #I2C_STATUS]
	cmp r0, #0
	bne loop_i2c_status_suspend
lp1_volt_skip:

	/* Disable the DVC-I2C Controller */
	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_ENB_H_CLR]

#endif
	/* start by jumping to clkm to safely disable PLLs, then jump
	 * to clks */
	mov	r0, #(1 << 28)
	str	r0, [r5, #CLK_RESET_SCLK_BURST]
	/* 2 us delay between changing sclk and cclk */
	wait_for_us r1, r7, r9
	add	r1, r1, #2
	wait_until r1, r7, r9
	mov	r0, #(1 << 28)
	str	r0, [r5, #CLK_RESET_CCLK_BURST]
	mov	r0, #0
	str	r0, [r5, #CLK_RESET_CCLK_DIVIDER]
	str	r0, [r5, #CLK_RESET_SCLK_DIVIDER]

	/* switch the clock source for mselect to be CLK_M */
	ldr	r0, [r5, #CLK_RESET_CLK_SOURCE_MSELECT]
	orr	r0, r0, #MSELECT_CLKM
	str	r0, [r5, #CLK_RESET_CLK_SOURCE_MSELECT]

#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* disable cl_dvfs logic clock (if dfll running, it's in open loop) */
	mov	r0, #(1 << 27)
	str	r0, [r5, #CLK_RESET_CLK_ENB_W_CLR]
#endif
	/* 2 us delay between changing sclk and disabling PLLs */
	wait_for_us r1, r7, r9
	add	r1, r1, #2
	wait_until r1, r7, r9

#if !defined(CONFIG_ARCH_TEGRA_14x_SOC)
	/* disable PLLM via PMC in LP1 */
	ldr	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	bic	r0, r0, #(1 << 12)
	str	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
#endif

	ldr	r11, [r4, #PMC_SCRATCH37]	@ load the LP1 flags
	tst	r11, #TEGRA_POWER_LP1_AUDIO	@ check if voice call is going on
#if !defined(CONFIG_ARCH_TEGRA_14x_SOC)
	bne	powerdown_pll_cx		@ if yes, do not turn off pll-p/pll-a
#else
	/*
         * BB needs PLLP and EMC on voice call/LP1BB. EMC may be clocked by
         * PLLC so we need to check the EMC source PLL to determine whether
         * PLLC can be turned OFF
         */
	bne	lp1bb_emc_source_check
	ldr	r0, lp_enter_state
	cmp	r0, #PMC_LP_STATE_LP1BB         @ check if we're entering LP1BB
	bne	powerdown_pll_pacx              @ if not, turn off plls p/a/c/x
lp1bb_emc_source_check:
	/* find source pll of EMC */
	ldr	r0, [r5, #CLK_RESET_CLK_SOURCE_EMC]
	mov	r0, r0, lsr #0x1d
	cmp	r0, #0x1                        @ EMC clocked by PLLC_OUT0?
	beq	powerdown_pll_x                 @ if yes, just turn off pll-x
	cmp	r0, #0x7                        @ EMC clocked by PLLC_UD?
	beq	powerdown_pll_x                 @ if yes, just turn off pll-x
	b	powerdown_pll_cx                @ if not, turn off pll-c/pll-x
#endif
powerdown_pll_pacx:
	ldr	r0, [r6, #FLOW_CONTROL_CLUSTER_CONTROL]
	tst	r0, #1
	ldr	r0, [r5, #CLK_RESET_PLLP_BASE]
	bic	r0, r0, #(1<<30)
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	orreq   r0, r0, #(1<<31)                @ enable PllP bypass on fast
#endif
	str	r0, [r5, #CLK_RESET_PLLP_BASE]
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	mov	r0, #CLK_RESET_PLLP_RESHIFT_ENABLE
	str	r0, [r5, #CLK_RESET_PLLP_RESHIFT]
#endif
	ldr	r0, [r5, #CLK_RESET_PLLA_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLA_BASE]

powerdown_pll_cx:
	ldr	r0, [r5, #CLK_RESET_PLLC_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLC_BASE]
powerdown_pll_x:
	ldr	r0, [r5, #CLK_RESET_PLLX_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLX_BASE]
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/*
	 * FIXME: put PLLM/C into IDDQ (need additional testing)
	 * pll_iddq_entry r1, r5, CLK_RESET_PLLM_MISC, CLK_RESET_PLLM_MISC_IDDQ
	 * pll_iddq_entry r1, r5, CLK_RESET_PLLC_MISC, CLK_RESET_PLLC_MISC_IDDQ
	 */
	pll_iddq_entry r1, r5, CLK_RESET_PLLX_MISC3, CLK_RESET_PLLX_MISC3_IDDQ
#endif

	/*
	 * Switch to STDBY clock (CLKS), bits 28:31 == 0
	 * Enable burst on CPU IRQ (bit 24)
	 * Set clock sources to CLKM (clock source 0)
	 */
	mov	r0, #(1 << 24)
	str	r0, [r5, #CLK_RESET_SCLK_BURST]
	mov	pc, lr

/*
 * tegra3_enter_sleep
 *
 * uses flow controller to enter sleep state
 * executes from IRAM with SDRAM in selfrefresh when target state is LP0 or LP1
 * executes from SDRAM with target state is LP2
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTRL_BASE
 * r7 = TEGRA_TMRUS_BASE
 */
tegra3_enter_sleep:
	ldr	r1, [r7]
	str	r1, [r4, #PMC_SCRATCH38]
	dsb
	cpu_id	r1

	cpu_to_csr_reg	r2, r1
	ldr	r0, [r6, r2]
	orr	r0, r0, #FLOW_CTRL_CSR_INTR_FLAG | FLOW_CTRL_CSR_EVENT_FLAG
	orr	r0, r0, #FLOW_CTRL_CSR_ENABLE
	str	r0, [r6, r2]

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC) \
	|| defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tst	r0, #FLOW_CTRL_IMMEDIATE_WAKE
	movne	r0, #FLOW_CTRL_WAITEVENT
	moveq	r0, #FLOW_CTRL_WAIT_FOR_INTERRUPT
	orr	r0, r0, #FLOW_CTRL_HALT_LIC_IRQ | FLOW_CTRL_HALT_LIC_FIQ
#else
	mov	r0, #FLOW_CTRL_WAIT_FOR_INTERRUPT
	orr	r0, r0, #FLOW_CTRL_HALT_CPU_IRQ | FLOW_CTRL_HALT_CPU_FIQ
#endif
	cpu_to_halt_reg r2, r1
	str	r0, [r6, r2]
	dsb
	ldr	r0, [r6, r2] /* memory barrier */

#ifndef CONFIG_ARM_SAVE_DEBUG_CONTEXT_NO_LOCK
	/* Set the Debug OS Double Lock for Debug Arch v7.1 or greater.
	   With this lock set, the debugger is completely locked out.
	   Disable this to debug WFI/powergating failures.
	*/
	mrc	p15, 0, r3, c0, c1, 2	@ ID_DFR0
	and	r3, r3, #0xF		@ coprocessor debug model
	cmp	r3, #5			@ debug arch >= v7.1?

	mov32	r1, 0xC5ACCE55
	mcrge	p14, 0, r1, c1, c3, 4	@ DBGOSDLR
#endif
halted:
	isb
	dsb
	wfi	/* CPU should be power gated here */

	/* !!!FIXME!!! Implement halt failure handler */
	b	halted

/*
 * tegra3_sdram_self_refresh
 *
 * called with MMU off and caches disabled
 * puts sdram in self refresh
 * must execute from IRAM
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTRL_BASE
 * r7 = TEGRA_TMRUS_BASE
 */

tegra3_save_config:

#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	adr	r2, tegra3_sdram_pad_address
	adr	r8, tegra3_sdram_pad_save
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	adr	r2, tegra11_sdram_pad_address
	adr	r8, tegra11_sdram_pad_save
#endif
	mov	r9, r2

padsave:
	ldr	r0, [r2], #4			@ r0 is emc register address

	ldr	r1, [r0]
	str	r1, [r8], #4			@ save emc register

	cmp	r8, r9
	bne	padsave
padsave_done:

	dsb
	mov	pc, lr

tegra3_sdram_self_refresh:
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	mov32	r0, TEGRA_EMC_BASE		@ r0 reserved for emc base
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	mov32	r0, TEGRA_EMC0_BASE		@ r0 reserved for emc base
#endif
enter_self_refresh:
	/* Enable SEL_DPD */
	ldr	r1, [r0, #EMC_SEL_DPD_CTRL]
	orr	r1, r1, #0xF
	orr	r1, r1, #0xF0
	orr	r1, r1, #0x100
	str	r1, [r0, #EMC_SEL_DPD_CTRL]

	mov	r1, #0
	str	r1, [r0, #EMC_ZCAL_INTERVAL]
	str	r1, [r0, #EMC_AUTO_CAL_INTERVAL]
	ldr	r1, [r0, #EMC_CFG]
	bic	r1, r1, #(1<<28)
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	bic	r1, r1, #(1<<29)
#endif
	str	r1, [r0, #EMC_CFG]		@ disable DYN_SELF_REF

	emc_timing_update r1, r0

	ldr	r1, [r7]
	add	r1, r1, #5
	wait_until r1, r7, r2

emc_wait_audo_cal:
	ldr	r1, [r0, #EMC_AUTO_CAL_STATUS]
	tst	r1, #(0x1<<31)		@ wait until AUTO_CAL_ACTIVE is clear
	bne	emc_wait_audo_cal

	mov	r1, #3
	str	r1, [r0, #EMC_REQ_CTRL]		@ stall incoming DRAM requests

emcidle:
	ldr	r1, [r0, #EMC_EMC_STATUS]
	tst	r1, #4
	beq	emcidle

	mov	r1, #1
	str	r1, [r0, #EMC_SELF_REF]

	emc_device_mask r1, r0

emcself:
	ldr	r2, [r0, #EMC_EMC_STATUS]
	and	r2, r2, r1
	cmp	r2, r1
	bne	emcself				@ loop until DDR in self-refresh

	ldr	r1, [r0, #EMC_XM2VTTGENPADCTRL]
	mov32	r2, 0xF8F8FFFF		@ clear XM2VTTGEN_DRVUP and XM2VTTGEN_DRVDN
	and	r1, r1, r2
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL]
	ldr	r1, [r0, #EMC_XM2VTTGENPADCTRL2]
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	orr	r1, r1, #7			@ set E_NO_VTTGEN
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || \
defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	orr	r1, r1, #0x3f			@ set E_NO_VTTGEN
#endif
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL2]

	emc_timing_update r1, r0

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	mov32	r1, TEGRA_EMC1_BASE
	cmp	r0, r1
	movne	r0, r1
	bne	enter_self_refresh
#endif

	ldr	r1, [r4, #PMC_CTRL]
	tst	r1, #PMC_CTRL_SIDE_EFFECT_LP0
	bne	pmc_io_dpd_skip

	ldr	r1, [r4, #PMC_POR_DPD_CTRL]
	orr	r1, r1, #0x80000003
	str	r1, [r4, #PMC_POR_DPD_CTRL]

#if !defined(CONFIG_ARCH_TEGRA_12x_SOC)
	mov32	r1, 0x8EC00000
	str	r1, [r4, #PMC_IO_DPD_REQ]
#endif
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	/* Put func pads in dpd explicitly */
	mov32	r1, 0x80400000
	str	r1, [r4, #PMC_IO_DPD_REQ]

dram_sr_wait1:
	ldr	r1, [r4, #PMC_IO_DPD_STATUS]
	tst	r1, #(1 << 22)
	beq	dram_sr_wait1

	mov32	r1, 0x830DFFFF
	str	r1, [r4, #PMC_IO_DPD3_REQ]

dram_sr_wait2:
	ldr	r1, [r4, #PMC_IO_DPD3_STATUS]
	tst	r1, #(1 << 18)
	beq	dram_sr_wait2

	/* Put VTTGEN pads in DPD */
	mov32	r1, 0x8CD00000
	str	r1, [r4, #PMC_IO_DPD3_REQ]

dram_sr_wait3:
	ldr	r1, [r4, #PMC_IO_DPD3_STATUS]
	tst	r1, #(1 << 20)
	beq	dram_sr_wait3

	/* Put BGBIAS pads in DPD */
	mov32	r1, 0x80020000
	str	r1, [r4, #PMC_IO_DPD3_REQ]

dram_sr_wait4:
	ldr	r1, [r4, #PMC_IO_DPD3_STATUS]
	tst	r1, #(1 << 17)
	beq	dram_sr_wait4

#endif
	dsb
	mov	pc, lr

pmc_io_dpd_skip:
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	/*
	 * Make sure the BGBIAS pads are not in DPD so that when the system
	 * comes out of LP0 at max EMC frequency we can read memory.
	 */
	ldr	r1, =PMC_IO_DPD2_REQ_CODE_DPD_OFF
	orr	r1, r1, #PMC_IO_DPD2_REQ_DISC_BIAS
	str	r1, [r4, #PMC_IO_DPD2_REQ]
#endif
	dsb
	mov	pc, lr

	.ltorg
/* dummy symbol for end of IRAM */
	.align L1_CACHE_SHIFT
	.globl tegra3_iram_end
tegra3_iram_end:
	b	.
#endif
