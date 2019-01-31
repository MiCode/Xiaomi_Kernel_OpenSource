/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
/*#include <asm/system.h>  // for SM*/
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif
#include "kd_imgsensor.h"
/*#define CAM_CALGETDLT_DEBUG*/
#define CAM_CAL_DEBUG
#define PFX "CAM_CAL_DRV"
#ifdef CAM_CAL_DEBUG
/*#include <linux/log.h>*/
#define PK_INF(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define PK_DBG(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define PK_ERR(format, args...) \
	pr_err(PFX "[%s] " format, __func__, ##args)
#else
#define PK_INF(format, args...) \
    pr_info(PFX "[%s] " format, __func__, ##args)
#define PK_DBG(format, args...)
#define PK_ERR(format, args...) \
	pr_err(PFX "[%s] " format, __func__, ##args)
#endif

#define CAM_CAL_DRV_NAME "CAM_CAL_DRV"
#define CAM_CAL_DEV_MAJOR_NUMBER 226

#define CAM_CAL_I2C_MAX_BUSNUM 2
#define CAM_CAL_I2C_MAX_SENSOR 4
#define CAM_CAL_MAX_BUF_SIZE 65536	/*For Safety, Can Be Adjested */

#define CAM_CAL_I2C_DEV1_NAME CAM_CAL_DRV_NAME
#define CAM_CAL_I2C_DEV2_NAME "CAM_CAL_DEV2"
#define CAM_CAL_I2C_DEV3_NAME "CAM_CAL_DEV3"
#define CAM_CAL_I2C_DEV4_NAME "CAM_CAL_DEV4"

static dev_t g_devNum = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static unsigned int g_drvOpened;
static struct i2c_client *g_pstI2Cclient;
static struct i2c_client *g_pstI2Cclient2;
static struct i2c_client *g_pstI2Cclient3;

#define MAX_EEPROM_BYTE 0x1FFF
#define CHECKSUM_OK_FLAG_ADDR 0x1FFF-1
static int g_cur_device_id = 0;
static int  g_cur_sensorid = 0;

int g_read_flag[3] = {0};
char g_otp_buf[3][MAX_EEPROM_BYTE] = {0};

#define DUMP_OTP_BUF 0

static DEFINE_SPINLOCK(g_spinLock);	/*for SMP */

enum CAM_CAL_BUS_ID {
	BUS_ID_MAIN = 0,
	BUS_ID_SUB,
	BUS_ID_MAIN2,
	BUS_ID_SUB2,
	BUS_ID_MAX,
};

static enum CAM_CAL_BUS_ID g_curBusIdx = BUS_ID_MAIN;
static struct i2c_client *g_Current_Client;


/*Note: Must Mapping to IHalSensor.h*/
enum {
	SENSOR_DEV_NONE = 0x00,
	SENSOR_DEV_MAIN = 0x01,
	SENSOR_DEV_SUB = 0x02,
	SENSOR_DEV_PIP = 0x03,
	SENSOR_DEV_MAIN_2 = 0x04,
	SENSOR_DEV_MAIN_3D = 0x05,
	SENSOR_DEV_SUB_2 = 0x08,
	SENSOR_DEV_MAX = 0x50
};

static unsigned int g_lastDevID = SENSOR_DEV_NONE;

/***********************************************************
 *
 ***********************************************************/
struct stCAM_CAL_CMD_INFO_STRUCT {
	unsigned int sensorID;
	unsigned int deviceID;
	struct i2c_client *client;
	cam_cal_cmd_func readCMDFunc;
	cam_cal_cmd_func writeCMDFunc;
};

static struct stCAM_CAL_CMD_INFO_STRUCT g_camCalDrvInfo[CAM_CAL_I2C_MAX_SENSOR];

/********************************************************************
 * EEPROM_set_i2c_bus()
 * To Setting current index of Bus and Device, and current client.
 ********************************************************************/

static int EEPROM_set_i2c_bus(unsigned int deviceID)
{
	switch (deviceID) {
	case SENSOR_DEV_MAIN:
		g_curBusIdx = BUS_ID_MAIN;
		g_Current_Client = g_pstI2Cclient;
		break;
	case SENSOR_DEV_SUB:
		g_curBusIdx = BUS_ID_SUB;
		g_Current_Client = g_pstI2Cclient2;
		break;
	case SENSOR_DEV_MAIN_2:
		g_curBusIdx = BUS_ID_MAIN2;
		g_Current_Client = g_pstI2Cclient3;
		break;
	case SENSOR_DEV_SUB_2:
		g_curBusIdx = BUS_ID_SUB2;
		g_Current_Client = g_pstI2Cclient;
		break;
	default:
		return -EFAULT;
	}
	PK_DBG("Set i2c bus end! deviceID=%d g_curBusIdx=%d g_Current=%p\n",
	       deviceID, g_curBusIdx, g_Current_Client);

	if (g_Current_Client == NULL) {
		PK_ERR("g_Current_Client is NULL");
		return -EFAULT;
	}

	return 0;

}


/************************************************************
 * I2C read function
 ************************************************************/
static struct i2c_client *g_pstI2CclientG;

/* add for linux-4.4 */
#ifndef I2C_WR_FLAG
#define I2C_WR_FLAG		(0x1000)
#define I2C_MASK_FLAG	(0x00ff)
#endif

static int Read_I2C_CAM_CAL(u16 a_u2Addr, u32 ui4_length, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[2] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF) };


	if (ui4_length > 8) {
		PK_ERR("exceed I2c-mt65xx.c 8 bytes limitation\n");
		return -1;
	}
	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr =
		g_pstI2CclientG->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_spinLock);

	i4RetValue = i2c_master_send(g_pstI2CclientG, puReadCmd, 2);
	if (i4RetValue != 2) {
		PK_DBG("I2C send read address failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstI2CclientG,
						(char *)a_puBuff, ui4_length);
	if (i4RetValue != ui4_length) {
		PK_DBG("I2C read data failed!!\n");
		return -1;
	}

	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = g_pstI2CclientG->addr & I2C_MASK_FLAG;
	spin_unlock(&g_spinLock);
	return 0;
}

int iReadData_CAM_CAL(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{
	int i4RetValue = 0;
	int i4ResidueDataLength;
	u32 u4IncOffset = 0;
	u32 u4CurrentOffset;
	u8 *pBuff;

	if (ui4_offset + ui4_length >= 0x2000) {
		PK_DBG
		    ("Read Error!! not supprt address >= 0x2000!!\n");
		return -1;
	}

	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;
	do {
		if (i4ResidueDataLength >= 8) {
			i4RetValue = Read_I2C_CAM_CAL(
				(u16) u4CurrentOffset, 8, pBuff);
			if (i4RetValue != 0) {
				PK_ERR("I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += 8;
			i4ResidueDataLength -= 8;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
		} else {
			i4RetValue =
			    Read_I2C_CAM_CAL(
			    (u16) u4CurrentOffset, i4ResidueDataLength, pBuff);
			if (i4RetValue != 0) {
				PK_ERR("I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += 8;
			i4ResidueDataLength -= 8;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
			/* break; */
		}
	} while (i4ResidueDataLength > 0);



	return 0;
}

unsigned int Common_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (iReadData_CAM_CAL(addr, size, data) == 0)
		return size;
	else
		return 0;
}
int do_checksum(unsigned char *buf,
    unsigned int first, unsigned int last, unsigned int checksum)
{

    unsigned int i = 0;
    unsigned int sum = 0;
    for(i = first; i <= last; i++) {
        sum += buf[i];
    }
    sum = (sum % 255) + 1;
    if (buf[checksum] == (sum & 0xff)) {
        PK_DBG("check sum sucess: calculated=0x%02X actual=0x%02X", sum, buf[checksum]);
        return 0;
    } else {
        PK_ERR("check sum failed: calculated=0x%02X actual=0x%02X first=0x%x last=0x%x",
                                                sum, buf[checksum],first,last);
        return -1;/*0 TODO -- ju*/
    }

}

struct checksum_info{
        //char * buf;
        unsigned int start_addr;
        unsigned int end_addr;
        unsigned int checksum_addr;
};
struct checksum_info lotus_imx486_ofilm_checksum_info[]={
            {0x01,0x23,0x24}, // basic info
            {0x26,0x3e,0x3f}, // segment info
            {0x43,0x51,0x52}, // af
            {0x57,0x69,0x6a}, // awb
            {0x83,0x7d2,0x7d3}, // lsc
            {0xe73,0x13bf,0x13c0}, // pdaf
            {0x1463,0x1c63,0x1c64}, // dual_cam
            {0x0,0x1f66,0x1f67}, // total_cam
            {-1,-1,-1}

};
struct checksum_info lotus_ov12a10_sunny_checksum_info[]={
            {0x01,0x23,0x24}, // basic info
            {0x26,0x3e,0x3f}, // segment info
            {0x43,0x4d,0x4e}, // af
            {0x57,0x5f,0x60}, // awb
            {0x83,0x7d2,0x7d3}, // lsc
            {0xe63,0x13bf,0x13c0}, // pdaf
            {0x13c4,0x1bc4,0x1bc5}, // dual_cam
            {0x0,0x1bf4,0x1bf5}, // total_cam
            {-1,-1,-1}

};
struct checksum_info lotus_ov02a10_ofilm_checksum_info[]={
            {0x01,0x23,0x24}, // basic info
            {0x26,0x3e,0x3f}, // segment info
            {0x57,0x69,0x6a}, // awb
            {0x83,0x7d2,0x7d3}, // lsc
            {0x0,0x1bf4,0x1bf5}, // total_cam
            {-1,-1,-1}

};
struct checksum_info lotus_ov02a10_sunny_checksum_info[]={
            {0x01,0x23,0x24}, // basic info
            {0x26,0x3e,0x3f}, // segment info
            {0x57,0x5f,0x60}, // awb
            {0x83,0x7d2,0x7d3}, // lsc
            {0x0,0x1bf4,0x1bf5}, // total_cam
            {-1,-1,-1}

};
static int sensor_do_checksum(unsigned char * buf)
{
    int ret = 0 , i = 0,ok = 0;
    struct checksum_info * info = NULL;

    switch(g_cur_sensorid){

        case LOTUS_IMX486_OFILM_SENSOR_ID:
            info = lotus_imx486_ofilm_checksum_info;
            break;
        case LOTUS_OV12A10_SUNNY_SENSOR_ID:
            info = lotus_ov12a10_sunny_checksum_info;
            break;
        case LOTUS_OV02A10_OFILM_SENSOR_ID:
            info = lotus_ov02a10_ofilm_checksum_info;
            break;
        case LOTUS_OV02A10_SUNNY_SENSOR_ID:
            info = lotus_ov02a10_sunny_checksum_info;
            break;
        default:
            return -1;
            PK_ERR("Unsuport Sensor , checksum faild\n");
        }

    // info checksum
    PK_DBG("begin do check sum  ...");

    for(i = 0 ; info[i].start_addr!= -1 ; i++){
        ret =  do_checksum(buf,info[i].start_addr,
                            info[i].end_addr,info[i].checksum_addr);
        if( ret < 0) {
            PK_ERR("%x checksum error index=%d buf=%p s 0x%x e 0x%x ca 0x%x",
                        g_cur_sensorid,i,buf,info[i].start_addr,
                            info[i].end_addr,info[i].checksum_addr);
            //return ret;
            ok = ret;
        }else{
            PK_DBG("check sum ok index=%d",i);
        }
    }
    return ok;
}

unsigned int read_otp_from_mem(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
    int total_size = 0,ret = 0;
#if DUMP_OTP_BUF
    int i = 0;
#endif
    g_pstI2CclientG = client;

    if(!g_read_flag[g_cur_device_id]){

        switch(g_cur_sensorid){

        case LOTUS_IMX486_OFILM_SENSOR_ID:
            total_size = 0x1F67;
            break;
        case LOTUS_OV12A10_SUNNY_SENSOR_ID:
        case LOTUS_OV02A10_OFILM_SENSOR_ID:
        case LOTUS_OV02A10_SUNNY_SENSOR_ID:
            total_size = 0x1BF5;
            break;
        default:
            total_size = 0;
            PK_ERR("Unsuport Sensor ,read otp faild\n");
        }
        PK_DBG("read otp from eeprom id=0x%x size=0x%x\n",g_cur_sensorid,total_size);


        if (iReadData_CAM_CAL(0, total_size + 1, &g_otp_buf[g_cur_device_id][0]) == 0){

            ret = sensor_do_checksum(&g_otp_buf[g_cur_device_id][0]);
            if(ret){
               PK_ERR("0x%x checksum failed",g_cur_sensorid);
            }else{
               PK_INF("0x%x checksum success",g_cur_sensorid);
               g_otp_buf[g_cur_device_id][CHECKSUM_OK_FLAG_ADDR] = 0x88; //0x88 means checksum good
            }

#if DUMP_OTP_BUF
            PK_DBG("dump_otp data begin sensorid =0x%04x size=0x%04x\n",g_cur_sensorid,total_size);
            for(i = 0 ;i <= total_size; i++)
            {
                PK_DBG("dump_otp addr = 0x%04x data = 0x%04x",i,g_otp_buf[g_cur_device_id][i]);
            }
#endif
            //if(!ret)
                g_read_flag[g_cur_device_id] = 1;


        }
    }

    if(g_read_flag[g_cur_device_id]){
		PK_DBG("OTP have read ,copy from memory\n");
        memcpy((void *)data,&g_otp_buf[g_cur_device_id][addr],size);
        return size;
    }
    return 0;
}
/*************************************************
 * EEPROM_get_cmd_info function
 *************************************************/

static int EEPROM_get_cmd_info(unsigned int sensorID,
	struct stCAM_CAL_CMD_INFO_STRUCT *cmdInfo)
{
	struct stCAM_CAL_LIST_STRUCT *pCamCalList = NULL;
	int i = 0;

	cam_cal_get_sensor_list(&pCamCalList);
	if (pCamCalList != NULL) {
		PK_DBG("pCamCalList!=NULL && pCamCalFunc!= NULL\n");
		for (i = 0; pCamCalList[i].sensorID != 0; i++) {
			if (pCamCalList[i].sensorID == sensorID) {
				g_Current_Client->addr =
					pCamCalList[i].slaveID >> 1;
				cmdInfo->client = g_Current_Client;

				PK_DBG("pCamCalList[%d].sensorID==%x\n", i,
				       pCamCalList[i].sensorID);
				PK_DBG("g_Current_Client->addr =%x\n",
						g_Current_Client->addr);
				PK_DBG("main=%p sub=%p main2=%p Cur=%p\n",
				       g_pstI2Cclient, g_pstI2Cclient2,
				       g_pstI2Cclient3, g_Current_Client);

/* if you have special EEPROM driver, you can modify here 2017.11.07 */
                switch(sensorID){
                  case LOTUS_IMX486_OFILM_SENSOR_ID:
                  case LOTUS_OV12A10_SUNNY_SENSOR_ID:
                  case LOTUS_OV02A10_OFILM_SENSOR_ID:
                  case LOTUS_OV02A10_SUNNY_SENSOR_ID:
                  case LOTUS_S5K4H7YX_OFILM_SENSOR_ID:
                  case LOTUS_S5K4H7YX_QTECH_SENSOR_ID:
                    cmdInfo->readCMDFunc = read_otp_from_mem;
                    break;
                  default:
                    cmdInfo->readCMDFunc = Common_read_region;
                }


				return 1;
			}
		}
	}
	return 0;

}

static struct stCAM_CAL_CMD_INFO_STRUCT *EEPROM_get_cmd_info_ex
	(unsigned int sensorID, unsigned int deviceID)
{
	int i = 0;

	/* To check device ID */
	for (i = 0; i < CAM_CAL_I2C_MAX_SENSOR; i++) {
		if (g_camCalDrvInfo[i].deviceID == deviceID)
			break;
	}
	/* To check cmd from Sensor ID */

	if (i == CAM_CAL_I2C_MAX_SENSOR) {
		for (i = 0; i < CAM_CAL_I2C_MAX_SENSOR; i++) {
			/* To Set Client */
			if (g_camCalDrvInfo[i].sensorID == 0) {
				PK_DBG("Start get_cmd_info!\n");
				EEPROM_get_cmd_info(sensorID,
					&g_camCalDrvInfo[i]);

				if (g_camCalDrvInfo[i].readCMDFunc != NULL) {
					g_camCalDrvInfo[i].sensorID = sensorID;
					g_camCalDrvInfo[i].deviceID = deviceID;
					PK_DBG("deviceID=%d, SensorID=%x\n",
						deviceID, sensorID);
				}
				break;
			}
		}
	}

	if (i == CAM_CAL_I2C_MAX_SENSOR) {	/*g_camCalDrvInfo is full */
		return NULL;
	} else {
		return &g_camCalDrvInfo[i];
	}
}

/**************************************************
 * EEPROM_HW_i2c_probe
 **************************************************/
static int EEPROM_HW_i2c_probe
	(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclient = client;

	/* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
	g_pstI2Cclient->timing = 100;	/* 100k */
	g_pstI2Cclient->ext_flag &= ~I2C_POLLING_FLAG;
#endif

	/* Default EEPROM Slave Address Main= 0xa0 */
	g_pstI2Cclient->addr = 0x50;
	spin_unlock(&g_spinLock);

	return 0;
}



/**********************************************
 * CAMERA_HW_i2c_remove
 **********************************************/
static int EEPROM_HW_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/***********************************************
 * EEPROM_HW_i2c_probe2
 ***********************************************/
static int EEPROM_HW_i2c_probe2
	(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclient2 = client;

	/* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
	g_pstI2Cclient2->timing = 100;	/* 100k */
	g_pstI2Cclient2->ext_flag &= ~I2C_POLLING_FLAG;
#endif

	/* Default EEPROM Slave Address sub = 0xa8 */
	g_pstI2Cclient2->addr = 0x54;
	spin_unlock(&g_spinLock);

	return 0;
}

/********************************************************
 * CAMERA_HW_i2c_remove2
 ********************************************************/
static int EEPROM_HW_i2c_remove2(struct i2c_client *client)
{
	return 0;
}

/********************************************************
 * EEPROM_HW_i2c_probe3
 ********************************************************/
static int EEPROM_HW_i2c_probe3
	(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclient3 = client;

	/* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
	g_pstI2Cclient3->timing = 100;	/* 100k */
	g_pstI2Cclient3->ext_flag &= ~I2C_POLLING_FLAG;
#endif

	/* Default EEPROM Slave Address Main2 = 0xa4 */
	g_pstI2Cclient3->addr = 0x52;
	spin_unlock(&g_spinLock);

	return 0;
}

/*************************************************************
 * CAMERA_HW_i2c_remove3
 *************************************************************/
static int EEPROM_HW_i2c_remove3(struct i2c_client *client)
{
	return 0;
}

/*************************************************************
 * I2C related variable
 *************************************************************/


static const struct i2c_device_id
	EEPROM_HW_i2c_id[] = { {CAM_CAL_DRV_NAME, 0}, {} };
static const struct i2c_device_id
	EEPROM_HW_i2c_id2[] = { {CAM_CAL_I2C_DEV2_NAME, 0}, {} };
static const struct i2c_device_id
	EEPROM_HW_i2c_id3[] = { {CAM_CAL_I2C_DEV3_NAME, 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id EEPROM_HW_i2c_of_ids[] = {
	{.compatible = "mediatek,camera_main_eeprom",},
	{}
};
#endif

struct i2c_driver EEPROM_HW_i2c_driver = {
	.probe = EEPROM_HW_i2c_probe,
	.remove = EEPROM_HW_i2c_remove,
	.driver = {
		   .name = CAM_CAL_DRV_NAME,
		   .owner = THIS_MODULE,

#ifdef CONFIG_OF
		   .of_match_table = EEPROM_HW_i2c_of_ids,
#endif
		   },
	.id_table = EEPROM_HW_i2c_id,
};

/*********************************************************
 * I2C Driver structure for Sub
 *********************************************************/
#ifdef CONFIG_OF
static const struct of_device_id EEPROM_HW2_i2c_driver_of_ids[] = {
	{.compatible = "mediatek,camera_sub_eeprom",},
	{}
};
#endif

struct i2c_driver EEPROM_HW_i2c_driver2 = {
	.probe = EEPROM_HW_i2c_probe2,
	.remove = EEPROM_HW_i2c_remove2,
	.driver = {
		   .name = CAM_CAL_I2C_DEV2_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = EEPROM_HW2_i2c_driver_of_ids,
#endif
		   },
	.id_table = EEPROM_HW_i2c_id2,
};

/**********************************************************
 * I2C Driver structure for Main2
 **********************************************************/
#ifdef CONFIG_OF
static const struct of_device_id EEPROM_HW3_i2c_driver_of_ids[] = {
	{.compatible = "mediatek,camera_main_two_eeprom",},
	{}
};
#endif

struct i2c_driver EEPROM_HW_i2c_driver3 = {
	.probe = EEPROM_HW_i2c_probe3,
	.remove = EEPROM_HW_i2c_remove3,
	.driver = {
		   .name = CAM_CAL_I2C_DEV3_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = EEPROM_HW3_i2c_driver_of_ids,
#endif
		   },
	.id_table = EEPROM_HW_i2c_id3,
};


/*******************************************************
 * EEPROM_HW_probe
 *******************************************************/
static int EEPROM_HW_probe(struct platform_device *pdev)
{
	i2c_add_driver(&EEPROM_HW_i2c_driver2);
	i2c_add_driver(&EEPROM_HW_i2c_driver3);
	return i2c_add_driver(&EEPROM_HW_i2c_driver);
}

/*******************************************************
 * EEPROM_HW_remove()
 *******************************************************/
static int EEPROM_HW_remove(struct platform_device *pdev)
{
	i2c_del_driver(&EEPROM_HW_i2c_driver);
	i2c_del_driver(&EEPROM_HW_i2c_driver2);
	i2c_del_driver(&EEPROM_HW_i2c_driver3);
	return 0;
}

/******************************************************
 *
 ******************************************************/
static struct platform_device g_platDev = {
	.name = CAM_CAL_DRV_NAME,
	.id = 0,
	.dev = {
		}
};


static struct platform_driver g_stEEPROM_HW_Driver = {
	.probe = EEPROM_HW_probe,
	.remove = EEPROM_HW_remove,
	.driver = {
		   .name = CAM_CAL_DRV_NAME,
		   .owner = THIS_MODULE,
		}
};


/************************************************/

#ifdef CONFIG_COMPAT
static int compat_put_cal_info_struct
	(struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
		struct stCAM_CAL_INFO_STRUCT __user *data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data->u4Offset);
	err |= put_user(i, &data32->u4Offset);
	err |= get_user(i, &data->u4Length);
	err |= put_user(i, &data32->u4Length);
	err |= get_user(i, &data->sensorID);
	err |= put_user(i, &data32->sensorID);
	err |= get_user(i, &data->deviceID);
	err |= put_user(i, &data32->deviceID);

	/* Assume pointer is not change */
#if 1
	err |= get_user(p, (compat_uptr_t *) &data->pu1Params);
	err |= put_user(p, &data32->pu1Params);
#endif
	return err;
}

static int EEPROM_compat_get_info
	(struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
		struct stCAM_CAL_INFO_STRUCT __user *data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->u4Offset);
	err |= put_user(i, &data->u4Offset);
	err |= get_user(i, &data32->u4Length);
	err |= put_user(i, &data->u4Length);
	err |= get_user(i, &data32->sensorID);
	err |= put_user(i, &data->sensorID);
	err |= get_user(i, &data32->deviceID);
	err |= put_user(i, &data->deviceID);

	err |= get_user(p, &data32->pu1Params);
	err |= put_user(compat_ptr(p), &data->pu1Params);

	return err;
}

/*************************************************
 * ioctl
 *************************************************/

static long EEPROM_drv_compat_ioctl
	(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32;
	struct stCAM_CAL_INFO_STRUCT __user *data;
	int err;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {

	case COMPAT_CAM_CALIOC_G_READ:{
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = EEPROM_compat_get_info(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
				CAM_CALIOC_G_READ, (unsigned long)data);
			err = compat_put_cal_info_struct(data32, data);

			if (err != 0)
				PK_ERR("getinfo_struct failed\n");

			return ret;
		}

	case COMPAT_CAM_CALIOC_S_WRITE:{
				/*Note: Write Command is Unverified! */
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = EEPROM_compat_get_info(data32, data);
			if (err)
				return err;

			ret = filp->f_op->unlocked_ioctl(filp,
				CAM_CALIOC_S_WRITE, (unsigned long)data);
			if (err != 0)
				PK_ERR("getinfo_struct failed\n");

			return ret;
		}
	default:
		return -ENOIOCTLCMD;
	}

}

#endif

#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int EEPROM_drv_ioctl(struct inode *a_pstInode,
			    struct file *a_pstFile,
			    unsigned int a_u4Command, unsigned long a_u4Param)
#else
static long EEPROM_drv_ioctl(struct file *file,
	unsigned int a_u4Command, unsigned long a_u4Param)
#endif
{

	int i4RetValue = 0;
	u8 *pBuff = NULL;
	u8 *pu1Params = NULL;
	/*u8 *tempP = NULL; */
	struct stCAM_CAL_INFO_STRUCT *ptempbuf = NULL;
	struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;

#ifdef CAM_CALGETDLT_DEBUG
	struct timeval ktv1, ktv2;
	unsigned long TimeIntervalUS;
#endif
	if (_IOC_DIR(a_u4Command) != _IOC_NONE) {
		pBuff = kmalloc(sizeof(struct stCAM_CAL_INFO_STRUCT),
					GFP_KERNEL);
		if (pBuff == NULL) {
			PK_DBG(" ioctl allocate pBuff mem failed\n");
			return -ENOMEM;
		}

		if (copy_from_user
		    ((u8 *) pBuff, (u8 *) a_u4Param,
				sizeof(struct stCAM_CAL_INFO_STRUCT))) {
			/*get input structure address */
			kfree(pBuff);
			PK_DBG("ioctl copy from user failed\n");
			return -EFAULT;
		}

		ptempbuf = (struct stCAM_CAL_INFO_STRUCT *)pBuff;

		if ((ptempbuf->u4Length <= 0) ||
			(ptempbuf->u4Length > CAM_CAL_MAX_BUF_SIZE)) {
			kfree(pBuff);
			PK_DBG("Buffer Length Error!\n");
			return -EFAULT;
		}

		pu1Params = kmalloc(ptempbuf->u4Length, GFP_KERNEL);

		if (pu1Params == NULL) {
			kfree(pBuff);
			PK_DBG("ioctl allocate pu1Params mem failed\n");
			return -ENOMEM;
		}

		if (copy_from_user
		    ((u8 *) pu1Params, (u8 *) ptempbuf->pu1Params,
		    ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pu1Params);
			PK_DBG("ioctl copy from user failed\n");
			return -EFAULT;
		}
	}
	if (ptempbuf == NULL) {	/*It have to add */
		PK_DBG("ptempbuf is Null !!!");
		return -EFAULT;
	}
	switch (a_u4Command) {

	case CAM_CALIOC_S_WRITE:	/*Note: Write Command is Unverified! */
		PK_DBG("CAM_CALIOC_S_WRITE start!\n");
#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif

		if (g_lastDevID != ptempbuf->deviceID) {
			g_lastDevID = ptempbuf->deviceID;
			if (EEPROM_set_i2c_bus(ptempbuf->deviceID) != 0) {
				PK_DBG("deviceID Error!\n");
				kfree(pBuff);
				kfree(pu1Params);
				return -EFAULT;
			}
		}

		pcmdInf = EEPROM_get_cmd_info_ex(ptempbuf->sensorID,
			ptempbuf->deviceID);

		if (pcmdInf != NULL) {
			if (pcmdInf->writeCMDFunc != NULL) {
				i4RetValue = pcmdInf->writeCMDFunc(
					pcmdInf->client,
					ptempbuf->u4Offset, pu1Params,
					ptempbuf->u4Length);
			} else
				PK_DBG("pcmdInf->writeCMDFunc == NULL\n");
		} else
			PK_DBG("pcmdInf == NULL\n");

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		PK_DBG("Write data %d bytes take %lu us\n",
			ptempbuf->u4Length, TimeIntervalUS);
#endif
		PK_DBG("CAM_CALIOC_S_WRITE End!\n");
		break;

	case CAM_CALIOC_G_READ:
		PK_DBG("CAM_CALIOC_G_READ start! offset=%d, length=%d\n",
			ptempbuf->u4Offset, ptempbuf->u4Length);

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif
        g_cur_device_id = ptempbuf->deviceID >> 1;
        g_cur_sensorid = ptempbuf->sensorID;
		if (g_lastDevID != ptempbuf->deviceID) {
			g_lastDevID = ptempbuf->deviceID;
			if (EEPROM_set_i2c_bus(ptempbuf->deviceID) != 0) {
				PK_DBG("deviceID Error!\n");
				kfree(pBuff);
				kfree(pu1Params);
				return -EFAULT;
			}
		}
		PK_DBG("SensorID=%x DeviceID=%x\n",
			ptempbuf->sensorID, ptempbuf->deviceID);
		pcmdInf = EEPROM_get_cmd_info_ex(
			ptempbuf->sensorID,
			ptempbuf->deviceID);

		if (pcmdInf != NULL) {
			if (pcmdInf->readCMDFunc != NULL)
				i4RetValue =
					pcmdInf->readCMDFunc(pcmdInf->client,
							  ptempbuf->u4Offset,
							  pu1Params,
							  ptempbuf->u4Length);
			else {
				PK_DBG("pcmdInf->readCMDFunc == NULL\n");
			}
		}
#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		PK_DBG("Read data %d bytes take %lu us\n",
			ptempbuf->u4Length, TimeIntervalUS);
#endif
		break;

	default:
		PK_DBG("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	if (_IOC_READ & _IOC_DIR(a_u4Command)) {
		if (copy_to_user
		    ((u8 __user *) ptempbuf->pu1Params, (u8 *) pu1Params,
				ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pu1Params);
			PK_DBG("ioctl copy to user failed\n");
			return -EFAULT;
		}
	}

	kfree(pBuff);
	kfree(pu1Params);
	return i4RetValue;
}

static int EEPROM_drv_open(struct inode *a_pstInode, struct file *a_pstFile)
{
	int ret = 0;

	PK_DBG("EEPROM_drv_open start\n");
	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		PK_DBG("Opened, return -EBUSY\n");
		ret = -EBUSY;
	} else {
		g_drvOpened = 1;
		spin_unlock(&g_spinLock);
	}
	mdelay(2);

	return ret;
}

static int EEPROM_drv_release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_spinLock);
	g_drvOpened = 0;
	spin_unlock(&g_spinLock);

	return 0;
}

static const struct file_operations g_stCAM_CAL_fops1 = {
	.owner = THIS_MODULE,
	.open = EEPROM_drv_open,
	.release = EEPROM_drv_release,
	/*.ioctl = CAM_CAL_Ioctl */
#ifdef CONFIG_COMPAT
	.compat_ioctl = EEPROM_drv_compat_ioctl,
#endif
	.unlocked_ioctl = EEPROM_drv_ioctl
};

/***********************************************
 *
 ***********************************************/

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1
static inline int EEPROM_chrdev_register(void)
{
	struct device *device = NULL;

	PK_DBG("EEPROM_chrdev_register Start\n");

#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_devNum, 0, 1, CAM_CAL_DRV_NAME)) {
		PK_DBG("Allocate device no failed\n");
		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_devNum, 1, CAM_CAL_DRV_NAME)) {
		PK_DBG("Register device no failed\n");
		return -EAGAIN;
	}
#endif

	g_charDrv = cdev_alloc();

	if (g_charDrv == NULL) {
		unregister_chrdev_region(g_devNum, 1);
		PK_DBG("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &g_stCAM_CAL_fops1);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		PK_DBG("Attatch file operation failed\n");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, "CAM_CALdrv1");
	if (IS_ERR(g_drvClass)) {
		int ret = PTR_ERR(g_drvClass);

		PK_DBG("Unable to create class, err = %d\n", ret);
		return ret;
	}
	device = device_create(g_drvClass, NULL, g_devNum, NULL,
		CAM_CAL_DRV_NAME);
	PK_DBG("EEPROM_chrdev_register End\n");

	return 0;
}

static void EEPROM_chrdev_unregister(void)
{
	/*Release char driver */

	class_destroy(g_drvClass);

	device_destroy(g_drvClass, g_devNum);

	cdev_del(g_charDrv);

	unregister_chrdev_region(g_devNum, 1);
}

/***********************************************
 *
 ***********************************************/

static int __init EEPROM_drv_init(void)
{
	PK_DBG("EEPROM_drv_init Start!\n");

	if (platform_driver_register(&g_stEEPROM_HW_Driver)) {
		PK_ERR("failed to register EEPROM driver i2C main\n");
		return -ENODEV;
	}

	if (platform_device_register(&g_platDev)) {
		PK_ERR("failed to register EEPROM device");
		return -ENODEV;
	}

	EEPROM_chrdev_register();

	PK_DBG("EEPROM_drv_init End!\n");
	return 0;
}

static void __exit EEPROM_drv_exit(void)
{

	platform_device_unregister(&g_platDev);
	platform_driver_unregister(&g_stEEPROM_HW_Driver);

	EEPROM_chrdev_unregister();
}
module_init(EEPROM_drv_init);
module_exit(EEPROM_drv_exit);

MODULE_DESCRIPTION("EEPROM Driver");
MODULE_AUTHOR("MM3_SW2");
MODULE_LICENSE("GPL");
