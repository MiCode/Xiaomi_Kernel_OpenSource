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

/*#define CAM_CALGETDLT_DEBUG*/
#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
/*#include <linux/log.h>*/
#define PFX "CAM_CAL_DRV"
#define CAM_CALINF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#define CAM_CALDB(format, args...)     pr_debug(PFX "[%s] " format, __func__, ##args)
#define CAM_CALERR(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define CAM_CALDB(x, ...)
#endif

#define CAM_CAL_DRV_NAME "CAM_CAL_DRV"
#define CAM_CAL_DEV_MAJOR_NUMBER 226

#define CAM_CAL_I2C_MAX_BUSNUM 2
#define CAM_CAL_I2C_MAX_SENSOR 4
#define CAM_CAL_MAX_BUF_SIZE 65536/*For Safety, Can Be Adjested*/

#define CAM_CAL_I2C_DEV1_NAME CAM_CAL_DRV_NAME
#define CAM_CAL_I2C_DEV2_NAME "CAM_CAL_DEV2"
#define CAM_CAL_I2C_DEV3_NAME "CAM_CAL_DEV3"
#define CAM_CAL_I2C_DEV4_NAME "CAM_CAL_DEV4"

static struct i2c_board_info i2cDev1 = { I2C_BOARD_INFO(CAM_CAL_I2C_DEV1_NAME, 0xA2 >> 1)};
static struct i2c_board_info i2cDev2 = { I2C_BOARD_INFO(CAM_CAL_I2C_DEV2_NAME, 0xA2 >> 1)};
static struct i2c_board_info i2cDev3 = { I2C_BOARD_INFO(CAM_CAL_I2C_DEV3_NAME, 0xA2 >> 1)};
static struct i2c_board_info i2cDev4 = { I2C_BOARD_INFO(CAM_CAL_I2C_DEV4_NAME, 0xA2 >> 1)};

static dev_t g_devNum = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static unsigned int g_drvOpened;
static DEFINE_SPINLOCK(g_spinLock); /*for SMP*/

typedef enum {
	I2C_DEV_1 = 0,
	I2C_DEV_2,
	I2C_DEV_3,
	I2C_DEV_4,
	I2C_DEV_MAX,
} CAM_CAL_DEV_ID;

typedef CAM_CAL_DEV_ID cam_cal_dev_id;

static struct i2c_board_info *g_i2c_info[I2C_DEV_MAX] = {&i2cDev1, &i2cDev2, &i2cDev3, &i2cDev4};
static cam_cal_dev_id g_curDevIdx = I2C_DEV_1;

typedef enum {
	BUS_ID_MAIN = 0,
	BUS_ID_SUB,
	BUS_ID_MAIN2,
	BUS_ID_SUB2,
	BUS_ID_MAX,
} CAM_CAL_BUS_ID;

typedef CAM_CAL_BUS_ID cam_cal_bus_id;

static unsigned int g_busNum[BUS_ID_MAX] = {2, 3, 3, 5};
static cam_cal_bus_id g_curBusIdx = BUS_ID_MAIN;

/*Note: Must Mapping to IHalSensor.h*/
enum {
	SENSOR_DEV_NONE = 0x00,
	SENSOR_DEV_MAIN = 0x01,
	SENSOR_DEV_SUB  = 0x02,
	SENSOR_DEV_PIP = 0x03,
	SENSOR_DEV_MAIN_2 = 0x04,
	SENSOR_DEV_MAIN_3D = 0x05,
	SENSOR_DEV_SUB_2 = 0x08,
	SENSOR_DEV_MAX = 0x50
};

static unsigned int g_lastDevID = SENSOR_DEV_NONE;

/*******************************************************************************
*
********************************************************************************/
typedef struct {
	unsigned int sensorID;
	unsigned int deviceID;
	struct i2c_client *client;
	cam_cal_cmd_func readCMDFunc;
	cam_cal_cmd_func writeCMDFunc;
} stCAM_CAL_CMD_INFO_STRUCT, *stPCAM_CAL_CMD_INFO_STRUCT;

static stCAM_CAL_CMD_INFO_STRUCT g_camCalDrvInfo[CAM_CAL_I2C_MAX_SENSOR];

/*******************************************************************************
*
********************************************************************************/

static int cam_cal_set_i2c_bus(unsigned int deviceID)
{
	switch (deviceID) {
	case SENSOR_DEV_MAIN:
		g_curBusIdx = BUS_ID_MAIN;
		g_curDevIdx = I2C_DEV_1;
		break;
	case SENSOR_DEV_SUB:
		g_curBusIdx = BUS_ID_SUB;
		g_curDevIdx = I2C_DEV_2;
		break;
	case SENSOR_DEV_MAIN_2:
		g_curBusIdx = BUS_ID_MAIN2;
		g_curDevIdx = I2C_DEV_3;
		break;
	case SENSOR_DEV_SUB_2:
		g_curBusIdx = BUS_ID_SUB2;
		g_curDevIdx = I2C_DEV_4;
	default:
		return 1;
	}
	CAM_CALDB("cam_cal_set_i2c_bus end! deviceID=%d g_curBusIdx=%d g_curDevIdx=%d\n",
		deviceID, g_curBusIdx, g_curDevIdx);

	return 0;

}

static int cam_cal_get_i2c_client(struct i2c_board_info *i2c_info,
				  struct i2c_client **client)
{
	const unsigned short addr_list[] = {
		i2c_info->addr,
		I2C_CLIENT_END
	};
	struct i2c_adapter *adapt = NULL;

	if (*client == NULL) {
		CAM_CALDB("i2c_info->addr ==%x, register i2c g_busNum[%d]=%d\n", i2c_info->addr,
			g_curBusIdx, g_busNum[g_curBusIdx]);

		adapt = i2c_get_adapter(g_busNum[g_curBusIdx]);
		if (adapt == NULL) {
			CAM_CALDB("failed to get adapter i2c busID=%d\n", g_busNum[g_curBusIdx]);
			return 0;
		} else {
			CAM_CALDB("g_adapt!=NULL, register i2c %d start !\n", g_curBusIdx);
			*client = i2c_new_probed_device(adapt, i2c_info, addr_list, NULL);

			i2c_put_adapter(adapt);

			if (!(*client)) {
				CAM_CALDB("failed to get client i2c busID=%d\n", g_busNum[g_curBusIdx]);
				return 0;
			}
		}
	}
	return 1;

}

static int cam_cal_get_cmd_info(unsigned int sensorID, stCAM_CAL_CMD_INFO_STRUCT *cmdInfo)
{
	stCAM_CAL_LIST_STRUCT *pCamCalList = NULL;
	stCAM_CAL_FUNC_STRUCT *pCamCalFunc = NULL;
	int i = 0, j = 0;

	cam_cal_get_sensor_list(&pCamCalList);
	cam_cal_get_func_list(&pCamCalFunc);
	if (pCamCalList != NULL && pCamCalFunc != NULL) {
		CAM_CALDB("pCamCalList!=NULL && pCamCalFunc!= NULL\n");
		for (i = 0; pCamCalList[i].sensorID != 0; i++) {
			CAM_CALDB("pCamCalList[%d].sensorID==%x\n", i, pCamCalList[i].sensorID);
			if (pCamCalList[i].sensorID == sensorID) {
				(*g_i2c_info[g_curDevIdx]).addr = pCamCalList[i].slaveID >> 1;
				CAM_CALDB("g_i2c_info[%d].addr =%x\n", g_curDevIdx,
					  (*g_i2c_info[g_curDevIdx]).addr);
				if (cam_cal_get_i2c_client(g_i2c_info[g_curDevIdx], &(cmdInfo->client))) {
					for (j = 0; pCamCalFunc[j].cmdType != CMD_NONE; j++) {
						CAM_CALDB("pCamCalFunc[%d].cmdType = %d,\
						pCamCalList[%d].cmdType = %d\n",\
						j, pCamCalFunc[j].cmdType, i,\
						pCamCalList[i].cmdType);
				if (pCamCalFunc[j].cmdType == pCamCalList[i].cmdType
						    || pCamCalList[i].cmdType == CMD_AUTO) {
					if (pCamCalList[i].checkFunc != NULL) {
					if (pCamCalList[i].checkFunc(cmdInfo->client,
						pCamCalFunc[j].readCamCalData)) {
					CAM_CALDB("pCamCalList[%d].checkFunc ok!\n", i);
					cmdInfo->readCMDFunc = pCamCalFunc[j].readCamCalData;
					/*151101=Write Command Unverified*/
					/*cmdInfo->writeCMDFunc = pCamCalFunc[j].writeCamCalData;*/
					return 1;
					} else if (pCamCalList[i].cmdType == CMD_AUTO) {
						CAM_CALDB("reset i2c\n");
						i2c_unregister_device(cmdInfo->client);
						cmdInfo->client = NULL;
						cam_cal_get_i2c_client(g_i2c_info[g_curDevIdx],
						&(cmdInfo->client));
					}
					}
				}
					}
					if (cmdInfo->client != NULL) {
						i2c_unregister_device(cmdInfo->client);
						CAM_CALDB("unregister i2c\n");
						cmdInfo->client = NULL;
					}
				} else {
					int infoDeciceID = 0;

					switch (pCamCalList[i].cmdType) {

					case CMD_MAIN:
						infoDeciceID = SENSOR_DEV_MAIN;
						break;
					case CMD_MAIN2:
						infoDeciceID = SENSOR_DEV_MAIN_2;
						break;
					case CMD_SUB:
						infoDeciceID = SENSOR_DEV_SUB;
						break;
					case CMD_SUB2:
						infoDeciceID = SENSOR_DEV_SUB_2;
						break;
					default:
						CAM_CALDB("cmdType=%d error.\n", pCamCalList[i].cmdType);
						return 0;
					}
					for (j = 0; j < CAM_CAL_I2C_MAX_SENSOR; j++) {
					if (g_camCalDrvInfo[j].deviceID == infoDeciceID) {
						CAM_CALDB("g_camCalDrvInfo[%d].deviceID = %x!\n", j,\
						g_camCalDrvInfo[j].deviceID);
						break;
					}
					}
					if (j < CAM_CAL_I2C_MAX_SENSOR &&
						pCamCalList[j].checkFunc(g_camCalDrvInfo[j].client,
						g_camCalDrvInfo[j].readCMDFunc)) {
						cmdInfo->client = g_camCalDrvInfo[j].client;
						cmdInfo->readCMDFunc = g_camCalDrvInfo[j].readCMDFunc;
						return 1;
					} else
						CAM_CALDB("Check infoDeciceID=%d cmdInfo failed\n", infoDeciceID);
				}
			}
		}
	}
	return 0;

}

static stCAM_CAL_CMD_INFO_STRUCT *cam_cal_get_cmd_info_ex(unsigned int sensorID,
	unsigned int deviceID)
{
	int i = 0;

	for (i = 0; i < CAM_CAL_I2C_MAX_SENSOR; i++) {
		if (g_camCalDrvInfo[i].deviceID == deviceID) {
			CAM_CALDB("g_camCalDrvInfo[%d].deviceID == deviceID == %x!\n", i, deviceID);
			break;
		}
	}

	if (i == CAM_CAL_I2C_MAX_SENSOR) {
		for (i = 0; i < CAM_CAL_I2C_MAX_SENSOR; i++) {
			if (g_camCalDrvInfo[i].sensorID == 0) {
				CAM_CALDB("g_camCalDrvInfo[%d].sensorID == 0, start get_cmd_info!\n", i);
				cam_cal_get_cmd_info(sensorID, &g_camCalDrvInfo[i]);

				if (g_camCalDrvInfo[i].readCMDFunc != NULL) {
					g_camCalDrvInfo[i].sensorID = sensorID;
					g_camCalDrvInfo[i].deviceID = deviceID;
					CAM_CALDB("deviceID=%d, SensorID=%x, BusID=%d\n",
						deviceID, sensorID, g_busNum[g_curBusIdx]);
				}
				break;
			}
		}
	}

	if (i == CAM_CAL_I2C_MAX_SENSOR) { /*g_camCalDrvInfo is full*/
		return NULL;
	} else {
		return &g_camCalDrvInfo[i];
	}
}


/*******************************************************************************
*
********************************************************************************/
static struct platform_device g_platDev = {
	.name = CAM_CAL_DRV_NAME,
	.id = 0,
	.dev = {
	}
};


static struct platform_driver g_platDrv = {
	.driver     = {
		.name   = CAM_CAL_DRV_NAME,
		.owner  = THIS_MODULE,
	}
};


#ifdef CONFIG_COMPAT
static int compat_put_cal_info_struct(
	COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
	stCAM_CAL_INFO_STRUCT __user *data)
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
	err |= get_user(p, (compat_uptr_t *)&data->pu1Params);
	err |= put_user(p, &data32->pu1Params);
#endif
	return err;
}

static int cam_cal_compat_get_info(
	COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
	stCAM_CAL_INFO_STRUCT __user *data)
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

static long cam_cal_drv_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	COMPAT_stCAM_CAL_INFO_STRUCT __user *data32;
	stCAM_CAL_INFO_STRUCT __user *data;
	int err;
/*
	CAM_CALDB("cam_cal_drv_compat_ioctl Start!,%p %p %x ioc size %d\n", filp->f_op ,
		  filp->f_op->unlocked_ioctl, cmd, _IOC_SIZE(cmd));
*/
	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {

	case COMPAT_CAM_CALIOC_G_READ: {
		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = cam_cal_compat_get_info(data32, data);
		if (err)
			return err;

		ret = filp->f_op->unlocked_ioctl(filp, CAM_CALIOC_G_READ,
						 (unsigned long)data);
		err = compat_put_cal_info_struct(data32, data);

		if (err != 0)
			CAM_CALERR("compat_put_acdk_sensor_getinfo_struct failed\n");

		return ret;
	}

	case COMPAT_CAM_CALIOC_S_WRITE: {/*Note: Write Command is Unverified!*/
		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = cam_cal_compat_get_info(data32, data);
		if (err)
			return err;

		ret = filp->f_op->unlocked_ioctl(filp, CAM_CALIOC_S_WRITE,
						 (unsigned long)data);
		/*err = compat_put_cal_info_struct(data32, data);*/
		/*151122=write won't need*/

		if (err != 0)
			CAM_CALERR("compat_put_acdk_sensor_getinfo_struct failed\n");

		return ret;
	}
	default:
		return -ENOIOCTLCMD;
	}

}

#endif

#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int cam_cal_drv_ioctl(struct inode *a_pstInode,
			     struct file *a_pstFile,
			     unsigned int a_u4Command,
			     unsigned long a_u4Param)
#else
static long cam_cal_drv_ioctl(
	struct file *file,
	unsigned int a_u4Command,
	unsigned long a_u4Param
)
#endif
{

	int i4RetValue = 0;
	u8 *pBuff = NULL;
	u8 *pu1Params = NULL;
	/*u8 *tempP = NULL;*/
	stCAM_CAL_INFO_STRUCT *ptempbuf = NULL;
	stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;

#ifdef CAM_CALGETDLT_DEBUG
	struct timeval ktv1, ktv2;
	unsigned long TimeIntervalUS;
#endif

	if (_IOC_NONE != _IOC_DIR(a_u4Command)) {
		pBuff = kmalloc(sizeof(stCAM_CAL_INFO_STRUCT), GFP_KERNEL);
		if (NULL == pBuff) {
			CAM_CALDB(" ioctl allocate pBuff mem failed\n");
			return -ENOMEM;
		}

		if (copy_from_user((u8 *) pBuff , (u8 *) a_u4Param, sizeof(stCAM_CAL_INFO_STRUCT))) {
			/*get input structure address*/
			kfree(pBuff);
			CAM_CALDB("ioctl copy from user failed\n");
			return -EFAULT;
		}

		ptempbuf = (stCAM_CAL_INFO_STRUCT *)pBuff;

		if ((ptempbuf->u4Length <= 0) || (ptempbuf->u4Length > CAM_CAL_MAX_BUF_SIZE)) {
			kfree(pBuff);
			CAM_CALDB("Buffer Length Error!\n");
			return -EFAULT;
		}

		pu1Params = kmalloc(ptempbuf->u4Length, GFP_KERNEL);

		if (NULL == pu1Params) {
			kfree(pBuff);
			CAM_CALDB("ioctl allocate pu1Params mem failed\n");
			return -ENOMEM;
		}
/*
		CAM_CALDB("init Working buffer address=0x%p, length=%d, command=0x%x, sensorID=%x\n",
			pu1Params, ptempbuf->u4Length, a_u4Command, ptempbuf->sensorID);
*/
		if (copy_from_user((u8 *)pu1Params, (u8 *)ptempbuf->pu1Params, ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pu1Params);
			CAM_CALDB("ioctl copy from user failed\n");
			return -EFAULT;
		}

	}


	switch (a_u4Command) {

	case CAM_CALIOC_S_WRITE:/*Note: Write Command is Unverified!*/
		CAM_CALDB("CAM_CALIOC_S_WRITE start!\n");
#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif

	if (g_lastDevID != ptempbuf->deviceID) {
		g_lastDevID = ptempbuf->deviceID;
		if (cam_cal_set_i2c_bus(ptempbuf->deviceID)) {
			CAM_CALDB("deviceID Error!\n");
			kfree(pBuff);
			kfree(pu1Params);
			return -EFAULT;
		}
	}

	pcmdInf = cam_cal_get_cmd_info_ex(ptempbuf->sensorID, ptempbuf->deviceID);

	if (pcmdInf != NULL) {
		/*CAM_CALDB("pcmdInf != NULL\n");*/

		if (pcmdInf->writeCMDFunc != NULL) {
			/*CAM_CALDB("before write offset=%d,pu1Params=%x, length=%d\n",
				ptempbuf->u4Offset, *pu1Params, ptempbuf->u4Length);*/
			i4RetValue = pcmdInf->writeCMDFunc(pcmdInf->client,
				ptempbuf->u4Offset, pu1Params, ptempbuf->u4Length);
		} else
			CAM_CALDB("pcmdInf->writeCMDFunc == NULL\n");
	} else
		CAM_CALDB("pcmdInf == NULL\n");

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		CAM_CALDB("Write data %d bytes take %lu us\n", ptempbuf->u4Length, TimeIntervalUS);
#endif
		CAM_CALDB("CAM_CALIOC_S_WRITE End!\n");
		break;

	case CAM_CALIOC_G_READ:
		CAM_CALDB("CAM_CALIOC_G_READ start! offset=%d, length=%d\n", ptempbuf->u4Offset,
			  ptempbuf->u4Length);

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif

		if (g_lastDevID != ptempbuf->deviceID) {
			g_lastDevID = ptempbuf->deviceID;
			cam_cal_set_i2c_bus(ptempbuf->deviceID);
		}

		pcmdInf = cam_cal_get_cmd_info_ex(ptempbuf->sensorID, ptempbuf->deviceID);

		if (pcmdInf != NULL) {
			/*CAM_CALDB("pcmdInf != NULL\n");*/
			if (pcmdInf->readCMDFunc != NULL)
				i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client,
					ptempbuf->u4Offset, pu1Params, ptempbuf->u4Length);
			else {
				CAM_CALDB("pcmdInf->readCMDFunc == NULL\n");
			}
		}

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		CAM_CALDB("Read data %d bytes take %lu us\n", ptempbuf->u4Length, TimeIntervalUS);
#endif
		/*CAM_CALDB("CAM_CALIOC_G_READ End!\n");*/
		break;

	default:
		CAM_CALDB("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	if (_IOC_READ & _IOC_DIR(a_u4Command)) {
		/*copy data to user space buffer, keep other input paremeter unchange.*/
		/*CAM_CALDB("to user length %d, Working buffer address 0x%p\n", ptempbuf->u4Length, pu1Params);*/
		if (copy_to_user((u8 __user *) ptempbuf->pu1Params , (u8 *)pu1Params , ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pu1Params);
			CAM_CALDB("ioctl copy to user failed\n");
			return -EFAULT;
		}
	}

	kfree(pBuff);
	kfree(pu1Params);
	return i4RetValue;
}

static int cam_cal_drv_open(struct inode *a_pstInode, struct file *a_pstFile)
{
	CAM_CALDB("cam_cal_drv_open start\n");
	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		CAM_CALDB("Opened, return -EBUSY\n");
		return -EBUSY;
	} else
		g_drvOpened = 1;

	spin_unlock(&g_spinLock);
	mdelay(2);

	return 0;
}

static int cam_cal_drv_release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_spinLock);
	g_drvOpened = 0;
	spin_unlock(&g_spinLock);

	return 0;
}

static const struct file_operations g_stCAM_CAL_fops1 = {
	.owner = THIS_MODULE,
	.open = cam_cal_drv_open,
	.release = cam_cal_drv_release,
	/*.ioctl = CAM_CAL_Ioctl*/
#ifdef CONFIG_COMPAT
	.compat_ioctl = cam_cal_drv_compat_ioctl,
#endif
	.unlocked_ioctl = cam_cal_drv_ioctl
};

/*******************************************************************************
*
********************************************************************************/

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1
static inline int cam_cal_chrdev_register(void)
{
	struct device *device = NULL;

	CAM_CALDB("cam_cal_chrdev_register Start\n");

#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_devNum, 0, 1, CAM_CAL_DRV_NAME)) {
		CAM_CALDB("Allocate device no failed\n");
		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_devNum, 1, CAM_CAL_DRV_NAME)) {
		CAM_CALDB("Register device no failed\n");
		return -EAGAIN;
	}
#endif

	g_charDrv = cdev_alloc();

	if (NULL == g_charDrv) {
		unregister_chrdev_region(g_devNum, 1);
		CAM_CALDB("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &g_stCAM_CAL_fops1);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		CAM_CALDB("Attatch file operation failed\n");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, "CAM_CALdrv1");
	if (IS_ERR(g_drvClass)) {
		int ret = PTR_ERR(g_drvClass);

		CAM_CALDB("Unable to create class, err = %d\n", ret);
		return ret;
	}
	device = device_create(g_drvClass, NULL, g_devNum, NULL, CAM_CAL_DRV_NAME);
	CAM_CALDB("cam_cal_chrdev_register End\n");

	return 0;
}

static void cam_cal_chrdev_unregister(void)
{
	/*Release char driver*/

	class_destroy(g_drvClass);

	device_destroy(g_drvClass, g_devNum);

	cdev_del(g_charDrv);

	unregister_chrdev_region(g_devNum, 1);
}

/*******************************************************************************
*
********************************************************************************/

static int __init cam_cal_drv_init(void)
{
	struct device_node *node1;

	CAM_CALDB("cam_cal_drv_init Start!\n");

	node1 = of_find_compatible_node(NULL, NULL, "mediatek,cam_cal_drv");
	if (node1) {
		of_property_read_u32(node1, "main_bus", &g_busNum[BUS_ID_MAIN]);
		of_property_read_u32(node1, "sub_bus", &g_busNum[BUS_ID_SUB]);
		of_property_read_u32(node1, "main2_bus", &g_busNum[BUS_ID_MAIN2]);
		of_property_read_u32(node1, "sub2_bus", &g_busNum[BUS_ID_SUB2]);
	}

	if (platform_driver_register(&g_platDrv)) {
		CAM_CALDB("failed to register CAM_CAL driver1\n");
		return -ENODEV;
	}

	if (platform_device_register(&g_platDev)) {
		CAM_CALDB("failed to register CAM_CAL device1\n");
		return -ENODEV;
	}

	cam_cal_chrdev_register();

	CAM_CALDB("cam_cal_drv_init End!\n");
	return 0;
}

static void __exit cam_cal_drv_exit(void)
{
	platform_device_unregister(&g_platDev);

	platform_driver_unregister(&g_platDrv);

	cam_cal_chrdev_unregister();
}

module_init(cam_cal_drv_init);
module_exit(cam_cal_drv_exit);

MODULE_DESCRIPTION("CAM_CAL Driver");
MODULE_AUTHOR("LukeHu <luke.hu@mediatek.com>");
MODULE_LICENSE("GPL");


