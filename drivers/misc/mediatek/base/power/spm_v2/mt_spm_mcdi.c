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

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

#include <mach/hotplug.h>
#include <mach/irqs.h>
#include <mach/mt_boot.h>
#include <mach/mt_cirq.h>
#include <mach/mt_cpuidle.h>
#include <mach/mt_gpt.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_misc.h>

#include "mt_spm_internal.h"

/**************************************
 * only for internal debug
 **************************************/
/* FIXME: for FPGA early porting */
/* #define  CONFIG_MTK_LDVT */
#ifdef CONFIG_MTK_LDVT
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define SPM_AEE_RR_REC 0

#define WAKE_SRC_FOR_MCDI (WAKE_SRC_R12_CSYSPWREQ_B | WAKE_SRC_R12_CPU_IRQ_B)
#define WAKE_SRC_FOR_MD32  0	/* (WAKE_SRC_AUD_MD32) */

#define SPM_MCDI_CORE_MAX_EXIT_TIME 100000

/*
* MCDI DVT GPT test
* GPT need to modify mt_idle.c and mt_spm_mcdi.c
*/
#define MCDI_DVT_IPI 0		/*0:disable, 1: enable : mt_idle.c , mt_spm_mcdi.c and mt_cpuidle.c */
#define MCDI_DVT_CPUxGPT 0	/*0:disable, 1: enable : GPT need to modify mt_idle.c and mt_spm_mcdi.c mt_cpuidle.c */
#define MCDI_DVT_W_DORMANT 0	/*1: with dormant, 0: without dormant */


#define __weak __attribute__((weak))
__weak unsigned int mt_get_chip_sw_ver(void) {return 0; }

/* TODO:[fixme:setting] */
static struct pwr_ctrl mcdi_ctrl = {
	.wake_src = WAKE_SRC_FOR_MCDI,
	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.wfi_op = WFI_OP_OR,
	.mcusys_idle_mask = 1,
	.mp0top_idle_mask = 1,
	.mp1top_idle_mask = 1,
	.md1_req_mask_b = 1,
	.md2_req_mask_b = 1,
	.disp_req_mask_b = 1,
	.mfg_req_mask_b = 1,
	.scp_req_mask_b = 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif
#if ((MCDI_DVT_CPUxGPT) || (MCDI_DVT_IPI)) && (!MCDI_DVT_W_DORMANT)
	.pcm_reserve = 0xff,
#endif
};

static const u32 mcdi_binary[] = {
	0x1840001f, 0x00000001, 0x1990001f, 0x10006600, 0x11407c1f, 0xe8208000,
	0x10006614, 0x00000000, 0x1b00001f, 0x21000001, 0x1b80001f, 0xd0010000,
	0x1880001f, 0x10006098, 0x18d0001f, 0x10006098, 0x69200003, 0xbeefbeef,
	0xd8200444, 0x17c07c1f, 0x18c0001f, 0xdeaddead, 0xe0800003, 0x18d0001f,
	0x10006098, 0x69200003, 0xabcdabcd, 0xd82002e4, 0x17c07c1f, 0x1880001f,
	0x10006098, 0x18c0001f, 0x12345678, 0xe0800003, 0x60a07c05, 0xa9100004,
	0x10006530, 0xa9100004, 0x10006534, 0xa9100004, 0x10006538, 0xa9100004,
	0x1000653c, 0xa9100004, 0x10006540, 0xa9100004, 0x10006544, 0xa9100004,
	0x10006548, 0xa9100004, 0x1000654c, 0x80a01002, 0x89100002, 0x10006614,
	0x80801001, 0xd8003ee2, 0x17c07c1f, 0x1a10001f, 0x10006174, 0x82002001,
	0x82201408, 0xd8200968, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x82002001,
	0xd8000948, 0x17c07c1f, 0x1a40001f, 0x10006208, 0x1a80001f, 0x10006248,
	0xc2404300, 0x17c07c1f, 0xa1400405, 0x1a10001f, 0x10006174, 0x8200a001,
	0x82209408, 0xd8200ba8, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x8200a001,
	0xd8000b88, 0x17c07c1f, 0x1a40001f, 0x1000620c, 0x1a80001f, 0x1000624c,
	0xc2404300, 0x17c07c1f, 0xa1508405, 0x1a10001f, 0x10006174, 0x82012001,
	0x82211408, 0xd8200de8, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x82012001,
	0xd8000dc8, 0x17c07c1f, 0x1a40001f, 0x10006210, 0x1a80001f, 0x10006250,
	0xc2404300, 0x17c07c1f, 0xa1510405, 0x1a10001f, 0x10006174, 0x8201a001,
	0x82219408, 0xd8201028, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x8201a001,
	0xd8001008, 0x17c07c1f, 0x1a40001f, 0x10006214, 0x1a80001f, 0x10006254,
	0xc2404300, 0x17c07c1f, 0xa1518405, 0x1a10001f, 0x10006174, 0x88800008,
	0x0000000f, 0x80c01402, 0x6a200003, 0x0000000f, 0x82241408, 0xd8201228,
	0x17c07c1f, 0x1a10001f, 0x10006608, 0x82042001, 0xd8001208, 0x17c07c1f,
	0xa1540405, 0x1a10001f, 0x10006174, 0x82022001, 0x82221408, 0xd8201468,
	0x17c07c1f, 0x1a10001f, 0x10006608, 0x82022001, 0xd8001448, 0x17c07c1f,
	0x1a40001f, 0x1000621c, 0x1a80001f, 0x10006260, 0xc2404300, 0x17c07c1f,
	0xa1520405, 0x1a10001f, 0x10006174, 0x8202a001, 0x82229408, 0xd82016a8,
	0x17c07c1f, 0x1a10001f, 0x10006608, 0x8202a001, 0xd8001688, 0x17c07c1f,
	0x1a40001f, 0x10006220, 0x1a80001f, 0x10006264, 0xc2404300, 0x17c07c1f,
	0xa1528405, 0x1a10001f, 0x10006174, 0x82032001, 0x82231408, 0xd82018e8,
	0x17c07c1f, 0x1a10001f, 0x10006608, 0x82032001, 0xd80018c8, 0x17c07c1f,
	0x1a40001f, 0x10006224, 0x1a80001f, 0x10006268, 0xc2404300, 0x17c07c1f,
	0xa1530405, 0x1a10001f, 0x10006174, 0x8203a001, 0x82239408, 0xd8201b28,
	0x17c07c1f, 0x1a10001f, 0x10006608, 0x8203a001, 0xd8001b08, 0x17c07c1f,
	0x1a40001f, 0x10006228, 0x1a80001f, 0x1000626c, 0xc2404300, 0x17c07c1f,
	0xa1538405, 0x1a10001f, 0x10006174, 0x88800008, 0x000000f0, 0x80c01402,
	0x6a200003, 0x000000f0, 0x82249408, 0xd8201d28, 0x17c07c1f, 0x1a10001f,
	0x10006608, 0x8204a001, 0xd8001d08, 0x17c07c1f, 0xa1548405, 0x1b80001f,
	0x20000550, 0x1a10001f, 0x10006608, 0x820fa001, 0xd8003ea8, 0x17c07c1f,
	0x81041401, 0xd8202044, 0x17c07c1f, 0x1a10001f, 0x10006164, 0x88800008,
	0x00000f0f, 0x71207c02, 0xb1003081, 0xb10c3081, 0xd8202e64, 0x17c07c1f,
	0x1a10001f, 0x10006608, 0x82042001, 0xd8002028, 0x17c07c1f, 0x81740405,
	0x81001401, 0xd82023e4, 0x17c07c1f, 0xc28049c0, 0x1280041f, 0xb10c3081,
	0xd82023e4, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x82002001, 0xd80022a8,
	0x17c07c1f, 0x1a40001f, 0x10006208, 0x1a80001f, 0x10006248, 0xc2404060,
	0x17c07c1f, 0x81700405, 0xe8208000, 0x10006530, 0x00000000, 0xe8208000,
	0x10006500, 0x00000000, 0xe8208000, 0x10006090, 0x00000001, 0x81009401,
	0xd8202764, 0x17c07c1f, 0xc28049c0, 0x1290841f, 0xd8202764, 0x17c07c1f,
	0x1a10001f, 0x10006608, 0x8200a001, 0xd8002628, 0x17c07c1f, 0x1a40001f,
	0x1000620c, 0x1a80001f, 0x1000624c, 0xc2404060, 0x17c07c1f, 0x81708405,
	0xe8208000, 0x10006534, 0x00000000, 0xe8208000, 0x10006504, 0x00000000,
	0xe8208000, 0x10006090, 0x00000002, 0x81011401, 0xd8202ae4, 0x17c07c1f,
	0xc28049c0, 0x1291041f, 0xd8202ae4, 0x17c07c1f, 0x1a10001f, 0x10006608,
	0x82012001, 0xd80029a8, 0x17c07c1f, 0x1a40001f, 0x10006210, 0x1a80001f,
	0x10006250, 0xc2404060, 0x17c07c1f, 0x81710405, 0xe8208000, 0x10006538,
	0x00000000, 0xe8208000, 0x10006508, 0x00000000, 0xe8208000, 0x10006090,
	0x00000004, 0x81019401, 0xd8202e64, 0x17c07c1f, 0xc28049c0, 0x1291841f,
	0xd8202e64, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x8201a001, 0xd8002d28,
	0x17c07c1f, 0x1a40001f, 0x10006214, 0x1a80001f, 0x10006254, 0xc2404060,
	0x17c07c1f, 0x81718405, 0xe8208000, 0x1000653c, 0x00000000, 0xe8208000,
	0x1000650c, 0x00000000, 0xe8208000, 0x10006090, 0x00000008, 0x81049401,
	0xd82030a4, 0x17c07c1f, 0x1a10001f, 0x10006164, 0x88800008, 0x0000f0f0,
	0x71207c02, 0xb1003081, 0xb10c3081, 0xd8203ea4, 0x17c07c1f, 0x1a10001f,
	0x10006608, 0x8204a001, 0xd8003088, 0x17c07c1f, 0x81748405, 0x81021401,
	0xd8203424, 0x17c07c1f, 0xc28049c0, 0x1292041f, 0xd8203424, 0x17c07c1f,
	0x1a10001f, 0x10006608, 0x82022001, 0xd80032e8, 0x17c07c1f, 0x1a40001f,
	0x1000621c, 0x1a80001f, 0x10006260, 0xc2404060, 0x17c07c1f, 0x81720405,
	0xe8208000, 0x10006540, 0x00000000, 0xe8208000, 0x10006510, 0x00000000,
	0xe8208000, 0x10006090, 0x00000010, 0x81029401, 0xd82037a4, 0x17c07c1f,
	0xc28049c0, 0x1292841f, 0xd82037a4, 0x17c07c1f, 0x1a10001f, 0x10006608,
	0x8202a001, 0xd8003668, 0x17c07c1f, 0x1a40001f, 0x10006220, 0x1a80001f,
	0x10006264, 0xc2404060, 0x17c07c1f, 0x81728405, 0xe8208000, 0x10006544,
	0x00000000, 0xe8208000, 0x10006514, 0x00000000, 0xe8208000, 0x10006090,
	0x00000020, 0x81031401, 0xd8203b24, 0x17c07c1f, 0xc28049c0, 0x1293041f,
	0xd8203b24, 0x17c07c1f, 0x1a10001f, 0x10006608, 0x82032001, 0xd80039e8,
	0x17c07c1f, 0x1a40001f, 0x10006224, 0x1a80001f, 0x10006268, 0xc2404060,
	0x17c07c1f, 0x81730405, 0xe8208000, 0x10006548, 0x00000000, 0xe8208000,
	0x10006518, 0x00000000, 0xe8208000, 0x10006090, 0x00000040, 0x81039401,
	0xd8203ea4, 0x17c07c1f, 0xc28049c0, 0x1293841f, 0xd8203ea4, 0x17c07c1f,
	0x1a10001f, 0x10006608, 0x8203a001, 0xd8003d68, 0x17c07c1f, 0x1a40001f,
	0x10006228, 0x1a80001f, 0x1000626c, 0xc2404060, 0x17c07c1f, 0x81738405,
	0xe8208000, 0x1000654c, 0x00000000, 0xe8208000, 0x1000651c, 0x00000000,
	0xe8208000, 0x10006090, 0x00000080, 0xd0000100, 0x17c07c1f, 0xe8208000,
	0x10006614, 0x00000000, 0xe8208000, 0x10006608, 0x00000000, 0x1940001f,
	0xaa55aa55, 0x1b80001f, 0x00001000, 0xf0000000, 0x17c07c1f, 0x1212841f,
	0xe2e00036, 0x17c07c1f, 0x17c07c1f, 0xe2e0003e, 0x1380201f, 0xe2e0003c,
	0xe2a00000, 0x18b0000a, 0x17c07c1f, 0x81040801, 0xd8004164, 0x17c07c1f,
	0xe2e0007c, 0x1b80001f, 0x20000007, 0xe2e0005c, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xe2a00001,
	0x18b0000a, 0x17c07c1f, 0x81040801, 0xd8204384, 0x17c07c1f, 0xe2e0002e,
	0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0x18f00002, 0x17c07c1f,
	0xa0d98403, 0xe0800003, 0x18f00002, 0x17c07c1f, 0x81090c01, 0xd8204544,
	0x17c07c1f, 0x80f98403, 0xe0800003, 0x18f00009, 0x17c07c1f, 0xa0d20403,
	0xe2400003, 0x1a10001f, 0x10006174, 0x82002808, 0xd82046a8, 0x17c07c1f,
	0x1880001f, 0x10001220, 0x18d0001f, 0x10001220, 0xa0d4a803, 0xe0800003,
	0x1a10001f, 0x10001228, 0x8214a808, 0xd8204808, 0x17c07c1f, 0x1880001f,
	0x10200684, 0x18d0001f, 0x10200684, 0xa0ca2803, 0xa0c92803, 0xe0800003,
	0xf0000000, 0x17c07c1f, 0x1a10001f, 0x10006164, 0x81002808, 0xb1142888,
	0xb1003081, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc mcdi_pcm = {
	.version	= "pcm_mcdi_ddrdfs_v6.1_20150629",
	.base		= mcdi_binary,
	.size		= 597,
	.sess		= 1,
	.replace	= 1,
	.addr_2nd	= 0,
};


struct spm_lp_scen __spm_mcdi = {
	.pcmdesc = &mcdi_pcm,
	.pwrctrl = &mcdi_ctrl,
};

static unsigned int spm_mcdi_lock;	/* offload MCDI */
static bool SPM_MCDI_isKICK;

#if SPM_AEE_RR_REC
unsigned int *p_is_mcdi_wfi;
#endif

DEFINE_SPINLOCK(__spm_mcdi_lock);
DEFINE_SPINLOCK(__mcdi_lock);

#if (MCDI_DVT_IPI)
unsigned int core0_IPI_issue_count = 0;
u8 mcdi_enter = 0;
int cpu0_enter_forLoop = 0;
static void empty_function(void *info)
{
	unsigned long flags;
	int cpu = smp_processor_id();

	spin_lock_irqsave(&__mcdi_lock, flags);
	mcdi_enter &= ~(1 << cpu);
	spin_unlock_irqrestore(&__mcdi_lock, flags);
	mdelay(1);
	spm_idle_dbg("core %x ipi received, core IPI command count: %d\n",
		 cpu, core0_IPI_issue_count);
}
#endif

unsigned int g_SPM_MCDI_Abnormal_WakeUp = 0;
unsigned int g_pre_SPM_MCDI_Abnormal_WakeUp = 0;

bool go_to_mcidle(int cpu)
{
	bool ret = 0;

#if (MCDI_DVT_IPI)
	while (1) {
		if (cpu == 0) {
			/* make sure cpu0 enter for loop first */
			cpu0_enter_forLoop = 1;
			/*check except core0 Standby WFI */
			if ((spm_read(CPU_IDLE_STA) & 0xfe) != 0x0) {
				mdelay(1);
				smp_call_function(empty_function, NULL, 0);
				spm_idle_dbg("core0 IPI\n");
				core0_IPI_issue_count++;
			}
		} else {
			if (cpu0_enter_forLoop) {
				if (spm_mcdi_wfi(cpu) == 1) {
					mdelay(1);
					spm_idle_dbg("CPU %d awake\n", cpu);
					ret = 1;
				} else
					return false;
				mdelay(1);
			} else
				return false;
		}
	}
#elif (MCDI_DVT_CPUxGPT)
	while (1) {
		if (cpu != 0) {
			if (spm_mcdi_wfi(cpu) == 1) {
				mdelay(1);
				spm_idle_dbg("CPU %d awake\n", cpu);
				ret = 1;
			} else
				return false;
		}
	}
#else
	if (spm_mcdi_wfi(cpu) == 1)
		ret = 1;

	if (g_SPM_MCDI_Abnormal_WakeUp != g_pre_SPM_MCDI_Abnormal_WakeUp) {
		spm_idle_dbg("SPM-MCDI Abnormal %x\n", g_SPM_MCDI_Abnormal_WakeUp);
		g_pre_SPM_MCDI_Abnormal_WakeUp = g_SPM_MCDI_Abnormal_WakeUp;
	}
#endif
	return ret;
}

static void spm_mcdi_cpu_wake_up_event(bool wake_up_event, bool disable_dormant_power)
{
	/* Use DUMMY1_PWR_CON[1] to decide if offload MCDI firmware */
	if (((spm_read(SPM_CPU_WAKEUP_EVENT) & 0x1) == 1)
	    && ((spm_read(DUMMY1_PWR_CON) & 0x1) == 0)) {
		spm_idle_ver("spm_mcdi_cpu_wake_up_event: SPM_CPU_WAKEUP_EVENT:%x, DUMMY1_PWR_CON %x",
			     spm_read(SPM_CPU_WAKEUP_EVENT), spm_read(DUMMY1_PWR_CON));
		return;
	}
	/* Inform SPM that CPU wants to program CPU_WAKEUP_EVENT and DISABLE_CPU_DROM */
	spm_write(SPM_SCP_MAILBOX, 0xbeefbeef);

	/* Wait SPM's response, can't use sleep api */
	while (spm_read(SPM_SCP_MAILBOX) != 0xdeaddead)
		;

	if (disable_dormant_power) {
		spm_write(DUMMY1_PWR_CON, spm_read(DUMMY1_PWR_CON) | 0x1);
		while (spm_read(DUMMY1_PWR_CON) != (spm_read(DUMMY1_PWR_CON) | 0x1))
			;

	} else {
		spm_write(DUMMY1_PWR_CON, spm_read(DUMMY1_PWR_CON) & ~0x1);
		while (spm_read(DUMMY1_PWR_CON) != (spm_read(DUMMY1_PWR_CON) & ~0x1))
			;
	}

	spm_write(SPM_CPU_WAKEUP_EVENT, wake_up_event);

	while (spm_read(SPM_CPU_WAKEUP_EVENT) != wake_up_event)
		;

	/* Inform SPM to see updated setting */
	spm_write(SPM_SCP_MAILBOX, 0xabcdabcd);

	while (spm_read(SPM_SCP_MAILBOX) != 0x12345678)
		;

	/* END OF sequence */
	spm_write(SPM_SCP_MAILBOX, 0x0);
}

void spm_go_to_mcdi(u32 spm_flags, u32 spm_data)
{
	unsigned long flags;
	struct pcm_desc *pcmdesc = __spm_mcdi.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_mcdi.pwrctrl;

	spin_lock_irqsave(&__spm_mcdi_lock, flags);
	if (SPM_MCDI_isKICK != 0) {
		spin_unlock_irqrestore(&__spm_mcdi_lock, flags);
		return;
	}

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
#if SPM_AEE_RR_REC
	*p_is_mcdi_wfi = 0;
#endif
	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_sync_vcore_dvfs_power_control(pwrctrl, __spm_vcore_dvfs.pwrctrl);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	__spm_kick_pcm_to_run(pwrctrl);

	SPM_MCDI_isKICK = 1;

	spin_unlock_irqrestore(&__spm_mcdi_lock, flags);
	spm_idle_ver("spm_go_to_mcdi()\n");
}

void spm_leave_MCDI(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_mcdi_lock, flags);

	if (SPM_MCDI_isKICK == 0) {
		spin_unlock_irqrestore(&__spm_mcdi_lock, flags);
		return;
	}

	SPM_MCDI_isKICK = 0;

	spm_mcdi_cpu_wake_up_event(1, 1);

#if SPM_AEE_RR_REC
	u32 spm_counter = 0;

	while (*p_is_mcdi_wfi != 0) {	/* don't use sleep command(wfi/wfe) */
		if (spm_counter >= SPM_MCDI_CORE_MAX_EXIT_TIME) {
			spm_idle_ver("spm_leave_MCDI: g_is_mcdi_wfi:%x\n", *p_is_mcdi_wfi);
			spm_counter = 0;
		}
		spm_counter++;
	}
#endif

	/*offload MCDI F/W */
	spm_mcdi_cpu_wake_up_event(1, 0);

	__spm_clean_after_wakeup();

	spin_unlock_irqrestore(&__spm_mcdi_lock, flags);

	spm_idle_ver("spm_leave_MCDI : OK\n");
}

static void spm_mcdi_wfi_sel_enter(int core_id)
{
	int core_id_val = core_id;

	/*SPM WFI Select by core number */
	switch (core_id_val) {
	case 0:
		spm_write(MP0_CPU0_IRQ_MASK, 1);
		spm_write(MP0_CPU0_WFI_EN, 1);

		break;
	case 1:
		spm_write(MP0_CPU1_IRQ_MASK, 1);
		spm_write(MP0_CPU1_WFI_EN, 1);
		break;
	case 2:
		spm_write(MP0_CPU2_IRQ_MASK, 1);
		spm_write(MP0_CPU2_WFI_EN, 1);
		break;
	case 3:
		spm_write(MP0_CPU3_IRQ_MASK, 1);
		spm_write(MP0_CPU3_WFI_EN, 1);
		break;
	case 4:
		spm_write(MP1_CPU0_IRQ_MASK, 1);
		spm_write(MP1_CPU0_WFI_EN, 1);
		break;
	case 5:
		spm_write(MP1_CPU1_IRQ_MASK, 1);
		spm_write(MP1_CPU1_WFI_EN, 1);
		break;
	case 6:
		spm_write(MP1_CPU2_IRQ_MASK, 1);
		spm_write(MP1_CPU2_WFI_EN, 1);
		break;
	case 7:
		spm_write(MP1_CPU3_IRQ_MASK, 1);
		spm_write(MP1_CPU3_WFI_EN, 1);
		break;
	default:
		break;
	}

}

static void spm_mcdi_wfi_sel_leave(int core_id)
{

	int core_id_val = core_id;

	/*SPM WFI Select by core number */
	switch (core_id_val) {
	case 0:
		spm_write(MP0_CPU0_WFI_EN, 0);
		spm_write(MP0_CPU0_IRQ_MASK, 0);
		break;
	case 1:
		spm_write(MP0_CPU1_WFI_EN, 0);
		spm_write(MP0_CPU1_IRQ_MASK, 0);
		break;
	case 2:
		spm_write(MP0_CPU2_WFI_EN, 0);
		spm_write(MP0_CPU2_IRQ_MASK, 0);
		break;
	case 3:
		spm_write(MP0_CPU3_WFI_EN, 0);
		spm_write(MP0_CPU3_IRQ_MASK, 0);
		break;
	case 4:
		spm_write(MP1_CPU0_WFI_EN, 0);
		spm_write(MP1_CPU0_IRQ_MASK, 0);
		break;
	case 5:
		spm_write(MP1_CPU1_WFI_EN, 0);
		spm_write(MP1_CPU1_IRQ_MASK, 0);
		break;
	case 6:
		spm_write(MP1_CPU2_WFI_EN, 0);
		spm_write(MP1_CPU2_IRQ_MASK, 0);
		break;
	case 7:
		spm_write(MP1_CPU3_WFI_EN, 0);
		spm_write(MP1_CPU3_IRQ_MASK, 0);
		break;
	default:
		break;
	}

}

bool spm_is_cpu_irq_occur(int core_id)
{
	bool ret = 0;
	/* check COREn IRQ */
	if (spm_read(SPM_WAKEUP_MISC) & (1 << (core_id)))
		ret = 1;
	/* check COREn FIQ */
	if (spm_read(SPM_WAKEUP_MISC) & (1 << (core_id + 8)))
		ret = 1;

#if (MCDI_DVT_IPI)
	if (ret == 1)
		spm_idle_ver("spm_read(SPM_WAKEUP_MISC)=0x%x\n", spm_read(SPM_WAKEUP_MISC));
#endif

	return ret;
}

bool spm_mcdi_can_enter(void)
{
	bool ret = 1;
	/*check is MCDI kick */
	if ((SPM_MCDI_isKICK == 0) || ((spm_read(SPM_CPU_WAKEUP_EVENT) & 0x1) == 1)
	    || ((spm_read(SPM_SW_RSV_3) & 0x1) == 1))
		ret = 0;

#ifndef CONFIG_CPU_ISOLATION
	if (atomic_read(&is_in_hotplug) >= 1)
		ret = 0;
#endif

#if (MCDI_DVT_IPI)
	if (ret == 0) {
		spm_idle_ver("SPM_MCDI_isKICK=%d\n", SPM_MCDI_isKICK);
		spm_idle_ver("spm_read(SPM_CPU_WAKEUP_EVENT)=%d\n", spm_read(SPM_CPU_WAKEUP_EVENT));
		spm_idle_ver("spm_read(MCU_PWR_CON)=%d\n", spm_read(MCU_PWR_CON));
	}
#endif

	return ret;
}

bool spm_mcdi_wfi(int core_id)
{
	bool ret = 0;
	unsigned long flags;
	int dmnt_ret = 0;
	int mcdi_core_id = core_id;

#if SPM_AEE_RR_REC
	spin_lock_irqsave(&__spm_lock, flags);
	*p_is_mcdi_wfi = (*p_is_mcdi_wfi | (1 << core_id));
	spin_unlock_irqrestore(&__spm_lock, flags);
#endif

	if (spm_mcdi_can_enter() == 0) {
#if SPM_AEE_RR_REC
		spin_lock_irqsave(&__spm_lock, flags);
		*p_is_mcdi_wfi = (*p_is_mcdi_wfi & ~(1 << core_id));
		spin_unlock_irqrestore(&__spm_lock, flags);
#endif

		return ret;
	}

	if ((spm_read(SPM_SW_RSV_0) & (1 << mcdi_core_id)) == 0) {

		if (spm_is_cpu_irq_occur(mcdi_core_id) == 0) {
			/*core wfi_sel & cpu mask */
			spm_mcdi_wfi_sel_enter(core_id);

			/*sync core1~n local timer to XGPT */
			mcidle_before_wfi(core_id);

			dmnt_ret = mt_cpu_dormant_interruptible(CPU_MCDI_MODE);

			if (dmnt_ret == MT_CPU_DORMANT_RESET) {
				ret = 1;

				/*check if MCDI abort by unkonw IRQ */
				while ((spm_read(SPM_IRQ_STA) & (1 << (mcdi_core_id + 4))) == 0) {
					spin_lock_irqsave(&__spm_lock, flags);
					g_SPM_MCDI_Abnormal_WakeUp |= (1 << core_id);
					spin_unlock_irqrestore(&__spm_lock, flags);
				}
			} else if (dmnt_ret == MT_CPU_DORMANT_ABORT)
				ret = 0;

			/*clear core wfi_sel & cpu unmask */
			spm_mcdi_wfi_sel_leave(core_id);

			mcidle_after_wfi(core_id);
			spin_lock_irqsave(&__spm_lock, flags);

			/*Clear SPM SW IRQ */
			spm_write(SPM_SW_INT_CLEAR, (0x1 << core_id));	/* PCM_SWINT_3 */
			spin_unlock_irqrestore(&__spm_lock, flags);
		}
	} else if (spm_is_cpu_irq_occur(core_id) == 0) {
		/*core wfi_sel & cpu mask */
		spm_mcdi_wfi_sel_enter(core_id);

		/*sync core1~n local timer to XGPT */
		mcidle_before_wfi(core_id);

		wfi_with_sync();

		mcidle_after_wfi(core_id);
		/*clear core wfi_sel & cpu unmask */
		spm_mcdi_wfi_sel_leave(core_id);
		/*Clear SPM SW IRQ */
		spm_write(SPM_SW_INT_CLEAR, (0x1 << core_id));	/* PCM_SWINT_3 */

		ret = 1;
	}

#if SPM_AEE_RR_REC
	spin_lock_irqsave(&__spm_lock, flags);
	*p_is_mcdi_wfi = (*p_is_mcdi_wfi & ~(1 << core_id));
	spin_unlock_irqrestore(&__spm_lock, flags);
#endif

	return ret;

}

void spm_set_mcdi_pcm_ver(void)
{
	CHIP_SW_VER ver = mt_get_chip_sw_ver();

	/* Get Different FW */
}

void __init spm_mcdi_init(void)
{
#if SPM_AEE_RR_REC
	p_is_mcdi_wfi = aee_rr_rec_mcdi_wfi();
	*p_is_mcdi_wfi = 0;
#endif

	spm_set_mcdi_pcm_ver();
}

static void __exit spm_mcdi_exit(void)
{
	spm_idle_ver("Exit SPM-MCDI\n\r");
}

#ifndef CONFIG_CPU_ISOLATION
void spm_mcdi_wakeup_all_cores(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_mcdi_lock, flags);
	if (SPM_MCDI_isKICK == 0) {
		spin_unlock_irqrestore(&__spm_mcdi_lock, flags);
		return;
	}

	/* trigger cpu wake up event */
	spm_mcdi_cpu_wake_up_event(1, 1);

	/* trigger cpu wake up event */
	spm_mcdi_cpu_wake_up_event(0, 0);
	spin_unlock_irqrestore(&__spm_mcdi_lock, flags);
}
#endif

static void spm_mcdi_enable(enum spm_mcdi_lock_id id, int mcdi_en)
{
	unsigned long flags;

	if (mcdi_en) {
		spin_lock_irqsave(&__spm_lock, flags);
		spm_mcdi_lock &= ~(1 << id);
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else {
		spin_lock_irqsave(&__spm_lock, flags);
		spm_mcdi_lock |= (1 << id);
		spin_unlock_irqrestore(&__spm_lock, flags);
	}
}

static unsigned int spm_mcdi_is_disable(void)
{
	return spm_mcdi_lock;
}

void spm_mcdi_switch_on_off(enum spm_mcdi_lock_id id, int mcdi_en)
{
	spm_mcdi_enable(id, mcdi_en);
	if (mcdi_en) {
		if (spm_mcdi_is_disable())
			return;
		spm_go_to_mcdi(0, 0);
	} else
		spm_leave_MCDI();
}

MODULE_DESCRIPTION("SPM-MCDI Driver v0.1");
