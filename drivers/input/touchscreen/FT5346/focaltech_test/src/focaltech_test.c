/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
#include <linux/delay.h>

#include <linux/input/ft5346.h>
#include "../include/focaltech_test_main.h"
#include "../include/focaltech_test_ini.h"


/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/

#define IC_TEST_VERSION  "Test version: V1.0.0--2016-08-01, (sync version of FT_MultipleTest: V4.0.0.0 ------ 2016-07-18)"

#define FTS_INI_FILE_PATH "/system/etc/"
#define FTS_CSV_FILE_PATH "/storage/"

#define FTS_TEST_BUFFER_SIZE        80*1024
#define FTS_TEST_PRINT_SIZE     128
/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/


/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/


/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int fts_test_get_ini_size(char *config_name);
static int fts_test_read_ini_data(char *config_name, char *config_buf);
static int fts_test_save_test_data(char *file_name, char *data_buf, int iLen);
static int fts_test_get_testparam_from_ini(char *config_name);

/*******************************************************************************
* functions body
*******************************************************************************/
extern struct i2c_client *fts_i2c_client;
extern void disable_irq(unsigned int irq);
extern void enable_irq(unsigned int irq);

static DEFINE_MUTEX(fts_mutex);
static int fts_test_i2c_read(unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen)
{
	int ret = -1;
	if (writelen > 0) {
		 struct i2c_msg msgs[] = {
			{
				.addr = fts_i2c_client->addr,
				.flags = 0,
				.len = writelen,
				.buf = writebuf,
			},
			{
				.addr = fts_i2c_client->addr,
				.flags = I2C_M_RD,
				.len = readlen,
				.buf = readbuf,
			},
		 };
		 ret = i2c_transfer(fts_i2c_client->adapter, msgs, 2);
		 if (ret < 0)
			dev_err(&fts_i2c_client->dev, "%s: i2c read error.\n", __func__);
	} else {
		 struct i2c_msg msgs[] = {
			{
				.addr = fts_i2c_client->addr,
				.flags = I2C_M_RD,
				.len = readlen,
				.buf = readbuf,
			},
		 };
		 ret = i2c_transfer(fts_i2c_client->adapter, msgs, 1);
		 if (ret < 0)
			dev_err(&fts_i2c_client->dev, "%s:i2c read error.\n", __func__);
	}

	return ret;

}

static int fts_test_i2c_write(unsigned char *writebuf, int writelen)
{
	int ret = -1;

	struct i2c_msg msgs[] = {
		 {
			.addr = fts_i2c_client->addr,
			.flags = 0,
			.len = writelen,
			.buf = writebuf,
		 },
	};
	ret = i2c_transfer(fts_i2c_client->adapter, msgs, 1);
	if (ret < 0)
		 dev_err(&fts_i2c_client->dev, "%s: i2c write error.\n", __func__);

	return ret;
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

	if (IS_ERR(pfile)) {
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
	if (NULL == pfile) {
		 pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
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
	sprintf(filepath, "%s%s", FTS_CSV_FILE_PATH, file_name);
	if (NULL == pfile) {
		 pfile = filp_open(filepath, O_CREAT|O_RDWR, 0);
	}
	if (IS_ERR(pfile)) {
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
	if (inisize <= 0) {
		 FTS_TEST_ERROR("%s ERROR:Get firmware size failed",  __func__);
		 return -EIO;
	}

	pcfiledata = fts_malloc(inisize + 1);
	if (NULL == pcfiledata) {
		 FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		 return -EPERM;
	}

	memset(pcfiledata, 0, inisize + 1);

	if (fts_test_read_ini_data(config_name, pcfiledata)) {
		 FTS_TEST_ERROR(" - ERROR: fts_test_read_ini_data failed");
		 fts_free(pcfiledata);
		 pcfiledata = NULL;

		 return -EIO;
	} else {
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




static int fts_test_entry(char *ini_file_name, char *bufdest, ssize_t *pinumread)
{
	/* place holder for future use */
	char cfgname[128];
	char *testdata = NULL;
	char *printdata = NULL;
	int iTestDataLen = 0;
	int ret = 0;
	int icycle = 0, i = 0;
	int print_index = 0;
	int test_result = -1;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_DBG("ini_file_name:%s.", ini_file_name);
	/*Used to obtain the test data stored in the library, pay attention to the size of the distribution space.*/
	FTS_TEST_DBG("Allocate memory, size: %d", FTS_TEST_BUFFER_SIZE);
	testdata = fts_malloc(FTS_TEST_BUFFER_SIZE);
	if (NULL == testdata) {
		 FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		 return -EPERM;
	}
	printdata = fts_malloc(FTS_TEST_PRINT_SIZE);
	if (NULL == printdata) {
		 FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		 return -EPERM;
	}
	/*Initialize the platform related I2C read and write functions*/

	init_i2c_write_func(fts_test_i2c_write);
	init_i2c_read_func(fts_test_i2c_read);

	/*Initialize pointer memory*/
	ret = focaltech_test_main_init();
	if (ret < 0) {
		 FTS_TEST_ERROR("focaltech_test_main_init() error.");
		 goto TEST_ERR;
	}

	/*Read parse configuration file*/
	memset(cfgname, 0, sizeof(cfgname));
	sprintf(cfgname, "%s", ini_file_name);
	FTS_TEST_DBG("ini_file_name = %s", cfgname);

	fts_test_funcs();

	if (fts_test_get_testparam_from_ini(cfgname) < 0) {
		 FTS_TEST_ERROR("get testparam from ini failure");
		 goto TEST_ERR;
	}

	/*Start testing according to the test configuration*/
	if (true == start_test_tp()) {

		 FTS_TEST_INFO("tp test pass");
		test_result = 0;
	}

	else {

		 FTS_TEST_INFO("tp test failure");
		test_result = -1;
	}


	/*Gets the number of tests in the test library and saves it*/
	iTestDataLen = get_test_data(testdata);


	icycle = 0;
	/*Print test data packets */
	FTS_TEST_DBG("print test data: \n");
	for (i = 0; i < iTestDataLen; i++) {
		 if (('\0' == testdata[i])
			|| (icycle == FTS_TEST_PRINT_SIZE - 2)
			|| (i == iTestDataLen-1)
		    ) {
			if (icycle == 0) {
				print_index++;
			} else {
				memcpy(printdata, testdata + print_index, icycle);
				printdata[FTS_TEST_PRINT_SIZE-1] = '\0';
				FTS_TEST_DBG("%s", printdata);
				print_index += icycle;
				icycle = 0;
			}
		 } else {
			icycle++;
		 }
	}
	FTS_TEST_DBG("\n");



	fts_test_save_test_data("testdata.csv", testdata, iTestDataLen);
	fts_test_save_test_data("testresult.txt", TestResult, TestResultLen);


	/*Release memory */
	focaltech_test_main_exit();



	if (NULL != testdata)
		fts_free(testdata);
	if (NULL != printdata)
		fts_free(printdata);

	FTS_TEST_FUNC_EXIT();

	return test_result;

TEST_ERR:
	if (NULL != testdata)
		fts_free(testdata);
	if (NULL != printdata)
		fts_free(printdata);

	FTS_TEST_FUNC_EXIT();

	return -EPERM;
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
	char *bufdest = NULL;
	ssize_t pinumread = 0;

	FTS_TEST_FUNC_ENTER();

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';
	FTS_TEST_DBG("fwname:%s.", fwname);
	mutex_lock(&fts_mutex);


	disable_irq(client->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(DISABLE);
#endif

	fts_test_entry("ft5346_ds.ini", bufdest, &pinumread);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(ENABLE);
#endif

	enable_irq(client->irq);
	mutex_unlock(&fts_mutex);


	FTS_TEST_FUNC_EXIT();

	return count;
}
/*  upgrade from app.bin
*    example:echo "***.ini" > fts_test
*/
static DEVICE_ATTR(fts_test, S_IRUGO|S_IWUSR, fts_test_show, fts_test_store);

/* add your attr in here*/
static struct attribute *fts_test_attributes[] = {
	&dev_attr_fts_test.attr,
	NULL
};

static struct attribute_group fts_test_attribute_group = {
	.attrs = fts_test_attributes
};


int fts_test_init(struct i2c_client *client)
{
	int err = 0;

	FTS_TEST_FUNC_ENTER();

	FTS_TEST_INFO("[focal] %s ",  IC_TEST_VERSION);


	err = sysfs_create_group(&client->dev.kobj, &fts_test_attribute_group);
	if (0 != err) {
		 FTS_TEST_ERROR("[focal] %s() - ERROR: sysfs_create_group() failed.",  __func__);
		 sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
		 return -EIO;
	} else {
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

int fts_open_short_test(char *ini_file_name, char *bufdest, ssize_t *pinumread)
{
	return fts_test_entry(ini_file_name, bufdest, pinumread);
}



EXPORT_SYMBOL(fts_open_short_test);

