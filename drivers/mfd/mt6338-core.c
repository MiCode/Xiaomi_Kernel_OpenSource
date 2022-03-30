// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/version.h>

#include <linux/mfd/mt6338.h>
#include <linux/mfd/mt6338-private.h>

#define MT6338_MFD_CELL(_name)					\
	{							\
		.name = #_name,					\
		.of_compatible = "mediatek," #_name,		\
	}

static bool mt6338_is_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static struct regmap_config mt6338_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = MT6338_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
	.volatile_reg = mt6338_is_volatile_reg,
};

static const struct mfd_cell mt6338_devs[] = {
	MT6338_MFD_CELL(mt6338-accdet),
	MT6338_MFD_CELL(mt6338-auxadc),
	MT6338_MFD_CELL(mt6338-sound),
	MT6338_MFD_CELL(mt6338-efuse),
	/* debug dev */
	/* { .name = "mt6360_dbg", },*/
};

static int mt6338_check_id(struct mt6338_pmic_info *mpi)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(mpi->regmap, MT6338_SWCID_H, &data);
	if (ret < 0) {
		dev_info(mpi->dev, "device not found\n");
		return ret;
	}
	if (data != MT6338_SWCID_H_CODE) {
		dev_info(mpi->dev, "not mt6338 chip\n");
		return -ENODEV;
	}
	mpi->chip_rev = (data << 8);
	ret = regmap_read(mpi->regmap, MT6338_SWCID_L, &data);
	mpi->chip_rev |= data;

	return 0;
}

void mt6338_Keyunlock(struct mt6338_pmic_info *mpi)
{
	regmap_write(mpi->regmap, MT6338_TOP_DIG_WPK, 0x38);
	regmap_write(mpi->regmap, MT6338_TOP_DIG_WPK_H, 0x63);
	regmap_write(mpi->regmap, MT6338_TOP_TMA_KEY, 0xc7);
	regmap_write(mpi->regmap, MT6338_TOP_TMA_KEY_H, 0x9c);
	regmap_write(mpi->regmap, MT6338_TOP2_ELR2, 0x2a);
	regmap_write(mpi->regmap, MT6338_TOP2_ELR3, 0x2a);
}

void mt6338_Keylock(struct mt6338_pmic_info *mpi)
{
	regmap_write(mpi->regmap, MT6338_TOP_DIG_WPK, 0x0);
	regmap_write(mpi->regmap, MT6338_TOP_DIG_WPK_H, 0x0);
	regmap_write(mpi->regmap, MT6338_TOP_TMA_KEY, 0x0);
	regmap_write(mpi->regmap, MT6338_TOP_TMA_KEY_H, 0x0);
	regmap_write(mpi->regmap, MT6338_PSC_WPK_L, 0x0);
	regmap_write(mpi->regmap, MT6338_PSC_WPK_H, 0x0);
	regmap_write(mpi->regmap, MT6338_HK_TOP_WKEY_L, 0x0);
	regmap_write(mpi->regmap, MT6338_HK_TOP_WKEY_H, 0x0);
}
void mt6338_LP_Setting(struct mt6338_pmic_info *mpi)
{
#ifdef LP_SETTING
	/*---Turn ON hardware clock DCM mode to save more power---*/
	regmap_update_bits(mpi->regmap,
				MT6338_LDO_VAUD18_CON2,
				RG_LDO_VAUD18_CK_SW_MODE_MASK_SFT,
				0x0 << RG_LDO_VAUD18_CK_SW_MODE_MASK_SFT);

	/*---LP Voltage Set---*/
	/*---Using PMRC_EN[15:0] in DVT---*/
	/*---HW0 (SRCLKEN0), HW1 (SRCLKEN1), SCP_VAO (SSHUB/VOW)---*/
	regmap_update_bits(mpi->regmap,
				MT6338_PMRC_CON1,
				RG_VR_SPM_MODE_MASK_SFT,
				0x1 << RG_VR_SPM_MODE_SFT);

	/* Change PAD_PAD_SRCLKEN_IN0 into SW mode */
	regmap_update_bits(mpi->regmap,
				MT6338_TOP_CON,
				RG_SRCLKEN_IN_HW_MODE_MASK_SFT,
				0x0 << RG_SRCLKEN_IN_HW_MODE_SFT);
	regmap_update_bits(mpi->regmap,
				MT6338_TOP_CON,
				RG_SRCLKEN_IN_EN_MASK_SFT,
				0x1 << RG_SRCLKEN_IN_EN_SFT);

	/*---Multi-User---*/
	regmap_update_bits(mpi->regmap,
				MT6338_LDO_VAUD18_MULTI_SW_0,
				RG_LDO_VAUD18_EN_1_MASK_SFT,
				0x0 << RG_LDO_VAUD18_EN_1_SFT);
	regmap_update_bits(mpi->regmap,
				MT6338_LDO_VAUD18_MULTI_SW_1,
				RG_LDO_VAUD18_EN_2_MASK_SFT,
				0x0 << RG_LDO_VAUD18_EN_2_SFT);
#endif
}

void mt6338_Suspend_Setting(struct mt6338_pmic_info *mpi)
{
	regmap_write(mpi->regmap, MT6338_MTC_CTL0, 0x10);
	regmap_write(mpi->regmap, MT6338_MTC_CTL0, 0x11);
	regmap_write(mpi->regmap, MT6338_MTC_CTL0, 0x13);

	regmap_write(mpi->regmap, MT6338_DA_INTF_STTING3, 0x08);

	regmap_write(mpi->regmap, MT6338_LDO_VAUD18_CON2, 0x1C);
	regmap_write(mpi->regmap, MT6338_LDO_VAUD18_OP_EN0, 0x01);
	regmap_write(mpi->regmap, MT6338_LDO_VAUD18_OP_CFG0, 0x01);

	regmap_write(mpi->regmap, MT6338_DA_INTF_STTING1, 0x64);
	regmap_write(mpi->regmap, MT6338_DA_INTF_STTING1, 0x66);
	regmap_write(mpi->regmap, MT6338_DA_INTF_STTING1, 0x76);
}

void mt6338_InitSetting(struct mt6338_pmic_info *mpi)
{

	regmap_write(mpi->regmap, MT6338_TOP_CON, 0x7);
	regmap_write(mpi->regmap, MT6338_TEST_CON0, 0x1f);
	regmap_write(mpi->regmap, MT6338_SMT_CON0, 0x3);
	regmap_write(mpi->regmap, MT6338_GPIO_PULLEN0, 0xf9);
	regmap_write(mpi->regmap, MT6338_GPIO_PULLEN1, 0x1f);
	regmap_write(mpi->regmap, MT6338_TOP_CKPDN_CON0, 0x5b);
	regmap_write(mpi->regmap, MT6338_HK_TOP_CLK_CON0, 0x15);

	regmap_write(mpi->regmap, MT6338_DA_INTF_STTING3, 0x0c);
	regmap_write(mpi->regmap, MT6338_MTC_CTL0, 0x13);
	regmap_write(mpi->regmap, MT6338_PLT_CON0, 0x0);
	regmap_write(mpi->regmap, MT6338_PLT_CON1, 0x0);

	regmap_write(mpi->regmap, MT6338_HK_TOP_CLK_CON0, 0x15);
	regmap_write(mpi->regmap, MT6338_AUXADC_CON0, 0x0);
	regmap_write(mpi->regmap, MT6338_AUXADC_TRIM_SEL2, 0x40);

	regmap_write(mpi->regmap, MT6338_TOP_TOP_CKHWEN_CON0, 0x0f);
	regmap_write(mpi->regmap, MT6338_LDO_TOP_CLK_DCM_CON0, 0x01);
	regmap_write(mpi->regmap, MT6338_LDO_TOP_VR_CLK_CON0, 0x00);
	regmap_write(mpi->regmap, MT6338_LDO_VAUD18_CON2, 0x1c);
	regmap_write(mpi->regmap, MT6338_PLT_CON0, 0x3a);
	regmap_write(mpi->regmap, MT6338_PLT_CON1, 0x0c);
}

static const unsigned short mt6338_slave_addr = MT6338_PMIC_SLAVEID;

static int mt6338_pmic_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct mt6338_pmic_info *mpi;
	struct regmap_config *regmap_config = &mt6338_regmap_config;
	int ret;

	dev_info(&client->dev, "+%s()\n", __func__);

	mpi = devm_kzalloc(&client->dev, sizeof(*mpi), GFP_KERNEL);
	if (!mpi)
		return -ENOMEM;
	mpi->i2c = client;
	mpi->dev = &client->dev;
	i2c_set_clientdata(client, mpi);
	mutex_init(&mpi->io_lock);

	dev_info(&client->dev, "+%s() mutex_init\n", __func__);

	/* regmap regiser */
	regmap_config->lock_arg = &mpi->io_lock;
	mpi->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(mpi->regmap)) {
		dev_info(&client->dev, "regmap register fail\n");
		return PTR_ERR(mpi->regmap);
	}
	/* chip id check */
	mt6338_check_id(mpi);

	/* mfd cell register */
	ret = devm_mfd_add_devices(&client->dev, PLATFORM_DEVID_NONE,
				   mt6338_devs, ARRAY_SIZE(mt6338_devs), NULL,
				   0, NULL);
	if (ret < 0) {
		dev_info(&client->dev, "mfd add cells fail\n");
		goto out;
	}
	dev_info(&client->dev, "execute InitSetting\n");

	/* initial setting */
	mt6338_Keyunlock(mpi);
	mt6338_LP_Setting(mpi);
	mt6338_InitSetting(mpi);
	mt6338_Suspend_Setting(mpi);

	dev_info(&client->dev, "Successfully probed\n");
	return 0;
out:
	i2c_unregister_device(mpi->i2c);

	return ret;
}

static int mt6338_pmic_remove(struct i2c_client *client)
{
	struct mt6338_pmic_info *mpi = i2c_get_clientdata(client);

	i2c_unregister_device(mpi->i2c);
	return 0;
}

static const struct of_device_id __maybe_unused mt6338_pmic_of_id[] = {
	{ .compatible = "mediatek,mt6338_pmic", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6338_pmic_of_id);

static const struct i2c_device_id mt6338_pmic_id[] = {
	{ "mt6338_pmic", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6338_pmic_id);

static struct i2c_driver mt6338_pmic_driver = {
	.driver = {
		.name = "mt6338_pmic",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6338_pmic_of_id),
	},
	.probe = mt6338_pmic_probe,
	.remove = mt6338_pmic_remove,
	.id_table = mt6338_pmic_id,
};
module_i2c_driver(mt6338_pmic_driver);

MODULE_AUTHOR("Ting-Fang Hou<ting-fang.hou@mediatek.com>");
MODULE_DESCRIPTION("MT6338 PMIC I2C Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
