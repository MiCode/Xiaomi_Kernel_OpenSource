// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/mfd/mt6315/registers.h>
#include <linux/pmif.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/mt6315-misc.h>
#include <linux/regulator/mt6315-regulator.h>

#define LP_INIT_SETTING_VERIFIED 1

#define MT6315_DECL_CHIP(_mid, _saddr)\
{				\
	.master_idx = _mid,	\
	.slave_addr = _saddr,	\
	.regmap = NULL,		\
}

static struct mt6315_misc mt6315_misc[] = {
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
	MT6315_DECL_CHIP(SPMI_MASTER_0, MT6315_SLAVE_ID_6),
	MT6315_DECL_CHIP(SPMI_MASTER_0, MT6315_SLAVE_ID_7),
	MT6315_DECL_CHIP(SPMI_MASTER_0, MT6315_SLAVE_ID_3),
#elif defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853)
	MT6315_DECL_CHIP(SPMI_MASTER_0, MT6315_SLAVE_ID_3),
#elif defined(CONFIG_MACH_MT6877)
	MT6315_DECL_CHIP(SPMI_MASTER_0, MT6315_SLAVE_ID_6),
	MT6315_DECL_CHIP(SPMI_MASTER_0, MT6315_SLAVE_ID_3),
#endif
};

static struct mt6315_misc *mt6315_find_chip_sid(u32 sid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt6315_misc); i++) {
		if (mt6315_misc[i].slave_addr == sid)
			return &mt6315_misc[i];
	}
	return NULL;
}

static unsigned int g_vmodem_vosel;
static unsigned int g_vnr_vosel;
static unsigned int g_vsram_md_vosel;

static void mt6315_S3_default_vosel(void)
{
	struct mt6315_misc *mt6315;
	struct regmap *regmap;

	mt6315 = mt6315_find_chip_sid(MT6315_SLAVE_ID_3);
	if (!mt6315) {
		pr_info("%s MT6315S3 not ready.\n", __func__);
		return;
	}

	regmap = mt6315->regmap;
	if (!regmap) {
		pr_info("%s null regmap.\n", __func__);
		return;
	}

	if (g_vmodem_vosel != 0) {
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
		regmap_write(regmap, MT6315_PMIC_RG_BUCK_VBUCK1_VOSEL_ADDR,
			     g_vmodem_vosel);
		regmap_write(regmap, MT6315_PMIC_RG_BUCK_VBUCK3_VOSEL_ADDR,
			     g_vnr_vosel);
		regmap_write(regmap, MT6315_PMIC_RG_BUCK_VBUCK4_VOSEL_ADDR,
			     g_vsram_md_vosel);
#elif defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853) \
|| defined(CONFIG_MACH_MT6877)
		regmap_write(regmap, MT6315_PMIC_RG_BUCK_VBUCK4_VOSEL_ADDR,
			     g_vsram_md_vosel);
		regmap_write(regmap, MT6315_PMIC_RG_BUCK_VBUCK1_VOSEL_ADDR,
			     g_vmodem_vosel);
#endif
		pr_info("[%s] set vmodem=0x%x, vnr=0x%x, vsram_md=0x%x\n"
			, __func__, g_vmodem_vosel, g_vnr_vosel,
			g_vsram_md_vosel);
	} else {
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
		regmap_read(regmap, MT6315_PMIC_DA_VBUCK1_VOSEL_ADDR,
			    &g_vmodem_vosel);
		regmap_read(regmap, MT6315_PMIC_DA_VBUCK3_VOSEL_ADDR,
			    &g_vnr_vosel);
		regmap_read(regmap, MT6315_PMIC_DA_VBUCK4_VOSEL_ADDR,
			    &g_vsram_md_vosel);
#elif defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853) \
|| defined(CONFIG_MACH_MT6877)
		regmap_read(regmap, MT6315_PMIC_DA_VBUCK1_VOSEL_ADDR,
			    &g_vmodem_vosel);
		g_vnr_vosel = g_vmodem_vosel;
		regmap_read(regmap, MT6315_PMIC_DA_VBUCK4_VOSEL_ADDR,
			    &g_vsram_md_vosel);
#endif
		pr_info("[%s] record vmodem=0x%x, vnr=0x%x, vsram_md=0x%x\n"
			, __func__, g_vmodem_vosel, g_vnr_vosel,
			g_vsram_md_vosel);
	}
}

void mt6315_vmd1_pmic_setting_on(void)
{
	/* Reset VMODEM/VNR/VSRAM_MD default voltage */
	mt6315_S3_default_vosel();
}

static int is_mt6315_S3_exist(void)
{
	int ret = 0;
	struct regulator *reg;

	reg = regulator_get_optional(NULL, "3_vbuck1");
	if (IS_ERR(reg))
		return 0;
	if (regulator_is_enabled(reg))
		ret = 1;
	regulator_put(reg);

	return ret;
}

#if !(defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853))
static int is_mt6315_S6_exist(void)
{
	int ret = 0;
	struct regulator *reg;

	reg = regulator_get_optional(NULL, "6_vbuck1");
	if (IS_ERR(reg))
		return 0;
	if (regulator_is_enabled(reg))
		ret = 1;
	regulator_put(reg);

	return ret;
}
#endif

#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
static int is_mt6315_S7_exist(void)
{
	int ret = 0;
	struct regulator *reg;

	reg = regulator_get_optional(NULL, "7_vbuck1");
	if (IS_ERR(reg))
		return 0;
	if (regulator_is_enabled(reg))
		ret = 1;
	regulator_put(reg);

	return ret;
}
#endif

int is_mt6315_exist(void)
{
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
	pr_info("%s S3:%d S6:%d S7:%d\n", __func__, is_mt6315_S3_exist()
	       , is_mt6315_S6_exist(), is_mt6315_S7_exist());
	if (is_mt6315_S3_exist() && is_mt6315_S6_exist() &&
	    is_mt6315_S7_exist())
		return 1;
#elif defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853)
	pr_info("%s S3:%d\n", __func__, is_mt6315_S3_exist());
	return is_mt6315_S3_exist();
#elif defined(CONFIG_MACH_MT6877)
	pr_info("%s S3:%d S6:%d\n", __func__, is_mt6315_S3_exist()
	       , is_mt6315_S6_exist());
	if (is_mt6315_S3_exist() && is_mt6315_S6_exist())
		return 1;
#endif
	return 0;
}

#if LP_INIT_SETTING_VERIFIED
static void mt6315_vbuck1_lp_setting(struct regmap *regmap,
		enum MT6315_BUCK_EN_USER user, unsigned char mode,
		unsigned char en, unsigned char cfg)
{
	if (user == MT6315_SRCLKEN0) {
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_MODE_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_MODE_SHIFT,
			mode << MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_MODE_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_EN_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_EN_SHIFT,
			en << MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_EN_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_CFG_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_CFG_SHIFT,
			cfg << MT6315_PMIC_RG_BUCK_VBUCK1_HW0_OP_CFG_SHIFT);
	} else {
		pr_info("%s non support user control(%d).\n", __func__, user);
	}
}

static void mt6315_vbuck2_lp_setting(struct regmap *regmap,
		enum MT6315_BUCK_EN_USER user, unsigned char mode,
		unsigned char en, unsigned char cfg)
{
	if (user == MT6315_SRCLKEN0) {
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_MODE_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_MODE_SHIFT,
			mode << MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_MODE_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_EN_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_EN_SHIFT,
			en << MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_EN_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_CFG_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_CFG_SHIFT,
			cfg << MT6315_PMIC_RG_BUCK_VBUCK2_HW0_OP_CFG_SHIFT);
	} else {
		pr_info("%s non support user control(%d).\n", __func__, user);
	}
}

static void mt6315_vbuck3_lp_setting(struct regmap *regmap,
		enum MT6315_BUCK_EN_USER user, unsigned char mode,
		unsigned char en, unsigned char cfg)
{
	if (user == MT6315_SRCLKEN0) {
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_MODE_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_MODE_SHIFT,
			mode << MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_MODE_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_EN_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_EN_SHIFT,
			en << MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_EN_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_CFG_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_CFG_SHIFT,
			cfg << MT6315_PMIC_RG_BUCK_VBUCK3_HW0_OP_CFG_SHIFT);
	} else {
		pr_info("%s non support user control(%d).\n", __func__, user);
	}
}

static void mt6315_vbuck4_lp_setting(struct regmap *regmap,
		enum MT6315_BUCK_EN_USER user, unsigned char mode,
		unsigned char en, unsigned char cfg)
{
	if (user == MT6315_SRCLKEN0) {
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_MODE_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_MODE_SHIFT,
			mode << MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_MODE_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_EN_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_EN_SHIFT,
			en << MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_EN_SHIFT);
		regmap_update_bits(regmap,
			MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_CFG_ADDR,
			0x1 << MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_CFG_SHIFT,
			cfg << MT6315_PMIC_RG_BUCK_VBUCK4_HW0_OP_CFG_SHIFT);
	} else {
		pr_info("%s non support user control(%d).\n", __func__, user);
	}
}


static void mt6315_lp_set(unsigned char slave_id, unsigned char buck_id,
		   enum MT6315_BUCK_EN_USER user, unsigned char op_mode,
		   unsigned char op_en, unsigned char op_cfg)
{
	struct mt6315_misc *mt6315;
	struct regmap *regmap;

	mt6315 = mt6315_find_chip_sid(slave_id);
	if (!mt6315) {
		pr_info("%s MT6315S%d not ready\n", __func__, slave_id);
		return;
	}

	regmap = mt6315->regmap;
	if (!regmap) {
		pr_info("%s null regmap.\n", __func__);
		return;
	}

	if (buck_id == 1)
		mt6315_vbuck1_lp_setting(regmap, user, op_mode, op_en, op_cfg);
	else if (buck_id == 2)
		mt6315_vbuck2_lp_setting(regmap, user, op_mode, op_en, op_cfg);
	else if (buck_id == 3)
		mt6315_vbuck3_lp_setting(regmap, user, op_mode, op_en, op_cfg);
	else if (buck_id == 4)
		mt6315_vbuck4_lp_setting(regmap, user, op_mode, op_en, op_cfg);
	else
		pr_info("%s invalid buck_id=%d.\n", __func__, buck_id);
}

/* enable VDIG18 SRCLKEN low power mode */
static void mt6315_vdig18_hw_op_set(unsigned char slave_id, unsigned char en)
{
	struct mt6315_misc *mt6315;
	struct regmap *regmap;

	mt6315 = mt6315_find_chip_sid(slave_id);
	if (!mt6315) {
		pr_info("%s MT6315S%d not ready\n", __func__, slave_id);
		return;
	}

	regmap = mt6315->regmap;
	if (!regmap) {
		pr_info("%s null regmap.\n", __func__);
		return;
	}

	regmap_write(regmap, MT6315_PMIC_DIG_WPK_KEY_H_ADDR, 0x63);
	regmap_write(regmap, MT6315_PMIC_DIG_WPK_KEY_ADDR, 0x15);
	regmap_update_bits(regmap,
		   MT6315_PMIC_RG_LDO_VDIG18_HW_OP_EN_ADDR,
		   0x1 << MT6315_PMIC_RG_LDO_VDIG18_HW_OP_EN_SHIFT,
		   en << MT6315_PMIC_RG_LDO_VDIG18_HW_OP_EN_SHIFT);
	regmap_write(regmap, MT6315_PMIC_DIG_WPK_KEY_ADDR, 0x0);
	regmap_write(regmap, MT6315_PMIC_DIG_WPK_KEY_H_ADDR, 0x0);
}
#endif /* End of LP_INIT_SETTING_VERIFIED */

static void mt6315_S3_lp_initial_setting(void)
{
#if LP_INIT_SETTING_VERIFIED
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_3, 1);
	/* vmodem/vnr/vsram_md */
	mt6315_lp_set(MT6315_SLAVE_ID_3, 1, MT6315_SRCLKEN0, 1, 1, HW_LP);
	mt6315_lp_set(MT6315_SLAVE_ID_3, 3, MT6315_SRCLKEN0, 1, 1, HW_LP);
	mt6315_lp_set(MT6315_SLAVE_ID_3, 4, MT6315_SRCLKEN0, 1, 1, HW_LP);
#elif defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6853)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_3, 1);
	/* vmodem/vpu/vsram_md */
	mt6315_lp_set(MT6315_SLAVE_ID_3, 1, MT6315_SRCLKEN0, 1, 1, HW_LP);
	mt6315_lp_set(MT6315_SLAVE_ID_3, 3, MT6315_SRCLKEN0, 1, 1, HW_LP);
	mt6315_lp_set(MT6315_SLAVE_ID_3, 4, MT6315_SRCLKEN0, 1, 1, HW_LP);
#elif defined(CONFIG_MACH_MT6877)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_3, 1);
	/* vmodem/vsram_md */
	mt6315_lp_set(MT6315_SLAVE_ID_3, 1, MT6315_SRCLKEN0, 1, 1, HW_LP);
	mt6315_lp_set(MT6315_SLAVE_ID_3, 4, MT6315_SRCLKEN0, 1, 1, HW_LP);

#endif
#endif
}

static void mt6315_S6_lp_initial_setting(void)
{
#if LP_INIT_SETTING_VERIFIED
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873) \
|| defined(CONFIG_MACH_MT6893)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_6, 1);
#elif defined(CONFIG_MACH_MT6877)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_6, 1);
	mt6315_lp_set(MT6315_SLAVE_ID_6, 4, MT6315_SRCLKEN0, 1, 1, HW_OFF);
#endif
#endif
}

static void mt6315_S7_lp_initial_setting(void)
{
#if LP_INIT_SETTING_VERIFIED
#if defined(CONFIG_MACH_MT6893)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_7, 1);
	/* vsram_core */
	mt6315_lp_set(MT6315_SLAVE_ID_7, 4, MT6315_SRCLKEN0, 1, 1, HW_LP);
#elif defined(CONFIG_MACH_MT6885)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_7, 1);
	/* vsram_core */
	mt6315_lp_set(MT6315_SLAVE_ID_7, 3, MT6315_SRCLKEN0, 1, 1, HW_LP);
#elif defined(CONFIG_MACH_MT6873)
	mt6315_vdig18_hw_op_set(MT6315_SLAVE_ID_7, 1);
#endif
#endif
}

static void mt6315_misc_initial_setting(u32 sid)
{
	switch (sid) {
	case MT6315_SLAVE_ID_3:
		mt6315_S3_lp_initial_setting();
		mt6315_S3_default_vosel();
		break;

	case MT6315_SLAVE_ID_6:
		mt6315_S6_lp_initial_setting();
		break;

	case MT6315_SLAVE_ID_7:
		mt6315_S7_lp_initial_setting();
		break;

	default:
		pr_info("unsupported chip sid: %d\n", sid);
		return;
	}
}

void mt6315_misc_init(u32 sid, struct regmap *regmap)
{
	struct mt6315_misc *mt6315;

	mt6315 = mt6315_find_chip_sid(sid);
	if (mt6315)
		mt6315->regmap = regmap;

	mt6315_misc_initial_setting(sid);

	pr_info("%s sid=%d done\n", __func__, sid);
}
