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
#include "focaltech_test.h"
#include "focaltech_core.h"
#include "../focaltech_flash.h"
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/kernel.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct fts_test_data test_data;
#define CTP_PARENT_PROC_NAME  "touchscreen"
#define CTP_OPEN_PROC_NAME        "ctp_openshort_test"
#define CTP_SELF_TEST "tp_selftest"

#if FTS_LOCK_DOWN_INFO
char focal_tp_lockdown_info[128];
//static u8 lockdown_info[8];
u8 focal_lcm_maker;
#define FTS_PROC_LOCKDOWN_FILE "lockdown_info"
static int fts_lockdown_proc_show(struct seq_file *file, void* data)
{
	char temp[40] = {0};

	sprintf(temp, "%s\n", focal_tp_lockdown_info);//tp_lockdown_info字符串格式化输出到temp中
	seq_printf(file, "%s\n", temp);//打印出这个对象节点的信息

	return 0;
}

static int fts_lockdown_proc_open (struct inode* inode, struct file* file)
{
	return single_open(file, fts_lockdown_proc_show, inode->i_private);
}

static const struct file_operations fts_lockdown_proc_fops =
{
	.open = fts_lockdown_proc_open,
	.read = seq_read,
};

#endif

/*****************************************************************************
* Static variables
*****************************************************************************/
static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos);
static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos);
static ssize_t ctp_selftest_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos);
static ssize_t ctp_selftest_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos);

static const struct file_operations ctp_open_procs_fops =
	{
		.write = ctp_open_proc_write,
		.read = ctp_open_proc_read,
		.owner = THIS_MODULE,
	};
static struct proc_dir_entry *ctp_device_proc = NULL;
#if 1
static int selftest_result = 0;
static struct proc_dir_entry *ctp_selftest_proc = NULL;
static const struct file_operations ctp_selftest_procs_fops =
		{
			.write = ctp_selftest_proc_write,
			.read = ctp_selftest_proc_read,
			.owner = THIS_MODULE,
		};
#endif
/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
void sys_delay(int ms)
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
    return vmalloc(size);
}

void fts_free_proc(void *p)
{
    return vfree(p);
}

/********************************************************************
 * test i2c read/write interface
 *******************************************************************/
int fts_test_i2c_read(u8 *writebuf, int writelen, u8 *readbuf, int readlen)
{
    int ret = 0;
#if 1
    if (NULL == fts_data) {
        FTS_TEST_ERROR("fts_data is null, no test");
        return -EINVAL;
    }
    ret = fts_i2c_read(fts_data->client, writebuf, writelen, readbuf, readlen);
#else
    ret = fts_i2c_read(writebuf, writelen, readbuf, readlen);
#endif

    if (ret < 0)
        return ret;
    else
        return 0;
}

int fts_test_i2c_write(u8 *writebuf, int writelen)
{
    int ret = 0;
#if 1
    if (NULL == fts_data) {
        FTS_TEST_ERROR("fts_data is null, no test");
        return -EINVAL;
    }
    ret = fts_i2c_write(fts_data->client, writebuf, writelen);
#else
    ret = fts_i2c_write(writebuf, writelen);
#endif

    if (ret < 0)
        return ret;
    else
        return 0;
}

int read_reg(u8 addr, u8 *val)
{
    return fts_test_i2c_read(&addr, 1, val, 1);
}

int write_reg(u8 addr, u8 val)
{
    int ret;
    u8 cmd[2] = {0};

    cmd[0] = addr;
    cmd[1] = val;
    ret = fts_test_i2c_write(cmd, 2);

    return ret;
}

/********************************************************************
 * test global function enter work/factory mode
 *******************************************************************/
int enter_work_mode(void)
{
    int ret = 0;
    u8 mode = 0;
    int i = 0;
    int j = 0;

    FTS_TEST_FUNC_ENTER();

    ret = read_reg(DEVIDE_MODE_ADDR, &mode);
    if ((0 == ret) && (((mode >> 4) & 0x07) == 0x00))
        return 0;

    for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
        ret = write_reg(DEVIDE_MODE_ADDR, 0x00);
        if (0 == ret) {
            for (j = 0; j < 20; j++) {
                ret = read_reg(DEVIDE_MODE_ADDR, &mode);
                if ((0 == ret) && (((mode >> 4) & 0x07) == 0x00))
                    return 0;
                else
                    sys_delay(16);
            }
        }

        sys_delay(50);
    }

    if (i >= ENTER_WORK_FACTORY_RETRIES) {
        FTS_TEST_ERROR("Enter work mode fail");
        return -EIO;
    }

    FTS_TEST_FUNC_EXIT();
    return 0;
}

int enter_factory_mode(void)
{
    int ret = 0;
    u8 mode = 0;
    int i = 0;
    int j = 0;

    ret = read_reg(DEVIDE_MODE_ADDR, &mode);
    if ((0 == ret) && (((mode >> 4) & 0x07) == 0x04))
        return 0;

    for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
        ret = write_reg(DEVIDE_MODE_ADDR, 0x40);
        if (0 == ret) {
            for (j = 0; j < 20; j++) {
                ret = read_reg(DEVIDE_MODE_ADDR, &mode);
                if ((0 == ret) && (((mode >> 4) & 0x07) == 0x04)) {
                    sys_delay(200);
                    return 0;
                } else
                    sys_delay(16);
            }
        }

        sys_delay(50);
    }

    if (i >= ENTER_WORK_FACTORY_RETRIES) {
        FTS_TEST_ERROR("Enter factory mode fail");
        return -EIO;
    }

    return 0;
}

/************************************************************************
* Name: fts_i2c_read_write
* Brief:  Write/Read Data by IIC
* Input: writebuf, writelen, readlen
* Output: readbuf
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char fts_i2c_read_write(unsigned char *writebuf, int  writelen, unsigned char *readbuf, int readlen)
{
    int ret;

    if (readlen > 0) {
        ret = fts_test_i2c_read(writebuf, writelen, readbuf, readlen);
    } else {
        ret = fts_test_i2c_write(writebuf, writelen);
    }

    if (ret >= 0)
        return (ERROR_CODE_OK);
    else
        return (ERROR_CODE_COMM_ERROR);
}

int fts_test_main_exit(void)
{
    FTS_TEST_FUNC_ENTER();
    /* Release memory test results */
    if (NULL !=  test_data.store_all_data) {
        FTS_TEST_DBG("[FTS] release memory store_all_data.");
        fts_free(test_data.store_all_data);
    }

    if (NULL != test_data.testresult) {
        FTS_TEST_DBG(" release memory testresult.");
        fts_free(test_data.testresult);
    }

    /* Releasing the memory of the detailed threshold structure */
    FTS_TEST_DBG("[FTS] release memory  free_struct_DetailThreshold.");
    free_struct_DetailThreshold();

    FTS_TEST_FUNC_EXIT();
    return 0;
}

//Save test data to SD card etc.
static int fts_test_save_test_data(char *file_name, char *data_buf, int len)
{
    struct file *pfile = NULL;
    char filepath[128];
    loff_t pos;
    mm_segment_t old_fs;

    FTS_TEST_FUNC_ENTER();
    memset(filepath, 0, sizeof(filepath));
    sprintf(filepath, "%s%s", FTS_SAVE_DATA_FILE_PATH, file_name);
    if (NULL == pfile) {
        pfile = filp_open(filepath, O_TRUNC | O_CREAT | O_RDWR, 0);
    }
    if (IS_ERR(pfile)) {
        FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
        return -EIO;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    vfs_write(pfile, data_buf, len, &pos);
    filp_close(pfile, NULL);
    set_fs(old_fs);

    FTS_TEST_FUNC_EXIT();
    return 0;
}

/************************************************************************
* Name: fts_set_testitem
* Brief:  set test item code and name
* Input: null
* Output: null
* Return:
**********************************************************************/
void fts_set_testitem(u8 itemcode)
{
    test_data.test_item[test_data.test_num].itemcode = itemcode;
    test_data.test_item[test_data.test_num].testnum = test_data.test_num;
    test_data.test_item[test_data.test_num].testresult = RESULT_NULL;
    test_data.test_num++;
}

/************************************************************************
* Name: init_storeparam_testdata
* Brief:  Init store param of test data
* Input: none
* Output: none
* Return: none
***********************************************************************/
void init_storeparam_testdata(void)
{
    test_data.len_store_msg_area = 0;
    //Msg Area, Add Line1
    test_data.len_store_msg_area += sprintf(test_data.store_msg_area, "ECC, 85, 170, IC Name, %s, IC Code, %x\n",  test_data.ini_ic_name,  test_data.screen_param.selected_ic);

    //Line2
    //msg_area_line2 = NULL;
    test_data.len_msg_area_line2 = 0;

    //Data Area
    //store_data_area = NULL;
    test_data.len_store_data_area = 0;
    test_data.start_line = 11;//The Start Line of Data Area is 11

    test_data.test_data_count = 0;
}

/************************************************************************
* Name: merge_all_testdata
* Brief:  Merge All Data of test result
* Input: none
* Output: none
* Return: none
***********************************************************************/
void merge_all_testdata(void)
{
    int len = 0;

    //Add the head part of Line2
    len = sprintf(test_data.tmp_buffer, "TestItem, %d, ", test_data.test_data_count);
    memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.tmp_buffer, len);
    test_data.len_store_msg_area += len;

    //Add other part of Line2, except for "\n"
    memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.msg_area_line2, test_data.len_msg_area_line2);
    test_data.len_store_msg_area += test_data.len_msg_area_line2;

    //Add Line3 ~ Line10
    len = sprintf(test_data.tmp_buffer, "\n\n\n\n\n\n\n\n\n");
    memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.tmp_buffer, len);
    test_data.len_store_msg_area += len;

    ///1.Add Msg Area
    memcpy(test_data.store_all_data, test_data.store_msg_area,  test_data.len_store_msg_area);

    ///2.Add Data Area
    if (0 != test_data.len_store_data_area) {
        memcpy(test_data.store_all_data + test_data.len_store_msg_area, test_data.store_data_area, test_data.len_store_data_area);
    }

    FTS_TEST_DBG("lenStoreMsgArea=%d,  lenStoreDataArea = %d",   test_data.len_store_msg_area, test_data.len_store_data_area);
}

/************************************************************************
* Name: allocate_testdata_memory
* Brief:  Allocate pointer Memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
int allocate_testdata_memory(void)
{
    //New buff
    test_data.store_msg_area = NULL;
    if (NULL == test_data.store_msg_area)
        test_data.store_msg_area = fts_malloc(BUFF_LEN_STORE_MSG_AREA);
    if (NULL == test_data.store_msg_area)
        goto ERR;

    test_data.msg_area_line2 = NULL;
    if (NULL == test_data.msg_area_line2)
        test_data.msg_area_line2 = fts_malloc(BUFF_LEN_MSG_AREA_LINE2);
    if (NULL == test_data.msg_area_line2)
        goto ERR;

    test_data.store_data_area = NULL;
    if (NULL == test_data.store_data_area)
        test_data.store_data_area = fts_malloc(BUFF_LEN_STORE_DATA_AREA);
    if (NULL == test_data.store_data_area)
        goto ERR;

    test_data.tmp_buffer = NULL;
    if (NULL == test_data.tmp_buffer)
        test_data.tmp_buffer = fts_malloc(BUFF_LEN_TMP_BUFFER);
    if (NULL == test_data.tmp_buffer)
        goto ERR;

    return 0;

ERR:
    FTS_TEST_ERROR("fts_malloc memory failed in function.");
    return -ENOMEM;
}

/************************************************************************
* Name: free_testdata_memory
* Brief:  Release pointer memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
void free_testdata_memory(void)
{
    //Release buff
    if (NULL != test_data.store_msg_area)
        fts_free(test_data.store_msg_area);

    if (NULL != test_data.msg_area_line2)
        fts_free(test_data.msg_area_line2);

    if (NULL != test_data.store_data_area)
        fts_free(test_data.store_data_area);

    if (NULL != test_data.tmp_buffer)
        fts_free(test_data.tmp_buffer);
}


/************************************************************************
* Name: init_test
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
int init_test(void)
{
    int ret = 0;
    u8 tx_num = test_data.screen_param.tx_num;
    u8 rx_num = test_data.screen_param.rx_num;

    ret = allocate_testdata_memory();
    if (ret < 0)
        return ret;

    init_storeparam_testdata();

    test_data.buffer = (int *)fts_malloc(((tx_num + 1) * rx_num) * sizeof(int));
    memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));

    return 0;
}

/************************************************************************
* Name: finish_test
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
void finish_test(void)
{
    merge_all_testdata();//Merge Test Result
    free_testdata_memory();//Release pointer memory
    if (test_data.buffer)
        fts_free(test_data.buffer);
}

int get_tx_rx_num(u8 tx_rx_reg, u8 *ch_num, u8 ch_num_max)
{
    int ret = 0;
    int i = 0;

    for (i = 0; i < 3; i++) {
        ret = read_reg(tx_rx_reg, ch_num);
        if ((ret < 0) || (*ch_num > ch_num_max)) {
            sys_delay(50);
        } else
            break;
    }

    if (i >= 3) {
        FTS_TEST_ERROR("get channel num fail");
        return -EIO;
    }

    return 0;
}

int get_channel_num(void)
{
    int ret = 0;
    u8 tx_num = 0;
    u8 rx_num = 0;

    FTS_TEST_FUNC_ENTER();

    ret = enter_factory_mode();
    if (ret < 0) {
        FTS_TEST_ERROR("enter factory mode fail, can't get tx/rx num");
        return ret;
    }

    test_data.screen_param.used_max_tx_num = TX_NUM_MAX;
    test_data.screen_param.used_max_rx_num = RX_NUM_MAX;
    test_data.screen_param.key_num_total = KEY_NUM_MAX;
    ret = get_tx_rx_num(FACTORY_REG_CHX_NUM, &tx_num, TX_NUM_MAX);
    if (ret < 0) {
        FTS_TEST_ERROR("get tx_num fail");
        return ret;
    }

    ret = get_tx_rx_num(FACTORY_REG_CHY_NUM, &rx_num, RX_NUM_MAX);
    if (ret < 0) {
        FTS_TEST_ERROR("get rx_num fail");
        return ret;
    }

    test_data.screen_param.tx_num = tx_num;
    test_data.screen_param.rx_num = rx_num;
    test_data.screen_param.used_max_tx_num = tx_num + KEY_NUM_MAX;
    test_data.screen_param.used_max_rx_num = rx_num + KEY_NUM_MAX;

    FTS_TEST_INFO("TxNum=%d, RxNum=%d, MaxTxNum=%d, MaxRxNum=%d",
                  test_data.screen_param.tx_num,
                  test_data.screen_param.rx_num,
                  test_data.screen_param.used_max_tx_num,
                  test_data.screen_param.used_max_rx_num);

    FTS_TEST_FUNC_EXIT();
    return 0;
}

int fts_test_main_init(void)
{
    int ret = 0;

    FTS_TEST_FUNC_ENTER();
    test_data.testresult_len = 0;
    test_data.testresult = fts_malloc(BUFF_LEN_TESTRESULT_BUFFER);
    if (NULL == test_data.testresult) {
        FTS_TEST_ERROR("malloc test result memory saved in .txt fail");
        return -ENOMEM;
    }

    /* Allocate memory, storage test results */
    test_data.store_all_data = fts_malloc(FTS_TEST_STORE_DATA_SIZE);
    if (NULL == test_data.store_all_data) {
        FTS_TEST_ERROR("malloc test result memory saved in .csv fail");
        return -ENOMEM;
    }

    /*  Allocate memory,  assigned to detail threshold structure*/
    ret = malloc_struct_DetailThreshold();

    FTS_TEST_FUNC_EXIT();
    return ret;
}

/*
 * get test basic information, need output to .txt
 */
static int fts_test_init_basicinfo(char *ini_file_name)
{
    int ret = 0;
    u8 val = 0;

    FTS_TEST_DBG("FTS TESTCODE VERSION:%s ",  IC_TEST_VERSION);

    FTS_TEST_DBG("ini file name:%s", ini_file_name);

    ret = read_reg(REG_FW_VERSION, &val);
    FTS_TEST_DBG("FW version:0x%02x", val);

    ret = read_reg(REG_VA_TOUCH_THR, &val);
    test_data.va_touch_thr = val;
    ret = read_reg(REG_VKEY_TOUCH_THR, &val);
    test_data.key_touch_thr = val;

    ret = get_channel_num();
    FTS_TEST_DBG("tx_num:%d, rx_num:%d",
                 test_data.screen_param.tx_num,
                 test_data.screen_param.rx_num);

    return ret;
}

static int fts_test_get_testparams(char *config_name)
{
    int ret = 0;

    ret = fts_test_get_testparam_from_ini(config_name);

    return ret;
}

/************************************************************************
* Name: fts_test_start
* Brief:  Test entry. Select test items based on IC series
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/
bool fts_test_start(void)
{
    bool testresult = false;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_DBG("IC_%s Test",  test_data.ini_ic_name);

    if (test_data.func->start_test) {
        testresult = test_data.func->start_test();
    } else {
        FTS_TEST_DBG("[Focal]start_test func null!\n");
        testresult = false;
    }

    enter_work_mode();

    FTS_TEST_FUNC_EXIT();

    return testresult;
}

static int fts_test_entry(char *ini_file_name)
{
    int ret = 0;
    int data_len = 0;
    int test_result = 0;
    FTS_TEST_FUNC_ENTER();

    /*Initialize pointer memory*/
    ret = fts_test_main_init();
    if (ret < 0) {
        FTS_TEST_ERROR("fts_test_main_init() error.");
        goto TEST_ERR;
    }

    ret = fts_test_init_basicinfo(ini_file_name);
    if (ret) {
        FTS_TEST_ERROR("test basic information init fail");
        goto TEST_ERR;
    }

    /*Read parse configuration file*/
    FTS_TEST_DBG("ini_file_name = %s", ini_file_name);
    ret = fts_test_get_testparams(ini_file_name);
    if (ret < 0) {
        FTS_TEST_ERROR("get testparam failed");
        goto TEST_ERR;
    }

    /*Select IC must match  IC from INI*/
    if ((test_data.screen_param.selected_ic >> 4  != FTS_CHIP_TEST_TYPE >> 4)) {
        FTS_TEST_ERROR("Select IC in driver:0x%x, INI:0x%x, no match", test_data.screen_param.selected_ic, FTS_CHIP_TEST_TYPE);
        goto TEST_ERR;
    }

    /*Start testing according to the test configuration*/
    if (true == fts_test_start()) {
		test_result = 1;
		printk("ttt test pass\n");
        FTS_TEST_SAVE_INFO("\n=======Tp test pass. \n\n");
    } else {
    	test_result = 0;
    	printk("ttt test fail\n");
        FTS_TEST_SAVE_INFO("\n=======Tp test failure. \n\n");
    }

    /*Gets the number of tests in the test library and saves it*/
    data_len = test_data.len_store_msg_area + test_data.len_store_data_area;
    fts_test_save_test_data("testdata.csv", test_data.store_all_data, data_len);
    fts_test_save_test_data("testresult.txt", test_data.testresult, test_data.testresult_len);

    /*Release memory */
    fts_test_main_exit();

    FTS_TEST_FUNC_EXIT();
	return test_result; 

TEST_ERR:
    enter_work_mode();
    fts_test_main_exit();

    FTS_TEST_FUNC_EXIT();
    return ret;
}
#if 1
 static int fts_set_ini_name(char *cfgname)
 {
		 int ret;
		 u8 vendor_id;
		 
		 ret = fts_i2c_read_reg(fts_data->client, FTS_REG_VENDOR_ID,&vendor_id);
		 FTS_TEST_DBG("LCD vendor id:0x%x\n",vendor_id);
		 if (vendor_id == OFILM_VENDOR){
		 	if(focal_lcm_maker == 0x36){
					sprintf(cfgname, "%s", "fts_ofilm_tima.ini");
				}else if (focal_lcm_maker == 0x37){
					sprintf(cfgname, "%s", "fts_ofilm_ebbg.ini");
				}else if (focal_lcm_maker == 0x35){
					sprintf(cfgname, "%s", "fts_ofilm_boe.ini");
				}else{
					 pr_err("ctp test not found test config \n");
				 }	 
		 	}else if(vendor_id == LENS_VENDOR){//lens + boe
				if (focal_lcm_maker == 0x35){
					sprintf(cfgname, "%s", "fts_lens_boe.ini");
					}
			}
		 return ret;
	 }
 
 static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
 {
	 char fwname[128] = {0};
	 struct fts_ts_data *ts_data = fts_data;
	 struct i2c_client *client;
	 struct input_dev *input_dev;
     int result = 0;
	 int ret;
	 int len = count;
	 
	 FTS_TEST_FUNC_ENTER();
 
	 client = ts_data->client;
	 input_dev = ts_data->input_dev;
	if(*ppos){
	    FTS_TEST_ERROR("tp test again return\n");
	    return 0;
		}
	*ppos += count;

	 memset(fwname, 0, sizeof(fwname));
	 fts_set_ini_name(fwname);
	 fwname[strlen(fwname)] = '\0';
	 FTS_TEST_DBG("fwname:%s.", fwname);
 
	 mutex_lock(&input_dev->mutex);
	 disable_irq(client->irq);
 
#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	 fts_esdcheck_switch(DISABLE);
#endif
 
	 ret = fts_test_entry(fwname);
	 if(ret == 1){
		 result = 1;
	 }else{
		 result = 0;
		 }
 
#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	 fts_esdcheck_switch(ENABLE);
#endif
 
	 enable_irq(client->irq);
	 mutex_unlock(&input_dev->mutex);
 
	 FTS_TEST_FUNC_EXIT();
	 if (count > 9)
		len = 9;
	 FTS_TEST_DBG("fts result = %d\n",result);
	 if (result == 1){
		 if (copy_to_user(buf, "result=1", len)) {
			FTS_TEST_ERROR("copy_to_user fail\n");
			return -1;
	 		}
	 	}else{
	 	if (copy_to_user(buf, "result=0", len)) {
			FTS_TEST_ERROR("copy_to_user fail\n");
			return -1;
	 		}
		}
	 return len;
}
 static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
 {
	 return -1;
	 }
#if 1
 static ssize_t ctp_selftest_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
 {
	 FTS_TEST_DBG("ctp_selftest_proc_read selftest_result = %d\n",selftest_result);	
 	
	if(*ppos){
	    FTS_TEST_ERROR("tp test again return\n");
	    return 0;
		}
	*ppos += count;

	 FTS_TEST_FUNC_EXIT();

	 if (selftest_result == 2){
		 if (copy_to_user(buf, "2\n", 2)) {
			FTS_TEST_ERROR("copy_to_user fail\n");
			return -1;
	 		}
	 	}else if (selftest_result == 1){
	 	if (copy_to_user(buf, "1\n", 2)) {
			FTS_TEST_ERROR("copy_to_user fail\n");
			return -1;
	 		}
		}else if (selftest_result == 0){
	 	if (copy_to_user(buf, "0\n", 2)) {
			FTS_TEST_ERROR("copy_to_user fail\n");
			return -1;
	 		}
		}else{
		if (copy_to_user(buf, "-1\n", 2)) {
			FTS_TEST_ERROR("copy_to_user fail\n");
			return -1;
			}
		}
		
	selftest_result = 0;
	return 2;
}
 static ssize_t ctp_selftest_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
 {
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_client *client;
	struct input_dev *input_dev;
	char fwname[128] = {0};
	int ret = -1;
	char test[10] = {"\0"};
	client = ts_data->client;
	input_dev = ts_data->input_dev;

	FTS_TEST_DBG("user echo %s to tp_selftest",userbuf);
	if(copy_from_user(test,userbuf,3)){
			FTS_TEST_ERROR("ctp_selftest_proc_write copy_from_user fail\n");
			return -1;
		}else{
			*ppos += count;
		}
		
	if(!strncmp("i2c",test,3)){// the process of tp open short test
	 	memset(fwname, 0, sizeof(fwname));
	 	fts_set_ini_name(fwname);
	 	fwname[strlen(fwname)] = '\0';
	 	FTS_TEST_DBG("fwname:%s.", fwname);
		mutex_lock(&input_dev->mutex);
		disable_irq(client->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	 	fts_esdcheck_switch(DISABLE);
#endif
	 	ret = fts_test_entry(fwname);
	 	if(ret == 1){
		 		selftest_result = 2;//test sucess
	 		}else{
		 		selftest_result = 1;//test fail
	 	}
 
#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
		fts_esdcheck_switch(ENABLE);
#endif
	 	enable_irq(client->irq);
	 	mutex_unlock(&input_dev->mutex);
	 
	}else{
		FTS_TEST_DBG("echo invaild cmd not do tp open short test\n");
		selftest_result = 0;//test invaild
	}
	 	FTS_TEST_DBG("ctp_selftest_proc_write selftest_result = %d\n",selftest_result);	
	 	return count;
 }

#endif
 void create_ctp_proc(void)
{
    //----------------------------------------
    //create read/write interface for tp information
    //the path is :proc/touchscreen
    //child node is :version
    //----------------------------------------
  struct proc_dir_entry *ctp_open_proc = NULL;

	printk("ttttt create_ctp_proc \n");	
  if( ctp_device_proc == NULL)
   {
	    ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
	    if(ctp_device_proc == NULL)
	    {
	        FTS_TEST_ERROR("create parent_proc fail\n");
	        return;
	    }
	}
    ctp_open_proc = proc_create(CTP_OPEN_PROC_NAME, 0777, ctp_device_proc, &ctp_open_procs_fops);
    if (ctp_open_proc == NULL)
    {
        FTS_TEST_ERROR("create open_proc fail\n");
    }

	
    ctp_selftest_proc = proc_create(CTP_SELF_TEST, 0777, NULL,&ctp_selftest_procs_fops);
    if (ctp_selftest_proc == NULL)
    {
        FTS_TEST_ERROR("create ctp_self fail\n");
    }
	
}


#else
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
    struct fts_ts_data *ts_data = fts_data;
    struct i2c_client *client;
    struct input_dev *input_dev;

    FTS_TEST_FUNC_ENTER();

    client = ts_data->client;
    input_dev = ts_data->input_dev;
    memset(fwname, 0, sizeof(fwname));
    sprintf(fwname, "%s", buf);
    fwname[count - 1] = '\0';
    FTS_TEST_DBG("fwname:%s.", fwname);

    mutex_lock(&input_dev->mutex);
    disable_irq(client->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
    fts_esdcheck_switch(DISABLE);
#endif

    fts_test_entry(fwname);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
    fts_esdcheck_switch(ENABLE);
#endif

    enable_irq(client->irq);
    mutex_unlock(&input_dev->mutex);

    FTS_TEST_FUNC_EXIT();
    return count;
}

/*  test from test.ini
*    example:echo "***.ini" > fts_test
*/
static DEVICE_ATTR(fts_test, S_IRUGO | S_IWUSR, fts_test_show, fts_test_store);

/* add your attr in here*/
static struct attribute *fts_test_attributes[] = {
    &dev_attr_fts_test.attr,
    NULL
};

static struct attribute_group fts_test_attribute_group = {
    .attrs = fts_test_attributes
};
#endif 
int fts_test_init(struct i2c_client *client)
{
    int ret = 0;
	printk("ttt fts_test_init\n");
    FTS_TEST_FUNC_ENTER();
#if 0 
    ret = sysfs_create_group(&client->dev.kobj, &fts_test_attribute_group);
    if (0 != ret) {
        FTS_TEST_ERROR( "[focal] %s() - ERROR: sysfs_create_group() failed.",  __func__);
        sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
    } else {
        FTS_TEST_DBG("[focal] %s() - sysfs_create_group() succeeded.", __func__);
    }
#else
	create_ctp_proc();

#endif
    FTS_TEST_FUNC_EXIT();

    return ret;
}

int fts_test_exit(struct i2c_client *client)
{
    FTS_TEST_FUNC_ENTER();


    FTS_TEST_FUNC_EXIT();
    return 0;
}
#if	FTS_LOCK_DOWN_INFO
int fts_lockdown_init(struct i2c_client *client, struct fts_ts_data *fts_data)
{

    struct proc_dir_entry *fts_lockdown_status_proc = NULL;


	if( ctp_device_proc == NULL)
	{
	    ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
	    if(ctp_device_proc == NULL)
	    {
	        FTS_TEST_ERROR("create parent_proc fail\n");
	        return 1;
	    }
	}
	fts_lockdown_status_proc = proc_create(FTS_PROC_LOCKDOWN_FILE, 0644, ctp_device_proc, &fts_lockdown_proc_fops);
	if (fts_lockdown_status_proc == NULL)
	{
		FTS_ERROR("fts, create_proc_entry ctp_lockdown_status_proc failed\n");
		return 1;
	}
	return 0 ;
}
#endif

