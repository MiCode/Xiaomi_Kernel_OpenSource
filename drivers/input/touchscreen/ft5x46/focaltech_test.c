
/*
 * FocalTech TouchScreen driver.
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
static struct proc_dir_entry *tp_data_dump_proc;
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




/************************************************************************
* Name: fts_i2c_write_reg
* Brief: write register
* Input: i2c info, reg address, reg value
* Output: no
* Return: fail <0
***********************************************************************/
int fts_i2c_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	u8 buf[2] = { 0 };

	buf[0] = regaddr;
	buf[1] = regvalue;
	return fts_i2c_write(client, buf, sizeof(buf));
}

/************************************************************************
* Name: fts_i2c_read_reg
* Brief: read register
* Input: i2c info, reg address, reg value
* Output: get reg value
* Return: fail <0
***********************************************************************/
int fts_i2c_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return fts_i2c_read(client, &regaddr, 1, regvalue, 1);
}

/************************************************************************
* Name: StartScan(Same function name as FT_MultipleTest)
* Brief:  Scan TP, do it before read Raw Data
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static int StartScan(struct i2c_client *client)
{
	u8 RegVal = 0x00;
	u8 times = 0;
	const u8 MaxTimes = 20;  //The longest wait 160ms
	u8 ret = -1;

	ret = fts_i2c_write_reg(client, 0x00, 0xC0);
	if (ret >= 0) {
		while (times++ < MaxTimes) {		//Wait for the scan to complete
			msleep(8);		//8ms
			ret = fts_i2c_read_reg(client, 0x00, &RegVal);
			if (RegVal == 0x40)
				break;
		}

		if (times > MaxTimes) {
			ret = -1;
		}
	}

	return ret;
}

int read_rawdata(struct i2c_client *client, int is_diff, s16 *data, int len)
{
	u8 reg = 0x6A;
	u8 regdata[1280] = { 0 };
	int remain_bytes;
	int pos = 0;
	int i = 0;
	int ret = 0;

	FTS_TEST_DBG("len=%d, is_diff=%d", len, is_diff);
	ret = fts_i2c_write_reg(client, 0x06, is_diff);
	if (ret < 0) {
		FTS_TEST_ERROR("read rawdata fail");
		return ret;
	}

	if (StartScan(client) < 0) {
		ret = -1;
		return ret;
	}

	ret = fts_i2c_write_reg(client, 0x01, 0xAD);

	if (len <= 256)
		ret = fts_i2c_read(client, &reg, 1, regdata, len);
	else{
		ret = fts_i2c_read(client, &reg, 1, regdata, 256);
		remain_bytes = len - 256;
		for (i = 1; remain_bytes > 0; i++) {
			if (remain_bytes > 256)
				ret = fts_i2c_read(client, &reg, 0, regdata + i * 256, 256);
			else
				ret = fts_i2c_read(client, &reg, 0, regdata + i * 256, remain_bytes);
			remain_bytes -= 256;
		}
	}

	for (i = 0; i < len; ) {
		data[pos++] = ((s16)(regdata[i]) << 8) + regdata[i+1];
		i += 2;
	}

	return ret;
}

static int32_t c_show(struct seq_file *m, void *v)
{
	int i = 0, j = 0, ret = 0;
	u8 val;
	s16 *data = NULL;
	s16 *data1 = NULL;
//	  s16 data[600] = { 0 };
//	  s16 data1[600] = { 0 };

	u8 txlen = 0;
	u8 rxlen = 0;

	data = (s16 *)vmalloc(PAGE_SIZE * 4);
	data1 = (s16 *)vmalloc(PAGE_SIZE * 4);
	if (!data || !data1) {
		ret = -EFAULT;
		goto out;
	}

	/* 0xEE = 1, not clb */
	FTS_TEST_ERROR("write data auto cal 0xee = 1");
	ret = fts_i2c_write_reg(fts_i2c_client, 0xEE, 1);
	if (ret < 0) {
		FTS_TEST_ERROR("write data auto cal fail");
		ret = -EFAULT;
		goto out;
	}

	/* Enter Factory Mode */
	FTS_TEST_ERROR("enter factory mode");
	ret = fts_i2c_write_reg(fts_i2c_client, 0x00, 0x40);
	do {
		ret = fts_i2c_read_reg(fts_i2c_client, 0x00, &val);
		if (val == 0x40)
			break;
		msleep(1);
	} while (i < 10);
	if (ret < 0) {
		FTS_TEST_ERROR("enter factory mode fail");
		ret = -EFAULT;
		goto out;
	}

	ret = fts_i2c_read_reg(fts_i2c_client, 0x02, &txlen);
	ret = fts_i2c_read_reg(fts_i2c_client, 0x03, &rxlen);
	/* read_rawdata */
	FTS_TEST_ERROR("read rawdata diff data");
	ret = read_rawdata(fts_i2c_client, 1, data, txlen * rxlen * 2);
	if (ret < 0) {
		FTS_TEST_ERROR("read rawdata fail");
		ret = -EFAULT;
		goto out;
	}

	seq_printf(m, "\nDIFF DATA\n");
	for (i = 0; (i < txlen) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < rxlen) && (j < RX_NUM_MAX); j++) {
			if (j == (rxlen - 1)) {
				seq_printf(m, "%5d, \n", data[rxlen * i + j]);
			} else {
				seq_printf(m, "%5d, ", data[rxlen * i + j]);
			}
		}
	}
	seq_printf(m, "\n\n");

	/* read_rawdata */
	FTS_TEST_ERROR("read rawdata raw data");
	ret = read_rawdata(fts_i2c_client, 0, data1, txlen * rxlen * 2);
	if (ret < 0) {
		FTS_TEST_ERROR("read rawdata fail");
		ret = -EFAULT;
		goto out;
	}

	seq_printf(m, "\nRAW DATA\n");
	for (i = 0; (i < txlen) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < rxlen) && (j < RX_NUM_MAX); j++) {
			if (j == (rxlen - 1)) {
				seq_printf(m, "%5d, \n", data1[rxlen * i + j]);
			} else {
				seq_printf(m, "%5d, ", data1[rxlen * i + j]);
			}
		}
	}
	seq_printf(m, "\n\n");

	/* Enter in work mode */
	FTS_TEST_ERROR("enter in work mode");
	ret = fts_i2c_write_reg(fts_i2c_client, 0x00, 0x00);
	if (ret < 0) {
		FTS_TEST_ERROR("write data auto cal fail");
		ret = -EFAULT;
		goto out;
	}
	do {
		ret = fts_i2c_read_reg(fts_i2c_client, 0x00, &val);
		if (val == 0x00)
			break;
		msleep(1);
	} while (i < 10);
out:

	//enable_irq(fts_i2c_client->irq);
	if (data) {
		vfree(data);
		data = NULL;
	}
	if (data1) {
		vfree(data1);
		data1 = NULL;
	}

	return 0;

}
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations fts_datadump_seq_ops = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= c_show
};

static int32_t fts_tp_data_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fts_datadump_seq_ops);
}

static const struct file_operations fts_datadump_fops = {
	.owner = THIS_MODULE,
	.open = fts_tp_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int fts_test_module_init(struct i2c_client *client)
{
	int err = 0;
	FTS_TEST_DBG("[focal] %s ",  FOCALTECH_TEST_INFO);
	fts_selftest_proc = proc_create("tp_selftest", 0644,
			NULL, &tp_selftest_ops);

	tp_data_dump_proc = proc_create("tp_data_dump", 0444, NULL, &fts_datadump_fops);

	return err;
}
int fts_test_module_exit(struct i2c_client *client)
{
	if (fts_selftest_proc)
		remove_proc_entry("tp_selftest", NULL);
	if (tp_data_dump_proc)
		remove_proc_entry("tp_data_dump", NULL);

	return 0;
}
