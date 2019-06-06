/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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

/************************************************************************
*
* File Name: focaltech_test.c
*
* Author:     Software Department, FocalTech
*
* Created: 2016-08-01
*
* Modify:
*
* Abstract: create char device and proc node for  the comm between APK and TP
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
#include <linux/delay.h>

#include "../../focaltech_core.h"
#include "../include/focaltech_test_main.h"
#include "../include/focaltech_test_ini.h"
#include <linux/seq_file.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define IC_TEST_VERSION  "Test version: V1.0.0--2016-12-28, (sync version of FT_MultipleTest: V4.0.0.0 ------ 2016-07-18)"


/*add by shenwenbin for selftest 20190111 begin*/
#define FTS_INI_FILE_PATH "/system/etc/"
#define FTS_TEST_DATA_FILE_PATH "/mnt/sdcard/"
/*add by shenwenbin for selftest 20190111 end*/

#define FTS_TEST_BUFFER_SIZE        80*1024
#define FTS_TEST_PRINT_SIZE         128
#define WRITE_BUF_SIZE              512
#define INFO_LEN                    5
#define PROC_TP                     "tp_selftest"

int g_autoTestResult = 0;

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/


/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/


/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int fts_test_get_ini_size(char *config_name);
static int fts_test_read_ini_data(char *config_name, char *config_buf);
static int fts_test_save_test_data(char *file_name, char *data_buf, int iLen);
static int fts_test_get_testparam_from_ini(char *config_name);
static int fts_test_entry(char *ini_file_name);

static int fts_test_i2c_read(unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen);
static int fts_test_i2c_write(unsigned char *writebuf, int writelen);
static struct proc_dir_entry *proc_tpselftest;
unsigned char *writeInfo;

/*****************************************************************************
* functions body
*****************************************************************************/
#if 0

extern struct i2c_client* fts_i2c_client;
extern int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);
extern int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen);
#endif
static int fts_test_i2c_read(unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen)
{
	int iret = -1;
#if 1


	iret = fts_i2c_read(fts_i2c_client, writebuf, writelen, readbuf, readlen);
#else
	iret = fts_i2c_read(writebuf, writelen, readbuf, readlen);
#endif

	return iret;

}

static int fts_test_i2c_write(unsigned char *writebuf, int writelen)
{
	int iret = -1;
#if 1


	iret = fts_i2c_write(fts_i2c_client, writebuf, writelen);
#else
	iret = fts_i2c_write(writebuf, writelen);
#endif

	return iret;
}


static int fts_test_get_ini_size(char *config_name)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;

	off_t fsize = 0;
	char filepath[128];

	FTS_TEST_FUNC_ENTER();

	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile))
	{
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;

	fsize = inode->i_size;
	filp_close(pfile, NULL);

	FTS_TEST_FUNC_ENTER();

	return fsize;
}


static int fts_test_read_ini_data(char *config_name, char *config_buf)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;

	off_t fsize = 0;
	char filepath[128];
	loff_t pos = 0;
	mm_segment_t old_fs;

	FTS_TEST_FUNC_ENTER();

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);
	if (NULL == pfile)
	{
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile))
	{
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;

	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, config_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	FTS_TEST_FUNC_EXIT();
	return 0;
}


static int fts_test_save_test_data(char *file_name, char *data_buf, int iLen)
{
	struct file *pfile = NULL;

	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	FTS_TEST_FUNC_ENTER();

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_TEST_DATA_FILE_PATH, file_name);
	if (NULL == pfile)
	{

		pfile = filp_open(filepath, O_TRUNC|O_CREAT|O_RDWR, 0);
	}
	if (IS_ERR(pfile))
	{
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(pfile, data_buf, iLen, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	FTS_TEST_FUNC_EXIT();
	return 0;
}


static int fts_test_get_testparam_from_ini(char *config_name)
{
	char *pcfiledata = NULL;
	int ret = 0;
	int inisize = 0;

	FTS_TEST_FUNC_ENTER();

	inisize = fts_test_get_ini_size(config_name);
	FTS_TEST_DBG("ini_size = %d ", inisize);
	if (inisize <= 0)
	{
		FTS_TEST_ERROR("%s ERROR:Get firmware size failed",  __func__);
		return -EIO;
	}

	pcfiledata = fts_malloc(inisize + 1);
	if (NULL == pcfiledata)
	{
		FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		return -1;
	}

	memset(pcfiledata, 0, inisize + 1);

	if (fts_test_read_ini_data(config_name, pcfiledata))
	{
		FTS_TEST_ERROR(" - ERROR: fts_test_read_ini_data failed" );
		fts_free(pcfiledata);
		pcfiledata = NULL;

		return -EIO;
	}
	else
	{
		FTS_TEST_DBG("fts_test_read_ini_data successful");
	}

	ret = set_param_data(pcfiledata);

	fts_free(pcfiledata);
	pcfiledata = NULL;

	FTS_TEST_FUNC_EXIT();

	if (ret < 0)
		return ret;

	return 0;
}




static int fts_test_entry(char *ini_file_name)
{
	/* place holder for future use */
	char cfgname[128];
	char *testdata = NULL;
	char *printdata = NULL;
	int iTestDataLen=0;
	int ret = 0;
	int retI2C = 0;
	int icycle = 0, i =0;
	int print_index = 0;


	FTS_TEST_FUNC_ENTER();
	FTS_TEST_DBG("ini_file_name:%s.", ini_file_name);
	/*Used to obtain the test data stored in the library, pay attention to the size of the distribution space.*/
	FTS_TEST_DBG("Allocate memory, size: %d", FTS_TEST_BUFFER_SIZE);
	testdata = fts_malloc(FTS_TEST_BUFFER_SIZE);
	if (NULL == testdata)
	{
		FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		return -1;
	}
	printdata = fts_malloc(FTS_TEST_PRINT_SIZE);
	if (NULL == printdata)
	{
		FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		return -1;
	}
	/*Initialize the platform related I2C read and write functions*/

#if 0
	init_i2c_write_func(fts_i2c_write);
	init_i2c_read_func(fts_i2c_read);
#else
	init_i2c_write_func(fts_test_i2c_write);
	retI2C = init_i2c_read_func(fts_test_i2c_read);
#endif

	/*Initialize pointer memory*/
	ret = focaltech_test_main_init();
	if (ret < 0)
	{
		FTS_TEST_ERROR("focaltech_test_main_init() error.");
		goto TEST_ERR;
	}

	/*Read parse configuration file*/
	memset(cfgname, 0, sizeof(cfgname));
	sprintf(cfgname, "%s", ini_file_name);
	FTS_TEST_DBG("ini_file_name = %s", cfgname);

	fts_test_funcs();

	if (fts_test_get_testparam_from_ini(cfgname) <0)
	{
		FTS_TEST_ERROR("get testparam from ini failure");
		goto TEST_ERR;
	}


	if ((g_ScreenSetParam.iSelectedIC >> 4  != FTS_CHIP_TEST_TYPE >> 4))
	{
		FTS_TEST_ERROR("Select IC and Read IC from INI does not match ");
		goto TEST_ERR;
	}


	/*Start testing according to the test configuration*/
	if (true == start_test_tp())
	{
		TestResultLen += sprintf(TestResult+TestResultLen, "Tp test pass. \n\n");
		writeInfo = "2";
		if (retI2C < 0) {
			writeInfo = "1";
		} else {
			writeInfo = "2";
		}
		g_autoTestResult = 1;
		FTS_TEST_INFO("tp test pass");
	}

	else
	{
		TestResultLen += sprintf(TestResult+TestResultLen, "Tp test failure. \n\n");
		writeInfo = "1";
		g_autoTestResult = 0;
		FTS_TEST_INFO("tp test failure");
	}


	/*Gets the number of tests in the test library and saves it*/
	iTestDataLen = get_test_data(testdata);


	icycle = 0;
	/*Print test data packets */
	FTS_TEST_DBG("print test data: \n");
	for (i = 0; i < iTestDataLen; i++)
	{
		if (('\0' == testdata[i])
			||(icycle == FTS_TEST_PRINT_SIZE -2)
			||(i == iTestDataLen-1)
		   )
		{
			if (icycle == 0)
			{
				print_index++;
			}
			else
			{
				memcpy(printdata, testdata + print_index, icycle);
				printdata[FTS_TEST_PRINT_SIZE-1] = '\0';
				FTS_TEST_DBG("%s", printdata);
				print_index += icycle;
				icycle = 0;
			}
		}
		else
		{
			icycle++;
		}
	}
	FTS_TEST_DBG("\n");



	fts_test_save_test_data("testdata.csv", testdata, iTestDataLen);
	fts_test_save_test_data("testresult.txt", TestResult, TestResultLen);

	/*add by shenwenbin for selftest 20190111 begin*/
	if(g_autoTestResult == 0)
	{
	    fts_test_save_test_data("testdata_fail.csv", testdata, iTestDataLen);
	    fts_test_save_test_data("testresult_fail.txt", TestResult, TestResultLen);
	}
	/*add by shenwenbin for selftest 20190111 end*/

	/*Release memory */
	focaltech_test_main_exit();



	if (NULL != testdata) fts_free(testdata);
	if (NULL != printdata) fts_free(printdata);

	FTS_TEST_FUNC_EXIT();

	return 0;

TEST_ERR:
	if (NULL != testdata) fts_free(testdata);
	if (NULL != printdata) fts_free(printdata);

	FTS_TEST_FUNC_EXIT();

	return -1;
}

/************************************************************************
* Name: fts_test_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

/************************************************************************
* Name: fts_test_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[128] = {0};
	struct i2c_client *client = fts_i2c_client;

	FTS_TEST_FUNC_ENTER();

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';
	printk("fwname:%s.", fwname);

	mutex_lock(&fts_input_dev->mutex);

	disable_irq(client->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(DISABLE);
#endif
	fts_test_entry( fwname);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(ENABLE);
#endif
	enable_irq(client->irq);

	mutex_unlock(&fts_input_dev->mutex);

	FTS_TEST_FUNC_EXIT();

	return count;
}
/*  upgrade from app.bin
*    example:echo "***.ini" > fts_test
*/
static DEVICE_ATTR(fts_test, S_IRUGO|S_IWUSR, fts_test_show, fts_test_store);

/* add your attr in here*/
static struct attribute *fts_test_attributes[] =
{
	&dev_attr_fts_test.attr,
	NULL
};

static struct attribute_group fts_test_attribute_group =
{
	.attrs = fts_test_attributes
};


int fts_test_init(struct i2c_client *client)
{
	int err=0;

	FTS_TEST_FUNC_ENTER();

	FTS_TEST_INFO("[focal] %s ",  IC_TEST_VERSION);


	err = sysfs_create_group(&client->dev.kobj, &fts_test_attribute_group);
	if (0 != err)
	{
		FTS_TEST_ERROR( "[focal] %s() - ERROR: sysfs_create_group() failed.",  __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
		return -EIO;
	}
	else
	{
		FTS_TEST_DBG("[focal] %s() - sysfs_create_group() succeeded.", __func__);
	}

	FTS_TEST_FUNC_EXIT();

	return err;
}
int fts_test_exit(struct i2c_client *client)
{
	FTS_TEST_FUNC_ENTER();
	sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

static int proc_tp_selftest_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", writeInfo);
	return 0;
}

static int proc_tp_selftest_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_tp_selftest_show, NULL);

}

static ssize_t proc_tp_selftest_write(struct file *file, const char __user *buffer,  size_t count, loff_t *f_pos)
{
	unsigned char writebuf[WRITE_BUF_SIZE];
	int buflen = count;
	char fwname[128] = {0};
	struct i2c_client *client = fts_i2c_client;
	struct fts_ts_data *data1;
	int ret;
	data1 = devm_kzalloc(&client->dev, sizeof(struct fts_ts_data), GFP_KERNEL);

	/*add by shenwenbin for selftest 20190111 begin*/
	if ( count > INFO_LEN)
		return -EFAULT;
	FTS_DEBUG("<<-proc_tp_selftest_write->>-----buflen: %d\n", buflen);
	if (copy_from_user(&writebuf, buffer, buflen))
	{
		FTS_DEBUG("[APK]: copy from user error!!");
		return -EFAULT;
	}
	FTS_DEBUG("<<-proc_tp_selftest_write->>-----writebuf: %s\n", writebuf);


	FTS_TEST_FUNC_ENTER();

	memset(fwname, 0, sizeof(fwname));
#if (FTS_GET_VENDOR_ID_NUM >= 2)
	sprintf(fwname, "Conf_MultipleSelfTest_0x%02x.ini", fts_wq_data->fw_vendor_id);
	fwname[30] = '\0';
#else
	sprintf(fwname, "Conf_MultipleSelfTest.ini");
	fwname[25] = '\0';
#endif
	FTS_DEBUG("<<-proc_tp_selftest_write->>------fwname:%s.\n", fwname);
	/*add by shenwenbin for selftest 20190111 end*/

	mutex_lock(&fts_input_dev->mutex);

	disable_irq(client->irq);

   #if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	   fts_esdcheck_switch(DISABLE);
   #endif
	 ret =  fts_test_entry(fwname);
	 if (ret < 0){
		 writeInfo = "0";
	 }

   #if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	   fts_esdcheck_switch(ENABLE);
   #endif
	   enable_irq(client->irq);

   mutex_unlock(&fts_input_dev->mutex);

   FTS_TEST_FUNC_EXIT();
	return count;
}

static const struct file_operations fts_proc_tpops =
{
	.owner  = THIS_MODULE,
	.open   = proc_tp_selftest_open,
	.read	= seq_read,
	.write  = proc_tp_selftest_write,
	.llseek	= seq_lseek,
	.release	= single_release,
};

int init_tp_selftest(struct i2c_client * client)
{
	proc_tpselftest = proc_create(PROC_TP, 0777, NULL, &fts_proc_tpops);
	if (NULL == proc_tpselftest)
	{
		FTS_ERROR("Couldn't create tp_selftest proc entry!");
		return -ENOMEM;
	}
	return 0;
}

