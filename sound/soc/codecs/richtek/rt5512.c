// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Mediatek Inc.
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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>

#include "rt5512.h"

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#include "richtek_spm_cls.h"
#endif

#include <mtk-sp-spk-amp.h>

#define GENERIC_DEBUGFS	1

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
#define RT5512_REG_ANA_TOP_CTRL1	(0xB6)

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
	struct mutex var_lock;
#if GENERIC_DEBUGFS
	struct dbg_info dbg_info;
#endif /* GENERIC_DEBUGFS */
	struct regmap *regmap;
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	struct richtek_spm_classdev spm;
#endif
	int t0;
	int pwr_cnt;
	unsigned int ff_gain;
	u16 chip_rev;
	u8 bst_mode;
	u8 dev_cnt;
};

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
	u8 *pdata;
	int i, ret;

	if (d->data_buffer_size < d->size)
		return -EINVAL;
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
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
	int val_cnt = 0, ret;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	if (copy_from_user(buf, user_buf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	/* buffer size check */
	if (d->data_buffer_size < d->size)
		return -EINVAL;
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
	int ret = 0;

	ret = snprintf(buf, sizeof(buf), "%d\n", mutex_is_locked(&d->io_lock));
	if (ret < 0)
		pr_debug("%s, ret = %d\n", __func__, ret);
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
	lock ? mutex_lock(&d->io_lock) : mutex_unlock(&d->io_lock);
	return cnt;
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
	d->ic_root = debugfs_create_dir(di->dirname, d->rt_root);
	if (!d->ic_root)
		goto err_cleanup_rt;
	debugfs_create_u16("reg", 0644, d->ic_root, &d->reg);
	debugfs_create_u16("size", 0644, d->ic_root, &d->size);
	if (!debugfs_create_file("data", 0644,
				 d->ic_root, di, &data_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("type", 0444,
				 d->ic_root, di, &type_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("lock", 0644,
				 d->ic_root, di, &lock_debug_fops))
		goto err_cleanup_ic;
	mutex_init(&d->io_lock);
	return 0;
err_cleanup_ic:
	debugfs_remove_recursive(d->ic_root);
err_cleanup_rt:
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
	return -ENODEV;
}

static void generic_debugfs_exit(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	mutex_destroy(&d->io_lock);
	debugfs_remove_recursive(d->ic_root);
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

static int rt5512_codec_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* un-mute */
		ret = snd_soc_component_update_bits(component, 0x03, 0x0002, 0);
		break;
	case SND_SOC_DAPM_POST_PMU:
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

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*  charge pump disable & disable UVP */
		ret |= snd_soc_component_update_bits(component, 0xb5, 0xf9fc,
						     0xf9fc);
		mdelay(11);

		dev_info(component->dev, "%s rt5512 update bst mode %d\n",
			 __func__, chip->bst_mode);
		/* boost config to adaptive mode */
		ret = snd_soc_component_update_bits(component, 0x40, 0x0003,
						    chip->bst_mode);
		mdelay(2);

		ret |= snd_soc_component_update_bits(component, 0x98, 0x0700,
						     0x0100);
		/* charge pump enable */
		ret |= snd_soc_component_update_bits(component, 0xb5, 0x0001,
						     0x0001);
		break;
	case SND_SOC_DAPM_POST_PMU:
		mdelay(2);
		ret |= snd_soc_component_update_bits(component, 0x98, 0x0200,
						     0x0200);
		/* UV enable */
		/*
		ret |= snd_soc_component_update_bits(component, 0xb5, 0x0600,
						     0x0600);
		*/
		dev_info(component->dev, "Amp on\n");
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_info(component->dev, "Amp off\n");
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
		ret = richtek_spm_classdev_trigger_ampoff(&chip->spm);
		if (ret < 0)
			dev_err(component->dev, "spm ampoff failed\n");
#endif

		/* enable mute */
		ret = snd_soc_component_update_bits(component, 0x03, 0x0002,
						    0x0002);
		/* Headroom 1.1V */
		ret |= snd_soc_component_update_bits(component, 0x41, 0x00ff,
						     0x002f);
		ret |= snd_soc_component_update_bits(component, 0x98, 0x0010,
						     0x0010);
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
		ret |= snd_soc_component_update_bits(component, 0x40, 0x0003,
						     0x0000);
		/* D_VBG, Bias current disable */
		ret |= snd_soc_component_update_bits(component, 0xb5, 0xfffe,
						     0x0000);
		break;
	default:
		break;
	}
	return ret;
}

static const char * const rt5512_i2smux_text[] = { "I2S1", "I2S2"};
static SOC_ENUM_SINGLE_DECL(rt5512_i2s_muxsel,
	SND_SOC_NOPM, 0, rt5512_i2smux_text);
static const struct snd_kcontrol_new rt5512_i2smux_ctrl =
	SOC_DAPM_ENUM("Switch", rt5512_i2s_muxsel);

static const struct snd_soc_dapm_widget rt5512_component_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("I2S Mux", SND_SOC_NOPM, 0, 0, &rt5512_i2smux_ctrl),
	SND_SOC_DAPM_DAC_E("DAC", NULL, RT5512_REG_PLL_CFG1,
		0, 1, rt5512_codec_dac_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC("VI ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("ClassD", RT5512_REG_SYSTEM_CTRL, 2, 0,
			       NULL, 0, rt5512_codec_classd_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

static const struct snd_soc_dapm_route rt5512_component_dapm_routes[] = {
	{ "I2S Mux", "I2S1", "AIF1 Playback" },
	{ "I2S Mux", "I2S2", "AIF2 Playback" },
	{ "DAC", NULL, "I2S Mux" },
	{ "PGA", NULL, "DAC" },
	{ "ClassD", NULL, "PGA" },
	{ "SPK", NULL, "ClassD" },
	{ "VI ADC", NULL, "ClassD" },
	{ "AIF1 Capture", NULL, "VI ADC" },
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

static int rt5512_get_bstmode(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = chip->bst_mode;
	return 0;
}

static int rt5512_set_bstmode(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	chip->bst_mode = ucontrol->value.integer.value[0];
	dev_info(component->dev, "%s, bst_mode = %d\n", __func__,
		 chip->bst_mode);
	return ret;
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
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	int ret;

	pm_runtime_get_sync(component->dev);
	if (ucontrol->value.integer.value[0]) {
		ret = snd_soc_component_update_bits(component,
						    RT5512_REG_PATH_BYPASS,
						    0x0004, 0x0004);
	} else {
		ret = snd_soc_component_update_bits(component,
						    RT5512_REG_PATH_BYPASS,
						    0x0004, 0x0000);
	}
	if (ret)
		dev_err(component->dev, "%s set CC Max Failed\n", __func__);
	pm_runtime_put_sync(component->dev);
	return ret;
}

static int rt5512_codec_get_istcbypass(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	int ret;

	pm_runtime_get_sync(component->dev);
	ret = snd_soc_component_read(component, RT5512_REG_PATH_BYPASS);

	if (ret > 0 && ret&0x0004)
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;
	pm_runtime_put_sync(component->dev);
	return 0;
}

static int rt5512_codec_get_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	int ret;

	pm_runtime_get_sync(component->dev);
	ret = snd_soc_get_volsw(kcontrol, ucontrol);
	if (ret < 0)
		dev_err(component->dev, "%s get volsw fail\n", __func__);
	pm_runtime_put_sync(component->dev);
	return ret;
}

static int rt5512_codec_put_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	int  put_ret = 0;

	pm_runtime_get_sync(component->dev);
	put_ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (put_ret < 0)
		dev_err(component->dev, "%s put volsw fail\n", __func__);
	pm_runtime_put_sync(component->dev);
	return put_ret;
}

static const DECLARE_TLV_DB_SCALE(vol_ctl_tlv, -1155, 5, 0);
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
	SOC_SINGLE_EXT("IDAC Gain Selection", RT5512_REG_ANA_TOP_CTRL1,
		       0, 31, 0,
		       rt5512_codec_get_volsw, rt5512_codec_put_volsw),
	SOC_SINGLE_EXT("VBAT BIT OUT EN", RT5512_REG_TDM_CFG3, 11, 1, 0,
		       rt5512_codec_get_volsw, rt5512_codec_put_volsw),
	SOC_SINGLE_EXT("Boost Mode", SND_SOC_NOPM, 0, 3, 0,
		       rt5512_get_bstmode, rt5512_set_bstmode),
	SOC_SINGLE_EXT("T0_SEL", SND_SOC_NOPM, 0, 7, 0,
			rt5512_component_get_t0, NULL),
	SOC_SINGLE_EXT("Chip_Rev", SND_SOC_NOPM, 0, 16, 0,
			rt5512_codec_get_chiprev, NULL),
	SOC_SINGLE_BOOL_EXT("IS_TC_BYPASS", SND_SOC_NOPM,
		rt5512_codec_get_istcbypass, rt5512_codec_put_istcbypass),
};

static int rt5512_component_setting(struct snd_soc_component *component)
{
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ret = snd_soc_component_read(component, 0x4d);
	if (ret < 0)
		return -EIO;
	chip->ff_gain = ret;
	if (chip->chip_rev == RT5512_REV_A) {
		/* RT5512A_RU012B_algorithm_20201110.lua */
		ret |= snd_soc_component_update_bits(component, 0xA1, 0xff18,
						     0x5b18);
		ret |= snd_soc_component_write(component, 0x69, 0x0002);
		ret |= snd_soc_component_write(component, 0x68, 0x000C);
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
		ret |= snd_soc_component_write(component, 0x68, 0x000C);
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

	dev_info(component->dev, "%s end\n", __func__);
	if (ret < 0)
		return ret;
	mdelay(5);
	return 0;
}

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
static int rt5512_spm_pre_calib(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret = snd_soc_component_update_bits(chip->component,
					    RT5512_REG_PATH_BYPASS, 0x0004,
					    0x0004);
	return ret;
}

static int rt5512_spm_post_calib(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret = snd_soc_component_update_bits(chip->component,
					    RT5512_REG_PATH_BYPASS, 0x0004,
					    0x00);

	return ret;
}

static int rt5512_spm_pre_vvalid(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret = snd_soc_component_write(chip->component, 0x4d, 0x00);
	mdelay(5);
	return ret;
}

static int rt5512_spm_post_vvalid(struct richtek_spm_classdev *ptc)
{
	struct rt5512_chip *chip = container_of(ptc, struct rt5512_chip, spm);
	int ret = 0;

	ret = snd_soc_component_write(chip->component, 0x4d, chip->ff_gain);
	return ret;
}

static struct richtek_spm_device_ops rt5512_spm_ops = {
	.pre_calib = rt5512_spm_pre_calib,
	.post_calib = rt5512_spm_post_calib,
	.pre_vvalid = rt5512_spm_pre_vvalid,
	.post_vvalid = rt5512_spm_post_vvalid,
};
#endif

static int rt5512_component_probe(struct snd_soc_component *component)
{
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	pm_runtime_get_sync(component->dev);

	chip->component = component;
	snd_soc_component_init_regmap(component, chip->regmap);

	ret = rt5512_component_setting(component);
	if (ret < 0) {
		dev_err(chip->dev, "rt5512 component setting failed\n");
		goto component_probe_fail;
	}

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	chip->spm.max_pwr = 7000;
	chip->spm.min_pwr = 5500;
	chip->spm.id = chip->dev_cnt;
	chip->spm.ops = &rt5512_spm_ops;
	ret = richtek_spm_classdev_register(component->dev, &chip->spm);
	if (ret < 0)
		dev_err(component->dev, "spm class register failed\n");
#endif
component_probe_fail:
	pm_runtime_put_sync(component->dev);
	return ret;
}

static void rt5512_component_remove(struct snd_soc_component *component)
{
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	struct rt5512_chip *chip = snd_soc_component_get_drvdata(component);

	richtek_spm_classdev_unregister(&chip->spm);
#endif
	snd_soc_component_exit_regmap(component);
}

static const struct snd_soc_component_driver rt5512_component_driver = {
	.probe = rt5512_component_probe,
	.remove = rt5512_component_remove,

	.controls = rt5512_component_snd_controls,
	.num_controls = ARRAY_SIZE(rt5512_component_snd_controls),
	.dapm_widgets = rt5512_component_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5512_component_dapm_widgets),
	.dapm_routes = rt5512_component_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5512_component_dapm_routes),

	.idle_bias_on = false,
};

static int rt5512_component_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm =
				     snd_soc_component_get_dapm(dai->component);
	int word_len = params_physical_width(hw_params);
	int aud_bit = params_width(hw_params);
	u16 reg_data = 0;
	int ret = 0;
	char *tmp = "SPK";

	dev_dbg(dai->dev, "%s: ++\n", __func__);
	dev_info(dai->dev, "format: 0x%08x, rate: 0x%08x, word_len: %d, aud_bit: %d\n",
		 params_format(hw_params), params_rate(hw_params), word_len,
		 aud_bit);
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
	dev_dbg(dai->dev, "%s: --\n", __func__);
	return snd_soc_dapm_enable_pin(dapm, tmp);
}

static int rt5512_component_aif_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(dai->component);
	int ret = 0;
	char *tmp = "SPK";

	dev_info(dai->dev, "%s\n", __func__);
	ret = snd_soc_dapm_disable_pin(dapm, tmp);
	if (ret < 0)
		return ret;
	return snd_soc_dapm_sync(dapm);
}

static const struct snd_soc_dai_ops rt5512_component_aif_ops = {
	.hw_params = rt5512_component_aif_hw_params,
	.hw_free = rt5512_component_aif_hw_free,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_soc_dai_driver rt5512_codec_dai[] = {
	{
		.name = "rt5512-aif",
		.playback = {
			.stream_name	= "AIF1 Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= STUB_RATES,
			.formats	= STUB_FORMATS,
		},
		.capture = {
			.stream_name	= "AIF1 Capture",
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
	},
	{
		.name = "rt5512-aif2",
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = STUB_RATES,
			.formats = STUB_FORMATS,
		},
		.ops = &rt5512_component_aif_ops,
	},
};

static inline int _rt5512_chip_power_on(struct rt5512_chip *chip, int on_off)
{
	int ret = 0;

	dev_info(chip->dev, "%s, %d\n", __func__, on_off);
	ret =  regmap_write_bits(chip->regmap, RT5512_REG_SYSTEM_CTRL, 0xffff,
				 on_off ? 0x0000 : 0x0001);
	if (ret)
		return ret;
	return 0;
}

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

static inline int rt5512_chip_id_check(struct rt5512_chip *chip)
{
	u8 id[2] = {0};
	int ret = 0;
	u8 data[2] = {0x00, 0x00};

	ret = i2c_smbus_write_i2c_block_data(chip->i2c, RT5512_REG_SYSTEM_CTRL,
					     2, data);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_read_i2c_block_data(chip->i2c, RT5512_REG_DEVID, 2, id);
	if (ret < 0)
		return ret;
	if (id[0] != 0) {
		dev_err(chip->dev, "%s device id not match, id = %x\n",
			__func__, id[0]);
		return -ENODEV;
	}
	chip->chip_rev = id[1];
	if (chip->chip_rev != RT5512_REV_A && chip->chip_rev != RT5512_REV_B) {
		dev_err(chip->dev, "%s chip rev not match, rev = %d\n",
			__func__, chip->chip_rev);
		return -ENODEV;
	}
	dev_info(chip->dev, "%s chip_rev = %d\n", __func__, chip->chip_rev);

	ret = i2c_smbus_read_i2c_block_data(chip->i2c, RT5512_REG_CALI_T0, 2,
					    id);
	if (ret < 0)
		return ret;
	chip->t0 = id[1];
	dev_info(chip->dev, "%s chip t0 = %d\n", __func__, chip->t0);

	data[1] = 0x01;
	ret = i2c_smbus_write_i2c_block_data(chip->i2c, RT5512_REG_SYSTEM_CTRL,
					     2, data);
	return ret;
}

static inline int rt5512_component_register(struct rt5512_chip *chip)
{
	return devm_snd_soc_register_component(chip->dev,
					       &rt5512_component_driver,
					       rt5512_codec_dai,
					       ARRAY_SIZE(rt5512_codec_dai));
}

int rt5512_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct rt5512_chip *chip = NULL;
	int ret = 0;
	static int dev_cnt;

	dev_info(&client->dev, "%s start\n", __func__);
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c = client;
	chip->dev = &client->dev;
	chip->dev_cnt = dev_cnt;
	mutex_init(&chip->var_lock);
	i2c_set_clientdata(client, chip);

	ret = rt5512_chip_id_check(chip);
	if (ret < 0) {
		dev_err(&client->dev, "chip id check fail, ret = %d\n", ret);
		return -ENODEV;
	}

	chip->bst_mode = 1; /* Battery Mode */

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

	pm_runtime_set_active(chip->dev);
	pm_runtime_use_autosuspend(chip->dev);
	pm_runtime_set_autosuspend_delay(chip->dev, 50);
	pm_runtime_enable(chip->dev);

	dev_set_name(chip->dev, "RT5512_MT_%d", chip->dev_cnt);
	ret = rt5512_component_register(chip);
	dev_info(chip->dev, "%s end, ret = %d\n", __func__, ret);
	if (ret == 0) {
		dev_cnt++;
		mtk_spk_set_type(MTK_SPK_MEDIATEK_RT5512);
	}
	return ret;
probe_fail:
	_rt5512_chip_power_on(chip, 0);
	mutex_destroy(&chip->var_lock);
	return ret;
}
EXPORT_SYMBOL(rt5512_i2c_probe);

int rt5512_i2c_remove(struct i2c_client *client)
{
	struct rt5512_chip *chip = i2c_get_clientdata(client);

	pm_runtime_disable(chip->dev);
	pm_runtime_set_suspended(chip->dev);
#if GENERIC_DEBUGFS
	generic_debugfs_exit(&chip->dbg_info);
#endif /* GENERIC_DEBUGFS */
	mutex_destroy(&chip->var_lock);
	return 0;
}
EXPORT_SYMBOL(rt5512_i2c_remove);

static int __maybe_unused rt5512_i2c_runtime_suspend(struct device *dev)
{
	struct rt5512_chip *chip = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(dev, "enter low power mode\n");
	ret = regmap_write_bits(chip->regmap, RT5512_REG_SYSTEM_CTRL,
				0xffff, 0x0001);
	if (ret < 0)
		dev_err(dev, "%s ret = %d\n", __func__, ret);
	return 0;
}

static int __maybe_unused rt5512_i2c_runtime_resume(struct device *dev)
{
	struct rt5512_chip *chip = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(dev, "exit low power mode\n");
	ret = regmap_write_bits(chip->regmap,
		RT5512_REG_SYSTEM_CTRL, 0xffff, 0x0000);
	if (ret < 0)
		dev_err(dev, "%s ret = %d\n", __func__, ret);
	usleep_range(2000, 2100);
	return 0;
}

static const struct dev_pm_ops rt5512_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(rt5512_i2c_runtime_suspend,
			   rt5512_i2c_runtime_resume, NULL)
};

static const struct of_device_id __maybe_unused rt5512_of_id[] = {
	{ .compatible = "richtek,rt5512",},
	{},
};
MODULE_DEVICE_TABLE(of, rt5512_of_id);

static const struct i2c_device_id rt5512_i2c_id[] = {
	{"rt5512", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, rt5512_i2c_id);

static struct i2c_driver rt5512_i2c_driver = {
	.driver = {
		.name = "rt5512",
		.of_match_table = of_match_ptr(rt5512_of_id),
		.pm = &rt5512_dev_pm_ops,
	},
	.probe = rt5512_i2c_probe,
	.remove = rt5512_i2c_remove,
	.id_table = rt5512_i2c_id,
};
module_i2c_driver(rt5512_i2c_driver);

MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT5512 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.5_M");
