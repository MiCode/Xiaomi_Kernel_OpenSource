
/*
 * FocalTech TouchScreen driver.
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

 /************************************************************************
* File Name: focaltech_test.c
* Author:	  Software Department, FocalTech
* Created: 2016-03-24
* Modify:
* Abstract: create char device and proc node for  the comm between APK and TP
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include "focaltech_test.h"
#include "focaltech_test_ft8716.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_TEST_INFO  "focaltech_test.c:V1.1.0 2016-05-19"
#define DEVIDE_MODE_ADDR	0x00
#define FTS_MALLOC_TYPE			1
static struct proc_dir_entry *fts_selftest_proc;
/*******************************************************************************
* functions body
*******************************************************************************/
int fts_test_result = RESULT_INVALID;

static int fts_i2c_read_test(unsigned char *writebuf, int writelen,
		unsigned char *readbuf, int readlen)
{
	int iret = -1;

	iret = fts_i2c_read(fts_i2c_client, writebuf,
			writelen, readbuf, readlen);
	return iret;

}

static int fts_i2c_write_test(unsigned char *writebuf, int writelen)
{
	int iret = -1;

	iret = fts_i2c_write(fts_i2c_client, writebuf, writelen);
	return iret;
}

void focal_msleep(int ms)
{
	msleep(ms);
}
void SysDelay(int ms)
{
	msleep(ms);
}
int focal_abs(int value)
{
	if (value < 0)
		value = 0 - value;
	return value;
}
void *fts_malloc(size_t size)
{
	if (FTS_MALLOC_TYPE == kmalloc_mode)
		return kmalloc(size, GFP_ATOMIC);
	if (FTS_MALLOC_TYPE == vmalloc_mode)
		return vmalloc(size);
	return NULL;
}
void fts_free(void *p)
{
	if (FTS_MALLOC_TYPE == kmalloc_mode)
		kfree(p);
	if (FTS_MALLOC_TYPE == vmalloc_mode)
		vfree(p);
}

/************************************************************************
* Name: ReadReg(Same function name as FT_MultipleTest)
* Brief:  Read Register
* Input: RegAddr
* Output: RegData
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
int ReadReg(unsigned char RegAddr, unsigned char *RegData)
{
	int iRet;

	iRet = fts_i2c_read_test(&RegAddr, 1, RegData, 1);

	if (iRet >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}
/************************************************************************
* Name: WriteReg(Same function name as FT_MultipleTest)
* Brief:  Write Register
* Input: RegAddr, RegData
* Output: null
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
int WriteReg(unsigned char RegAddr, unsigned char RegData)
{
	int iRet;
	unsigned char cmd[2] = {0};

	cmd[0] = RegAddr;
	cmd[1] = RegData;
	iRet = fts_i2c_write_test(cmd, 2);

	if (iRet >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}
/************************************************************************
* Name: Comm_Base_IIC_IO(Same function name as FT_MultipleTest)
* Brief:  Write/Read Data by IIC
* Input: pWriteBuffer, iBytesToWrite, iBytesToRead
* Output: pReadBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int iBytesToWrite,
		unsigned char *pReadBuffer, int iBytesToRead)
{
	int iRet;

	iRet = fts_i2c_read_test(pWriteBuffer, iBytesToWrite,
			pReadBuffer, iBytesToRead);

	if (iRet >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}
/************************************************************************
* Name: EnterWork(Same function name as FT_MultipleTest)
* Brief:  Enter Work Mode
* Input: null
* Output: null
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char EnterWork(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	FTS_TEST_DBG("");
	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
	if (ReCode == ERROR_CODE_OK) {
		if (((RunState>>4)&0x07) == 0x00)
			ReCode = ERROR_CODE_OK;
		else {
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0);
			if (ReCode == ERROR_CODE_OK) {
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
				if (ReCode == ERROR_CODE_OK) {
					if (((RunState>>4)&0x07) == 0x00)
						ReCode = ERROR_CODE_OK;
					else
						ReCode = ERROR_CODE_COMM_ERROR;
				}
			} else
				pr_err("%s,error\n", __func__);
		}
	}

	return ReCode;
}
/************************************************************************
* Name: EnterFactory
* Brief:  enter Fcatory Mode
* Input: null
* Output: null
* Return: Comm Code. Code = 0 is OK, else fail.
***********************************************************************/
unsigned char EnterFactory(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	FTS_TEST_DBG("");
	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
	if (ReCode == ERROR_CODE_OK) {
		if (((RunState>>4)&0x07) == 0x04)
			ReCode = ERROR_CODE_OK;
		else {
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0x40);
			if (ReCode == ERROR_CODE_OK) {
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);
				if (ReCode == ERROR_CODE_OK) {
					if (((RunState>>4)&0x07) == 0x04)
						ReCode = ERROR_CODE_OK;
					else
						ReCode = ERROR_CODE_COMM_ERROR;
				}
			}
		}
	} else
		FTS_TEST_ERR("EnterFactory read DEVIDE_MODE_ADDR error 1.");

	FTS_TEST_DBG(" END");
	return ReCode;
}
int fts_i2c_test(struct i2c_client *client)
{
	int retry = 5;
	int ReCode = ERROR_CODE_OK;
	unsigned char chip_id = 0;

	if (client == NULL)
		return RESULT_INVALID;
	fts_test_result = RESULT_INVALID;
	while (retry--) {
		ReCode = ReadReg(REG_CHIP_ID, &chip_id);
		if (ReCode == ERROR_CODE_OK)
			return RESULT_PASS;
		dev_err(&client->dev, "GTP i2c test failed time %d.\n", retry);
		msleep(20);
	}
	if (ReCode != ERROR_CODE_OK)
		return RESULT_NG;
	return RESULT_INVALID;
}


int fts_tp_selftest_open(struct inode *inode, struct file *file)
{
	return 0;
}

ssize_t fts_tp_selftest_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	char tmp[5];
	int cnt;

	if (*pos != 0)
		return 0;

	cnt = snprintf(tmp, sizeof(fts_test_result), "%d\n", fts_test_result);
	if (copy_to_user(buf, tmp, strlen(tmp)))
		return -EFAULT;
	*pos += cnt;
	return cnt;
}

ssize_t fts_tp_selftest_write(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	char tmp[6];
	int retval;

	if (!fts_i2c_client || count > sizeof(tmp)) {
		retval = -EINVAL;
		fts_test_result = RESULT_INVALID;
		goto out;
	}
	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		fts_test_result = RESULT_INVALID;
		goto out;
	}
	disable_irq(fts_i2c_client->irq);
	if (!strncmp(tmp, "short", 5))
		fts_test_result = FT8716_TestItem_ShortCircuitTest(fts_i2c_client);
	else if (!strncmp(tmp, "open", 4))
		fts_test_result = FT8716_TestItem_OpenTest(fts_i2c_client);
	else if (!strncmp(tmp, "i2c", 3))
		fts_test_result = fts_i2c_test(fts_i2c_client);
	retval = fts_test_result;
	EnterWork();
	enable_irq(fts_i2c_client->irq);
out:
	if (retval >= 0)
		retval = count;
	return retval;
}

int fts_tp_selftest_release(struct inode *inode, struct file *file)
{
	return 0;
}
static const struct file_operations tp_selftest_ops = {
	.open		= fts_tp_selftest_open,
	.read		= fts_tp_selftest_read,
	.write		= fts_tp_selftest_write,
	.release	= fts_tp_selftest_release,
};

int fts_test_module_init(struct i2c_client *client)
{

	FTS_TEST_DBG("[focal] %s ",  FOCALTECH_TEST_INFO);
	fts_selftest_proc = proc_create("tp_selftest", 0,
			NULL, &tp_selftest_ops);
	return 0;
}
int fts_test_module_exit(struct i2c_client *client)
{
	FTS_TEST_DBG("");
	if (fts_selftest_proc)
		remove_proc_entry("tp_selftest", NULL);
	return 0;
}
