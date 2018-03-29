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

#ifndef _MT_SPM_MTCMOS_
#define _MT_SPM_MTCMOS_

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/device.h>

#define STA_POWER_DOWN  0
#define STA_POWER_ON    1

#define SPM_POWERON_CONFIG_SET			(0x0000)
#define SPM_POWER_ON_VAL0			(0x0010)
#define SPM_POWER_ON_VAL1			(0x0014)
#define SPM_CLK_SETTLE				(0x0100)
#define SPM_CA7_CPU0_PWR_CON			(0x0200)
#define SPM_CA7_DBG_PWR_CON			(0x0204)
#define SPM_CA7_CPUTOP_PWR_CON			(0x0208)
#define SPM_VDE_PWR_CON				(0x0210)
#define SPM_MFG_PWR_CON				(0x0214)
#define SPM_CA7_CPU1_PWR_CON			(0x0218)
#define SPM_CA7_CPU2_PWR_CON			(0x021c)
#define SPM_CA7_CPU3_PWR_CON			(0x0220)
#define SPM_CA7_CPUTOP_L2_PDN			(0x0244)
#define SPM_CA7_CPUTOP_L2_SLEEP			(0x0248)
#define SPM_CA7_CPU0_L1_PDN			(0x025c)
#define SPM_CA7_CPU1_L1_PDN			(0x0264)
#define SPM_CA7_CPU2_L1_PDN			(0x026c)
#define SPM_CA7_CPU3_L1_PDN			(0x0274)
#define SPM_GCPU_SRAM_CON			(0x027c)
#define SPM_CA15_CPU0_PWR_CON			(0x02a0)
#define SPM_CA15_CPU1_PWR_CON			(0x02a4)
#define SPM_CA15_CPU2_PWR_CON			(0x02a8)
#define SPM_CA15_CPU3_PWR_CON			(0x02ac)
#define SPM_CA15_CPUTOP_PWR_CON			(0x02b0)
#define SPM_CA15_L1_PWR_CON			(0x02b4)
#define SPM_CA15_L2_PWR_CON			(0x02b8)
#define SPM_SLEEP_DUAL_VCORE_PWR_CON		(0x0404)
#define SPM_SLEEP_PTPOD2_CON			(0x0408)
#define SPM_APMCU_PWRCTL			(0x0600)
#define SPM_AP_DVFS_CON_SET			(0x0604)
#define SPM_AP_STANBY_CON			(0x0608)
#define SPM_PWR_STATUS				(0x060c)
#define SPM_PWR_STATUS_2ND			(0x0610)
#define SPM_SLEEP_TIMER_STA			(0x0720)
#define SPM_SLEEP_WAKEUP_EVENT_MASK		(0x0810)
#define SPM_SLEEP_CPU_WAKEUP_EVENT		(0x0814)
#define SPM_SLEEP_ISR_MASK			(0x0900)
#define SPM_SLEEP_ISR_STATUS			(0x0904)
#define SPM_SLEEP_ISR_RAW_STA			(0x0910)
#define SPM_SLEEP_MD32_ISR_RAW_STA		(0x0914)
#define SPM_SLEEP_WAKEUP_MISC			(0x0918)
#define SPM_SLEEP_BUS_PROTECT_RDY		(0x091c)
#define SPM_SLEEP_SUBSYS_IDLE_STA		(0x0920)
#define SPM_CA7_CPU0_IRQ_MASK			(0x0b30)
#define SPM_CA7_CPU1_IRQ_MASK			(0x0b34)
#define SPM_CA7_CPU2_IRQ_MASK			(0x0b38)
#define SPM_CA7_CPU3_IRQ_MASK			(0x0b3c)
#define SPM_CA15_CPU0_IRQ_MASK			(0x0b40)
#define SPM_CA15_CPU1_IRQ_MASK			(0x0b44)
#define SPM_CA15_CPU2_IRQ_MASK			(0x0b48)
#define SPM_CA15_CPU3_IRQ_MASK			(0x0b4c)
#define SPM_SLEEP_CA7_WFI0_EN			(0x0f00)
#define SPM_SLEEP_CA7_WFI1_EN			(0x0f04)
#define SPM_SLEEP_CA7_WFI2_EN			(0x0f08)
#define SPM_SLEEP_CA7_WFI3_EN			(0x0f0c)
#define SPM_SLEEP_CA15_WFI0_EN			(0x0f10)
#define SPM_SLEEP_CA15_WFI1_EN			(0x0f14)
#define SPM_SLEEP_CA15_WFI2_EN			(0x0f18)
#define SPM_SLEEP_CA15_WFI3_EN			(0x0f1c)

#define TOPAXI_PROT_EN				(0x0220)
#define TOPAXI_PROT_STA1			(0x0228)
#define TOPAXI_PROT_EN1				(0x0250)
#define TOPAXI_PROT_STA3			(0x0258)

#define CA15L_MISCDBG				(0x020c)

#define SPM_PROJECT_CODE			(0x0b16)

#define SRAM_ISOINT_B				(1U << 6)
#define SRAM_CKISO				(1U << 5)
#define PWR_CLK_DIS				(1U << 4)
#define PWR_ON_2ND				(1U << 3)
#define PWR_ON					(1U << 2)
#define PWR_ISO					(1U << 1)
#define PWR_RST_B				(1U << 0)

#define L1_PDN_ACK				(1U << 8)
#define L1_PDN					(1U << 0)
#define L2_SRAM_PDN_ACK				(1U << 8)
#define L2_SRAM_PDN				(1U << 0)
#define L2_SRAM_SLEEP_B_ACK			(1U << 8)
#define L2_SRAM_SLEEP_B				(1U << 0)

#define CPU3_CA15_L1_PDN_ACK			(1U << 11)
#define CPU2_CA15_L1_PDN_ACK			(1U << 10)
#define CPU1_CA15_L1_PDN_ACK			(1U <<  9)
#define CPU0_CA15_L1_PDN_ACK			(1U <<  8)
#define CPU3_CA15_L1_PDN_ISO			(1U <<  7)
#define CPU2_CA15_L1_PDN_ISO			(1U <<  6)
#define CPU1_CA15_L1_PDN_ISO			(1U <<  5)
#define CPU0_CA15_L1_PDN_ISO			(1U <<  4)
#define CPU3_CA15_L1_PDN			(1U <<  3)
#define CPU2_CA15_L1_PDN			(1U <<  2)
#define CPU1_CA15_L1_PDN			(1U <<  1)
#define CPU0_CA15_L1_PDN			(1U <<  0)
#define CA15_L2_SLEEPB_ACK			(1U << 10)
#define CA15_L2_PDN_ACK				(1U <<  8)
#define CA15_L2_SLEEPB_ISO			(1U <<  6)
#define CA15_L2_SLEEPB				(1U <<  4)
#define CA15_L2_PDN_ISO				(1U <<  2)
#define CA15_L2_PDN				(1U <<  0)

#define CA15_CPU3				(1U << 19)
#define CA15_CPU2				(1U << 18)
#define CA15_CPU1				(1U << 17)
#define CA15_CPU0				(1U << 16)
#define CA15_CPUTOP				(1U << 15)
#define CA7_DBG					(1U << 13)
#define CA7_CPU3				(1U << 12)
#define CA7_CPU2				(1U << 11)
#define CA7_CPU1				(1U << 10)
#define CA7_CPU0				(1U <<  9)
#define CA7_CPUTOP				(1U <<  8)

#define CA15_CPUTOP_STANDBYWFI			(1U << 25)
#define CA7_CPUTOP_STANDBYWFI			(1U << 24)
#define CA15_CPU3_STANDBYWFI			(1U << 23)
#define CA15_CPU2_STANDBYWFI			(1U << 22)
#define CA15_CPU1_STANDBYWFI			(1U << 21)
#define CA15_CPU0_STANDBYWFI			(1U << 20)
#define CA7_CPU3_STANDBYWFI			(1U << 19)
#define CA7_CPU2_STANDBYWFI			(1U << 18)
#define CA7_CPU1_STANDBYWFI			(1U << 17)
#define CA7_CPU0_STANDBYWFI			(1U << 16)

#define VCA15_PWR_ISO				(1U << 13)
#define VCA7_PWR_ISO				(1U << 12)

#define CA15_PDN_REQ				(30)
#define CA7_PDN_REQ				(29)
#define L2_PDN_REQ				(2)
#define SRAM_PDN				(0xf << 8)

#define CA15L_ACINACTM				(1U << 0)


/*
 * 1. for CPU MTCMOS: CPU0-7, DBG1, CPUSYS1
 * 2. call spm_mtcmos_cpu_lock/unlock() before/after any operations
 */
extern int spm_mtcmos_cpu_init(void);

extern void spm_mtcmos_cpu_lock(unsigned long *flags);
extern void spm_mtcmos_cpu_unlock(unsigned long *flags);

extern int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state,
						int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn);

extern int spm_mtcmos_ctrl_dbg0(int state);

extern int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn);

extern bool spm_cpusys0_can_power_down(void);
extern bool spm_cpusys1_can_power_down(void);

extern void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state);

extern int spm_topaxi_prot(int bit, int en);
extern int spm_topaxi_prot_l2(int bit, int en);

extern int __init mt_spm_mtcmos_init(void);

extern const struct cpu_operations cpu_psci_ops;

#endif
