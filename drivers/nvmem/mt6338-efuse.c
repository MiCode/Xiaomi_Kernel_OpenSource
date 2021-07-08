// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
/* PMIC EFUSE registers definition */
#define MT6338_TOP_CKPDN_CON0	0x10b
#define MT6338_TOP_CKHWEN_CON0	0x117
#define MT6338_OTP_CON0		0x38d
/* offset of OTP_CON0 */
#define RG_OTP_PA_L		0x0
#define RG_OTP_PA_H		0x1
#define RG_OTP_RD_TRIG		0xA
#define RG_RD_RDY_BYPASS	0xB
#define RG_OTP_RD_SW		0xD
#define RG_OTP_DOUT_SW		0xE
#define RG_OTP_RD_BUSY		0xF
/* mask of OTP_PA */
#define RG_OTP_PA_MASK		0x3FF
/* Mask definition for EFUSE control engine clock register */
#define RG_EFUSE_CK_PDN_HWEN_MASK	BIT(0)
#define RG_EFUSE_CK_PDN_MASK		BIT(2)
#define RG_OTP_RD_BUSY_MASK		BIT(0)
/* Register SET/CLR offset */
#define SET_OFFSET	0x1
#define CLR_OFFSET	0x2
/* EFUSE Register width (bytes) definitions */
#define EFUSE_REG_WIDTH		1
/* Timeout (us) of polling the status */
#define EFUSE_POLL_TIMEOUT	30000
#define EFUSE_POLL_DELAY_US	30
#define EFUSE_READ_DELAY_US	10
struct efuse_chip_data {
	unsigned int reg_num;
	unsigned int base;
	unsigned int ck_pdn;
	unsigned int ck_pdn_hwen;
};
struct mt6338_efuse {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	unsigned int base;
	const struct efuse_chip_data *data;
	int trig_sta;
};
static int mt6338_efuse_poll_busy(struct mt6338_efuse *efuse)
{
	int ret;
	unsigned int val = 0;

	udelay(EFUSE_POLL_DELAY_US);
	ret = regmap_read_poll_timeout(efuse->regmap,
					efuse->data->base + RG_OTP_RD_BUSY,
					val,
					!(val & RG_OTP_RD_BUSY_MASK),
					EFUSE_POLL_DELAY_US,
					EFUSE_POLL_TIMEOUT);
	if (ret) {
		dev_info(efuse->dev, "timeout to update the efuse status\n");
		return ret;
	}
	return 0;
}
/* Read bit 8*offset ~ 8offset+7, offset = 0 ~ 127 */
static int mt6338_efuse_read(void *context, unsigned int offset,
			     void *_val, size_t bytes)
{
	struct mt6338_efuse *efuse = context;
	unsigned int base = efuse->data->base;
	unsigned int buf = 0;
	unsigned int offset_end = offset + bytes;
	unsigned char *val = _val;
	unsigned short otp_pa_val;
	int ret;

	mutex_lock(&efuse->lock);
	/* Enable the efuse ctrl engine clock */
	ret = regmap_write(efuse->regmap,
			   efuse->data->ck_pdn_hwen + CLR_OFFSET,
			   RG_EFUSE_CK_PDN_HWEN_MASK);
	if (ret)
		goto unlock_efuse;
	ret = regmap_write(efuse->regmap,
			   efuse->data->ck_pdn + CLR_OFFSET,
			   RG_EFUSE_CK_PDN_MASK);
	if (ret)
		goto disable_efuse;
	/* Set SW trigger read */
	ret = regmap_write(efuse->regmap, base + RG_OTP_RD_SW, 1);
	if (ret)
		goto disable_efuse;
	for (; offset < offset_end; offset += EFUSE_REG_WIDTH) {
		/* Set the row to be read, one row is 1 bytes data */
		otp_pa_val = (offset * BITS_PER_BYTE) & RG_OTP_PA_MASK;
		ret = regmap_bulk_write(efuse->regmap, base + RG_OTP_PA_L, &otp_pa_val, 2);
		if (ret)
			goto disable_efuse;
		/* Start trigger read */
		efuse->trig_sta = efuse->trig_sta ? 0 : 1;
		ret = regmap_write(efuse->regmap, base + RG_OTP_RD_TRIG, efuse->trig_sta);
		if (ret) {
			efuse->trig_sta = efuse->trig_sta ? 0 : 1;
			goto disable_efuse;
		}
		/* Ensure reading process is not in busy state */
		ret = mt6338_efuse_poll_busy(efuse);
		if (ret)
			goto disable_efuse;
		/* Read data from efuse memory, must delay before read */
		udelay(EFUSE_READ_DELAY_US);
		ret = regmap_read(efuse->regmap, base + RG_OTP_DOUT_SW, &buf);
		if (ret)
			goto disable_efuse;
		dev_info(efuse->dev, "EFUSE[%d]=0x%x\n", offset, buf);
		*val++ = (unsigned char)buf;
	}
disable_efuse:
	/* Disable SW trigger read */
	regmap_write(efuse->regmap, base + RG_OTP_RD_SW, 0);
	/* Disable the efuse ctrl engine clock */
	regmap_write(efuse->regmap, efuse->data->ck_pdn_hwen + SET_OFFSET,
		     RG_EFUSE_CK_PDN_HWEN_MASK);
	regmap_write(efuse->regmap, efuse->data->ck_pdn + SET_OFFSET,
		     RG_EFUSE_CK_PDN_MASK);
unlock_efuse:
	mutex_unlock(&efuse->lock);
	return ret;
}
static int mt6338_efuse_probe(struct platform_device *pdev)
{
	struct nvmem_config econfig = { };
	struct nvmem_device *nvmem;
	struct mt6338_efuse *efuse;
	int ret;

	efuse = devm_kzalloc(&pdev->dev, sizeof(*efuse), GFP_KERNEL);
	if (!efuse)
		return -ENOMEM;
	efuse->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!efuse->regmap) {
		dev_info(&pdev->dev, "failed to get efuse regmap\n");
		return -ENODEV;
	}
	efuse->data = of_device_get_match_data(&pdev->dev);
	if (!efuse->data) {
		dev_info(&pdev->dev, "failed to get efuse data\n");
		return -ENODEV;
	}
	ret = regmap_read(efuse->regmap, efuse->data->base + RG_OTP_RD_TRIG,
			  &efuse->trig_sta);
	if (ret)
		return ret;
	mutex_init(&efuse->lock);
	efuse->dev = &pdev->dev;
	platform_set_drvdata(pdev, efuse);
	econfig.stride = 1;
	econfig.word_size = EFUSE_REG_WIDTH;
	econfig.read_only = true;
	econfig.name = "mt6338-efuse";
	econfig.size = efuse->data->reg_num * EFUSE_REG_WIDTH;
	econfig.reg_read = mt6338_efuse_read;
	econfig.priv = efuse;
	econfig.dev = &pdev->dev;

	nvmem = devm_nvmem_register(&pdev->dev, &econfig);
	if (IS_ERR(nvmem)) {
		dev_info(&pdev->dev, "failed to register %s nvmem config\n",
			econfig.name);
		mutex_destroy(&efuse->lock);
		return PTR_ERR(nvmem);
	}
	dev_info(&pdev->dev, "%s done\n", __func__);
	return 0;
}
static const struct efuse_chip_data mt6338_efuse_data = {
	.reg_num = 128,
	.base = MT6338_OTP_CON0,
	.ck_pdn = MT6338_TOP_CKPDN_CON0,
	.ck_pdn_hwen = MT6338_TOP_CKHWEN_CON0
};
static const struct of_device_id mt6338_efuse_of_match[] = {
	{
		.compatible = "mediatek,mt6338-efuse",
		.data = &mt6338_efuse_data
	}, {
		/* sentinel */
	}
};
static struct platform_driver mt6338_efuse_driver = {
	.probe = mt6338_efuse_probe,
	.driver = {
		.name = "mt6338-efuse",
		.of_match_table = mt6338_efuse_of_match,
	},
};
module_platform_driver(mt6338_efuse_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wen Su <Wen.Su@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC eFuse Driver for MT6338 PMIC");
