/*
 * arch/arm/mach-tegra/tegra_ptm.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_PTM_H
#define __MACH_TEGRA_PTM_H

#define PTM0_BASE	(TEGRA_CSITE_BASE + 0x1C000)
#define PTM1_BASE	(TEGRA_CSITE_BASE + 0x1D000)
#define PTM2_BASE	(TEGRA_CSITE_BASE + 0x1E000)
#define PTM3_BASE	(TEGRA_CSITE_BASE + 0x1F000)
#define FUNNEL_BASE	(TEGRA_CSITE_BASE + 0x4000)
#define TPIU_BASE	(TEGRA_CSITE_BASE + 0x3000)
#define ETB_BASE	(TEGRA_CSITE_BASE + 0x1000)

#define TRACER_ACCESSED		BIT(0)
#define TRACER_RUNNING		BIT(1)
#define TRACER_CYCLE_ACC	BIT(2)
#define TRACER_TRACE_DATA	BIT(3)
#define TRACER_TIMESTAMP	BIT(4)
#define TRACER_BRANCHOUTPUT	BIT(5)
#define TRACER_RETURN_STACK	BIT(6)

#define TRACER_TIMEOUT 10000

/*
 * CoreSight Management Register Offsets:
 * These registers offsets are same for both ETB, PTM, FUNNEL and TPIU
 */
#define CORESIGHT_LOCKACCESS	0xfb0
#define CORESIGHT_LOCKSTATUS	0xfb4
#define CORESIGHT_AUTHSTATUS	0xfb8
#define CORESIGHT_DEVID		0xfc8
#define CORESIGHT_DEVTYPE	0xfcc

/* PTM control register */
#define PTM_CTRL			0x0
#define PTM_CTRL_POWERDOWN		(1 << 0)
#define PTM_CTRL_PROGRAM		(1 << 10)
#define PTM_CTRL_CONTEXTIDSIZE(x)	(((x) & 3) << 14)
#define PTM_CTRL_STALLPROCESSOR		(1 << 7)
#define PTM_CTRL_BRANCH_OUTPUT		(1 << 8)
#define PTM_CTRL_CYCLEACCURATE		(1 << 12)
#define PTM_CTRL_TIMESTAMP_EN		(1 << 28)
#define PTM_CTRL_RETURN_STACK_EN	(1 << 29)

/* PTM configuration code register */
#define PTM_CONFCODE		(0x04)

/* PTM trigger event register */
#define PTM_TRIGEVT		(0x08)

/* address access type register bits, "PTM architecture",
 * table 3-27 */
/* - access type */
#define PTM_COMP_ACC_TYPE(x)		(0x80 + (x) * 4)
/* this is a read only bit */
#define PTM_ACC_TYPE_INSTR_ONLY		(1 << 0)
/* - context id comparator control */
#define PTM_ACC_TYPE_IGN_CONTEXTID	(0 << 8)
#define PTM_ACC_TYPE_CONTEXTID_CMP1	(1 << 8)
#define PTM_ACC_TYPE_CONTEXTID_CMP2	(2 << 8)
#define PTM_ACC_TYPE_CONTEXTID_CMP3	(3 << 8)
/* - security level control */
#define PTM_ACC_TYPE_PFT10_IGN_SECURITY	(0 << 10)
#define PTM_ACC_TYPE_PFT10_NS_ONLY	(1 << 10)
#define PTM_ACC_TYPE_PFT10_S_ONLY	(2 << 10)
#define PTM_ACC_TYPE_PFT11_NS_CTRL(x)	(((x & 1) << 11) | ((x & 2) << 13))
#define PTM_ACC_TYPE_PFT11_S_CTRL(x)	(((x & 1) << 10) | ((x & 2) << 12))
#define PTM_ACC_TYPE_PFT11_MATCH_ALL		0
#define PTM_ACC_TYPE_PFT11_MATCH_NONE		1
#define PTM_ACC_TYPE_PFT11_MATCH_PRIVILEGE	2
#define PTM_ACC_TYPE_PFT11_MATCH_USER		3
/*
 * If secureity extension is not implemented, the state mode is same as
 * secure state under security extension.
 */
#define PTM_ACC_TYPE_PFT11_NO_S_EXT_CTRL(x) PTM_ACC_TYPE_PFT11_S_CTRL(x)

#define PTM_COMP_VAL(x)			(0x40 + (x) * 4)

/* PTM status register */
#define PTM_STATUS			0x10
#define PTM_STATUS_OVERFLOW		BIT(0)
#define PTM_STATUS_PROGBIT		BIT(1)
#define PTM_STATUS_STARTSTOP		BIT(2)
#define PTM_STATUS_TRIGGER		BIT(3)
#define ptm_progbit(t, id) (ptm_readl((t), id, PTM_STATUS) & PTM_STATUS_PROGBIT)
#define ptm_started(t) (ptm_readl((t), PTM_STATUS) & PTM_STATUS_STARTSTOP)
#define ptm_triggered(t) (ptm_readl((t), PTM_STATUS) & PTM_STATUS_TRIGGER)

/* PTM trace start/stop resource control register */
#define PTM_START_STOP_CTRL	(0x18)

/* PTM trace enable control */
#define PTM_TRACE_ENABLE_CTRL1			0x24
#define PTM_TRACE_ENABLE_EXCL_CTRL		BIT(24)
#define PTM_TRACE_ENABLE_INCL_CTRL		0
#define PTM_TRACE_ENABLE_CTRL1_START_STOP_CTRL	BIT(25)

/* PTM trace enable event configuration */
#define PTM_TRACE_ENABLE_EVENT		0x20
/* PTM event resource */
#define COUNTER0			((0x4 << 4) | 0)
#define COUNTER1			((0x4 << 4) | 1)
#define COUNTER2			((0x4 << 4) | 2)
#define COUNTER3			((0x4 << 4) | 3)
#define EXT_IN0				((0x6 << 4) | 0)
#define EXT_IN1				((0x6 << 4) | 1)
#define EXT_IN2				((0x6 << 4) | 2)
#define EXT_IN3				((0x6 << 4) | 3)
#define EXTN_EXT_IN0			((0x6 << 4) | 8)
#define EXTN_EXT_IN1			((0x6 << 4) | 9)
#define EXTN_EXT_IN2			((0x6 << 4) | 10)
#define EXTN_EXT_IN3			((0x6 << 4) | 11)
#define IN_NS_STATE			((0x6 << 4) | 13)
#define TRACE_PROHIBITED		((0x6 << 4) | 14)
#define ALWAYS_TRUE			((0x6 << 4) | 15)
#define	LOGIC_A				(0x0 << 14)
#define	LOGIC_NOT_A			(0x1 << 14)
#define	LOGIC_A_AND_B			(0x2 << 14)
#define	LOGIC_NOT_A_AND_B		(0x3 << 14)
#define	LOGIC_NOT_A_AND_NOT_B		(0x4 << 14)
#define	LOGIC_A_OR_B			(0x5 << 14)
#define	LOGIC_NOT_A_OR_B		(0x6 << 14)
#define	LOGIC_NOT_A_OR_NOT_B		(0x7 << 14)
#define SET_RES_A(RES)			((RES) << 0)
#define SET_RES_B(RES)			((RES) << 7)
#define DEF_PTM_EVENT(LOGIC, B, A)	((LOGIC) | SET_RES_B(B) | SET_RES_A(A))

#define PTM_SYNC_FREQ		0x1e0

#define PTM_ID			0x1e4
#define PTMIDR_VERSION(x)	(((x) >> 4) & 0xff)
#define PTMIDR_VERSION_3_1	0x21
#define PTMIDR_VERSION_PFT_1_0	0x30
#define PTMIDR_VERSION_PFT_1_1	0x31

/* PTM configuration register */
#define PTM_CCE			0x1e8
#define PTMCCER_RETURN_STACK_IMPLEMENTED	BIT(23)
#define PTMCCER_TIMESTAMPING_IMPLEMENTED	BIT(22)
/* PTM management registers */
#define PTMMR_OSLAR		0x300
#define PTMMR_OSLSR		0x304
#define PTMMR_OSSRR		0x308
#define PTMMR_PDSR		0x314
/* PTM sequencer registers */
#define PTMSQ12EVR		0x180
#define PTMSQ21EVR		0x184
#define PTMSQ23EVR		0x188
#define PTMSQ31EVR		0x18C
#define PTMSQ32EVR		0x190
#define PTMSQ13EVR		0x194
#define PTMSQR			0x19C
/* PTM counter event */
#define PTMCNTRLDVR(n)		((0x50 + (n)) << 2)
#define PTMCNTENR(n)		((0x54 + (n)) << 2)
#define PTMCNTRLDEVR(n)		((0x58 + (n)) << 2)
#define PTMCNTVR(n)		((0x5c + (n)) << 2)
/* PTM timestamp event */
#define PTMTSEVR		0x1F8
/* PTM ATID registers */
#define PTM_TRACEIDR		0x200

/* ETB registers, "CoreSight Components TRM", 9.3 */
#define ETB_DEPTH		0x04
#define ETB_STATUS		0x0c
#define ETB_STATUS_FULL		BIT(0)
#define ETB_READMEM		0x10
#define ETB_READADDR		0x14
#define ETB_WRITEADDR		0x18
#define ETB_TRIGGERCOUNT	0x1c
#define ETB_CTRL		0x20
#define TRACE_CAPATURE_ENABLE	0x1
#define TRACE_CAPATURE_DISABLE	0x0
#define ETB_RWD			0x24
#define ETB_FF_CTRL		0x304
#define ETB_FF_CTRL_ENFTC	BIT(0)
#define ETB_FF_CTRL_ENFCONT	BIT(1)
#define ETB_FF_CTRL_FONFLIN	BIT(4)
#define ETB_FF_CTRL_MANUAL_FLUSH BIT(6)
#define ETB_FF_CTRL_TRIGIN	BIT(8)
#define ETB_FF_CTRL_TRIGEVT	BIT(9)
#define ETB_FF_CTRL_TRIGFL	BIT(10)
#define ETB_FF_CTRL_STOPFL	BIT(12)

#define FUNNEL_CTRL		0x0
#define FUNNEL_CTRL_CPU0	BIT(0)
#define FUNNEL_CTRL_CPU1	BIT(1)
#define FUNNEL_CTRL_CPU2	BIT(2)
#define FUNNEL_CTRL_ITM		BIT(3)
#define FUNNEL_CTRL_CPU3	BIT(4)
#define FUNNEL_MINIMUM_HOLD_TIME(x) ((x) << 8)
#define FUNNEL_PRIORITY		0x4
#define FUNNEL_PRIORITY_CPU0(p) ((p & 0x7)  << 0)
#define FUNNEL_PRIORITY_CPU1(p) ((p & 0x7)  << 3)
#define FUNNEL_PRIORITY_CPU2(p) ((p & 0x7)  << 6)
#define FUNNEL_PRIORITY_ITM(p)	((p & 0x7)  << 9)
#define FUNNEL_PRIORITY_CPU3(p) ((p & 0x7)  << 12)

#define TPIU_ITCTRL		0xf00
#define TPIU_FF_CTRL		0x304
#define TPIU_FF_CTRL_ENFTC	BIT(0)
#define TPIU_FF_CTRL_ENFCONT	BIT(1)
#define TPIU_FF_CTRL_FONFLIN	BIT(4)
#define TPIU_FF_CTRL_MANUAL_FLUSH BIT(6)
#define TPIU_FF_CTRL_TRIGIN	BIT(8)
#define TPIU_FF_CTRL_TRIGEVT	BIT(9)
#define TPIU_FF_CTRL_TRIGFL	BIT(10)
#define TPIU_FF_CTRL_STOPFL	BIT(12)
#define TPIU_ITATBCTR2		0xef0
#define TPIU_IME		1
#define INTEGRATION_ATREADY	1

#define CLK_TPIU_OUT_ENB_U			0x018
#define CLK_TPIU_OUT_ENB_U_TRACKCLK_IN		(0x1 << 13)
#define CLK_TPIU_SRC_TRACECLKIN			0x634
#define CLK_TPIU_SRC_TRACECLKIN_SRC_MASK	(0x7 << 29)
#define CLK_TPIU_SRC_TRACECLKIN_PLLP		(0x0 << 29)

#define LOCK_MAGIC		0xc5acce55

#define etb_writel(t, v, x)	__raw_writel((v), (t)->etb_regs + (x))
#define etb_readl(t, x)		__raw_readl((t)->etb_regs + (x))
#define etb_regs_lock(t)	etb_writel((t), 0, CORESIGHT_LOCKACCESS)
#define etb_regs_unlock(t)	etb_writel((t), LOCK_MAGIC,		\
						CORESIGHT_LOCKACCESS)

#define funnel_writel(t, v, x)	__raw_writel((v), (t)->funnel_regs + (x))
#define funnel_readl(t, x)	__raw_readl((t)->funnel_regs + (x))
#define funnel_regs_lock(t)	funnel_writel((t), 0, CORESIGHT_LOCKACCESS)
#define funnel_regs_unlock(t)	funnel_writel((t), LOCK_MAGIC,		\
						CORESIGHT_LOCKACCESS)

#define tpiu_writel(t, v, x)	__raw_writel((v), (t)->tpiu_regs + (x))
#define tpiu_readl(t, x)	__raw_readl((t)->tpiu_regs + (x))
#define tpiu_regs_lock(t)	tpiu_writel((t), 0, CORESIGHT_LOCKACCESS)
#define tpiu_regs_unlock(t)	tpiu_writel((t), LOCK_MAGIC,		\
						CORESIGHT_LOCKACCESS)

#define ptm_writel(t, id, v, x)	__raw_writel((v), (t)->ptm_regs[(id)] + (x))
#define ptm_readl(t, id, x)	__raw_readl((t)->ptm_regs[(id)] + (x))
#define ptm_regs_lock(t, id)	ptm_writel((t), (id), 0, CORESIGHT_LOCKACCESS);
#define ptm_regs_unlock(t, id)	ptm_writel((t), (id), LOCK_MAGIC,	\
						CORESIGHT_LOCKACCESS);
#define ptm_os_lock(t, id)	ptm_writel((t), (id), LOCK_MAGIC, PTMMR_OSLAR)
#define ptm_os_unlock(t, id)	ptm_writel((t), (id), 0, PTMMR_OSLAR)

#ifdef CONFIG_TEGRA_PTM
/* PTM required re-energized CPU enterring LP2 mode */
void ptm_power_idle_resume(int cpu);
#else
static inline void ptm_power_idle_resume(int cpu) {}
#endif

#endif
