/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include "kd_camera_hw.h"
#include "cam_cal.h"
#include "cam_cal_define.h"
//#include <asm/system.h>  // for SMP
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#include "cereus_ov12a10_ofilm_mipiraw_Sensor.h"

#define PFX "CEREUS_OV12A10_OFILM_OTP_CAL"
#define LOG_INF_DBG(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_INF_ERR(format, args...)    pr_err(PFX "[%s] " format, __FUNCTION__, ##args)
#define CAM_CALGETDLT_DEBUG
#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG


#define CAM_CALINF(fmt, arg...)    pr_debug("[%s] " fmt, __FUNCTION__, ##arg)
#define CAM_CALDB(fmt, arg...)     pr_debug("[%s] " fmt, __FUNCTION__, ##arg)
#define CAM_CALERR(fmt, arg...)    pr_err("[%s] " fmt, __FUNCTION__, ##arg)
#else
#define CAM_CALINF(x,...)
#define CAM_CALDB(x,...)
#define CAM_CALERR(fmt, arg...)    pr_err("[%s] " fmt, __FUNCTION__, ##arg)
#endif

static DEFINE_SPINLOCK(g_CAM_CALLock); // for SMP

#define USHORT unsigned short
#define BYTE   unsigned char
#define Sleep(ms) mdelay(ms)
#define CAM_CAL_DRVNAME "CEREUS_OV12A10_OFILM_CAM_CAL_DRV"
#define CAM_CAL_I2C_GROUP_ID 0
#define CAM_CAL_DEV_MAJOR_NUMBER 226

#define EEPROM_READ_ID        0xA1
#define EEPROM_WRITE_ID       0xA0

static dev_t g_CAM_CALdevno = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER,0);
static struct cdev * g_pCAM_CAL_CharDrv = NULL;


static struct class *CAM_CAL_class = NULL;
static atomic_t g_CAM_CALatomic;

#define I2C_SPEED             100
#define MAX_OFFSET            0xFFFF
#define DATA_SIZE             2048
#define I2C_BUS_NUM           0
#define CALI_DATA_OFFESET     0x0d52
#define CALI_DATA_FLAG_OFFSET 0x14f0
#define CALI_DATA_FLAG_VALUE  0x33
#define CALI_DATA_NUM         419
#define PATH_CALI   "/data/misc/westalgo/dualcam_cali.bin"
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

#define MAX_OTP_SIZE 4096

static int cereus_ov12a10_ofilm_eeprom_read = 0;

typedef union {
        u8 Data[MAX_OTP_SIZE];
//        OTP_MTK_TYPE       MtkOtpData;
} OTP_DATA;


OTP_DATA cereus_ov12a10_ofilm_eeprom_data = {{0}};

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData,u16 a_sizeRecvData, u16 i2cId);

static BYTE *bin_buffer = NULL;
static BYTE *cali_buffer = NULL;
static uint8_t bOtpFileExit = FALSE;

static void write_eeprom_i2c(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, EEPROM_WRITE_ID);
}

static bool write_eeprom(kal_uint16 addr, kal_uint16 size, BYTE *data)
{
    int i = 0;
    int count = 0;
    int offset = addr;

    for(i = 0; i < size; i++) {
        //if ( (selective_write_eeprom(offset, data + i)) >= 0) {
        write_eeprom_i2c((u16)offset, (u32) * (data + i));
        //LOG_INF("teddebug write addr 0x%0x data 0x%0x",offset,(u32)*(data + i));
        msleep(2);
        count++;
        //}
        offset++;
    }
    if (count == size)
        return true;

    LOG_INF_ERR("Eeprom write failed");
    return false;
}

static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
    char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

    if(addr > MAX_OFFSET) {
        LOG_INF_ERR("Unsupport addr = 0x%x\n", addr);
        return false;
    }

    if(iReadRegI2C(pu_send_cmd, 2, (u8 *)data, 1, EEPROM_READ_ID) < 0)
        return false;

    return true;
}

static bool read_eeprom_cali( kal_uint16 addr, BYTE *data, kal_uint32 size)
{

    int i = 0;
    int offset = addr;

    LOG_INF_ERR("Read eeprom calibration data, size = %d\n", size);

    //iMultiReadReg(addr, data, EEPROM_READ_ID, size);

    for(i = 0; i < size; i++) {
        if(!selective_read_eeprom(offset, &data[i]))
            return false;
        //  LOG_INF("read_eeprom 0x%0x 0x%0x\n",offset, data[i]);
        offset++;
    }

    return true;
}

static uint32_t read_dualcam_cali_data(void)
{
    struct file *fp;
    uint32_t data_size = 0;
    loff_t pos = 0;
    //int i = 0;
    mm_segment_t fs = get_fs();

    fp = filp_open(PATH_CALI, O_RDWR, 0666);
    if (IS_ERR(fp)) {
        LOG_INF_ERR("faile to open file cali data error\n");
        return -1;
    }

    data_size = vfs_llseek(fp, 0, SEEK_END);
    LOG_INF_ERR("Binary data size is %d bytes \n", data_size);

    if(data_size > 0) {
        if(bin_buffer == NULL) {
            bin_buffer = kzalloc(data_size, GFP_KERNEL);
            if (bin_buffer == NULL) {
                LOG_INF_ERR("[Error]malloc memery failed \n");
                goto close;
            }
        }
        // fs = get_fs();
        set_fs(KERNEL_DS);
        pos = 0;
        vfs_read(fp, bin_buffer, data_size, &pos);
        LOG_INF_ERR("Read new calibration data done!\n");

        //for( i=0; i < data_size; i++)
        //LOG_INF("data %d = %x\n",i,*(bin_buffer + i));

        filp_close(fp, NULL);
        set_fs(fs);
        return data_size;
    } else
        LOG_INF_ERR("[Error] Get calibration data failed\n");

close:
    filp_close(fp, NULL);
    set_fs(fs);
    LOG_INF_ERR("read dualcam cali data exit\n");
    return 0;
}

static int write_eeprom_memory(BYTE *data, uint32_t size)
{
    bool rc = false;

    rc = write_eeprom(CALI_DATA_OFFESET, size, data);
    if(!rc) {
        LOG_INF_ERR("%s: write eeprom failed\n", __func__);
        return -1;
    }

    return 0;
}

int get_verify_flag(void)
{

    BYTE flag = 0;

    if(!selective_read_eeprom(CALI_DATA_FLAG_OFFSET, &flag))
        return -1;

    LOG_INF_ERR("Get verify flag data 0x%x\n", flag);

    return flag;
}

static int set_verify_flag(void)
{
    int rc = -1;
    BYTE flag = 0;

    if(!selective_read_eeprom(CALI_DATA_FLAG_OFFSET, &flag))
        return rc;

    LOG_INF_ERR("Get verify flag data 0x%x\n", flag);

    if ( flag != CALI_DATA_FLAG_VALUE)
        write_eeprom_i2c(CALI_DATA_FLAG_OFFSET, CALI_DATA_FLAG_VALUE);
    else
        LOG_INF_ERR("Verify flag is already set\n");

    return 0;
}

int store_dualcam_cali_flag_cereus_ov12a10_ofilm(void)
{
    int rc = -1;
    LOG_INF_ERR(" Enter store_dualcam_cali_flag\n");
    rc = set_verify_flag();
    if (rc < 0) {
        LOG_INF_ERR(" %s: failed to set_verify_flag\n", __func__);
        return -1;
    }
    return 0;

}

int store_dualcam_cali_data_cereus_ov12a10_ofilm(void)
{
    int size = -1, rc = -1;

    LOG_INF_ERR("enter store_dualcam_cali_data\n");

    size = read_dualcam_cali_data();

    if (size < 0) {
        LOG_INF_ERR("Fail to get new calibration data\n");
        return -1;
    }

    rc = write_eeprom_memory(bin_buffer, size);
    if (rc < 0) {
        LOG_INF_ERR("%s: failed to write_eeprom_memory \n", __func__);
        return -1;
    }

    rc = set_verify_flag();
    if (rc < 0) {
        LOG_INF_ERR("%s: failed to set_verify_flag\n", __func__);
        return -1;
    }

    return 0;
}

int dump_dualcam_cali_flag_cereus_ov12a10_ofilm(void)
{
    return get_verify_flag();
}

int set_verify_flag_for_adb_cereus_ov12a10_ofilm(int flag_value)
{

    int rc = -1;
    BYTE flag = 0;
    LOG_INF_ERR("[nutall] write eeprom flag_value = 0x%x\n", flag_value);

    if(!selective_read_eeprom(CALI_DATA_FLAG_OFFSET, &flag)){
        return rc;
    }
    LOG_INF_ERR("[%s] [nutall] Get verify flag data 0x%x\n",__func__,flag);

    write_eeprom_i2c(CALI_DATA_FLAG_OFFSET, flag_value);
    msleep(2); //Important, if not, write will fail.

    return 0;

}


int dump_dualcam_cali_data_cereus_ov12a10_ofilm(void)
{
    struct file *eeprom_file = NULL;
    int err = 0;
    mm_segment_t fs = get_fs();
    loff_t pos;
    //struct path path;
    //mode_t m;

    if(cali_buffer == NULL) {
        cali_buffer = kzalloc(CALI_DATA_NUM, GFP_KERNEL);
        if (cali_buffer == NULL) {
            LOG_INF_ERR("[Error]malloc memery failed \n");
            return -1;
        }
    }

    if (FALSE == bOtpFileExit) {

        //err = kern_path(PATH_CALI, LOOKUP_FOLLOW, &path);
        //path_put(&path);

        if (0) { //!err) {
            LOG_INF_DBG("Path_lookup on %s returned error %d\n", PATH_CALI, err);
            LOG_INF_DBG("Doesn't need to read cali data to data dir");
        } else {
            LOG_INF_ERR("Need to read cali data to data dir");
            read_eeprom_cali(CALI_DATA_OFFESET, cali_buffer, CALI_DATA_NUM);

            eeprom_file = filp_open(PATH_CALI, O_RDWR | O_CREAT, 0777);

            if (IS_ERR(eeprom_file)) {
                LOG_INF_ERR("open epprom file error");
                return -1;
            }

            set_fs(KERNEL_DS);
            pos = 0;
            LOG_INF_ERR("eeprom_file: %p, cali_buffer: %p\n", eeprom_file, cali_buffer);
            err = vfs_write(eeprom_file, cali_buffer, CALI_DATA_NUM, &pos);
            if (err < 0)
                LOG_INF_ERR("write epprom data error");
            filp_close(eeprom_file, NULL);
            set_fs(fs);

            bOtpFileExit = TRUE;
            kfree(cali_buffer);
            cali_buffer = NULL;
        }
    }
    return 0;
}


int read_cereus_ov12a10_ofilm_eeprom_mtk_fmt(void)
{
	int i = 0;
	int rc;
	u8 mid_flag;
	u8 awb_flag;
	u8 af_flag;
	u8 lsc_flag;
	u8 mid_checksum;
	u8 awb_checksum;
	u8 af_checksum;
	u8 lsc_checksum;

	CAM_CALINF("OTP cereus_ov12a10_ofilm_eeprom_read =%d \n",cereus_ov12a10_ofilm_eeprom_read);
	if(1 == cereus_ov12a10_ofilm_eeprom_read ) {
		CAM_CALDB("OTP readed ! skip\n");
		return 1;
	}
	spin_lock(&g_CAM_CALLock);
	cereus_ov12a10_ofilm_eeprom_read = 1;
	spin_unlock(&g_CAM_CALLock);

	
	rc = selective_read_eeprom(0x0000,&mid_flag);
	rc = selective_read_eeprom(0x0015,&mid_checksum);

	printk("--------: mid_flag:%d, checksum:%d\n",mid_flag,mid_checksum);

	if(read_eeprom_cali(0x0001, &cereus_ov12a10_ofilm_eeprom_data.Data[0], 7) == false)
	{
		CAM_CALERR("read cereus_ov12a10_ofilm_eeprom GT24C16 i2c fail !?\n");
    		return -1;
	}

#if 1
	for(i = 0; i < 7; i++)
		printk("------: module info[0x%x] = 0x%x--------\n", 0x01+i, cereus_ov12a10_ofilm_eeprom_data.Data[0 + i]);
#endif

	//read AWB
	rc = selective_read_eeprom(0x0020,&awb_flag);
	rc = read_eeprom_cali(0x0021,&cereus_ov12a10_ofilm_eeprom_data.Data[7],20);
	rc = selective_read_eeprom(0x0035,&awb_checksum);
	printk("--------: awb_flag:%d, checksum:%d\n",awb_flag,awb_checksum);

#if 0
	for(i = 0; i < 20; i++)
		printk("------: awb info i=%d,[0x%x] = 0x%x--------\n", i, 0x21+i, cereus_ov12a10_ofilm_eeprom_data.Data[7 + i]);
#endif

	// read AF
	rc = selective_read_eeprom(0x07c0,&af_flag);
	rc = read_eeprom_cali(0x07c1,&cereus_ov12a10_ofilm_eeprom_data.Data[27], 1);//Direction
	rc = read_eeprom_cali(0x07c2,&cereus_ov12a10_ofilm_eeprom_data.Data[28], 8);
	rc = selective_read_eeprom(0x07ca,&af_checksum);

	printk("--------: af_flag:%d, checksum:%d\n",af_flag,af_checksum);
#if 0
	for (i = 0; i < 9; i++)
		printk("------: af info i=%d,[0x%x] = 0x%x--------\n", i, (0x7c1 + i), cereus_ov12a10_ofilm_eeprom_data.Data[27 + i]);
#endif
        //read LSC size
	
	
	rc = selective_read_eeprom(0x0040,&lsc_flag);
	rc = selective_read_eeprom(0x078d,&lsc_checksum);

	printk("---- lsc flag=%d, checksum=%d----\n",lsc_flag,lsc_checksum);

        //for lsc data
	rc = read_eeprom_cali(0x0041, &cereus_ov12a10_ofilm_eeprom_data.Data[36], 1868);
#if 0
	for (i = 0; i < 1868; i++)
		printk("------: lsc i=%d [0x%x] = 0x%x--------\n", i, 0x41+i, cereus_ov12a10_ofilm_eeprom_data.Data[36 + i]);
#endif
    
    rc = read_otp_pdaf_data(0x0801,&cereus_ov12a10_ofilm_eeprom_data.Data[1904],0x55C);

#if 0
	for (i = 0; i < 1372; i++)
		printk("------: pdaf i=%d [0x%x] = 0x%x--------\n", i, 0x801+i, cereus_ov12a10_ofilm_eeprom_data.Data[1904 + i]);
#endif

	return rc;
}


static int selective_read_region(u32 offset, BYTE* data,u16 i2c_id,u32 size)
{
    memcpy((void *)data,(void *)&cereus_ov12a10_ofilm_eeprom_data.Data[offset],size);
	printk("selective_read_region offset =%x size %d data read = %d\n", offset,size, *data);
    return size;
}


/*******************************************************************************
*
********************************************************************************/
#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int CAM_CAL_Ioctl(struct inode * a_pstInode,
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
#else
static long CAM_CAL_Ioctl(
    struct file *file,
    unsigned int a_u4Command,
    unsigned long a_u4Param
)
#endif
{
    int i4RetValue = 0;
    u8 * pBuff = NULL;
    u8 * pu1Params = NULL;
    struct stCAM_CAL_INFO_STRUCT *ptempbuf;
#ifdef CAM_CALGETDLT_DEBUG
    struct timeval ktv1, ktv2;
    unsigned long TimeIntervalUS;
#endif

    if(_IOC_NONE == _IOC_DIR(a_u4Command))
    {
    }
    else
    {
        pBuff = (u8 *)kmalloc(sizeof( struct stCAM_CAL_INFO_STRUCT),GFP_KERNEL);

        if(NULL == pBuff)
        {
            CAM_CALERR(" ioctl allocate mem failed\n");
            return -ENOMEM;
        }

        if(_IOC_WRITE & _IOC_DIR(a_u4Command))
        {
            if(copy_from_user((u8 *) pBuff , (u8 *) a_u4Param, sizeof( struct stCAM_CAL_INFO_STRUCT)))
            {    //get input structure address
                kfree(pBuff);
                CAM_CALERR("ioctl copy from user failed\n");
                return -EFAULT;
            }
        }
    }

    ptempbuf = ( struct stCAM_CAL_INFO_STRUCT *)pBuff;
    pu1Params = (u8*)kmalloc(ptempbuf->u4Length,GFP_KERNEL);
    if(NULL == pu1Params)
    {
        kfree(pBuff);
        CAM_CALERR("ioctl allocate mem failed\n");
        return -ENOMEM;
    }


    if(copy_from_user((u8*)pu1Params ,  (u8*)ptempbuf->pu1Params, ptempbuf->u4Length))
    {
        kfree(pBuff);
        kfree(pu1Params);
        CAM_CALERR(" ioctl copy from user failed\n");
        return -EFAULT;
    }

    switch(a_u4Command)
    {
        case CAM_CALIOC_S_WRITE:
            CAM_CALDB("Write CMD \n");
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv1);
#endif
            i4RetValue = 0;//iWriteData((u16)ptempbuf->u4Offset, ptempbuf->u4Length, pu1Params);
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv2);
            if(ktv2.tv_sec > ktv1.tv_sec)
            {
                TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
            }
            else
            {
                TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;
            }
            CAM_CALDB("Write data %d bytes take %lu us\n",ptempbuf->u4Length, TimeIntervalUS);
#endif
            break;
        case CAM_CALIOC_G_READ:
            CAM_CALDB("[CAM_CAL] Read CMD \n");
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv1);
#endif
            i4RetValue = selective_read_region(ptempbuf->u4Offset, pu1Params,EEPROM_WRITE_ID,ptempbuf->u4Length);

#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv2);
            if(ktv2.tv_sec > ktv1.tv_sec)
            {
                TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
            }
            else
            {
                TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;
            }
            CAM_CALDB("Read data %d bytes take %lu us\n",ptempbuf->u4Length, TimeIntervalUS);
#endif

            break;
        default :
      	     CAM_CALINF("[CAM_CAL] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    if(_IOC_READ & _IOC_DIR(a_u4Command))
    {
        //copy data to user space buffer, keep other input paremeter unchange.
        if(copy_to_user((u8 __user *) ptempbuf->pu1Params , (u8 *)pu1Params , ptempbuf->u4Length))
        {
            kfree(pBuff);
            kfree(pu1Params);
            CAM_CALERR("[CAM_CAL] ioctl copy to user failed\n");
            return -EFAULT;
        }
    }

    kfree(pBuff);
    kfree(pu1Params);
    return i4RetValue;
}

//add by li.tan@tcl.com for 64bit ioctl compat 20170422
#ifdef CONFIG_COMPAT
static int compat_put_cal_info_struct(
             struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
             struct stCAM_CAL_INFO_STRUCT __user *data)
{
    compat_uptr_t p;
    compat_uint_t i;
    int err;

    err = get_user(i, &data->u4Offset);
    err |= put_user(i, &data32->u4Offset);
    err |= get_user(i, &data->u4Length);
    err |= put_user(i, &data32->u4Length);
    err |= get_user(p, (compat_uptr_t *)&data->pu1Params);
    err |= put_user(p, &data32->pu1Params);

    return err;
}
static int compat_get_cal_info_struct(
           struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
           struct stCAM_CAL_INFO_STRUCT __user *data)
{
    compat_uptr_t p;
    compat_uint_t i;
    int err;

    err = get_user(i, &data32->u4Offset);
    err |= put_user(i, &data->u4Offset);
    err |= get_user(i, &data32->u4Length);
    err |= put_user(i, &data->u4Length);
    err |= get_user(p, &data32->pu1Params);
    err |= put_user(compat_ptr(p), &data->pu1Params);

    return err;
}

static long cereus_ov12a10_ofilm_Ioctl_Compat(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;
   struct  COMPAT_stCAM_CAL_INFO_STRUCT __user *data32;
   struct stCAM_CAL_INFO_STRUCT __user *data;
    int err;

	CAM_CALDB("[CAMERA SENSOR]DEVICE_ID,%p %p %x ioc size %d\n",filp->f_op ,filp->f_op->unlocked_ioctl,cmd,_IOC_SIZE(cmd) );

    if (!filp->f_op || !filp->f_op->unlocked_ioctl)
        return -ENOTTY;

    switch (cmd) {

    case COMPAT_CAM_CALIOC_G_READ:
    {
        data32 = compat_ptr(arg);
        data = compat_alloc_user_space(sizeof(*data));
        if (data == NULL)
            return -EFAULT;

        err = compat_get_cal_info_struct(data32, data);
        if (err)
            return err;

        ret = filp->f_op->unlocked_ioctl(filp, CAM_CALIOC_G_READ,(unsigned long)data);
        err = compat_put_cal_info_struct(data32, data);


        if(err != 0)
            CAM_CALERR("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct failed\n");
        return ret;
    }
    default:
        return -ENOIOCTLCMD;
    }
}
#endif
//add by li.tan@tcl.com for 64bit ioctl compat 20170422
static u32 g_u4Opened = 0;
//#define
//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
static int CAM_CAL_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    CAM_CALDB("CAM_CAL_Open\n");
    spin_lock(&g_CAM_CALLock);
    if(g_u4Opened)
    {
        spin_unlock(&g_CAM_CALLock);
		CAM_CALERR("Opened, return -EBUSY\n");
        return -EBUSY;
    }
    else
    {
        g_u4Opened = 1;
        atomic_set(&g_CAM_CALatomic,0);
    }
    spin_unlock(&g_CAM_CALLock);
    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int CAM_CAL_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    spin_lock(&g_CAM_CALLock);

    g_u4Opened = 0;

    atomic_set(&g_CAM_CALatomic,0);

    spin_unlock(&g_CAM_CALLock);

    return 0;
}

static const struct file_operations g_stCAM_CAL_fops =
{
    .owner = THIS_MODULE,
    .open = CAM_CAL_Open,
    .release = CAM_CAL_Release,
#ifdef CONFIG_COMPAT
     .compat_ioctl = cereus_ov12a10_ofilm_Ioctl_Compat,
#endif
    .unlocked_ioctl = CAM_CAL_Ioctl
};

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1
//#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1

inline static int RegisterCAM_CALCharDrv(void)
{
    struct device* CAM_CAL_device = NULL;
    CAM_CALDB("RegisterCAM_CALCharDrv\n");
#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
    if( alloc_chrdev_region(&g_CAM_CALdevno, 0, 1,CAM_CAL_DRVNAME) )
    {
        CAM_CALERR(" Allocate device no failed\n");

        return -EAGAIN;
    }
#else
    if( register_chrdev_region(  g_CAM_CALdevno , 1 , CAM_CAL_DRVNAME) )
    {
        CAM_CALERR(" Register device no failed\n");

        return -EAGAIN;
    }
#endif

    //Allocate driver
    g_pCAM_CAL_CharDrv = cdev_alloc();

    if(NULL == g_pCAM_CAL_CharDrv)
    {
        unregister_chrdev_region(g_CAM_CALdevno, 1);

        CAM_CALERR(" Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pCAM_CAL_CharDrv, &g_stCAM_CAL_fops);

    g_pCAM_CAL_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pCAM_CAL_CharDrv, g_CAM_CALdevno, 1))
    {
        CAM_CALERR(" Attatch file operation failed\n");

        unregister_chrdev_region(g_CAM_CALdevno, 1);

        return -EAGAIN;
    }

    CAM_CAL_class = class_create(THIS_MODULE, "CEREUS_OV12A10_OFILM_CAM_CALdrv");
    if (IS_ERR(CAM_CAL_class)) {
        int ret = PTR_ERR(CAM_CAL_class);
        CAM_CALERR("Unable to create class, err = %d\n", ret);
        return ret;
    }
    CAM_CAL_device = device_create(CAM_CAL_class, NULL, g_CAM_CALdevno, NULL, CAM_CAL_DRVNAME);

    return 0;
}

inline static void UnregisterCAM_CALCharDrv(void)
{
    //Release char driver
    cdev_del(g_pCAM_CAL_CharDrv);

    unregister_chrdev_region(g_CAM_CALdevno, 1);

    device_destroy(CAM_CAL_class, g_CAM_CALdevno);
    class_destroy(CAM_CAL_class);
}

static int CAM_CAL_probe(struct platform_device *pdev)
{
    return 0;
}

static int CAM_CAL_remove(struct platform_device *pdev)
{
    return 0;
}

static struct platform_driver g_st_cereus_ov12a10_ofilmCAM_CAL_Driver = {
	.probe		= CAM_CAL_probe,
	.remove	= CAM_CAL_remove,
	.driver		= {
		.name	= CAM_CAL_DRVNAME,
		.owner	= THIS_MODULE,
	}
};

static struct platform_device g_st_cereus_ov12a10_ofilmCAM_CAL_Device = {
    .name = CAM_CAL_DRVNAME,
    .id = 0,
    .dev = {
    }
};

static int __init cereus_ov12a10_ofilm_CAM_CAL_init(void)
{
    int i4RetValue = 0;

    CAM_CALDB("CAM_CAL_i2C_init\n");
    
   //Register char driver
	i4RetValue = RegisterCAM_CALCharDrv();
    if(i4RetValue){
 	   CAM_CALDB(" register char device failed!\n");
	   return i4RetValue;
	}
	CAM_CALDB(" Attached!! \n");

    if(platform_driver_register(&g_st_cereus_ov12a10_ofilmCAM_CAL_Driver)){
        CAM_CALERR("failed to register cereus_ov12a10_ofilm_eeprom driver\n");
        return -ENODEV;
    }

    if (platform_device_register(&g_st_cereus_ov12a10_ofilmCAM_CAL_Device))
    {
        CAM_CALERR("failed to register cereus_ov12a10_ofilm_eeprom driver, 2nd time\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit cereus_ov12a10_ofilm_CAM_CAL_exit(void)
{
	platform_driver_unregister(&g_st_cereus_ov12a10_ofilmCAM_CAL_Driver);
}

module_init(cereus_ov12a10_ofilm_CAM_CAL_init);
module_exit(cereus_ov12a10_ofilm_CAM_CAL_exit);

MODULE_DESCRIPTION("CAM_CAL driver");
MODULE_AUTHOR("Sean Lin <Sean.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");


