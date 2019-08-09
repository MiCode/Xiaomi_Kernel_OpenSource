/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#define PFX "CAM_CAL"
//#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

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
#include "eeprom_i2c_dev.h"
#include "eeprom_i2c_common_driver.h"
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
#define PK_INF(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define PK_DBG(format, args...) \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#define PK_ERR(format, args...) \
	pr_err(PFX "[%s] " format, __func__, ##args)
#else
#define PK_INF(format, args...)
#define PK_DBG(format, args...)
#define PK_ERR(format, args...) \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#endif

#define CAM_CAL_DRV_NAME "CAM_CAL_DRV"
#define CAM_CAL_DEV_MAJOR_NUMBER 226

#define CAM_CAL_MAX_BUF_SIZE 65536	/*For Safety, Can Be Adjested */

#define CAM_CAL_I2C_DEV1_NAME CAM_CAL_DRV_NAME
#define CAM_CAL_I2C_DEV2_NAME "CAM_CAL_DEV2"
#define CAM_CAL_I2C_DEV3_NAME "CAM_CAL_DEV3"
#define CAM_CAL_I2C_DEV4_NAME "CAM_CAL_DEV4"

static dev_t g_devNum = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static unsigned int g_drvOpened;
static struct i2c_client *g_pstI2Cclients[I2C_DEV_IDX_MAX] = { NULL };


#include <linux/hardware_info.h>
#include "kd_imgsensor.h"
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainData = NULL;
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromSubData = NULL;
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMain2Data = NULL;
extern int hardwareinfo_set_prop(int cmd, const char *name);


static DEFINE_SPINLOCK(g_spinLock);	/*for SMP */


/*Note: Must Mapping to IHalSensor.h*/
enum {
	SENSOR_DEV_NONE = 0x00,
	SENSOR_DEV_MAIN = 0x01,
	SENSOR_DEV_SUB = 0x02,
	SENSOR_DEV_PIP = 0x03,
	SENSOR_DEV_MAIN_2 = 0x04,
	SENSOR_DEV_MAIN_3D = 0x05,
	SENSOR_DEV_SUB_2 = 0x08,
	SENSOR_DEV_MAIN_3 = 0x10,
	SENSOR_DEV_MAX = 0x50
};

static unsigned int g_lastDevID = SENSOR_DEV_NONE;

/***********************************************************
 *
 ***********************************************************/
struct stCAM_CAL_CMD_INFO_STRUCT {
	unsigned int sensorID;
	unsigned int deviceID;
	unsigned int i2cAddr;
	struct i2c_client *client;
	cam_cal_cmd_func readCMDFunc;
	cam_cal_cmd_func writeCMDFunc;
	unsigned int maxEepromSize;
};

static struct stCAM_CAL_CMD_INFO_STRUCT g_camCalDrvInfo[CAM_CAL_SENSOR_IDX_MAX];

/********************************************************************
 * EEPROM_set_i2c_bus()
 * To i2c client and slave id
 ********************************************************************/

static int EEPROM_set_i2c_bus(unsigned int deviceID,
			      struct stCAM_CAL_CMD_INFO_STRUCT *cmdInfo)
{
	enum CAM_CAL_SENSOR_IDX idx;
	struct i2c_client *client;

	switch (deviceID) {
	case SENSOR_DEV_MAIN:
		idx = CAM_CAL_SENSOR_IDX_MAIN;
		break;
	case SENSOR_DEV_SUB:
		idx = CAM_CAL_SENSOR_IDX_SUB;
		break;
	case SENSOR_DEV_MAIN_2:
		idx = CAM_CAL_SENSOR_IDX_MAIN2;
		break;
	case SENSOR_DEV_SUB_2:
		idx = CAM_CAL_SENSOR_IDX_SUB2;
		break;
	case SENSOR_DEV_MAIN_3:
		idx = CAM_CAL_SENSOR_IDX_MAIN3;
		break;
	default:
		return -EFAULT;
	}
	client = g_pstI2Cclients[get_i2c_dev_sel(idx)];
	pr_debug("%s end! deviceID=%d index=%u client=%p\n",
		 __func__, deviceID, idx, client);

	if (client == NULL) {
		pr_err("i2c client is NULL");
		return -EFAULT;
	}

	if (cmdInfo != NULL) {
		client->addr = cmdInfo->i2cAddr;
		cmdInfo->client = client;
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
		pr_debug("pCamCalList!=NULL && pCamCalFunc!= NULL\n");
		for (i = 0; pCamCalList[i].sensorID != 0; i++) {
			if (pCamCalList[i].sensorID == sensorID) {
				pr_debug("pCamCalList[%d].sensorID==%x\n", i,
				       pCamCalList[i].sensorID);

				cmdInfo->i2cAddr = pCamCalList[i].slaveID >> 1;
				cmdInfo->readCMDFunc =
					pCamCalList[i].readCamCalData;
				cmdInfo->maxEepromSize =
					pCamCalList[i].maxEepromSize;

				/*
				 * Default 8K for Common_read_region driver
				 * 0 for others
				 */
				if (cmdInfo->readCMDFunc == Common_read_region
				    && cmdInfo->maxEepromSize == 0) {
					cmdInfo->maxEepromSize =
						DEFAULT_MAX_EEPROM_SIZE_8K;
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
	for (i = 0; i < CAM_CAL_SENSOR_IDX_MAX; i++) {
		if (g_camCalDrvInfo[i].deviceID == deviceID)
			break;
	}
	/* To check cmd from Sensor ID */

	if (i == CAM_CAL_SENSOR_IDX_MAX) {
		for (i = 0; i < CAM_CAL_SENSOR_IDX_MAX; i++) {
			/* To Set Client */
			if (g_camCalDrvInfo[i].sensorID == 0) {
				pr_debug("Start get_cmd_info!\n");
				EEPROM_get_cmd_info(sensorID,
					&g_camCalDrvInfo[i]);

				if (g_camCalDrvInfo[i].readCMDFunc != NULL) {
					g_camCalDrvInfo[i].sensorID = sensorID;
					g_camCalDrvInfo[i].deviceID = deviceID;
					pr_debug("deviceID=%d, SensorID=%x\n",
						deviceID, sensorID);
				}
				break;
			}
		}
	}

	if (i == CAM_CAL_SENSOR_IDX_MAX) {	/*g_camCalDrvInfo is full */
		return NULL;
	} else {
		return &g_camCalDrvInfo[i];
	}
}


static  void EEPROM_reset_cmd_info_ex
	(unsigned int sensorID, unsigned int deviceID){
	int i = 0;
	for (i = 0; i < CAM_CAL_SENSOR_IDX_MAX; i++) {
		if ((g_camCalDrvInfo[i].deviceID == deviceID)&&(sensorID == g_camCalDrvInfo[i].sensorID)){
			g_camCalDrvInfo[i].deviceID = 0;
			g_camCalDrvInfo[i].sensorID = 0;
		}
	}
	
}
//dump eeprom data start
void dumpEEPROMData(int u4Length,u8* pu1Params)
{
#if 0
	int i = 0;
	for(i = 0; i < u4Length; i += 16){
		if(u4Length - i  >= 16){
			PK_INF("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
			i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
			,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
			,pu1Params[i+15]);
		}else{
			int j = i;
			for(;j < u4Length;j++)
			PK_INF("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
		}
	}
	PK_INF("\n");
#endif
}					 
//dump eeprom data end	
#define MAX_EFUSE_ID_LENGTH  (64)
static u8 imgSensorStaticEfuseID[CAM_CAL_SENSOR_IDX_MAX][MAX_EFUSE_ID_LENGTH] ={{0},
                                   {0},
                                   {0},
                                   {0}};
void imgSensorSetDataEfuseID(u8*buf,u32 deviceID, u32 length)
{
   u8 efuseIDStr[MAX_EFUSE_ID_LENGTH] = {0};
   int index = 0;
   int len = 0;
   
   if(buf == NULL){
   	 PK_ERR("invalid buffer,please check your driver\n");
	 return ;
   }
   if((deviceID != 1)&&(deviceID != 2)&&(deviceID != 4))
   {
   	 PK_ERR("invalid deviceID(%d),please check your driver\n",deviceID);
	 return ;
   }
   memset(efuseIDStr,0,MAX_EFUSE_ID_LENGTH);
   for(index = 0;(index < length)&&(index < MAX_EFUSE_ID_LENGTH);index++){
     len += snprintf(efuseIDStr+len,sizeof(efuseIDStr),"%02x",buf[index]);
	 //PK_INF("deviceID = %d,len = %d,buf[%d]=%02x\n",deviceID,len,index,buf[index]);
   }
   PK_INF("deviceID = %d,efuseIDStr = %s\n",deviceID,efuseIDStr);
   index = deviceID/2;
   memset((char*)&imgSensorStaticEfuseID[index][0],0,MAX_EFUSE_ID_LENGTH);
   strncpy((char*)&imgSensorStaticEfuseID[index][0],(char*)efuseIDStr,strlen((char*)efuseIDStr));
   
   if(deviceID == 1){
   	  hardwareinfo_set_prop(HARDWARE_BACK_CAM_EFUSEID,(char*)efuseIDStr);
   }else if (deviceID == 2){
      hardwareinfo_set_prop(HARDWARE_FRONT_CAME_EFUSEID,(char*)efuseIDStr);
   }else if(deviceID == 4){
      hardwareinfo_set_prop(HARDWARE_BCAK_SUBCAM_EFUSEID,(char*)efuseIDStr);
   } 
}
u8 *getImgSensorEfuseID(u32 sensorID){ /*deviceID is match the SensorId,0,1,2,3*/
   if(sensorID > (CAM_CAL_SENSOR_IDX_MAX - 1)){
   	  PK_ERR("invalid deviceID = %d\n",sensorID);
   	  return NULL;
   }
   return &imgSensorStaticEfuseID[sensorID][0];
}

int imgSensorCheckEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* cData){
	int i = 0;
	int length = 0;
	int count;
	u32 sum = 0;
	u8* buffer = NULL;
	if((pData != NULL)&&(pData->dataBuffer != NULL)&&(cData != NULL)){
		buffer = pData->dataBuffer;
		//verity validflag and checksum
		for((count = 0);count < MAX_ITEM;count++){
			if((cData[count].item < MAX_ITEM)){
				if(cData[count].flagAdrees != cData[count].startAdress){
					if(buffer[cData[count].flagAdrees]!= cData[count].validFlag){
						PK_ERR("invalid otp data cItem=%d,flag=%d failed\n", cData[count].item,buffer[cData[count].flagAdrees]);
						return -ENODEV;
					} else {
						PK_INF("check cTtem=%d,flag=%d otp flag data successful!\n", cData[count].item,buffer[cData[count].flagAdrees]);
					}
				}
				sum = 0;
				length = cData[count].endAdress - cData[count].startAdress;
				for(i = 0;i <= length;i++){
					sum += buffer[cData[count].startAdress+i];
				}
				if(((sum%0xff)+1)!= buffer[cData[count].checksumAdress]){
					PK_ERR("checksum cItem=%d,0x%x,length = 0x%x failed\n",cData[count].item,sum,length);
					return -ENODEV;
				} else {
					PK_INF("checksum cItem=%d,0x%x,length = 0x%x successful!\n",cData[count].item,sum,length);
				}
			}else{
				break;
			}
		}	
	}else{
		PK_ERR("some data not inited!\n");
		return -ENODEV;
	}

	PK_INF("sensor[0x%x][0x%x] eeprom checksum success\n", pData->sensorID, pData->deviceID);

	return 0;
}

int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checkData){
    struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;
	int i4RetValue = -1;
    u32 vendorID = 0;
    u8 tmpBuf[4] = {0};
    u8 efuseIDBuf[16] = {0};
	int efuseCount = 16;
	int index = 0;

	if((pData == NULL)||(checkData == NULL)){
		PK_ERR("pData or checkData not inited!\n");
		return -EFAULT;
	}

	PK_INF("SensorID=%x DeviceID=%x\n",pData->sensorID, pData->deviceID);

	pcmdInf = EEPROM_get_cmd_info_ex(pData->sensorID,pData->deviceID);
	if (pcmdInf != NULL) {
    if(EEPROM_set_i2c_bus(pData->deviceID,pcmdInf) != 0) {
      PK_ERR("deviceID Error!\n");
      return -EFAULT;
    }
		if (pcmdInf->readCMDFunc != NULL){
			if (pData->dataBuffer == NULL){
				pData->dataBuffer = kmalloc(pData->dataLength, GFP_KERNEL);
				if (pData->dataBuffer == NULL) {
					PK_ERR("pData->dataBuffer is malloc fail\n");
					return -EFAULT;
				}
			}
			//verity vendorID
			i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, pData->vendorByte[0], &tmpBuf[0], 1);
			if(i4RetValue != 1){
				PK_ERR("vendorID read falied 0x%x != 0x%x\n",tmpBuf[0], pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
				return -EFAULT;
			}
			vendorID = tmpBuf[0];
			if(vendorID != pData->sensorVendorid >> 24){
				PK_ERR("vendorID cmp falied 0x%x != 0x%x\n",vendorID, pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
				return -EFAULT;
			}


			//get eeprom data
			i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, 0, pData->dataBuffer, pData->dataLength);
			if(i4RetValue != pData->dataLength){
				kfree(pData->dataBuffer);
				pData->dataBuffer = NULL;
				PK_ERR("readCMDFunc failed\n");
				EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
				return -EFAULT;
			}else{
				if(imgSensorCheckEepromData(pData,checkData) != 0){
					kfree(pData->dataBuffer);
					pData->dataBuffer = NULL;
					PK_ERR("checksum failed\n");
					EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
					return -EFAULT;
				}
				PK_INF("SensorID=%x DeviceID=%x read otp data success\n",pData->sensorID, pData->deviceID);
				//set efuse id start
                memset(efuseIDBuf,0,16);
				if(pData->sensorID == CEREUS_IMX486_SUNNY_SENSOR_ID){	
				   efuseCount = 11;  
				}else if(pData->sensorID == CACTUS_S5K3L8_SUNNY_SENSOR_ID){
				   efuseCount = 6;  
				}
				for(index = 0;index < efuseCount;index++){
					efuseIDBuf[index] = pData->dataBuffer[0x0f+efuseCount -index];
				}
				imgSensorSetDataEfuseID((u8*)efuseIDBuf,pData->deviceID,efuseCount);
				//det efuse id end
			}
		} else {
			PK_ERR("pcmdInf->readCMDFunc == NULL\n");
		}
	} else {
		PK_ERR("pcmdInf == NULL\n");
    }

	return i4RetValue;
}

int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData){
	int i4RetValue = 0;
	PK_INF("pData->deviceID = %d\n",pData->deviceID);
    if(pData->deviceID == 0x01){
		if(g_eepromMainData != NULL){
			return -ETXTBSY;
		}
		g_eepromMainData = pData;
    }
	else if(pData->deviceID == 0x02) {
		if(g_eepromSubData != NULL){
			return -ETXTBSY;
		}
		g_eepromSubData = pData;
	}
	else if(pData->deviceID == 0x04) {
		if(g_eepromMain2Data != NULL){
			return -ETXTBSY;
		}
		g_eepromMain2Data = pData;
	}else{
	    PK_ERR("we don't have this devices\n");
	    return -ENODEV;
	}
    if(pData->dataBuffer)
	   dumpEEPROMData(pData->dataLength,pData->dataBuffer);
	return i4RetValue;
}


/**************************************************
 * EEPROM_HW_i2c_probe
 **************************************************/
static int EEPROM_HW_i2c_probe
	(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclients[I2C_DEV_IDX_1] = client;

	/* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
	g_pstI2Cclients[I2C_DEV_IDX_1]->timing = gi2c_dev_timing[I2C_DEV_IDX_1];
	g_pstI2Cclients[I2C_DEV_IDX_1]->ext_flag &= ~I2C_POLLING_FLAG;
#endif

	/* Default EEPROM Slave Address Main= 0xa0 */
	g_pstI2Cclients[I2C_DEV_IDX_1]->addr = 0x50;
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
	g_pstI2Cclients[I2C_DEV_IDX_2] = client;

	/* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
	g_pstI2Cclients[I2C_DEV_IDX_2]->timing = gi2c_dev_timing[I2C_DEV_IDX_2];
	g_pstI2Cclients[I2C_DEV_IDX_2]->ext_flag &= ~I2C_POLLING_FLAG;
#endif

	/* Default EEPROM Slave Address sub = 0xa8 */
	g_pstI2Cclients[I2C_DEV_IDX_2]->addr = 0x54;
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
	g_pstI2Cclients[I2C_DEV_IDX_3] = client;

	/* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
	g_pstI2Cclients[I2C_DEV_IDX_3]->timing = gi2c_dev_timing[I2C_DEV_IDX_3];
	g_pstI2Cclients[I2C_DEV_IDX_3]->ext_flag &= ~I2C_POLLING_FLAG;
#endif

	/* Default EEPROM Slave Address Main2 = 0xa4 */
	g_pstI2Cclients[I2C_DEV_IDX_3]->addr = 0x52;
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
				pr_debug("getinfo_struct failed\n");

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
				pr_debug("getinfo_struct failed\n");

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

			pr_debug("ioctl allocate pBuff mem failed\n");
			return -ENOMEM;
		}

		if (copy_from_user
		    ((u8 *) pBuff, (u8 *) a_u4Param,
				sizeof(struct stCAM_CAL_INFO_STRUCT))) {
			/*get input structure address */
			kfree(pBuff);
			pr_debug("ioctl copy from user failed\n");
			return -EFAULT;
		}

		ptempbuf = (struct stCAM_CAL_INFO_STRUCT *)pBuff;

		if ((ptempbuf->u4Length <= 0) ||
			(ptempbuf->u4Length > CAM_CAL_MAX_BUF_SIZE)) {
			kfree(pBuff);
			pr_debug("Buffer Length Error!\n");
			return -EFAULT;
		}

		pu1Params = kmalloc(ptempbuf->u4Length, GFP_KERNEL);

		if (pu1Params == NULL) {
			kfree(pBuff);
			pr_debug("ioctl allocate pu1Params mem failed\n");
			return -ENOMEM;
		}

		if (copy_from_user
		    ((u8 *) pu1Params, (u8 *) ptempbuf->pu1Params,
		    ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pu1Params);
			pr_debug("ioctl copy from user failed\n");
			return -EFAULT;
		}
	}
	if (ptempbuf == NULL) {	/*It have to add */
		pr_debug("ptempbuf is Null !!!");
		return -EFAULT;
	}
	switch (a_u4Command) {

	case CAM_CALIOC_S_WRITE:	/*Note: Write Command is Unverified! */
		pr_debug("CAM_CALIOC_S_WRITE start!\n");
#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif

		pcmdInf = EEPROM_get_cmd_info_ex(ptempbuf->sensorID,
			ptempbuf->deviceID);

		/* Check the max size if specified */
		if (pcmdInf != NULL &&
		    (pcmdInf->maxEepromSize != 0) &&
		    (pcmdInf->maxEepromSize <
		     (ptempbuf->u4Offset + ptempbuf->u4Length))) {
			pr_debug("Error!! not support address >= 0x%x!!\n",
				 pcmdInf->maxEepromSize);
			kfree(pBuff);
			kfree(pu1Params);
			return -EFAULT;
		}

		if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
			if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
					       pcmdInf) != 0) {
				pr_debug("deviceID Error!\n");
				kfree(pBuff);
				kfree(pu1Params);
				return -EFAULT;
			}
			g_lastDevID = ptempbuf->deviceID;
		}

		if (pcmdInf != NULL) {
			if (pcmdInf->writeCMDFunc != NULL) {
				i4RetValue = pcmdInf->writeCMDFunc(
					pcmdInf->client,
					ptempbuf->u4Offset, pu1Params,
					ptempbuf->u4Length);
			} else
				pr_debug("pcmdInf->writeCMDFunc == NULL\n");
		} else
			pr_debug("pcmdInf == NULL\n");

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		pr_debug("Write data %d bytes take %lu us\n",
			ptempbuf->u4Length, TimeIntervalUS);
#endif
		pr_debug("CAM_CALIOC_S_WRITE End!\n");
		break;

	case CAM_CALIOC_G_READ:
		pr_debug("CAM_CALIOC_G_READ start! offset=%d, length=%d\n",
			ptempbuf->u4Offset, ptempbuf->u4Length);

#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif
    pr_debug("SensorID=%x DeviceID=%x\n",ptempbuf->sensorID, ptempbuf->deviceID);
    pcmdInf = EEPROM_get_cmd_info_ex(
      ptempbuf->sensorID,
      ptempbuf->deviceID);
    
    /* Check the max size if specified */
    if (pcmdInf != NULL &&
        (pcmdInf->maxEepromSize != 0) &&
        (pcmdInf->maxEepromSize <
         (ptempbuf->u4Offset + ptempbuf->u4Length))) {
      pr_debug("Error!! not support address >= 0x%x!!\n",
         pcmdInf->maxEepromSize);
      kfree(pBuff);
      kfree(pu1Params);
      return -EFAULT;
    }
    
    if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
      if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
                 pcmdInf) != 0) {
        pr_debug("deviceID Error!\n");
        kfree(pBuff);
        kfree(pu1Params);
        return -EFAULT;
      }
      g_lastDevID = ptempbuf->deviceID;
    }


    if((ptempbuf->deviceID == 0x01)&&(g_eepromMainData != NULL)){
			u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
			//PK_DBG("read in main\n");
			if((g_eepromMainData->dataBuffer)&&(totalLength <= g_eepromMainData->dataLength)){
				if(ptempbuf->u4Offset == 1){
					memcpy(pu1Params,(u8*)&g_eepromMainData->sensorVendorid,4);
				}else
			      memcpy(pu1Params,g_eepromMainData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
			   i4RetValue = ptempbuf->u4Length;
			}
			else
			  PK_ERR("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainData->dataBuffer,totalLength,g_eepromMainData->dataLength);	
        }
		else if((ptempbuf->deviceID == 0x02)&&(g_eepromSubData != NULL)){
			u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
			if((g_eepromSubData->dataBuffer)&&(totalLength <= g_eepromSubData->dataLength)){
                //PK_DBG("read in sub\n");
				if(ptempbuf->u4Offset == 1){
					memcpy(pu1Params,(u8*)&g_eepromSubData->sensorVendorid,4);
				}else
			        memcpy(pu1Params,g_eepromSubData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
			   i4RetValue = ptempbuf->u4Length;
			}
			else
			  PK_ERR("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromSubData->dataBuffer,totalLength,g_eepromSubData->dataLength);	

		}
		else if((ptempbuf->deviceID == 0x04)&&(g_eepromMain2Data != NULL)){
			u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
			//PK_DBG("read in main2\n");
			if((g_eepromMain2Data->dataBuffer)&&(totalLength <= g_eepromMain2Data->dataLength)){
				if(ptempbuf->u4Offset == 1){
					memcpy(pu1Params,(u8*)&g_eepromMain2Data->sensorVendorid,4);
				}else
			    	memcpy(pu1Params,g_eepromMain2Data->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
               i4RetValue = ptempbuf->u4Length;
			}else
			  PK_ERR("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMain2Data->dataBuffer,totalLength,g_eepromMain2Data->dataLength);	
		}

		else{

  		if (pcmdInf != NULL) {
  			if (pcmdInf->readCMDFunc != NULL)
  				i4RetValue =
  					pcmdInf->readCMDFunc(pcmdInf->client,
  							  ptempbuf->u4Offset,
  							  pu1Params,
  							  ptempbuf->u4Length);
  			else {
  				pr_debug("pcmdInf->readCMDFunc == NULL\n");
  				kfree(pBuff);
  				kfree(pu1Params);
  				return -EFAULT;
  			}
  		}
    }
#ifdef CAM_CALGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		pr_debug("Read data %d bytes take %lu us\n",
			ptempbuf->u4Length, TimeIntervalUS);
#endif
		break;

	default:
		pr_debug("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	if (_IOC_READ & _IOC_DIR(a_u4Command)) {
		if (copy_to_user
		    ((u8 __user *) ptempbuf->pu1Params, (u8 *) pu1Params,
				ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pu1Params);
			pr_debug("ioctl copy to user failed\n");
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

	pr_debug("%s start\n", __func__);
	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		pr_debug("Opened, return -EBUSY\n");
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
#define PATH_CALI   "/persist/camera/CalData.bin"
#include <linux/file.h>
#include <linux/proc_fs.h>
#define TOTAL_EEPROM_DATA_SIZE (0x1BF6)
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData,u16 a_sizeRecvData, u16 i2cId);
static u32 readStartAdress = 0;
//extern void test_imgSensorPowerON(int CameraId);
//extern void test_imgSensorPowerOFF(int CameraId);
static void dual_cam_cal_write_eeprom_i2c(u32 addr, u32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, 0xA8);
}

static bool dual_cam_cal_read_eeprom_i2c(u32 addr, u8 *data)
{
    char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

    if(iReadRegI2C(pu_send_cmd, 2, (u8 *)data, 1, 0xA8) < 0)
        return false;

    return true;
}
static int dual_cam_cal_read_total_data(u32 sensorID,u32 deviceID,u8* dataBuffer){
    struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;
	int i4RetValue = -1;
	//u32 i = 0;
	//bool ret = false ;

	if(dataBuffer == NULL){
		PK_ERR("DataBuffer not inited!\n");
		return -EFAULT;
	}

	PK_INF("SensorID=%x DeviceID=%x\n",sensorID, deviceID);
    
	pcmdInf = EEPROM_get_cmd_info_ex(sensorID,deviceID);
  
  if(EEPROM_set_i2c_bus(deviceID,pcmdInf) != 0) {
		PK_ERR("deviceID Error!\n");
		return -EFAULT;
	}
	/*
	for(i = 0; i < TOTAL_EEPROM_DATA_SIZE;i++){
		ret = dual_cam_cal_read_eeprom_i2c(i,&dataBuffer[i]);	
		if(!ret)
			break;
	}
	if(ret) 
		i4RetValue = TOTAL_EEPROM_DATA_SIZE;
		*/
	
	if (pcmdInf != NULL) {
		if (pcmdInf->readCMDFunc != NULL){
			//get eeprom data
			i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, 0, dataBuffer,TOTAL_EEPROM_DATA_SIZE);
		} else {
			PK_ERR("pcmdInf->readCMDFunc == NULL\n");
		}
	} else {
		PK_ERR("pcmdInf == NULL\n");
    }
    
	return i4RetValue;
}
static ssize_t dual_cam_cal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u8 temp[16]  = {0};
	int startAdress = 0x13B5;
	int i = 0;
	startAdress = readStartAdress;
	//test_imgSensorPowerON(0);
	//mdelay(5);
	for(i = 0; i < 16;i++){
		dual_cam_cal_read_eeprom_i2c(startAdress+i,&temp[i]);	
	}
	readStartAdress += 16; 
	//test_imgSensorPowerOFF(0);
   	return sprintf(buf, "0x%x[0~15] 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n", \
		            startAdress,temp[0],temp[1],temp[2],temp[3],temp[4],temp[5],temp[6],temp[7],temp[8],temp[9],temp[10],\
		            temp[11],temp[12],temp[13],temp[14],temp[15]);

}
static ssize_t dual_cam_cal_store(struct device *dev,  struct device_attribute *attr,
	         const char *buf, size_t count)
{
    struct file *fp;
    u32 dataSize = 0;
    loff_t pos = 0;
	u8 *caliBuffer = NULL;
	u32 startAdress = 0x0000;
	int sensorID = 0;
	int i = 0;
	ssize_t ret = -ENOENT;
	mm_segment_t fs;
	u32 checkSum = 0;
	u8 *totalBuf = NULL ;
	u8 readValue = 0;
	
	ret = kstrtoint(buf, 0, &sensorID);
	if(sensorID == CEREUS_IMX486_SUNNY_SENSOR_ID){
		startAdress = 0x13B5;
	}
	else if(sensorID == CEREUS_OV12A10_OFILM_SENSOR_ID){
		startAdress = 0x13E4;
	}else{
	    PK_ERR("error sensorID 0x%x\n",sensorID);
		readStartAdress = ret;
		ret = -ENOENT;
	    return ret;
	}
	PK_DBG("sensorID 0x%x\n",sensorID);	
	readStartAdress = startAdress;
    //int i = 0;
    fs = get_fs();
    fp = filp_open(PATH_CALI, O_RDONLY, 0666);
    if (IS_ERR(fp)) {
        PK_ERR("faile to open %s error\n",PATH_CALI);
        return -ENOENT;
    }
	dataSize = vfs_llseek(fp, 0, SEEK_END);
    PK_DBG("Binary data size is %d bytes \n", dataSize);

    if(dataSize > 0){
        if(caliBuffer == NULL) {
            caliBuffer = kzalloc(dataSize, GFP_KERNEL);
            if (caliBuffer == NULL) {
                PK_ERR("[Error]malloc memery failed \n");
				ret =  -ENOMEM;
                goto dual_cam_cal_store_failed;
            }
        }
        set_fs(KERNEL_DS);
        pos = 0;
        vfs_read(fp, caliBuffer, dataSize, &pos);
        PK_DBG("Read new calibration data done!\n");
        filp_close(fp, NULL);
        set_fs(fs);
    } else{
        PK_ERR("[Error] Get calibration data failed\n");
		goto dual_cam_cal_store_failed;
    }
	
	//test_imgSensorPowerON(0);w
	dual_cam_cal_read_eeprom_i2c(startAdress-1,&readValue);
	checkSum += readValue; 
	msleep(2); 
	if(sensorID == CEREUS_IMX486_SUNNY_SENSOR_ID){
		dual_cam_cal_write_eeprom_i2c(0x8000,0x06);
		msleep(5); 
	}
    for(i = 0; i < dataSize ;i++){
		dual_cam_cal_write_eeprom_i2c(startAdress+i,caliBuffer[i]);
		msleep(2); //Important, if not, write will fail.
		PK_DBG("caliBuffer[0x%x] = 0x%x \n",startAdress+i,caliBuffer[i]);
		checkSum += caliBuffer[i];
    }
	// dual camera data checksum start
	dual_cam_cal_write_eeprom_i2c(startAdress+i,(checkSum%0xff +1));
	msleep(2); //Important, if not, write will fail.
	// dual camera data checksum end

	//total  data checksum start
	totalBuf = kzalloc(TOTAL_EEPROM_DATA_SIZE, GFP_KERNEL);
    if(totalBuf == NULL) {
      	PK_ERR("malloc memery failed \n");
    }else{
        int retValue = 0;
		checkSum = 0;
        retValue = dual_cam_cal_read_total_data(sensorID,SENSOR_DEV_MAIN,totalBuf);
		if(retValue == TOTAL_EEPROM_DATA_SIZE){
			for(i = 0; i < (TOTAL_EEPROM_DATA_SIZE -1);i++)
				checkSum += totalBuf[i];
			dual_cam_cal_write_eeprom_i2c((TOTAL_EEPROM_DATA_SIZE -1),(checkSum%0xff +1));
			msleep(2); 
		}else{
		    PK_ERR("read total num data failed\n");
		}
		kfree(totalBuf);
    }
	//total  data checksum end
	if(sensorID == CEREUS_IMX486_SUNNY_SENSOR_ID){
		dual_cam_cal_write_eeprom_i2c(0x8000,0x0E);
		msleep(5);
    }
	//test_imgSensorPowerOFF(0);
	ret = count;
dual_cam_cal_store_failed:
	if(caliBuffer != NULL)
		kfree(caliBuffer);
    filp_close(fp, NULL);
    set_fs(fs);
    PK_DBG("write dualcam cali data exit\n");	
    return ret;
}
DEVICE_ATTR(dual_cam_cal, 0664, dual_cam_cal_show, dual_cam_cal_store); 

/***********************************************
 *
 ***********************************************/

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1
static inline int EEPROM_chrdev_register(void)
{
	struct device *device = NULL;

	pr_debug("EEPROM_chrdev_register Start\n");

#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_devNum, 0, 1, CAM_CAL_DRV_NAME)) {
		pr_debug("Allocate device no failed\n");
		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_devNum, 1, CAM_CAL_DRV_NAME)) {
		pr_debug("Register device no failed\n");
		return -EAGAIN;
	}
#endif

	g_charDrv = cdev_alloc();

	if (g_charDrv == NULL) {
		unregister_chrdev_region(g_devNum, 1);
		pr_debug("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &g_stCAM_CAL_fops1);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		pr_debug("Attatch file operation failed\n");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, "CAM_CALdrv1");
	if (IS_ERR(g_drvClass)) {
		int ret = PTR_ERR(g_drvClass);

		pr_debug("Unable to create class, err = %d\n", ret);
		return ret;
	}
	device = device_create(g_drvClass, NULL, g_devNum, NULL,
		CAM_CAL_DRV_NAME);
	pr_debug("EEPROM_chrdev_register End\n");

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
	pr_debug("%s Start!\n", __func__);

	if (platform_driver_register(&g_stEEPROM_HW_Driver)) {
		pr_debug("failed to register EEPROM driver i2C main\n");
		return -ENODEV;
	}

	if (platform_device_register(&g_platDev)) {
		pr_debug("failed to register EEPROM device");
		return -ENODEV;
	}

	EEPROM_chrdev_register();

	pr_debug("%s End!\n", __func__);
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
