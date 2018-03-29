/*
 *  sound/soc/codecs/rt5509.c
 *  Driver to Richtek RT5509 HPAMP IC
 *
 *  Copyright (C) 2015 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
/* alsa sound header */
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "rt5509.h"

struct reg_config {
	uint8_t reg_addr;
	uint32_t reg_data;
};

static const char * const prop_str[RT5509_CFG_MAX] = {
	"general",
	"boost_converter",
	"speaker_protect",
	"safe_guard",
	"eq",
	"bwe",
	"dcr",
	"mbdrc",
	"alc",
};

static struct reg_config battmode_config[] = {
};

static struct reg_config adaptive_config[] = {
};

static struct reg_config general_config[] = {
	{ 0x1e, 0x0b},
	{ 0x8a, 0x02},
	{ 0x94, 0x12},
	{ 0x15, 0x00},
	{ 0x81, 0x99},
	{ 0x50, 0x00},
	{ 0x2c, 0x00},
	{ 0x5b, 0x00},
	{ 0x82, 0x42},
	{ 0x83, 0xc0},
	{ 0x84, 0xb5},
	{ 0x85, 0xd5},
	{ 0xb3, 0x00},
	{ 0x88, 0x90},
	{ 0x96, 0xfd},
	{ 0x93, 0x55},
	{ 0x26, 0x00},
	{ 0x8d, 0xe0},
	{ 0x14, 0x7fff},
	{ 0xea, 0x81},
	{ 0xeb, 0xa1},
	{ 0xc3, 0x10},
	{ 0xfd, 0xf0},
	{ 0xee, 0x06},
	{ 0x86, 0x18},
	{ 0x13, 0x3bbb},
	{ 0x14, 0x6c00},
	{ 0x16, 0x4c},
	{ 0x18, 0x4a},
	{ 0x19, 0x016a},
	{ 0x1a, 0x0200},
	{ 0x1b, 0x77},
	{ 0x1c, 0x70},
	{ 0x1d, 0x80},
	{ 0x1f, 0x05d3},
	{ 0x20, 0x03cd},
	{ 0x21, 0x0209},
	{ 0x22, 0x07},
	{ 0x23, 0x47},
	{ 0x24, 0xc1},
	{ 0x25, 0x4f},
	{ 0x43, 0x44},
	{ 0x44, 0xb3},
	{ 0x45, 0x1a},
	{ 0x46, 0x800000},
	{ 0x81, 0xb9},
	{ 0x84, 0x35},
	{ 0x86, 0x14},
	{ 0x87, 0x04},
	{ 0x92, 0x57},
	{ 0x93, 0x54},
	{ 0xc0, 0x20},
	{ 0xc2, 0x000097},
	{ 0xeb, 0xa2},
	{ 0xec, 0x7474},
	{ 0xef, 0x001201},
	{ 0xf8, 0x55},
	{ 0xf9, 0x01},
	{ 0xfa, 0x0711},
};

static struct reg_config revc_general_config[] = {
	{ 0x14, 0x6000},
	{ 0x15, 0xB8},
	{ 0x16, 0x5c},
	{ 0x18, 0x0a},
	{ 0x19, 0x016a},
	{ 0x1a, 0x0000},
	{ 0x1c, 0x70},
	{ 0x1d, 0x80},
	{ 0x1e, 0x0b},
	{ 0x1f, 0x085b},
	{ 0x20, 0x05cf},
	{ 0x21, 0x02c7},
	{ 0x22, 0x0d},
	{ 0x23, 0x6f},
	{ 0x24, 0xb1},
	{ 0x25, 0x08},
	{ 0x2c, 0x7f},
	{ 0x2d, 0x0100},
	{ 0x2f, 0x0559},
	{ 0x45, 0x0b},
	{ 0x5a, 0x07dc},
	{ 0x81, 0xbb},
	{ 0x82, 0x43},
	{ 0x83, 0x54},
	{ 0x85, 0xd9},
	{ 0x86, 0x3f},
	{ 0x87, 0x1f},
	{ 0x88, 0x92},
	{ 0x8a, 0x02},
	{ 0x8d, 0xe0},
	{ 0x93, 0xd4},
	{ 0x94, 0x12},
	{ 0x96, 0x0d},
	{ 0x97, 0xa5},
	{ 0xb8, 0x80},
	{ 0xea, 0x95},
	{ 0xeb, 0xa4},
	{ 0xec, 0x7474},
	{ 0xee, 0x06},
	{ 0xf8, 0xd7},
	{ 0xfc, 0x955},
	{ 0xfd, 0xd8},
	{ 0xef, 0x001201},
	{ 0xb4, 0x81},
};

static int rt5509_block_read(
	void *client, u32 reg, int bytes, void *dest)
{
#if RT5509_SIMULATE_DEVICE
	struct rt5509_chip *chip = i2c_get_clientdata(client);
	int offset = 0, ret = 0;

	offset = rt5509_calculate_offset(reg);
	if (offset < 0) {
		dev_err(chip->dev, "%s: unknown register 0x%02x\n", __func__,
			ret);
		ret = -EINVAL;
	} else
		memcpy(dest, chip->sim + offset, bytes);
	return ret;
#else
	int ret = 0;

	/* customized for MTK I2C based upon no Sr */
	ret = i2c_master_send(client, (const char *)&reg, 1);
	if (ret < 0)
		return ret;
	return i2c_master_recv(client, dest, bytes);
#endif /* #if RT5509_SIMULATE_DEVICE */
}

static int rt5509_block_write(void *client, u32 reg,
	int bytes, const void *src)
{
#if RT5509_SIMULATE_DEVICE
	struct rt5509_chip *chip = i2c_get_clientdata(client);
	int offset = 0, ret = 0;

	offset = rt5509_calculate_offset(reg);
	if (offset < 0) {
		dev_err(chip->dev, "%s: unknown register 0x%02x\n", __func__,
			ret);
		ret = -EINVAL;
	} else
		memcpy(chip->sim + offset, src, bytes);
	return ret;
#else
	return i2c_smbus_write_i2c_block_data(client, reg, bytes, src);
#endif /* #if RT5509_SIMULATE_DEVICE */
}

static struct rt_regmap_fops rt5509_regmap_ops = {
	.read_device = rt5509_block_read,
	.write_device = rt5509_block_write,
};

/* Global read/write function */
static int rt5509_update_bits(struct i2c_client *i2c, u32 reg,
			   u32 mask, u32 data, int bytes)
{
	struct rt5509_chip *chip = i2c_get_clientdata(i2c);
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd;

	return rt_regmap_update_bits(chip->rd, &rrd, reg, mask, data);
#else
	u32 read_data = 0;
	u8* p_data = (u8 *)&read_data;
	int i = 0, j = 0, ret = 0;

	down(&chip->io_semaphore);
	ret = rt5509_block_read(chip->i2c, reg, bytes, &read_data);
	if (ret < 0)
		goto err_bits;
	j = (bytes / 2);
	for (i = 0; i < j; i++)
		swap(p_data[i], p_data[bytes - i]);
	ret = rt5509_block_write(chip->i2c, reg, bytes, &read_data);
	if (ret < 0)
		goto err_bits;
err_bits:
	up(&chip->io_semaphore);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt5509_set_bits(struct i2c_client *i2c, u32 reg, u8 mask)
{
	return rt5509_update_bits(i2c, reg, mask, mask, 1);
}

static int rt5509_clr_bits(struct i2c_client *i2c, u32 reg, u8 mask)
{
	return rt5509_update_bits(i2c, reg, mask, 0, 1);
}

static unsigned int rt5509_io_read(struct snd_soc_codec *codec,
	 unsigned int reg)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	dev_dbg(codec->dev, "%s: reg %02x\n", __func__, reg);
	ret = rt_regmap_reg_read(chip->rd, &rrd, reg);
	return (ret < 0 ? ret : rrd.rt_data.data_u32);
#else
	u8 data = 0;

	down(&chip->io_semaphore);
	ret = rt5509_block_read(chip->i2c, reg, 1, &data);
	up(&chip->io_semaphore);
	return (ret < 0 ? ret : data);
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt5509_io_write(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int data)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	dev_dbg(codec->dev, "%s: reg %02x data %02x\n", __func__, reg, data);
	return rt_regmap_reg_write(chip->rd, &rrd, reg, data);
#else
	int ret = 0;

	down(&chip->io_semaphore);
	ret = rt5509_block_write(chip->i2c, reg, 1, &data);
	up(&chip->io_semaphore);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static inline int rt5509_power_on(struct rt5509_chip *chip, bool en)
{
	int ret = 0;

	dev_dbg(chip->dev, "%s: en %d\n", __func__, en);
	if (en)
		ret = rt5509_clr_bits(chip->i2c, RT5509_REG_CHIPEN,
				RT5509_CHIPPD_ENMASK);
	else
		ret = rt5509_set_bits(chip->i2c, RT5509_REG_CHIPEN,
				RT5509_CHIPPD_ENMASK);
	mdelay(1);
	return ret;
}

static int rt5509_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

#else
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#endif
	static uint8_t mode_store;
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		dapm->bias_level = level;
		break;
	case SND_SOC_BIAS_STANDBY:
		ret = rt5509_power_on(chip, true);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_DSPKCONF5,
			RT5509_VBG_ENMASK, RT5509_VBG_ENMASK);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_DSPKVMID,
			RT5509_VMID_ENMASK, RT5509_VMID_ENMASK);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_MTPFLOWA,
					  0x04, 0x04);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_write(codec, RT5509_REG_BST_MODE, mode_store);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_VBATSENSE,
					  0x20, 0x20);
		if (ret < 0)
			goto out_set_bias;
		dapm->bias_level = level;
		ret = 0;
		break;
	case SND_SOC_BIAS_OFF:
		ret = snd_soc_update_bits(codec, RT5509_REG_VBATSENSE,
					  0x20, 0x00);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_read(codec, RT5509_REG_BST_MODE);
		if (ret < 0)
			goto out_set_bias;
		mode_store = ret;
		ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
					  0x03, 0x00);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_MTPFLOWA,
					  0x04, 0x00);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_DSPKVMID,
			RT5509_VMID_ENMASK, ~RT5509_VMID_ENMASK);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_update_bits(codec, RT5509_REG_DSPKCONF5,
			RT5509_VBG_ENMASK, ~RT5509_VBG_ENMASK);
		if (ret < 0)
			goto out_set_bias;
		ret = snd_soc_read(codec, RT5509_REG_INTERRUPT);
		if (ret < 0)
			goto out_set_bias;
		ret = rt5509_power_on(chip, false);
		if (ret < 0)
			goto out_set_bias;
		dapm->bias_level = level;
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}
out_set_bias:
	dev_info(codec->dev, "%s: bias_level %d\n", __func__, level);
	return ret;
}

static int rt5509_init_battmode_setting(struct snd_soc_codec *codec)
{
	int i = 0, ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(battmode_config); i++) {
		ret = snd_soc_write(codec, battmode_config[i].reg_addr,
				    battmode_config[i].reg_data);
		if (ret < 0)
			break;
	}
	return ret;
}

static int rt5509_init_adaptive_setting(struct snd_soc_codec *codec)
{
	int i = 0, ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(adaptive_config); i++) {
		ret = snd_soc_write(codec, adaptive_config[i].reg_addr,
				    adaptive_config[i].reg_data);
		if (ret < 0)
			break;
	}
	return ret;
}

static int rt5509_init_general_setting(struct snd_soc_codec *codec)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct reg_config *reg_cfg;
	int reg_cfg_size = 0;
	int i = 0, ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	if (chip->chip_rev >= RT5509_CHIP_REVC) {
		reg_cfg = revc_general_config;
		reg_cfg_size = ARRAY_SIZE(revc_general_config);
	} else {
		reg_cfg = general_config;
		reg_cfg_size = ARRAY_SIZE(general_config);
	}
	for (i = 0; i < reg_cfg_size; i++) {
		ret = snd_soc_write(codec, reg_cfg[i].reg_addr,
				    reg_cfg[i].reg_data);
		if (ret < 0)
			break;
	}
	return ret;
}

static int rt5509_do_tcsense_fix(struct snd_soc_codec *codec)
{
	uint32_t tc_sense = 0, vtemp = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN1, 0x40, 0x40);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_OTPCONF, 0x82);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5509_REG_OTPCONF, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN1, 0x40, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5509_REG_VTEMP_TRIM);
	if (ret < 0)
		return ret;
	vtemp = ret & 0xffff;
	tc_sense = (1073741824U / vtemp * 272 / 196) << 1;
	if (tc_sense & 0x10000)
		tc_sense = (tc_sense & 0xffff) + 1;
	tc_sense &= 0xffff;
	dev_dbg(codec->dev, "tc_sense %04x\n", tc_sense);
	return snd_soc_write(codec, RT5509_REG_TCOEFF, tc_sense);
}

static int rt5509_init_proprietary_setting(struct snd_soc_codec *codec)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct rt5509_proprietary_param *p_param = chip->pdata->p_param;
	const u8 *cfg;
	u32 cfg_size;
	int i = 0, j = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	if (!p_param)
		goto out_init_proprietary;
	ret = snd_soc_write(codec, RT5509_REG_CLKEN1, 0x04);
	if (ret < 0)
		goto out_init_proprietary;
	for (i = 0; i < RT5509_CFG_MAX; i++) {
		cfg = p_param->cfg[i];
		cfg_size = p_param->cfg_size[i];
		if (!cfg)
			continue;
		dev_dbg(chip->dev, "%s start\n", prop_str[i]);
		for (j = 0; j < cfg_size;) {
#ifdef CONFIG_RT_REGMAP
			ret = rt_regmap_block_write(chip->rd, cfg[0], cfg[1],
					      cfg + 2);
#else
			ret = rt5509_block_write(chip->i2c, cfg[0], cfg[1],
					      cfg + 2);
#endif /* #ifdef CONFIG_RT_REGMAP */
			if (ret < 0)
				dev_err(chip->dev, "set %02x fail\n", cfg[0]);
			j += (2 + cfg[1]);
			cfg += (2 + cfg[1]);
		}
		dev_dbg(chip->dev, "%s end\n", prop_str[i]);
	}
	ret = snd_soc_write(codec, RT5509_REG_CLKEN1, 0x00);
	if (ret < 0)
		goto out_init_proprietary;
	if (p_param->cfg_size[RT5509_CFG_SPEAKERPROT]) {
		ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
			RT5509_SPKPROT_ENMASK, RT5509_SPKPROT_ENMASK);
	}
out_init_proprietary:
	return ret;
}

static ssize_t rt5509_proprietary_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct rt5509_chip *chip = dev_get_drvdata(dev);
	struct rt5509_proprietary_param *param = chip->pdata->p_param;
	int i = 0, j = 0;
	const u8 *cfg;
	u32 cfg_size;

	dev_dbg(chip->dev, "%s\n", __func__);
	if (!param) {
		i += scnprintf(buf + i, PAGE_SIZE - i, "no proprietary parm\n");
		goto out_show;
	}
	for (j = 0; j < RT5509_CFG_MAX; j++) {
		cfg = param->cfg[j];
		cfg_size = param->cfg_size[j];
		if (!cfg) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "no %s cfg\n",
				       prop_str[j]);
			continue;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s size %d\n",
			       prop_str[j], cfg_size);
	}
out_show:
	return i;
}

static ssize_t rt5509_proprietary_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t cnt)
{
	struct rt5509_chip *chip = dev_get_drvdata(dev);
	struct rt5509_proprietary_param *param = NULL;
	int i = 0, size = 0;
	const u8 *bin_offset;
	u32 *sptr;

	dev_dbg(chip->dev, "%s, size %d\n", __func__, (int)cnt);
	/* do mark check */
	if (cnt < 7) {
		dev_err(chip->dev, "data is invalid\n");
		goto out_param_write;
	}
	if (strncmp("richtek", buf, 7)) {
		dev_err(chip->dev, "data is invalid\n");
		goto out_param_write;
	}
	/* do size check */
	sptr = (u32 *)(buf + cnt - 4);
	size = *sptr;
	dev_dbg(chip->dev, "size %d\n", size);
	if (cnt < 47 || (size + 47) != cnt) {
		dev_err(chip->dev, "sorry bin size is wrong\n");
		goto out_param_write;
	}
	for (i = 0; i < RT5509_CFG_MAX; i++) {
		sptr = (u32 *)(buf + cnt - 40 + i * 4);
		size -= *sptr;
	}
	if (size != 0) {
		dev_err(chip->dev, "sorry, bin format is wrong\n");
		goto out_param_write;
	}
	/* if previous one is existed, release it */
	param = chip->pdata->p_param;
	if (param) {
		dev_dbg(chip->dev, "previous existed\n");
		for (i = 0; i < RT5509_CFG_MAX; i++)
			devm_kfree(chip->dev, param->cfg[i]);
		devm_kfree(chip->dev, param);
		chip->pdata->p_param = NULL;
	}
	/* start to copy */
	param = devm_kzalloc(chip->dev, sizeof(*param), GFP_KERNEL);
	if (!param) {
		dev_err(chip->dev, "allocation memory fail\n");
		goto out_param_write;
	}
	bin_offset = buf + 7;
	for (i = 0; i < RT5509_CFG_MAX; i++) {
		sptr = (u32 *)(buf + cnt - 40 + i * 4);
		param->cfg_size[i] = *sptr;
		param->cfg[i] = devm_kzalloc(chip->dev,
					     sizeof(u8) * param->cfg_size[i],
					     GFP_KERNEL);
		memcpy(param->cfg[i], bin_offset, param->cfg_size[i]);
		bin_offset += param->cfg_size[i];
	}
	chip->pdata->p_param = param;
	if (rt5509_set_bias_level(chip->codec, SND_SOC_BIAS_STANDBY) < 0)
		goto out_param_write;
	rt5509_init_proprietary_setting(chip->codec);
	rt5509_do_tcsense_fix(chip->codec);
	if (rt5509_set_bias_level(chip->codec, SND_SOC_BIAS_OFF) < 0)
		goto out_param_write;
	return cnt;
out_param_write:
	return -EINVAL;
}

static struct device_attribute rt5509_proprietary_attr = {
	.attr = {
		.name = "prop_param",
		.mode = S_IRUGO | S_IWUSR,
	},
	.show = rt5509_proprietary_show,
	.store = rt5509_proprietary_store,
};

static int rt5509_param_probe(struct platform_device *pdev)
{
	struct rt5509_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int ret = 0;

	ret = device_create_file(&pdev->dev, &rt5509_proprietary_attr);
	if (ret < 0) {
		dev_err(&pdev->dev, "create file error\n");
		return ret;
	}
	platform_set_drvdata(pdev, chip);
	return 0;
}

static int rt5509_param_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &rt5509_proprietary_attr);
	return 0;
}

static struct platform_driver rt5509_param_driver = {
	.driver = {
		.name = "rt5509_param",
		.owner = THIS_MODULE,
	},
	.probe = rt5509_param_probe,
	.remove = rt5509_param_remove,
};

static int rt5509_param_create(struct rt5509_chip *chip)
{
	static int dev_cnt;

	chip->pdev = platform_device_register_data(chip->dev, "rt5509_param",
						   dev_cnt++, NULL, 0);
	if (!chip->pdev)
		return -EFAULT;
	return platform_driver_register(&rt5509_param_driver);
}

static void rt5509_param_destroy(struct rt5509_chip *chip)
{
	platform_device_unregister(chip->pdev);
	platform_driver_unregister(&rt5509_param_driver);
}

static int rt5509_codec_probe(struct snd_soc_codec *codec)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	/* CHIP Enable */
	ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
		RT5509_CHIPPD_ENMASK, ~RT5509_CHIPPD_ENMASK);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5509_init_general_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5509_init_adaptive_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5509_init_battmode_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5509_init_proprietary_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5509_do_tcsense_fix(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5509_param_create(chip);
	if (ret < 0)
		goto err_out_probe;
	chip->codec = codec;
	ret = rt5509_calib_create(chip);
	if (ret < 0)
		goto err_out_probe;
	dev_info(codec->dev, "%s\n", __func__);
	return rt5509_set_bias_level(codec, SND_SOC_BIAS_OFF);
err_out_probe:
	dev_err(codec->dev, "chip io error\n");
	/* Chip Disable */
	ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
		RT5509_CHIPPD_ENMASK, RT5509_CHIPPD_ENMASK);
	return ret < 0 ? ret : -EINVAL;
}

static int rt5509_codec_remove(struct snd_soc_codec *codec)
{
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);

	rt5509_calib_destroy(chip);
	rt5509_param_destroy(chip);
	return rt5509_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

#ifdef CONFIG_PM
static int rt5509_codec_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int rt5509_codec_resume(struct snd_soc_codec *codec)
{
	return 0;
}
#else
#define rt5509_codec_suspend NULL
#define rt5509_codec_resume NULL
#endif /* #ifdef CONFIG_PM */

static int rt5509_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#else
	struct snd_soc_codec *codec = w->codec;
#endif

	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN1,
			RT5509_CLKEN1_MASK, 0xbf);
		if (ret < 0)
			goto out_clk_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN2,
			RT5509_CLKEN2_MASK, RT5509_CLKEN2_MASK);
		if (ret < 0)
			goto out_clk_event;
		break;
	case SND_SOC_DAPM_POST_PMD:
		msleep(20);
		ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN2,
			RT5509_CLKEN2_MASK, ~RT5509_CLKEN2_MASK);
		if (ret < 0)
			goto out_clk_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN1,
			RT5509_CLKEN1_MASK, ~RT5509_CLKEN1_MASK);
		if (ret < 0)
			goto out_clk_event;
		break;
	default:
		break;
	}
out_clk_event:
	return ret;
}

static int rt5509_boost_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#else
	struct snd_soc_codec *codec = w->codec;
#endif
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
	static uint8_t mode_store;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = snd_soc_read(codec, RT5509_REG_BST_MODE);
		if (ret < 0)
			goto out_boost_event;
		mode_store = ret;
		ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
			0x03, 0x01);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
			RT5509_TRIWAVE_ENMASK, RT5509_TRIWAVE_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_MSKFLAG,
			0x3F, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_DSPKEN1,
					  RT5509_BUF_ENMASK | RT5509_BIAS_ENMASK,
					  RT5509_BUF_ENMASK | RT5509_BIAS_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec,
			RT5509_REG_AMPCONF, 0xF8, 0xB8);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_BSTTM,
			0x40, 0x40);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_OCPOTPEN,
			0x03, 0x03);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_OVPUVPCTRL,
			0xe0, 0xe0);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_MSKFLAG,
			0x3F, 0x3F);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN1,
			0x40, 0x40);
		if (ret < 0)
			goto out_boost_event;
		dev_info(chip->dev, "amp turn on\n");
		break;
	case SND_SOC_DAPM_POST_PMU:
		msleep(10);
		ret = snd_soc_update_bits(codec, RT5509_REG_BST_MODE,
			0x03, mode_store);
		if (ret < 0)
			goto out_boost_event;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_info(chip->dev, "amp turn off\n");
		ret = snd_soc_update_bits(codec, RT5509_REG_CLKEN1,
			0x40, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_OVPUVPCTRL,
			0xe0, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_write(codec, RT5509_REG_OCPMAX, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_OCPOTPEN,
			0x03, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_BSTTM,
			0x40, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec,
			RT5509_REG_AMPCONF, 0xF8, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_DSPKEN1,
			RT5509_BUF_ENMASK | RT5509_BIAS_ENMASK,
			~(RT5509_BUF_ENMASK | RT5509_BIAS_ENMASK));
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5509_REG_CHIPEN,
			RT5509_TRIWAVE_ENMASK, ~RT5509_TRIWAVE_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		break;
	default:
		break;
	}
out_boost_event:
	return ret;
}

static const char * const rt5509_i2smux_text[] = { "I2S1", "I2S2"};
static const char * const rt5509_i2sdomux_text[] = { "I2SDOR/L", "DATAI3"};
static SOC_ENUM_SINGLE_DECL(rt5509_i2s_muxsel,
	RT5509_REG_I2SSEL, RT5509_I2SSEL_SHFT, rt5509_i2smux_text);
static SOC_ENUM_SINGLE_DECL(rt5509_i2s_dosel,
	RT5509_REG_I2SDOSEL, 1, rt5509_i2sdomux_text);
static const struct snd_kcontrol_new rt5509_i2smux_ctrl =
	SOC_DAPM_ENUM("Switch", rt5509_i2s_muxsel);
static const struct snd_kcontrol_new rt5509_i2sdo_ctrl =
	SOC_DAPM_ENUM("Switch", rt5509_i2s_dosel);
static const struct snd_soc_dapm_widget rt5509_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("I2S Mux", SND_SOC_NOPM, 0, 0, &rt5509_i2smux_ctrl),
	SND_SOC_DAPM_MUX("I2SDO Mux", RT5509_REG_I2SDOSEL, 0, 0,
		&rt5509_i2sdo_ctrl),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("BOOST", RT5509_REG_CHIPEN, RT5509_SPKAMP_ENSHFT,
		0, NULL, 0, rt5509_boost_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("CLK", RT5509_REG_PLLCONF1, 0, 1, rt5509_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

static const struct snd_soc_dapm_route rt5509_dapm_routes[] = {
	{ "I2S Mux", "I2S1", "AIF1 Playback"},
	{ "I2S Mux", "I2S2", "AIF2 Playback"},
	/* DATAO path start */
	{ "I2SDO Mux", "I2SDOR/L", "I2S Mux"},
	{ "I2SDO Mux", "DATAI3", "I2S Mux"},
	{ "AIF1 Capture", NULL, "I2SDO Mux"},
	/* DATAO path end */
	{ "DAC", NULL, "I2S Mux"},
	{ "DAC", NULL, "CLK"},
	{ "PGA", NULL, "DAC"},
	{ "BOOST", NULL, "PGA"},
	{ "Speaker", NULL, "BOOST"},
};

static int rt5509_bypassdsp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = chip->bypass_dsp;
	return 0;
}

static int rt5509_bypassdsp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5509_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *se = (struct soc_enum *)kcontrol->private_value;
	static uint8_t func_en;
	static uint8_t chip_en;
	int ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0))
	if (ucontrol->value.enumerated.item[0] >= se->items)
#else
	if (ucontrol->value.enumerated.item[0] >= se->max)
#endif
		return -EINVAL;
	if (ucontrol->value.enumerated.item[0] == chip->bypass_dsp)
		return 0;
	if (ucontrol->value.enumerated.item[0]) {
		func_en = snd_soc_read(codec, RT5509_REG_FUNCEN);
		chip_en = snd_soc_read(codec, RT5509_REG_CHIPEN);
		ret = rt5509_power_on(chip, true);
		if (ret < 0)
			return ret;
		snd_soc_write(codec, RT5509_REG_FUNCEN, 0);
		snd_soc_write(codec, RT5509_REG_CHIPEN,
			      chip_en & (~RT5509_SPKPROT_ENMASK));
	} else {
		ret = rt5509_power_on(chip, true);
		if (ret < 0)
			return ret;
		snd_soc_write(codec, RT5509_REG_FUNCEN, func_en);
		snd_soc_write(codec, RT5509_REG_CHIPEN, chip_en);
	}
	chip->bypass_dsp = ucontrol->value.enumerated.item[0];
	return 0;
}

static int rt5509_put_spk_volsw(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#endif
	int orig_pwron = 0, ret = 0;

	orig_pwron = (dapm->bias_level == SND_SOC_BIAS_OFF) ? 0 : 1;
	if (!orig_pwron)
		rt5509_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (!orig_pwron)
		rt5509_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return ret;
}

static const DECLARE_TLV_DB_SCALE(dacvol_tlv, -1275, 5, 0);
static const DECLARE_TLV_DB_SCALE(boostvol_tlv, 0, 3, 0);
static const DECLARE_TLV_DB_SCALE(postpgavol_tlv, -31, 1, 0);
static const DECLARE_TLV_DB_SCALE(prepgavol_tlv, -6, 3, 0);
static const char * const rt5509_enable_text[] = { "Disable", "Enable"};
static const struct soc_enum rt5509_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rt5509_enable_text), rt5509_enable_text),
};
static const struct snd_kcontrol_new rt5509_controls[] = {
	SOC_SINGLE_EXT_TLV("DAC Volume", RT5509_REG_VOLUME, 0, 255, 1,
		snd_soc_get_volsw, rt5509_put_spk_volsw, dacvol_tlv),
	SOC_SINGLE_EXT_TLV("Boost Volume", RT5509_REG_SPKGAIN, RT5509_BSTGAIN_SHFT,
		5, 0, snd_soc_get_volsw, rt5509_put_spk_volsw, boostvol_tlv),
	SOC_SINGLE_EXT_TLV("PostPGA Volume", RT5509_REG_SPKGAIN,
		RT5509_POSTPGAGAIN_SHFT, 31, 0, snd_soc_get_volsw,
		rt5509_put_spk_volsw, postpgavol_tlv),
	SOC_SINGLE_EXT_TLV("PrePGA Volume", RT5509_REG_DSPKCONF1,
		RT5509_PREPGAGAIN_SHFT, 3, 0, snd_soc_get_volsw,
		rt5509_put_spk_volsw, prepgavol_tlv),
	SOC_SINGLE_EXT("Speaker Protection", RT5509_REG_CHIPEN, RT5509_SPKPROT_ENSHFT,
		1, 0, snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("Limiter Func", RT5509_REG_FUNCEN, RT5509_LMTEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("ALC Func", RT5509_REG_FUNCEN, RT5509_ALCEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("MBDRC Func", RT5509_REG_FUNCEN, RT5509_MBDRCEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("BWE Func", RT5509_REG_FUNCEN, RT5509_BEWEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("HPF Func", RT5509_REG_FUNCEN, RT5509_HPFEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("BQ Func", RT5509_REG_FUNCEN, RT5509_BQEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("CLIP Func", RT5509_REG_CLIP_CTRL, RT5509_CLIPEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("BoostMode", RT5509_REG_BST_MODE, RT5509_BSTMODE_SHFT, 3, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("I2S_Channel", RT5509_REG_I2SSEL, RT5509_I2SLRSEL_SHFT, 3, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("Ext_DO_Enable", RT5509_REG_I2SDOSEL, 0, 1, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("I2SDOL Mux", RT5509_REG_I2SDOLRSEL, 0, 15, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_SINGLE_EXT("I2SDOR Mux", RT5509_REG_I2SDOLRSEL, 4, 15, 0,
		snd_soc_get_volsw, rt5509_put_spk_volsw),
	SOC_ENUM_EXT("BypassDSP", rt5509_enum[0], rt5509_bypassdsp_get,
		rt5509_bypassdsp_put),
};

static struct snd_soc_codec_driver rt5509_codec_drv = {
	.probe = rt5509_codec_probe,
	.remove = rt5509_codec_remove,
	.suspend = rt5509_codec_suspend,
	.resume = rt5509_codec_resume,

	.controls = rt5509_controls,
	.num_controls = ARRAY_SIZE(rt5509_controls),
	.dapm_widgets = rt5509_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5509_dapm_widgets),
	.dapm_routes = rt5509_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5509_dapm_routes),

	.set_bias_level = rt5509_set_bias_level,
	.idle_bias_off = true,
	/* codec io */
	.read = rt5509_io_read,
	.write = rt5509_io_write,
};

static int rt5509_aif_digital_mute(struct snd_soc_dai *dai, int mute)
{
	dev_info(dai->dev, "%s: mute %d\n", __func__, mute);
	return snd_soc_update_bits(dai->codec, RT5509_REG_CHIPEN,
		RT5509_SPKMUTE_ENMASK,
		mute ? RT5509_SPKMUTE_ENMASK : ~RT5509_SPKMUTE_ENMASK);
}

static int rt5509_aif_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 regval = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s: fmt:%d\n", __func__, fmt);
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK ) {
	case SND_SOC_DAIFMT_I2S:
		regval |= (RT5509_AUDFMT_I2S << RT5509_AUDFMT_SHFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		regval |= (RT5509_AUDFMT_RIGHTJ << RT5509_AUDFMT_SHFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		regval |= (RT5509_AUDFMT_LEFTJ << RT5509_AUDFMT_SHFT);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		regval |= (RT5509_DSP_MODEA << RT5509_DSPMODE_SHFT);
		break;
	case SND_SOC_DAIFMT_DSP_B:
		regval |= (RT5509_DSP_MODEB << RT5509_DSPMODE_SHFT);
		break;
	default:
		break;
	}
	ret = snd_soc_update_bits(dai->codec, RT5509_REG_AUDFMT,
			RT5509_DSPMODE_MASK | RT5509_AUDFMT_MASK, regval);
	if (ret < 0)
		dev_err(dai->dev, "config dac audfmt error\n");
	return ret;
}

static int rt5509_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	return 0;
}

static int rt5509_aif_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	const struct snd_pcm_runtime *runtime = substream->runtime;
	/* 0 for sr and bckfs, 1 for audbits */
	u8 regval[2] = {0};
	u32 pll_divider = 0;
	u8 word_len = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s\n", __func__);
	dev_dbg(dai->dev, "format %d\n", runtime->format);
	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16:
	case SNDRV_PCM_FORMAT_U16:
		regval[0] |= (RT5509_BCKMODE_32FS << RT5509_BCKMODE_SHFT);
		regval[1] |= (RT5509_AUDBIT_16 << RT5509_AUDBIT_SHFT);
		pll_divider = 0x00100000;
		word_len = 16 * 4;
		break;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U18_3BE:
		regval[0] |= (RT5509_BCKMODE_48FS << RT5509_BCKMODE_SHFT);
		regval[1] |= (RT5509_AUDBIT_18 << RT5509_AUDBIT_SHFT);
		pll_divider = 0x000c0000;
		word_len = 18 * 4;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_U20_3BE:
		regval[0] |= (RT5509_BCKMODE_48FS << RT5509_BCKMODE_SHFT);
		regval[1] |= (RT5509_AUDBIT_20 << RT5509_AUDBIT_SHFT);
		pll_divider = 0x000c0000;
		word_len = 20 * 4;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
		regval[0] |= (RT5509_BCKMODE_48FS << RT5509_BCKMODE_SHFT);
		regval[1] |= (RT5509_AUDBIT_24 << RT5509_AUDBIT_SHFT);
		pll_divider = 0x000c0000;
		word_len = 24 * 4;
		break;
	case SNDRV_PCM_FORMAT_S32:
	case SNDRV_PCM_FORMAT_U32:
		regval[0] |= (RT5509_BCKMODE_64FS << RT5509_BCKMODE_SHFT);
		regval[1] |= (RT5509_AUDBIT_24 << RT5509_AUDBIT_SHFT);
		pll_divider = 0x00080000;
		word_len = 24 * 4;
		break;
	default:
		ret = -EINVAL;
		goto out_prepare;
	}
	dev_dbg(dai->dev, "rate %d\n", runtime->rate);
	switch (runtime->rate) {
	case 8000:
		regval[0] |= (RT5509_SRMODE_8K << RT5509_SRMODE_SHFT);
		pll_divider *= 6;
		break;
	case 11025:
	case 12000:
		regval[0] |= (RT5509_SRMODE_12K << RT5509_SRMODE_SHFT);
		pll_divider *= 4;
		break;
	case 16000:
		regval[0] |= (RT5509_SRMODE_16K << RT5509_SRMODE_SHFT);
		pll_divider *= 3;
		break;
	case 22050:
	case 24000:
		regval[0] |= (RT5509_SRMODE_24K << RT5509_SRMODE_SHFT);
		pll_divider *= 2;
		break;
	case 32000:
		regval[0] |= (RT5509_SRMODE_32K << RT5509_SRMODE_SHFT);
		pll_divider = (pll_divider * 3) >> 1;
		break;
	case 44100:
	case 48000:
		regval[0] |= (RT5509_SRMODE_48K << RT5509_SRMODE_SHFT);
		break;
	case 88200:
	case 96000:
		regval[0] |= (RT5509_SRMODE_96K << RT5509_SRMODE_SHFT);
		pll_divider >>= 1;
		break;
	case 176400:
	case 192000:
		regval[0] |= (RT5509_SRMODE_192K << RT5509_SRMODE_SHFT);
		pll_divider >>= 2;
		break;
	default:
		ret = -EINVAL;
		goto out_prepare;
	}
	ret = snd_soc_update_bits(dai->codec, RT5509_REG_AUDSR,
			RT5509_BCKMODE_MASK | RT5509_SRMODE_MASK, regval[0]);
	if (ret < 0) {
		dev_err(dai->dev, "configure bck and sr fail\n");
		goto out_prepare;
	}
	ret = snd_soc_update_bits(dai->codec, RT5509_REG_AUDFMT,
			RT5509_AUDBIT_MASK, regval[1]);
	if (ret < 0) {
		dev_err(dai->dev, "configure audbit fail\n");
		goto out_prepare;
	}
	ret = snd_soc_write(dai->codec, RT5509_REG_PLLDIVISOR, pll_divider);
	if (ret < 0) {
		dev_err(dai->dev, "configure pll divider fail\n");
		goto out_prepare;
	}
	ret = snd_soc_write(dai->codec, RT5509_REG_DMGFLAG, word_len);
	if (ret < 0)
		dev_err(dai->dev, "configure word len fail\n");
out_prepare:
	return ret;
}

static int rt5509_aif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
	return rt5509_set_bias_level(dai->codec, SND_SOC_BIAS_STANDBY);
}

static void rt5509_aif_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
}

static int rt5509_aif_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

	dev_dbg(dai->dev, "%s: cmd=%d\n", __func__, cmd);
	dev_dbg(dai->dev, "%s: %c\n", __func__, capture ? 'c' : 'p');
	return 0;
}

static const struct snd_soc_dai_ops rt5509_dai_ops = {
	.set_fmt = rt5509_aif_set_fmt,
	.hw_params = rt5509_aif_hw_params,
	.digital_mute = rt5509_aif_digital_mute,
	.startup = rt5509_aif_startup,
	.shutdown = rt5509_aif_shutdown,
	.trigger = rt5509_aif_trigger,
	.prepare = rt5509_aif_prepare,
};

#define RT5509_RATES SNDRV_PCM_RATE_8000_192000
#define RT5509_FORMATS (SNDRV_PCM_FMTBIT_S16 | SNDRV_PCM_FMTBIT_S18_3LE |\
	SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_3LE |\
	SNDRV_PCM_FMTBIT_S32)

static struct snd_soc_dai_driver rt5509_i2s_dais[] = {
	{
		.name = "rt5509-aif1",
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5509_RATES,
			.formats = RT5509_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5509_RATES,
			.formats = RT5509_FORMATS,
		},
		.ops = &rt5509_dai_ops,
	},
	{
		.name = "rt5509-aif2",
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5509_RATES,
			.formats = RT5509_FORMATS,
		},
		.ops = &rt5509_dai_ops,
	},
};

static inline int rt5509_codec_register(struct rt5509_chip *chip)
{
	return snd_soc_register_codec(chip->dev, &rt5509_codec_drv,
		rt5509_i2s_dais, ARRAY_SIZE(rt5509_i2s_dais));
}

static inline int rt5509_codec_unregister(struct rt5509_chip *chip)
{
	snd_soc_unregister_codec(chip->dev);
	return 0;
}

static int rt5509_handle_pdata(struct rt5509_chip *chip)
{
	return 0;
}

static int rt5509_i2c_initreg(struct rt5509_chip *chip)
{
	int ret = 0;

	/* mute first, before codec_probe register init */
	ret = rt5509_set_bits(chip->i2c,
		RT5509_REG_CHIPEN, RT5509_SPKMUTE_ENMASK);
	if (ret < 0)
		goto out_init_reg;
	/* disable TriWave */
	ret = rt5509_clr_bits(chip->i2c, RT5509_REG_CHIPEN,
		RT5509_TRIWAVE_ENMASK);
	if (ret < 0)
		goto out_init_reg;
	/* disable all digital clock */
	ret = rt5509_clr_bits(chip->i2c, RT5509_REG_CLKEN1,
		RT5509_CLKEN1_MASK);
	if (ret < 0)
		goto out_init_reg;
	ret = rt5509_clr_bits(chip->i2c, RT5509_REG_CLKEN2,
		RT5509_CLKEN2_MASK);
out_init_reg:
	return ret;
}

static int rt5509_get_chip_rev(struct rt5509_chip *chip)
{
	int ret = 0;
	u8 data = 0;

	ret = rt5509_block_read(chip->i2c, RT5509_REG_CHIPREV, 1, &data);
	if (ret < 0)
		return ret;
	if ((data & RT5509_CHIPID_MASK) != RT5509_CHIP_ID)
		return -ENODEV;
	chip->chip_rev = (data & RT5509_CHIPREV_MASK) >> RT5509_CHIPREV_SHFT;
	dev_dbg(chip->dev, "chip revision %d\n", chip->chip_rev);
	return 0;
}

static int rt5509_sw_reset(struct rt5509_chip *chip)
{
	int ret = 0;
	u8 data = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = rt5509_block_read(chip->i2c, RT5509_REG_SWRESET, 1, &data);
	if (ret < 0)
		return ret;
	data |= RT5509_SWRST_MASK;
	ret = rt5509_block_write(chip->i2c, RT5509_REG_SWRESET, 1, &data);
	mdelay(30);
	return ret;
}

static inline int _rt5509_power_on(struct rt5509_chip *chip, bool en)
{
	int ret = 0;
	u8 data = 0;

	dev_dbg(chip->dev, "%s: en %d\n", __func__, en);
	ret = rt5509_block_read(chip->i2c, RT5509_REG_CHIPEN, 1, &data);
	if (ret < 0)
		return ret;
	data = (en ? (data & ~0x01) : (data | 0x01));
	return rt5509_block_write(chip->i2c, RT5509_REG_CHIPEN, 1, &data);
}

#ifdef CONFIG_OF
static inline int rt5509_parse_dt(struct device *dev,
				  struct rt5509_pdata *pdata)
{
	struct device_node *param_np;
	struct property *prop;
	struct rt5509_proprietary_param *p_param;
	u32 len = 0;
	int i = 0;

	param_np = of_find_node_by_name(dev->of_node, "proprietary_param");
	if (!param_np)
		goto OUT_PARSE_DT;
	p_param = devm_kzalloc(dev, sizeof(*p_param), GFP_KERNEL);
	if (!p_param)
		return -ENOMEM;
	for (i = 0; i < RT5509_CFG_MAX; i++) {
		prop = of_find_property(param_np, prop_str[i], &len);
		if (!prop)
			dev_warn(dev, "no %s setting\n", prop_str[i]);
		else if (!len)
			dev_warn(dev, "%s cfg size is zero\n", prop_str[i]);
		else {
			p_param->cfg[i] = devm_kzalloc(dev, len * sizeof(u8),
						     GFP_KERNEL);
			if (!p_param->cfg[i]) {
				dev_err(dev, "alloc %s fail\n", prop_str[i]);
				return -ENOMEM;
			}
			memcpy(p_param->cfg[i], prop->value, len);
			p_param->cfg_size[i] = len;
		}
	}
	pdata->p_param = p_param;
OUT_PARSE_DT:
	return 0;
}
#else
static inline int rt5509_parse_dt(struct device *dev,
				  struct rt5509_pdata *pdata)
{
	return 0;
}
#endif /* #ifdef CONFIG_OF */

static int rt5509_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct rt5509_pdata *pdata = client->dev.platform_data;
	struct rt5509_chip *chip;
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = rt5509_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
		client->dev.platform_data = pdata;
	} else {
		if (!pdata) {
			dev_err(&client->dev, "Failed, no pdata specified\n");
			return -EINVAL;
		}
	}
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Failed, on memory allocation\n");
		goto err_parse_dt;
	}
	chip->i2c = client;
	chip->dev = &client->dev;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
#if RT5509_SIMULATE_DEVICE
	ret = rt5509_calculate_total_size();
	chip->sim = devm_kzalloc(&client->dev, ret, GFP_KERNEL);
	if (!chip->sim) {
		ret = -ENOMEM;
		goto err_simulate;
	}
#endif /* #if RT5509_SIMULATE_DEVICE */

	sema_init(&chip->io_semaphore, 1);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_set_active(chip->dev);
	pm_runtime_enable(chip->dev);
#else
	atomic_set(&chip->power_count, 1);
#endif /* #ifdef CONFIG_PM_RUNTIME */
	/* before sw_reset, set CHIP_PD = 0 */
	ret = _rt5509_power_on(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "power on fail 1\n");
		goto err_sw_reset;
	}
	/* do software reset at default */
	ret = rt5509_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "sw_reset fail\n");
		goto err_sw_reset;
	}
	ret = _rt5509_power_on(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "power on fail 2\n");
		goto err_pm_init;
	}
	/* get chip revisioin first */
	ret = rt5509_get_chip_rev(chip);
	if (ret < 0) {
		dev_err(chip->dev, "get chip rev fail\n");
		goto err_sw_reset;
	}
	/* register RegMAP */
	chip->rd = rt5509_regmap_register(
		&rt5509_regmap_ops, &client->dev, (void *)client, chip);
	if (!chip->rd) {
		dev_err(chip->dev, "create regmap device fail\n");
		ret = -EINVAL;
		goto err_regmap;
	}
	ret = rt5509_i2c_initreg(chip);
	if (ret < 0) {
		dev_err(chip->dev, "init_reg fail\n");
		goto err_initreg;
	}
	ret = rt5509_handle_pdata(chip);
	if (ret < 0) {
		dev_err(chip->dev, "init_pdata fail\n");
		goto err_pdata;
	}
	ret = rt5509_power_on(chip, false);
	if (ret < 0) {
		dev_err(chip->dev, "power off fail\n");
		goto err_put_sync;
	}
	dev_set_name(chip->dev, "%s", "RT5509_MT");
	ret = rt5509_codec_register(chip);
	if (ret < 0) {
		dev_err(chip->dev, "codec register fail\n");
		goto err_put_sync;
	}
	dev_dbg(&client->dev, "successfully driver probed\n");
	return 0;
err_put_sync:
err_pdata:
err_initreg:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rd);
#endif /* #ifdef CONFIG_RT_REGMAP */
err_regmap:
err_pm_init:
	_rt5509_power_on(chip, false);
err_sw_reset:
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(chip->dev);
	pm_runtime_set_suspended(chip->dev);
#else
	atomic_dec(&chip->power_count);
#endif /* #ifdef CONFIG_PM_RUNTIME */
#if RT5509_SIMULATE_DEVICE
	devm_kfree(chip->dev, chip->sim);
err_simulate:
#endif /* #if RT5509_SIMULATE_DEVICE */
	devm_kfree(&client->dev, chip);
err_parse_dt:
	if (client->dev.of_node)
		devm_kfree(&client->dev, pdata);
	return ret;
}

static int rt5509_i2c_remove(struct i2c_client *client)
{
	struct rt5509_chip *chip = i2c_get_clientdata(client);

	rt5509_codec_unregister(chip);
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rd);
#endif /* #ifdef CONFIG_RT_REGMAP */
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(chip->dev);
	pm_runtime_set_suspended(chip->dev);
#else
	atomic_set(&chip->power_count, 0);
#endif /* #ifdef CONFIG_PM_RUNTIME */
	_rt5509_power_on(chip, false);
#if RT5509_SIMULATE_DEVICE
	devm_kfree(chip->dev, chip->sim);
#endif /* #if RT5509_SIMULATE_DEVICE */
	devm_kfree(chip->dev, chip->pdata);
	chip->pdata = client->dev.platform_data = NULL;
	dev_dbg(&client->dev, "driver removed\n");
	return 0;
}

#ifdef CONFIG_PM
static int rt5509_i2c_suspend(struct device *dev)
{
	return 0;
}

static int rt5509_i2c_resume(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int rt5509_i2c_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int rt5509_i2c_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int rt5509_i2c_runtime_idle(struct device *dev)
{
	/* dummy function */
	return 0;
}
#endif /* #ifdef CONFIG_PM_RUNTIME */

static const struct dev_pm_ops rt5509_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rt5509_i2c_suspend, rt5509_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(rt5509_i2c_runtime_suspend,
			   rt5509_i2c_runtime_resume,
			   rt5509_i2c_runtime_idle)
#endif /* #ifdef CONFIG_PM_RUNTIME */
};
#define prt5509_i2c_pm_ops (&rt5509_i2c_pm_ops)
#else
#define prt5509_i2c_pm_ops (NULL)
#endif /* #ifdef CONFIG_PM */

static const struct i2c_device_id rt5509_i2c_id[] = {
	{ "speaker_amp", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt5509_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id rt5509_match_table[] = {
	{.compatible = "mediatek,speaker_amp",},
	{},
};
MODULE_DEVICE_TABLE(of, rt5509_match_table);
#endif /* #ifdef CONFIG_OF */

static struct i2c_driver rt5509_i2c_driver = {
	.driver = {
		.name = RT5509_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt5509_match_table),
		.pm = prt5509_i2c_pm_ops,
	},
	.probe = rt5509_i2c_probe,
	.remove = rt5509_i2c_remove,
	.id_table = rt5509_i2c_id,
};

module_i2c_driver(rt5509_i2c_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT5509 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT5509_DRV_VER);
