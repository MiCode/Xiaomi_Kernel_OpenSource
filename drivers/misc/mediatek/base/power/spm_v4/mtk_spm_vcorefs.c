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

#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif
#include <mt-plat/mtk_chip.h>

#include <mtk_spm_misc.h>
#include <mtk_spm_vcore_dvfs.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_pmic_wrap.h>
#include <mtk_dvfsrc_reg.h>
#if defined(CONFIG_MACH_MT6739)
#include <mtk_sleep_reg_md_reg_mt6739.h>
#else
#include <mtk_sleep_reg_md_reg_mt6763.h>
#endif
#include <mtk_eem.h>
/* #include <ext_wd_drv.h> */
#include "mtk_devinfo.h"

#ifdef CONFIG_MTK_MMDVFS
#include <mmdvfs_mgr.h>
#endif

#define is_dvfs_in_progress()    (spm_read(DVFSRC_LEVEL) & 0xFFFF)
#define get_dvfs_level()         (spm_read(DVFSRC_LEVEL) >> 16)

#define MIN(a, b)                ((a) >= (b) ? (b) : (a))

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

u32 plat_channel_num;
u32 plat_chip_ver;
u32 dram_issue;

#ifdef CONFIG_MTK_MMDVFS
enum mmdvfs_lcd_size_enum plat_lcd_resolution;
#else
int plat_lcd_resolution;
#endif

enum spm_vcorefs_step {
	SPM_VCOREFS_ENTER = 0x00000001,
	SPM_VCOREFS_DVFS_START = 0x000000ff,
	SPM_VCOREFS_DVFS_END = 0x000001ff,
	SPM_VCOREFS_LEAVE = 0x000007ff,
};

static inline void spm_vcorefs_footprint(enum spm_vcorefs_step step)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_vcore_dvfs_status(step);
#endif
}

char *spm_vcorefs_dump_dvfs_regs(char *p)
{
	if (p) {
		#if defined(CONFIG_MACH_MT6739)

		#else
		p += sprintf(p, "dram_issue: 0x%x\n", dram_issue);
		#endif
/*
 * p += sprintf(p, "(v:%d)(r:%d)(c:%d)\n",
 * plat_chip_ver, plat_lcd_resolution, plat_channel_num);
 */
		#if 1
		/* DVFSRC */
		p += sprintf(p, "DVFSRC_RECORD_COUNT    : 0x%x\n",
			spm_read(DVFSRC_RECORD_COUNT));
		p += sprintf(p, "DVFSRC_LAST            : 0x%x\n",
			spm_read(DVFSRC_LAST));
		p += sprintf(p,
			"DVFSRC_RECORD_0_1~3_1: 0x%08x,0x%08x,0x%08x,0x%08x\n",
			spm_read(DVFSRC_RECORD_0_1),
			spm_read(DVFSRC_RECORD_1_1),
			spm_read(DVFSRC_RECORD_2_1),
			spm_read(DVFSRC_RECORD_3_1));
		p += sprintf(p,
			"DVFSRC_RECORD_4_1~7_1: 0x%08x,0x%08x,0x%08x,0x%08x\n",
			spm_read(DVFSRC_RECORD_4_1),
			spm_read(DVFSRC_RECORD_5_1),
			spm_read(DVFSRC_RECORD_6_1),
			spm_read(DVFSRC_RECORD_7_1));
		p += sprintf(p,
			"DVFSRC_RECORD_0_0~3_0: 0x%08x,0x%08x,0x%08x,0x%08x\n",
			spm_read(DVFSRC_RECORD_0_0),
			spm_read(DVFSRC_RECORD_1_0),
			spm_read(DVFSRC_RECORD_2_0),
			spm_read(DVFSRC_RECORD_3_0));
		p += sprintf(p,
			"DVFSRC_RECORD_4_0~7_0: 0x%08x,0x%08x,0x%08x,0x%08x\n",
			spm_read(DVFSRC_RECORD_4_0),
			spm_read(DVFSRC_RECORD_5_0),
			spm_read(DVFSRC_RECORD_6_0),
			spm_read(DVFSRC_RECORD_7_0));
		p += sprintf(p,
			"DVFSRC_RECORD_MD_0~3: 0x%08x, 0x%08x,0x%08x,0x%08x\n",
			spm_read(DVFSRC_RECORD_MD_0),
			spm_read(DVFSRC_RECORD_MD_1),
			spm_read(DVFSRC_RECORD_MD_2),
			spm_read(DVFSRC_RECORD_MD_3));
		p += sprintf(p,
			"DVFSRC_RECORD_MD_4~7:0x%08x,0x%08x,0x%08x,0x%08x\n",
			spm_read(DVFSRC_RECORD_MD_4),
			spm_read(DVFSRC_RECORD_MD_5),
			spm_read(DVFSRC_RECORD_MD_6),
			spm_read(DVFSRC_RECORD_MD_7));
		p += sprintf(p, "DVFSRC_LEVEL           : 0x%x\n",
			spm_read(DVFSRC_LEVEL));
		p += sprintf(p, "DVFSRC_VCORE_REQUEST   : 0x%x\n",
			spm_read(DVFSRC_VCORE_REQUEST));
		p += sprintf(p, "DVFSRC_EMI_REQUEST     : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST));
		p += sprintf(p, "DVFSRC_MD_REQUEST      : 0x%x\n",
			spm_read(DVFSRC_MD_REQUEST));
		p += sprintf(p, "DVFSRC_RSRV_0          : 0x%x\n",
			spm_read(DVFSRC_RSRV_0));
		/* SPM */
		p += sprintf(p, "SPM_SW_FLAG            : 0x%x\n",
			spm_read(SPM_SW_FLAG));
		p += sprintf(p, "SPM_SW_RSV_5           : 0x%x\n",
			spm_read(SPM_SW_RSV_5));
		p += sprintf(p, "DVFSRC_MD_GEAR         : 0x%x\n",
			spm_read(DVFSRC_MD_GEAR));
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

		p += sprintf(p, "SPM_DVS_LEVEL          : 0x%x\n",
			spm_read(SPM_DVS_LEVEL));

		p += sprintf(p, "PCM_REG_DATA_0~3: 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG0_DATA), spm_read(PCM_REG1_DATA),
			spm_read(PCM_REG2_DATA), spm_read(PCM_REG3_DATA));
		p += sprintf(p, "PCM_REG_DATA_4~7: 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG4_DATA), spm_read(PCM_REG5_DATA),
			spm_read(PCM_REG6_DATA), spm_read(PCM_REG7_DATA));
		p += sprintf(p, "PCM_REG_DATA_8~11: 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG8_DATA),
			spm_read(PCM_REG9_DATA),
			spm_read(PCM_REG10_DATA),
			spm_read(PCM_REG11_DATA));
		p += sprintf(p, "PCM_REG_DATA_12~15: 0x%x, 0x%x, 0x%x, 0x%x\n",
			spm_read(PCM_REG12_DATA),
			spm_read(PCM_REG13_DATA),
			spm_read(PCM_REG14_DATA),
			spm_read(PCM_REG15_DATA));
		p += sprintf(p,
			"MDPTP_VMODEM_SPM_DVFS_CMD16~19: 0x%x,0x%x,0x%x,0x%x\n",
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD16),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD17),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD18),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD19));
		p += sprintf(p, "SPM_DVFS_CMD0~1        : 0x%x, 0x%x\n",
			spm_read(SPM_DVFS_CMD0), spm_read(SPM_DVFS_CMD1));
		p += sprintf(p, "PCM_IM_PTR             : 0x%x (%u)\n",
			spm_read(PCM_IM_PTR), spm_read(PCM_IM_LEN));
		#endif
	} else {
/*
 * spm_vcorefs_warn("(v:%d)(r:%d)(c:%d)\n",
 * plat_chip_ver, plat_lcd_resolution, plat_channel_num);
 */
		#if 1
		/* DVFSRC */
		spm_vcorefs_warn("DVFSRC_RECORD_COUNT    : 0x%x\n",
			spm_read(DVFSRC_RECORD_COUNT));
		spm_vcorefs_warn("DVFSRC_LAST            : 0x%x\n",
			spm_read(DVFSRC_LAST));
		spm_vcorefs_warn("DVFSRC_RECORD_0_1~3_1  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_0_1),
			spm_read(DVFSRC_RECORD_1_1),
			spm_read(DVFSRC_RECORD_2_1),
			spm_read(DVFSRC_RECORD_3_1));
		spm_vcorefs_warn("DVFSRC_RECORD_4_1~7_1  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_4_1),
			spm_read(DVFSRC_RECORD_5_1),
			spm_read(DVFSRC_RECORD_6_1),
			spm_read(DVFSRC_RECORD_7_1));
		spm_vcorefs_warn("DVFSRC_RECORD_0_0~3_0  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_0_0),
			spm_read(DVFSRC_RECORD_1_0),
			spm_read(DVFSRC_RECORD_2_0),
			spm_read(DVFSRC_RECORD_3_0));
		spm_vcorefs_warn("DVFSRC_RECORD_4_0~7_0  : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_4_0),
			spm_read(DVFSRC_RECORD_5_0),
			spm_read(DVFSRC_RECORD_6_0),
			spm_read(DVFSRC_RECORD_7_0));
		spm_vcorefs_warn("DVFSRC_RECORD_MD_0~3   : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_MD_0),
			spm_read(DVFSRC_RECORD_MD_1),
			spm_read(DVFSRC_RECORD_MD_2),
			spm_read(DVFSRC_RECORD_MD_3));
		spm_vcorefs_warn("DVFSRC_RECORD_MD_4~7   : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			spm_read(DVFSRC_RECORD_MD_4),
			spm_read(DVFSRC_RECORD_MD_5),
			spm_read(DVFSRC_RECORD_MD_6),
			spm_read(DVFSRC_RECORD_MD_7));
		spm_vcorefs_warn("DVFSRC_LEVEL           : 0x%x\n",
			spm_read(DVFSRC_LEVEL));
		spm_vcorefs_warn("DVFSRC_VCORE_REQUEST   : 0x%x\n",
			spm_read(DVFSRC_VCORE_REQUEST));
		spm_vcorefs_warn("DVFSRC_EMI_REQUEST     : 0x%x\n",
			spm_read(DVFSRC_EMI_REQUEST));
		spm_vcorefs_warn("DVFSRC_MD_REQUEST      : 0x%x\n",
			spm_read(DVFSRC_MD_REQUEST));
		/* SPM */
		spm_vcorefs_warn("SPM_SW_FLAG            : 0x%x\n",
			spm_read(SPM_SW_FLAG));
		spm_vcorefs_warn("SPM_SW_RSV_5           : 0x%x\n",
			spm_read(SPM_SW_RSV_5));
		spm_vcorefs_warn("SPM_SW_RSV_11          : 0x%x\n",
			spm_read(SPM_SW_RSV_11));
		spm_vcorefs_warn("DVFSRC_MD_GEAR         : 0x%x\n",
			spm_read(DVFSRC_MD_GEAR));
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
		spm_vcorefs_warn("PCM_REG_DATA_0~3       : 0x%x, 0x%x, 0x%x, 0x%x\n",
				spm_read(PCM_REG0_DATA),
				spm_read(PCM_REG1_DATA),
				spm_read(PCM_REG2_DATA),
				spm_read(PCM_REG3_DATA));
		spm_vcorefs_warn("PCM_REG_DATA_4~7       : 0x%x, 0x%x, 0x%x, 0x%x\n",
				spm_read(PCM_REG4_DATA),
				spm_read(PCM_REG5_DATA),
				spm_read(PCM_REG6_DATA),
				spm_read(PCM_REG7_DATA));
		spm_vcorefs_warn("PCM_REG_DATA_8~11      : 0x%x, 0x%x, 0x%x, 0x%x\n",
				spm_read(PCM_REG8_DATA),
				spm_read(PCM_REG9_DATA),
				spm_read(PCM_REG10_DATA),
				spm_read(PCM_REG11_DATA));
		spm_vcorefs_warn("PCM_REG_DATA_12~15     : 0x%x, 0x%x, 0x%x, 0x%x\n",
				spm_read(PCM_REG12_DATA),
				spm_read(PCM_REG13_DATA),
				spm_read(PCM_REG14_DATA),
				spm_read(PCM_REG15_DATA));
		spm_vcorefs_warn("MDPTP_VMODEM_SPM_DVFS_CMD16~19   : 0x%x, 0x%x, 0x%x, 0x%x\n",
				spm_read(SLEEP_REG_MD_SPM_DVFS_CMD16),
				spm_read(SLEEP_REG_MD_SPM_DVFS_CMD17),
				spm_read(SLEEP_REG_MD_SPM_DVFS_CMD18),
				spm_read(SLEEP_REG_MD_SPM_DVFS_CMD19));
		spm_vcorefs_warn("SPM_DVFS_CMD0~1        : 0x%x, 0x%x\n",
			spm_read(SPM_DVFS_CMD0), spm_read(SPM_DVFS_CMD1));
		spm_vcorefs_warn("PCM_IM_PTR             : 0x%x (%u)\n",
			spm_read(PCM_IM_PTR), spm_read(PCM_IM_LEN));
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

#if defined(CONFIG_MACH_MT6763)
u32 spm_vcorefs_get_md_srcclkena(void)
{
	return spm_read(PCM_REG13_DATA) & (1U << 8);
}
#endif

static void spm_dvfsfw_init(int curr_opp)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
		VCOREFS_SMC_CMD_0, curr_opp, dram_issue, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

int spm_vcorefs_pwarp_cmd(void)
{
#if 1
#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if defined(CONFIG_MACH_MT6739)

	unsigned long flags;
	int opp;

	spin_lock_irqsave(&__spm_lock, flags);

	for (opp = 0; opp < NUM_OPP; opp++)
		mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_2, opp, get_vcore_ptp_volt(opp), 0);

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_vcorefs_warn("%s: atf\n", __func__);

#else
	/* PMIC_WRAP_PHASE_ALLINONE */
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_ALLINONE,
		CMD_0, get_vcore_ptp_volt(OPP_3)); /* 0.7 */
/*
 * mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_ALLINONE, CMD_0,
 * get_vcore_ptp_volt(OPP_2));
 */ /* 0.7 */
/*
 * mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_ALLINONE, CMD_1,
 * get_vcore_ptp_volt(OPP_1));
 */ /* 0.8 */
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_ALLINONE,
		CMD_1, get_vcore_ptp_volt(OPP_0)); /* 0.8 */

	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_ALLINONE);

	spm_vcorefs_warn("%s: kernel\n", __func__);

#endif
#else
	int ret;
	struct spm_data spm_d;

	memset(&spm_d, 0, sizeof(struct spm_data));

	spm_d.u.vcorefs.vcore_level0 = get_vcore_ptp_volt(OPP_0);
	spm_d.u.vcorefs.vcore_level1 = get_vcore_ptp_volt(OPP_1);
	spm_d.u.vcorefs.vcore_level2 = get_vcore_ptp_volt(OPP_2);
	spm_d.u.vcorefs.vcore_level3 = get_vcore_ptp_volt(OPP_3);

	ret = spm_to_sspm_command(SPM_VCORE_PWARP_CMD, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);

	spm_vcorefs_warn("%s: sspm\n", __func__);
#endif
#endif
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

#if defined(CONFIG_MACH_MT6739)
void dvfsrc_msdc_autok_finish(void)
{
	spm_write(DVFSRC_VCORE_REQUEST,
		spm_read(DVFSRC_VCORE_REQUEST) | (0x3 << 26));
}
#endif

static void dvfsrc_hw_policy_mask(bool force)
{
#if defined(CONFIG_MACH_MT6739)
	if (force || vcorefs_i_hwpath_en()) {
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) & ~(0xf << 0));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) & ~(0xf << 16));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 3));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 0));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 5));
		spm_write(DVFSRC_VCORE_REQUEST,
			spm_read(DVFSRC_VCORE_REQUEST) & ~(0x3 << 26));
		spm_write(DVFSRC_MD_REQUEST, 0x0);
	} else {
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) | (0x1 << 0));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) | (0x1 << 16));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 3));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 0));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 5));
	}
#else

	if (force || vcorefs_i_hwpath_en()) {
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) & ~(0xf << 0));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) & ~(0xf << 4));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) & ~(0xf << 8));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) & ~(0xf << 16));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 3));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 0));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) | (0x1 << 5));
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x0);
		spm_write(DVFSRC_MD_REQUEST, 0x0);
		spm_request_dvfs_opp(0, OPP_3);
	} else {
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) | (0x9 << 0));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) | (0x9 << 4));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) | (0x2 << 8));
		spm_write(DVFSRC_EMI_REQUEST,
			spm_read(DVFSRC_EMI_REQUEST) | (0x9 << 16));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 3));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 0));
		spm_write(DVFSRC_MD_SW_CONTROL,
			spm_read(DVFSRC_MD_SW_CONTROL) & ~(0x1 << 5));
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x8000C0C0);
	}
#endif
}

static int spm_trigger_dvfs(int kicker, int opp, bool fix)
{
	int r = 0;

#if defined(CONFIG_MACH_MT6739)
	u32 vcore_req[NUM_OPP] = {0x2, 0x2, 0x1, 0x0};
	u32 emi_req[NUM_OPP] = {0x1, 0x0, 0x0, 0x0};
	/* u32 md_req[NUM_OPP] = {0x0, 0x0, 0x0, 0x0}; */
	u32 dvfsrc_level[NUM_OPP] = {0x8, 0x4, 0x2, 0x1};

#else
	u32 vcore_req[NUM_OPP] = {0x1, 0x0, 0x0, 0x0};
	u32 emi_req[NUM_OPP] = {0x2, 0x2, 0x1, 0x0};
	/* u32 md_req[NUM_OPP] = {0x0, 0x0, 0x0, 0x0}; */
	u32 dvfsrc_level[NUM_OPP] = {0x8, 0x4, 0x2, 0x1};

	if (__spm_get_dram_type() == SPMFW_LP3_1CH) {
		vcore_req[1] = 0x1;
		emi_req[1] = 0x1;
	}
#endif

	if (fix)
		dvfsrc_hw_policy_mask(1);
	else
		dvfsrc_hw_policy_mask(0);
#if 1
	/* check DVFS idle */
	r = wait_spm_complete_by_condition(is_dvfs_in_progress() == 0,
		SPM_DVFS_TIMEOUT);
	if (r < 0) {
		spm_vcorefs_dump_dvfs_regs(NULL);
/* aee_kernel_warning("SPM Warring", "Vcore DVFS timeout warning"); */
		return -1;
	}
#endif
	spm_write(DVFSRC_VCORE_REQUEST,
		(spm_read(DVFSRC_VCORE_REQUEST) & ~(0x3 << 20))
		| (vcore_req[opp] << 20));
	spm_write(DVFSRC_EMI_REQUEST,
		(spm_read(DVFSRC_EMI_REQUEST) & ~(0x3 << 20))
		| (emi_req[opp] << 20));
/*
 * spm_write(DVFSRC_MD_REQUEST, (spm_read(DVFSRC_MD_REQUEST)
 * & ~(0x7 << 3)) | (md_req[opp] << 3));
 */

	vcorefs_crit_mask(log_mask(), kicker,
		"[%s] fix: %d, opp: %d, vcore: 0x%x, emi: 0x%x, md: 0x%x\n",
		__func__, fix, opp,
		spm_read(DVFSRC_VCORE_REQUEST), spm_read(DVFSRC_EMI_REQUEST),
		spm_read(DVFSRC_MD_REQUEST));
#if 1
	/* check DVFS timer */
	if (fix)
		r = wait_spm_complete_by_condition(
			get_dvfs_level() == dvfsrc_level[opp],
			SPM_DVFS_TIMEOUT);
	else
		r = wait_spm_complete_by_condition(
			get_dvfs_level() >= dvfsrc_level[opp],
			SPM_DVFS_TIMEOUT);

	if (r < 0) {
		spm_vcorefs_dump_dvfs_regs(NULL);
	/* aee_kernel_warning("SPM Warring", "Vcore DVFS timeout warning"); */
		return -1;
	}
#endif
	return 0;
}

int spm_dvfs_flag_init(void)
{
	int flag = SPM_FLAG_RUN_COMMON_SCENARIO;

	if (!vcorefs_vcore_dvs_en())
		flag |= SPM_FLAG_DIS_VCORE_DVS;
	if (!vcorefs_dram_dfs_en())
		flag |= SPM_FLAG_DIS_VCORE_DFS;

/*
 * flag = SPM_FLAG_RUN_COMMON_SCENARIO | SPM_FLAG_DIS_VCORE_DVS
 * | SPM_FLAG_DIS_VCORE_DFS;
 */

	return flag;
}

void dvfsrc_md_scenario_update(bool suspend)
{
#if defined(CONFIG_MACH_MT6739)

#else
	if (__spm_get_dram_type() == SPMFW_LP4X_2CH ||
			__spm_get_dram_type() == SPMFW_LP4_2CH_2400) {
		/* LP4 2CH */
		if (suspend) {
			spm_write(DVFSRC_EMI_MD2SPM0, 0x00000000);
			spm_write(DVFSRC_EMI_MD2SPM1, 0x800080C0);
			spm_write(DVFSRC_VCORE_MD2SPM0, 0x800080C0);
		} else {
			spm_write(DVFSRC_EMI_MD2SPM0, 0x0000003E);
			spm_write(DVFSRC_EMI_MD2SPM1, 0x800080C0);
			spm_write(DVFSRC_VCORE_MD2SPM0, 0x800080C0);
		}
	} else {
		/* LP3 1CH */
		if (suspend) {
			spm_write(DVFSRC_EMI_MD2SPM0, 0x000000C0);
			spm_write(DVFSRC_EMI_MD2SPM1, 0x80008000);
			spm_write(DVFSRC_VCORE_MD2SPM0, 0x800080C0);
		} else {
			spm_write(DVFSRC_EMI_MD2SPM0, 0x0000003E);
			spm_write(DVFSRC_EMI_MD2SPM1, 0x800080C0);
			spm_write(DVFSRC_VCORE_MD2SPM0, 0x800080C0);
		}
	}
#endif
}

#if defined(CONFIG_MACH_MT6739)
void dvfsrc_mdsrclkena_control_nolock(bool on)
{
	if (on)
		spm_write(DVFSRC_VCORE_REQUEST,
			spm_read(DVFSRC_VCORE_REQUEST) | (0x3 << 26));
	else
		spm_write(DVFSRC_VCORE_REQUEST,
			spm_read(DVFSRC_VCORE_REQUEST) & ~(0x3 << 26));
}

void dvfsrc_mdsrclkena_control(bool on)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	dvfsrc_mdsrclkena_control_nolock(on);

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_vcorefs_warn("mdsrclkena_control (%d)(0x%x)\n",
		on, spm_read(DVFSRC_VCORE_REQUEST));
}
#endif

static void dvfsrc_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

#if defined(CONFIG_MACH_MT6739)
	spm_write(DVFSRC_LEVEL_LABEL_0_1, 0x00010000);
	spm_write(DVFSRC_LEVEL_LABEL_2_3, 0x00130002);
	spm_write(DVFSRC_LEVEL_LABEL_4_5, 0x01010100);
	spm_write(DVFSRC_LEVEL_LABEL_6_7, 0x01130102);
	spm_write(DVFSRC_LEVEL_LABEL_8_9, 0x01130113);
	spm_write(DVFSRC_LEVEL_LABEL_10_11, 0x01130113);
	spm_write(DVFSRC_LEVEL_LABEL_12_13, 0x01130113);
	spm_write(DVFSRC_LEVEL_LABEL_14_15, 0x01130113);
	spm_write(DVFSRC_OPT_MASK, 0x0000000C);
	spm_write(DVFSRC_TIMEOUT_NEXTREQ, 0x000000FF);

	spm_write(DVFSRC_EMI_HRT, 0x00000010);
	spm_write(DVFSRC_VCORE_HRT, 0x00000010);
	spm_write(DVFSRC_EMI_MD2SPM0, 0x00000000);
	spm_write(DVFSRC_EMI_MD2SPM1, 0x00000000);
	spm_write(DVFSRC_MD_VMD_REMAP, 0x00000000);
	spm_write(DVFSRC_DEBOUNCE_FOUR, 0x0);

	spm_write(DVFSRC_EMI_REQUEST, 0x00111001);
	spm_write(DVFSRC_VCORE_REQUEST, 0x0C200000);
	spm_write(DVFSRC_FORCE, 0x00080000);
	spm_write(DVFSRC_BASIC_CONTROL, 0x0000803B);
	spm_write(DVFSRC_BASIC_CONTROL, 0x0000023B);
#else

	if (__spm_get_dram_type() == SPMFW_LP4X_2CH ||
			__spm_get_dram_type() == SPMFW_LP4_2CH_2400) {
		/* LP4 2CH */
		spm_write(DVFSRC_LEVEL_LABEL_0_1, 0x00100000);
		spm_write(DVFSRC_LEVEL_LABEL_2_3, 0x00210020);
		spm_write(DVFSRC_LEVEL_LABEL_4_5, 0x01000022);
		spm_write(DVFSRC_LEVEL_LABEL_6_7, 0x01200110);
		spm_write(DVFSRC_LEVEL_LABEL_8_9, 0x01220121);
		spm_write(DVFSRC_LEVEL_LABEL_10_11, 0x02100200);
		spm_write(DVFSRC_LEVEL_LABEL_12_13, 0x02210220);
		spm_write(DVFSRC_LEVEL_LABEL_14_15, 0x03220222);

		spm_write(DVFSRC_EMI_HRT, 0x00001C14);
		spm_write(DVFSRC_VCORE_HRT, 0x00001C1C);
		spm_write(DVFSRC_EMI_MD2SPM0, 0x0000003E);
		spm_write(DVFSRC_EMI_MD2SPM1, 0x800080C0);
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x800080C0);
	} else {
		/* LP3 1CH */
		spm_write(DVFSRC_LEVEL_LABEL_0_1, 0x00100000);
		spm_write(DVFSRC_LEVEL_LABEL_2_3, 0x00210011);
		spm_write(DVFSRC_LEVEL_LABEL_4_5, 0x01000022);
		spm_write(DVFSRC_LEVEL_LABEL_6_7, 0x01110110);
		spm_write(DVFSRC_LEVEL_LABEL_8_9, 0x01220121);
		spm_write(DVFSRC_LEVEL_LABEL_10_11, 0x02100200);
		spm_write(DVFSRC_LEVEL_LABEL_12_13, 0x02210211);
		spm_write(DVFSRC_LEVEL_LABEL_14_15, 0x03220222);

		spm_write(DVFSRC_EMI_HRT, 0x00001810);
		spm_write(DVFSRC_VCORE_HRT, 0x00001818);
		spm_write(DVFSRC_EMI_MD2SPM0, 0x0000003E);
		spm_write(DVFSRC_EMI_MD2SPM1, 0x800080C0);
		spm_write(DVFSRC_VCORE_MD2SPM0, 0x800080C0);
	}

	spm_write(DVFSRC_RSRV_1, 0x00000004);
	spm_write(DVFSRC_TIMEOUT_NEXTREQ, 0x00000011);

	spm_write(DVFSRC_EMI_REQUEST, 0x00290299);
	spm_write(DVFSRC_VCORE_REQUEST, 0x00110000);
	spm_write(DVFSRC_FORCE, 0x00080000);
	spm_write(DVFSRC_BASIC_CONTROL, 0x0000803B);
	spm_write(DVFSRC_BASIC_CONTROL, 0x0000023B);
#endif


#if 0
	mtk_rgu_cfg_dvfsrc(1);
#endif
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

dvfsrc_exit:

	spm_vcorefs_warn("spm_dvfsrc_register_init: dvfsrc_base = %p\n",
		dvfsrc_base);
}

static void spm_check_status_before_dvfs(void)
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
	bool fix = (((1U << krconf->kicker) & autok_kir_group)
			|| krconf->kicker == KIR_SYSFSX)
			&& krconf->opp != OPP_UNREQ;
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

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
		VCOREFS_SMC_CMD_1, spm_flags, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_vcorefs_warn("[%s] done\n", __func__);
}

#if defined(CONFIG_MACH_MT6763)
unsigned int dram_request_opp = OPP_3;
unsigned int cmmgr_request_opp = OPP_3;
void spm_request_dvfs_opp(int id, enum dvfs_opp opp)
{
	u32 emi_req[NUM_OPP] = {0x2, 0x2, 0x1, 0x0};
	u32 emi_req_lp3[NUM_OPP] = {0x2, 0x1, 0x1, 0x0};

	switch (id) {
	case 0: /* ZQTX */
		if (__spm_get_dram_type() != SPMFW_LP4X_2CH &&
				__spm_get_dram_type() != SPMFW_LP4_2CH_2400)
			return;

		dram_request_opp = (unsigned int)opp;
		opp = MIN(dram_request_opp, cmmgr_request_opp);

		mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_2, id, emi_req[opp], 0);
		/* spm_vcorefs_warn("DRAM ZQTX tracking request: %d\n", opp); */
		break;
	case 1: /* CMMGR */
		cmmgr_request_opp = (unsigned int)opp;
		opp = MIN(dram_request_opp, cmmgr_request_opp);

		if (__spm_get_dram_type() == SPMFW_LP3_1CH)
			mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
				VCOREFS_SMC_CMD_2, 0, emi_req_lp3[opp], 0);
		else
			mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
				VCOREFS_SMC_CMD_2, 0, emi_req[opp], 0);
		break;
	default:
		break;
	}
}
#endif

static void plat_info_init(void)
{
#if 0
	/* HW chip version */
	plat_chip_ver = mt_get_chip_sw_ver();

	/* lcd resolution */
	#ifdef CONFIG_MTK_MMDVFS
	plat_lcd_resolution = mmdvfs_get_lcd_resolution();
	#else
	plat_lcd_resolution = 46;
	#endif

	spm_vcorefs_warn("chip_ver: %d, lcd_resolution: %d channel_num: %d\n",
		plat_chip_ver, plat_lcd_resolution, plat_channel_num);
#endif
	dram_issue = get_devinfo_with_index(138);
	dram_issue = (dram_issue & (1U << 8));

	spm_vcorefs_warn("dram_issue: 0x%x\n", dram_issue);
}

void spm_vcorefs_init(void)
{
	int flag;

	dvfsrc_register_init();
	vcorefs_module_init();
	plat_info_init();

	if (is_vcorefs_feature_enable()) {
		flag = spm_dvfs_flag_init();
		vcorefs_init_opp_table();
		spm_dvfsfw_init(spm_vcorefs_get_opp());
		spm_go_to_vcorefs(flag);
		dvfsrc_init();
		vcorefs_late_init_dvfs();
		spm_vcorefs_warn("[%s] DONE\n", __func__);
	} else {
		#if VMODEM_VCORE_COBUCK
		flag = SPM_FLAG_RUN_COMMON_SCENARIO |
			SPM_FLAG_DIS_VCORE_DVS | SPM_FLAG_DIS_VCORE_DFS;
		spm_dvfsfw_init(spm_vcorefs_get_opp());
		spm_go_to_vcorefs(flag);
		dvfsrc_init();
		#endif
		spm_vcorefs_warn("[%s] VCORE DVFS IS DISABLE\n", __func__);
	}
}

MODULE_DESCRIPTION("SPM VCORE-DVFS DRIVER");
