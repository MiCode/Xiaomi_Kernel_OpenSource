// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "eeprom_driver.h"
#include "eeprom_i2c_common_driver.h"
#include "cam_cal_list.h"

#include "cam_cal.h"

#define DEV_NODE_NAME_PREFIX "camera_eeprom"
#define DEV_NAME_FMT "camera_eeprom%u"
#define DEV_CLASS_NAME_FMT "camera_eepromdrv%u"
#define EEPROM_DEVICE_NNUMBER 255

#include "cam_cal_config.h"

static struct EEPROM_DRV ginst_drv[MAX_EEPROM_NUMBER];

static struct stCAM_CAL_LIST_STRUCT *get_list(struct CAM_CAL_SENSOR_INFO *sinfo)
{
	struct stCAM_CAL_LIST_STRUCT *plist;

	cam_cal_get_sensor_list(&plist);

	while (plist &&
	       (plist->sensorID != 0) &&
	       (plist->sensorID != sinfo->sensor_id))
		plist++;

	return plist;
}

static unsigned int read_region(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned char *buf,
				unsigned int offset, unsigned int size)
{
	unsigned int ret;
	unsigned short dts_addr;
	struct stCAM_CAL_LIST_STRUCT *plist = get_list(&pdata->sensor_info);
	unsigned int size_limit = (plist && plist->maxEepromSize > 0)
		? plist->maxEepromSize : DEFAULT_MAX_EEPROM_SIZE_8K;

	if (offset + size > size_limit) {
		error_log("Not support address >= 0x%x!!\n", size_limit);
		return 0;
	}

	if (plist && plist->readCamCalData) {
		must_log("i2c addr 0x%x\n", plist->slaveID);
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		dts_addr = pdata->pdrv->pi2c_client->addr;
		pdata->pdrv->pi2c_client->addr = (plist->slaveID >> 1);
		ret = plist->readCamCalData(pdata->pdrv->pi2c_client,
					    offset, buf, size);
		pdata->pdrv->pi2c_client->addr = dts_addr;
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	} else {
		must_log("no customized\n");
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		ret = Common_read_region(pdata->pdrv->pi2c_client,
					 offset, buf, size);
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	}

	return ret;
}

static unsigned int write_region(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned char *buf,
				unsigned int offset, unsigned int size)
{
	unsigned int ret;
	unsigned short dts_addr;
	struct stCAM_CAL_LIST_STRUCT *plist = get_list(&pdata->sensor_info);
	unsigned int size_limit = (plist && plist->maxEepromSize > 0)
		? plist->maxEepromSize : DEFAULT_MAX_EEPROM_SIZE_8K;

	if (offset + size > size_limit) {
		error_log("Not support address >= 0x%x!!\n", size_limit);
		return 0;
	}

	if (plist && plist->writeCamCalData) {
		must_log("i2c addr 0x%x\n", plist->slaveID);
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		dts_addr = pdata->pdrv->pi2c_client->addr;
		pdata->pdrv->pi2c_client->addr = (plist->slaveID >> 1);
		ret = plist->writeCamCalData(pdata->pdrv->pi2c_client,
					    offset, buf, size);
		pdata->pdrv->pi2c_client->addr = dts_addr;
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	} else {
		must_log("no customized\n");
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		ret = Common_write_region(pdata->pdrv->pi2c_client,
					 offset, buf, size);
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	}

	return ret;
}

static int eeprom_open(struct inode *a_inode, struct file *a_file)
{
	struct EEPROM_DRV_FD_DATA *pdata;
	struct EEPROM_DRV *pdrv;

	// must_log("open\n");

	pdata = kmalloc(sizeof(struct EEPROM_DRV_FD_DATA), GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

	pdrv = container_of(a_inode->i_cdev, struct EEPROM_DRV, cdev);

	pdata->pdrv = pdrv;
	pdata->sensor_info.sensor_id = 0;

	a_file->private_data = pdata;

	return 0;
}

static int eeprom_release(struct inode *a_inode, struct file *a_file)
{
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;

	// must_log("release\n");

	kfree(pdata);

	return 0;
}

static ssize_t eeprom_read(struct file *a_file, char __user *user_buffer,
			   size_t size, loff_t *offset)
{
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;
	u8 *kbuf = kmalloc(size, GFP_KERNEL);

	must_log("read %lu %llu\n", size, *offset);

	if (kbuf == NULL)
		return -ENOMEM;

	if (read_region(pdata, kbuf, *offset, size) != size ||
	    copy_to_user(user_buffer, kbuf, size)) {
		kfree(kbuf);
		return -EFAULT;
	}

	*offset += size;
	kfree(kbuf);
	return size;
}

static ssize_t eeprom_write(struct file *a_file, const char __user *user_buffer,
			    size_t size, loff_t *offset)
{
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;
	u8 *kbuf = kmalloc(size, GFP_KERNEL);

	must_log("write %lu %llu\n", size, *offset);

	if (kbuf == NULL)
		return -ENOMEM;

	if (copy_from_user(kbuf, user_buffer, size) ||
	    write_region(pdata, kbuf, *offset, size) != size) {
		kfree(kbuf);
		return -EFAULT;
	}

	*offset += size;
	kfree(kbuf);
	return size;
}

static loff_t eeprom_seek(struct file *a_file, loff_t offset, int whence)
{
#define MAX_LENGTH 16192 /*MAX 16k bytes*/
	loff_t new_pos = 0;

	switch (whence) {
	case 0: /* SEEK_SET: */
		new_pos = offset;
		break;
	case 1: /* SEEK_CUR: */
		new_pos = a_file->f_pos + offset;
		break;
	case 2: /* SEEK_END: */
		new_pos = MAX_LENGTH + offset;
		break;
	default:
		return -EINVAL;
	}

	if (new_pos < 0)
		return -EINVAL;

	a_file->f_pos = new_pos;

	return new_pos;
}

static long eeprom_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	unsigned int ret;
	void *pBuff = NULL;
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;

	if (_IOC_DIR(a_cmd) == _IOC_NONE)
		return -EFAULT;

	pBuff = kzalloc(_IOC_SIZE(a_cmd), GFP_KERNEL);
	if (pBuff == NULL)
		return -ENOMEM;
	memset(pBuff, 0, _IOC_SIZE(a_cmd));

	if ((_IOC_WRITE & _IOC_DIR(a_cmd)) &&
	    copy_from_user(pBuff,
			   (void *)a_param,
			   _IOC_SIZE(a_cmd))) {

		kfree(pBuff);
		must_log("ioctl copy from user failed\n");
		return -EFAULT;
	}

	switch (a_cmd) {
	case CAM_CALIOC_S_SENSOR_INFO:
		pdata->sensor_info.sensor_id =
			((struct CAM_CAL_SENSOR_INFO *)pBuff)->sensor_id;
		must_log("sensor id = 0x%x\n",
		       pdata->sensor_info.sensor_id);
		break;
	case CAM_CALIOC_G_GKI_QUERY:
		/* debug_log("QUERY\n"); */
		break;
	case CAM_CALIOC_G_GKI_READ:
		ret = get_cal_data(pdata, (unsigned int *)pBuff);
		if (ret == CAM_CAL_ERR_NO_ERR) {
			if (copy_to_user((u8 __user *) a_param, (u8 *) pBuff, _IOC_SIZE(a_cmd))) {
				kfree(pBuff);
				return CAM_CAL_ERR_NO_DEVICE;
			}
		}
		kfree(pBuff);
		return ret;
	case CAM_CALIOC_G_GKI_NEED_POWER_ON:
		ret = get_is_need_power_on(pdata, (unsigned int *)pBuff);
		if (ret == CAM_CAL_ERR_NO_ERR) {
			if (copy_to_user((u8 __user *) a_param, (u8 *) pBuff, _IOC_SIZE(a_cmd))) {
				kfree(pBuff);
				return CAM_CAL_ERR_NO_DEVICE;
			}
		}
		kfree(pBuff);
		return ret;
	default:
		kfree(pBuff);
		must_log("No such command %d\n", a_cmd);
		return -EPERM;
	}

	kfree(pBuff);
	return 0;
}

#ifdef CONFIG_COMPAT
static long eeprom_compat_ioctl(struct file *a_file, unsigned int a_cmd,
				unsigned long a_param)
{
	must_log("compat ioctl\n");

	return 0;
}
#endif

static const struct file_operations geeprom_file_operations = {
	.owner = THIS_MODULE,
	.open = eeprom_open,
	.read = eeprom_read,
	.write = eeprom_write,
	.llseek = eeprom_seek,
	.release = eeprom_release,
	.unlocked_ioctl = eeprom_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = eeprom_compat_ioctl
#endif
};

static inline int retrieve_index(struct i2c_client *client,
				 unsigned int *index)
{
	const char *node_name = client->dev.of_node->name;
	const size_t prefix_len = strlen(DEV_NODE_NAME_PREFIX);

	if (strncmp(node_name, DEV_NODE_NAME_PREFIX, prefix_len) == 0 &&
	    kstrtouint(node_name + prefix_len, 10, index) == 0) {
		must_log("index = %u\n", *index);
		return 0;
	}

	pr_err("invalid node name format\n");
	*index = 0;
	return -EINVAL;
}

static inline int eeprom_driver_register(struct i2c_client *client,
					 unsigned int index)
{
	int ret = 0;
	struct EEPROM_DRV *pinst;
	char device_drv_name[DEV_NAME_STR_LEN_MAX] = { 0 };
	char class_drv_name[DEV_NAME_STR_LEN_MAX] = { 0 };

	if (index >= MAX_EEPROM_NUMBER) {
		pr_err("node index out of bound\n");
		return -EINVAL;
	}

	ret = snprintf(device_drv_name, DEV_NAME_STR_LEN_MAX - 1,
		DEV_NAME_FMT, index);
	if (ret < 0) {
		pr_info(
		"[eeprom]%s error, ret = %d", __func__, ret);
		return -EFAULT;
	}
	ret = snprintf(class_drv_name, DEV_NAME_STR_LEN_MAX - 1,
		DEV_CLASS_NAME_FMT, index);
	if (ret < 0) {
		pr_info(
		"[eeprom]%s error, ret = %d", __func__, ret);
		return -EFAULT;
	}

	ret = 0;
	pinst = &ginst_drv[index];
	pinst->dev_no = MKDEV(EEPROM_DEVICE_NNUMBER, index);

	if (alloc_chrdev_region(&pinst->dev_no, 0, 1, device_drv_name)) {
		pr_err("Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Attatch file operation. */
	cdev_init(&pinst->cdev, &geeprom_file_operations);

	/* Add to system */
	if (cdev_add(&pinst->cdev, pinst->dev_no, 1)) {
		pr_err("Attatch file operation failed\n");
		unregister_chrdev_region(pinst->dev_no, 1);
		return -EAGAIN;
	}

	memcpy(pinst->class_name, class_drv_name, DEV_NAME_STR_LEN_MAX);
	pinst->pclass = class_create(THIS_MODULE, pinst->class_name);
	if (IS_ERR(pinst->pclass)) {
		ret = PTR_ERR(pinst->pclass);

		pr_err("Unable to create class, err = %d\n", ret);
		return ret;
	}

	device_create(pinst->pclass, NULL, pinst->dev_no, NULL,
		      device_drv_name);

	pinst->pi2c_client = client;
	mutex_init(&pinst->eeprom_mutex);

	return ret;
}

static inline int eeprom_driver_unregister(unsigned int index)
{
	struct EEPROM_DRV *pinst = NULL;

	if (index >= MAX_EEPROM_NUMBER) {
		pr_err("node index out of bound\n");
		return -EINVAL;
	}

	pinst = &ginst_drv[index];

	/* Release char driver */
	unregister_chrdev_region(pinst->dev_no, 1);

	device_destroy(pinst->pclass, pinst->dev_no);
	class_destroy(pinst->pclass);

	return 0;
}

static int eeprom_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	unsigned int index = 0;

	must_log("probe start name: %s\n", client->dev.of_node->name);

	if (retrieve_index(client, &index) < 0)
		return -EINVAL;
	else
		return eeprom_driver_register(client, index);
}

static int eeprom_remove(struct i2c_client *client)
{
	unsigned int index = 0;

	must_log("remove name: %s\n", client->dev.of_node->name);

	if (retrieve_index(client, &index) < 0)
		return -EINVAL;
	else
		return eeprom_driver_unregister(index);
}

static const struct of_device_id eeprom_of_match[] = {
	{ .compatible = "mediatek,camera_eeprom", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, eeprom_of_match);

static struct i2c_driver eeprom_i2c_init = {
	.driver = {
		.name   = "mediatek,camera_eeprom",
		.of_match_table = of_match_ptr(eeprom_of_match),
	},
	.probe      = eeprom_probe,
	.remove     = eeprom_remove,
};

module_i2c_driver(eeprom_i2c_init);

MODULE_DESCRIPTION("camera eeprom driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");

