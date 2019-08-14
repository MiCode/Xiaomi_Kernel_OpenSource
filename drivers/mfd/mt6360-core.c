// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/version.h>

#include <linux/mfd/mt6360.h>
#include <linux/mfd/mt6360-private.h>

/* reg 0 -> 0 ~ 7 */
#define MT6360_CHG_TREG_EVT		(4)
#define MT6360_CHG_AICR_EVT		(5)
#define MT6360_CHG_MIVR_EVT		(6)
#define MT6360_PWR_RDY_EVT		(7)
/* REG 1 -> 8 ~ 15 */
#define MT6360_CHG_BATSYSUV_EVT		(9)
#define MT6360_FLED_CHG_VINOVP_EVT	(11)
#define MT6360_CHG_VSYSUV_EVT		(12)
#define MT6360_CHG_VSYSOV_EVT		(13)
#define MT6360_CHG_VBATOV_EVT		(14)
#define MT6360_CHG_VBUSOV_EVT		(15)
/* REG 2 -> 16 ~ 23 */
/* REG 3 -> 24 ~ 31 */
#define MT6360_WD_PMU_DET		(25)
#define MT6360_WD_PMU_DONE		(26)
#define MT6360_CHG_TMRI			(27)
#define MT6360_CHG_ADPBADI		(29)
#define MT6360_CHG_RVPI			(30)
#define MT6360_OTPI			(31)
/* REG 4 -> 32 ~ 39 */
#define MT6360_CHG_AICCMEASL		(32)
#define MT6360_CHGDET_DONEI		(34)
#define MT6360_WDTMRI			(35)
#define MT6360_SSFINISHI		(36)
#define MT6360_CHG_RECHGI		(37)
#define MT6360_CHG_TERMI		(38)
#define MT6360_CHG_IEOCI		(39)
/* REG 5 -> 40 ~ 47 */
#define MT6360_PUMPX_DONEI		(40)
#define MT6360_BAT_OVP_ADC_EVT		(41)
#define MT6360_TYPEC_OTP_EVT		(42)
#define MT6360_ADC_WAKEUP_EVT		(43)
#define MT6360_ADC_DONEI		(44)
#define MT6360_BST_BATUVI		(45)
#define MT6360_BST_VBUSOVI		(46)
#define MT6360_BST_OLPI			(47)
/* REG 6 -> 48 ~ 55 */
#define MT6360_ATTACH_I			(48)
#define MT6360_DETACH_I			(49)
#define MT6360_QC30_STPDONE		(51)
#define MT6360_QC_VBUSDET_DONE		(52)
#define MT6360_HVDCP_DET		(53)
#define MT6360_CHGDETI			(54)
#define MT6360_DCDTI			(55)
/* REG 7 -> 56 ~ 63 */
#define MT6360_FOD_DONE_EVT		(56)
#define MT6360_FOD_OV_EVT		(57)
#define MT6360_CHRDET_UVP_EVT		(58)
#define MT6360_CHRDET_OVP_EVT		(59)
#define MT6360_CHRDET_EXT_EVT		(60)
#define MT6360_FOD_LR_EVT		(61)
#define MT6360_FOD_HR_EVT		(62)
#define MT6360_FOD_DISCHG_FAIL_EVT	(63)
/* REG 8 -> 64 ~ 71 */
#define MT6360_USBID_EVT		(64)
#define MT6360_APWDTRST_EVT		(65)
#define MT6360_EN_EVT			(66)
#define MT6360_QONB_RST_EVT		(67)
#define MT6360_MRSTB_EVT		(68)
#define MT6360_OTP_EVT			(69)
#define MT6360_VDDAOV_EVT		(70)
#define MT6360_SYSUV_EVT		(71)
/* REG 9 -> 72 ~ 79 */
#define MT6360_FLED_STRBPIN_EVT		(72)
#define MT6360_FLED_TORPIN_EVT		(73)
#define MT6360_FLED_TX_EVT		(74)
#define MT6360_FLED_LVF_EVT		(75)
#define MT6360_FLED2_SHORT_EVT		(78)
#define MT6360_FLED1_SHORT_EVT		(79)
/* REG 10 -> 80 ~ 87 */
#define MT6360_FLED2_STRB_EVT		(80)
#define MT6360_FLED1_STRB_EVT		(81)
#define MT6360_FLED2_STRB_TO_EVT	(82)
#define MT6360_FLED1_STRB_TO_EVT	(83)
#define MT6360_FLED2_TOR_EVT		(84)
#define MT6360_FLED1_TOR_EVT		(85)
/* REG 11 -> 88 ~ 95 */
/* REG 12 -> 96 ~ 103 */
#define MT6360_BUCK1_PGB_EVT		(96)
#define MT6360_BUCK1_OC_EVT		(100)
#define MT6360_BUCK1_OV_EVT		(101)
#define MT6360_BUCK1_UV_EVT		(102)
/* REG 13 -> 104 ~ 111 */
#define MT6360_BUCK2_PGB_EVT		(104)
#define MT6360_BUCK2_OC_EVT		(108)
#define MT6360_BUCK2_OV_EVT		(109)
#define MT6360_BUCK2_UV_EVT		(110)
/* REG 14 -> 112 ~ 119 */
#define MT6360_LDO1_OC_EVT		(113)
#define MT6360_LDO2_OC_EVT		(114)
#define MT6360_LDO3_OC_EVT		(115)
#define MT6360_LDO5_OC_EVT		(117)
#define MT6360_LDO6_OC_EVT		(118)
#define MT6360_LDO7_OC_EVT		(119)
/* REG 15 -> 120 ~ 127 */
#define MT6360_LDO1_PGB_EVT		(121)
#define MT6360_LDO2_PGB_EVT		(122)
#define MT6360_LDO3_PGB_EVT		(123)
#define MT6360_LDO5_PGB_EVT		(125)
#define MT6360_LDO6_PGB_EVT		(126)
#define MT6360_LDO7_PGB_EVT		(127)

#define MT6360_REGMAP_IRQ_REG(_irq_evt)		\
	REGMAP_IRQ_REG(_irq_evt, (_irq_evt) / 8, BIT((_irq_evt) % 8))

#define MT6360_MFD_CELL(_name)					\
	{							\
		.name = #_name,					\
		.of_compatible = "mediatek," #_name,		\
		.num_resources = ARRAY_SIZE(_name##_resources),	\
		.resources = _name##_resources,			\
	}

static const struct regmap_irq mt6360_pmu_irqs[] =  {
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_TREG_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_AICR_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_MIVR_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_PWR_RDY_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_BATSYSUV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED_CHG_VINOVP_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_VSYSUV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_VSYSOV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_VBATOV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_VBUSOV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_WD_PMU_DET),
	MT6360_REGMAP_IRQ_REG(MT6360_WD_PMU_DONE),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_TMRI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_ADPBADI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_RVPI),
	MT6360_REGMAP_IRQ_REG(MT6360_OTPI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_AICCMEASL),
	MT6360_REGMAP_IRQ_REG(MT6360_CHGDET_DONEI),
	MT6360_REGMAP_IRQ_REG(MT6360_WDTMRI),
	MT6360_REGMAP_IRQ_REG(MT6360_SSFINISHI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_RECHGI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_TERMI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_IEOCI),
	MT6360_REGMAP_IRQ_REG(MT6360_PUMPX_DONEI),
	MT6360_REGMAP_IRQ_REG(MT6360_CHG_TREG_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BAT_OVP_ADC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_TYPEC_OTP_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_ADC_WAKEUP_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_ADC_DONEI),
	MT6360_REGMAP_IRQ_REG(MT6360_BST_BATUVI),
	MT6360_REGMAP_IRQ_REG(MT6360_BST_VBUSOVI),
	MT6360_REGMAP_IRQ_REG(MT6360_BST_OLPI),
	MT6360_REGMAP_IRQ_REG(MT6360_ATTACH_I),
	MT6360_REGMAP_IRQ_REG(MT6360_DETACH_I),
	MT6360_REGMAP_IRQ_REG(MT6360_QC30_STPDONE),
	MT6360_REGMAP_IRQ_REG(MT6360_QC_VBUSDET_DONE),
	MT6360_REGMAP_IRQ_REG(MT6360_HVDCP_DET),
	MT6360_REGMAP_IRQ_REG(MT6360_CHGDETI),
	MT6360_REGMAP_IRQ_REG(MT6360_DCDTI),
	MT6360_REGMAP_IRQ_REG(MT6360_FOD_DONE_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FOD_OV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHRDET_UVP_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHRDET_OVP_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_CHRDET_EXT_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FOD_LR_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FOD_HR_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FOD_DISCHG_FAIL_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_USBID_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_APWDTRST_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_EN_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_QONB_RST_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_MRSTB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_OTP_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_VDDAOV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_SYSUV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED_STRBPIN_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED_TORPIN_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED_TX_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED_LVF_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED2_SHORT_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED1_SHORT_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED2_STRB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED1_STRB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED2_STRB_TO_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED1_STRB_TO_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED2_TOR_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_FLED1_TOR_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK1_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK1_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK1_OV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK1_UV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK2_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK2_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK2_OV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_BUCK2_UV_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO1_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO2_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO3_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO5_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO6_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO7_OC_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO1_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO2_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO3_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO5_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO6_PGB_EVT),
	MT6360_REGMAP_IRQ_REG(MT6360_LDO7_PGB_EVT),
};

static int mt6360_pmu_handle_post_irq(void *irq_drv_data)
{
	struct mt6360_pmu_info *mpi = irq_drv_data;

	return regmap_update_bits(mpi->regmap,
		MT6360_PMU_IRQ_SET, MT6360_IRQ_RETRIG, MT6360_IRQ_RETRIG);
}

static const struct regmap_irq_chip mt6360_pmu_irq_chip = {
	.irqs = mt6360_pmu_irqs,
	.num_irqs = ARRAY_SIZE(mt6360_pmu_irqs),
	.num_regs = MT6360_PMU_IRQ_REGNUM,
	.mask_base = MT6360_PMU_CHG_MASK1,
	.status_base = MT6360_PMU_CHG_IRQ1,
	.ack_base = MT6360_PMU_CHG_IRQ1,
	.init_ack_masked = true,
	.use_ack = true,
	.handle_post_irq = mt6360_pmu_handle_post_irq,
};

static const struct regmap_config mt6360_pmu_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MT6360_PMU_MAXREG,
};

static const struct resource mt6360_adc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6360_ADC_DONEI, "adc_donei"),
};

static const struct resource mt6360_chg_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_TREG_EVT, "chg_treg_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_PWR_RDY_EVT, "pwr_rdy_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_BATSYSUV_EVT, "chg_batsysuv_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_VSYSUV_EVT, "chg_vsysuv_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_VSYSOV_EVT, "chg_vsysov_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_VBATOV_EVT, "chg_vbatov_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_VBUSOV_EVT, "chg_vbusov_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_AICCMEASL, "chg_aiccmeasl"),
	DEFINE_RES_IRQ_NAMED(MT6360_WDTMRI, "wdtmri"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_RECHGI, "chg_rechgi"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_TERMI, "chg_termi"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHG_IEOCI, "chg_ieoci"),
	DEFINE_RES_IRQ_NAMED(MT6360_PUMPX_DONEI, "pumpx_donei"),
	DEFINE_RES_IRQ_NAMED(MT6360_ATTACH_I, "attach_i"),
	DEFINE_RES_IRQ_NAMED(MT6360_CHRDET_EXT_EVT, "chrdet_ext_evt"),
};

static const struct resource mt6360_led_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6360_FLED_CHG_VINOVP_EVT, "fled_chg_vinovp_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_FLED_LVF_EVT, "fled_lvf_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_FLED2_SHORT_EVT, "fled2_short_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_FLED1_SHORT_EVT, "fled1_short_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_FLED2_STRB_TO_EVT, "fled2_strb_to_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_FLED1_STRB_TO_EVT, "fled1_strb_to_evt"),
};

static const struct resource mt6360_pmic_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK1_PGB_EVT, "buck1_pgb_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK1_OC_EVT, "buck1_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK1_OV_EVT, "buck1_ov_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK1_UV_EVT, "buck1_uv_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK2_PGB_EVT, "buck2_pgb_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK2_OC_EVT, "buck2_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK2_OV_EVT, "buck2_ov_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_BUCK2_UV_EVT, "buck2_uv_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO6_OC_EVT, "ldo6_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO7_OC_EVT, "ldo7_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO6_PGB_EVT, "ldo6_pgb_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO7_PGB_EVT, "ldo7_pgb_evt"),
};

static const struct resource mt6360_ldo_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6360_LDO1_OC_EVT, "ldo1_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO2_OC_EVT, "ldo2_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO3_OC_EVT, "ldo3_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO5_OC_EVT, "ldo5_oc_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO1_PGB_EVT, "ldo1_pgb_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO2_PGB_EVT, "ldo2_pgb_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO3_PGB_EVT, "ldo3_pgb_evt"),
	DEFINE_RES_IRQ_NAMED(MT6360_LDO5_PGB_EVT, "ldo5_pgb_evt"),
};

static const struct mfd_cell mt6360_devs[] = {
	MT6360_MFD_CELL(mt6360_adc),
	MT6360_MFD_CELL(mt6360_chg),
	MT6360_MFD_CELL(mt6360_led),
	MT6360_MFD_CELL(mt6360_pmic),
	MT6360_MFD_CELL(mt6360_ldo),
	/* tcpc dev */
	{
		.name = "mt6360_tcpc",
		.of_compatible = "mediatek,mt6360_tcpc",
	},
	/* debug dev */
	{ .name = "mt6360_dbg", },
};

static const unsigned short mt6360_slave_addr[MT6360_SLAVE_MAX] = {
	MT6360_PMU_SLAVEID,
	MT6360_PMIC_SLAVEID,
	MT6360_LDO_SLAVEID,
	MT6360_TCPC_SLAVEID,
};

static int mt6360_pmu_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct mt6360_pmu_info *mpi;
	unsigned int reg_data = 0;
	int i, ret;

	mpi = devm_kzalloc(&client->dev, sizeof(*mpi), GFP_KERNEL);
	if (!mpi)
		return -ENOMEM;
	mpi->dev = &client->dev;
	i2c_set_clientdata(client, mpi);

	/* regmap regiser */
	mpi->regmap = devm_regmap_init_i2c(client, &mt6360_pmu_regmap_config);
	if (IS_ERR(mpi->regmap)) {
		dev_err(&client->dev, "regmap register fail\n");
		return PTR_ERR(mpi->regmap);
	}
	/* chip id check */
	ret = regmap_read(mpi->regmap, MT6360_PMU_DEV_INFO, &reg_data);
	if (ret < 0) {
		dev_err(&client->dev, "device not found\n");
		return ret;
	}
	if ((reg_data & CHIP_VEN_MASK) != CHIP_VEN_MT6360) {
		dev_err(&client->dev, "not mt6360 chip\n");
		return -ENODEV;
	}
	mpi->chip_rev = reg_data & CHIP_REV_MASK;
	/* irq register */
	memcpy(&mpi->irq_chip, &mt6360_pmu_irq_chip, sizeof(mpi->irq_chip));
	mpi->irq_chip.name = dev_name(&client->dev);
	mpi->irq_chip.irq_drv_data = mpi;
	ret = devm_regmap_add_irq_chip(&client->dev, mpi->regmap, client->irq,
				       IRQF_TRIGGER_FALLING, 0, &mpi->irq_chip,
				       &mpi->irq_data);
	if (ret < 0) {
		dev_err(&client->dev, "regmap irq chip add fail\n");
		return ret;
	}
	/* new i2c slave device */
	for (i = 0; i < MT6360_SLAVE_MAX; i++) {
		if (mt6360_slave_addr[i] == client->addr) {
			mpi->i2c[i] = client;
			continue;
		}
		mpi->i2c[i] = i2c_new_dummy(client->adapter,
					    mt6360_slave_addr[i]);
		if (!mpi->i2c[i]) {
			dev_err(&client->dev, "new i2c dev [%d] fail\n", i);
			ret = -ENODEV;
			goto out;
		}
		i2c_set_clientdata(mpi->i2c[i], mpi);
	}
	/* mfd cell register */
	ret = devm_mfd_add_devices(&client->dev, PLATFORM_DEVID_AUTO,
				   mt6360_devs, ARRAY_SIZE(mt6360_devs), NULL,
				   0, regmap_irq_get_domain(mpi->irq_data));
	if (ret < 0) {
		dev_err(&client->dev, "mfd add cells fail\n");
		goto out;
	}
	dev_info(&client->dev, "Successfully probed\n");
	return 0;
out:
	while (--i >= 0) {
		if (mpi->i2c[i]->addr == client->addr)
			continue;
		i2c_unregister_device(mpi->i2c[i]);
	}
	return ret;
}

static int mt6360_pmu_remove(struct i2c_client *client)
{
	struct mt6360_pmu_info *mpi = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < MT6360_SLAVE_MAX; i++) {
		if (mpi->i2c[i]->addr == client->addr)
			continue;
		i2c_unregister_device(mpi->i2c[i]);
	}
	return 0;
}

static int __maybe_unused mt6360_pmu_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(i2c->irq);
	disable_irq(i2c->irq);
	return 0;
}

static int __maybe_unused mt6360_pmu_resume(struct device *dev)
{

	struct i2c_client *i2c = to_i2c_client(dev);

	enable_irq(i2c->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(i2c->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_pm_ops,
			 mt6360_pmu_suspend, mt6360_pmu_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_of_id);

static const struct i2c_device_id mt6360_pmu_id[] = {
	{ "mt6360_pmu", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6360_pmu_id);

static struct i2c_driver mt6360_pmu_driver = {
	.driver = {
		.name = "mt6360_pmu",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_of_id),
	},
	.probe = mt6360_pmu_probe,
	.remove = mt6360_pmu_remove,
	.id_table = mt6360_pmu_id,
};
module_i2c_driver(mt6360_pmu_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU I2C Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
