// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 Richtek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/regmap.h>

#include "rt5512.h"
#include "richtek_spm_cls.h"

#ifndef GENERIC_DEBUGFS
#define GENERIC_DEBUGFS	1
#endif /* if not define GENERIC_DEBUGFS */


#if GENERIC_DEBUGFS
struct dbg_internal {
	struct dentry *rt_root;
	struct dentry *ic_root;
	bool rt_dir_create;
	struct mutex io_lock;
	u16 reg;
	u16 size;
	u16 data_buffer_size;
	void *data_buffer;
	bool access_lock;
};

struct dbg_info {
	const char *dirname;
	const char *devname;
	const char *typestr;
	void *io_drvdata;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	struct dbg_internal internal;
};
#endif /* GENERIC_DEBUGFS */

struct rt5512_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct snd_soc_component *component;
	struct mutex io_lock;
	struct mutex var_lock;
#if GENERIC_DEBUGFS
	struct dbg_info dbg_info;
#endif /* GENERIC_DEBUGFS */
	struct regmap *regmap;
	int t0;
	u8 dev_cnt;
	int pwr_cnt;
	int chip_rev;
	unsigned int ff_gain;
	struct richtek_spm_classdev spm;

};

#define RT5512_REV_A	(2)
#define RT5512_REV_B	(3)

#define RT5512_REG_DEVID		(0x00)
#define RT5512_REG_SERIAL_DATA_STATUS	(0x01)
#define RT5512_REG_SYSTEM_CTRL		(0x03)
#define RT5512_REG_IRQ_EN		(0x04)
#define RT5512_REG_IRQ_STATUS1		(0x05)
#define RT5512_REG_SERIAL_CFG1		(0x10)
#define RT5512_REG_DATAO_SEL		(0x12)
#define RT5512_REG_TDM_CFG3		(0x15)
#define RT5512_REG_HPF_CTRL		(0x16)
#define RT5512_REG_HPF_COEF_1		(0x19)
#define RT5512_REG_HPF_COEF_2		(0x1B)
#define RT5512_REG_PATH_BYPASS		(0x1F)
#define RT5512_REG_WDT_CTRL		(0x20)
#define RT5512_REG_HCLIP_CTRL		(0x24)
#define RT5512_REG_VOL_CTRL		(0x29)
#define RT5512_REG_CLIP_CTRL		(0x30)
#define RT5512_REG_CALI_T0		(0x3F)
#define RT5512_REG_BST_CTRL		(0x40)
#define RT5512_REG_BST_L1		(0x41)
#define RT5512_REG_PROTECTION_CFG	(0x46)
#define RT5512_REG_PLL_CFG1		(0x60)
#define RT5512_REG_DRE_CTRL		(0x68)
#define RT5512_REG_DRE_THDMODE		(0x69)
#define RT5512_REG_FINE_RISING		(0x6C)
#define RT5512_REG_FINE_FALLING		(0x6D)
#define RT5512_REG_PWM_CTRL		(0x70)
#define RT5512_REG_DC_PROTECT_CTRL	(0x74)
#define RT5512_REG_DITHER_CTRL		(0x78)
#define RT5512_REG_MISC_CTRL1		(0x98)
#define RT5512_REG_GVSENSE_CONST	(0x9D)
#define RT5512_REG_SPK_IB		(0xA2)
#define RT5512_REG_ANA_SPK_CTRL6	(0xA6)
#define RT5512_REG_NSW			(0xAD)
#define RT5512_REG_VBG			(0xAE)
#define RT5512_REG_BST_BIAS		(0xB0)
#define RT5512_REG_VSADC_BLKBIAS	(0xB1)
#define RT5512_REG_CS_CTRL		(0xB3)
#define RT5512_REG_ANA_TOP_CTRL0	(0xB5)

#if GENERIC_DEBUGFS
static int rt5512_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	struct rt5512_chip *chip = (struct rt5512_chip *)drvdata;

	return i2c_smbus_read_i2c_block_data(chip->i2c, reg, size, val);
}

static int rt5512_dbg_io_write(void *drvdata, u16 reg,
			       const void *val, u16 size)
{
	struct rt5512_chip *chip = (struct rt5512_chip *)drvdata;

	return i2c_smbus_write_i2c_block_data(chip->i2c, reg, size, val);
}
#endif /* GENERIC_DEBUGFS */

static struct regmap_config rt5512_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0xff,
};

#if GENERIC_DEBUGFS
#ifdef CONFIG_DEBUG_FS
/* reg/size/data/bustype */
#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(1000)

static int data_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;
	struct dbg_internal *d = &di->internal;
	void *buffer = NULL;
	u8 *pdata = NULL;
	int i, ret;

	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}
	/* read transfer */
	if (!di->io_read)
		return -EPERM;
	ret = di->io_read(di->io_drvdata, d->reg, d->data_buffer, d->size);
	if (ret < 0)
		return ret;
	pdata = d->data_buffer;
	seq_puts(s, "0x");
	for (i = 0; i < d->size; i++)
		seq_printf(s, "%02x,", *(pdata + i));
	seq_puts(s, "\n");
	return 0;
}

static int data_debug_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_open(file, data_debug_show, inode->i_private);
	return simple_open(inode, file);
}

static ssize_t data_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	void *buffer = NULL;
	u8 *pdata = NULL;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token = NULL, *cur = NULL;
	int val_cnt = 0, ret;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	if (copy_from_user(buf, user_buf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	/* buffer size check */
	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}
	/* data parsing */
	cur = buf;
	pdata = d->data_buffer;
	while ((token = strsep(&cur, ",\n")) != NULL) {
		if (!*token)
			break;
		if (val_cnt++ >= d->size)
			break;
		if (kstrtou8(token, 16, pdata++))
			return -EINVAL;
	}
	if (val_cnt != d->size)
		return -EINVAL;
	/* write transfer */
	if (!di->io_write)
		return -EPERM;
	ret = di->io_write(di->io_drvdata, d->reg, d->data_buffer, d->size);
	return (ret < 0) ? ret : cnt;
}

static int data_debug_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_release(inode, file);
	return 0;
}

static const struct file_operations data_debug_fops = {
	.open = data_debug_open,
	.read = seq_read,
	.write = data_debug_write,
	.llseek = seq_lseek,
	.release = data_debug_release,
};

static int type_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;

	seq_printf(s, "%s,%s\n", di->typestr, di->devname);
	return 0;
}

static int type_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_debug_show, inode->i_private);
}

static const struct file_operations type_debug_fops = {
	.open = type_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t lock_debug_read(struct file *file,
			       char __user *user_buf, size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	char buf[10];
	bool lock;

	mutex_lock(&d->io_lock);
	lock = d->access_lock;
	mutex_unlock(&d->io_lock);

	snprintf(buf, sizeof(buf), "%d\n", lock);
	return simple_read_from_buffer(user_buf, cnt, loff, buf, strlen(buf));

}

static ssize_t lock_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	u32 lock;
	int ret;

	ret = kstrtou32_from_user(user_buf, cnt, 0, &lock);
	if (ret < 0)
		return ret;
	mutex_lock(&d->io_lock);
	if (!!lock == d->access_lock)
		ret = -EFAULT;
	d->access_lock = !!lock;
	mutex_unlock(&d->io_lock);
	return (ret < 0) ? ret : cnt;
}

static const struct file_operations lock_debug_fops = {
	.open = simple_open,
	.read = lock_debug_read,
	.write = lock_debug_write,
};

static int generic_debugfs_init(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	/* valid check */
	if (!di->dirname || !di->devname || !di->typestr)
		return -EINVAL;
	d->data_buffer_size = PREALLOC_RBUFFER_SIZE;
	d->data_buffer = kzalloc(PREALLOC_RBUFFER_SIZE, GFP_KERNEL);
	if (!d->data_buffer)
		return -ENOMEM;
	/* create debugfs */
	d->rt_root = debugfs_lookup("ext_dev_io", NULL);
	if (!d->rt_root) {
		d->rt_root = debugfs_create_dir("ext_dev_io", NULL);
		if (!d->rt_root)
			return -ENODEV;
		d->rt_dir_create = true;
	}
	mutex_init(&d->io_lock);
	d->ic_root = debugfs_create_dir(di->dirname, d->rt_root);
	if (!d->ic_root)
		goto err_cleanup_rt;
	if (!debugfs_create_u16("reg", 0644, d->ic_root, &d->reg))
		goto err_cleanup_ic;
	if (!debugfs_create_u16("size", 0644, d->ic_root, &d->size))
		goto err_cleanup_ic;
	if (!debugfs_create_file("data", 0644,
				 d->ic_root, di, &data_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("type", 0444,
				 d->ic_root, di, &type_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("lock", 0644,
				 d->ic_root, di, &lock_debug_fops))
		goto err_cleanup_ic;
	return 0;
err_cleanup_ic:
	debugfs_remove_recursive(d->ic_root);
err_cleanup_rt:
	mutex_destroy(&d->io_lock);
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
	return -ENODEV;
}

static void generic_debugfs_exit(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	debugfs_remove_recursive(d->ic_root);
	mutex_destroy(&d->io_lock);
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
}
#else
static inline int generic_debugfs_init(struct dbg_info *di)
{
	return 0;
}

static inline void generic_debugfs_exit(struct dbg_info *di) {}
#endif /* CONFIG_DEBUG_FS */
#endif /* GENERIC_DEBUGFS */

static inline int rt5512_chip_power_on(struct rt5512_chip *chip, int on_off)
{
	int ret = 0;

	dev_info(chip->dev, "%s, %d\n", __func__, on_off);
	mutex_lock(&chip->var_lock);
	if (on_off) {
		if (chip->pwr_cnt++ == 0) {
			ret = regmap_write_bits(chip->regmap,
						RT5512_REG_SYSTEM_CTRL,
						0xffff, 0x0000);
		}
	} else {
		if (--chip->pwr_cnt == 0) {
			ret = regmap_write_bits(chip->regmap,
						RT5512_REG_SYSTEM_CTRL,
						0xffff, 0x0001);
		}
		if (chip->pwr_cnt < 0) {
			dev_warn(chip->dev, "not paired on/off\n");
			chip->pwr_cnt = 0;
		}
	}
	mutex_unlock(&chip->var_lock);
	if (ret)
		return ret;
	return 0;
}

static int rt5512_codec_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* un-mute */
		ret = snd_soc_component_update_bits(component, 0x03, 0x0002, 0x0000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return ret;
}

static int rt5512_codec_classd_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_info(component->dev, "%s, event(%d)\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mdelay(11);
		/*  charge pump disable & disable UVP */
		ret |= snd_soc_component_write(component, 0xb5, 0xf9fc);
		mdelay(2);
		/* boost config to adaptive mode */
		ret |= snd_soc_component_write(component, 0x40, 0x0f5f);

		ret |= snd_soc_component_write(component, 0x98, 0x898c);

		mdelay(2);
		/* charge pump enable */
		ret |= snd_soc_component_write(component, 0xb5, 0xf9fd);
		break;
	case SND_SOC_DAPM_POST_PMU:
		mdelay(15);
		ret |= snd_soc_component_write(component, 0x98, 0x8b8c);
		/* UV enable */
		ret |= snd_soc_component_write(component, 0xb5, 0xfffd);
		dev_info(component->dev, "Amp on\n");
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_info(component->dev, "amp off\n");
		ret = richtek_spm_classdev_trigger_ampoff(&chip->spm);
		if (ret < 0)
			dev_err(component->dev, "spm ampoff faled\n");

		/* enable mute */
		ret = snd_soc_component_update_bits(component, 0x03, 0x0002, 0x0002);
		/* Adaptive Mode */
		ret |= snd_soc_component_write(component, 0x40, 0x0f5f);
		/* Headroom 1.1V */
		ret |= snd_soc_component_write(component, 0x41, 0x002f);
		break;
	case SND_SOC_DAPM_POST_PMD:

		/* un-mute */
		ret |= snd_soc_component_update_bits(component, 0x03, 0x0002,
						     0x0000);
		mdelay(2);

		/* charge pump disable*/
		ret |= snd_soc_component_update_bits(component, 0xb5, 0x0001,
						     0x0000);

		mdelay(1);
		/* set boost to disable mode */
		ret |= snd_soc_component_write(component, 0x40, 0x0f5c);
		/* D_VBG, Bias current disable */
		ret |= snd_soc_component_write(component, 0xb5, 0x00);
		break;
	default:
		break;
	}
	return ret;
}

static const struct snd_soc_dapm_widget rt5512_component_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("DAC", NULL, RT5512_REG_PLL_CFG1,
		0, 1, rt5512_codec_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC("VI ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("ClassD", RT5512_REG_SYSTEM_CTRL, 2, 0,
			       NULL, 0, rt5512_codec_classd_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

static const struct snd_soc_dapm_route rt5512_component_dapm_routes[] = {
	{ "DAC", NULL, "aif_playback" },
	{ "PGA", NULL, "DAC" },
	{ "ClassD", NULL, "PGA" },
	{ "SPK", NULL, "ClassD" },
	{ "VI ADC", NULL, "ClassD" },
	{ "aif_capture", NULL, "VI ADC" },
};

static int rt5512_component_get_t0(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s, chip->t0 = %d\n", __func__, chip->t0);
	ucontrol->value.integer.value[0] = chip->t0;
	return 0;
}

static const DECLARE_TLV_DB_SCALE(vol_ctl_tlv, -1155, 5, 0);

static int rt5512_codec_get_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret;

	ret = rt5512_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(component->dev, "%s power on fail\n", __func__);
		return ret;
	}

	ret = snd_soc_get_volsw(kcontrol, ucontrol);
	if (ret < 0) {
		dev_err(component->dev, "%s get volsw fail\n", __func__);
		return ret;
	}

	ret = rt5512_chip_power_on(chip, 0);
	if (ret < 0) {
		dev_err(component->dev, "%s power off fail\n", __func__);
		return ret;
	}
	return ret;
}

static int rt5512_codec_put_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0, put_ret = 0;

	ret = rt5512_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(component->dev, "%s power on fail\n", __func__);
		return ret;
	}
	put_ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (put_ret < 0) {
		dev_err(component->dev, "%s put volsw fail\n", __func__);
		return put_ret;
	}
	ret = rt5512_chip_power_on(chip, 0);
	if (ret < 0) {
		dev_err(component->dev, "%s power off fail\n", __func__);
		return ret;
	}
	return put_ret;
}

static int rt5512_codec_get_chiprev(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = chip->chip_rev & 0xf;

	return 0;
}

static int rt5512_codec_put_istcbypass(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret;

	ret = rt5512_chip_power_on(chip, 1);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr on fail\n", __func__);

	if (ucontrol->value.integer.value[0]) {
		ret = snd_soc_component_update_bits(component, RT5512_REG_PATH_BYPASS, 0x0004,
					  0x0004);
	} else {
		ret = snd_soc_component_update_bits(component, RT5512_REG_PATH_BYPASS, 0x0004,
					  0x0000);
	}
	if (ret) {
		dev_err(component->dev, "%s set CC Max Failed\n", __func__);
		return ret;
	}

	ret = rt5512_chip_power_on(chip, 0);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr off fail\n", __func__);
	return ret;
}

static int rt5512_codec_get_istcbypass(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret;

	ret = rt5512_chip_power_on(chip, 1);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr on fail\n", __func__);

	ret = snd_soc_component_test_bits(component, RT5512_REG_PATH_BYPASS, 0x0004, 0x0004);
	if (ret) /* 4A */
		ucontrol->value.integer.value[0] = 0;
	else
		ucontrol->value.integer.value[0] = 1;

	ret = rt5512_chip_power_on(chip, 0);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr off fail\n", __func__);

	return 0;
}

static const struct snd_kcontrol_new rt5512_component_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Volume_Ctrl", RT5512_REG_VOL_CTRL, 0, 255,
			   1, rt5512_codec_get_volsw, rt5512_codec_put_volsw,
			   vol_ctl_tlv),
	SOC_SINGLE_EXT("Data Output Left Channel Selection",
		       RT5512_REG_DATAO_SEL, 3, 7, 0,
		       rt5512_codec_get_volsw, rt5512_codec_put_volsw),
	SOC_SINGLE_EXT("Data Output Right Channel Selection",
		       RT5512_REG_DATAO_SEL, 0, 7, 0,
		       rt5512_codec_get_volsw, rt5512_codec_put_volsw),
	SOC_SINGLE_EXT("Audio Input Selection", RT5512_REG_DATAO_SEL,
		       6, 3, 0,
		       rt5512_codec_get_volsw, rt5512_codec_put_volsw),
	SOC_SINGLE_EXT("T0_SEL", SND_SOC_NOPM, 0, 7, 0,
			rt5512_component_get_t0, NULL),
	SOC_SINGLE_EXT("Chip_Rev", SND_SOC_NOPM, 0, 16, 0,
			rt5512_codec_get_chiprev, NULL),
	SOC_SINGLE_BOOL_EXT("IS_TC_BYPASS", SND_SOC_NOPM,
		rt5512_codec_get_istcbypass, rt5512_codec_put_istcbypass),
};

static int rt5512_codec_setting(struct snd_soc_component *component)
{
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	snd_soc_component_read(component, 0x4d, &chip->ff_gain);
	if (chip->chip_rev == RT5512_REV_A) {
		/* RT5512A_RU012B_algorithm_20201110.lua */
		ret |= snd_soc_component_update_bits(component, 0xA1, 0xff18,
						     0x5b18);
		ret |= snd_soc_component_write(component, 0x69, 0x0002);
		ret |= snd_soc_component_write(component, 0x68, 0x000D);
		ret |= snd_soc_component_write(component, 0x6C, 0x0010);
		ret |= snd_soc_component_write(component, 0x6D, 0x0008);
		ret |= snd_soc_component_write(component, 0x30, 0x0002);
		ret |= snd_soc_component_write(component, 0xA7, 0x0A84);
		ret |= snd_soc_component_write(component, 0x20, 0x00A2);
		ret |= snd_soc_component_write(component, 0x8B, 0x0040);

		/* BST optimizations for efficiency */
		ret |= snd_soc_component_write(component, 0xAD, 0x40F7);
		ret |= snd_soc_component_write(component, 0x41, 0x0028);
		ret |= snd_soc_component_write(component, 0x49, 0x0495);
		/* 2019.0821 */
		ret |= snd_soc_component_write(component, 0x46, 0x001D);
		ret |= snd_soc_component_write(component, 0x45, 0x5292);
		ret |= snd_soc_component_write(component, 0x4C, 0x0293);

		ret |= snd_soc_component_write(component, 0xA2, 0x355D);
		ret |= snd_soc_component_update_bits(component, 0xAE, 0x00ff,
						     0x0056);
		ret |= snd_soc_component_write(component, 0xA5, 0x6612);
		ret |= snd_soc_component_write(component, 0x70, 0x0021);
		ret |= snd_soc_component_write(component, 0xA6, 0x3135);

		/* boost THD performance enhance */
		ret |= snd_soc_component_write(component, 0x9B, 0x5f37);
		ret |= snd_soc_component_write(component, 0x4A, 0xD755);

		/* V.I sense performance enhance */
		ret |= snd_soc_component_write(component, 0xB3, 0x9103);
		ret |= snd_soc_component_update_bits(component, 0xB1, 0xfff0,
						     0xA5AA);
		ret |= snd_soc_component_write(component, 0xB0, 0xD5A5);
		ret |= snd_soc_component_write(component, 0x98, 0x8B8C);
		ret |= snd_soc_component_write(component, 0x78, 0x00f2);

		ret |= snd_soc_component_write(component, 0x9D, 0x00FC);
		ret |= snd_soc_component_write(component, 0x38, 0x9BEB);
		ret |= snd_soc_component_write(component, 0x39, 0x8BAC);
		ret |= snd_soc_component_write(component, 0x3A, 0x7E7D);
		ret |= snd_soc_component_write(component, 0x3B, 0x7395);
		ret |= snd_soc_component_write(component, 0x3C, 0x6A68);
		ret |= snd_soc_component_write(component, 0x3D, 0x6295);
		ret |= snd_soc_component_write(component, 0x3E, 0x5BD4);

		ret |= snd_soc_component_update_bits(component, 0x12, 0x0007, 0x0006);
		ret |= snd_soc_component_update_bits(component, 0xB6, 0x0400, 0x0000);
	} else if (chip->chip_rev == RT5512_REV_B) { /* REV_B */
		/* RT5512B_RU012D_algorithm_20201110.lua */
		ret |= snd_soc_component_update_bits(component, 0xA1, 0xff18, 0x5b18);
		ret |= snd_soc_component_write(component, 0x69, 0x0002);
		ret |= snd_soc_component_write(component, 0x68, 0x000D);
		ret |= snd_soc_component_write(component, 0x6C, 0x0010);
		ret |= snd_soc_component_write(component, 0x6D, 0x0008);
		ret |= snd_soc_component_write(component, 0x30, 0x0002);
		ret |= snd_soc_component_write(component, 0xA7, 0x0A84);
		ret |= snd_soc_component_write(component, 0x20, 0x00A2);
		ret |= snd_soc_component_write(component, 0x8B, 0x0040);

		/* BST optimizations for efficiency */
		ret |= snd_soc_component_write(component, 0xAD, 0x40F7);
		ret |= snd_soc_component_write(component, 0x41, 0x0028);
		ret |= snd_soc_component_write(component, 0x49, 0x0495);
		/* 2019.0821 */
		ret |= snd_soc_component_write(component, 0x46, 0x001D);
		ret |= snd_soc_component_write(component, 0x45, 0x5292);
		ret |= snd_soc_component_write(component, 0x4C, 0x0293);

		ret |= snd_soc_component_write(component, 0xA2, 0x355D);
		ret |= snd_soc_component_update_bits(component, 0xAE, 0x00ff, 0x0056);
		ret |= snd_soc_component_write(component, 0xA5, 0x6612);
		ret |= snd_soc_component_write(component, 0x70, 0x0021);
		ret |= snd_soc_component_write(component, 0xA6, 0x3135);

		/* boost THD performance enhance */
		ret |= snd_soc_component_write(component, 0x9B, 0x5f37);
		ret |= snd_soc_component_write(component, 0x4A, 0xD755);

		/* V.I sense performance enhance */
		ret |= snd_soc_component_write(component, 0xB3, 0x9103);
		ret |= snd_soc_component_update_bits(component, 0xB1, 0xfff0, 0xA5AA);
		ret |= snd_soc_component_write(component, 0xB0, 0xD5A5);
		ret |= snd_soc_component_write(component, 0x98, 0x8B8C);
		ret |= snd_soc_component_write(component, 0x78, 0x00f2);

		ret |= snd_soc_component_write(component, 0x9D, 0x00FC);
		ret |= snd_soc_component_write(component, 0x38, 0x9BEB);
		ret |= snd_soc_component_write(component, 0x39, 0x8BAC);
		ret |= snd_soc_component_write(component, 0x3A, 0x7E7D);
		ret |= snd_soc_component_write(component, 0x3B, 0x7395);
		ret |= snd_soc_component_write(component, 0x3C, 0x6A68);
		ret |= snd_soc_component_write(component, 0x3D, 0x6295);
		ret |= snd_soc_component_write(component, 0x3E, 0x5BD4);

		ret |= snd_soc_component_update_bits(component, 0x12, 0x0007, 0x0006);
		ret |= snd_soc_component_update_bits(component, 0xB6, 0x0400, 0x0000);
	}
	pr_info("%s end\n", __func__);

	if (ret < 0)
		return ret;
	mdelay(5);
	return 0;
}

static int rt5512_codec_set_bias_level(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (dapm->bias_level == level) {
		dev_warn(component->dev, "%s: repeat level change\n", __func__);
		goto level_change_skip;
	}
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;
		dev_info(component->dev, "exit low power mode\n");
		ret = rt5512_chip_power_on(chip, 1);
		if (ret < 0)
			dev_err(component->dev, "power on fail\n");
		break;
	case SND_SOC_BIAS_OFF:
		dev_info(component->dev, "enter low power mode\n");
		ret = rt5512_chip_power_on(chip, 0);
		if (ret < 0)
			dev_err(component->dev, "power off fail\n");
		break;
	default:
		return -EINVAL;
	}
	dapm->bias_level = level;
	dev_info(component->dev, "c bias_level = %d\n", level);
level_change_skip:
	return 0;
}

static int rt5512_spm_pre_calib(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret |= rt5512_chip_power_on(chip, 1);
	ret |= snd_soc_component_update_bits(chip->component, RT5512_REG_PATH_BYPASS, 0x0004,
				  0x0004);
	ret |= rt5512_chip_power_on(chip, 0);
	return ret;
}

static int rt5512_spm_post_calib(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret |= rt5512_chip_power_on(chip, 1);
	ret |= snd_soc_component_update_bits(chip->component, RT5512_REG_PATH_BYPASS, 0x0004,
				   0x00);
	ret |= rt5512_chip_power_on(chip, 0);

	return ret;
}

static int rt5512_spm_pre_vvalid(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret |= rt5512_chip_power_on(chip, 1);
	ret |= snd_soc_component_write(chip->component, 0x4d, 0x00);
	ret |= rt5512_chip_power_on(chip, 0);
	mdelay(5);
	return ret;
}

static int rt5512_spm_post_vvalid(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret |= rt5512_chip_power_on(chip, 1);
	ret |= snd_soc_component_write(chip->component, 0x4d, chip->ff_gain);
	ret |= rt5512_chip_power_on(chip, 0);
	return ret;
}

static struct richtek_spm_device_ops rt5512_spm_ops = {
	.pre_calib = rt5512_spm_pre_calib,
	.post_calib = rt5512_spm_post_calib,
	.pre_vvalid = rt5512_spm_pre_vvalid,
	.post_vvalid = rt5512_spm_post_vvalid,
};

static int rt5512_codec_probe(struct snd_soc_component *component)
{
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	pr_info("%s\n", __func__);

	chip->component = component;
	snd_soc_component_init_regmap(component, chip->regmap);
	ret = rt5512_codec_set_bias_level(component, SND_SOC_BIAS_STANDBY);
	if (ret < 0) {
		dev_err(component->dev, "config bias standby fail\n");
		return ret;
	}

	ret = rt5512_codec_setting(component);
	if (ret < 0) {
		dev_err(chip->dev, "rt5512 codec setting failed\n");
		return ret;
	}

	ret = rt5512_codec_set_bias_level(component, SND_SOC_BIAS_OFF);
	if (ret < 0) {
		dev_err(component->dev, "config bias off fail\n");
		return ret;
	}

	chip->spm.max_pwr = 7000;
	chip->spm.min_pwr = 5500;
	chip->spm.ops = &rt5512_spm_ops;
	ret = richtek_spm_classdev_register(component->dev, &chip->spm);
	if (ret < 0) {
		dev_err(component->dev, "spm class register faled\n");
		return ret;
	}
	return ret;
}

static void rt5512_codec_remove(struct snd_soc_component *component)
{
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);

	pr_info("%s\n", __func__);
	richtek_spm_classdev_unregister(&chip->spm);
	snd_soc_component_exit_regmap(component);
}

static const struct snd_soc_component_driver rt5512_codec_driver = {
	.probe = rt5512_codec_probe,
	.remove = rt5512_codec_remove,

	.controls = rt5512_component_snd_controls,
	.num_controls = ARRAY_SIZE(rt5512_component_snd_controls),
	.dapm_widgets = rt5512_component_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5512_component_dapm_widgets),
	.dapm_routes = rt5512_component_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5512_component_dapm_routes),

	.set_bias_level = rt5512_codec_set_bias_level,
	.idle_bias_on = false,
};

static int rt5512_codec_aif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(dai->component);
	int ret = 0;

	dev_info(dai->dev, "%s\n", __func__);
	if (dapm->bias_level == SND_SOC_BIAS_OFF)
		ret = rt5512_codec_set_bias_level(dai->component,
						  SND_SOC_BIAS_STANDBY);
	return ret;
}

static void rt5512_codec_aif_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_info(dai->dev, "%s\n", __func__);
}

static int rt5512_codec_aif_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_info(dai->dev, "%s\n", __func__);
	return 0;
}

static int rt5512_component_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	int word_len = params_physical_width(hw_params);
	int aud_bit = params_width(hw_params);
	u16 reg_data = 0;
	int ret = 0;

	dev_info(dai->dev, "%s: ++\n", __func__);
	dev_info(dai->dev, "format: 0x%08x\n", params_format(hw_params));
	dev_info(dai->dev, "rate: 0x%08x\n", params_rate(hw_params));
	dev_info(dai->dev, "word_len: %d, aud_bit: %d\n", word_len, aud_bit);
	if (word_len > 32 || word_len < 16) {
		dev_err(dai->dev, "not supported word length\n");
		return -ENOTSUPP;
	}
	switch (aud_bit) {
	case 16:
		reg_data = 3;
		break;
	case 18:
		reg_data = 2;
		break;
	case 20:
		reg_data = 1;
		break;
	case 24:
	case 32:
		reg_data = 0;
		break;
	default:
		return -ENOTSUPP;
	}
	ret = snd_soc_component_update_bits(dai->component,
		RT5512_REG_SERIAL_CFG1, 0x00c0, (reg_data << 6));
	if (ret < 0) {
		dev_err(dai->dev, "config aud bit fail\n");
		return ret;
	}

	ret = snd_soc_component_update_bits(dai->component,
		RT5512_REG_TDM_CFG3, 0x03f0, word_len << 4);
	if (ret < 0) {
		dev_err(dai->dev, "config word len fail\n");
		return ret;
	}
	dev_info(dai->dev, "%s: --\n", __func__);
	return 0;
}

static int rt5512_codec_aif_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

	dev_info(dai->dev, "%s: cmd = %d\n", __func__, cmd);
	dev_info(dai->dev, "%s: %c\n", __func__, capture ? 'c' : 'p');
	return 0;
}

static const struct snd_soc_dai_ops rt5512_component_aif_ops = {
	.startup = rt5512_codec_aif_startup,
	.shutdown = rt5512_codec_aif_shutdown,
	.prepare = rt5512_codec_aif_prepare,
	.hw_params = rt5512_component_aif_hw_params,
	.trigger = rt5512_codec_aif_trigger,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_soc_dai_driver rt5512_codec_dai = {
	.name = "rt5512-aif",
	.playback = {
		.stream_name	= "aif_playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture = {
		.stream_name	= "aif_capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates = STUB_RATES,
		.formats = STUB_FORMATS,
	},
	/* dai properties */
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	.symmetric_samplebits = 1,
	/* dai operations */
	.ops = &rt5512_component_aif_ops,
};

static inline int _rt5512_chip_sw_reset(struct rt5512_chip *chip)
{
	int ret;
	u8 data[2] = {0x00, 0x00};
	u8 reg_data[2] = {0x00, 0x80};

	/* turn on main pll first, then trigger reset */
	ret = i2c_smbus_write_i2c_block_data(chip->i2c, RT5512_REG_SYSTEM_CTRL,
					     2, data);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_write_i2c_block_data(chip->i2c, RT5512_REG_SYSTEM_CTRL,
					     2, reg_data);
	if (ret < 0)
		return ret;
	mdelay(14);

	if (chip->chip_rev == RT5512_REV_B) {
		reg_data[0] = 0;
		reg_data[1] = 0;
		ret = i2c_smbus_write_i2c_block_data(chip->i2c, 0x98, 2,
						     reg_data);
		if (ret < 0) {
			dev_err(chip->dev, "%s write 0x98 failed\n", __func__);
			return ret;
		}
		mdelay(10);
		reg_data[0] = 0xE1;
		reg_data[1] = 0xFD;
		ret = i2c_smbus_write_i2c_block_data(chip->i2c, 0xb5, 2,
						     reg_data);
		if (ret < 0) {
			dev_err(chip->dev, "%s write 0xb5 failed\n", __func__);
			return ret;
		}
	}
	return 0;
}

static int rt5512_get_t0(struct rt5512_chip *chip)
{
	int ret;
	unsigned int val;

	/* chip power on */
	ret = rt5512_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 2 fail\n");
		return ret;
	}

	ret = regmap_read(chip->regmap, RT5512_REG_CALI_T0, &val);
	if (ret)
		return ret;

	ret = rt5512_chip_power_on(chip, 0);
	if (ret < 0) {
		dev_err(chip->dev, "chip power off fail\n");
		return ret;
	}

	dev_info(chip->dev, "%s val = %x\n", __func__, val);
	return val&0x00ff;
}

static inline int rt5512_chip_id_check(struct rt5512_chip *chip)
{
	u8 id[2] = {0};
	int ret = 0;
	int chip_rev = 0;
	u8 data[2] = {0x00, 0x00};

	ret = i2c_smbus_write_i2c_block_data(chip->i2c, RT5512_REG_SYSTEM_CTRL,
					     2, data);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_read_i2c_block_data(chip->i2c, RT5512_REG_DEVID, 2, id);
	if (ret < 0)
		return ret;
	chip_rev = id[1];

	data[1] = 0x01;
	ret = i2c_smbus_write_i2c_block_data(chip->i2c, RT5512_REG_SYSTEM_CTRL,
					     2, data);
	if (ret < 0)
		return ret;
	return chip_rev;
}

int rt5512_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct rt5512_chip *chip = NULL;
	static int dev_cnt;
	int ret = 0;
	int chip_rev;

	pr_info("%s start\n", __func__);

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c = client;
	chip->dev = &client->dev;
	chip->dev_cnt = dev_cnt++;
	mutex_init(&chip->var_lock);
	i2c_set_clientdata(client, chip);

	chip_rev = rt5512_chip_id_check(chip);
	if (chip_rev < 0) {
		dev_err(&client->dev, "chip id check fail\n");
		return -ENODEV;
	}
	chip->chip_rev = chip_rev;

	/* chip reset first */
	ret = _rt5512_chip_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "chip reset fail\n");
		goto probe_fail;
	}

	chip->regmap = devm_regmap_init_i2c(client, &rt5512_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "failed to initialise regmap: %d\n", ret);
		return ret;
	}

#if GENERIC_DEBUGFS
	/* debugfs interface */
	chip->dbg_info.dirname = devm_kasprintf(&client->dev,
						GFP_KERNEL, "RT5512.%s",
						dev_name(&client->dev));
	chip->dbg_info.devname = devm_kasprintf(&client->dev,
						GFP_KERNEL, "%s",
						dev_name(&client->dev));
	chip->dbg_info.typestr = devm_kasprintf(&client->dev,
						GFP_KERNEL, "I2C,RT5512");
	chip->dbg_info.io_drvdata = chip;
	chip->dbg_info.io_read = rt5512_dbg_io_read;
	chip->dbg_info.io_write = rt5512_dbg_io_write;

	ret = generic_debugfs_init(&chip->dbg_info);
	if (ret < 0) {
		dev_err(&client->dev, "generic dbg init fail\n");
		return -EINVAL;
	}
#endif /* GENERIC_DEBUGFS */

	/* get t0 information */
	chip->t0 = rt5512_get_t0(chip);
	if (chip->t0 < 0) {
		dev_err(chip->dev, "get t0 fail(%d)\n", chip->t0);
		ret = chip->t0;
		goto probe_fail;
	}


	dev_set_name(chip->dev, "RT5512_MT_%d", chip->dev_cnt);
	ret = snd_soc_register_component(chip->dev, &rt5512_codec_driver,
				     &rt5512_codec_dai, 1);

	pr_info("%s end, ret = %d\n", __func__, ret);


	return ret;
probe_fail:
	mutex_destroy(&chip->var_lock);
	return ret;
}
EXPORT_SYMBOL(rt5512_i2c_probe);

int rt5512_i2c_remove(struct i2c_client *client)
{
	struct rt5512_chip *chip = i2c_get_clientdata(client);

#if GENERIC_DEBUGFS
	generic_debugfs_exit(&chip->dbg_info);
#endif /* GENERIC_DEBUGFS */
	mutex_destroy(&chip->var_lock);
	return 0;
}
EXPORT_SYMBOL(rt5512_i2c_remove);

static int __init rt5512_driver_init(void)
{
	pr_info("%s\n", __func__);
	return 0;
}
module_init(rt5512_driver_init);

static void __exit rt5512_driver_exit(void)
{
	pr_info("%s\n", __func__);
}
module_exit(rt5512_driver_exit);

MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT5512 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.5_M");
/*
 * 1.0.2_M
 *	1. update INIT SETTING & amp on flow
 * 1.0.3_M
 *	1. update amp flow
 * 1.0.4_M
 *	1. fix id check return error code
 * 1.0.5_M
 *	1. fix lock debug io
 */
