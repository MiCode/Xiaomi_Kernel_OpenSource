/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <mt-plat/mtk_secure_api.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <linux/fb.h>
#include <linux/notifier.h>
#if defined(CONFIG_MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#endif

#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif
#include <mt-plat/mtk_chip.h>

//#include <mtk_spm_misc.h>
#include <mtk_spm_vcore_dvfs.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_pmic_wrap.h>
#include <mtk_dvfsrc_reg.h>
#include <mtk_sleep_reg_md_reg_mt6771.h>
//#include <mtk_eem.h>
#include <ext_wd_drv.h>
#include "mtk_devinfo.h"
//#include <mt_emi_api.h>

#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc.h>

#include <mt-plat/mtk_devinfo.h>

#include <mtk_ts_setting.h>
#include <tscpu_settings.h>


#define is_dvfs_in_progress()    (spm_read(DVFSRC_LEVEL) & 0xFFFF)
#define get_dvfs_level()         (spm_read(DVFSRC_LEVEL) >> 16)

/*
 * only for internal debug
 */
#define SPM_VCOREFS_TAG	"[name:spm&][VcoreFS] "
#define spm_vcorefs_err spm_vcorefs_info
#define spm_vcorefs_warn spm_vcorefs_info
#define spm_vcorefs_debug spm_vcorefs_info
#define spm_vcorefs_info(fmt, args...)	\
	printk_deferred(SPM_VCOREFS_TAG fmt, ##args)

void __iomem *dvfsrc_base;
void __iomem *qos_sram_base;

int plat_lcd_resolution;

enum spm_vcorefs_step {
	SPM_VCOREFS_ENTER = 0x00000001,
	SPM_VCOREFS_DVFS_START = 0x000000ff,
	SPM_VCOREFS_DVFS_END = 0x000001ff,
	SPM_VCOREFS_LEAVE = 0x000007ff,
};

__weak int vcore_opp_init(void) { return 0; }
__weak unsigned int get_vcore_opp_volt(unsigned int opp) { return 800000; }
__weak int tscpu_min_temperature(void) { return 40; }
__weak int get_emi_bwvl(unsigned int bw_index) { return 0; }
__weak unsigned int get_emi_bwst(unsigned int bw_index) { return 0; }

int vcore_map[NUM_OPP] = {
	VCORE_OPP_0,
	VCORE_OPP_1,
	VCORE_OPP_1,
	VCORE_OPP_1
};

static struct pwr_ctrl vcorefs_ctrl;

struct spm_lp_scen __spm_vcorefs = {
	.pwrctrl = &vcorefs_ctrl,
};

static int force_opp_enable;

int is_force_opp_enable(void)
{
	return force_opp_enable;
}

/* lt_opp config */
static int lt_opp_feature_en = 1;
static int enter_lt_opp_temp;
static int leave_lt_opp_temp = 10000;

static int lt_opp_enable;
static int adj_vcore_uv = 12500;
static int last_temp;
struct pm_qos_request temp_emi_req;
struct pm_qos_request temp_vcore_req;

int spm_get_vcore_opp(unsigned int opp)
{
	if (opp < 0 || opp >= NUM_OPP) {
		spm_vcorefs_err("invalid input opp:%d\n", opp);
		opp = 0;
	}
	return vcore_map[opp];
}

static inline void spm_vcorefs_footprint(enum spm_vcorefs_step step)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_vcore_dvfs_status(step);
#endif
}

char *spm_vcorefs_dump_dvfs_regs(char *p)
{
	u32 bwst0_val = get_emi_bwst(0);
	u32 bwvl0_val = get_emi_bwvl(0);

	if (p) {
		#if 1
		/* DVFSRC */
		p += sprintf(p, "DVFSRC_RECORD_COUNT    : 0x%x\n",
				spm_read(DVFSRC_RECORD_COUNT));
		p += sprintf(p, "DVFSRC_LAST            : 0x%x\n",
				spm_read(DVFSRC_LAST));
		p += sprintf(p,
		 "DVFSRC_RECORD_0_1~3_1  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 spm_read(DVFSRC_RECORD_0_1), spm_read(DVFSRC_RECORD_1_1),
		 spm_read(DVFSRC_RECORD_2_1), spm_read(DVFSRC_RECORD_3_1));
		p += sprintf(p,
		 "DVFSRC_RECORD_4_1~7_1  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 spm_read(DVFSRC_RECORD_4_1), spm_read(DVFSRC_RECORD_5_1),
		 spm_read(DVFSRC_RECORD_6_1), spm_read(DVFSRC_RECORD_7_1));
		p += sprintf(p,
		 "DVFSRC_RECORD_0_0~3_0  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 spm_read(DVFSRC_RECORD_0_0), spm_read(DVFSRC_RECORD_1_0),
		 spm_read(DVFSRC_RECORD_2_0), spm_read(DVFSRC_RECORD_3_0));
		p += sprintf(p,
		 "DVFSRC_RECORD_4_0~7_0  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 spm_read(DVFSRC_RECORD_4_0), spm_read(DVFSRC_RECORD_5_0),
		 spm_read(DVFSRC_RECORD_6_0), spm_read(DVFSRC_RECORD_7_0));
		p += sprintf(p,
		 "DVFSRC_RECORD_MD_0~3   : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 spm_read(DVFSRC_RECORD_MD_0), spm_read(DVFSRC_RECORD_MD_1),
		 spm_read(DVFSRC_RECORD_MD_2), spm_read(DVFSRC_RECORD_MD_3));
		p += sprintf(p,
		 "DVFSRC_RECORD_MD_4~7   : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 spm_read(DVFSRC_RECORD_MD_4), spm_read(DVFSRC_RECORD_MD_5),
		spm_read(DVFSRC_RECORD_MD_6), spm_read(DVFSRC_RECORD_MD_7));
		p += sprintf(p, "DVFSRC_BASIC_CONTROL   : 0x%x\n",
				spm_read(DVFSRC_BASIC_CONTROL));
		p += sprintf(p, "DVFSRC_LEVEL           : 0x%x\n",
				spm_read(DVFSRC_LEVEL));
		p += sprintf(p, "DVFSRC_VCORE_REQUEST   : 0x%x\n",
				spm_read(DVFSRC_VCORE_REQUEST));
		p += sprintf(p, "DVFSRC_VCORE_REQUEST2  : 0x%x\n",
				spm_read(DVFSRC_VCORE_REQUEST2));
		p += sprintf(p, "DVFSRC_EMI_REQUEST     : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST));
		p += sprintf(p, "DVFSRC_EMI_REQUEST2    : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST2));
		p += sprintf(p, "DVFSRC_EMI_REQUEST3    : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST3));
		p += sprintf(p, "DVFSRC_MD_REQUEST      : 0x%x\n",
			spm_read(DVFSRC_MD_REQUEST));
		p += sprintf(p, "DVFSRC_RSRV_0          : 0x%x\n",
			spm_read(DVFSRC_RSRV_0));
		p += sprintf(p, "DVFSRC_SW_REQ          : 0x%x\n",
			spm_read(DVFSRC_SW_REQ));
		p += sprintf(p, "DVFSRC_SW_REQ2         : 0x%x\n",
			spm_read(DVFSRC_SW_REQ2));
		p += sprintf(p, "DVFSRC_SEC_SW_REQ      : 0x%x\n",
			spm_read(DVFSRC_SEC_SW_REQ));
		p += sprintf(p, "DVFSRC_INT             : 0x%x\n",
			spm_read(DVFSRC_INT));
		p += sprintf(p, "DVFSRC_FORCE           : 0x%x\n",
			spm_read(DVFSRC_FORCE));
		p += sprintf(p, "DVFSRC_EMI_MD2SPM0/1   : 0x%x / 0x%x\n",
			spm_read(DVFSRC_EMI_MD2SPM0),
			spm_read(DVFSRC_EMI_MD2SPM1));
		p += sprintf(p, "DVFSRC_VCORE_MD2SPM0   : 0x%x\n",
			spm_read(DVFSRC_VCORE_MD2SPM0));
		/* SPM */
		p += sprintf(p, "SPM_SW_FLAG            : 0x%x\n",
			spm_read(SPM_SW_FLAG));
		p += sprintf(p, "SPM_SW_RSV_5           : 0x%x\n",
			spm_read(SPM_SW_RSV_5));
		p += sprintf(p, "MD2SPM_DVFS_CON        : 0x%x\n",
			spm_read(MD2SPM_DVFS_CON));
		p += sprintf(p, "SPM_DVFS_EVENT_STA     : 0x%x\n",
			spm_read(SPM_DVFS_EVENT_STA));
		p += sprintf(p, "SPM_DVFS_LEVEL         : 0x%x\n",
			spm_read(SPM_DVFS_LEVEL));
		p += sprintf(p, "SPM_DFS_LEVEL          : 0x%x\n",
			spm_read(SPM_DFS_LEVEL));
		p += sprintf(p, "SPM_DVS_LEVEL          : 0x%x\n",
			spm_read(SPM_DVS_LEVEL));
		p += sprintf(p, "SPM_ACK_CHK_TIMER2	: 0x%x\n",
			spm_read(SPM_ACK_CHK_TIMER2));

		p += sprintf(p,
			"PCM_REG_DATA_0~3       : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG0_DATA), spm_read(PCM_REG1_DATA),
			spm_read(PCM_REG2_DATA), spm_read(PCM_REG3_DATA));
		p += sprintf(p,
			"PCM_REG_DATA_4~7       : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG4_DATA), spm_read(PCM_REG5_DATA),
			spm_read(PCM_REG6_DATA), spm_read(PCM_REG7_DATA));
		p += sprintf(p,
			"PCM_REG_DATA_8~11      : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG8_DATA), spm_read(PCM_REG9_DATA),
			spm_read(PCM_REG10_DATA), spm_read(PCM_REG11_DATA));
		p += sprintf(p,
			"PCM_REG_DATA_12~15     : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG12_DATA), spm_read(PCM_REG13_DATA),
			spm_read(PCM_REG14_DATA), spm_read(PCM_REG15_DATA));
		p += sprintf(p,
		"MDPTP_VMODEM_SPM_DVFS_CMD16~19   : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD16),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD17),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD18),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD19));
		p += sprintf(p, "SPM_DVFS_CMD0~1        : 0x%x, 0x%x\n",
			spm_read(SPM_DVFS_CMD0), spm_read(SPM_DVFS_CMD1));
		p += sprintf(p, "PCM_IM_PTR             : 0x%x (%u)\n",
			spm_read(PCM_IM_PTR), spm_read(PCM_IM_LEN));

		/* BW Info */
		p += sprintf(p,
			"BW_TOTAL: %d (AVG: %d) thres: 0x%x, 0x%x seg: 0x%x\n",
			dvfsrc_get_bw(QOS_TOTAL), dvfsrc_get_bw(QOS_TOTAL_AVE),
			spm_read(DVFSRC_EMI_QOS0), spm_read(DVFSRC_EMI_QOS1),
			spm_read(QOS_SRAM_SEG));

		/* EMI Monitor */
		p += sprintf(p,
		"TOTAL_EMI(level 1/2): %d/%d (bwst: 0x%x, bwvl: 0x%x)\n",
		(bwst0_val & 1), ((bwst0_val >> 1) & 1), bwst0_val, bwvl0_val);

		p += sprintf(p,
		"lt_opp: feature_en=%d, enable=%d, enter_temp=%d, leave_temp=%d (last_temp=%d)\n",
			 lt_opp_feature_en, lt_opp_enable, enter_lt_opp_temp,
			 leave_lt_opp_temp, last_temp);
		#endif
	} else {
		#if 1
		/* DVFSRC */
		spm_vcorefs_warn("DVFSRC_RECORD_COUNT    : 0x%x\n",
			spm_read(DVFSRC_RECORD_COUNT));
		spm_vcorefs_warn("DVFSRC_LAST            : 0x%x\n",
			spm_read(DVFSRC_LAST));
		spm_vcorefs_warn
		("DVFSRC_RECORD_0_1~3_1  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_0_1),
			spm_read(DVFSRC_RECORD_1_1),
			spm_read(DVFSRC_RECORD_2_1),
			spm_read(DVFSRC_RECORD_3_1));
		spm_vcorefs_warn
		("DVFSRC_RECORD_4_1~7_1  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_4_1),
			spm_read(DVFSRC_RECORD_5_1),
			spm_read(DVFSRC_RECORD_6_1),
			spm_read(DVFSRC_RECORD_7_1));
		spm_vcorefs_warn
		("DVFSRC_RECORD_0_0~3_0  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_0_0),
			spm_read(DVFSRC_RECORD_1_0),
			spm_read(DVFSRC_RECORD_2_0),
			spm_read(DVFSRC_RECORD_3_0));
		spm_vcorefs_warn
		("DVFSRC_RECORD_4_0~7_0  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_4_0),
			spm_read(DVFSRC_RECORD_5_0),
			spm_read(DVFSRC_RECORD_6_0),
			spm_read(DVFSRC_RECORD_7_0));
		spm_vcorefs_warn
		("DVFSRC_RECORD_MD_0~3   : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_MD_0),
			spm_read(DVFSRC_RECORD_MD_1),
			spm_read(DVFSRC_RECORD_MD_2),
			spm_read(DVFSRC_RECORD_MD_3));
		spm_vcorefs_warn
		("DVFSRC_RECORD_MD_4~7   : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_MD_4),
			spm_read(DVFSRC_RECORD_MD_5),
			spm_read(DVFSRC_RECORD_MD_6),
			spm_read(DVFSRC_RECORD_MD_7));
		spm_vcorefs_warn("DVFSRC_BASIC_CONTROL   : 0x%x\n",
			spm_read(DVFSRC_BASIC_CONTROL));
		spm_vcorefs_warn("DVFSRC_LEVEL           : 0x%x\n",
			spm_read(DVFSRC_LEVEL));
		spm_vcorefs_warn("DVFSRC_VCORE_REQUEST   : 0x%x\n",
			spm_read(DVFSRC_VCORE_REQUEST));
		spm_vcorefs_warn("DVFSRC_VCORE_REQUEST2  : 0x%x\n",
			spm_read(DVFSRC_VCORE_REQUEST2));
		spm_vcorefs_warn("DVFSRC_EMI_REQUEST     : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST));
		spm_vcorefs_warn("DVFSRC_EMI_REQUEST2    : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST2));
		spm_vcorefs_warn("DVFSRC_EMI_REQUEST3    : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST3));
		spm_vcorefs_warn("DVFSRC_MD_REQUEST      : 0x%x\n",
			spm_read(DVFSRC_MD_REQUEST));
		spm_vcorefs_warn("DVFSRC_RSRV_0          : 0x%x\n",
			spm_read(DVFSRC_RSRV_0));
		spm_vcorefs_warn("DVFSRC_SW_REQ          : 0x%x\n",
			spm_read(DVFSRC_SW_REQ));
		spm_vcorefs_warn("DVFSRC_SW_REQ2         : 0x%x\n",
			spm_read(DVFSRC_SW_REQ2));
		spm_vcorefs_warn("DVFSRC_SEC_SW_REQ      : 0x%x\n",
			spm_read(DVFSRC_SEC_SW_REQ));
		spm_vcorefs_warn("DVFSRC_INT             : 0x%x\n",
			spm_read(DVFSRC_INT));
		spm_vcorefs_warn("DVFSRC_EMI_MD2SPM0/1   : 0x%x / 0x%x\n",
			spm_read(DVFSRC_EMI_MD2SPM0),
			spm_read(DVFSRC_EMI_MD2SPM1));
		spm_vcorefs_warn("DVFSRC_VCORE_MD2SPM0   : 0x%x\n",
			spm_read(DVFSRC_VCORE_MD2SPM0));
		/* SPM */
		spm_vcorefs_warn("SPM_SW_FLAG            : 0x%x\n",
			spm_read(SPM_SW_FLAG));
		spm_vcorefs_warn("SPM_SW_RSV_5           : 0x%x\n",
			spm_read(SPM_SW_RSV_5));
		spm_vcorefs_warn("MD2SPM_DVFS_CON        : 0x%x\n",
			spm_read(MD2SPM_DVFS_CON));
		spm_vcorefs_warn("SPM_DVFS_EVENT_STA     : 0x%x\n",
			spm_read(SPM_DVFS_EVENT_STA));
		spm_vcorefs_warn("SPM_DVFS_LEVEL         : 0x%x\n",
			spm_read(SPM_DVFS_LEVEL));
		spm_vcorefs_warn("SPM_DFS_LEVEL          : 0x%x\n",
			spm_read(SPM_DFS_LEVEL));
		spm_vcorefs_warn("SPM_DVS_LEVEL          : 0x%x\n",
			spm_read(SPM_DVS_LEVEL));
		spm_vcorefs_warn("SPM_ACK_CHK_TIMER2     : 0x%x\n",
			spm_read(SPM_ACK_CHK_TIMER2));

		spm_vcorefs_warn
		("PCM_REG_DATA_0~3       : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG0_DATA), spm_read(PCM_REG1_DATA),
			spm_read(PCM_REG2_DATA), spm_read(PCM_REG3_DATA));
		spm_vcorefs_warn
		("PCM_REG_DATA_4~7       : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG4_DATA), spm_read(PCM_REG5_DATA),
			spm_read(PCM_REG6_DATA), spm_read(PCM_REG7_DATA));
		spm_vcorefs_warn
		("PCM_REG_DATA_8~11      : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG8_DATA), spm_read(PCM_REG9_DATA),
			spm_read(PCM_REG10_DATA), spm_read(PCM_REG11_DATA));
		spm_vcorefs_warn
		("PCM_REG_DATA_12~15     : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG12_DATA), spm_read(PCM_REG13_DATA),
			spm_read(PCM_REG14_DATA), spm_read(PCM_REG15_DATA));
		spm_vcorefs_warn
		("MDPTP_VMODEM_SPM_DVFS_CMD16~19   : 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD16),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD17),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD18),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD19));
		spm_vcorefs_warn("SPM_DVFS_CMD0~1        : 0x%x, 0x%x\n",
				spm_read(SPM_DVFS_CMD0),
				spm_read(SPM_DVFS_CMD1));
		spm_vcorefs_warn("PCM_IM_PTR             :: 0x%x (%u)\n",
				spm_read(PCM_IM_PTR), spm_read(PCM_IM_LEN));

		/* BW Info */
		spm_vcorefs_warn
			("BW_TOTAL: %d (AVG: %d) thres: 0x%x, 0x%x seg: 0x%x\n",
			dvfsrc_get_bw(QOS_TOTAL), dvfsrc_get_bw(QOS_TOTAL_AVE),
			spm_read(DVFSRC_EMI_QOS0), spm_read(DVFSRC_EMI_QOS1),
			spm_read(QOS_SRAM_SEG));
		/* EMI Monitor */
		spm_vcorefs_warn
		("TOTAL_EMI(level 1/2): %d/%d (bwst: 0x%x, bwvl: 0x%x)\n",
		(bwst0_val & 1), ((bwst0_val >> 1) & 1),
		bwst0_val, bwvl0_val);

		spm_vcorefs_warn
		("lt_opp: f_en=%d enable=%d, in_t=%d, out_t=%d (last_t=%d)\n",
		lt_opp_feature_en, lt_opp_enable,
		enter_lt_opp_temp, leave_lt_opp_temp, last_temp);
		#endif
	}

	return p;
}

/*
 * condition: false will loop for check
 */
#define wait_spm_complete_by_condition(condition, timeout)	\
({								\
	int i = 0;						\
	while (!(condition)) {					\
		if (i >= (timeout)) {				\
			i = -EBUSY;				\
			break;					\
		}						\
		udelay(1);					\
		i++;						\
	}							\
	i;							\
})

u32 spm_vcorefs_get_MD_status(void)
{
	return spm_read(MD2SPM_DVFS_CON);
}

u32 spm_vcorefs_get_md_srcclkena(void)
{
	return spm_read(PCM_REG13_DATA) & (1U << 8);
}

static void spm_dvfsfw_init(int curr_opp)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	SMC_CALL(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_0, curr_opp, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

int spm_vcorefs_pwarp_cmd(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	/* 0.7V opp */
	SMC_CALL(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS, VCOREFS_SMC_CMD_3, 0,
			vcore_uv_to_pmic(get_vcore_opp_volt(VCORE_DVFS_OPP_3)));

	/* 0.8V opp */
	SMC_CALL(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS, VCOREFS_SMC_CMD_3, 1,
			vcore_uv_to_pmic(get_vcore_opp_volt(VCORE_DVFS_OPP_0)));

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_vcorefs_warn("%s: atf\n", __func__);

	return 0;
}

int spm_vcorefs_get_opp(void)
{
	unsigned long flags;
	int level;

	if (is_vcorefs_can_work() == 1) {
		spin_lock_irqsave(&__spm_lock, flags);

		level = (spm_read(DVFSRC_LEVEL) >> 16);

		if (level == 0x8)
			level = OPP_0;
		else if (level == 0x4)
			level = OPP_1;
		else if (level == 0x2)
			level = OPP_2;
		else if (level == 0x1)
			level = OPP_3;
		else if (level == 0x10)
			level = 4;
		else if (level == 0x20)
			level = 5;
		else if (level == 0x40)
			level = 6;
		else if (level == 0x80)
			level = 7;
		else if (level == 0x100)
			level = 8;
		else if (level == 0x200)
			level = 9;
		else if (level == 0x400)
			level = 10;
		else if (level == 0x800)
			level = 11;
		else if (level == 0x1000)
			level = 12;
		else if (level == 0x2000)
			level = 13;
		else if (level == 0x4000)
			level = 14;
		else if (level == 0x8000)
			level = 15;

		spin_unlock_irqrestore(&__spm_lock, flags);
	} else {
		level = BOOT_UP_OPP;
	}

	return level;
}

int spm_vcorefs_get_dvfs_opp(void)
{
	int dvfs_opp;

	if (is_vcorefs_can_work() == 1) {
		dvfs_opp = (spm_read(SPM_SW_RSV_5) & 0xFFFF);
		switch (dvfs_opp) {
		case 0x8:
		case 0x80:
		case 0x800:
			dvfs_opp = OPP_0;
		break;
		case 0x4:
		case 0x40:
		case 0x400:
			dvfs_opp = OPP_1;
		break;
		case 0x2:
		case 0x20:
		case 0x200:
			dvfs_opp = OPP_2;
		break;
		case 0x1:
		case 0x10:
		case 0x100:
		case 0x1000:
			dvfs_opp = OPP_3;
		break;
		default:
			dvfs_opp = BOOT_UP_OPP;
		break;
		}
	} else {
		dvfs_opp = BOOT_UP_OPP;
	}

	return dvfs_opp;
}

void dvfsrc_hw_policy_mask(bool mask)
{
	if (mask) {
		spm_write(DVFSRC_EMI_REQUEST, 0);
		spm_write(DVFSRC_EMI_REQUEST3, 0);
		spm_write(DVFSRC_VCORE_REQUEST, 0);
		spm_write(DVFSRC_VCORE_REQUEST2, 0);
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 3));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 0));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 5));
		spm_write(DVFSRC_SW_REQ2, 0);
	} else {
		spm_write(DVFSRC_EMI_REQUEST, 0x00290209);
		spm_write(DVFSRC_EMI_REQUEST3, 0x09000000);
		spm_write(DVFSRC_VCORE_REQUEST, 0x00150000);
		/* spm_write(DVFSRC_VCORE_REQUEST2, 0x29292929); */
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 3));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 0));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 5));
	}
}

static int spm_trigger_dvfs(int kicker, int opp, bool fix)
{
	int r = 0;

	u32 vcore_req[NUM_OPP] = {0x1, 0x1, 0x0, 0x0};
	u32 emi_req[NUM_OPP] = {0x2, 0x1, 0x1, 0x0};

	if (__spm_get_dram_type() == SPMFW_LP4X_2CH_3200) {
		vcore_req[1] = 0x0;
		emi_req[1] = 0x2;
	}

	if (fix) {
		force_opp_enable = 1;
		dvfsrc_hw_policy_mask(1);
	} else {
		force_opp_enable = 0;
		dvfsrc_hw_policy_mask(0);
	}

	/* check DVFS idle */
	r = wait_spm_complete_by_condition(is_dvfs_in_progress() == 0,
					SPM_DVFS_TIMEOUT);
	if (r < 0) {
		spm_vcorefs_warn("[%s]wait idle timeout !\n", __func__);
		spm_vcorefs_dump_dvfs_regs(NULL);
		aee_kernel_warning("VCOREFS", "wait idle timeout");
		return -1;
	}

		if (opp >= NUM_OPP || opp < OPP_0)
			opp = NUM_OPP - 1;

		spm_write(DVFSRC_SW_REQ,
			(spm_read(DVFSRC_SW_REQ) & ~(0x3 << 2))
			| (vcore_req[opp] << 2));
		spm_write(DVFSRC_SW_REQ,
			(spm_read(DVFSRC_SW_REQ) & ~(0x3))
			| (emi_req[opp]));

	/* check DVFS timer */
	if (fix) {
		if (opp >= 0)
			r = wait_spm_complete_by_condition
			(spm_vcorefs_get_dvfs_opp() == opp, SPM_DVFS_TIMEOUT);
	} else {
		r = wait_spm_complete_by_condition
		(spm_vcorefs_get_dvfs_opp() <= opp, SPM_DVFS_TIMEOUT);
	}

	if (r < 0) {
		spm_vcorefs_warn("[%s]wait complete timeout!\n", __func__);
		spm_vcorefs_dump_dvfs_regs(NULL);
		aee_kernel_warning("VCOREFS", "wait complete timeout");
		return -1;
	}

	vcorefs_crit_mask(log_mask(), kicker,
		"[%s] fix:%d, opp:%d, sw:0x%x, md:0x%x level:0x%x rsv5:0x%x\n",
			__func__, fix, opp,
			spm_read(DVFSRC_SW_REQ), spm_read(DVFSRC_MD_REQUEST),
			spm_read(DVFSRC_LEVEL), spm_read(SPM_SW_RSV_5));
	return 0;
}

int spm_dvfs_flag_init(void)
{
	int flag = SPM_FLAG_RUN_COMMON_SCENARIO;

	if (!vcorefs_vcore_dvs_en())
		flag |= SPM_FLAG_DIS_VCORE_DVS;
	if (!vcorefs_dram_dfs_en())
		flag |= SPM_FLAG_DIS_VCORE_DFS;
	if (!vcorefs_mm_clk_en())
		flag |= SPM_FLAG_DISABLE_MMSYS_DVFS;

	return flag;
}



void dvfsrc_md_scenario_update(bool suspend)
{
	int spmfw_dram_type = __spm_get_dram_type();

	if (spmfw_dram_type == SPMFW_LP3_1CH_1866) {
		if (suspend)
			spm_write(DVFSRC_EMI_MD2SPM0, 0x0);
		else
			spm_write(DVFSRC_EMI_MD2SPM0, 0x38);
	} else if (spmfw_dram_type == SPMFW_LP4X_2CH_3200) {
		if (suspend)
			spm_write(DVFSRC_EMI_MD2SPM0, 0x0);
		else
			spm_write(DVFSRC_EMI_MD2SPM0, 0x38);
	} else if ((spmfw_dram_type == SPMFW_LP4X_2CH_3733) ||
		   (spmfw_dram_type == SPMFW_LP4_2CH_2400)) {
		if (suspend)
			spm_write(DVFSRC_EMI_MD2SPM0, 0x80C0);
		else
			spm_write(DVFSRC_EMI_MD2SPM0, 0x80F8);
	} else {
		spm_vcorefs_warn("un-support spmfw_dram_type: %d\n",
				spmfw_dram_type);
	}
}

static bool is_screen_off;
void dvfsrc_md_scenario_update_to_fb(bool suspend)
{
	if (!suspend && is_screen_off)
		return; /* scrren off stay suspend setting */

	dvfsrc_md_scenario_update(suspend);
}

void dvfsrc_set_vcore_request(unsigned int mask,
			unsigned int shift, unsigned int level)
{
	int opp, r = 0;
	unsigned long flags;
	unsigned int val;

	opp = VCORE_OPP_NUM - 1 - level;
	opp = get_min_opp_for_vcore(opp);
	spin_lock_irqsave(&__spm_lock, flags);

	/* check DVFS idle */
	r = wait_spm_complete_by_condition(is_dvfs_in_progress() == 0,
					SPM_DVFS_TIMEOUT);
	if (r < 0) {
		spm_vcorefs_warn("[%s]wait idle timeout!(level=%d, opp=%d)\n",
				__func__, level, opp);
		spm_vcorefs_dump_dvfs_regs(NULL);
		aee_kernel_warning("VCOREFS", "wait idle timeout");
		goto out;
	}

	val = (spm_read(DVFSRC_VCORE_REQUEST)
		& ~(mask << shift)) | (level << shift);
	spm_write(DVFSRC_VCORE_REQUEST, val);

	r = wait_spm_complete_by_condition(spm_vcorefs_get_dvfs_opp() <= opp,
					SPM_DVFS_TIMEOUT);
	if (r < 0) {
		spm_vcorefs_warn
			("[%s]vcore wait complete timeout!(level=%d, opp=%d)\n",
			__func__, level, opp);
		spm_vcorefs_dump_dvfs_regs(NULL);
		aee_kernel_warning("VCOREFS", "wait complete timeout");
	}

out:
	spin_unlock_irqrestore(&__spm_lock, flags);
}

static int scp_vcore_level;
void dvfsrc_set_scp_vcore_request(unsigned int level)
{
	if (is_force_opp_enable())
		return;

	if (is_vcorefs_can_work() != 1) {
		scp_vcore_level = level;
		return;
	}
	dvfsrc_set_vcore_request(0x3, 30, (level & 0x3));
}

void dvfsrc_set_power_model_ddr_request(unsigned int level)
{
	unsigned int val;

	if (is_force_opp_enable())
		return;

	val = (spm_read(DVFSRC_SW_REQ2) & ~(0x3)) | level;
	spm_write(DVFSRC_SW_REQ2, val);
}

static void dvfsrc_init(void)
{
	unsigned long flags;

	if (is_force_opp_enable())
		return;

	spin_lock_irqsave(&__spm_lock, flags);

	if (__spm_get_dram_type() == SPMFW_LP3_1CH_1866) {
		/* LP3 1CH 1866 */
		spm_write(DVFSRC_LEVEL_LABEL_0_1, 0x00100000);
		spm_write(DVFSRC_LEVEL_LABEL_2_3, 0x00210011);
		spm_write(DVFSRC_LEVEL_LABEL_4_5, 0x01100100);
		spm_write(DVFSRC_LEVEL_LABEL_6_7, 0x01210111);
		spm_write(DVFSRC_LEVEL_LABEL_8_9, 0x02100200);
		spm_write(DVFSRC_LEVEL_LABEL_10_11, 0x02210211);
		spm_write(DVFSRC_LEVEL_LABEL_12_13, 0x03210321);
		spm_write(DVFSRC_LEVEL_LABEL_14_15, 0x03210321);

		spm_write(DVFSRC_EMI_QOS0, 0x26);
		spm_write(DVFSRC_EMI_QOS1, 0x32);
		spm_write(DVFSRC_EMI_MD2SPM0, 0x38);
		spm_write(DVFSRC_EMI_MD2SPM1, 0x80C0);
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x80C0);

	} else if (__spm_get_dram_type() == SPMFW_LP4X_2CH_3200) {
		/* LP4 2CH 3200 */
		spm_write(DVFSRC_LEVEL_LABEL_0_1, 0x00100000);
		spm_write(DVFSRC_LEVEL_LABEL_2_3, 0x00210020);
		spm_write(DVFSRC_LEVEL_LABEL_4_5, 0x01100100);
		spm_write(DVFSRC_LEVEL_LABEL_6_7, 0x01210120);
		spm_write(DVFSRC_LEVEL_LABEL_8_9, 0x02100200);
		spm_write(DVFSRC_LEVEL_LABEL_10_11, 0x02210220);
		spm_write(DVFSRC_LEVEL_LABEL_12_13, 0x03210321);
		spm_write(DVFSRC_LEVEL_LABEL_14_15, 0x03210321);

		spm_write(DVFSRC_EMI_QOS0, 0x32);
		spm_write(DVFSRC_EMI_QOS1, 0x4C);
		spm_write(DVFSRC_EMI_MD2SPM0, 0x38);
		spm_write(DVFSRC_EMI_MD2SPM1, 0x80C0);
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x80C0);
	} else if ((__spm_get_dram_type() == SPMFW_LP4X_2CH_3733) ||
		   (__spm_get_dram_type() == SPMFW_LP4_2CH_2400)) {
		/* LP4 2CH 3600 */
		spm_write(DVFSRC_LEVEL_LABEL_0_1, 0x00100000);
		spm_write(DVFSRC_LEVEL_LABEL_2_3, 0x00210011);
		spm_write(DVFSRC_LEVEL_LABEL_4_5, 0x01100100);
		spm_write(DVFSRC_LEVEL_LABEL_6_7, 0x01210111);
		spm_write(DVFSRC_LEVEL_LABEL_8_9, 0x02100200);
		spm_write(DVFSRC_LEVEL_LABEL_10_11, 0x02210211);
		spm_write(DVFSRC_LEVEL_LABEL_12_13, 0x03210321);
		spm_write(DVFSRC_LEVEL_LABEL_14_15, 0x03210321);

		/* todo: EMI/VCORE HRT, MD2SPM, BW setting */
		spm_write(DVFSRC_EMI_QOS0, 0x32);
		spm_write(DVFSRC_EMI_QOS1, 0x66);
		spm_write(DVFSRC_EMI_MD2SPM0, 0x80F8);
		spm_write(DVFSRC_EMI_MD2SPM1, 0x0);
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x80C0);
	}

	spm_write(DVFSRC_RSRV_1, 0x0000001C);
	spm_write(DVFSRC_TIMEOUT_NEXTREQ, 0x00000013);
	spm_write(DVFSRC_INT_EN, 0x2);

	spm_write(DVFSRC_EMI_REQUEST, 0x00290209);
	spm_write(DVFSRC_EMI_REQUEST2, 0);

	spm_write(DVFSRC_VCORE_REQUEST, 0x00150000);
	/* spm_write(DVFSRC_VCORE_REQUEST2, 0x29000000); */

#if defined(CONFIG_MTK_QOS_SUPPORT)
	spm_write(DVFSRC_QOS_EN, 0x0000407F);
	spm_write(DVFSRC_EMI_REQUEST3, 0x09000000);
#else
	spm_write(DVFSRC_QOS_EN, 0x00000000);
#endif
	spm_write(DVFSRC_FORCE, 0x00400000);
	spm_write(DVFSRC_BASIC_CONTROL, 0x0000C07B);
	spm_write(DVFSRC_BASIC_CONTROL, 0x0000017B);

	mtk_rgu_cfg_dvfsrc(1);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

static void dvfsrc_register_init(void)
{
	struct device_node *node;

	/* dvfsrc */
	node = of_find_compatible_node(NULL, NULL, "mediatek,dvfsrc_top");
	if (!node) {
		spm_vcorefs_err("[DVFSRC] find node failed\n");
		goto dvfsrc_exit;
	}

	dvfsrc_base = of_iomap(node, 0);
	if (!dvfsrc_base) {
		spm_vcorefs_err("[DVFSRC] base failed\n");
		goto dvfsrc_exit;
	}

	qos_sram_base = of_iomap(node, 1);
	if (!qos_sram_base) {
		spm_vcorefs_err("[QOS_SRAM] base failed\n");
		goto dvfsrc_exit;
	}

dvfsrc_exit:

	spm_vcorefs_warn("spm_dvfsrc_register_init: dvfsrc_base = %p\n",
			dvfsrc_base);
}

void spm_check_status_before_dvfs(void)
{
	int flag;

	if (spm_read(PCM_REG15_DATA) != 0x0)
		return;

	flag = spm_dvfs_flag_init();

	spm_dvfsfw_init(spm_vcorefs_get_opp());

	spm_go_to_vcorefs(flag);
}

int spm_set_vcore_dvfs(struct kicker_config *krconf)
{
	unsigned long flags;
	int r = 0;
	u32 autok_kir_group = AUTOK_KIR_GROUP;
	bool fix = (((1U << krconf->kicker) & autok_kir_group) ||
			krconf->kicker == KIR_SYSFSX) &&
			krconf->opp != OPP_UNREQ;
	int opp = fix ? krconf->opp : krconf->dvfs_opp;

	spm_check_status_before_dvfs();

	spm_vcorefs_footprint(SPM_VCOREFS_ENTER);

	spin_lock_irqsave(&__spm_lock, flags);

	spm_vcorefs_footprint(SPM_VCOREFS_DVFS_START);

	r = spm_trigger_dvfs(krconf->kicker, opp, fix);

	spm_vcorefs_footprint(SPM_VCOREFS_DVFS_END);

	spm_vcorefs_footprint(SPM_VCOREFS_LEAVE);

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_vcorefs_footprint(0);

	return r;
}

void spm_go_to_vcorefs(int spm_flags)
{
	unsigned long flags;

	spm_vcorefs_warn("pcm_flag: 0x%x\n", spm_flags);

	spin_lock_irqsave(&__spm_lock, flags);

	SMC_CALL(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_1, spm_flags, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_vcorefs_warn("[%s] done\n", __func__);
}

void spm_request_dvfs_opp(int id, enum dvfs_opp opp)
{
	u32 emi_req[NUM_OPP] = {0x2, 0x1, 0x1, 0x0};

	if (__spm_get_dram_type() == SPMFW_LP4X_2CH_3200)
		emi_req[1] = 0x2;

	if (is_vcorefs_can_work() != 1)
		return;

	if (is_force_opp_enable())
		opp = NUM_OPP-1;

	switch (id) {
	case 0: /* ZQTX */
		if (!((__spm_get_dram_type() == SPMFW_LP4X_2CH_3733) ||
			(__spm_get_dram_type() == SPMFW_LP4X_2CH_3200) ||
			(__spm_get_dram_type() == SPMFW_LP4_2CH_2400)))
			return;
		SMC_CALL(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
				VCOREFS_SMC_CMD_2, id, emi_req[opp]);
		break;
	default:
		break;
	}
}

static void plat_info_init(void)
{
	int hw_reserve = get_devinfo_with_index(120);

	if ((hw_reserve >> 1) & 0x1)
		lt_opp_feature_en = 0;
	spm_vcorefs_warn("[%s] hw_rsv=0x%x, lt_opp_feautre_en=%d\n",
				__func__, hw_reserve, lt_opp_feature_en);
}

#define SEG_P38_6M 0x24
#define SEG_P38_5M 0x34
#define P38_DDR_MAX_khz 3300000

static void seg_info_init(void)
{
	int seg_info = get_devinfo_with_index(30);

	spm_vcorefs_warn("[%s] seg_info=0x%x\n", __func__, seg_info);

	if ((seg_info&0xFF) == SEG_P38_6M ||
	    (seg_info&0xFF) == SEG_P38_5M) {
		if (vcorefs_get_ddr_by_steps(OPP_0) > P38_DDR_MAX_khz ||
		   vcorefs_get_curr_ddr() > P38_DDR_MAX_khz) {
			vcorefs_set_vcore_dvs_en(false);
			vcorefs_set_ddr_dfs_en(false);
			spm_vcorefs_warn("disable dvfs due to segment\n");
		}
	}
}

#if 0
static int vcorefs_is_lp_flavor(void)
{
	int r = 0;
#if defined(CONFIG_ARM64) && defined(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)
	int len;

	len = sizeof(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);

	if (strncmp(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES + len - 4,
			"_lp", 3) == 0)
		r = 1;

	spm_vcorefs_warn("flavor check: %s, is_lp: %d\n",
			CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES, r);
#endif

	return r;
}
#endif

static int spm_vcorefs_fb_notifier_callback(struct notifier_block *self,
					unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		is_screen_off = false;
		dvfsrc_md_scenario_update(false);
		spm_vcorefs_warn("SCREEN ON (emi_md2spm0: 0x%x)\n",
				spm_read(DVFSRC_EMI_MD2SPM0));
		break;
	case FB_BLANK_POWERDOWN:
		is_screen_off = true;
		dvfsrc_md_scenario_update(true);
		spm_vcorefs_warn("SCREEN OFF (emi_md2spm0: 0x%x)\n",
				spm_read(DVFSRC_EMI_MD2SPM0));
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block spm_vcorefs_fb_notif = {
		.notifier_call = spm_vcorefs_fb_notifier_callback,
};

#if defined(CONFIG_MTK_QOS_SUPPORT)
static void dvfsrc_init_qos_opp(void)
{
	u32 vcore_req[VCORE_OPP_NUM] = {0x1, 0x0};
	u32 emi_req[DDR_OPP_NUM] = {0x2, 0x1, 0x0};
	int emi_opp, vcore_opp;

	emi_opp = pm_qos_request(PM_QOS_EMI_OPP);
	vcore_opp = pm_qos_request(PM_QOS_VCORE_OPP);

	if (emi_opp >= DDR_OPP_NUM)
		emi_opp = DDR_OPP_NUM - 1;
	if (vcore_opp >= VCORE_OPP_NUM)
		vcore_opp = VCORE_OPP_NUM - 1;

	spm_vcorefs_warn
		("pm_qos curr opp: emi = %d(req: %d), vcore = %d(req: %d)\n",
		emi_opp, emi_req[emi_opp], vcore_opp, vcore_req[vcore_opp]);
	/* set vcore_opp */
	if (vcore_req[vcore_opp]) {
		spm_write(DVFSRC_VCORE_REQUEST2,
			(spm_read(DVFSRC_VCORE_REQUEST2)
			& ~(0x03000000)) | (vcore_req[vcore_opp] << 24));
	}
	/* set emi_opp */
	if (emi_req[emi_opp]) {
		spm_write(DVFSRC_SW_REQ,
				(spm_read(DVFSRC_SW_REQ) & ~(0x3))
				| (emi_req[emi_opp]));
	}
	spm_vcorefs_warn("pm_qos init opp (sw_req: 0x%x, vcore_req2: 0x%x)\n",
			spm_read(DVFSRC_SW_REQ),
			spm_read(DVFSRC_VCORE_REQUEST2));
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	dvfsrc_update_sspm_ddr_opp_table(OPP_0,
			vcorefs_get_ddr_by_steps(OPP_0));
#endif
}
#endif



void vcorefs_set_lt_opp_feature(int en)
{
	lt_opp_feature_en = en;
}

void vcorefs_set_lt_opp_enter_temp(int val)
{
	enter_lt_opp_temp = val;
}

void vcorefs_set_lt_opp_leave_temp(int val)
{
	leave_lt_opp_temp = val;
}

void vcorefs_temp_opp_init(void)
{
#ifdef CONFIG_THERMAL
	int init_temp = tscpu_min_temperature();
#else
	int init_temp = 40000;
#endif

	int spmfw_dram_type = __spm_get_dram_type();
	int vcore_val;

	if (!lt_opp_feature_en)
		return;

	if (init_temp == CLEAR_TEMP)
		return;

	spm_vcorefs_warn("[%s] init temp: %d (enter_lt: %d, leave_lt: %d\n)",
			__func__, init_temp,
			enter_lt_opp_temp, leave_lt_opp_temp);
	pm_qos_add_request(&temp_emi_req,
			PM_QOS_EMI_OPP, PM_QOS_EMI_OPP_DEFAULT_VALUE);
	pm_qos_add_request(&temp_vcore_req,
			PM_QOS_VCORE_OPP, PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	if (init_temp < enter_lt_opp_temp) {
		lt_opp_enable = 1;
		vcore_val = get_vcore_opp_volt(VCORE_DVFS_OPP_0) + adj_vcore_uv;

		update_vcore_opp_uv(VCORE_DVFS_OPP_0, vcore_val);
		if (spmfw_dram_type != SPMFW_LP4X_2CH_3200)
			update_vcore_opp_uv(VCORE_DVFS_OPP_1, vcore_val);
	}
}

void vcorefs_temp_opp_config(int temp)
{
	int vcore_val;
	int pre_vcore, pre_ddr;

	if (!lt_opp_feature_en)
		return;

	if (is_vcorefs_can_work() != 1)
		return;

	if (temp == CLEAR_TEMP)
		return;

	if ((temp > leave_lt_opp_temp) && (lt_opp_enable == 1)) {
		lt_opp_enable = 0;
		vcore_val = get_vcore_opp_volt(VCORE_DVFS_OPP_0) - adj_vcore_uv;

		/* update dvfs cmd table */
		update_vcore_opp_uv(VCORE_DVFS_OPP_0, vcore_val);
		if (__spm_get_dram_type() != SPMFW_LP4X_2CH_3200)
			update_vcore_opp_uv(VCORE_DVFS_OPP_1, vcore_val);

		/* appy adjust vcore */
		if (get_cur_vcore_opp() == VCORE_OPP_0) {
			pre_vcore = vcorefs_get_curr_vcore();
			pre_ddr = vcorefs_get_curr_ddr();
			pm_qos_update_request(&temp_vcore_req, VCORE_OPP_0);
			pm_qos_update_request(&temp_emi_req, DDR_OPP_0);

			pmic_set_register_value_nolock(PMIC_RG_BUCK_VCORE_VOSEL,
					vcore_uv_to_pmic(vcore_val));
			spm_vcorefs_warn
				("lt_opp change!! from (%d, %d) to (%d, %d)\n",
				pre_vcore, pre_ddr, vcorefs_get_curr_vcore(),
				vcorefs_get_curr_ddr());
			pm_qos_update_request(&temp_emi_req,
				PM_QOS_EMI_OPP_DEFAULT_VALUE);
			pm_qos_update_request(&temp_vcore_req,
				PM_QOS_VCORE_OPP_DEFAULT_VALUE);
		}

		spm_vcorefs_warn
		("leave lt_opp vtable[%d, %d, %d, %d] temp: %d > %d (%d, %d)\n",
				get_vcore_opp_volt(0), get_vcore_opp_volt(1),
				get_vcore_opp_volt(2), get_vcore_opp_volt(3),
				temp, leave_lt_opp_temp,
				vcorefs_get_curr_vcore(),
				vcorefs_get_curr_ddr());
	} else if ((temp < enter_lt_opp_temp) && (lt_opp_enable == 0)) {
		lt_opp_enable = 1;
		vcore_val = get_vcore_opp_volt(VCORE_DVFS_OPP_0) + adj_vcore_uv;

		/* update dvfs cmd table */
		update_vcore_opp_uv(VCORE_DVFS_OPP_0, vcore_val);
		if (__spm_get_dram_type() != SPMFW_LP4X_2CH_3200)
			update_vcore_opp_uv(VCORE_DVFS_OPP_1, vcore_val);


		/* appy adjust vcore */
		if (get_cur_vcore_opp() == VCORE_OPP_0) {
			pre_vcore = vcorefs_get_curr_vcore();
			pre_ddr = vcorefs_get_curr_ddr();
			pm_qos_update_request(&temp_vcore_req, VCORE_OPP_0);
			pm_qos_update_request(&temp_emi_req, DDR_OPP_0);
			pmic_set_register_value_nolock(PMIC_RG_BUCK_VCORE_VOSEL,
					vcore_uv_to_pmic(vcore_val));
			spm_vcorefs_warn
				("lt_opp change! from (%d, %d) to (%d, %d)\n",
				pre_vcore, pre_ddr, vcorefs_get_curr_vcore(),
						vcorefs_get_curr_ddr());
			pm_qos_update_request(&temp_emi_req,
						PM_QOS_EMI_OPP_DEFAULT_VALUE);
			pm_qos_update_request(&temp_vcore_req,
						PM_QOS_VCORE_OPP_DEFAULT_VALUE);
		}
		spm_vcorefs_warn
		("enter lt_opp vtable[%d, %d, %d, %d] temp: %d < %d (%d, %d)\n",
			get_vcore_opp_volt(0), get_vcore_opp_volt(1),
			get_vcore_opp_volt(2), get_vcore_opp_volt(3),
			temp, leave_lt_opp_temp,
			vcorefs_get_curr_vcore(), vcorefs_get_curr_ddr());
	}
	last_temp = temp;
}


void spm_vcorefs_init(void)
{
	int flag;
	int r;
#if 0
	if (vcorefs_is_lp_flavor()) {
		vcorefs_set_vcore_dvs_en(true);
		vcorefs_set_ddr_dfs_en(true);
	}
#endif
	seg_info_init();
	dvfsrc_register_init();
	vcorefs_module_init();
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	helio_dvfsrc_sspm_ipi_init(is_vcorefs_feature_enable(),
				__spm_get_dram_type());
#endif
	plat_info_init();
	vcore_opp_init();

	if (lt_opp_feature_en)
		vcorefs_temp_opp_init();

	vcorefs_init_opp_table();

	r = fb_register_client(&spm_vcorefs_fb_notif);
	if (r)
		vcorefs_err("FAILED TO REGISTER FB CLIENT (%d)\n", r);

	if (is_vcorefs_feature_enable()) {
		flag = spm_dvfs_flag_init();
		spm_dvfsfw_init(spm_vcorefs_get_opp());
		spm_go_to_vcorefs(flag);
		dvfsrc_init();
		vcorefs_late_init_dvfs();
		if (scp_vcore_level) {
			spm_vcorefs_warn("[%s] set_scp_vcore_req, level=%d\n",
					__func__, scp_vcore_level);
			dvfsrc_set_scp_vcore_request(scp_vcore_level);
			scp_vcore_level = 0;
		}
#if defined(CONFIG_MTK_QOS_SUPPORT)
		dvfsrc_init_qos_opp();
#endif
		spm_vcorefs_warn("[%s] DONE\n", __func__);
	} else {
		#if VMODEM_VCORE_COBUCK
		flag = SPM_FLAG_RUN_COMMON_SCENARIO
			| SPM_FLAG_DIS_VCORE_DVS
			| SPM_FLAG_DIS_VCORE_DFS;
		spm_dvfsfw_init(spm_vcorefs_get_opp());
		spm_go_to_vcorefs(flag);
		dvfsrc_init();
		#endif
		spm_vcorefs_warn("[%s] VCORE DVFS IS DISABLE\n", __func__);
	}
}

/* met profile table */
u32 met_vcorefs_info[INFO_MAX];
u32 met_vcorefs_src[SRC_MAX];

char *met_info_name[INFO_MAX] = {
	"OPP",
	"FREQ",
	"VCORE",
	"SW_RSV5",
};

char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"QOS_EMI_LEVEL",
	"QOS_VCORE_LEVEL",
	"CM_MGR_LEVEL",
	"TOTAL_EMI_LEVEL_1",
	"TOTAL_EMI_LEVEL_2",
	"TOTAL_EMI_MON_BW",
	"QOS_BW_LEVEL1",
	"QOS_BW_LEVEL2",
	"SCP_VCORE_LEVEL",
};

/* met profile function */
int vcorefs_get_opp_info_num(void)
{
	return INFO_MAX;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_num);

int vcorefs_get_src_req_num(void)
{
	return SRC_MAX;
}
EXPORT_SYMBOL(vcorefs_get_src_req_num);

char **vcorefs_get_opp_info_name(void)
{
	return met_info_name;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_name);

char **vcorefs_get_src_req_name(void)
{
	return met_src_name;
}
EXPORT_SYMBOL(vcorefs_get_src_req_name);

unsigned int *vcorefs_get_opp_info(void)
{
	int opp = spm_vcorefs_get_dvfs_opp();

	met_vcorefs_info[INFO_OPP_IDX] = opp;
	met_vcorefs_info[INFO_FREQ_IDX] = vcorefs_get_ddr_by_steps(opp);
	met_vcorefs_info[INFO_VCORE_IDX] = get_vcore_opp_volt(opp);
	met_vcorefs_info[INFO_SW_RSV5_IDX] = spm_read(SPM_SW_RSV_5);

	return met_vcorefs_info;
}
EXPORT_SYMBOL(vcorefs_get_opp_info);

unsigned int *vcorefs_get_src_req(void)
{
	u32 qos_total_bw = spm_read(DVFSRC_SW_BW_0) +
			   spm_read(DVFSRC_SW_BW_1) +
			   spm_read(DVFSRC_SW_BW_2) +
			   spm_read(DVFSRC_SW_BW_3) +
			   spm_read(DVFSRC_SW_BW_4);
	u32 total_bw_status = get_emi_bwst(0);
	u32 total_bw_last = (get_emi_bwvl(0) & 0x7F) * 813;

	u32 qos0_thres = spm_read(DVFSRC_EMI_QOS0);
	u32 qos1_thres = spm_read(DVFSRC_EMI_QOS1);

	met_vcorefs_src[SRC_MD2SPM_IDX] = spm_read(MD2SPM_DVFS_CON);
	met_vcorefs_src[SRC_QOS_EMI_LEVEL_IDX] =
			spm_read(DVFSRC_SW_REQ) & 0x3;
	met_vcorefs_src[SRC_QOS_VCORE_LEVEL_IDX] =
			(spm_read(DVFSRC_VCORE_REQUEST2) >> 24) & 0x3;

	met_vcorefs_src[SRC_CM_MGR_LEVEL_IDX] =
			spm_read(DVFSRC_SW_REQ2) & 0x3;
	met_vcorefs_src[SRC_TOTAL_EMI_LEVEL_1_IDX] =
			total_bw_status & 0x1;
	met_vcorefs_src[SRC_TOTAL_EMI_LEVEL_2_IDX] =
			(total_bw_status >> 1) & 0x1;
	met_vcorefs_src[SRC_TOTAL_EMI_MON_BW_IDX] = total_bw_last;
	met_vcorefs_src[SRC_QOS_BW_LEVEL1_IDX] =
			(qos_total_bw >= qos0_thres) ? 1 : 0;
	met_vcorefs_src[SRC_QOS_BW_LEVEL2_IDX] =
			(qos_total_bw >= qos1_thres) ? 1 : 0;

	met_vcorefs_src[SRC_SCP_VCORE_LEVEL_IDX] =
			(spm_read(DVFSRC_VCORE_REQUEST) >> 30) & 0x3;
	return met_vcorefs_src;
}
EXPORT_SYMBOL(vcorefs_get_src_req);

MODULE_DESCRIPTION("SPM VCORE-DVFS DRIVER");
