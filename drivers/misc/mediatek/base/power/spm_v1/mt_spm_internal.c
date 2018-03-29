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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/of_fdt.h>
#include <uapi/asm/setup.h>
#include <mt-plat/upmu_common.h>

#include "mt_spm_internal.h"

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
#include "mt_vcore_dvfs.h"
#endif

void __weak aee_sram_printk(const char *fmt, ...)
{
}

int __weak is_ext_buck_exist(void)
{
	return 0;
}

void __attribute__((weak)) __iomem *spm_get_i2c_base(void)
{
	return NULL;
}

/*
 * Config and Parameter
 */
#define LOG_BUF_SIZE		256


/*
 * Define and Declare
 */
DEFINE_SPINLOCK(__spm_lock);
atomic_t __spm_mainpll_req = ATOMIC_INIT(0);

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
static const char *wakesrc_str[32] = {
	[0] = "SPM_MERGE",
	[1] = "LTE_PTP",
	[2] = "KP",
	[3] = "WDT",
	[4] = "GPT",
	[5] = "EINT",
	[6] = "CONN_WDT",
	[7] = "CCIF0_MD",
	[8] = "CCIF1_MD",
	[9] = "LOW_BAT",
	[10] = "CONN2AP",
	[11] = "F26M_WAKE",
	[12] = "F26M_SLEEP",
	[13] = "PCM_WDT",
	[14] = "USB_CD ",
	[15] = "USB_PDN",
	[16] = "LTE_WAKE",
	[17] = "LTE_SLEEP",
	[18] = "SEJ",
	[19] = "UART0",
	[20] = "AFE",
	[21] = "THERM",
	[22] = "CIRQ",
	[23] = "MD1_VRF18_WAKE",
	[24] = "SYSPWREQ",
	[25] = "MD_WDT",
	[26] = "C2K_WDT",
	[27] = "CLDMA_WDT",
	[28] = "MD1_VRF18_SLEEP",
	[29] = "CPU_IRQ",
	[30] = "APSRC_WAKE",
	[31] = "APSRC_SLEEP",
};
#else /* CONFIG_ARCH_MT6580 */
static const char *wakesrc_str[32] = {
	[0] = "SPM_MERGE",
	[1] = "AUDIO_REQ",
	[2] = "KP",
	[3] = "WDT",
	[4] = "GPT",
	[5] = "EINT",
	[6] = "CONN_WDT",
	[7] = "GCE",
	[8] = "CCIF0_MD",
	[9] = "LOW_BAT",
	[10] = "CONN2AP",
	[11] = "F26M_WAKE",
	[12] = "F26M_SLEE",
	[13] = "PCM_WDT",
	[14] = "USB_CD ",
	[15] = "USB_PDN",
	[16] = "MD1_VRF18_WAKE",
	[17] = "MD1_VRF18_SLEEP",
	[18] = "DBGSYS",
	[19] = "UART0",
	[20] = "AFE",
	[21] = "THERM",
	[22] = "CIRQ",
	[23] = "SEJ",
	[24] = "SYSPWREQ",
	[25] = "MD1_WDT",
	[26] = "CPU0_IRQ",
	[27] = "CPU1_IRQ",
	[28] = "CPU2_IRQ",
	[29] = "CPU3_IRQ",
	[30] = "APSRC_WAKE",
	[31] = "APSRC_SLEEP",
};
#endif

unsigned int __attribute__((weak))
	pmic_read_interface_nolock(unsigned int RegNum,
								unsigned int *val,
								unsigned int MASK,
								unsigned int SHIFT)
{
	return 0;
}

unsigned int __attribute__((weak))
	pmic_config_interface_nolock(unsigned int RegNum,
									unsigned int val,
									unsigned int MASK,
									unsigned int SHIFT)
{
	return 0;
}

/*
 * Function and API
 */
void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc)
{
	u32 con1;

#ifdef SPM_VCORE_EN
	if (spm_read(SPM_PCM_REG1_DATA) == 0x1) {
		/* SPM code swapping (force high voltage) */
		spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 1);
		while (spm_read(SPM_PCM_REG11_DATA) != 0x55AA55AA)
			udelay(1);
		spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0);
	}
#endif

	/* reset PCM */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_PCM_SW_RESET);
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);
	BUG_ON((spm_read(SPM_PCM_FSM_STA) & 0x3fffff) != PCM_FSM_STA_DEF);	/* PCM reset failed */

	/* init PCM_CON0 (disable event vector) */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_IM_SLEEP_DVS);

	/* init PCM_CON1 (disable PCM timer but keep PCM WDT setting) */
	con1 = spm_read(SPM_PCM_CON1) & (CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
	spm_write(SPM_PCM_CON1, con1 | CON1_CFG_KEY | CON1_EVENT_LOCK_EN |
		  CON1_SPM_SRAM_ISO_B | CON1_SPM_SRAM_SLP_B |
		  (pcmdesc->replace ? 0 : CON1_IM_NONRP_EN) |
		  CON1_MIF_APBEN | CON1_MD32_APB_INTERNAL_EN);
}

void __spm_kick_im_to_fetch(const struct pcm_desc *pcmdesc)
{
	u32 ptr, len, con0;

	/* tell IM where is PCM code (use slave mode if code existed) */
	ptr = base_va_to_pa(pcmdesc->base);
	len = pcmdesc->size - 1;
	if (spm_read(SPM_PCM_IM_PTR) != ptr || spm_read(SPM_PCM_IM_LEN) != len || pcmdesc->sess > 2) {
		spm_write(SPM_PCM_IM_PTR, ptr);
		spm_write(SPM_PCM_IM_LEN, len);
	} else {
		spm_write(SPM_PCM_CON1, spm_read(SPM_PCM_CON1) | CON1_CFG_KEY | CON1_IM_SLAVE);
	}

	/* kick IM to fetch (only toggle IM_KICK) */
	con0 = spm_read(SPM_PCM_CON0) & ~(CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY | CON0_IM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY);
}

void __spm_init_pcm_register(void)
{
	/* init r0 with POWER_ON_VAL0 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL0));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R0);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* init r7 with POWER_ON_VAL1 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(SPM_PCM_PWR_IO_EN, 0);
}

void __spm_init_event_vector(const struct pcm_desc *pcmdesc)
{
	/* init event vector register */
	spm_write(SPM_PCM_EVENT_VECTOR0, pcmdesc->vec0);
	spm_write(SPM_PCM_EVENT_VECTOR1, pcmdesc->vec1);
	spm_write(SPM_PCM_EVENT_VECTOR2, pcmdesc->vec2);
	spm_write(SPM_PCM_EVENT_VECTOR3, pcmdesc->vec3);
	spm_write(SPM_PCM_EVENT_VECTOR4, pcmdesc->vec4);
	spm_write(SPM_PCM_EVENT_VECTOR5, pcmdesc->vec5);
	spm_write(SPM_PCM_EVENT_VECTOR6, pcmdesc->vec6);
	spm_write(SPM_PCM_EVENT_VECTOR7, pcmdesc->vec7);

	/* event vector will be enabled by PCM itself */
}

void __spm_set_power_control(const struct pwr_ctrl *pwrctrl)
{
	/* set other SYS request mask */
	spm_write(SPM_AP_STANBY_CON, (!pwrctrl->md_vrf18_req_mask_b << 29) |
		  (!pwrctrl->lte_mask << 26) |
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		  (spm_read(SPM_AP_STANBY_CON) & ASC_SRCCLKENI_MASK) |
#else
		  (!pwrctrl->srclkenai_mask << 25) |
#endif
		  (!!pwrctrl->md2_apsrc_sel << 24) |
		  (!pwrctrl->conn_mask << 23) |
		  (!!pwrctrl->md_apsrc_sel << 22) |
		  (!pwrctrl->md32_req_mask << 21) |
		  (!pwrctrl->md2_req_mask << 20) |
		  (!pwrctrl->md1_req_mask << 19) |
		  (!pwrctrl->gce_req_mask << 18) |
		  (!pwrctrl->mfg_req_mask << 17) |
		  (!pwrctrl->disp_req_mask << 16) |
		  (!!pwrctrl->mcusys_idle_mask << 7) |
		  (!!pwrctrl->ca15top_idle_mask << 6) |
		  (!!pwrctrl->ca7top_idle_mask << 5) | (!!pwrctrl->wfi_op << 4));
	spm_write(SPM_PCM_SRC_REQ, (!pwrctrl->ccifmd_md2_event_mask << 8) |
		  (!pwrctrl->ccifmd_md1_event_mask << 7) |
		  (!pwrctrl->ccif1_to_ap_mask << 5) |
		  (!pwrctrl->ccif1_to_md_mask << 4) |
		  (!pwrctrl->ccif0_to_ap_mask << 3) |
		  (!pwrctrl->ccif0_to_md_mask << 2) |
		  (!!pwrctrl->pcm_f26m_req << 1) | (!!pwrctrl->pcm_apsrc_req << 0));
	spm_write(SPM_PCM_PASR_DPD_2, (!pwrctrl->isp1_ddr_en_mask << 4) |
		  (!pwrctrl->isp0_ddr_en_mask << 3) |
		  (!pwrctrl->dpi_ddr_en_mask << 2) |
		  (!pwrctrl->dsi1_ddr_en_mask << 1) | (!pwrctrl->dsi0_ddr_en_mask << 0));

	/* set CPU WFI mask */
	spm_write(SPM_SLEEP_CA15_WFI0_EN, !!pwrctrl->ca15_wfi0_en);
	spm_write(SPM_SLEEP_CA15_WFI1_EN, !!pwrctrl->ca15_wfi1_en);
	spm_write(SPM_SLEEP_CA15_WFI2_EN, !!pwrctrl->ca15_wfi2_en);
	spm_write(SPM_SLEEP_CA15_WFI3_EN, !!pwrctrl->ca15_wfi3_en);
	spm_write(SPM_SLEEP_CA7_WFI0_EN, !!pwrctrl->ca7_wfi0_en);
	spm_write(SPM_SLEEP_CA7_WFI1_EN, !!pwrctrl->ca7_wfi1_en);
	spm_write(SPM_SLEEP_CA7_WFI2_EN, !!pwrctrl->ca7_wfi2_en);
	spm_write(SPM_SLEEP_CA7_WFI3_EN, !!pwrctrl->ca7_wfi3_en);
}

void __spm_set_wakeup_event(const struct pwr_ctrl *pwrctrl)
{
	u32 val, mask, isr;

	/* set PCM timer (set to max when disable) */
	if (pwrctrl->timer_val_cust == 0)
		val = pwrctrl->timer_val ? : PCM_TIMER_MAX;
	else
		val = pwrctrl->timer_val_cust;

	spm_write(SPM_PCM_TIMER_VAL, val);
	spm_write(SPM_PCM_CON1, spm_read(SPM_PCM_CON1) | CON1_CFG_KEY | CON1_PCM_TIMER_EN);

	/* unmask AP wakeup source */
	if (pwrctrl->wake_src_cust == 0)
		mask = pwrctrl->wake_src;
	else
		mask = pwrctrl->wake_src_cust;

	if (pwrctrl->syspwreq_mask)
		mask &= ~WAKE_SRC_SYSPWREQ;
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~mask);

	/* unmask SPM ISR (keep TWAM setting) */
	isr = spm_read(SPM_SLEEP_ISR_MASK) & ISRM_TWAM;
	spm_write(SPM_SLEEP_ISR_MASK, isr | ISRM_RET_IRQ_AUX);
}

void __spm_kick_pcm_to_run(const struct pwr_ctrl *pwrctrl)
{
	u32 con0;

	/* init register to match PCM expectation */
	spm_write(SPM_PCM_MAS_PAUSE_MASK, 0xffffffff);
	spm_write(SPM_PCM_REG_DATA_INI, 0);
	/* spm_write(SPM_CLK_CON, spm_read(SPM_CLK_CON) & ~CC_DISABLE_DORM_PWR); */

	/* set PCM flags and data */
	spm_write(SPM_PCM_FLAGS, pwrctrl->pcm_flags);
	spm_write(SPM_PCM_RESERVE, pwrctrl->pcm_reserve);

	/* lock Infra DCM when PCM runs */
	spm_write(SPM_CLK_CON, (spm_read(SPM_CLK_CON) & ~CC_LOCK_INFRA_DCM) |
		  (pwrctrl->infra_dcm_lock ? CC_LOCK_INFRA_DCM : 0));

	/* enable r0 and r7 to control power */
	spm_write(SPM_PCM_PWR_IO_EN, (pwrctrl->r0_ctrl_en ? PCM_PWRIO_EN_R0 : 0) |
		  (pwrctrl->r7_ctrl_en ? PCM_PWRIO_EN_R7 : 0));

	/* kick PCM to run (only toggle PCM_KICK) */
	con0 = spm_read(SPM_PCM_CON0) & ~(CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY);
}

void __spm_get_wakeup_status(struct wake_status *wakesta)
{
	/* get PC value if PCM assert (pause abort) */
	wakesta->assert_pc = spm_read(SPM_PCM_REG_DATA_INI);

	/* get wakeup event */
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	wakesta->r12 = spm_read(SPM_PCM_RESERVE3);
#else
	wakesta->r12 = spm_read(SPM_PCM_REG12_DATA);
#endif

	wakesta->raw_sta = spm_read(SPM_SLEEP_ISR_RAW_STA);
	wakesta->wake_misc = spm_read(SPM_SLEEP_WAKEUP_MISC);

	/* get sleep time */
	wakesta->timer_out = spm_read(SPM_PCM_TIMER_OUT);

	/* get other SYS and co-clock status */
	wakesta->r13 = spm_read(SPM_PCM_REG13_DATA);
	wakesta->idle_sta = spm_read(SPM_SLEEP_SUBSYS_IDLE_STA);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = spm_read(SPM_PCM_RESERVE4);

	/* get special pattern (0xf0000 or 0x10000) if sleep abort */
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	wakesta->event_reg = spm_read(SPM_PCM_PASR_DPD_2);
#else
	wakesta->event_reg = spm_read(SPM_PCM_EVENT_REG_STA);
#endif

	/* get ISR status */
	wakesta->isr = spm_read(SPM_SLEEP_ISR_STATUS);
}

void __spm_clean_after_wakeup(void)
{
	/* disable r0 and r7 to control power */
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* clean CPU wakeup event */
	spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0);

	/* clean PCM timer event */
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | (spm_read(SPM_PCM_CON1) & ~CON1_PCM_TIMER_EN));

	/* clean wakeup event raw status (for edge trigger event) */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~0);

	/* clean ISR status (except TWAM) */
	spm_write(SPM_SLEEP_ISR_MASK, spm_read(SPM_SLEEP_ISR_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_SLEEP_ISR_STATUS, ISRC_ALL_EXC_TWAM);
	spm_write(SPM_PCM_SW_INT_CLEAR, PCM_SW_INT_ALL);
}

#define spm_print(suspend, fmt, args...)	\
do {						\
	if (!suspend)				\
		spm_debug(fmt, ##args);		\
	else					\
		spm_crit2(fmt, ##args);		\
} while (0)

wake_reason_t __spm_output_wake_reason(const struct wake_status *wakesta,
				       const struct pcm_desc *pcmdesc, bool suspend)
{
	int i;
	char buf[LOG_BUF_SIZE] = { 0 };
	wake_reason_t wr = WR_UNKNOWN;

	if (wakesta->assert_pc != 0) {
		spm_print(suspend, "PCM ASSERT AT %u (%s), r13 = 0x%x, debug_flag = 0x%x\n",
			  wakesta->assert_pc, pcmdesc->version, wakesta->r13, wakesta->debug_flag);
		return WR_PCM_ASSERT;
	}

	if (wakesta->r12 & WAKE_SRC_SPM_MERGE) {
		if (wakesta->wake_misc & WAKE_MISC_PCM_TIMER) {
			strcat(buf, " PCM_TIMER");
			wr = WR_PCM_TIMER;
		}
		if (wakesta->wake_misc & WAKE_MISC_TWAM) {
			strcat(buf, " TWAM");
			wr = WR_WAKE_SRC;
		}
		if (wakesta->wake_misc & WAKE_MISC_CPU_WAKE) {
			strcat(buf, " CPU");
			wr = WR_WAKE_SRC;
		}
	}
	for (i = 1; i < 32; i++) {
		if (wakesta->r12 & (1U << i)) {
			if ((strlen(buf) + strlen(wakesrc_str[i])) < LOG_BUF_SIZE)
				strncat(buf, wakesrc_str[i], strlen(wakesrc_str[i]));

			wr = WR_WAKE_SRC;
		}
	}
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
	BUG_ON(strlen(buf) >= LOG_BUF_SIZE);
#endif

	spm_warn("wake up by%s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x\n",
		  buf, wakesta->timer_out, wakesta->r13, wakesta->debug_flag);

	spm_warn(
		  "r12 = 0x%x, raw_sta = 0x%x, idle_sta = 0x%x, event_reg = 0x%x, isr = 0x%x\n",
		  wakesta->r12, wakesta->raw_sta, wakesta->idle_sta, wakesta->event_reg,
		  wakesta->isr);

	return wr;
}

void __spm_dbgout_md_ddr_en(bool enable)
{
	/* set TEST_MODE_CFG */
	spm_write(0xf0000230, (spm_read(0xf0000230) & ~(0x7fff << 16)) |
		  (0x3 << 26) | (0x3 << 21) | (0x3 << 16));

	/* set md_ddr_en to GPIO150 */
	spm_write(0xf0001500, 0x70e);
	spm_write(0xf00057e4, 0x7);

	/* set emi_clk_off_req to GPIO140 */
	spm_write(0xf000150c, 0x3fe);
	spm_write(0xf00057c4, 0x7);

	/* enable debug output */
	spm_write(SPM_PCM_DEBUG_CON, !!enable);
}

#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
u32 __spm_dpidle_sodi_set_pmic_setting(void)
{
	u32 vsram_vosel_on_lb = 0;
	u32 vsram_vosel_offset = 0;
	u32 vsram_vosel_delta = 0;

	pmic_read_interface_nolock(MT6328_PMIC_VSRAM_VOSEL_ON_LB_ADDR,
								&vsram_vosel_on_lb,
								MT6328_PMIC_VSRAM_VOSEL_ON_LB_MASK,
								MT6328_PMIC_VSRAM_VOSEL_ON_LB_SHIFT);
	pmic_read_interface_nolock(MT6328_PMIC_VSRAM_VOSEL_OFFSET_ADDR,
								&vsram_vosel_offset,
								MT6328_PMIC_VSRAM_VOSEL_OFFSET_MASK,
								MT6328_PMIC_VSRAM_VOSEL_OFFSET_SHIFT);
	pmic_read_interface_nolock(MT6328_PMIC_VSRAM_VOSEL_DELTA_ADDR,
								&vsram_vosel_delta,
								MT6328_PMIC_VSRAM_VOSEL_DELTA_MASK,
								MT6328_PMIC_VSRAM_VOSEL_DELTA_SHIFT);

	/* delta = 0v */
	spm_write(SPM_PCM_RESERVE3, (vsram_vosel_offset << 8) | vsram_vosel_delta);

	/* 0.85v */
	pmic_config_interface_nolock(MT6328_PMIC_VSRAM_VOSEL_ON_LB_ADDR,
									(vsram_vosel_on_lb & 0xff80) | 0x28,
									MT6328_PMIC_VSRAM_VOSEL_ON_LB_MASK,
									MT6328_PMIC_VSRAM_VOSEL_ON_LB_SHIFT);

	return vsram_vosel_on_lb;
}

void __spm_dpidle_sodi_restore_pmic_setting(u32 vsram_vosel_on_lb)
{
	pmic_config_interface_nolock(MT6328_PMIC_VSRAM_VOSEL_ON_LB_ADDR,
									vsram_vosel_on_lb,
									MT6328_PMIC_VSRAM_VOSEL_ON_LB_MASK,
									MT6328_PMIC_VSRAM_VOSEL_ON_LB_SHIFT);
}

#ifdef CONFIG_ARCH_MT6753
#include <mach/mt_clkmgr.h>
#include <linux/i2c.h>
#include "mt_i2c.h"
static u32 i2c4_conf_backup;
static u32 i2c4_conf_backup1;
void __spm_enable_i2c4_clk(void)
{
	if (is_ext_buck_exist()) {
		i2c4_base = spm_get_i2c_base();

		if (i2c4_base == NULL)
			BUG();

		enable_clock(MT_CG_PERI_I2C4, "suspend");

		/* Backup I2C setting */
		i2c4_conf_backup = spm_read(i2c4_base + 0x10);
		spm_write(i2c4_base + 0x10, (i2c4_conf_backup & ~(0x1 << 4)));
		i2c4_conf_backup1 = spm_read(i2c4_base + 0x48);
		spm_write(i2c4_base + 0x48, (i2c4_conf_backup1 & ~(0x1)));
	}
}

void __spm_disable_i2c4_clk(void)
{
	if (is_ext_buck_exist()) {
		i2c4_base = spm_get_i2c_base();

		if (i2c4_base == NULL)
			BUG();

		/* Restore I2C setting */
		spm_write(i2c4_base + 0x48, i2c4_conf_backup1);
		spm_write(i2c4_base + 0x10, i2c4_conf_backup);

		disable_clock(MT_CG_PERI_I2C4, "suspend");
	}
}
#endif

static u32 spm_dram_dummy_read_flags;
#ifdef CONFIG_OF
static int dt_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	const __be32 *reg;
	const struct dram_info *dram_info = NULL;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	reg = (const __be32 *)of_get_flat_dt_prop(node, "reg", NULL);
	if (reg == NULL)
		return 0;

	if (node) {
		/* orig_dram_info */
		dram_info = (const struct dram_info *)of_get_flat_dt_prop(node, "orig_dram_info", NULL);
	}

	if (dram_info) {
		if ((dram_info->rank_info[1].start == 0x60000000)
		    || (dram_info->rank_info[1].start == 0x70000000))
			spm_dram_dummy_read_flags |= SPM_DRAM_RANK1_ADDR_SEL0;
		else if (dram_info->rank_info[1].start == 0x80000000)
			spm_dram_dummy_read_flags |= SPM_DRAM_RANK1_ADDR_SEL1;
		else if (dram_info->rank_info[1].start == 0xc0000000)
			spm_dram_dummy_read_flags |= SPM_DRAM_RANK1_ADDR_SEL2;
		else if (dram_info->rank_info[1].start == 0xa0000000)
			spm_dram_dummy_read_flags |= SPM_DRAM_RANK1_ADDR_SEL3;
		else if (dram_info->rank_info[1].size != 0x0) {
			pr_err("dram rank1_info_error: no rank info\n");
			BUG_ON(1);
		}
	}

	return node;
}
#endif


void spm_set_dram_bank_info_pcm_flag(u32 *pcm_flags)
{
	*pcm_flags |= spm_dram_dummy_read_flags;
}


bool spm_set_pcm_init_flag(void)
{
#ifdef CONFIG_OF
	int node;

	node = of_scan_flat_dt(dt_scan_memory, NULL);

#else
	pr_err("dram rank1_info_error: no rank info\n");
	BUG_ON(1);
#endif

	return true;
}
#endif /* !defined(CONFIG_ARCH_MT6580) */

MODULE_DESCRIPTION("SPM-Internal Driver v0.1");
