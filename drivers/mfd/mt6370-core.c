// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include <linux/mfd/mt6370/mt6370.h>
#include <linux/mfd/mt6370/mt6370-private.h>

/* reg 0 -> 0 ~ 7 */
#define MT6370_CHG_TREG_EVT		(4)
#define MT6370_CHG_AICR_EVT		(5)
#define MT6370_CHG_MIVR_EVT		(6)
#define MT6370_PWR_RDY_EVT		(7)
/* REG 1 -> 8 ~ 15 */
#define MT6370_CHG_VINOVP_EVT		(11)
#define MT6370_CHG_VSYSUV_EVT		(12)
#define MT6370_CHG_VSYSOV_EVT		(13)
#define MT6370_CHG_VBATOV_EVT		(14)
#define MT6370_CHG_VBUSOV_EVT		(15)
/* REG 2 -> 16 ~ 23 */
#define MT6370_TS_BAT_COLD_EVT		(20)
#define MT6370_TS_BAT_COOL_EVT		(21)
#define MT6370_TS_BAT_WARM_EVT		(22)
#define MT6370_TS_BAT_HOT_EVT		(23)
/* REG 3 -> 24 ~ 31 */
#define MT6370_CHG_TMRI			(27)
#define MT6370_BATABSI			(28)
#define MT6370_ADPBADI			(29)
#define MT6370_CHG_RVPI			(30)
#define MT6370_OTPI				(31)
/* REG 4 -> 32 ~ 39 */
#define MT6370_CHG_AICLMEASL	(32)
#define MT6370_ICHGMEASI		(33)
#define MT6370_CHGDET_DONEI		(34)
#define MT6370_WDTMRI			(35)
#define MT6370_SSFINISHI		(36)
#define MT6370_CHG_RECHGI		(37)
#define MT6370_CHG_TERMI		(38)
#define MT6370_CHG_IEOCI		(39)
/* REG 5 -> 40 ~ 47 */
/* REG 6 -> 48 ~ 55 */
#define MT6370_ATTACH_I			(48)
#define MT6370_DETACH_I			(49)
#define MT6370_QC30_STPDONE		(51)
#define MT6370_QC_VBUSDET_DONE		(52)
#define MT6370_HVDCP_DET		(53)
#define MT6370_CHGDETI			(54)
#define MT6370_DCDTI			(55)
/* REG 7 -> 56 ~ 63 */
#define MT6370_DIRCHG_VGOKI		(59)
#define MT6370_DIRCHG_WDTMRI		(60)
#define MT6370_DIRCHG_UCI		(61)
#define MT6370_DIRCHG_OCI		(62)
#define MT6370_DIRCHG_OVI		(63)
/* REG 8 -> 64 ~ 71 */
#define MT6370_SWON_EVT		(67)
#define MT6370_UVP_D_EVT		(68)
#define MT6370_UVP_EVT			(69)
#define MT6370_OVP_D_EVT		(70)
#define MT6370_OVP_EVT		(71)
/* REG 9 -> 72 ~ 79 */
#define MT6370_FLED_STRBPIN_EVT		(72)
#define MT6370_FLED_TORPIN_EVT		(73)
#define MT6370_FLED_TX_EVT		(74)
#define MT6370_FLED_LVF_EVT		(75)
#define MT6370_FLED2_SHORT_EVT		(78)
#define MT6370_FLED1_SHORT_EVT		(79)
/* REG 10 -> 80 ~ 87 */
#define MT6370_FLED2_STRB_EVT		(80)
#define MT6370_FLED1_STRB_EVT		(81)
#define MT6370_FLED2_STRB_TO_EVT	(82)
#define MT6370_FLED1_STRB_TO_EVT	(83)
#define MT6370_FLED2_TOR_EVT		(84)
#define MT6370_FLED1_TOR_EVT		(85)
/* REG 11 -> 88 ~ 95 */
#define MT6370_OTP_EVT		(93)
#define MT6370_VDDA_OVP_EVT		(94)
#define MT6370_VDDA_UV_EVT		(95)
/* REG 12 -> 96 ~ 103 */
#define MT6370_LDO_OC_EVT		(103)
/* REG 13 -> 104 ~ 111 */
#define MT6370_ISINK4_SHORT_EVT		(104)
#define MT6370_ISINK3_SHORT_EVT		(105)
#define MT6370_ISINK2_SHORT_EVT		(106)
#define MT6370_ISINK1_SHORT_EVT		(107)
#define MT6370_ISINK4_OPEN_EVT		(108)
#define MT6370_ISINK3_OPEN_EVT		(109)
#define MT6370_ISINK2_OPEN_EVT		(110)
#define MT6370_ISINK1_OPEN_EVT		(111)
/* REG 14 -> 112 ~ 119 */
#define MT6370_BLED_OCP_EVT		(118)
#define MT6370_BLED_OVP_EVT		(119)
/* REG 15 -> 120 ~ 127 */
#define MT6370_DSV_VNEG_OCP_EVT		(123)
#define MT6370_DSV_VPOS_OCP_EVT		(124)
#define MT6370_DSV_BST_OCP_EVT		(125)
#define MT6370_DSV_VNEG_SCP_EVT		(126)
#define MT6370_DSV_VPOS_SCP_EVT		(127)


static const struct regmap_irq mt6370_pmu_irqs[] =  {
	REGMAP_IRQ_REG_LINE(MT6370_CHG_TREG_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_AICR_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_MIVR_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_PWR_RDY_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_VINOVP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_VSYSUV_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_VSYSOV_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_VBATOV_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_VBUSOV_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_TS_BAT_COLD_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_TS_BAT_COOL_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_TS_BAT_WARM_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_TS_BAT_HOT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_TMRI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_BATABSI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ADPBADI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_RVPI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_OTPI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_AICLMEASL, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ICHGMEASI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHGDET_DONEI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_WDTMRI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_SSFINISHI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_RECHGI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_TERMI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHG_IEOCI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ATTACH_I, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DETACH_I, 8),
	REGMAP_IRQ_REG_LINE(MT6370_QC30_STPDONE, 8),
	REGMAP_IRQ_REG_LINE(MT6370_QC_VBUSDET_DONE, 8),
	REGMAP_IRQ_REG_LINE(MT6370_HVDCP_DET, 8),
	REGMAP_IRQ_REG_LINE(MT6370_CHGDETI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DCDTI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DIRCHG_VGOKI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DIRCHG_WDTMRI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DIRCHG_UCI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DIRCHG_OCI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DIRCHG_OVI, 8),
	REGMAP_IRQ_REG_LINE(MT6370_SWON_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_UVP_D_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_UVP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_OVP_D_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_OVP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED_STRBPIN_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED_TORPIN_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED_TX_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED_LVF_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED2_SHORT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED1_SHORT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED2_STRB_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED1_STRB_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED2_STRB_TO_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED1_STRB_TO_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED2_TOR_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_FLED1_TOR_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_OTP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_VDDA_OVP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_VDDA_UV_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_LDO_OC_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK4_SHORT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK3_SHORT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK2_SHORT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK1_SHORT_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK4_OPEN_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK3_OPEN_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK2_OPEN_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_ISINK1_OPEN_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_BLED_OCP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_BLED_OVP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DSV_VNEG_OCP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DSV_VPOS_OCP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DSV_BST_OCP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DSV_VNEG_SCP_EVT, 8),
	REGMAP_IRQ_REG_LINE(MT6370_DSV_VPOS_SCP_EVT, 8),
};

static int mt6370_pmu_handle_post_irq(void *irq_drv_data)
{
	struct mt6370_pmu_data *mpd = irq_drv_data;

	return regmap_update_bits(mpd->regmap,
		MT6370_PMU_REG_IRQSET, MT6370_IRQ_RETRIG, MT6370_IRQ_RETRIG);
}

static struct regmap_irq_chip mt6370_pmu_irq_chip = {
	.irqs = mt6370_pmu_irqs,
	.num_irqs = ARRAY_SIZE(mt6370_pmu_irqs),
	.num_regs = MT6370_PMU_IRQ_REGNUM,
	.mask_base = MT6370_PMU_CHGMASK1,
	.status_base = MT6370_PMU_REG_CHGIRQ1,
	.ack_base = MT6370_PMU_REG_CHGIRQ1,
	.init_ack_masked = true,
	.use_ack = true,
	.handle_post_irq = mt6370_pmu_handle_post_irq,
};

static bool mt6370_is_volatile_reg(struct device *dev, unsigned int reg)
{
	if (reg >= MT6370_PMU_CHGMASK1 && reg <= MT6370_PMU_BLMASK) //not sure max or ldomask2
		return false;
	return true;
}

static struct regmap_config mt6370_pmu_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MT6370_PMU_MAXREG,

	.cache_type = REGCACHE_FLAT,
	.volatile_reg = mt6370_is_volatile_reg,
};

static const struct resource mt6370_pmu_core_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_OTP_EVT, "otp"),
	DEFINE_RES_IRQ_NAMED(MT6370_VDDA_OVP_EVT, "vdda_ovp"),
	DEFINE_RES_IRQ_NAMED(MT6370_VDDA_UV_EVT, "vdda_uv"),
};

static const struct resource mt6370_chager_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_CHG_TREG_EVT, "chg_treg"),
	DEFINE_RES_IRQ_NAMED(MT6370_CHG_MIVR_EVT, "chg_mivr"),
	DEFINE_RES_IRQ_NAMED(MT6370_PWR_RDY_EVT, "pwr_rdy"),
	DEFINE_RES_IRQ_NAMED(MT6370_CHG_AICLMEASL, "chg_aiclmeasl"),
	DEFINE_RES_IRQ_NAMED(MT6370_ATTACH_I, "attachi"),
	DEFINE_RES_IRQ_NAMED(MT6370_UVP_D_EVT, "ovpctrl_uvp_d_evt"),
	DEFINE_RES_IRQ_NAMED(MT6370_DIRCHG_WDTMRI, "chg_wdtmri"),
	DEFINE_RES_IRQ_NAMED(MT6370_CHG_TMRI, "chg_tmri"),
	DEFINE_RES_IRQ_NAMED(MT6370_CHG_VBUSOV_EVT, "chg_vbusov"),
	DEFINE_RES_IRQ_NAMED(MT6370_DCDTI, "dcdti"),
};

static const struct resource mt6370_pmu_fled1_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_FLED_LVF_EVT, "fled_lvf"),
	DEFINE_RES_IRQ_NAMED(MT6370_FLED2_SHORT_EVT, "fled2_short"),
	DEFINE_RES_IRQ_NAMED(MT6370_FLED1_SHORT_EVT, "fled1_short"),
};

static const struct resource mt6370_pmu_ldo_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_LDO_OC_EVT, "ldo_oc"),
};

static const struct resource mt6370_pmu_rgbled_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK4_SHORT_EVT, "isink4_short"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK3_SHORT_EVT, "isink3_short"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK2_SHORT_EVT, "isink2_short"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK1_SHORT_EVT, "isink1_short"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK4_OPEN_EVT, "isink4_open"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK3_OPEN_EVT, "isink3_open"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK2_OPEN_EVT, "isink2_open"),
	DEFINE_RES_IRQ_NAMED(MT6370_ISINK1_OPEN_EVT, "isink1_open"),
};

static const struct resource mt6370_pmu_bled_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_BLED_OCP_EVT, "bled_ocp"),
};

static const struct resource mt6370_pmu_dsv_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6370_DSV_VNEG_OCP_EVT, "dsv_vneg_ocp"),
	DEFINE_RES_IRQ_NAMED(MT6370_DSV_VPOS_OCP_EVT, "dsv_vpos_ocp"),
	DEFINE_RES_IRQ_NAMED(MT6370_DSV_BST_OCP_EVT, "dsv_bst_ocp"),
	DEFINE_RES_IRQ_NAMED(MT6370_DSV_VNEG_SCP_EVT, "dsv_vneg_scp"),
	DEFINE_RES_IRQ_NAMED(MT6370_DSV_VPOS_SCP_EVT, "dsv_vpos_scp"),
};

static const struct mfd_cell mt6370_devs[] = {
	OF_MFD_CELL("mt6370_pmu_core", mt6370_pmu_core_resources,
		NULL, 0, 0, "mediatek,mt6370_pmu_core"),
	OF_MFD_CELL("mt6370_chg", mt6370_chager_resources,
		    NULL, 0, 0, "mediatek,mt6370_pmu_charger"),
	OF_MFD_CELL("mt6370_pmu_fled1", mt6370_pmu_fled1_resources,
		    NULL, 0, 0, "mediatek,mt6370_pmu_fled1"),
	OF_MFD_CELL("mt6370_pmu_fled2", NULL,
		    NULL, 0, 0, "mediatek,mt6370_pmu_fled2"),
	OF_MFD_CELL("mt6370_pmu_ldo", mt6370_pmu_ldo_resources,
		    NULL, 0, 0, "mediatek,mt6370_pmu_ldo"),
	OF_MFD_CELL("mt6370_pmu_rgbled", mt6370_pmu_rgbled_resources,
		    NULL, 0, 0, "mediatek,mt6370_pmu_rgbled"),
	OF_MFD_CELL("mt6370_pmu_bled", mt6370_pmu_bled_resources,
		    NULL, 0, 0, "mediatek,mt6370_pmu_bled"),
	OF_MFD_CELL("mt6370_pmu_dsv", mt6370_pmu_dsv_resources,
		    NULL, 0, 0, "mediatek,mt6370_pmu_dsv"),
	{ .name = "mt6370_dbg", },
};

static const unsigned short mt6370_slave_addr[MT6370_SLAVE_MAX] = {
	MT6370_PMU_SLAVEID,
};

static int mt6370_pmu_probe(struct i2c_client *client)
{
	struct mt6370_pmu_data *mpd;
	struct regmap_config *regmap_config = &mt6370_pmu_regmap_config;
	unsigned int reg_data;
	int i, ret;

	mpd = devm_kzalloc(&client->dev, sizeof(*mpd), GFP_KERNEL);
	if (!mpd)
		return -ENOMEM;
	mpd->dev = &client->dev;
	mutex_init(&mpd->io_lock);
	i2c_set_clientdata(client, mpd);

	regmap_config->lock_arg = &mpd->io_lock;
	mpd->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(mpd->regmap)) {
		dev_err(&client->dev, "Failed to register regmap\n");
		return PTR_ERR(mpd->regmap);
	}

	ret = regmap_read(mpd->regmap, MT6370_PMU_REG_DEVINFO, &reg_data);
	if (ret) {
		dev_err(&client->dev, "Device not found\n");
		return ret;
	}

	mpd->chip_rev = reg_data & CHIP_VEN_MASK;
	if ((mpd->chip_rev != MT6370_VENDOR_ID) && (mpd->chip_rev != MT6371_VENDOR_ID) &&
		(mpd->chip_rev != MT6372C_VENDOR_ID))  {
		dev_err(&client->dev, "Device not supported\n");
		return -ENODEV;
	}
	mt6370_pmu_irq_chip.irq_drv_data = mpd;
	ret = devm_regmap_add_irq_chip(&client->dev, mpd->regmap, client->irq,
				       IRQF_TRIGGER_FALLING, 0,
				       &mt6370_pmu_irq_chip, &mpd->irq_data);
	if (ret) {
		dev_err(&client->dev, "Failed to add Regmap IRQ Chip\n");
		return ret;
	}

	mpd->i2c[0] = client;
	for (i = 1; i < MT6370_SLAVE_MAX; i++) {
		mpd->i2c[i] = devm_i2c_new_dummy_device(&client->dev,
							client->adapter,
							mt6370_slave_addr[i]);
		if (IS_ERR(mpd->i2c[i])) {
			dev_err(&client->dev,
				"Failed to get new dummy I2C device for address 0x%x",
				mt6370_slave_addr[i]);
			return PTR_ERR(mpd->i2c[i]);
		}
		i2c_set_clientdata(mpd->i2c[i], mpd);
	}
	ret = devm_mfd_add_devices(&client->dev, PLATFORM_DEVID_AUTO,
				   mt6370_devs, ARRAY_SIZE(mt6370_devs), NULL,
				   0, regmap_irq_get_domain(mpd->irq_data));
	if (ret) {
		dev_err(&client->dev,
			"Failed to register subordinate devices\n");
		return ret;
	}

	return 0;
}

static int __maybe_unused mt6370_pmu_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(i2c->irq);
	return 0;
}

static int __maybe_unused mt6370_pmu_resume(struct device *dev)
{

	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(i2c->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6370_pmu_pm_ops,
			 mt6370_pmu_suspend, mt6370_pmu_resume);

static const struct of_device_id __maybe_unused mt6370_pmu_of_id[] = {
	{ .compatible = "mediatek,mt6370_pmu", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6370_pmu_of_id);

static struct i2c_driver mt6370_pmu_driver = {
	.driver = {
		.name = "mt6370_pmu",
		.pm = &mt6370_pmu_pm_ops,
		.of_match_table = of_match_ptr(mt6370_pmu_of_id),
	},
	.probe_new = mt6370_pmu_probe,
};
module_i2c_driver(mt6370_pmu_driver);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("MT6370 PMU I2C Driver");
MODULE_LICENSE("GPL v2");
