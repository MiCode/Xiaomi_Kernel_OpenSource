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
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <asm/setup.h>
#include "mt_spm_internal.h"
#include "mt_vcorefs_governor.h"
#include "mt_spm_vcore_dvfs.h"
#include "mt_spm_misc.h"
#include <mt-plat/upmu_common.h>
#if defined(CONFIG_ARCH_MT6797)
#include <camera_isp.h>
#endif
#if defined(CONFIG_ARCH_MT6797)
#include <scp_helper.h>
#endif

#if defined(CONFIG_ARCH_MT6797)
int __attribute__((weak)) check_scp_resource(void)
{
	return 0;
}
#endif

/**************************************
 * Config and Parameter
 **************************************/
#define LOG_BUF_SIZE		256

#define SPM_WAKE_PERIOD         600	/* sec */

#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757)
/* CPU_PWR_STATUS */
/* CPU_PWR_STATUS_2ND */
#define MP0_CPU0                (1U <<  9)
#define MP0_CPU1                (1U << 10)
#define MP0_CPU2                (1U << 11)
#define MP0_CPU3                (1U << 12)
#define MP1_CPU0                (1U << 16)
#define MP1_CPU1                (1U << 17)
#define MP1_CPU2                (1U << 18)
#define MP1_CPU3                (1U << 19)
#elif defined(CONFIG_ARCH_MT6797)
/* CPU_PWR_STATUS */
/* CPU_PWR_STATUS_2ND */
#define MP0_CPU0                (1U << 15)
#define MP0_CPU1                (1U << 14)
#define MP0_CPU2                (1U << 13)
#define MP0_CPU3                (1U << 12)
#define MP1_CPU0                (1U << 11)
#define MP1_CPU1                (1U << 10)
#define MP1_CPU2                (1U <<  9)
#define MP1_CPU3                (1U <<  8)
#define MP2_CPU0                (1U <<  7)
#define MP2_CPU1                (1U <<  6)
#define MP2_CPU2                (1U <<  5)
#define MP2_CPU3                (1U <<  4)
#define MP3_CPU0                (1U <<  3)
#define MP3_CPU1                (1U <<  2)
#define MP3_CPU2                (1U <<  1)
#define MP3_CPU3                (1U <<  0)
#endif

/**************************************
 * Define and Declare
 **************************************/
DEFINE_SPINLOCK(__spm_lock);
atomic_t __spm_mainpll_req = ATOMIC_INIT(0);

static u32 pcm_timer_ramp_max = 1;
static u32 pcm_timer_ramp_max_sec_loop = 1;

const char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_MD32_WDT_EVENT_B",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_MD32_SPM_IRQ_B",
	[11] = " R12_26M_WAKE",
	[12] = " R12_26M_SLEEP",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USB_CDSC_B",
	[15] = " R12_USB_POWERDWN_B",
	[16] = " R12_C2K_WDT_IRQ_B",
	[17] = " R12_EINT_EVENT_SECURE_B",
	[18] = " R12_CCIF1_EVENT_B",
	[19] = " R12_UART0_IRQ_B",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERM_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_MD2_WDT_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_CLDMA_EVENT_B",
	[27] = " R12_SEJ_WDT_GPT_B",
	[28] = " R12_ALL_MD32_WAKEUP_B",
	[29] = " R12_CPU_IRQ_B",
	[30] = " R12_APSRC_WAKE",
	[31] = " R12_APSRC_SLEEP",
};

const char *twam_event_str[32] = {
	[0] = "SRCCLKENI_0",
	[1] = "SRCCLKENI_1",
	[2] = "SRCCLKENO_0",
	[3] = "SRCCLKENO_1",
	[4] = "MD1_SRCCLKENA",
	[5] = "MD1_APSRC_REQ",
	[6] = "MD1_DDR_EN",
	[7] = "MD2_SRCCLKENA",
	[8] = "MD2_APSRC_REQ",
	[9] = "MD2_DDR_EN",
	[10] = "CONN_SRCCLKENA",
	[11] = "CONN_APSRC_REQ",
	[12] = "SCP_SRCCLKENA",
	[13] = "SCP_APSRC_REQ",
	[14] = "DISP_REQ",
	[15] = "MFG_REQ",
	[16] = "MD1_VRF18_REQ",
	[17] = "C2K_VRF18_REQ",
	[18] = "LTE_SRCCLKENA",
	[19] = "APSRC_ACK",
	[20] = "SRCCLKENA_ACK",
	[21] = "VRF18_SRC_ACK",
	[22] = "INFRA_SRC_ACK",
	[23] = "26M_STATE",
	[24] = "INFRA_STATE",
	[25] = "APSRC_STATE",
	[26] = "VRF18_STATE",
	[27] = "DISP_STATE",
	[28] = "EMI_CLK_OFF_REQ",
	[29] = "EMI_CLK_OFF_ACK",
	[30] = "DDREN_STATE",
	[31] = "CONN_DDR_EN",
};

#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757)
#define SPM_CPU_PWR_STATUS		PWR_STATUS
#define SPM_CPU_PWR_STATUS_2ND	PWR_STATUS_2ND

unsigned int spm_cpu_bitmask[NR_CPUS] = {
	MP0_CPU0,
	MP0_CPU1,
	MP0_CPU2,
	MP0_CPU3,
	MP1_CPU0,
	MP1_CPU1,
	MP1_CPU2,
	MP1_CPU3,
};

unsigned int spm_cpu_bitmask_all = MP0_CPU0 |
									MP0_CPU1 |
									MP0_CPU2 |
									MP0_CPU3 |
									MP1_CPU0 |
									MP1_CPU1 |
									MP1_CPU2 | MP1_CPU3;
#elif defined(CONFIG_ARCH_MT6797)
#define SPM_CPU_PWR_STATUS		CPU_PWR_STATUS
#define SPM_CPU_PWR_STATUS_2ND	CPU_PWR_STATUS_2ND

/* FIXME: use `NR_CPUS` after CONFIG_NR_CPUS workaround fixed */
unsigned int spm_cpu_bitmask[10] = {
	MP0_CPU0,
	MP0_CPU1,
	MP0_CPU2,
	MP0_CPU3,
	MP1_CPU0,
	MP1_CPU1,
	MP1_CPU2,
	MP1_CPU3,
	MP2_CPU0,
	MP2_CPU1
};

unsigned int spm_cpu_bitmask_all = MP0_CPU0 |
									MP0_CPU1 |
									MP0_CPU2 |
									MP0_CPU3 |
									MP1_CPU0 |
									MP1_CPU1 |
									MP1_CPU2 |
									MP1_CPU3 |
									MP2_CPU0 | MP2_CPU1;

static u32 scp_request_freq;

u32 is_scp_request_freq(void)
{
	return scp_request_freq;
}
#endif

/**************************************
 * Function and API
 **************************************/
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757)
void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc)
{
	u32 con1;

	int retry = 0, timeout = 2000;
	u32 save_r1, save_r15, save_pcm_sta, save_irq_sta;
	u32 final_r15, final_pcm_sta;

	/* [Vcorefs] backup r0 to POWER_ON_VAL0 for MEM Ctrl should work during PCM reset */
	if (spm_read(PCM_REG1_DATA) == 0x1) {
		save_r1  = spm_read(PCM_REG1_DATA);
		save_r15 = spm_read(PCM_REG15_DATA);
		save_pcm_sta = spm_read(PCM_FSM_STA);
		save_irq_sta = spm_read(SPM_IRQ_STA);
		con1 = spm_read(SPM_WAKEUP_EVENT_MASK);

		if (save_r15 != 0) {
			spm_write(SPM_WAKEUP_EVENT_MASK, (con1 & ~(0x1)));
			spm_write(SPM_CPU_WAKEUP_EVENT, 1);
			while ((spm_read(SPM_IRQ_STA) & PCM_IRQ_ROOT_MASK_LSB) == 0) {
				if (retry > timeout) {
					pr_err("[VcoreFS] init state: r15=0x%x r1=0x%x pcmsta=0x%x irqsta=0x%x\n",
						save_r15, save_r1, save_pcm_sta, save_irq_sta);
					pr_err("[VcoreFS] CPU waiting F/W ack fail, PCM_FSM_STA: 0x%x, timeout: %d\n",
						spm_read(PCM_FSM_STA), timeout);
					pr_err("[VcoreFS] curr state: r15=0x%x r6=0x%x pcmsta=0x%x irqsta=0x%x\n",
						spm_read(PCM_REG15_DATA), spm_read(PCM_REG6_DATA),
						spm_read(PCM_FSM_STA), spm_read(SPM_IRQ_STA));

					__check_dvfs_halt_source(__spm_vcore_dvfs.pwrctrl->dvfs_halt_src_chk);
					final_r15     = spm_read(PCM_REG15_DATA);
					final_pcm_sta = spm_read(PCM_FSM_STA);
					pr_err("[VcoreFS] next state: r15=0x%x r6=0x%x pcmsta=0x%x irqsta=0x%x\n",
						final_r15, spm_read(PCM_REG6_DATA),
						final_pcm_sta, spm_read(SPM_IRQ_STA));

				if (final_r15 == 0 && (final_pcm_sta & 0xFFFF) == 0x8490)
					break;

				pr_err("[VcoreFS] can not reset without idle(r15=0x%x pcm_sta=0x%x)\n",
					final_r15, final_pcm_sta);

				}
				udelay(1);
				retry++;
			}
		spm_write(SPM_CPU_WAKEUP_EVENT, 0);
		spm_write(SPM_WAKEUP_EVENT_MASK, con1);
		}

		/* backup mem control from r0 to POWER_ON_VAL0 */
		if (spm_read(SPM_POWER_ON_VAL0) != spm_read(PCM_REG0_DATA)) {
			spm_crit("VAL0 from 0x%x to 0x%x\n", spm_read(SPM_POWER_ON_VAL0), spm_read(PCM_REG0_DATA));
			spm_write(SPM_POWER_ON_VAL0, spm_read(PCM_REG0_DATA));
		}

		/* disable r0 and r7 to control power */
		spm_write(PCM_PWR_IO_EN, 0);

		/* [Vcorefs] disable pcm timer after leaving FW */
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~PCM_TIMER_EN_LSB));
	}

	/* reset PCM */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | PCM_SW_RESET_LSB);
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
	if ((spm_read(PCM_FSM_STA) & 0x7fffff) != PCM_FSM_STA_DEF) {
		spm_crit2("reset pcm(PCM_FSM_STA=0x%x\n", spm_read(PCM_FSM_STA));
		BUG(); /* PCM reset failed */
	}

	/* init PCM_CON0 (disable event vector) */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | EN_IM_SLEEP_DVS_LSB);

	/* init PCM_CON1 (disable PCM timer but keep PCM WDT setting) */
	con1 = spm_read(PCM_CON1) & (PCM_WDT_WAKE_MODE_LSB | PCM_WDT_EN_LSB);
	spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | EVENT_LOCK_EN_LSB |
		  SPM_SRAM_ISOINT_B_LSB | SPM_SRAM_SLEEP_B_LSB |
		  (pcmdesc->replace ? 0 : IM_NONRP_EN_LSB) |
		  MIF_APBEN_LSB | SCP_APB_INTERNAL_EN_LSB);
}

#elif defined(CONFIG_ARCH_MT6797)
void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc)
{
	u32 con1;
	int retry = 0, timeout = 5000;
	u32 save_r1, save_r15, save_pcm_sta, save_irq_sta, dvfs_en;

	/* [Vcorefs] backup r0 to POWER_ON_VAL0 for MEM Ctrl should work during PCM reset */
	if (spm_read(PCM_REG1_DATA) == 0x1) {
		if (!vcorefs_sodi_rekick_lock()) {
			dvfs_en = !is_vcorefs_dvfs_enable();
			save_r1  = spm_read(PCM_REG1_DATA);
			save_r15 = spm_read(PCM_REG15_DATA);
			save_pcm_sta = spm_read(PCM_FSM_STA);
			save_irq_sta = spm_read(SPM_IRQ_STA);
			con1 = spm_read(SPM_WAKEUP_EVENT_MASK);
			spm_write(SPM_WAKEUP_EVENT_MASK, (con1 & ~(0x1)));

			/* don't change vcore and dram when dvfs enable reture false */
			spm_write(SPM_SW_RSV_1, (spm_read(SPM_SW_RSV_1) & (~0xF)) | SPM_OFFLOAD);
			spm_write(SPM_CPU_WAKEUP_EVENT, 1);

			if (is_vcorefs_feature_enable()) {
				__spm_backup_vcore_dvfs_dram_shuffle();
				while ((spm_read(SPM_IRQ_STA) & PCM_IRQ_ROOT_MASK_LSB) == 0) {
					if (retry > timeout) {
						pr_err("[VcoreFS] init state: r15=0x%x r1=0x%x pcmsta=0x%x irqsta=0x%x\n",
							save_r15, save_r1, save_pcm_sta, save_irq_sta);
						pr_err("[VcoreFS] CPU waiting F/W ack fail, PCM_FSM_STA: 0x%x, timeout: %d\n",
									spm_read(PCM_FSM_STA), timeout);
						pr_err("[VcoreFS] curr state: r15=0x%x r6=0x%x pcmsta=0x%x irqsta=0x%x\n",
							spm_read(PCM_REG15_DATA), spm_read(PCM_REG6_DATA),
							spm_read(PCM_FSM_STA), spm_read(SPM_IRQ_STA));

						spm_vcorefs_dump_dvfs_regs(NULL);
						BUG();
					}
					udelay(1);
					retry++;
				}
			}

#if SPM_AEE_RR_REC
			aee_rr_rec_spm_common_scenario_val(0);
#endif
			spm_write(SPM_CPU_WAKEUP_EVENT, 0);
			spm_write(SPM_WAKEUP_EVENT_MASK, con1);
			spm_write(SPM_SW_RSV_1, (spm_read(SPM_SW_RSV_1) & (~0xF)) | SPM_CLEAN_WAKE_EVENT_DONE);
		}

		/* backup mem control from r0 to POWER_ON_VAL0 */
		if (spm_read(SPM_POWER_ON_VAL0) != spm_read(PCM_REG0_DATA)) {
			spm_crit("VAL0 from 0x%x to 0x%x\n", spm_read(SPM_POWER_ON_VAL0), spm_read(PCM_REG0_DATA));
			spm_write(SPM_POWER_ON_VAL0, spm_read(PCM_REG0_DATA));
		}

		/* disable r0 and r7 to control power */
		spm_write(PCM_PWR_IO_EN, 0);

		/* [Vcorefs] disable pcm timer after leaving FW */
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~PCM_TIMER_EN_LSB));

		/* backup vcore state from REG6[24:23] to RSV_5[24:23] */
		spm_write(SPM_SW_RSV_5, (spm_read(SPM_SW_RSV_5) & ~(0x3 << 23)) |
					(spm_read(PCM_REG6_DATA) & SPM_VCORE_STA_REG));
	}

	/* reset PCM */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | PCM_SW_RESET_LSB);
	retry = 0;
	while ((spm_read(PCM_FSM_STA) & 0x7fffff) != PCM_FSM_STA_DEF) {
		if (retry > timeout) {
			spm_crit2("reset pcm PCM_FSM_STA=0x%x\n", spm_read(PCM_FSM_STA));
			BUG(); /* PCM reset failed */
		}
		udelay(1);
		retry++;
	}
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);

	/* init PCM_CON0 (disable event vector) */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | EN_IM_SLEEP_DVS_LSB);

	/* init PCM_CON1 (disable PCM timer but keep PCM WDT setting) */
	con1 = spm_read(PCM_CON1) & (PCM_WDT_WAKE_MODE_LSB | PCM_WDT_EN_LSB);
	spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | EVENT_LOCK_EN_LSB |
		  SPM_SRAM_ISOINT_B_LSB | SPM_SRAM_SLEEP_B_LSB |
		  (pcmdesc->replace ? 0 : IM_NONRP_EN_LSB) |
		  MIF_APBEN_LSB | SCP_APB_INTERNAL_EN_LSB);

	/* clear SPM_SW_INT after scenario change */
	spm_write(SPM_SW_INT_CLEAR, PCM_SW_INT0);
}

bool is_vcorefs_fw(bool dynamic_load)
{
	struct pcm_desc *pcmdesc;
	u32 ptr, len;

	if (dynamic_load) {
		u32 vcorefs_idx = spm_get_pcm_vcorefs_index();

		if (dyna_load_pcm[vcorefs_idx].ready) {
			pcmdesc = &(dyna_load_pcm[vcorefs_idx].desc);
		} else {
			pr_err("[%s] dyna load F/W fail\n", __func__);
			BUG();
		}
	} else {
		pcmdesc = __spm_vcore_dvfs.pcmdesc;
	}

	/* tell IM where is PCM code (use slave mode if code existed) */
	if (pcmdesc->base_dma) {
		ptr = pcmdesc->base_dma;
		/* for 4GB mode */
		if (enable_4G())
			MAPPING_DRAM_ACCESS_ADDR(ptr);
	} else {
		ptr = base_va_to_pa(pcmdesc->base);
	}
	len = pcmdesc->size - 1;

	if (spm_read(PCM_IM_PTR) != ptr || spm_read(PCM_IM_LEN) != len)
		return false;
	else
		return true;
}
#endif

void __spm_kick_im_to_fetch(const struct pcm_desc *pcmdesc)
{
	u32 ptr, len, con0;

	/* tell IM where is PCM code (use slave mode if code existed) */
	if (pcmdesc->base_dma) {
		ptr = pcmdesc->base_dma;
		/* for 4GB mode */
		if (enable_4G())
			MAPPING_DRAM_ACCESS_ADDR(ptr);
	} else {
		ptr = base_va_to_pa(pcmdesc->base);
	}
	len = pcmdesc->size - 1;
	if (spm_read(PCM_IM_PTR) != ptr || spm_read(PCM_IM_LEN) != len || pcmdesc->sess > 2) {
		spm_write(PCM_IM_PTR, ptr);
		spm_write(PCM_IM_LEN, len);
	} else {
		spm_write(PCM_CON1, spm_read(PCM_CON1) | SPM_REGWR_CFG_KEY | IM_SLAVE_LSB);
	}

	/* kick IM to fetch (only toggle IM_KICK) */
	con0 = spm_read(PCM_CON0) & ~(IM_KICK_L_LSB | PCM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | IM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
}

void __spm_init_pcm_register(void)
{
	/* init r0 with POWER_ON_VAL0 */
	spm_write(PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL0));
	spm_write(PCM_PWR_IO_EN, PCM_RF_SYNC_R0);
	spm_write(PCM_PWR_IO_EN, 0);

	/* init r7 with POWER_ON_VAL1 */
	spm_write(PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(PCM_PWR_IO_EN, 0);
}

void __spm_init_event_vector(const struct pcm_desc *pcmdesc)
{
	/* init event vector register */
	spm_write(PCM_EVENT_VECTOR0, pcmdesc->vec0);
	spm_write(PCM_EVENT_VECTOR1, pcmdesc->vec1);
	spm_write(PCM_EVENT_VECTOR2, pcmdesc->vec2);
	spm_write(PCM_EVENT_VECTOR3, pcmdesc->vec3);
	spm_write(PCM_EVENT_VECTOR4, pcmdesc->vec4);
	spm_write(PCM_EVENT_VECTOR5, pcmdesc->vec5);
	spm_write(PCM_EVENT_VECTOR6, pcmdesc->vec6);
	spm_write(PCM_EVENT_VECTOR7, pcmdesc->vec7);
	spm_write(PCM_EVENT_VECTOR8, pcmdesc->vec8);
	spm_write(PCM_EVENT_VECTOR9, pcmdesc->vec9);
	spm_write(PCM_EVENT_VECTOR10, pcmdesc->vec10);
	spm_write(PCM_EVENT_VECTOR11, pcmdesc->vec11);
	spm_write(PCM_EVENT_VECTOR12, pcmdesc->vec12);
	spm_write(PCM_EVENT_VECTOR13, pcmdesc->vec13);
	spm_write(PCM_EVENT_VECTOR14, pcmdesc->vec14);
	spm_write(PCM_EVENT_VECTOR15, pcmdesc->vec15);

	/* event vector will be enabled by PCM itself */
}

#if defined(CONFIG_ARCH_MT6757)
void __spm_set_power_control(const struct pwr_ctrl *pwrctrl)
{
	/* SPM_AP_STANDBY_CON */
	spm_write(SPM_AP_STANDBY_CON,
		((pwrctrl->wfi_op & 0x1) << 0) |
		((pwrctrl->mp0_cputop_idle_mask & 0x1) << 1) |
		((pwrctrl->mp1_cputop_idle_mask & 0x1) << 2) |
		((pwrctrl->mcusys_idle_mask & 0x1) << 4) |
		((pwrctrl->mm_mask_b & 0x3) << 16) |
		((pwrctrl->md_ddr_en_dbc_en & 0x1) << 18) |
		((pwrctrl->md_mask_b & 0x3) << 19) |
		((pwrctrl->scp_mask_b & 0x1) << 21) |
		((pwrctrl->lte_mask_b & 0x1) << 22) |
		((pwrctrl->srcclkeni_mask_b & 0x1) << 23) |
		((pwrctrl->md_apsrc_1_sel & 0x1) << 24) |
		((pwrctrl->md_apsrc_0_sel & 0x1) << 25) |
		((pwrctrl->conn_mask_b & 0x1) << 26) |
		((pwrctrl->conn_apsrc_sel & 0x1) << 27));

	/* SPM_SRC_REQ */
	spm_write(SPM_SRC_REQ,
		((pwrctrl->spm_apsrc_req & 0x1) << 0) |
		((pwrctrl->spm_f26m_req & 0x1) << 1) |
		((pwrctrl->spm_lte_req & 0x1) << 2) |
		((pwrctrl->spm_infra_req & 0x1) << 3) |
		((pwrctrl->spm_vrf18_req & 0x1) << 4) |
		((pwrctrl->spm_dvfs_req & 0x1) << 5) |
		((pwrctrl->spm_dvfs_force_down & 0x1) << 6) |
		((pwrctrl->spm_ddren_req & 0x1) << 7) |
		((pwrctrl->spm_rsv_src_req & 0x7) << 8) |
		((pwrctrl->cpu_md_dvfs_sop_force_on & 0x1) << 16));

	/* SPM_SRC_MASK */
	spm_write(SPM_SRC_MASK,
		((pwrctrl->csyspwreq_mask & 0x1) << 0) |
		((pwrctrl->ccif0_md_event_mask_b & 0x1) << 1) |
		((pwrctrl->ccif0_ap_event_mask_b & 0x1) << 2) |
		((pwrctrl->ccif1_md_event_mask_b & 0x1) << 3) |
		((pwrctrl->ccif1_ap_event_mask_b & 0x1) << 4) |
		((pwrctrl->ccifmd_md1_event_mask_b & 0x1) << 5) |
		((pwrctrl->ccifmd_md2_event_mask_b & 0x1) << 6) |
		((pwrctrl->dsi0_vsync_mask_b & 0x1) << 7) |
		((pwrctrl->dsi1_vsync_mask_b & 0x1) << 8) |
		((pwrctrl->dpi_vsync_mask_b & 0x1) << 9) |
		((pwrctrl->isp0_vsync_mask_b & 0x1) << 10) |
		((pwrctrl->isp1_vsync_mask_b & 0x1) << 11) |
		((pwrctrl->md_srcclkena_0_infra_mask_b & 0x1) << 12) |
		((pwrctrl->md_srcclkena_1_infra_mask_b & 0x1) << 13) |
		((pwrctrl->conn_srcclkena_infra_mask_b & 0x1) << 14) |
		((pwrctrl->md32_srcclkena_infra_mask_b & 0x1) << 15) |
		((pwrctrl->srcclkeni_infra_mask_b & 0x1) << 16) |
		((pwrctrl->md_apsrc_req_0_infra_mask_b & 0x1) << 17) |
		((pwrctrl->md_apsrc_req_1_infra_mask_b & 0x1) << 18) |
		((pwrctrl->conn_apsrcreq_infra_mask_b & 0x1) << 19) |
		((pwrctrl->md32_apsrcreq_infra_mask_b & 0x1) << 20) |
		((pwrctrl->md_ddr_en_0_mask_b & 0x1) << 21) |
		((pwrctrl->md_ddr_en_1_mask_b & 0x1) << 22) |
		((pwrctrl->md_vrf18_req_0_mask_b & 0x1) << 23) |
		((pwrctrl->md_vrf18_req_1_mask_b & 0x1) << 24) |
		((pwrctrl->md1_dvfs_req_mask & 0x3) << 25) |
		((pwrctrl->cpu_dvfs_req_mask & 0x1) << 27) |
		((pwrctrl->emi_bw_dvfs_req_mask & 0x1) << 28) |
		((pwrctrl->md_srcclkena_0_dvfs_req_mask_b & 0x1) << 29) |
		((pwrctrl->md_srcclkena_1_dvfs_req_mask_b & 0x1) << 30) |
		((pwrctrl->conn_srcclkena_dvfs_req_mask_b & 0x1) << 31));

	/* SPM_SRC2_MASK */
	spm_write(SPM_SRC2_MASK,
		((pwrctrl->dvfs_halt_mask_b & 0x1f) << 0) |
		((pwrctrl->vdec_req_mask_b & 0x1) << 6) |
		((pwrctrl->gce_req_mask_b & 0x1) << 7) |
		((pwrctrl->cpu_md_dvfs_req_merge_mask_b & 0x1) << 8) |
		((pwrctrl->md_ddr_en_dvfs_halt_mask_b & 0x3) << 9) |
		((pwrctrl->dsi0_vsync_dvfs_halt_mask_b & 0x1) << 11) |
		((pwrctrl->dsi1_vsync_dvfs_halt_mask_b & 0x1) << 12) |
		((pwrctrl->dpi_vsync_dvfs_halt_mask_b & 0x1) << 13) |
		((pwrctrl->isp0_vsync_dvfs_halt_mask_b & 0x1) << 14) |
		((pwrctrl->isp1_vsync_dvfs_halt_mask_b & 0x1) << 15) |
		((pwrctrl->conn_ddr_en_mask_b & 0x1) << 16) |
		((pwrctrl->disp_req_mask_b & 0x1) << 17) |
		((pwrctrl->disp1_req_mask_b & 0x1) << 18) |
		((pwrctrl->mfg_req_mask_b & 0x1) << 19) |
		((pwrctrl->c2k_ps_rccif_wake_mask_b & 0x1) << 20) |
		((pwrctrl->c2k_l1_rccif_wake_mask_b & 0x1) << 21) |
		((pwrctrl->ps_c2k_rccif_wake_mask_b & 0x1) << 22) |
		((pwrctrl->l1_c2k_rccif_wake_mask_b & 0x1) << 23) |
		((pwrctrl->sdio_on_dvfs_req_mask_b & 0x1) << 24) |
		((pwrctrl->emi_boost_dvfs_req_mask_b & 0x1) << 25) |
		((pwrctrl->cpu_md_emi_dvfs_req_prot_dis & 0x1) << 26) |
		((pwrctrl->dramc_spcmd_apsrc_req_mask_b & 0x1) << 27));

	/* SW_CRTL_EVENT */
	spm_write(SW_CRTL_EVENT, (pwrctrl->sw_ctrl_event_on & 0x1) << 0);

	/* SPM_SW_RSV_6 */
	spm_write(SPM_SW_RSV_6,
		((pwrctrl->md_srcclkena_0_2d_dvfs_req_mask_b & 0x1) << 0) |
		((pwrctrl->md_srcclkena_1_2d_dvfs_req_mask_b & 0x1) << 1) |
		((pwrctrl->dvfs_up_2d_dvfs_req_mask_b & 0x1) << 2) |
		((pwrctrl->disable_off_load_lpm & 0x1) << 3));

#if 0
	/* SPM_WAKEUP_EVENT_MASK */
	spm_write(SPM_WAKEUP_EVENT_MASK,
		((pwrctrl->spm_wakeup_event_mask & 0xffffffff) << 0));

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	spm_write(SPM_WAKEUP_EVENT_EXT_MASK,
		((pwrctrl->spm_wakeup_event_ext_mask & 0xffffffff) << 0));
#endif

	spm_write(SPM_CLK_CON, (spm_read(SPM_CLK_CON) & ~CC_SRCLKENA_MASK_0) |
			(pwrctrl->srclkenai_mask ? CC_SRCLKENA_MASK_0 : 0));

	/* set CPU WFI mask */
	spm_write(MP1_CPU0_WFI_EN, !!pwrctrl->mp1_cpu0_wfi_en);
	spm_write(MP1_CPU1_WFI_EN, !!pwrctrl->mp1_cpu1_wfi_en);
	spm_write(MP1_CPU2_WFI_EN, !!pwrctrl->mp1_cpu2_wfi_en);
	spm_write(MP1_CPU3_WFI_EN, !!pwrctrl->mp1_cpu3_wfi_en);
	spm_write(MP0_CPU0_WFI_EN, !!pwrctrl->mp0_cpu0_wfi_en);
	spm_write(MP0_CPU1_WFI_EN, !!pwrctrl->mp0_cpu1_wfi_en);
	spm_write(MP0_CPU2_WFI_EN, !!pwrctrl->mp0_cpu2_wfi_en);
	spm_write(MP0_CPU3_WFI_EN, !!pwrctrl->mp0_cpu3_wfi_en);
}
#else
void __spm_set_power_control(const struct pwr_ctrl *pwrctrl)
{
	/* set other SYS request mask */
	spm_write(SPM_AP_STANDBY_CON, (!!pwrctrl->conn_apsrc_sel << 27) |
			(!!pwrctrl->conn_mask_b << 26) |
			(!!pwrctrl->md_apsrc0_sel << 25) |
			(!!pwrctrl->md_apsrc1_sel << 24) |
			(spm_read(SPM_AP_STANDBY_CON) & SRCCLKENI_MASK_B_LSB) | /* bit23 */
			(!!pwrctrl->lte_mask_b << 22) |
			(!!pwrctrl->scp_req_mask_b << 21) |
			(!!pwrctrl->md2_req_mask_b << 20) |
			(!!pwrctrl->md1_req_mask_b << 19) |
			(!!pwrctrl->md_ddr_dbc_en << 18) |
#if defined(CONFIG_ARCH_MT6797)
			(!!pwrctrl->mcusys_idle_mask << 6) |
			(!!pwrctrl->mptop_idle_mask << 5) |
			(!!pwrctrl->mp3top_idle_mask << 4) |
			(!!pwrctrl->mp2top_idle_mask << 3) |
#else
			(!!pwrctrl->mcusys_idle_mask << 4) |
#endif
			(!!pwrctrl->mp1top_idle_mask << 2) |
			(!!pwrctrl->mp0top_idle_mask << 1) |
			(!!pwrctrl->wfi_op << 0));
#if defined(CONFIG_ARCH_MT6797)
#if  !defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	spm_write(SPM_AP_STANDBY_CON, spm_read(SPM_AP_STANDBY_CON) & ~SCP_MASK_B_LSB);
#endif
#endif

#if defined(CONFIG_ARCH_MT6755)
	spm_write(SPM_SRC_REQ, (!!pwrctrl->cpu_md_dvfs_sop_force_on << 16) |
			(!!pwrctrl->spm_flag_run_common_scenario << 10) |
			(!!pwrctrl->spm_flag_dis_vproc_vsram_dvs << 9) |
			(!!pwrctrl->spm_flag_keep_csyspwrupack_high << 8) |
			(!!pwrctrl->spm_ddren_req << 7) |
			(!!pwrctrl->spm_dvfs_force_down << 6) |
			(!!pwrctrl->spm_dvfs_req << 5) |
			(!!pwrctrl->spm_vrf18_req << 4) |
			(!!pwrctrl->spm_infra_req << 3) |
			(!!pwrctrl->spm_lte_req << 2) |
			(!!pwrctrl->spm_f26m_req << 1) |
			(!!pwrctrl->spm_apsrc_req << 0));
#elif defined(CONFIG_ARCH_MT6797)
	{
		u8 spm_vrf18_req = pwrctrl->spm_vrf18_req;
		u8 spm_infra_req = pwrctrl->spm_infra_req;
		u8 spm_f26m_req = pwrctrl->spm_f26m_req;

		if ((is_check_scp_freq_req() == 1) && check_scp_resource()) {
			scp_request_freq = 1;

			spm_vrf18_req = 1;
			spm_infra_req = 1;
			spm_f26m_req = 1;
		} else {
			scp_request_freq = 0;
		}

		spm_write(SPM_SRC_REQ, (!!pwrctrl->cpu_md_dvfs_sop_force_on << 16) |
				(!!pwrctrl->spm_flag_run_common_scenario << 10) |
				(!!pwrctrl->spm_flag_dis_vproc_vsram_dvs << 9) |
				(!!pwrctrl->spm_flag_keep_csyspwrupack_high << 8) |
				(!!pwrctrl->spm_ddren_req << 7) |
				(!!pwrctrl->spm_dvfs_force_down << 6) |
				(!!pwrctrl->spm_dvfs_req << 5) |
				(!!spm_vrf18_req << 4) |
				(!!spm_infra_req << 3) |
				(!!pwrctrl->spm_lte_req << 2) |
				(!!spm_f26m_req << 1) |
				(!!pwrctrl->spm_apsrc_req << 0));
	}
#endif

	spm_write(SPM_SRC_MASK,
			(!!pwrctrl->conn_srcclkena_dvfs_req_mask_b << 31) |
			(!!pwrctrl->md_srcclkena_1_dvfs_req_mask_b << 30) |
			(!!pwrctrl->md_srcclkena_0_dvfs_req_mask_b << 29) |
			(!!pwrctrl->emi_bw_dvfs_req_mask << 28) |
#if defined(CONFIG_ARCH_MT6797)
			(!!pwrctrl->cpu_dvfs_req_mask << 27) |
			(!!pwrctrl->md1_dvfs_req_mask << 25) |
#endif
			(!!pwrctrl->md_vrf18_req_1_mask_b << 24) |
			(!!pwrctrl->md_vrf18_req_0_mask_b << 23) |
			(!!pwrctrl->md_ddr_en_1_mask_b << 22) |
			(!!pwrctrl->md_ddr_en_0_mask_b << 21) |
			(!!pwrctrl->md32_apsrcreq_infra_mask_b << 20) |
			(!!pwrctrl->conn_apsrcreq_infra_mask_b << 19) |
			(!!pwrctrl->md_apsrcreq_1_infra_mask_b << 18) |
			(!!pwrctrl->md_apsrcreq_0_infra_mask_b << 17) |
			(!!pwrctrl->srcclkeni_infra_mask_b << 16) |
			(!!pwrctrl->md32_srcclkena_infra_mask_b << 15) |
			(!!pwrctrl->conn_srcclkena_infra_mask_b << 14) |
			(!!pwrctrl->md_srcclkena_1_infra_mask_b << 13) |
			(!!pwrctrl->md_srcclkena_0_infra_mask_b << 12) |
			((pwrctrl->vsync_mask_b & 0x1f) << 7) |
			(!!pwrctrl->ccifmd_md2_event_mask_b << 6) |
			(!!pwrctrl->ccifmd_md1_event_mask_b << 5) |
			(!!pwrctrl->ccif1_to_ap_mask_b << 4) |
			(!!pwrctrl->ccif1_to_md_mask_b << 3) |
			(!!pwrctrl->ccif0_to_ap_mask_b << 2) |
			(!!pwrctrl->ccif0_to_md_mask_b << 1));

	spm_write(SPM_SRC2_MASK,
#if defined(CONFIG_ARCH_MT6797)
			(!!pwrctrl->disp_od_req_mask_b << 27) |
#endif
			(!!pwrctrl->cpu_md_emi_dvfs_req_prot_dis << 26) |
			(!!pwrctrl->emi_boost_dvfs_req_mask_b << 25) |
			(!!pwrctrl->sdio_on_dvfs_req_mask_b << 24) |
			(!!pwrctrl->l1_c2k_rccif_wake_mask_b << 23) |
			(!!pwrctrl->ps_c2k_rccif_wake_mask_b << 22) |
			(!!pwrctrl->c2k_l1_rccif_wake_mask_b << 21) |
			(!!pwrctrl->c2k_ps_rccif_wake_mask_b << 20) |
			(!!pwrctrl->mfg_req_mask_b << 19) |
			(!!pwrctrl->disp1_req_mask_b << 18) |
			(!!pwrctrl->disp_req_mask_b << 17) |
			(!!pwrctrl->conn_ddr_en_mask_b << 16) |
			((pwrctrl->vsync_dvfs_halt_mask_b & 0x1f) << 11) |	/* 5bit */
			(!!pwrctrl->md2_ddr_en_dvfs_halt_mask_b << 10) |
			(!!pwrctrl->md1_ddr_en_dvfs_halt_mask_b << 9) |
			(!!pwrctrl->cpu_md_dvfs_erq_merge_mask_b << 8) |
			(!!pwrctrl->gce_req_mask_b << 7) |
			(!!pwrctrl->vdec_req_mask_b << 6) |
			((pwrctrl->dvfs_halt_mask_b & 0x1f) << 0));	/* 5bit */

	spm_write(SPM_CLK_CON, (spm_read(SPM_CLK_CON) & ~CC_SRCLKENA_MASK_0) |
			(pwrctrl->srclkenai_mask ? CC_SRCLKENA_MASK_0 : 0));

	/* set CPU WFI mask */
#if defined(CONFIG_ARCH_MT6797)
	spm_write(MP2_CPU0_WFI_EN, !!pwrctrl->mp2_cpu0_wfi_en);
	spm_write(MP2_CPU1_WFI_EN, !!pwrctrl->mp2_cpu1_wfi_en);
#endif
	spm_write(MP1_CPU0_WFI_EN, !!pwrctrl->mp1_cpu0_wfi_en);
	spm_write(MP1_CPU1_WFI_EN, !!pwrctrl->mp1_cpu1_wfi_en);
	spm_write(MP1_CPU2_WFI_EN, !!pwrctrl->mp1_cpu2_wfi_en);
	spm_write(MP1_CPU3_WFI_EN, !!pwrctrl->mp1_cpu3_wfi_en);
	spm_write(MP0_CPU0_WFI_EN, !!pwrctrl->mp0_cpu0_wfi_en);
	spm_write(MP0_CPU1_WFI_EN, !!pwrctrl->mp0_cpu1_wfi_en);
	spm_write(MP0_CPU2_WFI_EN, !!pwrctrl->mp0_cpu2_wfi_en);
	spm_write(MP0_CPU3_WFI_EN, !!pwrctrl->mp0_cpu3_wfi_en);
}
#endif

void __spm_set_vcorefs_wakeup_event(const struct pwr_ctrl *src_pwr_ctrl)
{
	u32 mask;

	mask = src_pwr_ctrl->wake_src;

	spm_write(SPM_WAKEUP_EVENT_MASK, ~mask);
}

void __spm_set_wakeup_event(const struct pwr_ctrl *pwrctrl)
{
	u32 val, mask, isr;

	/* set PCM timer (set to max when disable) */
	if (pwrctrl->timer_val_ramp_en != 0) {
		val = pcm_timer_ramp_max;

		pcm_timer_ramp_max++;

		if (pcm_timer_ramp_max >= 300)
			pcm_timer_ramp_max = 1;
	} else if (pwrctrl->timer_val_ramp_en_sec != 0) {
		val = pcm_timer_ramp_max * 1600;	/* 50ms */

		pcm_timer_ramp_max += 1;
		if (pcm_timer_ramp_max >= 300)	/* max 15 sec */
			pcm_timer_ramp_max = 1;

		pcm_timer_ramp_max_sec_loop++;
		if (pcm_timer_ramp_max_sec_loop >= 50) {
			pcm_timer_ramp_max_sec_loop = 0;
			/* range 6min to 10min */
			val = (pcm_timer_ramp_max + 300) * 32000;
		}
	} else {
		if (pwrctrl->timer_val_cust == 0)
			val = pwrctrl->timer_val ? : PCM_TIMER_MAX;
		else
			val = pwrctrl->timer_val_cust;
	}

	spm_write(PCM_TIMER_VAL, val);
	spm_write(PCM_CON1, spm_read(PCM_CON1) | SPM_REGWR_CFG_KEY | PCM_TIMER_EN_LSB);

	/* unmask AP wakeup source */
	if (pwrctrl->wake_src_cust == 0)
		mask = pwrctrl->wake_src;
	else
		mask = pwrctrl->wake_src_cust;

#if defined(CONFIG_ARCH_MT6757)
	if (pwrctrl->csyspwreq_mask)
#else
	if (pwrctrl->syspwreq_mask)
#endif
		mask &= ~WAKE_SRC_R12_CSYSPWREQ_B;
	spm_write(SPM_WAKEUP_EVENT_MASK, ~mask);

#if 0
	/* unmask MD32 wakeup source */
	spm_write(SPM_SLEEP_MD32_WAKEUP_EVENT_MASK, ~pwrctrl->wake_src_md32);
#endif

	/* unmask SPM ISR (keep TWAM setting) */
	isr = spm_read(SPM_IRQ_MASK) & SPM_TWAM_IRQ_MASK_LSB;
	spm_write(SPM_IRQ_MASK, isr | ISRM_RET_IRQ_AUX);
}

void __spm_kick_pcm_to_run(const struct pwr_ctrl *pwrctrl)
{
	u32 con0;

	/* init register to match PCM expectation */
	spm_write(SPM_MAS_PAUSE_MASK_B, 0xffffffff);
	spm_write(SPM_MAS_PAUSE2_MASK_B, 0xffffffff);
	spm_write(PCM_REG_DATA_INI, 0);

	/* set PCM flags and data */
	spm_write(SPM_SW_FLAG, pwrctrl->pcm_flags);
	spm_write(SPM_SW_RSV_0, pwrctrl->pcm_reserve);

	/* lock Infra DCM when PCM runs */
	spm_write(SPM_CLK_CON, (spm_read(SPM_CLK_CON) & ~SPM_LOCK_INFRA_DCM_LSB) |
		  (pwrctrl->infra_dcm_lock ? SPM_LOCK_INFRA_DCM_LSB : 0));

	/* enable r0 and r7 to control power */
	spm_write(PCM_PWR_IO_EN, (pwrctrl->r0_ctrl_en ? PCM_PWRIO_EN_R0 : 0) |
		  (pwrctrl->r7_ctrl_en ? PCM_PWRIO_EN_R7 : 0));

	/* kick PCM to run (only toggle PCM_KICK) */
	con0 = spm_read(PCM_CON0) & ~(IM_KICK_L_LSB | PCM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | PCM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
}

void __spm_get_wakeup_status(struct wake_status *wakesta)
{
	/* get PC value if PCM assert (pause abort) */
	wakesta->assert_pc = spm_read(PCM_REG_DATA_INI);

	/* get wakeup event */
	wakesta->r12 = spm_read(SPM_SW_RSV_0);
	wakesta->r12_ext = spm_read(PCM_REG12_EXT_DATA);
	wakesta->raw_sta = spm_read(SPM_WAKEUP_STA);
	wakesta->raw_ext_sta = spm_read(SPM_WAKEUP_EXT_STA);
	wakesta->wake_misc = spm_read(SPM_BSI_D0_SR);	/* backup of SLEEP_WAKEUP_MISC */

	/* get sleep time */
	wakesta->timer_out = spm_read(SPM_BSI_D1_SR);	/* backup of PCM_TIMER_OUT */

	/* get other SYS and co-clock status */
	wakesta->r13 = spm_read(PCM_REG13_DATA);
	wakesta->idle_sta = spm_read(SUBSYS_IDLE_STA);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = spm_read(SPM_SW_DEBUG);

	/* get special pattern (0xf0000 or 0x10000) if sleep abort */
	wakesta->event_reg = spm_read(SPM_BSI_D2_SR);	/* PCM_EVENT_REG_STA */

	/* get ISR status */
	wakesta->isr = spm_read(SPM_IRQ_STA);
}

void __spm_clean_after_wakeup(void)
{
	/* [Vcorefs] can not switch back to POWER_ON_VAL0 here,
	   the FW stays in VCORE DVFS which use r0 to Ctrl MEM */
	/* disable r0 and r7 to control power */
	/* spm_write(PCM_PWR_IO_EN, 0); */

	/* clean CPU wakeup event */
	spm_write(SPM_CPU_WAKEUP_EVENT, 0);

	/* [Vcorefs] not disable pcm timer here, due to the
	   following vcore dvfs will use it for latency check */
	/* clean PCM timer event */
	/* spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~PCM_TIMER_EN_LSB)); */

	/* clean wakeup event raw status (for edge trigger event) */
	spm_write(SPM_WAKEUP_EVENT_MASK, ~0);

	/* clean ISR status (except TWAM) */
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_IRQ_STA, ISRC_ALL_EXC_TWAM);
#if defined(CONFIG_ARCH_MT6757)
	spm_write(SPM_SWINT_CLR, PCM_SW_INT_ALL);
#else
	spm_write(SPM_SW_INT_CLEAR, PCM_SW_INT_ALL);
#endif
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
		/* add size check for vcoredvfs */
		spm_print(suspend, "PCM ASSERT AT %u (%s%s), r13 = 0x%x, debug_flag = 0x%x\n",
			  wakesta->assert_pc, (wakesta->assert_pc > pcmdesc->size) ? "NOT " : "",
			  pcmdesc->version, wakesta->r13, wakesta->debug_flag);
		return WR_PCM_ASSERT;
	}

	if (wakesta->r12 & WAKE_SRC_R12_PCM_TIMER) {
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
	BUG_ON(strlen(buf) >= LOG_BUF_SIZE);

	spm_print(suspend, "wake up by%s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x\n",
		  buf, wakesta->timer_out, wakesta->r13, wakesta->debug_flag);

	spm_print(suspend,
		  "r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x, idle_sta = 0x%x, event_reg = 0x%x, isr = 0x%x\n",
		  wakesta->r12, wakesta->r12_ext, wakesta->raw_sta, wakesta->idle_sta,
		  wakesta->event_reg, wakesta->isr);

#if defined(CONFIG_ARCH_MT6797)
	spm_print(suspend, "raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x, scp_request_freq = %u\n",
			wakesta->raw_ext_sta,
			wakesta->wake_misc,
			spm_read(SPM_SW_FLAG),
			scp_request_freq);
	if (spm_read(spm_infracfg_base + 0x4C)) {
		spm_print(suspend, "bsi_bpi_tx_err_addr_reg = 0x%x, clk_cf_8 = 0x%x, mainpll_con0 = 0x%x\n",
			spm_read(spm_infracfg_base + 0x4C),
			spm_read(spm_cksys_base + 0xc0),
			spm_read(spm_apmixed_base + 0x220));
		BUG();
	}
#else
	spm_print(suspend, "raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x\n",
			wakesta->raw_ext_sta,
			wakesta->wake_misc,
			spm_read(SPM_SW_FLAG));
#endif

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
	spm_write(PCM_DEBUG_CON, !!enable);
}

unsigned int spm_get_cpu_pwr_status(void)
{
	unsigned int pwr_stat[2] = { 0 };
	unsigned int stat = 0;
	unsigned int ret_stat = 0;
	int i;

	pwr_stat[0] = spm_read(SPM_CPU_PWR_STATUS);
	pwr_stat[1] = spm_read(SPM_CPU_PWR_STATUS_2ND);

	stat = (pwr_stat[0] & spm_cpu_bitmask_all) & (pwr_stat[1] & spm_cpu_bitmask_all);

	for (i = 0; i < nr_cpu_ids; i++)
		if (stat & spm_cpu_bitmask[i])
			ret_stat |= (1 << i);

	return ret_stat;
}

long int spm_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

void __spm_check_md_pdn_power_control(struct pwr_ctrl *pwr_ctrl)
{
	if (is_md_c2k_conn_power_off())
		pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_MD_INFRA_PDN;
	else
		pwr_ctrl->pcm_flags &= ~SPM_FLAG_DIS_MD_INFRA_PDN;
}

void __spm_sync_vcore_dvfs_power_control(struct pwr_ctrl *dest_pwr_ctrl, const struct pwr_ctrl *src_pwr_ctrl)
{
	/* pwr_ctrl for mask/ctrl register */
	dest_pwr_ctrl->dvfs_halt_mask_b			= src_pwr_ctrl->dvfs_halt_mask_b;
	dest_pwr_ctrl->sdio_on_dvfs_req_mask_b		= src_pwr_ctrl->sdio_on_dvfs_req_mask_b;
#if defined(CONFIG_ARCH_MT6757)
	dest_pwr_ctrl->cpu_md_dvfs_req_merge_mask_b	= src_pwr_ctrl->cpu_md_dvfs_req_merge_mask_b;
	dest_pwr_ctrl->sw_ctrl_event_on			= src_pwr_ctrl->sw_ctrl_event_on;
	dest_pwr_ctrl->md_srcclkena_0_2d_dvfs_req_mask_b = src_pwr_ctrl->md_srcclkena_0_2d_dvfs_req_mask_b;
	dest_pwr_ctrl->md_srcclkena_1_2d_dvfs_req_mask_b = src_pwr_ctrl->md_srcclkena_1_2d_dvfs_req_mask_b;
	dest_pwr_ctrl->dvfs_up_2d_dvfs_req_mask_b	= src_pwr_ctrl->dvfs_up_2d_dvfs_req_mask_b;
	dest_pwr_ctrl->disable_off_load_lpm		= src_pwr_ctrl->disable_off_load_lpm;
#else
	dest_pwr_ctrl->cpu_md_dvfs_erq_merge_mask_b	= src_pwr_ctrl->cpu_md_dvfs_erq_merge_mask_b;
	dest_pwr_ctrl->md1_ddr_en_dvfs_halt_mask_b	= src_pwr_ctrl->md1_ddr_en_dvfs_halt_mask_b;
	dest_pwr_ctrl->md2_ddr_en_dvfs_halt_mask_b	= src_pwr_ctrl->md2_ddr_en_dvfs_halt_mask_b;
	dest_pwr_ctrl->vsync_dvfs_halt_mask_b		= src_pwr_ctrl->vsync_dvfs_halt_mask_b;
#endif
	dest_pwr_ctrl->md_srcclkena_0_dvfs_req_mask_b	= src_pwr_ctrl->md_srcclkena_0_dvfs_req_mask_b;
	dest_pwr_ctrl->md_srcclkena_1_dvfs_req_mask_b	= src_pwr_ctrl->md_srcclkena_1_dvfs_req_mask_b;
	dest_pwr_ctrl->conn_srcclkena_dvfs_req_mask_b	= src_pwr_ctrl->conn_srcclkena_dvfs_req_mask_b;

	dest_pwr_ctrl->emi_boost_dvfs_req_mask_b	= src_pwr_ctrl->emi_boost_dvfs_req_mask_b;
	dest_pwr_ctrl->emi_bw_dvfs_req_mask		= src_pwr_ctrl->emi_bw_dvfs_req_mask;
	dest_pwr_ctrl->cpu_md_emi_dvfs_req_prot_dis	= src_pwr_ctrl->cpu_md_emi_dvfs_req_prot_dis;
	dest_pwr_ctrl->spm_dvfs_req			= src_pwr_ctrl->spm_dvfs_req;
	dest_pwr_ctrl->spm_dvfs_force_down		= src_pwr_ctrl->spm_dvfs_force_down;
	dest_pwr_ctrl->cpu_md_dvfs_sop_force_on		= src_pwr_ctrl->cpu_md_dvfs_sop_force_on;
#if defined(SPM_VCORE_EN_MT6755) || defined(CONFIG_ARCH_MT6757)
	dest_pwr_ctrl->dvfs_halt_src_chk = src_pwr_ctrl->dvfs_halt_src_chk;
#endif

	/* pwr_ctrl pcm_flag */
	if (src_pwr_ctrl->pcm_flags_cust != 0) {
		if ((src_pwr_ctrl->pcm_flags_cust & SPM_FLAG_DIS_VCORE_DVS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DVS;

		if ((src_pwr_ctrl->pcm_flags_cust & SPM_FLAG_DIS_VCORE_DFS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DFS;

		if ((src_pwr_ctrl->pcm_flags_cust & SPM_FLAG_EN_MET_DBG_FOR_VCORE_DVFS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_EN_MET_DBG_FOR_VCORE_DVFS;
	} else {
		if ((src_pwr_ctrl->pcm_flags & SPM_FLAG_DIS_VCORE_DVS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DVS;

		if ((src_pwr_ctrl->pcm_flags & SPM_FLAG_DIS_VCORE_DFS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DFS;

		if ((src_pwr_ctrl->pcm_flags & SPM_FLAG_EN_MET_DBG_FOR_VCORE_DVFS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_EN_MET_DBG_FOR_VCORE_DVFS;
	}
}

void __spm_backup_vcore_dvfs_dram_shuffle(void)
{
#ifdef SPM_VCORE_EN_MT6797
	u32 shuf;

	shuf = spm_read(PCM_REG6_DATA) & (0x3 << 23);
	spm_write(SPM_SW_RSV_5, (spm_read(SPM_SW_RSV_5) & ~(0x3 << 23)) | shuf);
#endif
}

#define MM_DVFS_DISP1_HALT_MASK 0x1
#define MM_DVFS_DISP0_HALT_MASK 0x2
#define MM_DVFS_ISP_HALT_MASK   0x4
#define MM_DVFS_GCE_HALT_MASK   0x10

int __check_dvfs_halt_source(int enable)
{
	u32 val, orig_val;
	bool is_halt = 1;
#if defined(CONFIG_ARCH_MT6797)
	int i;
#endif

	val = spm_read(SPM_SRC2_MASK);
	orig_val = val;

	if (enable == 0) {
		pr_err("[VcoreFS]dvfs_halt_src_chk is disabled\n");
		return 0;
	}

	pr_err("[VcoreFS]SRC2_MASK=0x%x\n", val);
	if ((spm_read(CPU_DVFS_REQ) & DVFS_HALT_LSB) == 0) {
		is_halt = 0;
		pr_err("[VcoreFS]dvfs_halt already clear!(%d, 0x%x)\n", is_halt, spm_read(CPU_DVFS_REQ));
		aee_kernel_exception_api(__FILE__, __LINE__,
			 DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER | DB_OPT_DISPLAY_HANG_DUMP | DB_OPT_DUMP_DISPLAY,
			 "DVFS_HALT_UNKNOWN", "DVFS_HALT_UNKNOWN");
	}
	pr_err("[VcoreFS]halt_status(1)=0x%x\n", spm_read(CPU_DVFS_REQ));
	if ((val & MM_DVFS_ISP_HALT_MASK) && (is_halt)) {
		pr_err("[VcoreFS]isp_halt[0]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				val, spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));

#if defined(CONFIG_ARCH_MT6797)
		/* Test bitmask halt_en */
		for (i = 0; i < ISP_HALT_DMA_AMOUNT; i++) {
			ISP_Halt_Mask(i);
			udelay(50);
			if ((spm_read(CPU_DVFS_REQ) & DVFS_HALT_LSB) == 0) {
				pr_err("[VcoreFS]dvfs_halt is hold by ISP subgroup %d\n", i);
				break;
			}
		}
#endif
		spm_write(SPM_SRC2_MASK, (val & ~MM_DVFS_ISP_HALT_MASK));
		udelay(50);
		pr_err("[VcoreFS]isp_halt[1]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				spm_read(SPM_SRC2_MASK), spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		if ((spm_read(CPU_DVFS_REQ) & DVFS_HALT_LSB) == 0) {
			aee_kernel_exception_api(__FILE__, __LINE__,
			 DB_OPT_DEFAULT, "DVFS_HALT_ISP", "DVFS_HALT_ISP");
			is_halt = 0;
			pr_err("[VcoreFS]dvfs_halt is hold by ISP (%d, 0x%x)\n", is_halt, spm_read(CPU_DVFS_REQ));
		}
	}

	pr_err("[VcoreFS]halt_status(2)=0x%x\n", spm_read(CPU_DVFS_REQ));
	val = spm_read(SPM_SRC2_MASK);
	if ((val & MM_DVFS_DISP1_HALT_MASK) && (is_halt)) {
		pr_err("[VcoreFS]disp1_halt[0]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				 val, spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		spm_write(SPM_SRC2_MASK, (val & ~MM_DVFS_DISP1_HALT_MASK));
		udelay(50);
		pr_err("[VcoreFS]disp1_halt[1]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				spm_read(SPM_SRC2_MASK), spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		if ((spm_read(CPU_DVFS_REQ) & DVFS_HALT_LSB) == 0) {
			aee_kernel_exception_api(__FILE__, __LINE__,
			 DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER | DB_OPT_DISPLAY_HANG_DUMP | DB_OPT_DUMP_DISPLAY,
			 "DVFS_HALT_DISP1", "DVFS_HALT_DISP1");
			/* primary_display_diagnose(); */
			is_halt = 0;
			pr_err("[VcoreFS]dvfs_halt is hold by DISP1 (%d, 0x%x)\n", is_halt, spm_read(CPU_DVFS_REQ));
		}
	}

	pr_err("[VcoreFS]halt_status(3)=0x%x\n", spm_read(CPU_DVFS_REQ));
	val = spm_read(SPM_SRC2_MASK);
	if ((val & MM_DVFS_DISP0_HALT_MASK) && (is_halt)) {
		pr_err("[VcoreFS]disp0_halt[0]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				 val, spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		spm_write(SPM_SRC2_MASK, (val & ~MM_DVFS_DISP0_HALT_MASK));
		udelay(50);
		pr_err("[VcoreFS]disp0_halt[1]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				spm_read(SPM_SRC2_MASK), spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		if ((spm_read(CPU_DVFS_REQ) & DVFS_HALT_LSB) == 0) {
			aee_kernel_exception_api(__FILE__, __LINE__,
			 DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER | DB_OPT_DISPLAY_HANG_DUMP | DB_OPT_DUMP_DISPLAY,
			 "DVFS_HALT_DISP0", "DVFS_HALT_DISP0");
			/* primary_display_diagnose(); */
			is_halt = 0;
			pr_err("[VcoreFS]dvfs_halt is hold by DISP0 (%d, 0x%x)\n", is_halt, spm_read(CPU_DVFS_REQ));
		}
	}

	pr_err("[VcoreFS]halt_status(4)=0x%x\n", spm_read(CPU_DVFS_REQ));
	val = spm_read(SPM_SRC2_MASK);
	if ((val & MM_DVFS_GCE_HALT_MASK) && (is_halt)) {
		pr_err("[VcoreFS]gce_halt[0]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				val, spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		spm_write(SPM_SRC2_MASK, (val & ~MM_DVFS_GCE_HALT_MASK));
		udelay(50);
		pr_err("[VcoreFS]gce_halt[1]:src2_mask=0x%x r6=0x%x r15=0x%x\n",
				spm_read(SPM_SRC2_MASK), spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));
		if ((spm_read(CPU_DVFS_REQ) & DVFS_HALT_LSB) == 0) {
			aee_kernel_warning_api(__FILE__, __LINE__,
			 DB_OPT_DEFAULT, "DVFS_HALT_GCE", "DVFS_HALT_GCE");
			is_halt = 0;
			pr_err("[VcoreFS]dvfs_halt is hold by GCE (%d, 0x%x)\n", is_halt, spm_read(CPU_DVFS_REQ));
		}
	}

	udelay(1000); /* change to 1 ms for slow dvs slope FW */
	spm_write(SPM_SRC2_MASK, orig_val);
	pr_err("[VcoreFS]restore src_mask=0x%x, r6=0x%x r15=0x%x\n",
			spm_read(SPM_SRC2_MASK), spm_read(PCM_REG6_DATA), spm_read(PCM_REG15_DATA));

	/* BUG(); */

	return 0;
}

void spm_set_dummy_read_addr(void)
{
	u32 rank0_addr, rank1_addr, dram_rank_num;

	dram_rank_num = g_dram_info_dummy_read->rank_num;
	rank0_addr = g_dram_info_dummy_read->rank_info[0].start;
	if (dram_rank_num == 1)
		rank1_addr = rank0_addr;
	else
		rank1_addr = g_dram_info_dummy_read->rank_info[1].start;

	spm_crit("dram_rank_num: %d\n", dram_rank_num);
	spm_crit("dummy read addr: rank0: 0x%x, rank1: 0x%x\n", rank0_addr, rank1_addr);

#if defined(CONFIG_ARCH_MT6797)
	MAPPING_DRAM_ACCESS_ADDR(rank0_addr);
	MAPPING_DRAM_ACCESS_ADDR(rank1_addr);
	spm_crit("dummy read addr(4GB: %d): rank0: 0x%x, rank1: 0x%x\n", enable_4G(), rank0_addr, rank1_addr);
#endif

	spm_write(SPM_PASR_DPD_1, rank0_addr);
	spm_write(SPM_PASR_DPD_2, rank1_addr);
}

void spm_get_twam_table(const char ***table)
{
	*table = (const char **)twam_event_str;
}

bool is_md_c2k_conn_power_off(void)
{
	u32 md1_pwr_con = 0;
	u32 c2k_pwr_con = 0;
	u32 conn_pwr_con = 0;

	md1_pwr_con = spm_read(MD1_PWR_CON);
	c2k_pwr_con = spm_read(C2K_PWR_CON);
	conn_pwr_con = spm_read(CONN_PWR_CON);

#if 0
	pr_err("md1_pwr_con = 0x%08x, c2k_pwr_con = 0x%08x, conn_pwr_con = 0x%08x\n",
	       md1_pwr_con, c2k_pwr_con, conn_pwr_con);
#endif

	if (!((md1_pwr_con & 0x1F) == 0x12))
		return false;

	if (!((c2k_pwr_con & 0x1F) == 0x12))
		return false;

	if (!((conn_pwr_con & 0x1F) == 0x12))
		return false;

	return true;
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static u32 pmic_rg_auxadc_ck_pdn_hwen;
static u32 pmic_rg_efuse_ck_pdn;
#endif

void __spm_backup_pmic_ck_pdn(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* PMIC setting 2015/07/31 by Chia-Lin/Kev */
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353) && defined(CONFIG_ARCH_MT6755)
	pmic_read_interface_nolock(PMIC_CLK_AUXADC_CK_PDN_HWEN_ADDR,
				   &pmic_rg_auxadc_ck_pdn_hwen,
				   PMIC_CLK_AUXADC_CK_PDN_HWEN_MASK,
				   PMIC_CLK_AUXADC_CK_PDN_HWEN_SHIFT);
	pmic_config_interface_nolock(PMIC_CLK_AUXADC_CK_PDN_HWEN_ADDR,
				     0,
				     PMIC_CLK_AUXADC_CK_PDN_HWEN_MASK,
				     PMIC_CLK_AUXADC_CK_PDN_HWEN_SHIFT);

	pmic_read_interface_nolock(PMIC_CLK_EFUSE_CK_PDN_ADDR,
				   &pmic_rg_efuse_ck_pdn,
				   PMIC_CLK_EFUSE_CK_PDN_MASK,
				   PMIC_CLK_EFUSE_CK_PDN_SHIFT);
	pmic_config_interface_nolock(PMIC_CLK_EFUSE_CK_PDN_ADDR,
				     1,
				     PMIC_CLK_EFUSE_CK_PDN_MASK,
				     PMIC_CLK_EFUSE_CK_PDN_SHIFT);
#else
	pmic_read_interface_nolock(MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_ADDR,
				   &pmic_rg_auxadc_ck_pdn_hwen,
				   MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_MASK,
				   MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_SHIFT);
	pmic_config_interface_nolock(MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_ADDR,
				     0,
				     MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_MASK,
				     MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_SHIFT);

	pmic_read_interface_nolock(MT6351_PMIC_RG_EFUSE_CK_PDN_ADDR,
				   &pmic_rg_efuse_ck_pdn,
				   MT6351_PMIC_RG_EFUSE_CK_PDN_MASK,
				   MT6351_PMIC_RG_EFUSE_CK_PDN_SHIFT);
	pmic_config_interface_nolock(MT6351_PMIC_RG_EFUSE_CK_PDN_ADDR,
				     1,
				     MT6351_PMIC_RG_EFUSE_CK_PDN_MASK,
				     MT6351_PMIC_RG_EFUSE_CK_PDN_SHIFT);
#endif
#endif
}

void __spm_restore_pmic_ck_pdn(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* PMIC setting 2015/07/31 by Chia-Lin/Kev */
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353) && defined(CONFIG_ARCH_MT6755)
	pmic_config_interface_nolock(PMIC_CLK_AUXADC_CK_PDN_HWEN_ADDR,
				     pmic_rg_auxadc_ck_pdn_hwen,
				     PMIC_CLK_AUXADC_CK_PDN_HWEN_MASK,
				     PMIC_CLK_AUXADC_CK_PDN_HWEN_SHIFT);

	pmic_config_interface_nolock(PMIC_CLK_EFUSE_CK_PDN_ADDR,
				     pmic_rg_efuse_ck_pdn,
				     PMIC_CLK_EFUSE_CK_PDN_MASK,
				     PMIC_CLK_EFUSE_CK_PDN_SHIFT);
#else
	pmic_config_interface_nolock(MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_ADDR,
				     pmic_rg_auxadc_ck_pdn_hwen,
				     MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_MASK,
				     MT6351_PMIC_RG_AUXADC_CK_PDN_HWEN_SHIFT);

	pmic_config_interface_nolock(MT6351_PMIC_RG_EFUSE_CK_PDN_ADDR,
				     pmic_rg_efuse_ck_pdn,
				     MT6351_PMIC_RG_EFUSE_CK_PDN_MASK,
				     MT6351_PMIC_RG_EFUSE_CK_PDN_SHIFT);
#endif
#endif
}

void __spm_bsi_top_init_setting(void)
{
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757)
		/* BSI_TOP init setting */
		spm_write(spm_bsi1cfg + 0x2004, 0x8000A824);
		spm_write(spm_bsi1cfg + 0x2010, 0x20001201);
		spm_write(spm_bsi1cfg + 0x2014, 0x150b0000);
		spm_write(spm_bsi1cfg + 0x2020, 0x0e001841);
		spm_write(spm_bsi1cfg + 0x2024, 0x150b0000);
		spm_write(spm_bsi1cfg + 0x2030, 0x1);
#endif
}

void __spm_pmic_pg_force_on(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353) && defined(CONFIG_ARCH_MT6755)
	pmic_config_interface_nolock(PMIC_STRUP_DIG_IO_PG_FORCE_ADDR,
			0x1,
			PMIC_STRUP_DIG_IO_PG_FORCE_MASK,
			PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT);
	pmic_config_interface_nolock(PMIC_RG_STRUP_VIO18_PG_ENB_ADDR,
			0x1,
			PMIC_RG_STRUP_VIO18_PG_ENB_MASK,
			PMIC_RG_STRUP_VIO18_PG_ENB_SHIFT);
#else
	pmic_config_interface_nolock(MT6351_PMIC_STRUP_DIG_IO_PG_FORCE_ADDR,
			0x1,
			MT6351_PMIC_STRUP_DIG_IO_PG_FORCE_MASK,
			MT6351_PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT);
	pmic_config_interface_nolock(MT6351_PMIC_RG_STRUP_VIO18_PG_ENB_ADDR,
			0x1,
			MT6351_PMIC_RG_STRUP_VIO18_PG_ENB_MASK,
			MT6351_PMIC_RG_STRUP_VIO18_PG_ENB_SHIFT);
#endif
#endif
}

void __spm_pmic_pg_force_off(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353) && defined(CONFIG_ARCH_MT6755)
	pmic_config_interface_nolock(PMIC_STRUP_DIG_IO_PG_FORCE_ADDR,
			0x0,
			PMIC_STRUP_DIG_IO_PG_FORCE_MASK,
			PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT);
	pmic_config_interface_nolock(PMIC_RG_STRUP_VIO18_PG_ENB_ADDR,
			0x0,
			PMIC_RG_STRUP_VIO18_PG_ENB_MASK,
			PMIC_RG_STRUP_VIO18_PG_ENB_SHIFT);
#else
	pmic_config_interface_nolock(MT6351_PMIC_STRUP_DIG_IO_PG_FORCE_ADDR,
			0x0,
			MT6351_PMIC_STRUP_DIG_IO_PG_FORCE_MASK,
			MT6351_PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT);
	pmic_config_interface_nolock(MT6351_PMIC_RG_STRUP_VIO18_PG_ENB_ADDR,
			0x0,
			MT6351_PMIC_RG_STRUP_VIO18_PG_ENB_MASK,
			MT6351_PMIC_RG_STRUP_VIO18_PG_ENB_SHIFT);
#endif
#endif
}

void __spm_pmic_low_iq_mode(int en)
{
#if defined(CONFIG_ARCH_MT6797)
	if (en) {
		pmic_config_interface_nolock(MT6351_VGPU_ANA_CON7, 1, 0x1, 1);
	} else {
		pmic_config_interface_nolock(MT6351_VGPU_ANA_CON7, 0, 0x1, 1);
	}
#endif
}

void __spm_set_pcm_wdt(int en)
{
	/* enable PCM WDT (normal mode) to start count if needed */
	if (en) {
		u32 con1;

		con1 = spm_read(PCM_CON1) & ~(PCM_WDT_WAKE_MODE_LSB | PCM_WDT_EN_LSB);
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | con1);

		if (spm_read(PCM_TIMER_VAL) > PCM_TIMER_MAX)
			spm_write(PCM_TIMER_VAL, PCM_TIMER_MAX);
		spm_write(PCM_WDT_VAL, spm_read(PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
		spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | PCM_WDT_EN_LSB);
	} else {
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) &
		~PCM_WDT_EN_LSB));
	}

}

/* FIXME: check charger for get_dynamic_period */
int __attribute__ ((weak)) get_dynamic_period(int first_use, int first_wakeup_time,
					      int battery_capacity_level)
{
	return 60;
}

u32 _spm_get_wake_period(int pwake_time, wake_reason_t last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
		period = get_dynamic_period(last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
		if (period <= 0) {
			spm_warn("CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		spm_crit2("pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
		period = 36 * 3600;

	return period;
}


MODULE_DESCRIPTION("SPM-Internal Driver v0.1");
