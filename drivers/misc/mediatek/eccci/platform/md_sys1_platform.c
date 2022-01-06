// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
#include <linux/clk.h>
#ifdef USING_PM_RUNTIME
#include <linux/pm_runtime.h>
#else
#include <dt-bindings/clock/mt6779-clk.h>
#endif
#ifdef CONFIG_MTK_EMI_BWL
#include <emi_mbw.h>
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

#ifdef CONFIG_MTK_QOS_SUPPORT
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif

#include <linux/regulator/consumer.h> /* for MD PMIC */

#include "ccci_core.h"
#include "ccci_platform.h"
#include "modem_sys.h"

#include "md_sys1_platform.h"
#include "modem_secure_base.h"
#include "modem_reg_base.h"
#include "ap_md_reg_dump.h"

#if IS_ENABLED(CONFIG_MTK_PBM)
#include "mtk_pbm.h"
#endif

struct ccci_md_regulator {
	struct regulator *reg_ref;
	unsigned char *reg_name;
	unsigned long reg_vol0;
	unsigned long reg_vol1;
};
static struct ccci_md_regulator md_reg_table[] = {
	{ NULL, "md_vmodem", 825000, 825000},
	{ NULL, "md_vnr", 825000, 825000},
	{ NULL, "md_vmdfe", 0, 0},
	{ NULL, "md_vsram", 825000, 825000},
	{ NULL, "md_vdigrf", 700000, 700000},
};

static struct ccci_plat_val md_cd_plat_val_ptr;

static struct ccci_clk_node clk_table[] = {
/* #ifdef USING_PM_RUNTIME */
	{ NULL, "scp-sys-md1-main"},
/* #endif */
};
#define TAG "mcd"

#define ROr2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)|c))
#define RAnd2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)&c))
#define RabIsc(a, b, c) ((ccci_read32(a, b)&c) != c)

#ifdef ENABLE_DEBUG_DUMP /* Fix me! */
void md1_subsys_debug_dump(enum subsys_id sys)
{
	struct ccci_modem *md = NULL;

	if (sys != SYS_MD1)
		return;
		/* add debug dump */

	CCCI_NORMAL_LOG(0, TAG, "%s\n", __func__);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, TAG, "%s dump start\n", __func__);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF |
			DUMP_FLAG_REG | DUMP_FLAG_QUEUE_0_1 |
			DUMP_MD_BOOTUP_STATUS, NULL, 0);
		mdelay(1000);
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	}
	CCCI_NORMAL_LOG(0, TAG, "%s exit\n", __func__);
}


struct pg_callbacks md1_subsys_handle = {
	.debug_dump = md1_subsys_debug_dump,
};

void ccci_dump(void)
{
	md1_subsys_debug_dump(SYS_MD1);
}
EXPORT_SYMBOL(ccci_dump);
#endif

/* md1 sys_clk_cg no need set in this API*/
static void ccci_set_clk_cg(struct ccci_modem *md, unsigned int on)
{
}

static int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	md_info->md_boot_slave_En =
	 ioremap_wc(md->hw_info->md_boot_slave_En, 0x4);
	md_info->md_rgu_base =
	 ioremap_wc(md->hw_info->md_rgu_base, 0x300);

	md_reg = kzalloc(sizeof(struct md_pll_reg), GFP_KERNEL);
	if (md_reg == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
		 "md_sw_init:alloc md reg map mem fail\n");
		return -1;
	}

	md_reg->md_boot_stats_select =
		ioremap_wc(MD1_BOOT_STATS_SELECT, 4);
	md_reg->md_boot_stats = ioremap_wc(MD1_CFG_BOOT_STATS, 4);
	/*just for dump end*/

	md_info->md_pll_base = md_reg;

#ifdef MD_PEER_WAKEUP
	md_info->md_peer_wakeup = ioremap_wc(MD_PEER_WAKEUP, 0x4);
#endif
	return 0;
}

void md_cd_lock_modem_clock_src(int locked)
{
	int settle = -1;
	struct arm_smccc_res res = {0};

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
		MD_REG_AP_MDSRC_REQ, locked, 0, 0, 0, 0, &res);
	if (res.a0)
		CCCI_ERROR_LOG(-1, TAG,
			"md clock source requeset ret = 0x%llX\n", res.a0);

	if (locked) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_SETTLE, 0, 0, 0, 0, 0, &res);

		CCCI_MEM_LOG_TAG(-1, TAG,
			"a0 = 0x%llX; a1 = 0x%llX\n", res.a0, res.a1);

		if (res.a1 == 0 && res.a0 > 0 && res.a0 < 10)
			settle = res.a0; /* ATF */
		else if (res.a0 == 0 && res.a1 > 0 && res.a1 < 10)
			settle = res.a1; /* TF-A */
		else {
			settle = 3;
			CCCI_ERROR_LOG(-1, TAG,
				"settle fail (0x%llX, 0x%llX) set = %d\n",
				res.a0, res.a1, settle);
		}
		mdelay(settle);

		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_ACK, 0, 0, 0, 0, 0, &res);
		CCCI_MEM_LOG_TAG(-1, TAG,
			"settle = %d; ret = 0x%llX\n", settle, res.a0);
		CCCI_NOTICE_LOG(-1, TAG,
			"settle = %d; ret = 0x%llX\n", settle, res.a0);
	}
}

static void md_cd_get_md_bootup_status(
	unsigned int *buff, int length)
{
	struct arm_smccc_res res = {0};

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_BOOT_STATUS, 0, 0, 0, 0, 0, &res);

	if (buff && (length >= 2)) {
		buff[0] = (unsigned int)res.a1;
		buff[1] = (unsigned int)res.a2;
	}

	CCCI_NOTICE_LOG(-1, TAG,
		"[%s] AP: boot_ret=%lu, boot_status_0=%lX, boot_status_1=%lX\n",
		__func__, res.a0, res.a1, res.a2);
}

static void md_cd_dump_debug_register(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[
		CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };

	/* EMI debug feature */
#if IS_ENABLED(CONFIG_MTK_EMI)
	mtk_emidbg_dump();
#endif

	md_cd_get_md_bootup_status(reg_value, 2);
	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"((reg_value[0] == 0) && (ccif_sram[1] == 0))\n");
		return;
	} else if (!((reg_value[0] == 0x5443000CU) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF))) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"0x%X, no dump\n", reg_value[0]);
		return;
	}
	if (unlikely(in_interrupt())) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}

	md_cd_lock_modem_clock_src(1);

	if (md_cd_plat_val_ptr.md_gen >= 6298)
		md_dump_reg(md->index);

	md_cd_lock_modem_clock_src(0);

}

static void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

static void md1_pmic_setting_init(struct platform_device *plat_dev)
{
	int idx, ret = 0;
	int value[2] = { 0, 0 };

	if (plat_dev->dev.of_node == NULL) {
		CCCI_ERROR_LOG(0, TAG, "modem OF node NULL\n");
		return;
	}

	CCCI_BOOTUP_LOG(-1, TAG, "get pmic setting\n");
	for (idx = 0; idx < ARRAY_SIZE(md_reg_table); idx++) {
		md_reg_table[idx].reg_ref =
			devm_regulator_get_optional(&plat_dev->dev,
			md_reg_table[idx].reg_name);
		/* get regulator fail and set reg_vol=0 */
		if (IS_ERR(md_reg_table[idx].reg_ref)) {
			ret = PTR_ERR(md_reg_table[idx].reg_ref);
			if ((ret != -ENODEV) && plat_dev->dev.of_node) {
				CCCI_ERROR_LOG(-1, TAG,
					"%s:get regulator(%s) fail, ret = %d\n",
					__func__, md_reg_table[idx].reg_name, ret);
				md_reg_table[idx].reg_vol0 = 0;
				md_reg_table[idx].reg_vol0 = 0;
			}
		} else {
			/* get regulator success and get value from dts */
			ret = of_property_read_u32_array(plat_dev->dev.of_node,
				md_reg_table[idx].reg_name, value, ARRAY_SIZE(value));
			if (!ret) {
				/* get value success, and update table voltage value */
				md_reg_table[idx].reg_vol0 = value[0];
				md_reg_table[idx].reg_vol1 = value[1];
			} else
				CCCI_ERROR_LOG(-1, TAG, "update vol:%s fail ret=%d\n",
					md_reg_table[idx].reg_name, ret);

			CCCI_BOOTUP_LOG(-1, TAG,
				"get regulator(%s=%ld %d) successfully\n",
				md_reg_table[idx].reg_name,
				md_reg_table[idx].reg_vol0, md_reg_table[idx].reg_vol1);
		}
	}
}

static void md1_pmic_setting_on(void)
{
	int ret = -1, idx;

	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON]%s start\n", __func__);
	CCCI_NORMAL_LOG(-1, TAG, "[POWER ON]%s start\n", __func__);

	for (idx = 0; idx < ARRAY_SIZE(md_reg_table); idx++) {
		if (IS_ERR(md_reg_table[idx].reg_ref)) {
			ret = PTR_ERR(md_reg_table[idx].reg_ref);
			if (ret != -ENODEV) {
				CCCI_ERROR_LOG(-1, TAG,
					"%s:get regulator(%s) fail, ret = %d\n",
					__func__, md_reg_table[idx].reg_name, ret);
				CCCI_BOOTUP_LOG(-1, TAG, "bypass pmic_%s set\n",
						md_reg_table[idx].reg_name);
				continue;
			}
		} else {
			/* VMODEM+NR+FE->2ms->VSRAM_MD */
			if (strcmp(md_reg_table[idx].reg_name,
				"md_vsram") == 0)
				udelay(2000);
			ret = regulator_set_voltage(md_reg_table[idx].reg_ref,
				md_reg_table[idx].reg_vol0,
				md_reg_table[idx].reg_vol1);
			if (ret) {
				CCCI_ERROR_LOG(-1, TAG, "pmic_%s set fail\n",
					md_reg_table[idx].reg_name);
				continue;
			} else
				CCCI_BOOTUP_LOG(-1, TAG,
					"[POWER ON]pmic set_voltage %s=%ld uV\n",
					md_reg_table[idx].reg_name,
					md_reg_table[idx].reg_vol0);

			ret = regulator_sync_voltage(
				md_reg_table[idx].reg_ref);
			if (ret)
				CCCI_ERROR_LOG(-1, TAG, "pmic_%s sync fail\n",
					md_reg_table[idx].reg_name);
			else
				CCCI_BOOTUP_LOG(-1, TAG,
					"[POWER ON]pmic get_voltage %s=%d uV\n",
					md_reg_table[idx].reg_name,
					regulator_get_voltage(
					md_reg_table[idx].reg_ref));
		}
	}
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON]%s end\n", __func__);
	CCCI_NORMAL_LOG(-1, TAG, "[POWER ON]%s end\n", __func__);

}

static void flight_mode_set_by_atf(struct ccci_modem *md,
		unsigned int flightMode)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_FLIGHT_MODE_SET,
		flightMode, 0, 0, 0, 0, 0, &res);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[%s] flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
		__func__, res.a0, res.a1, res.a2, res.a3);
}

static int md_cd_topclkgen_off(struct ccci_modem *md)
{
	unsigned int reg_value;
	int ret;

	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER OFF]%s start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF]%s start\n", __func__);

	ret = regmap_read(md_cd_plat_val_ptr.topckgen_clk_base, 0, &reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:read topckgen_clk_base fail,ret=%d\n",
			__func__, ret);
		return ret;
	}
	reg_value |= ((1<<8) | (1<<9));
	regmap_write(md_cd_plat_val_ptr.topckgen_clk_base, 0, reg_value);

	ret = regmap_read(md_cd_plat_val_ptr.topckgen_clk_base, 0, &reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:read topckgen_clk_base fail,ret=%d\n",
			__func__, ret);
		return ret;
	}
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF]%s end: set md1_clk_mod = 0x%x\n",
		__func__, reg_value);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF]%s end: set md1_clk_mod = 0x%x\n",
		__func__, reg_value);


	return 0;
}

/*
 * revert sequencer setting to AOC1.0 for gen98:
 * 1.disable sequencer
 * 2.wait sequencer done
 * 3.revert mux of sequencer to AOC1.0
 */
int md1_revert_sequencer_setting(struct ccci_modem *md)
{
	void __iomem *reg = NULL;
	int count = 0;

	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] %s start\n", __func__);

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << REVERT_SEQUENCER_BIT))) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER OFF] bypass %s\n", __func__);
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER OFF] bypass %s\n", __func__);
		return 0;
	}

	reg = ioremap_wc(0x1C803000, 0x1000);
	if (reg == NULL) {
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER OFF] ioremap 0x100 bytes from 0x1C803000 fail\n");
		return -1;
	}

	/* disable sequencer */
	ccci_write32(reg, 0x204, 0x0);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] disable sequencer done\n");

	/* retry 1000 * 1ms = 1s*/
	while (1) {
		/* wait sequencer done */
		if (ccci_read32(reg, 0x310) == 0x1010001)
			break;
		count++;
		udelay(1000);
		if (count == 1000) {
			CCCI_ERROR_LOG(md->index, TAG,
				"[POWER OFF] wait sequencer fail,0x1C803200=0x%x,0x1C803204=0x%x,0x1C803208=0x%x,0x1C803310=0x%x\n",
				ccci_read32(reg, 0x200), ccci_read32(reg, 0x204),
				ccci_read32(reg, 0x208), ccci_read32(reg, 0x310));
			iounmap(reg);
			return -2;
		}
	}

	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] wait sequencer done\n");

	/* revert mux of sequencer to AOC1.0 */
	ccci_write32(reg, 0x208, 0x5000D);

	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] %s end\n", __func__);

	iounmap(reg);

	return 0;
}

static int md1_enable_sequencer_setting(struct ccci_modem *md)
{
	void __iomem *reg = NULL;
	int count = 0;

	if (!md_cd_plat_val_ptr.md_first_power_on) {
		CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF]%s:md_first_power_on=%u,exit\n",
			__func__, md_cd_plat_val_ptr.md_first_power_on);
		return 0;
	}

	CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF] %s start\n", __func__);

	reg = ioremap_wc(0x1C803000, 0x1000);
	if (reg == NULL) {
		CCCI_ERROR_LOG(
			md->index, TAG,
			"[POWER OFF] %s:ioremap 0x100 bytes from 0x1C803000 fail\n",
			__func__);
		return -1;
	}

	/* enable sequencer */
	ccci_write32(reg, 0x204, 0x1);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] %s enable sequencer done\n", __func__);

	/* retry 1000 * 1ms = 1s*/
	while (1) {
		/* wait sequencer done */
		if (ccci_read32(reg, 0x310) == 0x4040040)
			break;
		count++;
		udelay(1000);
		if (count == 1000) {
			CCCI_ERROR_LOG(
				md->index, TAG,
				"[POWER OFF] wait sequencer fail,0x1C803200=0x%x,0x1C803204=0x%x,0x1C803208=0x%x,0x1C803310=0x%x\n",
				ccci_read32(reg, 0x200),
				ccci_read32(reg, 0x204),
				ccci_read32(reg, 0x208),
				ccci_read32(reg, 0x310));
			iounmap(reg);
			return -2;
		}
	}

	iounmap(reg);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF] %s end\n", __func__);

	return 0;
}

static int md1_disable_sequencer_setting(struct ccci_modem *md)
{
	void __iomem *reg = NULL;
	int count = 0;

	if (!md_cd_plat_val_ptr.md_first_power_on) {
		CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON]%s:md_first_power_on=%u,exit\n",
			__func__, md_cd_plat_val_ptr.md_first_power_on);
		return 0;
	}

	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON] %s start\n", __func__);

	reg = ioremap_wc(0x1C803000, 0x1000);
	if (reg == NULL) {
		CCCI_ERROR_LOG(
			md->index, TAG,
			"[POWER ON] %s:ioremap 0x100 bytes from 0x1C803000 fail\n",
			__func__);
		return -1;
	}

	/* disable sequencer */
	ccci_write32(reg, 0x204, 0x0);
	CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER ON] %s:disable sequencer done\n", __func__);

	/* retry 1000 * 1ms = 1s*/
	while (1) {
		/* wait sequencer done */
		if (ccci_read32(reg, 0x310) == 0x1010001)
			break;
		count++;
		udelay(1000);
		if (count == 1000) {
			CCCI_ERROR_LOG(
				md->index, TAG,
				"[POWER OFF] wait sequencer fail,0x1C803200=0x%x,0x1C803204=0x%x,0x1C803208=0x%x,0x1C803310=0x%x\n",
				ccci_read32(reg, 0x200),
				ccci_read32(reg, 0x204),
				ccci_read32(reg, 0x208),
				ccci_read32(reg, 0x310));
			iounmap(reg);
			return -2;
		}
	}

	iounmap(reg);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON] %s end\n", __func__);

	return 0;
}

static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = -1;
	unsigned int reg_value = 0;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 0, 0);
#endif

	if (md->index != MD_SYS1) {
		CCCI_ERROR_LOG(-1, TAG,
			"[POWER OFF] MD idx error(%d)\n", md->index);
		return -1;
	}

	/* revert sequencer setting to AOC1.0 for gen98 */
	if (md_cd_plat_val_ptr.md_gen >= 6298) {
		ret = md1_revert_sequencer_setting(md);
		if (ret)
			return ret;
	}

	/* 1. power off MD MTCMOS */
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF] MD MTCMOS OFF start\n");
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] MD MTCMOS OFF start\n");
#ifdef USING_PM_RUNTIME
	ret = pm_runtime_put_sync(&md->plat_dev->dev);
#else
	clk_disable_unprepare(clk_table[0].clk_ref);
	CCCI_BOOTUP_LOG(md->index, TAG, "CCF:disable md1 clk\n");
#endif
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF] MD MTCMOS OFF end: ret = %d\n", ret);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] MD MTCMOS OFF end: ret = %d\n", ret);

	/* 1. power off srclkena for gen97 */
	if (md_cd_plat_val_ptr.md_gen == 6297 &&
	    (md_cd_plat_val_ptr.power_flow_config & (1 << SRCCLKENA_SETTING_BIT))) {
		ret = regmap_read(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA, &reg_value);
		if (ret) {
			CCCI_ERROR_LOG(md->index, TAG,
				"%s:read INFRA_AO_MD_SRCCLKENA fail,ret=%d",
				__func__, ret);
			return ret;
		}
		reg_value &= ~(0xFF);
		regmap_write(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA, reg_value);
		ret = regmap_read(md->hw_info->plat_val->infra_ao_base,
					INFRA_AO_MD_SRCCLKENA, &reg_value);
		if (ret) {
			CCCI_ERROR_LOG(md->index, TAG,
				"%s:read INFRA_AO_MD_SRCCLKENA fail,ret=%d",
				__func__, ret);
			return ret;
		}
		CCCI_BOOTUP_LOG(md->index, TAG,
			"%s: set md1_srcclkena=0x%x\n", __func__, reg_value);
	}

	flight_mode_set_by_atf(md, true);

	/* enable sequencer setting to AOC2.5 for gen98 */
	if (md_cd_plat_val_ptr.md_gen >= 6298) {
		ret = md1_enable_sequencer_setting(md);
		if (ret)
			return ret;
	}

	/* modem topclkgen off setting */
	md_cd_topclkgen_off(md);

	/* 5. DLPT */
#if IS_ENABLED(CONFIG_MTK_PBM)
	kicker_pbm_by_md(KR_MD1, false);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"Call end kicker_pbm_by_md(0,false)\n");
#endif

	return ret;
}

static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
	flight_mode_set_by_atf(md, true);
	return 0;
}

static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
	flight_mode_set_by_atf(md, false);
	return 0;
}

static int md_start_platform(struct ccci_modem *md)
{
	struct arm_smccc_res res;

	int timeout = 100; /* 100 * 20ms = 2s */
	unsigned long ret;
#ifndef USING_PM_RUNTIME
	int retval = 0;
#endif

	if ((md->per_md_data.config.setting&MD_SETTING_FIRST_BOOT) == 0)
		return 0;

	md1_pmic_setting_init(md->plat_dev);

	while (timeout > 0) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG, MD_CHECK_DONE,
			0, 0, 0, 0, 0, &res);
		ret = res.a0;
		if (!ret) {
			CCCI_ERROR_LOG(md->index, TAG, "BROM PASS\n");
			break;
		}
		timeout--;
		msleep(20);
	}
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_CHECK_FLAG, 0, 0, 0, 0, 0, &res);
	CCCI_ERROR_LOG(md->index, TAG,
		"flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
		res.a0, res.a1, res.a2, res.a3);
	CCCI_ERROR_LOG(md->index, TAG, "dummy md sys clk\n");

	md_cd_get_md_bootup_status(NULL, 0);

	if (ret != 0) {
		/* BROM */
		CCCI_ERROR_LOG(md->index, TAG, "BROM Failed\n");
	}

	md_cd_power_off(md, 0);

	return ret;
}

static int md_cd_topclkgen_on(struct ccci_modem *md)
{
	unsigned int reg_value = 0;
	int ret;

	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s start\n", __func__);

	ret = regmap_read(md_cd_plat_val_ptr.topckgen_clk_base, 0, &reg_value);
	if (ret)
		return -1;
	reg_value &= ~((1<<8) | (1<<9));
	regmap_write(md_cd_plat_val_ptr.topckgen_clk_base, 0, reg_value);

	ret = regmap_read(md_cd_plat_val_ptr.topckgen_clk_base, 0, &reg_value);
	if (ret)
		return -2;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s end: set md1_clk_mod = 0x%x\n",
		__func__, reg_value);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s end: set md1_clk_mod = 0x%x\n",
		__func__, reg_value);

	return 0;
}

static int mtk_ccci_cfg_srclken_o1_on(struct ccci_modem *md)
{
	unsigned int val = 0;
	int ret;

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << SRCLKEN_O1_BIT))) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] bypass %s step\n", __func__);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] bypass %s step\n", __func__);
		return 0;
	}

	if (md_cd_plat_val_ptr.srclken_o1_bit < 0)
		return -1;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on start\n", __func__);

	if (IS_ERR(md_cd_plat_val_ptr.spm_sleep_base)) {
		ret = -1;
		goto SRC_CLK_O1_DONE;
	}
	ret = regmap_write(md_cd_plat_val_ptr.spm_sleep_base, 0,
		0x0B160001);
	if (ret) {
		ret = -2;
		goto SRC_CLK_O1_DONE;
	}
	ret = regmap_read(md_cd_plat_val_ptr.spm_sleep_base, 0, &val);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:read spm_sleep_base fail,ret=%d\n",
				__func__, ret);
	CCCI_INIT_LOG(-1, TAG,
		"[POWER ON]%s:spm_sleep_base: val:0x%x\n",
		__func__, val);

	ret = regmap_read(md_cd_plat_val_ptr.spm_sleep_base, 8, &val);
	if (ret) {
		ret = -3;
		goto SRC_CLK_O1_DONE;
	}
	CCCI_INIT_LOG(-1, TAG,
		"[POWER ON]%s:spm_sleep_base+8: val:0x%x +\n",
		__func__, val);
	val |= md_cd_plat_val_ptr.srclken_o1_bit;

	ret = regmap_write(md_cd_plat_val_ptr.spm_sleep_base, 8, val);
	if (ret) {
		ret = -4;
		goto SRC_CLK_O1_DONE;
	}
	ret = regmap_read(md_cd_plat_val_ptr.spm_sleep_base, 8, &val);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:read spm_sleep_base+8 fail,ret=%d\n",
				__func__, ret);
	CCCI_INIT_LOG(-1, TAG,
		"[POWER ON]%s:spm_sleep_base+8: val:0x%x -\n",
		__func__, val);
SRC_CLK_O1_DONE:
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on done, ret = %d\n",
		__func__, ret);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on done, ret = %d\n",
		__func__, ret);

	return ret;
}

static int md_cd_srcclkena_setting(struct ccci_modem *md)
{
	unsigned int reg_value;
	int ret;

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << SRCCLKENA_SETTING_BIT))) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] bypass md_cd_srcclkena_setting step\n");
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] bypass md_cd_srcclkena_setting step\n");
		return 0;
	}

	ret = regmap_read(md->hw_info->plat_val->infra_ao_base,
		INFRA_AO_MD_SRCCLKENA, &reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:read INFRA_AO_MD_SRCCLKENA fail,ret=%d\n",
			__func__, ret);
		return ret;
	}

	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ret = regmap_write(md->hw_info->plat_val->infra_ao_base,
		INFRA_AO_MD_SRCCLKENA, reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:write INFRA_AO_MD_SRCCLKENA value=%u fail,ret=%d\n",
			__func__, reg_value, ret);
		return -1;
	}

	ret = regmap_read(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA, &reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:re-read INFRA_AO_MD_SRCCLKENA fail,ret=%d\n",
			__func__, ret);
	}
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
		__func__, reg_value);

	return 0;
}

static int ccci_md_pll_sec(unsigned long stage)
{
	struct arm_smccc_res res = {0};

	CCCI_NORMAL_LOG(-1, TAG, "Trigger MD pll setting, %d\n", (int)stage);

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_LK_BOOT_PLAT, stage, 0, 0, 0, 0, &res);
	if (res.a0)
		return -1;
	return 0;
}

static int delay_and_wait(int wait_us, int set_val, int wait_val)
{
#define WAITE_UNIT   (10)
	int wait_counter = 1000000/WAITE_UNIT; // 1s
	int ret;
	unsigned int val = 0;

	udelay(wait_us);
	ret = regmap_write(md_cd_plat_val_ptr.spm_sleep_base, 0x404, set_val);
	if (ret)
		return -2;

	CCCI_NORMAL_LOG(-1, TAG, "[POWER ON] polling after %dus wait\n", wait_us);

	do {
		ret = regmap_read(md_cd_plat_val_ptr.spm_sleep_base, 0x404, &val);
		if (ret) {
			CCCI_ERROR_LOG(-1, TAG,
				"%s:read spm_sleep_base fail,ret=%d\n",
					__func__, ret);
			return -3;
		}
		if ((val & 0x10) == wait_val)
			break;
		udelay(WAITE_UNIT);
		wait_counter--;
	} while (wait_counter);

	if (!wait_counter && ((val & 0x10) != wait_val)) {
		CCCI_NORMAL_LOG(-1, TAG, "[POWER ON] polling end after %dus wait, 0x%x\n",
			wait_us, val);
		return -1;
	}

	CCCI_NORMAL_LOG(-1, TAG, "[POWER ON] polling end after %dus wait\n", wait_us);

	return 0;
}

static int set_pll_wa_flow(void)
{
	unsigned long stage = 0;
	int ret;

	if (IS_ERR(md_cd_plat_val_ptr.spm_sleep_base)) {
		ret = -1;
		goto SET_PLL_END;
	}

	for (stage = 0; stage < 3; stage++) {
		ret = ccci_md_pll_sec(stage);
		switch (stage) {
		case 0:
			ret = delay_and_wait(90, 1, 0x10);
			if (ret < 0) {
				ret = -2;
				goto SET_PLL_END;
			}
			break;
		case 1:
			ret = delay_and_wait(60, 0, 0x00);
			if (ret < 0) {
				ret = -3;
				goto SET_PLL_END;
			}
			ret = delay_and_wait(250, 1, 0x10);
			if (ret < 0) {
				ret = -4;
				goto SET_PLL_END;
			}

			break;
		case 2:
			ret = delay_and_wait(500, 0, 0x00);
			if (ret < 0) {
				ret = -5;
				goto SET_PLL_END;
			}
			break;
		}
	}
SET_PLL_END:

	return ret;

}

static void md_pll_setting(struct ccci_modem *md)
{
	int ret = 0;

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << MD_PLL_SETTING)))
		return;

	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER ON] %s start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON] %s start\n", __func__);

	ret = set_pll_wa_flow();

	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER ON] %s end, %d\n", __func__, ret);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON] %s end, %d\n", __func__, ret);
}

static int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;

	/* step 1: PMIC setting */
	md1_pmic_setting_on();

	/* modem topclkgen on setting */
	ret = md_cd_topclkgen_on(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] md_cd_topclkgen_on fail, ret=%d\n", ret);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] md_cd_topclkgen_on fail, ret=%d\n", ret);
		return ret;
	}

	/* step 2: MD srcclkena setting */
	ret = md_cd_srcclkena_setting(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] md_cd_srcclkena_setting fail, ret=%d\n", ret);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] md_cd_srcclkena_setting fail, ret=%d\n", ret);
		return ret;
	}

	ret = mtk_ccci_cfg_srclken_o1_on(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] mtk_ccci_cfg_srclken_o1_on fail, ret=%d\n", ret);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] mtk_ccci_cfg_srclken_o1_on fail, ret=%d\n", ret);
		return ret;
	}

	/* disable sequencer setting to AOC2.5 for gen98 */
	if (md_cd_plat_val_ptr.md_gen >= 6298) {
		ret = md1_disable_sequencer_setting(md);
		if (ret)
			return ret;
	}

	/* steip 3: power on MD_INFRA and MODEM_TOP */
	flight_mode_set_by_atf(md, false);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON] MD MTCMOS ON start\n");
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON] MD MTCMOS ON start\n");
#ifdef USING_PM_RUNTIME
	ret = pm_runtime_get_sync(&md->plat_dev->dev);
#else
	ret = clk_prepare_enable(clk_table[0].clk_ref);
#endif
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON] MD MTCMOS ON end: ret = %d\n", ret);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON] MD MTCMOS ON end: ret = %d\n", ret);

#if IS_ENABLED(CONFIG_MTK_PBM)
	kicker_pbm_by_md(KR_MD1, true);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"Call end kicker_pbm_by_md(0,true)\n");
#endif

	if (ret)
		return ret;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 1, 0);
#endif

	md_pll_setting(md);

	/* md_first_power_on set 1 */
	md_cd_plat_val_ptr.md_first_power_on = 1;

	return 0;
}

static int md_cd_let_md_go(struct ccci_modem *md)
{
	struct arm_smccc_res res;

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER ON]set MD boot slave\n");
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON]set MD boot slave\n");

	/* make boot vector take effect */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_KERNEL_BOOT_UP, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]set MD boot slave done: ret=%lx, boot_status_0=%lx, boot_status_1=%lx, boot_slave = %lx\n",
		res.a0, res.a1, res.a2, res.a3);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]set MD boot slave done: ret=%lx, boot_status_0=%lx, boot_status_1=%lx, boot_slave = %lx\n",
		res.a0, res.a1, res.a2, res.a3);

	return 0;
}

static struct ccci_plat_ops md_cd_plat_ptr = {
	.init = &ccci_platform_init_6873,
	.md_dump_reg = &md_dump_register_6873,
	//.cldma_hw_rst = &md_cldma_hw_reset,
	.set_clk_cg = &ccci_set_clk_cg,
	.remap_md_reg = &md_cd_io_remap_md_side_register,
	.lock_modem_clock_src = &md_cd_lock_modem_clock_src,
	.get_md_bootup_status = &md_cd_get_md_bootup_status,
	.debug_reg = &md_cd_dump_debug_register,
	.check_emi_state = &md_cd_check_emi_state,
	.soft_power_off = &md_cd_soft_power_off,
	.soft_power_on = &md_cd_soft_power_on,
	.start_platform = &md_start_platform,
	.power_on = &md_cd_power_on,
	.let_md_go = &md_cd_let_md_go,
	.power_off = &md_cd_power_off,
	.vcore_config = NULL,
};

static int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	int idx = 0;
	int ret = -1;
#ifdef USING_PM_RUNTIME
	int retval = 0;
#endif

	if (dev_ptr->dev.of_node == NULL) {
		CCCI_ERROR_LOG(0, TAG, "modem OF node NULL\n");
		return -1;
	}

	memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
	ret = of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,md_id", &dev_cfg->index);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, TAG, "%s:get DTS:md_id fail\n",
			__func__);
		return -1;
	}
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"modem hw info get idx:%d\n", dev_cfg->index);
	if ((dev_cfg->index != MD_SYS1) ||
		!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}

	dev_cfg->major = 0;
	dev_cfg->minor_base = 0;
	ret = of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,cldma_capability", &dev_cfg->capability);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, TAG, "%s:get DTS:cldma_capability fail\n",
			__func__);
		return -1;
	}
	ret = of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,offset_epon_md1", &hw_info->md_epon_offset);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, TAG, "%s:get DTS:mediatek,offset_epon_md1 fail\n",
			__func__);
		hw_info->md_epon_offset = 0;
	}
	ret = of_property_read_u32_index(dev_ptr->dev.of_node,
			"mediatek,offset_epon_md1", 1, &retval);
	if (ret < 0)
		hw_info->md_l2sram_base = NULL;
	else
		hw_info->md_l2sram_base = of_iomap(dev_ptr->dev.of_node, 0);
	CCCI_NORMAL_LOG(0, TAG, "%s, val: %s, 0x%x\n", __func__,
		hw_info->md_l2sram_base?"l2sram":"mddbgss", hw_info->md_epon_offset);

	hw_info->md_wdt_irq_id =
	 irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
	hw_info->ap_ccif_irq1_id =
	 irq_of_parse_and_map(dev_ptr->dev.of_node, 2);

	/* Device tree using none flag to register irq,
	 * sensitivity has set at "irq_of_parse_and_map"
	 */
	hw_info->ap_ccif_irq1_flags = IRQF_TRIGGER_NONE;
	hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;

	hw_info->sram_size = CCIF_SRAM_SIZE;
	hw_info->md_rgu_base = MD_RGU_BASE;
	hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
	ret = of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,md_generation", &md_cd_plat_val_ptr.md_gen);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, TAG, "%s:get DTS:md_gen fail\n",
			__func__);
		return -1;
	}


	md_cd_plat_val_ptr.infra_ao_base =
			syscon_regmap_lookup_by_phandle(dev_ptr->dev.of_node,
			"ccci-infracfg");
	if (IS_ERR(md_cd_plat_val_ptr.infra_ao_base)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"infra_ao fail: NULL!\n");
		return -1;
	}

	hw_info->plat_ptr = &md_cd_plat_ptr;
	hw_info->plat_val = &md_cd_plat_val_ptr;
	if ((hw_info->plat_ptr == NULL) || (hw_info->plat_val == NULL))
		return -1;
	hw_info->plat_val->offset_epof_md1 = 7*1024+0x234;
	for (idx = 0; idx < ARRAY_SIZE(clk_table); idx++) {
		clk_table[idx].clk_ref = devm_clk_get(&dev_ptr->dev,
			clk_table[idx].clk_name);
		if (IS_ERR(clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(dev_cfg->index, TAG,
				 "md%d get %s failed\n",
					dev_cfg->index + 1,
					clk_table[idx].clk_name);
			clk_table[idx].clk_ref = NULL;
		}
	}
	md_cd_plat_val_ptr.topckgen_clk_base =
			syscon_regmap_lookup_by_phandle(dev_ptr->dev.of_node,
			"ccci-topckgen");
	if (IS_ERR(md_cd_plat_val_ptr.topckgen_clk_base)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"topckgen_clk_base fail: NULL!\n");
		return -1;
	}

/*
 * md_cd_plat_val_ptr.power_flow_config will decide use which flow:
 * bit0: means to set srcclkena
 * bit1: means to set srclken_o1_on
 * bit2: means to config md1_revert_sequencer_setting
 */
	ret = of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,power_flow_config",
		&md_cd_plat_val_ptr.power_flow_config);
	if (ret < 0) {
		md_cd_plat_val_ptr.power_flow_config = 0;
		CCCI_ERROR_LOG(0, TAG, "%s:get DTS:power_flow_config fail\n",
			__func__);
	} else
		CCCI_NORMAL_LOG(dev_cfg->index, TAG,
			"%s:power_flow_config=0x%x\n",
			__func__, md_cd_plat_val_ptr.power_flow_config);

	ret = of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,srclken_o1", &md_cd_plat_val_ptr.srclken_o1_bit);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, TAG,
			"%s:get DTS: srclken_o1 fail, no need set\n",
			__func__);
		md_cd_plat_val_ptr.srclken_o1_bit = -1;
	} else
		CCCI_NORMAL_LOG(dev_cfg->index, TAG,
			"%s:srclken_o1_bit=0x%x\n",
			__func__, md_cd_plat_val_ptr.srclken_o1_bit);

	if (hw_info->ap_ccif_irq1_id == 0 ||
		hw_info->md_wdt_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"ccif_irq1:%d, md_wdt_irq:%d\n",
			hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);
		return -1;
	}

	/* Get spm sleep base */
	md_cd_plat_val_ptr.spm_sleep_base =
			syscon_regmap_lookup_by_phandle(dev_ptr->dev.of_node,
			"ccci_spmsleep");
	if (IS_ERR(md_cd_plat_val_ptr.spm_sleep_base))
		CCCI_ERROR_LOG(0, TAG,
			"%s: get spm_sleep_base reg failed\n",
			__func__);
	else
		CCCI_INIT_LOG(-1, TAG, "spm_sleep_base:0x%lx\n",
			(unsigned long)md_cd_plat_val_ptr.spm_sleep_base);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"dev_major:%d,minor_base:%d,capability:%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ccif_irq1:%d,md_wdt_irq:%d\n",
		hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);

#ifdef USING_PM_RUNTIME
	pm_runtime_enable(&dev_ptr->dev);
	dev_pm_syscore_device(&dev_ptr->dev, true);

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON start\n");
	CCCI_NORMAL_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON start\n");

	retval = pm_runtime_get_sync(&dev_ptr->dev); /* match lk on */

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON end %d\n", retval);
	CCCI_NORMAL_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON end %d\n", retval);

#endif

	return 0;
}

static int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

static void ccci_modem_shutdown(struct platform_device *dev)
{
}

static int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

static int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

static int ccci_modem_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

static int ccci_modem_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_resume(pdev);
}

static int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;

	/* set flag for next md_start */
	md->per_md_data.config.setting |= MD_SETTING_RELOAD;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	/* restore IRQ */
#ifdef FEATURE_PM_IPO_H
	irq_set_irq_type(md_ctrl->cldma_irq_id, IRQF_TRIGGER_HIGH);
	irq_set_irq_type(md_ctrl->md_wdt_irq_id, IRQF_TRIGGER_RISING);
#endif
	return 0;
}

#include <linux/module.h>
#include <linux/platform_device.h>

static int ccci_modem_probe(struct platform_device *plat_dev)
{
	struct ccci_dev_cfg dev_cfg;
	int ret;
	struct md_hw_info *md_hw;

	/* Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md hw mem fail\n", __func__);
		return -1;
	}
	ret = md_cd_get_modem_hw_info(plat_dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get hw info fail(%d)\n", __func__, ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}
#ifdef CCCI_KMODULE_ENABLE
	ccci_init();
#endif
	ret = ccci_modem_init_common(plat_dev, &dev_cfg, md_hw);
	if (ret < 0) {
		kfree(md_hw);
		md_hw = NULL;
	}
	return ret;
}

static const struct dev_pm_ops ccci_modem_pm_ops = {
	.suspend = ccci_modem_pm_suspend,
	.resume = ccci_modem_pm_resume,
	.freeze = ccci_modem_pm_suspend,
	.thaw = ccci_modem_pm_resume,
	.poweroff = ccci_modem_pm_suspend,
	.restore = ccci_modem_pm_resume,
	.restore_noirq = ccci_modem_pm_restore_noirq,
};

#ifdef CONFIG_OF
static const struct of_device_id ccci_modem_of_ids[] = {
	{.compatible = "mediatek,mddriver",},
	{}
};
#endif

static struct platform_driver ccci_modem_driver = {

	.driver = {
		   .name = "driver_modem",
#ifdef CONFIG_OF
		   .of_match_table = ccci_modem_of_ids,
#endif

#ifdef CONFIG_PM
		   .pm = &ccci_modem_pm_ops,
#endif
		   },
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static int __init modem_cd_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_modem_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"clmda modem platform driver register fail(%d)\n",
			ret);
		return ret;
	}
	return 0;
}

module_init(modem_cd_init);

MODULE_AUTHOR("CCCI");
MODULE_DESCRIPTION("CCCI modem driver v0.1");
MODULE_LICENSE("GPL");

