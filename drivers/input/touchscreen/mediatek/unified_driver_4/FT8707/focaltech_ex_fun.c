/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2015, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 /*******************************************************************************
*
* File Name: Focaltech_ex_fun.c
*
* Author: Xu YongFeng
*
* Created: 2015-01-29
*
* Modify by mshl on 2015-03-20
*
* Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
#include <lgtp_common.h>

#include <lgtp_common_driver.h>
#include <lgtp_platform_api_i2c.h>
#include <lgtp_platform_api_misc.h>
#include <lgtp_device_ft8707.h>


/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define LGTP_MODULE "[FT8707_ex_fun]"

/*create apk debug channel*/
#define PROC_UPGRADE							0
#define PROC_READ_REGISTER						1
#define PROC_WRITE_REGISTER					2
#define PROC_AUTOCLB							4
#define PROC_UPGRADE_INFO						5
#define PROC_WRITE_DATA						6
#define PROC_READ_DATA							7
#define PROC_SET_TEST_FLAG						8
#define FTS_DEBUG_DIR_NAME					"fts_debug"
#define PROC_NAME								"ftxxxx-debug"
#define WRITE_BUF_SIZE							1016
#define READ_BUF_SIZE							1016

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
struct mutex *pMutexTouch_focal;

/*******************************************************************************
* Static variables
*******************************************************************************/
static unsigned char proc_operate_mode = PROC_UPGRADE;
static struct proc_dir_entry *fts_proc_entry;
/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
/* #if FT_ESD_PROTECT */

/* #endif */
/*******************************************************************************
* Static function prototypes
*******************************************************************************/

/*interface of write proc*/
/************************************************************************
*   Name: fts_debug_write
*  Brief:interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static ssize_t fts_debug_write(struct file *filp, const char __user *buff, size_t count,
			       loff_t *ppos)
{
	struct i2c_client *client = Touch_Get_I2C_Handle();
	unsigned char writebuf[WRITE_BUF_SIZE];
	int buflen = count;
	int writelen = 0;
	int ret = 0;
#if FT_ESD_PROTECT
/* TOUCH_LOG("\n  zax proc w 0\n"); */
	esd_switch(0);
	apk_debug_flag = 1;
/* TOUCH_LOG("\n  zax v= %d\n",apk_debug_flag); */

#endif
	if (copy_from_user(&writebuf, buff, buflen)) {
		TOUCH_LOG("%s:copy from user error\n", __func__);
#if FT_ESD_PROTECT
		esd_switch(1);
		apk_debug_flag = 0;
#endif
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];

			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen - 1] = '\0';
			TOUCH_LOG("%s\n", upgrade_file_path);

			/* #if FT_ESD_PROTECT */
			/* esd_switch(0);apk_debug_flag = 1; */
			/* #endif */
			TouchDisableIrq();
			ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);
			TouchEnableIrq();
			if (ret < 0) {
#if FT_ESD_PROTECT
				esd_switch(1);
				apk_debug_flag = 0;
#endif
				TOUCH_LOG("%s:upgrade failed.\n", __func__);
				return ret;
			}
			/* #if FT_ESD_PROTECT */
			/* esd_switch(1);apk_debug_flag = 0; */
			/* #endif */
		}
		break;
		/* case PROC_SET_TEST_FLAG: */

		/* break; */
	case PROC_SET_TEST_FLAG:
#if FT_ESD_PROTECT
		apk_debug_flag = writebuf[1];
		if (1 == apk_debug_flag)
			esd_switch(0);
		else if (0 == apk_debug_flag)
			esd_switch(1);
		TOUCH_LOG("\n zax flag=%d\n", apk_debug_flag);
#endif
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		ret = FT8707_I2C_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
#if FT_ESD_PROTECT
			esd_switch(1);
			apk_debug_flag = 0;
#endif
			TOUCH_LOG("%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = FT8707_I2C_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
#if FT_ESD_PROTECT
			esd_switch(1);
			apk_debug_flag = 0;
#endif
			TOUCH_LOG("%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_AUTOCLB:
		TOUCH_LOG("%s: autoclb\n", __func__);
		fts_ctpm_auto_clb(client);
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = count - 1;
		if (writelen > 0) {
			ret = FT8707_I2C_Write(client, writebuf + 1, writelen);
			if (ret < 0) {
#if FT_ESD_PROTECT
				esd_switch(1);
				apk_debug_flag = 0;
#endif
				TOUCH_LOG("%s:write iic error\n", __func__);
				return ret;
			}
		}
		break;
	default:
		break;
	}

#if FT_ESD_PROTECT
/* TOUCH_LOG("\n  zax proc w 1\n"); */
	esd_switch(1);
	apk_debug_flag = 0;
/* TOUCH_LOG("\n  zax v= %d\n",apk_debug_flag); */
#endif
	return count;
}

/* interface of read proc */
/************************************************************************
*   Name: fts_debug_read
*  Brief:interface of read proc
* Input: point to the data, no use, no use, read len, no use, no use
* Output: page point to data
* Return: read char number
***********************************************************************/
static ssize_t fts_debug_read(struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
	struct i2c_client *client = Touch_Get_I2C_Handle();
	int ret = 0;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	unsigned char buf[READ_BUF_SIZE];
#if FT_ESD_PROTECT
/* TOUCH_LOG("\n  zax proc r 0\n"); */
	esd_switch(0);
	apk_debug_flag = 1;
/* TOUCH_LOG("\n  zax v= %d\n",apk_debug_flag); */
#endif
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/* after calling fts_debug_write to upgrade */
		regaddr = 0xA6;
		ret = fts_read_reg(client, regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = FT8707_I2C_Read(client, NULL, 0, buf, readlen);
		if (ret < 0) {
#if FT_ESD_PROTECT
			esd_switch(1);
			apk_debug_flag = 0;
#endif
			TOUCH_LOG("%s:read iic error\n", __func__);
			return ret;
		}
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		readlen = count;
		ret = FT8707_I2C_Read(client, NULL, 0, buf, readlen);
		if (ret < 0) {
#if FT_ESD_PROTECT
			esd_switch(1);
			apk_debug_flag = 0;
#endif
			TOUCH_LOG("%s:read iic error\n", __func__);
			return ret;
		}

		num_read_chars = readlen;
		break;
	case PROC_WRITE_DATA:
		break;
	default:
		break;
	}

	if (copy_to_user(buff, buf, num_read_chars)) {
		TOUCH_LOG("%s:copy to user error\n", __func__);
#if FT_ESD_PROTECT
		esd_switch(1);
		apk_debug_flag = 0;
#endif
		return -EFAULT;
	}
#if FT_ESD_PROTECT
	/* TOUCH_LOG("\n  zax proc r 1\n"); */
	esd_switch(1);
	apk_debug_flag = 0;
/* TOUCH_LOG("\n  zax v= %d\n",apk_debug_flag); */
#endif
	/* memcpy(buff, buf, num_read_chars); */
	return num_read_chars;
}

static const struct file_operations fts_proc_fops = {
	.owner = THIS_MODULE,
	.read = fts_debug_read,
	.write = fts_debug_write,

};

/************************************************************************
* Name: fts_create_apk_debug_channel
* Brief:  create apk debug channel
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_apk_debug_channel(struct i2c_client *client)
{
	fts_proc_entry = proc_create(PROC_NAME, 0777, NULL, &fts_proc_fops);
	if (NULL == fts_proc_entry) {
		TOUCH_LOG("Couldn't create proc entry!\n");
		return -ENOMEM;
	}

	TOUCH_LOG("Create proc entry success!\n");
	return 0;
}

/************************************************************************
* Name: fts_release_apk_debug_channel
* Brief:  release apk debug channel
* Input: no
* Output: no
* Return: no
***********************************************************************/
void fts_release_apk_debug_channel(void)
{
	if (fts_proc_entry)
		proc_remove(fts_proc_entry);
}

/************************************************************************
* Name: fts_tpfwver_show
* Brief:  show tp fw vwersion
* Input: device, device attribute, char buf
* Output: no
* Return: char number
***********************************************************************/
static ssize_t fts_tpfwver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = Touch_Get_I2C_Handle();
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	/* struct i2c_client *client = container_of(dev, struct i2c_client, dev);  jacob use globle fts_wq_data data */
	mutex_lock(pMutexTouch_focal);
	if (fts_read_reg(client, FTS_REG_FW_VER, &fwver) < 0)
		return -1;


	if (fwver == 255)
		num_read_chars = snprintf(buf, PAGE_SIZE, "get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);

	mutex_unlock(pMutexTouch_focal);

	return num_read_chars;
}

/************************************************************************
* Name: fts_tpfwver_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_tpfwver_store(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_tprwreg_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_tprwreg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_tprwreg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_tprwreg_store(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ssize_t num_read_chars = 0;
	int retval;
	/*u32 wmreg=0; */
	unsigned long int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = { 0 };

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(pMutexTouch_focal);
/* mutex_lock(&fts_input_dev->mutex); */
	num_read_chars = count - 1;
	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			TOUCH_ERR("please input 2 or 4 character\n");
			goto error_return;
		}
	}
	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);
	if (0 != retval) {
		TOUCH_ERR
		    ("%s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"\n",
		     __func__, buf);
		goto error_return;
	}
	if (2 == num_read_chars) {
		/*read register */
		regaddr = wmreg;
		TOUCH_LOG("[focal](0x%02x)\n", regaddr);
		if (fts_read_reg(client, regaddr, &regvalue) < 0)
			TOUCH_LOG("[Focal] %s : Could not read the register(0x%02x)\n", __func__,
				  regaddr);
		else
			TOUCH_LOG("[Focal] %s : the register(0x%02x) is 0x%02x\n", __func__,
				  regaddr, regvalue);
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (fts_write_reg(client, regaddr, regvalue) < 0)
			TOUCH_ERR("[Focal] %s : Could not write the register(0x%02x)\n", __func__,
				  regaddr);
		else
			TOUCH_LOG("[Focal] %s : Write 0x%02x into register(0x%02x) successful\n",
				  __func__, regvalue, regaddr);
	}
error_return:
	mutex_unlock(pMutexTouch_focal);
/* mutex_unlock(&fts_input_dev->mutex); */

	return count;
}

/************************************************************************
* Name: fts_fwupdate_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_fwupdate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_fwupdate_store
* Brief:  upgrade from *.i
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_fwupdate_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	/* struct fts_ts_data *data = NULL; */
	u8 uc_host_fm_ver;
	int i_ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	/* data = (struct fts_ts_data *) i2c_get_clientdata(client); */
	const struct firmware *app_fw = NULL;
	const struct firmware *boot_fw = NULL;
	char *app_name = NULL;
	char *boot_name = NULL;
#if FT_ESD_PROTECT
	esd_switch(0);
	apk_debug_flag = 1;
#endif
	mutex_lock(pMutexTouch_focal);

	TouchDisableIrq();

	/*get firmware name and boot name */
	FT8707_Get_DefaultFWName(&app_name, &boot_name);
	i_ret = FT8707_RequestFirmware(app_name, boot_name, &app_fw, &boot_fw);
	if (i_ret == TOUCH_FAIL) {
		TOUCH_ERR("[%s] Fail to Request Firmware\n", __func__);
		return count;
	}
/* i_ret = fts_ctpm_fw_upgrade_with_i_file(client); */
	i_ret =
	    fts_ctpm_fw_upgrade_with_i_file(client, (u8 *) app_fw->data, app_fw->size,
					    (u8 *) boot_fw->data, boot_fw->size);

	/* Release F/W */
	release_firmware(app_fw);
	release_firmware(boot_fw);

	if (i_ret == 0) {
		msleep(300);
		uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		dev_dbg(dev, "%s [FTS] upgrade to new version 0x%x\n", __func__, uc_host_fm_ver);
	} else {
		TOUCH_ERR("%s ERROR:[FTS] upgrade failed ret=%d.\n", __func__, i_ret);
	}



	/* fts_ctpm_auto_upgrade(client); */
	TouchEnableIrq();
	mutex_unlock(pMutexTouch_focal);
#if FT_ESD_PROTECT
	esd_switch(1);
	apk_debug_flag = 0;
#endif
	return count;
}

/************************************************************************
* Name: fts_fwupgradeapp_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_fwupgradeapp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_fwupgradeapp_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_fwupgradeapp_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count - 1] = '\0';
#if FT_ESD_PROTECT
	esd_switch(0);
	apk_debug_flag = 1;
#endif
	mutex_lock(pMutexTouch_focal);

	TouchDisableIrq();
	fts_ctpm_fw_upgrade_with_app_file(client, fwname);
	TouchEnableIrq();

	mutex_unlock(pMutexTouch_focal);
#if FT_ESD_PROTECT
	esd_switch(1);
	apk_debug_flag = 0;
#endif
	return count;
}

/************************************************************************
* Name: fts_ftsgetprojectcode_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_getprojectcode_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	return -EPERM;
}

/************************************************************************
* Name: fts_ftsgetprojectcode_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_getprojectcode_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

/****************************************/
/* sysfs */
/* get the fw version
*   example:cat ftstpfwver
*/
static DEVICE_ATTR(ftstpfwver, S_IRUGO | S_IWUSR, fts_tpfwver_show, fts_tpfwver_store);
/* upgrade from *.i
*   example: echo 1 > ftsfwupdate
*/
static DEVICE_ATTR(ftsfwupdate, S_IRUGO | S_IWUSR, fts_fwupdate_show, fts_fwupdate_store);
/* read and write register
*   read example: echo 88 > ftstprwreg ---read register 0x88
*   write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
*
*   note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(ftstprwreg, S_IRUGO | S_IWUSR, fts_tprwreg_show, fts_tprwreg_store);
/*  upgrade from app.bin
*    example:echo "*_app.bin" > ftsfwupgradeapp
*/
static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO | S_IWUSR, fts_fwupgradeapp_show,
		   fts_fwupgradeapp_store);
static DEVICE_ATTR(ftsgetprojectcode, S_IRUGO | S_IWUSR, fts_getprojectcode_show,
		   fts_getprojectcode_store);



/* add your attr in here*/
static struct attribute *fts_attributes[] = {
	&dev_attr_ftstpfwver.attr,
	&dev_attr_ftsfwupdate.attr,
	&dev_attr_ftstprwreg.attr,
	&dev_attr_ftsfwupgradeapp.attr,
	&dev_attr_ftsgetprojectcode.attr,
	NULL
};

static struct attribute_group fts_attribute_group = {
	.attrs = fts_attributes
};

/************************************************************************
* Name: fts_create_sysfs
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_sysfs(struct i2c_client *client, TouchDriverData *pDriverData)
{
	int err;

	err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
	if (0 != err) {
		TOUCH_LOG("%s() - ERROR: sysfs_create_group() failed.\n", __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
		return -EIO;
	}
	pr_info("fts:%s() - sysfs_create_group() succeeded.\n", __func__);

	pMutexTouch_focal = &pDriverData->thread_lock;
	/* HidI2c_To_StdI2c(client); */
	return err;
}

/************************************************************************
* Name: fts_remove_sysfs
* Brief:  remove sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/
int fts_remove_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
	return 0;
}
