// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: CY Huang <cy_huang@richtek.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
/* vfs */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
/* alsa sound header */
#include <sound/soc.h>
/* 64bit integer */
#include <linux/math64.h>

#include "rt5509.h"

#define RT5509_CALIB_MAGIC (5526789)

static struct class *rt5509_cal_class;
static int calib_status;

enum {
	RT5509_CALIB_CTRL_START = 0,
	RT5509_CALIB_CTRL_DCROFFSET,
	RT5509_CALIB_CTRL_N20DB,
	RT5509_CALIB_CTRL_N15DB,
	RT5509_CALIB_CTRL_N10DB,
	RT5509_CALIB_CTRL_READOTP,
	RT5509_CALIB_CTRL_READRAPP,
	RT5509_CALIB_CTRL_WRITEOTP,
	RT5509_CALIB_CTRL_WRITEFILE,
	RT5509_CALIB_CTRL_END,
	RT5509_CALIB_CTRL_ALLINONE,
	RT5509_CALIB_CTRL_MAX,
};

static int rt5509_calib_get_dcroffset(struct rt5509_chip *chip)
{
	struct snd_soc_component *component = chip->component;
	uint32_t delta_v = 0, vtemp = 0;
	int ret = 0;

	dev_info(component->dev, "%s\n", __func__);
	ret = snd_soc_component_read(component, RT5509_REG_VTEMP_TRIM);
	if (ret < 0)
		return ret;
	vtemp = ret & 0xffff;
	ret = snd_soc_component_read(component, RT5509_REG_VTHRMDATA);
	if (ret < 0)
		return ret;
	ret &= 0xffff;
	delta_v = (2730 - 400) * (ret - vtemp) / vtemp;
	return delta_v;
}

static int rt5509_calib_chosen_db(struct rt5509_chip *chip, int choose)
{
	struct snd_soc_component *component = chip->component;
	u32 data = 0;
	uint8_t mode_store;
	int i = 0, ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	ret = snd_soc_component_read(component, RT5509_REG_BST_MODE);
	if (ret < 0)
		return ret;
	mode_store = ret;
	ret = snd_soc_component_update_bits(component, RT5509_REG_BST_MODE,
		0x03, 0x02);
	if (ret < 0)
		return ret;
	data = 0x0080;
	ret = snd_soc_component_write(component, RT5509_REG_CALIB_REQ, data);
	if (ret < 0)
		return ret;
	switch (choose) {
	case RT5509_CALIB_CTRL_N20DB:
		data = 0x0ccc;
		break;
	case RT5509_CALIB_CTRL_N15DB:
		data = 0x16c3;
		break;
	case RT5509_CALIB_CTRL_N10DB:
		data = 0x287a;
		break;
	default:
		return -EINVAL;
	}
	ret = snd_soc_component_write(component, RT5509_REG_CALIB_GAIN, data);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_read(component, RT5509_REG_CALIB_CTRL);
	if (ret < 0)
		return ret;
	data = ret;
	data |= 0x80;
	ret = snd_soc_component_write(component, RT5509_REG_CALIB_CTRL, data);
	if (ret < 0)
		return ret;
	mdelay(120);
	while (i++ < 3) {
		ret = snd_soc_component_read(component, RT5509_REG_CALIB_CTRL);
		if (ret < 0)
			return ret;
		if (ret & 0x01)
			break;
		mdelay(20);
	}
	data &= ~(0x80);
	ret = snd_soc_component_write(component, RT5509_REG_CALIB_CTRL, data);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_update_bits(component, RT5509_REG_BST_MODE,
		0x03, mode_store);
	if (ret < 0)
		return ret;
	if (i > 3) {
		dev_err(chip->dev, "over ready count\n");
		return -EINVAL;
	}
	return snd_soc_component_read(component, RT5509_REG_CALIB_OUT0);
}

static int rt5509_calib_read_otp(struct rt5509_chip *chip)
{
	struct snd_soc_component *component = chip->component;
	int ret = 0;

	ret = snd_soc_component_read(component, RT5509_REG_ISENSEGAIN);
	if (ret < 0)
		return ret;
	ret &= 0xffffff;
	return ret;
}

static int rt5509_calib_write_otp(struct rt5509_chip *chip)
{
	struct snd_soc_component *component = chip->component;
	uint8_t mode_store;
	uint32_t param = chip->calib_dev.rspk;
	uint32_t param_store;
	uint32_t bst_th;
	int ret = 0;

	ret = snd_soc_component_read(component, RT5509_REG_BST_TH1);
	if (ret < 0)
		return ret;
	bst_th = ret;
	ret = snd_soc_component_read(component, RT5509_REG_BST_MODE);
	if (ret < 0)
		return ret;
	mode_store = ret;
	ret = snd_soc_component_write(component, RT5509_REG_BST_TH1, 0x029b);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_update_bits(component, RT5509_REG_BST_MODE,
		0x03, 0x02);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_CALIB_DCR, param);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_read(component, RT5509_REG_OTPDIN);
	ret &= 0x00ffff;
	ret |= 0xc50000;
	ret = snd_soc_component_write(component, RT5509_REG_OTPDIN, ret);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_OTPCONF, 0x81);
	if (ret < 0)
		return ret;
	msleep(100);
	ret = snd_soc_component_write(component, RT5509_REG_OTPCONF, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_update_bits(component, RT5509_REG_BST_MODE,
		0x03, mode_store);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_BST_TH1, bst_th);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_CALIB_DCR, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_OTPDIN, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_OTPCONF, 0x82);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_write(component, RT5509_REG_OTPCONF, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_read(component, RT5509_REG_CALIB_DCR);
	param_store = ret & 0xffffff;
	dev_info(chip->dev, "store %08x, put %08x\n", param_store,
		 param);
	if (param_store != param)
		return -EINVAL;
	ret = snd_soc_component_read(component, RT5509_REG_OTPDIN);
	dev_info(chip->dev, "otp_din = 0x%08x\n", ret);
	if ((ret & 0xff0000) != 0xc50000)
		return -EINVAL;
	chip->calibrated = 1;
	return 0;
}

static int rt5509_calib_rwotp(struct rt5509_chip *chip, int choose)
{
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	switch (choose) {
	case RT5509_CALIB_CTRL_READOTP:
		ret = rt5509_calib_read_otp(chip);
		break;
	case RT5509_CALIB_CTRL_WRITEOTP:
		ret = rt5509_calib_write_otp(chip);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int rt5509_calib_read_rapp(struct rt5509_chip *chip)
{
	struct snd_soc_component *component = chip->component;
	int ret = 0;

	ret = snd_soc_component_read(component, RT5509_REG_RAPP);
	if (ret < 0)
		return ret;
	ret &= 0xffffff;
	return ret;
}

static int rt5509_calib_write_file(struct rt5509_chip *chip)
{
	return 0;
}

static int rt5509_calib_start_process(struct rt5509_chip *chip)
{
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	ret = snd_soc_component_read(chip->component, RT5509_REG_CHIPEN);
	if (ret < 0)
		return ret;
	if (!(ret & RT5509_SPKAMP_ENMASK)) {
		dev_err(chip->dev, "class D not turn on\n");
		return -EINVAL;
	}
	ret = snd_soc_component_read(chip->component,
				       RT5509_REG_I2CBCKLRCKCONF);
	if (ret < 0)
		return ret;
	if (ret & 0x08) {
		dev_err(chip->dev, "BCK loss\n");
		return -EINVAL;
	}
	ret = snd_soc_component_read(chip->component, RT5509_REG_CALIB_REQ);
	if (ret < 0)
		return ret;
	chip->pilot_freq = ret & 0xffff;
	return 0;
}

static int rt5509_calib_end_process(struct rt5509_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return snd_soc_component_write(chip->component, RT5509_REG_CALIB_REQ,
			chip->pilot_freq);
}

static int rt5509_calib_trigger_read(struct rt5509_calib_classdev *cdev)
{
	struct rt5509_chip *chip = dev_get_drvdata(cdev->dev->parent);
	int ret = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = rt5509_calib_start_process(chip);
	if (ret < 0) {
		dev_err(chip->dev, "start fail\n");
		dev_err(chip->dev, "bck not valid or amp not turn on\n");
		goto out_trigger_read;
	}
	ret = rt5509_calib_get_dcroffset(chip);
	if (ret < 0) {
		cdev->dcr_offset = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->dcr_offset = ret;
	dev_dbg(chip->dev, "dcr_offset -> %d\n", cdev->dcr_offset);
	ret = rt5509_calib_chosen_db(chip, RT5509_CALIB_CTRL_N15DB);
	if (ret < 0) {
		cdev->n15db = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->n15db = ret;
	dev_dbg(chip->dev, "n15db -> 0x%08x\n", cdev->n15db);
	ret = rt5509_calib_rwotp(chip, RT5509_CALIB_CTRL_READOTP);
	if (ret < 0) {
		cdev->gsense_otp = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->gsense_otp = ret;
	dev_dbg(chip->dev, "gsense_otp -> 0x%08x\n", cdev->gsense_otp);
	ret = rt5509_calib_read_rapp(chip);
	if (ret < 0) {
		cdev->rapp = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->rapp = ret;
	dev_dbg(chip->dev, "rapp -> 0x%08x\n", cdev->rapp);
	return 0;
out_trigger_read:
	return ret;
}

static int rt5509_calib_trigger_write(struct rt5509_calib_classdev *cdev)
{
	struct rt5509_chip *chip = dev_get_drvdata(cdev->dev->parent);
	int ret = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = rt5509_calib_rwotp(chip, RT5509_CALIB_CTRL_WRITEOTP);
	if (ret < 0)
		goto out_trigger_write;
	ret = rt5509_calib_write_file(chip);
	if (ret < 0)
		goto out_trigger_write;
	ret = rt5509_calib_end_process(chip);
	if (ret < 0)
		goto out_trigger_write;
	return 0;
out_trigger_write:
	return ret;
}

static int64_t rt5509_integer_dcr_calculation(int index, uint32_t n_db)
{
	int64_t a, x;
	int64_t coeffi;
	int i;
	int64_t ret;

	switch (index) {
	case RT5509_CALIB_CTRL_N20DB:
		coeffi = 81051042;
		break;
	case RT5509_CALIB_CTRL_N15DB:
		coeffi = 25630590;
		break;
	case RT5509_CALIB_CTRL_N10DB:
		coeffi = 8105104;
		break;
	default:
		return -1;
	}
	a = n_db * coeffi;
	x = 1 << 24;
	for (i = 0; i < 10; i++)
		x = div_s64(((x * x + a) >> 1), x);
	ret = 1;
	ret <<= 32;
	ret *= 10000000;
	return div_s64(ret, x);
}

#define RefT (-40)
#define alpha_r (265)
static int rt5509_calib_trigger_calculation(struct rt5509_calib_classdev *cdev)
{
	struct rt5509_chip *chip = dev_get_drvdata(cdev->dev->parent);
	int64_t dcr_n15i, dcr_i;
	int64_t alpha_rappi, rappi;
	int64_t rspki;
	int64_t rspk_mini, rspk_maxi;

	dev_info(chip->dev, "dcr_offset = 0x%08x\n", cdev->dcr_offset);
	dev_info(chip->dev, "n15db reg = 0x%08x\n", cdev->n15db);
	dev_info(chip->dev, "gsense_otp reg = 0x%08x\n", cdev->gsense_otp);
	dcr_n15i = rt5509_integer_dcr_calculation(RT5509_CALIB_CTRL_N15DB,
						  cdev->n15db);
	if (dcr_n15i < 0)
		return -EINVAL;
	dcr_i = dcr_n15i;
	alpha_rappi = cdev->alphaspk;
	rappi = cdev->rapp;
	rappi <<= 9;
	dev_info(chip->dev, "rappi = %llx\n", rappi);
	rspki = div_s64((dcr_i * cdev->gsense_otp), 8);
	rspki *= (cdev->alphaspk + 25);
	rspki = div_s64(rspki, (alpha_r + RefT));
	rspki = div_s64(rspki, 1048576);
	dev_info(chip->dev, "pre rspki = %llx\n", rspki);
	rspki -= (div_s64((rappi * (alpha_rappi + 25)), (alpha_rappi + 50)));
	dev_info(chip->dev, "post rspki = %llx\n", rspki);
	rspk_mini = cdev->rspkmin;
	rspk_mini <<= 32;
	rspk_maxi = cdev->rspkmax;
	rspk_maxi <<= 32;
	if ((rspki * 80) < rspk_mini || (rspki * 80) > rspk_maxi) {
		dev_err(chip->dev, "rspki over range\n");
		return -EINVAL;
	}
	rspki >>= 9;
	cdev->rspk = (uint32_t)rspki;
	cdev->rspk &= 0xffffff;
	dev_info(chip->dev, "rspk = 0x%08x\n", cdev->rspk);
	return 0;
}

void rt5509_calib_destroy(struct rt5509_chip *chip)
{
	dev_dbg(chip->dev, "%s\n", __func__);
	device_unregister(chip->calib_dev.dev);
}
EXPORT_SYMBOL_GPL(rt5509_calib_destroy);

int rt5509_calib_create(struct rt5509_chip *chip)
{
	struct rt5509_calib_classdev *pcalib_dev = &chip->calib_dev;
	int ret = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = snd_soc_component_read(chip->component, RT5509_REG_OTPDIN);
	ret &= 0xff0000;
	if (ret == 0xc50000)
		chip->calibrated = 1;
	ret = snd_soc_component_read(chip->component, RT5509_REG_CALIB_DCR);
	ret &= 0xffffff;
	pcalib_dev->rspk = ret;
	/* default rspk min,max,alphspk */
	pcalib_dev->rspkmin = 10;
	pcalib_dev->rspkmax = 160;
	pcalib_dev->alphaspk = 265;
	pcalib_dev->trigger_read = rt5509_calib_trigger_read;
	pcalib_dev->trigger_write = rt5509_calib_trigger_write;
	pcalib_dev->trigger_calculation = rt5509_calib_trigger_calculation;
	pcalib_dev->dev = device_create(rt5509_cal_class, chip->dev, 0,
				pcalib_dev, "rt5509.%d", chip->pdev->id);
	if (IS_ERR(pcalib_dev->dev))
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL_GPL(rt5509_calib_create);

static ssize_t rt_calib_dev_attr_show(struct device *,
		struct device_attribute *, char *);
static ssize_t rt_calib_dev_attr_store(struct device *,
		struct device_attribute *, const char *, size_t);
static struct device_attribute rt5509_dev_attrs[] = {
	__ATTR(n20db, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(n15db, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(n10db, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(gsense_otp, 0444, rt_calib_dev_attr_show,
	       rt_calib_dev_attr_store),
	__ATTR(rapp, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(rspk, 0664, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(calib_data, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(dcr_offset, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(calibrated, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(chip_rev, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(rspkmin, 0644, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(rspkmax, 0644, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(alphaspk, 0644, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(event_read, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR_NULL,
};

static struct attribute *rt5509_cal_dev_attrs[] = {
	&rt5509_dev_attrs[0].attr,
	&rt5509_dev_attrs[1].attr,
	&rt5509_dev_attrs[2].attr,
	&rt5509_dev_attrs[3].attr,
	&rt5509_dev_attrs[4].attr,
	&rt5509_dev_attrs[5].attr,
	&rt5509_dev_attrs[6].attr,
	&rt5509_dev_attrs[7].attr,
	&rt5509_dev_attrs[8].attr,
	&rt5509_dev_attrs[9].attr,
	&rt5509_dev_attrs[10].attr,
	&rt5509_dev_attrs[11].attr,
	&rt5509_dev_attrs[12].attr,
	&rt5509_dev_attrs[13].attr,
	NULL,
};

static const struct attribute_group rt5509_cal_group = {
	.attrs = rt5509_cal_dev_attrs,
};

static const struct attribute_group *rt5509_cal_groups[] = {
	&rt5509_cal_group,
	NULL,
};

enum {
	RT5509_CALIB_DEV_N20DB = 0,
	RT5509_CALIB_DEV_N15DB,
	RT5509_CALIB_DEV_N10DB,
	RT5509_CALIB_DEV_GSENSE_OTP,
	RT5509_CALIB_DEV_RAPP,
	RT5509_CALIB_DEV_RSPK,
	RT5509_CALIB_DEV_CALIB_DATA,
	RT5509_CALIB_DEV_DCROFFSET,
	RT5509_CALIB_DEV_CALIBRATED,
	RT5509_CALIB_DEV_CHIPREV,
	RT5509_CALIB_DEV_RSPKMIN,
	RT5509_CALIB_DEV_RSPKMAX,
	RT5509_CALIB_DEV_ALPHASPK,
	RT5509_CALIB_DEV_EVENT_READ,
	RT5509_CALIB_DEV_MAX,
};

static int calib_data_file_read(struct rt5509_chip *chip, char *buf)
{
	return -EINVAL;
}

static int rt_dev_event_read(struct rt5509_chip *chip, char *buf)
{
	struct snd_soc_component *component = chip->component;
	int i, index = 0, ret = 0;

	ret = snd_soc_component_read(component, RT5509_REG_CHIPEN);
	if (ret < 0)
		return ret;
	if (!(ret & RT5509_SPKAMP_ENMASK)) {
		dev_err(chip->dev, "amp not turn on\n");
		return -EINVAL;
	}
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "important reg dump ++\n");
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x03) -> 0x%02x\n", ret);
	ret = snd_soc_component_read(component, RT5509_REG_CHIPREV);
	if (ret < 0)
		return ret;
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x00) -> 0x%02x\n", ret);
	ret = snd_soc_component_read(component, RT5509_REG_EVENTINFO);
	if (ret < 0)
		return ret;
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x01) -> 0x%02x\n", ret);
	ret = snd_soc_component_read(component, RT5509_REG_DMGFLAG);
	if (ret < 0)
		return ret;
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x02) -> 0x%02x\n", ret);
	ret = snd_soc_component_read(component, RT5509_REG_BST_MODE);
	if (ret < 0)
		return ret;
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x1e) -> 0x%02x\n", ret);
	ret = snd_soc_component_read(component, RT5509_REG_ISENSEGAIN);
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x46) -> 0x%06x\n", ret & 0xffffff);
	ret = snd_soc_component_read(component, RT5509_REG_CALIB_DCR);
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x4e) -> 0x%06x\n", ret & 0xffffff);
	ret = snd_soc_component_read(component, RT5509_REG_VTEMP_TRIM);
	if (ret < 0)
		return ret;
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0xc4) -> 0x%04x\n", ret);
	ret = snd_soc_component_read(component, RT5509_REG_INTERRUPT);
	if (ret < 0)
		return ret;
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "events -> 0x%04x\n", ret);
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "important reg dump --\n");
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "impedance curve ++\n");
	for (i = 0x10; i <= 0x1a; i++) {
		ret = snd_soc_component_write(component,
					      RT5509_REG_SPKRPTSEL, i);
		if (ret < 0)
			return ret;
		ret = snd_soc_component_read(component, RT5509_REG_SPKRPT);
		index += scnprintf(buf + index, PAGE_SIZE - index,
				   "i(0x%02x) -> 0x%06x\n", i, ret & 0xffffff);
	}
	ret = snd_soc_component_write(component, RT5509_REG_SPKRPTSEL, 0x0d);
	if (ret < 0)
		return ret;
	ret = snd_soc_component_read(component, RT5509_REG_SPKRPT);
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "i(0x0d) -> 0x%06x\n", ret & 0xffffff);
	index += scnprintf(buf + index, PAGE_SIZE - index,
			   "impedance curve --\n");
	ret = index;
	return ret;
}

static ssize_t rt_calib_dev_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rt5509_chip *chip = dev_get_drvdata(dev->parent);
	struct rt5509_calib_classdev *calib_dev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt5509_dev_attrs;
	int ret = 0;

	switch (offset) {
	case RT5509_CALIB_DEV_N20DB:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->n20db);
		break;
	case RT5509_CALIB_DEV_N15DB:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->n15db);
		break;
	case RT5509_CALIB_DEV_N10DB:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->n10db);
		break;
	case RT5509_CALIB_DEV_GSENSE_OTP:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n",
				calib_dev->gsense_otp);
		break;
	case RT5509_CALIB_DEV_RAPP:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->rapp);
		break;
	case RT5509_CALIB_DEV_RSPK:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->rspk);
		break;
	case RT5509_CALIB_DEV_CALIB_DATA:
		ret = calib_data_file_read(chip, buf);
		break;
	case RT5509_CALIB_DEV_DCROFFSET:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n",
				calib_dev->dcr_offset);
		break;
	case RT5509_CALIB_DEV_CALIBRATED:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", chip->calibrated);
		break;
	case RT5509_CALIB_DEV_CHIPREV:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", chip->chip_rev);
		break;
	case RT5509_CALIB_DEV_RSPKMIN:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", calib_dev->rspkmin);
		break;
	case RT5509_CALIB_DEV_RSPKMAX:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", calib_dev->rspkmax);
		break;
	case RT5509_CALIB_DEV_ALPHASPK:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", calib_dev->alphaspk);
		break;
	case RT5509_CALIB_DEV_EVENT_READ:
		ret = rt_dev_event_read(chip, buf);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t rt_calib_dev_attr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct rt5509_calib_classdev *calib_dev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt5509_dev_attrs;
	uint32_t tmp = 0;
	int32_t tmp2 = 0;

	switch (offset) {
	case RT5509_CALIB_DEV_RSPK:
		if (sscanf(buf, "0x%08x", &tmp) != 1)
			return -EINVAL;
		calib_dev->rspk = tmp;
		break;
	case RT5509_CALIB_DEV_RSPKMIN:
		if (kstrtoint(buf, 10, &tmp2) < 0)
			return -EINVAL;
		calib_dev->rspkmin = tmp2;
		break;
	case RT5509_CALIB_DEV_RSPKMAX:
		if (kstrtoint(buf, 10, &tmp2) < 0)
			return -EINVAL;
		calib_dev->rspkmax = tmp2;
		break;
	case RT5509_CALIB_DEV_ALPHASPK:
		if (kstrtoint(buf, 10, &tmp2) < 0)
			return -EINVAL;
		calib_dev->alphaspk = tmp2;
		break;
	default:
		return -EINVAL;
	}
	return count;
}

static ssize_t rt_calib_class_attr_show(struct class *,
		struct class_attribute *, char *);
static ssize_t rt_calib_class_attr_store(struct class *,
		struct class_attribute *, const char *, size_t);
static struct class_attribute rt5509_class_attrs[] = {
	__ATTR(trigger, 0220, rt_calib_class_attr_show,
	       rt_calib_class_attr_store),
	__ATTR(status, 0444, rt_calib_class_attr_show,
	       rt_calib_class_attr_store),
	__ATTR_NULL,
};

enum {
	RT5509_CALIB_CLASS_TRIGGER = 0,
	RT5509_CALIB_CLASS_STATUS,
	RT5509_CALIB_CLASS_MAX,
};

static ssize_t rt_calib_class_attr_show(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - rt5509_class_attrs;
	int ret = 0;

	switch (offset) {
	case RT5509_CALIB_CLASS_STATUS:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", calib_status);
		break;
	case RT5509_CALIB_CLASS_TRIGGER:
	default:
		return -EINVAL;
	}
	return ret;
}

static int rt_calib_trigger_read(struct device *dev, void *data)
{
	struct rt5509_calib_classdev *calib_dev = dev_get_drvdata(dev);

	return calib_dev->trigger_read ?
			calib_dev->trigger_read(calib_dev) : -EINVAL;
}

static int rt_calib_trigger_write(struct device *dev, void *data)
{
	struct rt5509_calib_classdev *calib_dev = dev_get_drvdata(dev);

	return calib_dev->trigger_write ?
			calib_dev->trigger_write(calib_dev) : -EINVAL;
}

static int rt_calib_trigger_calculation(struct device *dev, void *data)
{
	struct rt5509_calib_classdev *calib_dev = dev_get_drvdata(dev);

	return calib_dev->trigger_calculation ?
			calib_dev->trigger_calculation(calib_dev) : -EINVAL;
}

static int rt_calib_trigger_sequence(struct class *cls, int seq)
{
	int ret = 0;

	switch (seq) {
	case RT5509_CALIB_CTRL_START:
		ret = class_for_each_device(cls, NULL, NULL,
					    rt_calib_trigger_read);
		break;
	case RT5509_CALIB_CTRL_END:
		ret = class_for_each_device(cls, NULL, NULL,
					    rt_calib_trigger_write);
		break;
	case RT5509_CALIB_CTRL_ALLINONE:
		ret = rt_calib_trigger_sequence(cls, RT5509_CALIB_CTRL_START);
		if (ret < 0) {
			pr_err("%s: trigger read fail\n", cls->name);
			return ret;
		}
		ret = class_for_each_device(cls, NULL, NULL,
					    rt_calib_trigger_calculation);
		if (ret < 0) {
			pr_err("%s: trigger calculation fail\n", cls->name);
			return ret;
		}
		ret = rt_calib_trigger_sequence(cls, RT5509_CALIB_CTRL_END);
		if (ret < 0) {
			pr_err("%s: trigger write fail\n", cls->name);
			return ret;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t rt_calib_class_attr_store(struct class *cls,
		struct class_attribute *attr, const char *buf, size_t cnt)
{
	const ptrdiff_t offset = attr - rt5509_class_attrs;
	int parse_val = 0;
	int ret = 0;

	switch (offset) {
	case RT5509_CALIB_CLASS_TRIGGER:
		if (kstrtoint(buf, 10, &parse_val) < 0)
			return -EINVAL;
		parse_val -= RT5509_CALIB_MAGIC;
		ret = rt_calib_trigger_sequence(cls, parse_val);
		calib_status = ret;
		if (ret < 0)
			return ret;
		break;
	case RT5509_CALIB_CLASS_STATUS:
	default:
		return -EINVAL;
	}
	return cnt;
}

static int __init rt5509_cal_init(void)
{
	int i = 0, ret = 0;

	rt5509_cal_class = class_create(THIS_MODULE, "rt5509_cal");
	if (IS_ERR(rt5509_cal_class))
		return PTR_ERR(rt5509_cal_class);
	for (i = 0; rt5509_class_attrs[i].attr.name; i++) {
		ret = class_create_file(rt5509_cal_class,
					&rt5509_class_attrs[i]);
		if (ret < 0)
			goto out_cal_init;
	}
	rt5509_cal_class->dev_groups = rt5509_cal_groups;
	return 0;
out_cal_init:
	while (--i >= 0)
		class_remove_file(rt5509_cal_class, &rt5509_class_attrs[i]);
	class_destroy(rt5509_cal_class);
	return ret;
}

static void __exit rt5509_cal_exit(void)
{
	int i = 0;

	for (i = 0; rt5509_class_attrs[i].attr.name; i++)
		class_remove_file(rt5509_cal_class, &rt5509_class_attrs[i]);
	class_destroy(rt5509_cal_class);
}

subsys_initcall(rt5509_cal_init);
module_exit(rt5509_cal_exit);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT5509 SPKAMP calibration");
MODULE_LICENSE("GPL");
