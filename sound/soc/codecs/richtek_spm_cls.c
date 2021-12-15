// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Mediatek Inc.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <linux/i2c.h>
#include <sound/soc.h>
#include "../mediatek/common/mtk-sp-spk-amp.h"
#include "richtek_spm_cls.h"

enum {
	RICHTEK_SPM_DEV_TMAX,
	RICHTEK_SPM_DEV_TMAXCNT,
	RICHTEK_SPM_DEV_XMAX,
	RICHTEK_SPM_DEV_XMAXCNT,
	RICHTEK_SPM_DEV_TMAXKEEP,
	RICHTEK_SPM_DEV_XMAXKEEP,
	RICHTEK_SPM_DEV_RSPK,
	RICHTEK_SPM_DEV_CALIBRATED,
	RICHTEK_SPM_DEV_VVALIDATION_REAL_POWER,
	RICHTEK_SPM_DEV_PCBTRACE,
	RICHTEK_SPM_DEV_MAX,
};

#pragma pack(push, 1)
struct richtek_ipi_cmd {
	int32_t data[16];
	int32_t type;
	int32_t cmd;
	int32_t id;
} __aligned(32);
#pragma pack(pop)

enum {
	RTK_IPI_TYPE_NORMAL = 0,
	RTK_IPI_TYPE_CALIBRATION,
	RTK_IPI_TYPE_NR,
};

enum {
	RTK_IPI_CMD_CALIBRATION,
	RTK_IPI_CMD_PCBTRACE,
	RTK_IPI_CMD_VVALIDATION,
	RTK_IPI_CMD_NR,
};

enum {
	RICHTEK_SPM_CLASS_STATUS, /* this is for Samsung Calibration */
	RICHTEK_SPM_CLASS_VVALIDATION,
	RICHTEK_SPM_CLASS_MAX,
};

static struct class *richtek_spm_class;
static int cali_status;

static ssize_t richtek_spm_class_attr_show(struct class *,
					   struct class_attribute *, char *);
static ssize_t richtek_spm_class_attr_store(struct class *,
				struct class_attribute *, const char *, size_t);
static const struct class_attribute richtek_spm_class_attrs[] = {
	__ATTR(status, 0664, richtek_spm_class_attr_show,
	       richtek_spm_class_attr_store),
	__ATTR(v_status, 0664, richtek_spm_class_attr_show,
	       richtek_spm_class_attr_store),
	__ATTR_NULL,
};

static ssize_t richtek_spm_class_attr_show(struct class *cls,
					struct class_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - richtek_spm_class_attrs;
	int ret = 0;

	pr_info("%s: %d\n", __func__, offset);

	switch (offset) {
	case RICHTEK_SPM_CLASS_STATUS:
	case RICHTEK_SPM_CLASS_VVALIDATION:
		ret = scnprintf(buf, PAGE_SIZE,
			"%s\n", cali_status ? "Enabled" : "Disable");
		break;
	}
	return ret;
}

static int rt_spm_trigger_calibration(struct device *dev, void *data)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);
	struct richtek_ipi_cmd ric;
	int ret = 0;
	u32 tmp = *(u32 *)data;
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	uint32_t data_size = 0;
#endif
	int polling_cnt = 6;

	ric.type = RTK_IPI_TYPE_CALIBRATION;
	ric.id = rdc->id;
	if (tmp < 0 || tmp >= RTK_IPI_CMD_NR)
		return -EINVAL;
	rdc->calib_running = cali_status = 1;
	switch (tmp) {
	case 0:
		ric.cmd = RTK_IPI_CMD_PCBTRACE;
		if (rdc->ops && rdc->ops->pre_calib)
			rdc->ops->pre_calib(rdc);
		break;
	case 1:
		ric.cmd = RTK_IPI_CMD_CALIBRATION;
		if (rdc->ops && rdc->ops->pre_calib)
			rdc->ops->pre_calib(rdc);
		break;
	case 2:
		ric.cmd = RTK_IPI_CMD_VVALIDATION;
		if (rdc->ops && rdc->ops->pre_vvalid)
			rdc->ops->pre_vvalid(rdc);
		break;
	default:
		ric.cmd = tmp;
		break;
	}
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	/* enable calibration mode */
	ret = mtk_spk_send_ipi_buf_to_dsp(&ric, sizeof(ric));
#endif
	if (ret < 0) {
		pr_err("%s ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	do {
		msleep(100);
		/* get calibration result */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
		ret = mtk_spk_recv_ipi_buf_from_dsp((int8_t *)&ric,
				sizeof(ric),
				&data_size);
#endif
		if (ret == 0 && ric.data[0] == 0)
			break;
	} while (polling_cnt--);

	if (!polling_cnt || ric.data[0]) {
		switch (ric.cmd) {
		case RTK_IPI_CMD_CALIBRATION:
			dev_err(dev, "calibration failed...\n");
			dev_err(dev, "%s status = %d, calib_dcr = %d\n",
				__func__, ric.data[0], ric.data[1]);
			if (ric.data[0] != 0)
				rdc->rspk = 0;
			if (rdc->ops && rdc->ops->post_calib)
				rdc->ops->post_calib(rdc);
			break;
		case RTK_IPI_CMD_PCBTRACE:
			dev_err(dev, "pcb trace failed...\n");
			break;
		case RTK_IPI_CMD_VVALIDATION:
			dev_err(dev, "vvalidation failed...\n");
			if (rdc->ops->post_vvalid)
				rdc->ops->post_vvalid(rdc);
			break;
		default:
			break;
		}
	} else {
		switch (ric.cmd) {
		case RTK_IPI_CMD_CALIBRATION:
			dev_info(dev, "%s status = %d, calib_dcr = %d\n",
				 __func__, ric.data[0], ric.data[1]);
			rdc->rspk = ric.data[1];
			if (rdc->ops && rdc->ops->post_calib)
				rdc->ops->post_calib(rdc);
			break;
		case RTK_IPI_CMD_PCBTRACE:
			dev_info(dev, "%s status = %d, pcb trace = %d\n",
				 __func__, ric.data[0], ric.data[1]);
			rdc->pcb_trace = ric.data[1];
			if (rdc->ops && rdc->ops->post_calib)
				rdc->ops->post_calib(rdc);
			break;
		case RTK_IPI_CMD_VVALIDATION:
			dev_info(dev, "%s status = %d, pwr = %d, err = %d\n",
				 __func__, ric.data[0], ric.data[1],
				 ric.data[2]);
			if (ric.data[2] == 0)
				rdc->pwr = ric.data[1];
			else
				rdc->pwr = 0;
			if (rdc->ops && rdc->ops->post_vvalid)
				rdc->ops->post_vvalid(rdc);
			break;
		}
	}

	/* change to normal mode */
	ric.type = RTK_IPI_TYPE_NORMAL;
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	ret = mtk_spk_send_ipi_buf_to_dsp(&ric, sizeof(ric));
#endif
	if (ret < 0) {
		dev_err(dev, "%s ret = %d\n", __func__, ret);
		rdc->calib_running = 0;
		cali_status = 0;
		return -EINVAL;
	}
	rdc->calib_running = 0;
	cali_status = 0;

	return 0;
}

static ssize_t richtek_spm_class_attr_store(struct class *cls,
					    struct class_attribute *attr,
					    const char *buf, size_t cnt)
{
	const ptrdiff_t offset = attr - richtek_spm_class_attrs;
	u32 tmp = 0;
	int ret = 0;

	pr_info("%s: %d\n", __func__, offset);
	switch (offset) {
	case RICHTEK_SPM_CLASS_STATUS:
		ret = kstrtou32(buf, 0, &tmp);
		if (ret < 0)
			return ret;
		ret = class_for_each_device(cls, NULL, &tmp,
					    rt_spm_trigger_calibration);
		if (ret < 0)
			return ret;
		break;
	case RICHTEK_SPM_CLASS_VVALIDATION:
		ret = kstrtou32(buf, 0, &tmp);
		if (ret < 0)
			return ret;
		if (tmp == 1)
			tmp = 2;
		else
			break;
		ret = class_for_each_device(cls, NULL, &tmp,
					    rt_spm_trigger_calibration);
		if (ret < 0)
			return ret;
		break;
	default:
		return -EINVAL;
	}
	return cnt;
}

static ssize_t richtek_spm_dev_attr_show(struct device *,
					 struct device_attribute *, char *);
static ssize_t richtek_spm_dev_attr_store(struct device *,
					  struct device_attribute *,
					  const char *, size_t);
static struct device_attribute richtek_spm_dev_attrs[] = {
	__ATTR(tmax, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(tmaxcnt, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(xmax, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(xmaxcnt, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(tmaxkeep, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(xmaxkeep, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(rspk, 0644,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(calibrated, 0644,
	       richtek_spm_dev_attr_show, richtek_spm_dev_attr_store),
	__ATTR(pwr, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR(pcb_trace, 0444,
	       richtek_spm_dev_attr_show, NULL),
	__ATTR_NULL,
};

static struct attribute *richtek_spm_classdev_attrs[] = {
	&richtek_spm_dev_attrs[0].attr,
	&richtek_spm_dev_attrs[1].attr,
	&richtek_spm_dev_attrs[2].attr,
	&richtek_spm_dev_attrs[3].attr,
	&richtek_spm_dev_attrs[4].attr,
	&richtek_spm_dev_attrs[5].attr,
	&richtek_spm_dev_attrs[6].attr,
	&richtek_spm_dev_attrs[7].attr,
	&richtek_spm_dev_attrs[8].attr,
	&richtek_spm_dev_attrs[9].attr,
	NULL,
};

static const struct attribute_group richtek_spm_classdev_group = {
	.attrs = richtek_spm_classdev_attrs,
};

static const struct attribute_group *richtek_spm_classdev_groups[] = {
	&richtek_spm_classdev_group,
	NULL,
};

int rt_spm_monitor_convert(int id, char *buf, int size, s32 val)
{
	int ret;

	if (!buf || size == 0)
		return -EINVAL;
	switch (id) {
	case RICHTEK_SPM_DEV_TMAX:
		/* Fall Through */
	case RICHTEK_SPM_DEV_TMAXKEEP:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", val / 1000);
		break;
	case RICHTEK_SPM_DEV_TMAXCNT:
	case RICHTEK_SPM_DEV_XMAX:
	case RICHTEK_SPM_DEV_XMAXKEEP:
		/* Fall Through */
	case RICHTEK_SPM_DEV_XMAXCNT:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t richtek_spm_dev_attr_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t cnt)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - richtek_spm_dev_attrs;
	u32 tmp = 0;
	int ret = 0;

	switch (offset) {
	case RICHTEK_SPM_DEV_RSPK:
		ret = kstrtou32(buf, 0, &tmp);
		if (ret < 0)
			return ret;
		rdc->rspk = tmp;
		dev_info(dev, "%s: rspk = %d\n", __func__, rdc->rspk);
		break;
	case RICHTEK_SPM_DEV_CALIBRATED:
		ret = kstrtou32(buf, 0, &tmp);
		if (ret < 0)
			return ret;
		if (tmp == 1)
			rdc->calibrated = 1;
		break;
	default:
		break;
	};
	return cnt;
}

static ssize_t richtek_spm_dev_attr_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - richtek_spm_dev_attrs;
	int ret = -EINVAL;

	dev_info(dev, "%s: %d\n", __func__, offset);

	mutex_lock(&rdc->var_lock);
	switch (offset) {
	case RICHTEK_SPM_DEV_RSPK:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", rdc->rspk);
		break;
	case RICHTEK_SPM_DEV_CALIBRATED:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", rdc->calibrated);
		break;
	case RICHTEK_SPM_DEV_PCBTRACE:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", rdc->pcb_trace);
		break;
	case RICHTEK_SPM_DEV_VVALIDATION_REAL_POWER:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			(rdc->pwr >= rdc->min_pwr && rdc->pwr <= rdc->max_pwr) ? 1 : 0);
		dev_info(dev, "%s pwr = %d.%dW\n", __func__, rdc->pwr/1000,
			 rdc->pwr%1000);
		break;
	case RICHTEK_SPM_DEV_TMAX:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
						 rdc->tmax);
		break;
	case RICHTEK_SPM_DEV_TMAXCNT:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
						 rdc->tmaxcnt);
		break;
	case RICHTEK_SPM_DEV_XMAX:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
						 rdc->xmax);
		break;
	case RICHTEK_SPM_DEV_XMAXCNT:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
						 rdc->xmaxcnt);
		break;
	case RICHTEK_SPM_DEV_TMAXKEEP:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
						 rdc->boot_on_tmax);
		break;
	case RICHTEK_SPM_DEV_XMAXKEEP:
		ret = rt_spm_monitor_convert(offset, buf, PAGE_SIZE,
						 rdc->boot_on_xmax);
		break;
	};

	mutex_unlock(&rdc->var_lock);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int richtek_spm_classdev_suspend(struct device *dev)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc->ops && rdc->ops->suspend) ? rdc->ops->suspend(rdc) : 0;
}

static int richtek_spm_classdev_resume(struct device *dev)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc->ops && rdc->ops->resume) ? rdc->ops->resume(rdc) : 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(richtek_spm_class_pm_ops,
			 richtek_spm_classdev_suspend,
			 richtek_spm_classdev_resume);

int richtek_spm_classdev_register(struct device *parent,
				  struct richtek_spm_classdev *rdc)
{
	static u32 spk_index;

	if (rdc == NULL)
		return -EINVAL;
	if (parent == NULL && rdc->name == NULL) {
		pr_err("[richtek_spm] no name can be specified\n");
		return -EINVAL;
	}
	if (rdc->name == NULL) {
		rdc->name = devm_kasprintf(parent,
					   GFP_KERNEL, "rt_amp%d", spk_index);
		if (!rdc->name)
			return -ENOMEM;
	}
	rdc->dev = device_create_with_groups(richtek_spm_class, parent, 0,
					     rdc, rdc->groups, "%s",
					     rdc->name);
	if (IS_ERR(rdc->dev))
		return PTR_ERR(rdc->dev);
	dev_set_drvdata(rdc->dev, rdc);
	/* init */
	mutex_init(&rdc->var_lock);
	rdc->spkidx = spk_index++;
	return 0;
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_register);

void richtek_spm_classdev_unregister(struct richtek_spm_classdev *rdc)
{
	mutex_destroy(&rdc->var_lock);
	device_unregister(rdc->dev);
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_unregister);

static void devm_richtek_spm_classdev_release(struct device *dev, void *res)
{
	richtek_spm_classdev_unregister(
		*(struct richtek_spm_classdev **)res);
}

int devm_richtek_spm_classdev_register(struct device *parent,
				       struct richtek_spm_classdev *rdc)
{
	struct richtek_spm_classdev **prdc;
	int rc;

	prdc = devres_alloc(devm_richtek_spm_classdev_release,
			    sizeof(*prdc), GFP_KERNEL);
	if (!prdc)
		return -ENOMEM;
	rc = richtek_spm_classdev_register(parent, rdc);
	if (rc < 0) {
		devres_free(prdc);
		return rc;
	}
	*prdc = rdc;
	devres_add(parent, prdc);
	return 0;
}
EXPORT_SYMBOL_GPL(devm_richtek_spm_classdev_register);

static int rt_spm_classdev_ampon(struct richtek_spm_classdev *rdc)
{
	return 0;
}

static int rt_spm_classdev_ampoff(struct richtek_spm_classdev *rdc)
{
	int ret;
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	uint32_t data_size = 0;
#endif
	struct richtek_ipi_cmd ric;

	if (!rdc)
		return -EINVAL;

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	ret = mtk_spk_recv_ipi_buf_from_dsp((int8_t *)&ric, sizeof(ric),
					    &data_size);
#endif
	if (ret < 0) {
		pr_err("%s recv big data ipi failed\n", __func__);
		return ret;
	}

	rdc->tmax = ric.data[0];
	rdc->tmaxcnt = ric.data[1];
	rdc->xmax = ric.data[2];
	rdc->xmaxcnt = ric.data[3];

	pr_info("%s richtek tmax = %d, tmaxcnt = %d, xmax = %d, xmaxcnt = %d\n",
		__func__, rdc->tmax, rdc->tmaxcnt, rdc->xmax, rdc->xmaxcnt);

	mutex_lock(&rdc->var_lock);
	rdc->boot_on_xmax = max(rdc->xmax, rdc->boot_on_xmax);
	rdc->boot_on_tmax = max(rdc->tmax, rdc->boot_on_tmax);
	mutex_unlock(&rdc->var_lock);
	return (ret < 0) ? ret : 0;
}

static int rt_spm_cls_trigger_ampon(struct device *dev, void *data)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc == data) ? rt_spm_classdev_ampon(rdc) : 0;
}

static int rt_spm_cls_trigger_ampoff(struct device *dev, void *data)
{
	struct richtek_spm_classdev *rdc = dev_get_drvdata(dev);

	return (rdc == data) ? rt_spm_classdev_ampoff(rdc) : 0;
}

int richtek_spm_classdev_trigger_ampon(struct richtek_spm_classdev *rdc)
{
	return class_for_each_device(richtek_spm_class, NULL,
				     rdc, rt_spm_cls_trigger_ampon);
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_trigger_ampon);

int richtek_spm_classdev_trigger_ampoff(
	struct richtek_spm_classdev *rdc)
{
	return class_for_each_device(richtek_spm_class, NULL,
				     rdc, rt_spm_cls_trigger_ampoff);
}
EXPORT_SYMBOL_GPL(richtek_spm_classdev_trigger_ampoff);

static int __init richtek_spm_init(void)
{
	int i = 0, ret = 0;

	richtek_spm_class = class_create(THIS_MODULE, "richtek_spm");
	if (IS_ERR(richtek_spm_class))
		return PTR_ERR(richtek_spm_class);
	richtek_spm_class->pm = &richtek_spm_class_pm_ops;
	for (i = 0; richtek_spm_class_attrs[i].attr.name; i++) {
		ret = class_create_file(richtek_spm_class,
					richtek_spm_class_attrs + i);
		if (ret < 0)
			goto out_cls_attr;
	}
	richtek_spm_class->dev_groups = richtek_spm_classdev_groups;
	return 0;
out_cls_attr:
	while (--i >= 0) {
		class_remove_file(richtek_spm_class,
				  richtek_spm_class_attrs + i);
	}
	class_destroy(richtek_spm_class);
	return ret;
}
subsys_initcall(richtek_spm_init);

static void __exit richtek_spm_exit(void)
{
	int i = 0;

	for (i = 0; richtek_spm_class_attrs[i].attr.name; i++) {
		class_remove_file(richtek_spm_class,
				  richtek_spm_class_attrs + i);
	}
	class_destroy(richtek_spm_class);
}
module_exit(richtek_spm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Richtek BIGDATA Class driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION("1.0.4_G");

/* 1.0.1_G
 * 1. implement calibration interface like LSI Platform
 * 1.0.2_G
 * 1. modify calibration trigger node path
 * 1.0.3_G
 * 1. modify calibration command and add v_status for vvalidation
 * 1.0.4_G
 * 1. change pwr node show information
 * 2. update check patch
 */
