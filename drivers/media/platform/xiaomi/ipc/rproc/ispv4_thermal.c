// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 */

// #include "linux/kernel.h"
#define pr_fmt(fmt) "ispv4 thermal" fmt
#include "../rpmsg/ispv4_rpmsg.h"
#include "ispv4_rproc.h"
#include "ispv4_thermal.h"
#define POLLING_SPAN_MS 2000
#define SENSOR_NUM 7
#define THERMAL_CNT 1
#define THERMAL_NAME "thermal_isp"


struct ispv4_thermal {
	int major;
	int minor;
	dev_t devid;
	struct device *dev;
	struct class *class;
	struct attribute_group attrs;
	struct mutex up_lock;
	struct xm_ispv4_rproc *rp;
	struct timer_list timer;
	struct work_struct twork;
};

static atomic_t ispv4_maxtemp = ATOMIC_INIT(0);
static atomic_t ispv4_npucore0 = ATOMIC_INIT(0);
static atomic_t ispv4_npucore1 = ATOMIC_INIT(0);
static atomic_t ispv4_npucore2 = ATOMIC_INIT(0);
static atomic_t ispv4_npucore3 = ATOMIC_INIT(0);
static atomic_t ispv4_mipi = ATOMIC_INIT(0);
static atomic_t ispv4_isp = ATOMIC_INIT(0);
static atomic_t ispv4_ddrphy = ATOMIC_INIT(0);
static atomic_t ispv4_top1 = ATOMIC_INIT(0);
static atomic_t ispv4_top2 = ATOMIC_INIT(0);

static ssize_t ispv4_maxtemp_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_maxtemp));
}

static ssize_t ispv4_maxtemp_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_maxtemp, val);

	return len;
}

static DEVICE_ATTR(ispv4_maxtemp, 0664, ispv4_maxtemp_show,
		   ispv4_maxtemp_store);

// npucore0
static ssize_t ispv4_npucore0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_npucore0));
}

static ssize_t ispv4_npucore0_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_npucore0, val);

	return len;
}

static DEVICE_ATTR(ispv4_npucore0, 0664, ispv4_npucore0_show,
		   ispv4_npucore0_store);

// npucore1
static ssize_t ispv4_npucore1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_npucore1));
}

static ssize_t ispv4_npucore1_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_npucore1, val);

	return len;
}

static DEVICE_ATTR(ispv4_npucore1, 0664, ispv4_npucore1_show,
		   ispv4_npucore1_store);

// npucore2
static ssize_t ispv4_npucore2_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_npucore2));
}

static ssize_t ispv4_npucore2_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_npucore2, val);

	return len;
}

static DEVICE_ATTR(ispv4_npucore2, 0664, ispv4_npucore2_show,
		   ispv4_npucore2_store);

// npucore3
static ssize_t ispv4_npucore3_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_npucore3));
}

static ssize_t ispv4_npucore3_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_npucore3, val);

	return len;
}

static DEVICE_ATTR(ispv4_npucore3, 0664, ispv4_npucore3_show,
		   ispv4_npucore3_store);

// mipi
static ssize_t ispv4_mipi_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_mipi));
}

static ssize_t ispv4_mipi_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_mipi, val);

	return len;
}

static DEVICE_ATTR(ispv4_mipi, 0664, ispv4_mipi_show, ispv4_mipi_store);

// isp
static ssize_t ispv4_isp_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_isp));
}

static ssize_t ispv4_isp_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_isp, val);

	return len;
}

static DEVICE_ATTR(ispv4_isp, 0664, ispv4_isp_show, ispv4_isp_store);

// ddrphy
static ssize_t ispv4_ddrphy_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_ddrphy));
}

static ssize_t ispv4_ddrphy_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_ddrphy, val);

	return len;
}

static DEVICE_ATTR(ispv4_ddrphy, 0664, ispv4_ddrphy_show, ispv4_ddrphy_store);

// top1
static ssize_t ispv4_top1_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_top1));
}

static ssize_t ispv4_top1_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_top1, val);

	return len;
}

static DEVICE_ATTR(ispv4_top1, 0664, ispv4_top1_show, ispv4_top1_store);

// top2
static ssize_t ispv4_top2_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&ispv4_top2));
}

static ssize_t ispv4_top2_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	int val = -1;
	val = simple_strtol(buf, NULL, 10);

	atomic_set(&ispv4_top2, val);

	return len;
}

static DEVICE_ATTR(ispv4_top2, 0664, ispv4_top2_show, ispv4_top2_store);

static struct attribute *isp_thermal_dev_attr_group[] = {
	&dev_attr_ispv4_maxtemp.attr,
	&dev_attr_ispv4_npucore0.attr,
	&dev_attr_ispv4_npucore1.attr,
	&dev_attr_ispv4_npucore2.attr,
	&dev_attr_ispv4_npucore3.attr,
	&dev_attr_ispv4_mipi.attr,
	&dev_attr_ispv4_isp.attr,
	&dev_attr_ispv4_ddrphy.attr,
	&dev_attr_ispv4_top1.attr,
	&dev_attr_ispv4_top2.attr,
	NULL,
};

static void thermal_check_ack_work(struct work_struct *work)
{
	int i;
	int ret;
	uint32_t max_val = 0;
	char temp_buf[8] = { '0' };
	struct ispv4_thermal *tdev =
		container_of(work, struct ispv4_thermal, twork);
	int msg_len = sizeof(struct xm_ispv4_rpmsg_pkg) + 4;
	uint32_t *recv_buf = NULL;
	struct xm_ispv4_rpmsg_pkg *send_buf = kzalloc(msg_len + 50, GFP_KERNEL);
	if (send_buf == NULL)
		return;

	send_buf->func = ICC_REQUEST_THERMAL_VALUE;
	send_buf->data[0] = 1;
	// for (i = 0; i < SENSOR_NUM; i++) {
	// 	dev_info(tdev->dev, "init buffer %d  =  %d", i,
	// 		 *((uint32_t *)send_buf + i));
	// 	max_val = max_t(uint32_t, max_val, *((uint32_t *)send_buf + i));
	// }
	ret = ispv4_eptdev_isp_send(tdev->rp, XM_ISPV4_IPC_EPT_RPMSG_ASST,
			MIPC_MSGHEADER_CMD, msg_len + 50, send_buf, false,
			NULL);
	// ret = 1;
	if(ret) {
		if (ret == -ENOLINK) {
			dev_info(tdev->dev, "%s: worker early down", __func__);
		}
		else if (ret == -ENODEV) {
			dev_info(tdev->dev, "%s: rpmsg early down", __func__);
		}
		else {
			dev_info(tdev->dev, "%s: get thermal time-out", __func__);
		}
		goto err;
	}

	recv_buf = (void *)send_buf;
	// Skip header.
	recv_buf += 3;
	dev_info(tdev->dev, "%s:already receive data\n", __func__);
	for (i = 1; i <= SENSOR_NUM; i++) {
		// max_val =
		// 	max_t(long, max_val, simple_strtol(send_buf->data[i] + i, NULL, 8));
		dev_info(tdev->dev, "send_buf %d  =  %d", i, recv_buf[i]);
		max_val = max_t(uint32_t, max_val, recv_buf[i]);
	}

	// store sysfs
	snprintf(temp_buf, 8, "%d", recv_buf[1]);
	ispv4_ddrphy_store(tdev->dev, &dev_attr_ispv4_ddrphy, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", recv_buf[2]);
	ispv4_npucore0_store(tdev->dev, &dev_attr_ispv4_npucore0, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", recv_buf[3]);
	ispv4_npucore1_store(tdev->dev, &dev_attr_ispv4_npucore1, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", recv_buf[4]);
	ispv4_top1_store(tdev->dev, &dev_attr_ispv4_top1, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", recv_buf[5]);
	ispv4_npucore3_store(tdev->dev, &dev_attr_ispv4_npucore3, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", recv_buf[6]);
	ispv4_npucore2_store(tdev->dev, &dev_attr_ispv4_npucore2, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", recv_buf[7]);
	ispv4_isp_store(tdev->dev, &dev_attr_ispv4_isp, temp_buf, 8);
	snprintf(temp_buf, 8, "%d", max_val);
	ispv4_maxtemp_store(tdev->dev, &dev_attr_ispv4_maxtemp, temp_buf, 8);
	// ispv4_top2_store(tdev->dev, &dev_attr_ispv4_top2, data + 8, 8);
	// ispv4_mipi_store(tdev->dev, &dev_attr_ispv4_mipi, data + 32, 8);
err:
	kfree(send_buf);
	dev_info(tdev->dev, "%s exit\n", __FUNCTION__);
}

static void thermal_check_ack_timer(struct timer_list *t)
{
	struct ispv4_thermal *tdev =
		container_of(t, struct ispv4_thermal, timer);
	schedule_work(&tdev->twork);
	mod_timer(t, jiffies + msecs_to_jiffies(2000));
}

static int ispv4_register_thermal_device(struct ispv4_thermal *tdev)
{
	int ret = 0;
	struct kernfs_node *sysfs_sd = NULL;
	struct kernfs_node *thermal_sd = NULL;
	struct kernfs_node *class_sd = NULL;
	struct class *cls = NULL;
	struct subsys_private *cp = NULL;
	struct kobject *kobj_temp = NULL;

	ret = alloc_chrdev_region(&tdev->devid, 0, THERMAL_CNT, THERMAL_NAME);

	if(ret) {
		dev_err(tdev->dev, "thermal_isp alloc_chrdev_region fail.\n");
		return -1;
	}

	tdev->major = MAJOR(tdev->devid);
	tdev->minor = MINOR(tdev->devid);

	dev_info(tdev->dev, "thermal_isp major=%d, minor=%d\n", tdev->major,
		tdev->minor);

	sysfs_sd = kernel_kobj->sd->parent;
	if (!sysfs_sd) {
		dev_err(tdev->dev, "%s: sysfs_sd is NULL\n", __func__);
		goto release_chrdev;
	} else {
		class_sd = kernfs_find_and_get(sysfs_sd, "class");
		if (!class_sd) {
			dev_err(tdev->dev, "%s:can not find class_sd\n",
				__func__);
			goto release_chrdev;
		} else {
			thermal_sd = kernfs_find_and_get(class_sd, "thermal");
			if (thermal_sd) {
				kobj_temp = (struct kobject *)thermal_sd->priv;
				if (kobj_temp) {
					cp = to_subsys_private(kobj_temp);
					cls = cp->class;
				} else {
					dev_err(tdev->dev,
						"%s:can not find thermal kobj\n",
						__func__);
					goto release_chrdev;
				}
			} else {
				dev_err(tdev->dev,
					"%s:can not find thermal_sd\n",
					__func__);
				goto release_chrdev;
			}
		}
	}

	if (!tdev->class && cls) {
		tdev->class = cls;
		tdev->dev = device_create(tdev->class, NULL, tdev->devid, NULL,
					  THERMAL_NAME);
		if (!tdev->dev) {
			dev_err(tdev->dev, "%s create device dev err\n",
				__func__);
			goto release_chrdev;
		}
		tdev->attrs.attrs = isp_thermal_dev_attr_group;
		ret = sysfs_create_group(&tdev->dev->kobj, &tdev->attrs);
		if (ret) {
			dev_err(tdev->dev,
				"%s ERROR: Cannot create sysfs struct !:%d\n",
				__func__, ret);
			goto release_chrdev;
		}
	}
	dev_info(tdev->dev, "tdev register success for ispv4\n");
	return 0;
release_chrdev:
	unregister_chrdev_region(tdev->devid, THERMAL_CNT);
	return -1;
}

static struct xm_ispv4_rproc *rpdev_to_xmrp(struct device *dev)
{
	struct device *rproc_dev;
	struct xm_ispv4_rproc *rp;
	rproc_dev = dev->parent->parent->parent;
	rp = container_of(rproc_dev, struct rproc, dev)->priv;
	return rp;
}

int ispv4_thermal_probe(struct platform_device *pdev)
{
	int ret;
	struct device *rpdev = pdev->dev.parent;
	struct ispv4_thermal *tdev =
		kzalloc(sizeof(struct ispv4_thermal), GFP_KERNEL);
	if (tdev == NULL) {
		ret = -ENOMEM;
		goto alloc_err;
	}
	tdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, tdev);
	tdev->rp = rpdev_to_xmrp(rpdev);
	ret = ispv4_register_thermal_device(tdev);
	if (ret != 0)
		goto reg_err;
	INIT_WORK(&tdev->twork, thermal_check_ack_work);
	timer_setup(&tdev->timer, thermal_check_ack_timer, 0);
	mod_timer(&tdev->timer, jiffies + msecs_to_jiffies(2000));
	return 0;
reg_err:
	// node_err:
	kfree(tdev);
alloc_err:
	return ret;
}

int ispv4_thermal_remove(struct platform_device *pdev)
{
	struct ispv4_thermal *tdev = platform_get_drvdata(pdev);
	del_timer_sync(&tdev->timer);
	cancel_work_sync(&tdev->twork);
	dev_info(tdev->dev, "%s:destroy_thermal_isp_node", __func__);
	sysfs_remove_group(&tdev->dev->kobj, &tdev->attrs);
	if (NULL != tdev->class) {
		device_destroy(tdev->class, tdev->devid);
		tdev->class = NULL;
	}
	unregister_chrdev_region(tdev->devid, THERMAL_CNT);
	kfree(tdev);
	return 0;
}

static struct platform_driver ispv4_thermal = {
	.driver =
		{
			.name = "ispv4_thermal",
		},
	.probe = ispv4_thermal_probe,
	.remove = ispv4_thermal_remove,
};
module_platform_driver(ispv4_thermal);

MODULE_AUTHOR("Xiaomi, Inc.");
MODULE_DESCRIPTION("Thermal driver for ispv4");
MODULE_LICENSE("GPL v2");
