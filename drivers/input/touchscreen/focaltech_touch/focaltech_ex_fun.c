/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

/*****************************************************************************
*
* File Name: Focaltech_ex_fun.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
/*create apk debug channel*/
#define PROC_UPGRADE                            0
#define PROC_READ_REGISTER                      1
#define PROC_WRITE_REGISTER                     2
#define PROC_AUTOCLB                            4
#define PROC_UPGRADE_INFO                       5
#define PROC_WRITE_DATA                         6
#define PROC_READ_DATA                          7
#define PROC_SET_TEST_FLAG                      8
#define PROC_SET_SLAVE_ADDR                     10
#define PROC_HW_RESET                           11
#define PROC_NAME                               "ftxxxx-debug"
#define WRITE_BUF_SIZE                          512
#define READ_BUF_SIZE                           512

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/
static unsigned char proc_operate_mode = PROC_UPGRADE;
static struct proc_dir_entry *fts_proc_entry;
static struct
{
	int op;
	int reg;
	int value;
	int result;
} g_rwreg_result;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
#if FTS_ESDCHECK_EN
static void esd_process(u8 *writebuf, int buflen, bool flag)
{
	if (flag)
	{
		if ( (writebuf[1] == 0xFC) && (writebuf[2] == 0x55) && (buflen == 0x03) )
		{
			/* Upgrade command */
			FTS_DEBUG("[ESD]: Upgrade command(%x %x %x)!!", writebuf[0], writebuf[1], writebuf[2]);
			fts_esdcheck_switch(DISABLE);
		}
		else if ( (writebuf[1] == 0x00) && (writebuf[2] == 0x40) && (buflen == 0x03) )
		{
			/* factory mode bit 4 5 6 */
			FTS_DEBUG("[ESD]: Entry factory mode(%x %x %x)!!", writebuf[0], writebuf[1], writebuf[2]);
			fts_esdcheck_switch(DISABLE);
		}
		else if ( (writebuf[1] == 0x00) && (writebuf[2] == 0x00) && (buflen == 0x03) )
		{
			/* normal mode bit 4 5 6 */
			FTS_DEBUG("[ESD]: Exit factory mode(%x %x %x)!!", writebuf[0], writebuf[1], writebuf[2]);
			fts_esdcheck_switch(ENABLE);
		}
		else
		{
			fts_esdcheck_proc_busy(1);
		}
	}
	else
	{
		if ( (writebuf[1] == 0x07) && (buflen == 0x02) )
		{
			FTS_DEBUG("[ESD]: Upgrade finish-trigger reset(07)(%x %x)!!", writebuf[0], writebuf[1]);
			fts_esdcheck_switch(ENABLE);
		}
		else
		{
			fts_esdcheck_proc_busy(0);
		}
	}
}
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
/*interface of write proc*/
/************************************************************************
*   Name: fts_debug_write
*  Brief:interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static ssize_t fts_debug_write(struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
	unsigned char writebuf[WRITE_BUF_SIZE];
	int buflen = count;
	int writelen = 0;
	int ret = 0;
	char tmp[25];

	if (copy_from_user(&writebuf, buff, buflen))
	{
		FTS_DEBUG("[APK]: copy from user error!!");
		return -EFAULT;
	}

	#if FTS_ESDCHECK_EN
	esd_process(writebuf, buflen, 1);
	#endif
	proc_operate_mode = writebuf[0];
	switch (proc_operate_mode)
	{
		case PROC_UPGRADE:
		{
			char upgrade_file_path[FILE_NAME_LENGTH];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			FTS_DEBUG("%s\n", upgrade_file_path);
			fts_irq_disable();
			#if FTS_ESDCHECK_EN
			fts_esdcheck_switch(DISABLE);
			#endif
			if (fts_updatefun_curr.upgrade_with_app_bin_file)
				ret = fts_updatefun_curr.upgrade_with_app_bin_file(fts_i2c_client, upgrade_file_path);
			#if FTS_ESDCHECK_EN
			fts_esdcheck_switch(ENABLE);
			#endif
			fts_irq_enable();
			if (ret < 0)
			{
				FTS_ERROR("[APK]: upgrade failed!!");
			}
		}
			break;
		case PROC_SET_TEST_FLAG:
			FTS_DEBUG("[APK]: PROC_SET_TEST_FLAG = %x!!", writebuf[1]);
			#if FTS_ESDCHECK_EN
			if (writebuf[1] == 0)
			{
				fts_esdcheck_switch(DISABLE);
			}
			else
			{
				fts_esdcheck_switch(ENABLE);
			}
			#endif
			break;
		case PROC_READ_REGISTER:
			writelen = 1;
			ret = fts_i2c_write(fts_i2c_client, writebuf + 1, writelen);
			if (ret < 0)
			{
				FTS_ERROR("[APK]: write iic error!!");
			}
			break;
		case PROC_WRITE_REGISTER:
			writelen = 2;
			ret = fts_i2c_write(fts_i2c_client, writebuf + 1, writelen);
			if (ret < 0)
			{
				FTS_ERROR("[APK]: write iic error!!");
			}
			break;
		case PROC_SET_SLAVE_ADDR:

			ret = fts_i2c_client->addr;
			FTS_DEBUG("Original i2c addr 0x%x ", ret<<1 );
			if (writebuf[1] != fts_i2c_client->addr)
			{
				fts_i2c_client->addr = writebuf[1];
				FTS_DEBUG("Change i2c addr 0x%x to 0x%x", ret<<1, writebuf[1]<<1);

			}
			break;
		case PROC_HW_RESET:

			sprintf(tmp, "%s", writebuf + 1);
			tmp[buflen - 1] = '\0';
			if (strncmp(tmp, "focal_driver", 12)==0)
			{
				FTS_DEBUG("Begin HW Reset");
				fts_reset_proc(1);
			}
			break;
		case PROC_AUTOCLB:
			FTS_DEBUG("[APK]: autoclb!!");
			fts_ctpm_auto_clb(fts_i2c_client);
			break;
		case PROC_READ_DATA:
		case PROC_WRITE_DATA:
			writelen = count - 1;
			if (writelen>0)
			{
				ret = fts_i2c_write(fts_i2c_client, writebuf + 1, writelen);
				if (ret < 0)
				{
					FTS_ERROR("[APK]: write iic error!!");
				}
			}
			break;
		default:
			break;
	}

	#if FTS_ESDCHECK_EN
	esd_process(writebuf, buflen, 0);
	#endif

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		return count;
	}
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
	int ret = 0;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	unsigned char buf[READ_BUF_SIZE];

	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(1);
	#endif
	switch (proc_operate_mode)
	{
		case PROC_UPGRADE:

			regaddr = FTS_REG_FW_VER;
			ret = fts_i2c_read_reg(fts_i2c_client, regaddr, &regvalue);
			if (ret < 0)
				num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
			else
				num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
			break;
		case PROC_READ_REGISTER:
			readlen = 1;
			ret = fts_i2c_read(fts_i2c_client, NULL, 0, buf, readlen);
			if (ret < 0)
			{
				#if FTS_ESDCHECK_EN
				fts_esdcheck_proc_busy(0);
				#endif
				FTS_ERROR("[APK]: read iic error!!");
				return ret;
			}
			num_read_chars = 1;
			break;
		case PROC_READ_DATA:
			readlen = count;
			ret = fts_i2c_read(fts_i2c_client, NULL, 0, buf, readlen);
			if (ret < 0)
			{
				#if FTS_ESDCHECK_EN
				fts_esdcheck_proc_busy(0);
				#endif
				FTS_ERROR("[APK]: read iic error!!");
				return ret;
			}
			num_read_chars = readlen;
			break;
		case PROC_WRITE_DATA:
			break;
		default:
			break;
	}

	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(0);
	#endif

	if (copy_to_user(buff, buf, num_read_chars))
	{
		FTS_ERROR("[APK]: copy to user error!!");
		return -EFAULT;
	}

	return num_read_chars;
}

static const struct file_operations fts_proc_fops =
{
	.owner  = THIS_MODULE,
	.read   = fts_debug_read,
	.write  = fts_debug_write,
};
#else
/* interface of write proc */
/************************************************************************
*   Name: fts_debug_write
*  Brief:interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static int fts_debug_write(struct file *filp,
					       const char __user *buff, unsigned long len, void *data)
{
	unsigned char writebuf[WRITE_BUF_SIZE];
	int buflen = len;
	int writelen = 0;
	int ret = 0;
	char tmp[25];

	if (copy_from_user(&writebuf, buff, buflen))
	{
		FTS_ERROR("[APK]: copy from user error!!");
		return -EFAULT;
	}
	#if FTS_ESDCHECK_EN
	esd_process(writebuf, buflen, 1);
	#endif
	proc_operate_mode = writebuf[0];
	switch (proc_operate_mode)
	{
		case PROC_UPGRADE:
		{
			char upgrade_file_path[FILE_NAME_LENGTH];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			FTS_DEBUG("%s\n", upgrade_file_path);
			fts_irq_disable();
			#if FTS_ESDCHECK_EN
			fts_esdcheck_switch(DISABLE);
			#endif
			if (fts_updatefun_curr.upgrade_with_app_bin_file)
				ret = fts_updatefun_curr.upgrade_with_app_bin_file(fts_i2c_client, upgrade_file_path);
			#if FTS_ESDCHECK_EN
			fts_esdcheck_switch(ENABLE);
			#endif
			fts_irq_enable();
			if (ret < 0)
			{
				FTS_ERROR("[APK]: upgrade failed!!");
			}
		}
			break;
		case PROC_SET_TEST_FLAG:
			FTS_DEBUG("[APK]: PROC_SET_TEST_FLAG = %x!!", writebuf[1]);
			#if FTS_ESDCHECK_EN
			if (writebuf[1] == 0)
			{
				fts_esdcheck_switch(DISABLE);
			}
			else
			{
				fts_esdcheck_switch(ENABLE);
			}
			#endif
			break;
		case PROC_READ_REGISTER:
			writelen = 1;
			ret = fts_i2c_write(fts_i2c_client, writebuf + 1, writelen);
			if (ret < 0)
			{
				FTS_ERROR("[APK]: write iic error!!n");
			}
			break;
		case PROC_WRITE_REGISTER:
			writelen = 2;
			ret = fts_i2c_write(fts_i2c_client, writebuf + 1, writelen);
			if (ret < 0)
			{
				FTS_ERROR("[APK]: write iic error!!");
			}
			break;
		case PROC_SET_SLAVE_ADDR:
			ret = fts_i2c_client->addr;
			FTS_DEBUG("Original i2c addr 0x%x ", ret<<1 );
			if (writebuf[1] != fts_i2c_client->addr)
			{
				fts_i2c_client->addr = writebuf[1];
				FTS_DEBUG("Change i2c addr 0x%x to 0x%x", ret<<1, writebuf[1]<<1);

			}
			break;
		case PROC_HW_RESET:
			sprintf(tmp, "%s", writebuf + 1);
			tmp[buflen - 1] = '\0';
			if (strncmp(tmp, "focal_driver", 12)==0)
			{
				FTS_DEBUG("Begin HW Reset");
				fts_reset_proc(1);
			}
			break;
		case PROC_AUTOCLB:
			FTS_DEBUG("[APK]: autoclb!!");
			fts_ctpm_auto_clb(fts_i2c_client);
			break;
		case PROC_READ_DATA:
		case PROC_WRITE_DATA:
			writelen = len - 1;
			if (writelen>0)
			{
				ret = fts_i2c_write(fts_i2c_client, writebuf + 1, writelen);
				if (ret < 0)
				{
					FTS_ERROR("[APK]: write iic error!!");
				}
			}
			break;
		default:
			break;
	}

	#if FTS_ESDCHECK_EN
	esd_process(writebuf, buflen, 0);
	#endif

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		return len;
	}
}

/* interface of read proc */
/************************************************************************
*   Name: fts_debug_read
*  Brief:interface of read proc
* Input: point to the data, no use, no use, read len, no use, no use
* Output: page point to data
* Return: read char number
***********************************************************************/
static int fts_debug_read( char *page, char **start,
					       off_t off, int count, int *eof, void *data )
{
	int ret = 0;
	unsigned char buf[READ_BUF_SIZE];
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;

	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(1);
	#endif
	switch (proc_operate_mode)
	{
		case PROC_UPGRADE:

			regaddr = FTS_REG_FW_VER;
			ret = fts_i2c_read_reg(fts_i2c_client, regaddr, &regvalue);
			if (ret < 0)
				num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
			else
				num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
			break;
		case PROC_READ_REGISTER:
			readlen = 1;
			ret = fts_i2c_read(fts_i2c_client, NULL, 0, buf, readlen);
			if (ret < 0)
			{
				#if FTS_ESDCHECK_EN
				fts_esdcheck_proc_busy(0);
				#endif
				FTS_ERROR("[APK]: read iic error!!");
				return ret;
			}
			num_read_chars = 1;
			break;
		case PROC_READ_DATA:
			readlen = count;
			ret = fts_i2c_read(fts_i2c_client, NULL, 0, buf, readlen);
			if (ret < 0)
			{
				#if FTS_ESDCHECK_EN
				fts_esdcheck_proc_busy(0);
				#endif
				FTS_ERROR("[APK]: read iic error!!");
				return ret;
			}

			num_read_chars = readlen;
			break;
		case PROC_WRITE_DATA:
			break;
		default:
			break;
	}

	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(0);
	#endif

	memcpy(page, buf, num_read_chars);
	return num_read_chars;
}
#endif
/************************************************************************
* Name: fts_create_apk_debug_channel
* Brief:  create apk debug channel
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_apk_debug_channel(struct i2c_client * client)
{
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	fts_proc_entry = proc_create(PROC_NAME, 0777, NULL, &fts_proc_fops);
	#else
	fts_proc_entry = create_proc_entry(PROC_NAME, 0777, NULL);
	#endif
	if (NULL == fts_proc_entry)
	{
		FTS_ERROR("Couldn't create proc entry!");
		return -ENOMEM;
	}
	else
	{
		FTS_INFO("Create proc entry success!");

	#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
		fts_proc_entry->write_proc = fts_debug_write;
		fts_proc_entry->read_proc = fts_debug_read;
	#endif
	}
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
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
		proc_remove(fts_proc_entry);
	#else
		remove_proc_entry(PROC_NAME, NULL);
	#endif
}

/*
 * fts_hw_reset interface
 */
static ssize_t fts_hw_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return -EPERM;
}
static ssize_t fts_hw_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	fts_reset_proc(200);
	count = snprintf(buf, PAGE_SIZE, "hw reset executed\n");

	return count;
}

/*
 * fts_irq interface
 */
static ssize_t fts_irq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (FTS_SYSFS_ECHO_ON(buf))
	{
		FTS_INFO("[EX-FUN]enable irq");
		fts_irq_enable();
	}
	else if (FTS_SYSFS_ECHO_OFF(buf))
	{
		FTS_INFO("[EX-FUN]disable irq");
		fts_irq_disable();
	}
	return count;
}

static ssize_t fts_irq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return -EPERM;
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
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&fts_input_dev->mutex);

	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(1);
	#endif
	if (fts_i2c_read_reg(client, FTS_REG_FW_VER, &fwver) < 0)
	{
		num_read_chars = snprintf(buf, PAGE_SIZE, "I2c transfer error!\n");
	}
	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(0);
	#endif
	if (fwver == 255)
		num_read_chars = snprintf(buf, PAGE_SIZE, "get tp fw version fail!\n");
	else
	{
		num_read_chars = snprintf(buf, PAGE_SIZE, "FT5446 fw version:\t%02X\n", fwver);
	}

	mutex_unlock(&fts_input_dev->mutex);

	return num_read_chars;
}
/************************************************************************
* Name: fts_tpfwver_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_tpfwver_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

static int fts_is_hex_char(const char ch)
{
	int result = 0;
	if (ch >= '0' && ch <= '9')
	{
		result = 1;
	}
	else if (ch >= 'a' && ch <= 'f')
	{
		result = 1;
	}
	else if (ch >= 'A' && ch <= 'F')
	{
		result = 1;
	}
	else
	{
		result = 0;
	}

	return result;
}

static int fts_hex_char_to_int(const char ch)
{
	int result = 0;
	if (ch >= '0' && ch <= '9')
	{
		result = (int)(ch - '0');
	}
	else if (ch >= 'a' && ch <= 'f')
	{
		result = (int)(ch - 'a') + 10;
	}
	else if (ch >= 'A' && ch <= 'F')
	{
		result = (int)(ch - 'A') + 10;
	}
	else
	{
		result = -1;
	}

	return result;
}

static int fts_hex_to_str(char *hex, int iHexLen, char *ch, int *iChLen)
{
	int high=0;
	int low=0;
	int tmp = 0;
	int i = 0;
	int iCharLen = 0;
	if (hex == NULL || ch == NULL)
	{
		return -1;
	}

	FTS_DEBUG("iHexLen: %d in function:%s!!\n\n", iHexLen, __func__);
	if (iHexLen %2 == 1)
	{
		return -2;
	}

	for (i=0; i<iHexLen; i+=2)
	{
		high = fts_hex_char_to_int(hex[i]);
		if (high < 0)
		{
			ch[iCharLen] = '\0';
			return -3;
		}

		low = fts_hex_char_to_int(hex[i+1]);
		if (low < 0)
		{
			ch[iCharLen] = '\0';
			return -3;
		}
		tmp = (high << 4) + low;
		ch[iCharLen++] = (char)tmp;
	}
	ch[iCharLen] = '\0';
	*iChLen = iCharLen;
	FTS_DEBUG("iCharLen: %d, iChLen: %d in function:%s!!\n\n", iCharLen, *iChLen, __func__);
	return 0;
}

static void fts_str_to_bytes(char * bufStr, int iLen, char* uBytes, int *iBytesLen)
{
	int i=0;
	int iNumChLen=0;
	*iBytesLen=0;

	for (i=0; i<iLen; i++)
	{
		if (fts_is_hex_char(bufStr[i]))
		{
			bufStr[iNumChLen++] = bufStr[i];
		}
	}

	bufStr[iNumChLen] = '\0';
	fts_hex_to_str(bufStr, iNumChLen, uBytes, iBytesLen);
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
	int count;

	mutex_lock(&fts_input_dev->mutex);

	if (!g_rwreg_result.op)
	{
		if (g_rwreg_result.result == 0)
		{
			count = sprintf(buf, "Read %02X: %02X\n", g_rwreg_result.reg, g_rwreg_result.value);
		}
		else
		{
			count = sprintf(buf, "Read %02X failed, ret: %d\n", g_rwreg_result.reg,  g_rwreg_result.result);
		}
	}
	else
	{
		if (g_rwreg_result.result == 0)
		{
			count = sprintf(buf, "Write %02X, %02X success\n", g_rwreg_result.reg,  g_rwreg_result.value);
		}
		else
		{
			count = sprintf(buf, "Write %02X failed, ret: %d\n", g_rwreg_result.reg,  g_rwreg_result.result);
		}
	}
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_tprwreg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_tprwreg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	long unsigned int wmreg=0;
	u8 regaddr=0xff, regvalue=0xff;
	u8 valbuf[5]= {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&fts_input_dev->mutex);
	num_read_chars = count - 1;
	if (num_read_chars != 2)
	{
		if (num_read_chars != 4)
		{
			FTS_ERROR("please input 2 or 4 character");
			goto error_return;
		}
	}
	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);
	fts_str_to_bytes((char*)buf, num_read_chars, valbuf, &retval);

	if (1==retval)
	{
		regaddr = valbuf[0];
		retval = 0;
	}
	else if (2==retval)
	{
		regaddr = valbuf[0];
		regvalue = valbuf[1];
		retval = 0;
	}
	else
		retval =-1;

	if (0 != retval)
	{
		FTS_ERROR("%s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"", __FUNCTION__, buf);
		goto error_return;
	}
	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(1);
	#endif
	if (2 == num_read_chars)
	{
		g_rwreg_result.op = 0;
		g_rwreg_result.reg = regaddr;
		/*read register*/
		regaddr = wmreg;
		g_rwreg_result.result = fts_i2c_read_reg(client, regaddr, &regvalue);
		if (g_rwreg_result.result < 0)
		{
			FTS_ERROR("Could not read the register(0x%02x)", regaddr);
		}
		else
		{
			FTS_INFO("the register(0x%02x) is 0x%02x", regaddr, regvalue);
			g_rwreg_result.value = regvalue;
			g_rwreg_result.result = 0;
		}
	}
	else
	{
		regaddr = wmreg>>8;
		regvalue = wmreg;

		g_rwreg_result.op = 1;
		g_rwreg_result.reg = regaddr;
		g_rwreg_result.value = regvalue;
		g_rwreg_result.result = fts_i2c_write_reg(client, regaddr, regvalue);
		if (g_rwreg_result.result < 0)
		{
			FTS_ERROR("Could not write the register(0x%02x)", regaddr);

		}
		else
		{
			FTS_INFO("Write 0x%02x into register(0x%02x) successful", regvalue, regaddr);
			g_rwreg_result.result = 0;
		}
	}
	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(0);
	#endif
error_return:
	mutex_unlock(&fts_input_dev->mutex);

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
static ssize_t fts_fwupdate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&fts_input_dev->mutex);
	fts_irq_disable();
	#if FTS_ESDCHECK_EN
	fts_esdcheck_switch(DISABLE);
	#endif

	if (fts_updatefun_curr.upgrade_with_app_i_file)
		fts_updatefun_curr.upgrade_with_app_i_file(client);
	#if FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN
	if (fts_updatefun_curr.upgrade_with_lcd_cfg_i_file)
		fts_updatefun_curr.upgrade_with_lcd_cfg_i_file(client);
	#endif

	#if FTS_ESDCHECK_EN
	fts_esdcheck_switch(ENABLE);
	#endif
	fts_irq_enable();
	mutex_unlock(&fts_input_dev->mutex);

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
static ssize_t fts_fwupgradeapp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[FILE_NAME_LENGTH];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';

	mutex_lock(&fts_input_dev->mutex);
	fts_irq_disable();
	#if FTS_ESDCHECK_EN
	fts_esdcheck_switch(DISABLE);
	#endif
	if (fts_updatefun_curr.upgrade_with_app_bin_file)
		fts_updatefun_curr.upgrade_with_app_bin_file(client, fwname);
	#if FTS_ESDCHECK_EN
	fts_esdcheck_switch(ENABLE);
	#endif
	fts_irq_enable();
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_driverversion_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_driverversion_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	mutex_lock(&fts_input_dev->mutex);
	count = sprintf(buf, FTS_DRIVER_VERSION "\n");
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_driverversion_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_driverversion_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

#if FTS_ESDCHECK_EN
/************************************************************************
* Name: fts_esdcheck_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_esdcheck_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&fts_input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
	{
		FTS_DEBUG("enable esdcheck");
		fts_esdcheck_switch(ENABLE);
	}
	else if (FTS_SYSFS_ECHO_OFF(buf))
	{
		FTS_DEBUG("disable esdcheck");
		fts_esdcheck_switch(DISABLE);
	}
	mutex_unlock(&fts_input_dev->mutex);

	return -EPERM;
}

/************************************************************************
* Name: fts_esdcheck_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_esdcheck_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	mutex_lock(&fts_input_dev->mutex);

	count = sprintf(buf, "Esd check: %s\n", fts_esdcheck_get_status() ? "On" : "Off");

	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
#endif
/************************************************************************
* Name: fts_module_config_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_module_config_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;

	mutex_lock(&fts_input_dev->mutex);

	count += sprintf(buf, "FTS_CHIP_TYPE: \t\t\t%04X\n", FTS_CHIP_TYPE);
	count += sprintf(buf+count, "FTS_DEBUG_EN: \t\t\t%s\n", FTS_DEBUG_EN ? "ON" : "OFF");
	#if defined(FTS_MT_PROTOCOL_B_EN)
	count += sprintf(buf+count, "TS_MT_PROTOCOL_B_EN: \t\t%s\n", FTS_MT_PROTOCOL_B_EN ? "ON" : "OFF");
	#endif
	count += sprintf(buf+count, "FTS_GESTURE_EN: \t\t%s\n", FTS_GESTURE_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_ESDCHECK_EN: \t\t%s\n", FTS_ESDCHECK_EN ? "ON" : "OFF");
	#if defined(FTS_PSENSOR_EN)
	count += sprintf(buf+count, "FTS_PSENSOR_EN: \t\t%s\n", FTS_PSENSOR_EN ? "ON" : "OFF");
	#endif
	count += sprintf(buf+count, "FTS_GLOVE_EN: \t\t\t%s\n", FTS_GLOVE_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_COVER_EN: \t\t%s\n", FTS_COVER_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_CHARGER_EN: \t\t\t%s\n", FTS_CHARGER_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_REPORT_PRESSURE_EN: \t\t%s\n", FTS_REPORT_PRESSURE_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_TEST_EN: \t\t\t%s\n", FTS_TEST_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_APK_NODE_EN: \t\t%s\n", FTS_APK_NODE_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_POWER_SOURCE_CUST_EN: \t%s\n", FTS_POWER_SOURCE_CUST_EN ? "ON" : "OFF");
	count += sprintf(buf+count, "FTS_AUTO_UPGRADE_EN: \t\t%s\n", FTS_AUTO_UPGRADE_EN ? "ON" : "OFF");

	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_module_config_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_module_config_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_show_log_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_show_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	mutex_lock(&fts_input_dev->mutex);
	count = sprintf(buf, "Log: %s\n", g_show_log ? "On" : "Off");
	mutex_unlock(&fts_input_dev->mutex);

	return count;
}
/************************************************************************
* Name: fts_show_log_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_show_log_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */

	mutex_lock(&fts_input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
	{
		FTS_DEBUG("enable show log info/error");
		g_show_log = 1;
	}
	else if (FTS_SYSFS_ECHO_OFF(buf))
	{
		FTS_DEBUG("disable show log info/error");
		g_show_log = 0;
	}
	mutex_unlock(&fts_input_dev->mutex);
	return count;
}
/************************************************************************
* Name: fts_dumpreg_store
* Brief:  no
* Input: device, device attribute, char buf, char count
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_dumpreg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* place holder for future use */
	return -EPERM;
}

/************************************************************************
* Name: fts_dumpreg_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_dumpreg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char tmp[256];
	int count = 0;
	u8 regvalue = 0;
	struct i2c_client *client;

	mutex_lock(&fts_input_dev->mutex);
	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(1);
	#endif
	client = container_of(dev, struct i2c_client, dev);

	fts_i2c_read_reg(client, FTS_REG_POWER_MODE, &regvalue);
	count += sprintf(tmp + count, "Power Mode:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_FW_VER, &regvalue);
	count += sprintf(tmp + count, "FW Ver:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &regvalue);
	count += sprintf(tmp + count, "Vendor ID:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_LCD_BUSY_NUM, &regvalue);
	count += sprintf(tmp + count, "LCD Busy Number:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_GESTURE_EN, &regvalue);
	count += sprintf(tmp + count, "Gesture Mode:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_CHARGER_MODE_EN, &regvalue);
	count += sprintf(tmp + count, "charge stat:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_INT_CNT, &regvalue);
	count += sprintf(tmp + count, "INT count:0x%02x\n", regvalue);

	fts_i2c_read_reg(client, FTS_REG_FLOW_WORK_CNT, &regvalue);
	count += sprintf(tmp + count, "ESD count:0x%02x\n", regvalue);
	#if FTS_ESDCHECK_EN
	fts_esdcheck_proc_busy(0);
	#endif
	memcpy(buf, tmp, count);
	mutex_unlock(&fts_input_dev->mutex);
	return count;
}


static ssize_t fts_lockdown_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct fts_ts_data *data = i2c_get_clientdata(client);
	size_t ret = -1;

	if(data->tp_lockdown_info_temp == NULL)
	{
		return  ret;
	}

	return snprintf(buf, FTS_LOCKDOWN_LEN - 1, "%s\n", data->tp_lockdown_info_temp);
}

static ssize_t fts_lockdown_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct fts_ts_data *data = dev_get_drvdata(dev);

	if (size > FTS_FW_NAME_MAX_LEN - 1)
		return -EINVAL;
	strlcpy(data->tp_lockdown_info_temp, buf, size);
	if (data->tp_lockdown_info_temp[size-1] == '\n')
		data->tp_lockdown_info_temp[size-1] = 0;

	return size;
}


/****************************************/
/* sysfs */
/* get the fw version
*   example:cat fw_version
*/
static DEVICE_ATTR(fts_fw_version, S_IRUGO|S_IWUSR, fts_tpfwver_show, fts_tpfwver_store);

/* upgrade from *.i
*   example: echo 1 > fw_update
*/
static DEVICE_ATTR(fts_fw_update, S_IRUGO|S_IWUSR, fts_fwupdate_show, fts_fwupdate_store);
/* read and write register
*   read example: echo 88 > rw_reg ---read register 0x88
*   write example:echo 8807 > rw_reg ---write 0x07 into register 0x88
*
*   note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(fts_rw_reg, S_IRUGO|S_IWUSR, fts_tprwreg_show, fts_tprwreg_store);
/*  upgrade from app.bin
*    example:echo "*_app.bin" > upgrade_app
*/
static DEVICE_ATTR(fts_upgrade_app, S_IRUGO|S_IWUSR, fts_fwupgradeapp_show, fts_fwupgradeapp_store);
static DEVICE_ATTR(fts_driver_version, S_IRUGO|S_IWUSR, fts_driverversion_show, fts_driverversion_store);
static DEVICE_ATTR(fts_dump_reg, S_IRUGO|S_IWUSR, fts_dumpreg_show, fts_dumpreg_store);
static DEVICE_ATTR(fts_show_log, S_IRUGO|S_IWUSR, fts_show_log_show, fts_show_log_store);
static DEVICE_ATTR(fts_module_config, S_IRUGO|S_IWUSR, fts_module_config_show, fts_module_config_store);
static DEVICE_ATTR(fts_hw_reset, S_IRUGO|S_IWUSR, fts_hw_reset_show, fts_hw_reset_store);
static DEVICE_ATTR(fts_irq, S_IRUGO|S_IWUSR, fts_irq_show, fts_irq_store);

static DEVICE_ATTR(tp_lock_down_info, S_IWUSR|S_IRUGO, fts_lockdown_show, fts_lockdown_store);


#if FTS_ESDCHECK_EN
static DEVICE_ATTR(fts_esd_check, S_IRUGO|S_IWUSR, fts_esdcheck_show, fts_esdcheck_store);
#endif

/* add your attr in here*/
static struct attribute *fts_attributes[] =
{
	&dev_attr_fts_fw_version.attr,
	&dev_attr_fts_fw_update.attr,
	&dev_attr_fts_rw_reg.attr,
	&dev_attr_fts_dump_reg.attr,
	&dev_attr_fts_upgrade_app.attr,
	&dev_attr_fts_driver_version.attr,
	&dev_attr_fts_show_log.attr,
	&dev_attr_fts_module_config.attr,
	&dev_attr_fts_hw_reset.attr,
	&dev_attr_fts_irq.attr,

	&dev_attr_tp_lock_down_info.attr,

	#if FTS_ESDCHECK_EN
	&dev_attr_fts_esd_check.attr,
	#endif
	NULL
};

static struct attribute_group fts_attribute_group =
{
	.attrs = fts_attributes
};

/************************************************************************
* Name: fts_create_sysfs
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_sysfs(struct i2c_client * client)
{
	int err;
	err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
	if (0 != err)
	{
		FTS_ERROR("[EX]: sysfs_create_group() failed!!");
		sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
		return -EIO;
	}
	else
	{
		FTS_INFO("[EX]: sysfs_create_group() succeeded!!");
	}
	return err;
}
/************************************************************************
* Name: fts_remove_sysfs
* Brief:  remove sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/
int fts_remove_sysfs(struct i2c_client * client)
{
	sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
	return 0;
}
