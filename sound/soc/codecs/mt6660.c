// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include "mt6660.h"

union mt6660_multi_byte_data {
	u32 data_u32;
	u16 data_u16;
	u8 data_u8;
	u8 data[4];
};

struct codec_reg_val {
	u32 addr;
	u32 mask;
	u32 data;
};

struct reg_size_table {
	u32 addr;
	u8 size;
};

static const struct reg_size_table mt6660_reg_size_table[] = {
	{ MT6660_REG_HPF1_COEF, 4 },
	{ MT6660_REG_HPF2_COEF, 4 },
	{ MT6660_REG_TDM_CFG3, 2 },
	{ MT6660_REG_RESV17, 2 },
	{ MT6660_REG_RESV23, 2 },
	{ MT6660_REG_SIGMAX, 2 },
	{ MT6660_REG_DEVID, 2},
	{ MT6660_REG_TDM_CFG3, 2},
	{ MT6660_REG_HCLIP_CTRL, 2},
	{ MT6660_REG_DA_GAIN, 2},
};

static int mt6660_get_reg_size(uint32_t addr)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt6660_reg_size_table); i++) {
		if (mt6660_reg_size_table[i].addr == addr)
			return mt6660_reg_size_table[i].size;
	}
	return 1;
}

static int32_t mt6660_i2c_update_bits(struct mt6660_chip *chip,
	uint32_t addr, uint32_t mask, uint32_t data)
{
	int ret = 0;
	uint32_t value;
	int size = mt6660_get_reg_size(addr);
	union mt6660_multi_byte_data mdata;

	memcpy(&mdata, &data, sizeof(uint32_t));

	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_i2c_block_data(
		chip->i2c, addr, size, (u8 *)&value);
	if (ret < 0) {
		mutex_unlock(&chip->io_lock);
		return ret;
	}
	switch (size) {
	case 1:
		value &= ~mask;
		value |= (mdata.data_u8 & mask);
		break;
	case 2:
		value = be16_to_cpu(value);
		value &= ~mask;
		value |= (mdata.data_u16 & mask);
		value = be16_to_cpu(value);
		break;
	case 4:
		value = be32_to_cpu(value);
		value &= ~mask;
		value |= (mdata.data_u32 & mask);
		value = be32_to_cpu(value);
		break;
	default:
		dev_err(chip->dev, "%s Invalid bytes\n", __func__);
		break;
	}

	ret = i2c_smbus_write_i2c_block_data(
		chip->i2c, addr, size, (u8 *)&value);
	if (ret < 0) {
		mutex_unlock(&chip->io_lock);
		return ret;
	}
	mutex_unlock(&chip->io_lock);

	return 0;
}

static int mt6660_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	struct mt6660_chip *chip = (struct mt6660_chip *)drvdata;

	return i2c_smbus_read_i2c_block_data(chip->i2c, reg, size, val);
}

static int mt6660_dbg_io_write(void *drvdata, u16 reg,
			       const void *val, u16 size)
{
	struct mt6660_chip *chip = (struct mt6660_chip *)drvdata;

	return i2c_smbus_write_i2c_block_data(chip->i2c, reg, size, val);
}

static int mt6660_i2c_read(struct mt6660_chip *chip, unsigned int reg)
{
	int size = mt6660_get_reg_size(reg);
	int i = 0, ret = 0;
	u8 data[4] = {0};
	u32 reg_data = 0;

	ret = i2c_smbus_read_i2c_block_data(chip->i2c, reg, size, data);
	if (ret < 0)
		return ret;
	for (i = 0; i < size; i++) {
		reg_data <<= 8;
		reg_data |= data[i];
	}
	return reg_data;
}

static unsigned int mt6660_component_io_read(
	struct snd_soc_component *component, unsigned int reg)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);

	return mt6660_i2c_read(chip, reg);
}

static int mt6660_component_io_write(struct snd_soc_component *component,
	unsigned int reg, unsigned int data)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);
	int size = mt6660_get_reg_size(reg);
	u8 reg_data[4] = {0};
	int i = 0;

	for (i = 0; i < size; i++)
		reg_data[size - i - 1] = (data >> (8 * i)) & 0xff;

	return i2c_smbus_write_i2c_block_data(chip->i2c, reg, size, reg_data);
}

static const int mt6660_dump_table[] = {
	MT6660_REG_DEVID,
	MT6660_REG_SYSTEM_CTRL,
	MT6660_REG_IRQ_STATUS1,
	MT6660_REG_SERIAL_CFG1,
	MT6660_REG_DATAO_SEL,
	MT6660_REG_TDM_CFG3,
	MT6660_REG_HPF_CTRL,
	MT6660_REG_HPF1_COEF,
	MT6660_REG_HPF2_COEF,
	MT6660_REG_PATH_BYPASS,
	MT6660_REG_WDT_CTRL,
	MT6660_REG_HCLIP_CTRL,
	MT6660_REG_VOL_CTRL,
	MT6660_REG_SPS_CTRL,
	MT6660_REG_SIGMAX,
	MT6660_REG_CALI_T0,
	MT6660_REG_BST_CTRL,
	MT6660_REG_PROTECTION_CFG,
	MT6660_REG_DA_GAIN,
	MT6660_REG_AUDIO_IN2_SEL,
	MT6660_REG_SIG_GAIN,
	MT6660_REG_PLL_CFG1,
	MT6660_REG_DRE_CTRL,
	MT6660_REG_DRE_THDMODE,
	MT6660_REG_DRE_CORASE,
	MT6660_REG_PWM_CTRL,
	MT6660_REG_DC_PROTECT_CTRL,
	MT6660_REG_ADC_USB_MODE,
	MT6660_REG_INTERNAL_CFG,
	MT6660_REG_RESV0,
	MT6660_REG_RESV1,
	MT6660_REG_RESV2,
	MT6660_REG_RESV3,
	MT6660_REG_RESV7,
	MT6660_REG_RESV10,
	MT6660_REG_RESV11,
	MT6660_REG_RESV16,
	MT6660_REG_RESV17,
	MT6660_REG_RESV19,
	MT6660_REG_RESV21,
	MT6660_REG_RESV23,
	MT6660_REG_RESV31,
	MT6660_REG_RESV40,
};

#ifdef CONFIG_DEBUG_FS
/* reg/size/data/bustype */
#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(1000)

static int data_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
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
	void *buffer;
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
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

static int dump_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;
	struct mt6660_chip *chip =
		container_of(di, struct mt6660_chip, dbg_info);
	int i = 0, ret = 0;

	if (!chip) {
		pr_err("%s chip is null\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(mt6660_dump_table); i++) {
		ret = mt6660_i2c_read(chip, mt6660_dump_table[i]);
		seq_printf(s,
			"reg 0x%02x : 0x%x\n", mt6660_dump_table[i], ret);
	}
	return 0;
}

static int dump_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_debug_show, inode->i_private);
}

static const struct file_operations dump_debug_fops = {
	.open = dump_debug_open,
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
	if (!debugfs_create_file("dumps", 0444,
				d->ic_root, di, &dump_debug_fops))
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

static const struct codec_reg_val e4_reg_inits[] = {
	{ MT6660_REG_WDT_CTRL, 0x80, 0x00 },
	{ MT6660_REG_SPS_CTRL, 0x01, 0x01 },
	{ MT6660_REG_AUDIO_IN2_SEL, 0x1c, 0x04 },
	{ MT6660_REG_RESV11, 0x0c, 0x00 },
	{ MT6660_REG_RESV31, 0x03, 0x03 },
	{ MT6660_REG_RESV40, 0x01, 0x00 },
	{ MT6660_REG_RESV0, 0x44, 0x04 },
	{ MT6660_REG_RESV19, 0xff, 0x82 },
	{ MT6660_REG_RESV17, 0x7777, 0x7273 },
	{ MT6660_REG_RESV16, 0x07, 0x03 },
	{ MT6660_REG_DRE_CORASE, 0xe0, 0x20 },
	{ MT6660_REG_ADDA_CLOCK, 0xff, 0x70 },
	{ MT6660_REG_RESV21, 0xff, 0x20 },
	{ MT6660_REG_DRE_THDMODE, 0xff, 0x40 },
	{ MT6660_REG_RESV23, 0xffff, 0x17f8 },
	{ MT6660_REG_PWM_CTRL, 0xff, 0x15 },
	{ MT6660_REG_ADC_USB_MODE, 0xff, 0x00 },
	{ MT6660_REG_PROTECTION_CFG, 0xff, 0x1d },
	{ MT6660_REG_HPF1_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_HPF2_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_SIG_GAIN, 0xff, 0x58 },
	{ MT6660_REG_RESV6, 0xff, 0xce },
	{ MT6660_REG_SIGMAX, 0xffff, 0x7fff },
	{ MT6660_REG_DA_GAIN, 0xffff, 0x0116 },
	{ MT6660_REG_TDM_CFG3, 0x1800, 0x0800 },
	{ MT6660_REG_DRE_CTRL, 0x1f, 0x07 },
};

static int mt6660_i2c_init_setting(struct mt6660_chip *chip)
{
	int i, len, ret;
	const struct codec_reg_val *init_table;

	init_table = e4_reg_inits;
	len = ARRAY_SIZE(e4_reg_inits);

	for (i = 0; i < len; i++) {
		ret = mt6660_i2c_update_bits(chip, init_table[i].addr,
				init_table[i].mask, init_table[i].data);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int mt6660_chip_power_on(
	struct snd_soc_component *component, int on_off)
{
	struct mt6660_chip *chip = (struct mt6660_chip *)
		snd_soc_component_get_drvdata(component);
	int ret = 0;
	unsigned int val;

	dev_dbg(component->dev, "%s: on_off = %d\n", __func__, on_off);
	mutex_lock(&chip->var_lock);
	if (on_off) {
		if (chip->pwr_cnt == 0) {
			ret = mt6660_i2c_update_bits(chip,
				MT6660_REG_SYSTEM_CTRL, 0x01, 0x00);
			val = mt6660_i2c_read(chip, MT6660_REG_IRQ_STATUS1);
			dev_info(chip->dev,
				"%s reg0x05 = 0x%x\n", __func__, val);
		}
		chip->pwr_cnt++;
	} else {
		chip->pwr_cnt--;
		if (chip->pwr_cnt == 0) {
			ret = mt6660_i2c_update_bits(chip,
				MT6660_REG_SYSTEM_CTRL, 0x01, 0xff);
		}
		if (chip->pwr_cnt < 0) {
			dev_warn(chip->dev, "not paired on/off\n");
			chip->pwr_cnt = 0;
		}
	}
	mutex_unlock(&chip->var_lock);
	if (ret < 0)
		pr_err("%s ret = %d\n", __func__, ret);
	return ret;
}

static int mt6660_component_set_bias_level(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	int ret;
	unsigned int val;
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);

	if (dapm->bias_level == level) {
		dev_warn(component->dev, "%s: repeat level change\n", __func__);
		return 0;
	}
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;
		dev_dbg(component->dev, "exit low power mode\n");
		ret = mt6660_chip_power_on(component, 1);
		if (ret < 0) {
			dev_err(component->dev, "power on fail\n");
			return ret;
		}
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(component->dev, "enter low power mode\n");
		val = mt6660_i2c_read(chip, MT6660_REG_IRQ_STATUS1);
		dev_info(component->dev,
			"%s reg0x05 = 0x%x\n", __func__, val);
		ret = mt6660_chip_power_on(component, 0);
		if (ret < 0) {
			dev_err(component->dev, "power off fail\n");
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}
	dapm->bias_level = level;
	dev_dbg(component->dev, "c bias_level = %d\n", level);
	return 0;
}

static int mt6660_component_probe(struct snd_soc_component *component)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ret = mt6660_component_set_bias_level(component, SND_SOC_BIAS_STANDBY);
	if (ret < 0) {
		dev_err(component->dev, "config bias standby fail\n");
		return ret;
	}

	ret = mt6660_component_set_bias_level(component, SND_SOC_BIAS_OFF);
	if (ret < 0) {
		dev_err(component->dev, "config bias off fail\n");
		return ret;
	}
	chip->component = component;
	return 0;
}

static void mt6660_component_remove(struct snd_soc_component *component)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s++\n", __func__);
	chip->component = NULL;
	dev_dbg(component->dev, "%s--\n", __func__);
}

static int mt6660_codec_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(1000, 1100);
		break;
	}
	return 0;
}

static int mt6660_codec_classd_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(component->dev,
			"%s: before classd turn on\n", __func__);
		/* config to adaptive mode */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_BST_CTRL, 0x03, 0x03);
		if (ret < 0) {
			dev_err(component->dev, "config mode adaptive fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* voltage sensing enable */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV7, 0x04, 0x04);
		if (ret < 0) {
			dev_err(component->dev,
				"enable voltage sensing fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* voltage sensing disable */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV7, 0x04, 0x00);
		if (ret < 0) {
			dev_err(component->dev,
				"disable voltage sensing fail\n");
			return ret;
		}
		/* pop-noise improvement 1 */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV10, 0x10, 0x10);
		if (ret < 0) {
			dev_err(component->dev,
				"pop-noise improvement 1 fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(component->dev,
			"%s: after classd turn off\n", __func__);
		/* pop-noise improvement 2 */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV10, 0x10, 0x00);
		if (ret < 0) {
			dev_err(component->dev,
				"pop-noise improvement 2 fail\n");
			return ret;
		}
		/* config to off mode */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_BST_CTRL, 0x03, 0x00);
		if (ret < 0) {
			dev_err(component->dev, "config mode off fail\n");
			return ret;
		}
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget mt6660_component_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("DAC", NULL, MT6660_REG_PLL_CFG1,
		0, 1, mt6660_codec_dac_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC("VI ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("ClassD", MT6660_REG_SYSTEM_CTRL, 2, 0,
			       NULL, 0, mt6660_codec_classd_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

static const struct snd_soc_dapm_route mt6660_component_dapm_routes[] = {
	{ "DAC", NULL, "aif_playback"},
	{ "PGA", NULL, "DAC"},
	{ "ClassD", NULL, "PGA"},
	{ "SPK", NULL, "ClassD"},
	{ "VI ADC", NULL, "ClassD"},
	{ "aif_capture", NULL, "VI ADC"},
};

static int mt6660_component_put_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	int ret, put_ret = 0;

	ret = mt6660_chip_power_on(component, 1);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr on fail\n", __func__);
	put_ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;
	ret = mt6660_chip_power_on(component, 0);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr off fail\n", __func__);
	return put_ret;
}

static int mt6660_component_get_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mt6660_chip *chip = (struct mt6660_chip *)
		snd_soc_component_get_drvdata(component);
	int ret = -EINVAL;

	if (!strcmp(kcontrol->id.name, "Chip_Rev")) {
		ucontrol->value.integer.value[0] = chip->chip_rev & 0x0f;
		ret = 0;
	}
	return ret;
}

static const DECLARE_TLV_DB_SCALE(vol_ctl_tlv, -1155, 5, 0);

static const struct snd_kcontrol_new mt6660_component_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Volume_Ctrl", MT6660_REG_VOL_CTRL, 0, 255,
			   1, snd_soc_get_volsw, mt6660_component_put_volsw,
			   vol_ctl_tlv),
	SOC_SINGLE_EXT("WDT_Enable", MT6660_REG_WDT_CTRL, 7, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("Hard_Clip_Enable", MT6660_REG_HCLIP_CTRL, 8, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("Clip_Enable", MT6660_REG_SPS_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("BoostMode", MT6660_REG_BST_CTRL, 0, 3, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("DRE_Enable", MT6660_REG_DRE_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("DC_Protect_Enable",
		MT6660_REG_DC_PROTECT_CTRL, 3, 1, 0,
		snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("I2SLRS", MT6660_REG_DATAO_SEL, 6, 3, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("I2SDOLS", MT6660_REG_DATAO_SEL, 3, 7, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("I2SDORS", MT6660_REG_DATAO_SEL, 0, 7, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	/* for debug purpose */
	SOC_SINGLE_EXT("HPF_AUD_IN_EN", MT6660_REG_HPF_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("AUD_LOOP_BACK", MT6660_REG_PATH_BYPASS, 4, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("Mute_Enable", MT6660_REG_SYSTEM_CTRL, 1, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("CS_Comp_Disable", MT6660_REG_PATH_BYPASS, 2, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("T0_SEL", MT6660_REG_CALI_T0, 0, 7, 0,
		       snd_soc_get_volsw, NULL),
	SOC_SINGLE_EXT("Chip_Rev", SND_SOC_NOPM, 0, 16, 0,
		       mt6660_component_get_volsw, NULL),
};


static const struct snd_soc_component_driver mt6660_component_driver = {
	.probe = mt6660_component_probe,
	.remove = mt6660_component_remove,

	.read = mt6660_component_io_read,
	.write = mt6660_component_io_write,

	.controls = mt6660_component_snd_controls,
	.num_controls = ARRAY_SIZE(mt6660_component_snd_controls),
	.dapm_widgets = mt6660_component_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6660_component_dapm_widgets),
	.dapm_routes = mt6660_component_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6660_component_dapm_routes),

	.set_bias_level = mt6660_component_set_bias_level,
	.idle_bias_on = false, /* idle_bias_off = true */
};

static int mt6660_component_aif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(dai->component);
	int ret = 0;

	dev_dbg(dai->dev, "%s\n", __func__);
	if (dapm->bias_level == SND_SOC_BIAS_OFF)
		ret = mt6660_component_set_bias_level(dai->component,
						  SND_SOC_BIAS_STANDBY);
	return ret;
}

static int mt6660_component_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	int word_len = params_physical_width(hw_params);
	int aud_bit = params_width(hw_params);
	u16 reg_data = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s: ++\n", __func__);
	dev_dbg(dai->dev, "format: 0x%08x\n", params_format(hw_params));
	dev_dbg(dai->dev, "rate: 0x%08x\n", params_rate(hw_params));
	dev_dbg(dai->dev, "word_len: %d, aud_bit: %d\n", word_len, aud_bit);
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
		MT6660_REG_SERIAL_CFG1, 0xc0, (reg_data << 6));
	if (ret < 0) {
		dev_err(dai->dev, "config aud bit fail\n");
		return ret;
	}
	ret = snd_soc_component_update_bits(dai->component,
		MT6660_REG_TDM_CFG3, 0x3f0, word_len << 4);
	if (ret < 0) {
		dev_err(dai->dev, "config word len fail\n");
		return ret;
	}
	dev_dbg(dai->dev, "%s: --\n", __func__);
	return 0;
}

static const struct snd_soc_dai_ops mt6660_component_aif_ops = {
	.startup = mt6660_component_aif_startup,
	.hw_params = mt6660_component_aif_hw_params,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_soc_dai_driver mt6660_codec_dai = {
	.name = "mt6660-aif",
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
	.ops = &mt6660_component_aif_ops,
};

static inline int mt6660_chip_id_check(struct i2c_client *i2c)
{
	u8 id[2] = {0};
	int ret = 0;

	i2c_smbus_write_byte_data(i2c, 0x03, 0x00);
	ret = i2c_smbus_read_i2c_block_data(i2c, MT6660_REG_DEVID, 2, id);
	if (ret < 0)
		return ret;
	ret = (id[0] << 8) + id[1];
	ret &= 0x0ff0;
	if (ret != 0x00e0 && ret != 0x01e0)
		return -ENODEV;
	i2c_smbus_write_byte_data(i2c, 0x03, 0x01);
	return 0;
}

static inline int _mt6660_chip_sw_reset(struct mt6660_chip *chip)
{
	i2c_smbus_write_byte_data(chip->i2c, MT6660_REG_SYSTEM_CTRL, 0x80);
	msleep(30);
	return 0;
}

static inline int _mt6660_chip_power_on(struct mt6660_chip *chip, int on_off)
{
	u8 reg_data = 0;
	int ret = 0;

	ret = i2c_smbus_read_byte_data(chip->i2c, MT6660_REG_SYSTEM_CTRL);
	if (ret < 0)
		return ret;
	reg_data = (u8)ret;
	if (on_off)
		reg_data &= (~0x01);
	else
		reg_data |= 0x01;
	return i2c_smbus_write_byte_data(
		chip->i2c, MT6660_REG_SYSTEM_CTRL, reg_data);
}

static inline int _mt6660_read_chip_revision(struct mt6660_chip *chip)
{
	u8 reg_data[2] = {0};
	int ret = 0;

	ret = i2c_smbus_read_i2c_block_data(
		chip->i2c, MT6660_REG_DEVID, 2, reg_data);
	if (ret < 0) {
		dev_err(chip->dev, "get chip revision fail\n");
		return ret;
	}
	chip->chip_rev = reg_data[1];
	return 0;
}

int mt6660_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mt6660_chip *chip = NULL;
	int ret = 0;

	ret = mt6660_chip_id_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "chip id check fail\n");
		return ret;
	}
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c = client;
	chip->dev = &client->dev;
	mutex_init(&chip->var_lock);
	mutex_init(&chip->io_lock);
	i2c_set_clientdata(client, chip);

	/* debugfs interface */
	chip->dbg_info.dirname = devm_kasprintf(&client->dev,
						GFP_KERNEL, "MT6660.%s",
						dev_name(&client->dev));
	chip->dbg_info.devname = dev_name(&client->dev);
	chip->dbg_info.typestr = devm_kasprintf(&client->dev,
						GFP_KERNEL, "I2C,MT6660");
	chip->dbg_info.io_drvdata = chip;
	chip->dbg_info.io_read = mt6660_dbg_io_read;
	chip->dbg_info.io_write = mt6660_dbg_io_write;

	ret = generic_debugfs_init(&chip->dbg_info);
	if (ret < 0) {
		dev_err(&client->dev, "generic dbg init fail\n");
		return -EINVAL;
	}

	/* chip power on */
	ret = _mt6660_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 1 fail\n");
		goto probe_fail;
	}
	/* chip reset first */
	ret = _mt6660_chip_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "chip reset fail\n");
		goto probe_fail;
	}
	/* chip power on */
	ret = _mt6660_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 2 fail\n");
		goto probe_fail;
	}
	ret = _mt6660_read_chip_revision(chip);
	if (ret < 0) {
		dev_err(chip->dev, "read chip revision fail\n");
		goto probe_fail;
	}
	ret = mt6660_i2c_init_setting(chip);
	if (ret < 0) {
		dev_err(chip->dev, "chip i2c init setting fail\n");
		goto probe_fail;
	}
	ret = _mt6660_chip_power_on(chip, 0);
	if (ret < 0) {
		dev_err(chip->dev, "chip power off fail\n");
		goto probe_fail;
	}
	dev_set_name(chip->dev, "MT6660_MT_0");
	dev_info(chip->dev, "%s--dev name:MT6660_MT_0\n", __func__);
	return snd_soc_register_component(chip->dev,
		&mt6660_component_driver, &mt6660_codec_dai, 1);
probe_fail:
	mutex_destroy(&chip->var_lock);
	return ret;
}
EXPORT_SYMBOL(mt6660_i2c_probe);

int mt6660_i2c_remove(struct i2c_client *client)
{
	struct mt6660_chip *chip = i2c_get_clientdata(client);

	dev_dbg(chip->dev, "%s++\n", __func__);
	snd_soc_unregister_component(chip->dev);
	generic_debugfs_exit(&chip->dbg_info);
	mutex_destroy(&chip->var_lock);
	dev_dbg(chip->dev, "%s--\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mt6660_i2c_remove);

MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("MT6660 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.8_G");

/* 1.0.7_G
 *	add return check for snprintf function
 * 1.0.8_G
 *	fix debug node access deadlock
 */
