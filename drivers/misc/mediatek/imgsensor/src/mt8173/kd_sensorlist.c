/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>	/* proc file use */
#include <linux/dma-mapping.h>
/*#include <linux/xlog.h>*/
#include <sync_write.h>
/*#include <mt_typedefs.h>*/
#ifndef MUINT8
typedef unsigned char MUINT8;
#endif
#ifndef MUINT32
typedef unsigned int MUINT32;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int UINT32;
#endif
#ifndef MUINT8
typedef signed int MINT32;
#endif

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE  (1)
#endif
#ifndef BOOL
typedef unsigned char BOOL;
#endif

#ifndef kal_uint32
typedef unsigned int kal_uint32;
#endif

/* #include <kd_camera_hw.h> */
/* #include <asm/system.h> */

#define MTKCAM_USING_CCF
#ifdef MTKCAM_USING_CCF
#include <linux/clk.h>
#else
#error " MTKCAM_USING_CCF is not defined"
#include <mach/mt_clkmgr.h>	/* For clock mgr APIS. enable_clock()/disable_clock(). */
#endif



#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"
#include "kd_imgsensor_errcode.h"

#include "kd_sensorlist.h"

#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define MTKCAM_USING_PWRREG
#ifdef MTKCAM_USING_PWRREG	/* Power Regulator Framework */
#include <linux/regulator/consumer.h>
#endif

#define MTKCAM_USING_DTGPIO
#ifdef MTKCAM_USING_DTGPIO	/* Device Tree GPIO */
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#error "ERROR: MTKCAM_USING_DTGPIO is not defined"
#endif

#endif
/* #define CONFIG_COMPAT */
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#include <mt_cam.h>
#define PDAF_DATA_SIZE 4096

extern int hardware_id;

static DEFINE_SPINLOCK(kdsensor_drv_lock);

/* I2C bus # should be defined in board-proj.c to be proj dependent */
/*
#ifndef SUPPORT_I2C_BUS_NUM1
#define SUPPORT_I2C_BUS_NUM1        0
#endif
#ifndef SUPPORT_I2C_BUS_NUM2
#define SUPPORT_I2C_BUS_NUM2        2
#endif
*/

#define CAMERA_HW_DRVNAME1  "kd_camera_hw"
#define CAMERA_HW_DRVNAME2  "kd_camera_hw_bus2"
#define PDAF_DATA_SIZE 4096
/* Camera Power Regulator Framework */
#ifdef MTKCAM_USING_PWRREG

struct regulator *cam1_pwr_vcama = NULL;
struct regulator *cam1_pwr_vcamd = NULL;
struct regulator *cam1_pwr_vcamio = NULL;
struct regulator *cam1_pwr_vcamaf = NULL;

struct regulator *cam2_pwr_vcama = NULL;
struct regulator *cam2_pwr_vcamd = NULL;
struct regulator *cam2_pwr_vcamio = NULL;
struct regulator *cam2_pwr_vcamaf = NULL;

#endif

/* Common Clock Framework (CCF) */
#ifdef MTKCAM_USING_CCF
struct clk *g_camclk_camtg_sel;
struct clk *g_camclk_univpll_d26;
struct clk *g_camclk_univpll2_d2;
#endif

static MSDK_SENSOR_REG_INFO_STRUCT g_MainSensorReg;
static MSDK_SENSOR_REG_INFO_STRUCT g_SubSensorReg;

static struct i2c_board_info i2c_devs1 __initdata = {
	I2C_BOARD_INFO(CAMERA_HW_DRVNAME1, 0xfe >> 1) };
static struct i2c_board_info i2c_devs2 __initdata = {
	I2C_BOARD_INFO(CAMERA_HW_DRVNAME2, 0xfe >> 1) };

#define SENSOR_WR32(addr, data)    mt65xx_reg_sync_writel(data, addr)	/* For 89 Only.   // NEED_TUNING_BY_PROJECT */
/* #define SENSOR_WR32(addr, data)    iowrite32(data, addr)    // For 89 Only.   // NEED_TUNING_BY_PROJECT */
#define SENSOR_RD32(addr)          ioread32(addr)
/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_sensorlist]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(PFX "[%s]" fmt, __func__, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_WARN(fmt, arg...) pr_warn(PFX "[%s] Warning:" fmt, __func__, ##arg)
#define PK_ERR(fmt, arg...)	pr_err(PFX "[%s]" fmt, __func__, ##arg)
#define PK_XLOG_INFO(fmt, arg...)	pr_debug(PFX "[%s]" fmt, __func__, ##arg)

#else
#define PK_DBG(a, ...)
#define PK_ERR(a, ...)


#endif

#define CAMERA_MODULE_INFO

#ifdef CAMERA_MODULE_INFO
#define CAMERA_NAME_SIZE 100
static char back_camera_module_name[CAMERA_NAME_SIZE];
static char front_camera_module_name[CAMERA_NAME_SIZE];
static int camera_name_sysfs_inited;
static struct kobject *camera_name_kobj;

static ssize_t back_camera_module_name_show(struct kobject *d,
	struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, CAMERA_NAME_SIZE, "%s\n", back_camera_module_name);

}
static ssize_t front_camera_module_name_show(struct kobject *d,
			  struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, CAMERA_NAME_SIZE, "%s\n", front_camera_module_name);

}
static ssize_t back_camera_module_name_store(struct kobject *d, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	memcpy(back_camera_module_name, buf, count);
	back_camera_module_name[count] = '\0';
	return count;
}
static ssize_t front_camera_module_name_store(struct kobject *d, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	memcpy(front_camera_module_name, buf, count);
	front_camera_module_name[count] = '\0';
	return count;
}

static struct kobj_attribute backcamera_module_name_attr = {
	.attr = {
	.name = "backcamera_module_name",
	.mode = S_IRWXG|S_IRWXU|S_IROTH,
	},
	.show = back_camera_module_name_show,
	.store = back_camera_module_name_store,
};
static struct kobj_attribute frontcamera_module_name_attr = {
	.attr = {
	.name = "frontcamera_module_name",
	.mode = S_IRWXG|S_IRWXU|S_IROTH,
	},
	.show = front_camera_module_name_show,
	.store = front_camera_module_name_store,
};
static struct attribute *camera_name_attrs[] = {
	&backcamera_module_name_attr.attr,
	&frontcamera_module_name_attr.attr,
	NULL,
};
static struct attribute_group camera_name_attrs_group = {
	.attrs = camera_name_attrs,
};
#endif


/*******************************************************************************
* Proifling
********************************************************************************/
#define PROFILE 1
#if PROFILE
static struct timeval tv1, tv2;
/*******************************************************************************
*
********************************************************************************/
inline void KD_IMGSENSOR_PROFILE_INIT(void)
{
	do_gettimeofday(&tv1);
}

/*******************************************************************************
*
********************************************************************************/
inline void KD_IMGSENSOR_PROFILE(char *tag)
{
	unsigned long TimeIntervalUS;

	spin_lock(&kdsensor_drv_lock);

	do_gettimeofday(&tv2);
	TimeIntervalUS = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
	tv1 = tv2;

	spin_unlock(&kdsensor_drv_lock);
	PK_DBG("[%s]Profile = %lu\n", tag, TimeIntervalUS);
}
#else
inline static void KD_IMGSENSOR_PROFILE_INIT(void)
{
}

inline static void KD_IMGSENSOR_PROFILE(char *tag)
{
}
#endif

/*******************************************************************************
*
********************************************************************************/
extern int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName,
			      BOOL On, char *mode_name);
/* extern ssize_t strobe_VDIrq(void);  //cotta : add for high current solution */

/*******************************************************************************
*
********************************************************************************/

static struct i2c_client *g_pstI2Cclient;
static struct i2c_client *g_pstI2Cclient2;

/* 81 is used for V4L driver */
static dev_t g_CAMERA_HWdevno = MKDEV(250, 0);
static dev_t g_CAMERA_HWdevno2;
static struct cdev *g_pCAMERA_HW_CharDrv;
static struct cdev *g_pCAMERA_HW_CharDrv2;
static struct class *sensor_class;
static struct class *sensor2_class;

static atomic_t g_CamHWOpend;
static atomic_t g_CamHWOpend2;
static atomic_t g_CamHWOpening;
static atomic_t g_CamDrvOpenCnt;
static atomic_t g_CamDrvOpenCnt2;

/* static u32 gCurrI2CBusEnableFlag = 0; */
static u32 gI2CBusNum;

#define SET_I2CBUS_FLAG(_x_)        ((1<<_x_)|(gCurrI2CBusEnableFlag))
#define CLEAN_I2CBUS_FLAG(_x_)      ((~(1<<_x_))&(gCurrI2CBusEnableFlag))

static DEFINE_MUTEX(kdCam_Mutex);
static BOOL bSesnorVsyncFlag = FALSE;
static ACDK_KD_SENSOR_SYNC_STRUCT g_NewSensorExpGain = {
	128, 128, 128, 128, 1000, 640, 0xFF, 0xFF, 0xFF, 0 };


extern MULTI_SENSOR_FUNCTION_STRUCT2 kd_MultiSensorFunc;
static MULTI_SENSOR_FUNCTION_STRUCT2 *g_pSensorFunc = &kd_MultiSensorFunc;
/* remove static declarations for using these functions in camera_isp.c */
/*
static SENSOR_FUNCTION_STRUCT *g_pInvokeSensorFunc[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = { NULL, NULL };
static BOOL g_bEnableDriver[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = { FALSE, FALSE };
static CAMERA_DUAL_CAMERA_SENSOR_ENUM g_invokeSocketIdx[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = { DUAL_CAMERA_NONE_SENSOR, DUAL_CAMERA_NONE_SENSOR };
static char g_invokeSensorNameStr[KDIMGSENSOR_MAX_INVOKE_DRIVERS][32] = { KDIMGSENSOR_NOSENSOR, KDIMGSENSOR_NOSENSOR };
*/
BOOL g_bEnableDriver[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = { FALSE, FALSE };
SENSOR_FUNCTION_STRUCT *g_pInvokeSensorFunc[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = { NULL, NULL };
CAMERA_DUAL_CAMERA_SENSOR_ENUM g_invokeSocketIdx[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = {
	DUAL_CAMERA_NONE_SENSOR, DUAL_CAMERA_NONE_SENSOR };
char g_invokeSensorNameStr[KDIMGSENSOR_MAX_INVOKE_DRIVERS][32] = {
	KDIMGSENSOR_NOSENSOR, KDIMGSENSOR_NOSENSOR };
/* static int g_SensorExistStatus[3]={0,0,0}; */
static wait_queue_head_t kd_sensor_wait_queue;
bool setExpGainDoneFlag = 0;
static unsigned int g_CurrentSensorIdx;
static unsigned int g_IsSearchSensor;
/*=============================================================================

=============================================================================*/
/*******************************************************************************
* i2c relative start
* migrate new style i2c driver interfaces required by Kirby 20100827
********************************************************************************/
static const struct i2c_device_id CAMERA_HW_i2c_id[] = { {CAMERA_HW_DRVNAME1, 0}, {} };
static const struct i2c_device_id CAMERA_HW_i2c_id2[] = { {CAMERA_HW_DRVNAME2, 0}, {} };



/*******************************************************************************
* general camera image sensor kernel driver
*******************************************************************************/
UINT32 kdGetSensorInitFuncList(ACDK_KD_SENSOR_INIT_FUNCTION_STRUCT **ppSensorList)
{
	if (NULL == ppSensorList) {
		PK_DBG("[kdGetSensorInitFuncList]ERROR: NULL ppSensorList\n");
		return 1;
	}
	*ppSensorList = &kdSensorList[0];
	return 0;
}				/* kdGetSensorInitFuncList() */


/*******************************************************************************
*iMultiReadReg
********************************************************************************/
int iMultiReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId, u16 number)
{
	int i4RetValue = 0;
	char puReadCmd[2] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF) };

	if (gI2CBusNum == camera_i2c_bus_num1) {
		spin_lock(&kdsensor_drv_lock);

		g_pstI2Cclient->addr = (i2cId >> 1);

		spin_unlock(&kdsensor_drv_lock);

		/*  */
		i4RetValue = i2c_master_send(g_pstI2Cclient, puReadCmd, 2);
		PK_ERR("[CAMERA SENSOR] i2c_master_send return %d", i4RetValue);
		if (i4RetValue != 2) {
			PK_ERR("[CAMERA SENSOR] I2C send failed, addr = 0x%x, data = 0x%x !!\n",
				a_u2Addr, *a_puBuff);
			return -1;
		}
		/*  */
		i4RetValue = i2c_master_recv(g_pstI2Cclient, (char *)a_puBuff, number);
		PK_ERR("[CAMERA SENSOR] i2c_master_recv return %d, try to read %d bytes", i4RetValue, number);
		if (i4RetValue != number) {
			PK_ERR("[CAMERA SENSOR] I2C read failed!! expect %d, received %d\n", number, i4RetValue);
			return -1;
		}
	} else {
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient2->addr = (i2cId >> 1);
		spin_unlock(&kdsensor_drv_lock);
		/*  */
		i4RetValue = i2c_master_send(g_pstI2Cclient2, puReadCmd, 2);
		PK_ERR("[CAMERA SENSOR] i2c_master_send return %d", i4RetValue);
		if (i4RetValue != 2) {
			PK_DBG("[CAMERA SENSOR] I2C send failed, addr = 0x%x, data = 0x%x !!\n",
				a_u2Addr, *a_puBuff);
			return -1;
		}
		/*  */
		i4RetValue = i2c_master_recv(g_pstI2Cclient2, (char *)a_puBuff, number);
		PK_ERR("[CAMERA SENSOR] i2c_master_recv return %d, try to read %d bytes", i4RetValue, number);
		if (i4RetValue != number) {
			PK_ERR("[CAMERA SENSOR] I2C read failed!! expect %d, received %d\n", number, i4RetValue);
			return -1;
		}
	}
	return 0;
}


/*******************************************************************************
* iReadReg
********************************************************************************/
int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId)
{
	int i4RetValue = 0;
	char puReadCmd[2] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF) };

	if (gI2CBusNum == camera_i2c_bus_num1) {
		spin_lock(&kdsensor_drv_lock);

		g_pstI2Cclient->addr = (i2cId >> 1);

		spin_unlock(&kdsensor_drv_lock);

		/*  */
		i4RetValue = i2c_master_send(g_pstI2Cclient, puReadCmd, 2);
		if (i4RetValue != 2) {
			PK_DBG("[CAMERA SENSOR] I2C send failed, addr = 0x%x, data = 0x%x !!\n",
			       a_u2Addr, *a_puBuff);
			return -1;
		}
		/*  */
		i4RetValue = i2c_master_recv(g_pstI2Cclient, (char *)a_puBuff, 1);
		if (i4RetValue != 1) {
			PK_DBG("[CAMERA SENSOR] I2C read failed!!\n");
			return -1;
		}
	} else {
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient2->addr = (i2cId >> 1);

		spin_unlock(&kdsensor_drv_lock);
		/*  */
		i4RetValue = i2c_master_send(g_pstI2Cclient2, puReadCmd, 2);
		if (i4RetValue != 2) {
			PK_DBG("[CAMERA SENSOR] I2C send failed, addr = 0x%x, data = 0x%x !!\n",
			       a_u2Addr, *a_puBuff);
			return -1;
		}
		/*  */
		i4RetValue = i2c_master_recv(g_pstI2Cclient2, (char *)a_puBuff, 1);
		if (i4RetValue != 1) {
			PK_DBG("[CAMERA SENSOR] I2C read failed!!\n");
			return -1;
		}
	}
	return 0;
}

/*******************************************************************************
* iReadRegI2C
********************************************************************************/
int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData,
		u16 i2cId)
{
	int i4RetValue = 0;

	if (gI2CBusNum == camera_i2c_bus_num1) {
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient->addr = (i2cId >> 1);

		spin_unlock(&kdsensor_drv_lock);
		/*  */
		i4RetValue = i2c_master_send(g_pstI2Cclient, a_pSendData, a_sizeSendData);
		if (i4RetValue != a_sizeSendData) {
			PK_DBG("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
			return -1;
		}

		i4RetValue = i2c_master_recv(g_pstI2Cclient, (char *)a_pRecvData, a_sizeRecvData);
		if (i4RetValue != a_sizeRecvData) {
			PK_DBG("[CAMERA SENSOR] I2C read failed!!\n");
			return -1;
		}
	} else {
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient2->addr = (i2cId >> 1);

		spin_unlock(&kdsensor_drv_lock);
		i4RetValue = i2c_master_send(g_pstI2Cclient2, a_pSendData, a_sizeSendData);
		if (i4RetValue != a_sizeSendData) {
			PK_DBG("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
			return -1;
		}

		i4RetValue = i2c_master_recv(g_pstI2Cclient2, (char *)a_pRecvData, a_sizeRecvData);
		if (i4RetValue != a_sizeRecvData) {
			PK_DBG("[CAMERA SENSOR] I2C read failed!!\n");
			return -1;
		}
	}
	return 0;
}


/*******************************************************************************
* iWriteReg
********************************************************************************/
int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId)
{
	int i4RetValue = 0;
	int u4Index = 0;
	u8 *puDataInBytes = (u8 *) &a_u4Data;
	int retry = 3;

	char puSendCmd[6] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF),
		0, 0, 0, 0
	};

/* PK_DBG("Addr : 0x%x,Val : 0x%x\n",a_u2Addr,a_u4Data); */

	/* KD_IMGSENSOR_PROFILE_INIT(); */
	spin_lock(&kdsensor_drv_lock);

	if (gI2CBusNum == camera_i2c_bus_num1) {
		g_pstI2Cclient->addr = (i2cId >> 1);
	} else {
		g_pstI2Cclient2->addr = (i2cId >> 1);
	}
	spin_unlock(&kdsensor_drv_lock);


	if (a_u4Bytes > 2) {
		PK_DBG("[CAMERA SENSOR] exceed 2 bytes\n");
		return -1;
	}

	if (a_u4Data >> (a_u4Bytes << 3)) {
		PK_DBG("[CAMERA SENSOR] warning!! some data is not sent!!\n");
	}

	for (u4Index = 0; u4Index < a_u4Bytes; u4Index += 1) {
		puSendCmd[(u4Index + 2)] = puDataInBytes[(a_u4Bytes - u4Index - 1)];
	}
	/*  */
	do {
		if (gI2CBusNum == camera_i2c_bus_num1) {
			i4RetValue = i2c_master_send(g_pstI2Cclient, puSendCmd, (a_u4Bytes + 2));
		} else {
			i4RetValue = i2c_master_send(g_pstI2Cclient2, puSendCmd, (a_u4Bytes + 2));
		}
		if (i4RetValue != (a_u4Bytes + 2)) {
			PK_DBG("[CAMERA SENSOR] I2C send failed addr = 0x%x, data = 0x%x !!\n",
			       a_u2Addr, a_u4Data);
		} else {
			break;
		}
		uDELAY(50);
	} while ((retry--) > 0);
	/* KD_IMGSENSOR_PROFILE("iWriteReg"); */
	return 0;
}

int kdSetI2CBusNum(u32 i2cBusNum)
{

	if ((i2cBusNum != camera_i2c_bus_num2) && (i2cBusNum != camera_i2c_bus_num1)) {
		PK_ERR("[kdSetI2CBusNum] i2c bus number is not correct(%d)\n", i2cBusNum);
		return -1;
	}
	spin_lock(&kdsensor_drv_lock);
	gI2CBusNum = i2cBusNum;
	spin_unlock(&kdsensor_drv_lock);

	return 0;
}

void kdSetI2CSpeed(u32 i2cSpeed)
{
	/*
	   if(gI2CBusNum == camera_i2c_bus_num1) {
	   spin_lock(&kdsensor_drv_lock);
	   g_pstI2Cclient->timing = i2cSpeed;
	   spin_unlock(&kdsensor_drv_lock);
	   }
	   else{
	   spin_lock(&kdsensor_drv_lock);
	   g_pstI2Cclient2->timing = i2cSpeed;
	   spin_unlock(&kdsensor_drv_lock);
	   }
	 */
}

/*******************************************************************************
* kdReleaseI2CTriggerLock
********************************************************************************/
int kdReleaseI2CTriggerLock(void)
{
	int ret = 0;

	/* ret = mt_wait4_i2c_complete(); */

	/* if (ret < 0 ) { */
	/* PK_DBG_FUNC("[error]wait i2c fail\n"); */
	/* } */

	return ret;
}

/*******************************************************************************
* iBurstWriteReg
********************************************************************************/
#define MAX_CMD_LEN          255
int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId)
{

	dma_addr_t phyAddr;
	u8 *buf = NULL;
	u32 old_addr = 0;
	int ret = 0;
	int retry = 0;

	if (gI2CBusNum == camera_i2c_bus_num1) {
		if (bytes > MAX_CMD_LEN) {
			PK_DBG("[iBurstWriteReg] exceed the max write length\n");
			return 1;
		}

		phyAddr = 0;

		buf = kzalloc(bytes, GFP_KERNEL);

		if (NULL == buf) {
			PK_DBG("[iBurstWriteReg] Not enough memory\n");
			return -1;
		}

		memcpy(buf, pData, bytes);
		/* PK_DBG("[iBurstWriteReg] bytes = %d, phy addr = 0x%x\n", bytes, phyAddr ); */

		old_addr = g_pstI2Cclient->addr;
		spin_lock(&kdsensor_drv_lock);
#if 0
		g_pstI2Cclient->addr =
		    (((g_pstI2Cclient->addr >> 1) & I2C_MASK_FLAG) | I2C_DMA_FLAG);
#else
		/*g_pstI2Cclient->addr = ( ((i2cId >> 1) &  I2C_MASK_FLAG) | I2C_DMA_FLAG ); */
		g_pstI2Cclient->addr = (i2cId >> 1);
#endif
		spin_unlock(&kdsensor_drv_lock);

		ret = 0;
		retry = 3;
		do {
			ret = i2c_master_send(g_pstI2Cclient, (u8 *) phyAddr, bytes);
			retry--;
			if (ret != bytes) {
				PK_ERR("Error sent I2C ret = %d\n", ret);
			}
		} while ((ret != bytes) && (retry > 0));

		kfree(buf);
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient->addr = old_addr;
		spin_unlock(&kdsensor_drv_lock);
	} else {
		if (bytes > MAX_CMD_LEN) {
			PK_DBG("[iBurstWriteReg] exceed the max write length\n");
			return 1;
		}
		phyAddr = 0;
		buf = kzalloc(bytes, GFP_KERNEL);

		if (NULL == buf) {
			PK_DBG("[iBurstWriteReg] Not enough memory\n");
			return -1;
		}

		memcpy(buf, pData, bytes);
		/* PK_DBG("[iBurstWriteReg] bytes = %d, phy addr = 0x%x\n", bytes, phyAddr ); */

		old_addr = g_pstI2Cclient2->addr;
		spin_lock(&kdsensor_drv_lock);
		/*g_pstI2Cclient2->addr = ( ((g_pstI2Cclient2->addr >> 1) &  I2C_MASK_FLAG) | I2C_DMA_FLAG ); */
		g_pstI2Cclient2->addr = (g_pstI2Cclient2->addr >> 1);
		spin_unlock(&kdsensor_drv_lock);
		ret = 0;
		retry = 3;
		do {
			ret = i2c_master_send(g_pstI2Cclient2, (u8 *) phyAddr, bytes);
			retry--;
			if (ret != bytes) {
				PK_ERR("Error sent I2C ret = %d\n", ret);
			}
		} while ((ret != bytes) && (retry > 0));

		kfree(buf);
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient2->addr = old_addr;
		spin_unlock(&kdsensor_drv_lock);
	}
	return 0;
}


/*******************************************************************************
* iMultiWriteReg
********************************************************************************/

int iMultiWriteReg(u8 *pData, u16 lens, u16 i2cId)
{
	int ret = 0;

	if (gI2CBusNum == camera_i2c_bus_num1) {
		g_pstI2Cclient->addr = (i2cId >> 1);
		ret = i2c_master_send(g_pstI2Cclient, pData, lens);
	} else {
		g_pstI2Cclient2->addr = (i2cId >> 1);
		ret = i2c_master_send(g_pstI2Cclient2, pData, lens);
	}

	if (ret != lens) {
		PK_DBG("Error sent I2C ret = %d\n", ret);
	}
	return 0;
}


/*******************************************************************************
* iWriteRegI2C
********************************************************************************/
int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId)
{
	int i4RetValue = 0;
	int retry = 3;

/* PK_DBG("Addr : 0x%x,Val : 0x%x\n",a_u2Addr,a_u4Data); */

	/* KD_IMGSENSOR_PROFILE_INIT(); */
	spin_lock(&kdsensor_drv_lock);
	if (gI2CBusNum == camera_i2c_bus_num1) {
		g_pstI2Cclient->addr = (i2cId >> 1);
	} else {
		g_pstI2Cclient2->addr = (i2cId >> 1);
	}
	spin_unlock(&kdsensor_drv_lock);
	/*  */

	do {
		if (gI2CBusNum == camera_i2c_bus_num1) {
			i4RetValue = i2c_master_send(g_pstI2Cclient, a_pSendData, a_sizeSendData);
		} else {
			i4RetValue = i2c_master_send(g_pstI2Cclient2, a_pSendData, a_sizeSendData);
		}
		if (i4RetValue != a_sizeSendData) {
			PK_DBG("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x, Data = 0x%x\n",
			       a_pSendData[0], a_pSendData[1]);
		} else {
			break;
		}
		uDELAY(50);
	} while ((retry--) > 0);
	/* KD_IMGSENSOR_PROFILE("iWriteRegI2C"); */
	return 0;
}

/*******************************************************************************
* sensor function adapter
********************************************************************************/
#define KD_MULTI_FUNCTION_ENTRY()	/* PK_XLOG_INFO("[%s]:E\n",__FUNCTION__) */
#define KD_MULTI_FUNCTION_EXIT()	/* PK_XLOG_INFO("[%s]:X\n",__FUNCTION__) */
/*  */
MUINT32 kdSetI2CSlaveID(MINT32 i, MUINT32 socketIdx, MUINT32 firstSet)
{
	unsigned long long FeaturePara[4];
	MUINT32 FeatureParaLen = 0;

	FeaturePara[0] = socketIdx;
	FeaturePara[1] = firstSet;
	FeatureParaLen = sizeof(unsigned long long) * 2;
	return g_pInvokeSensorFunc[i]->SensorFeatureControl(SENSOR_FEATURE_SET_SLAVE_I2C_ID,
							    (MUINT8 *) FeaturePara,
							    (MUINT32 *) &FeatureParaLen);
}

/*  */
MUINT32 kd_MultiSensorOpen(void)
{
	MUINT32 ret = ERROR_NONE;
	MINT32 i = 0;

	KD_MULTI_FUNCTION_ENTRY();
	/* from hear to tail */
	/* for ( i = KDIMGSENSOR_INVOKE_DRIVER_0 ; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS ; i++ ) { */
	/* from tail to head. */
	for (i = (KDIMGSENSOR_MAX_INVOKE_DRIVERS - 1); i >= KDIMGSENSOR_INVOKE_DRIVER_0; i--) {
		if (g_bEnableDriver[i] && g_pInvokeSensorFunc[i]) {
			if (0 != (g_CurrentSensorIdx & g_invokeSocketIdx[i])) {
				/* turn on power */
				ret = kdCISModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM)
							 g_invokeSocketIdx[i],
							 (char *)g_invokeSensorNameStr[i], true,
							 CAMERA_HW_DRVNAME1);
				if (ERROR_NONE != ret) {
					PK_ERR("[%s]", __func__);
					return ret;
				}
				/* wait for power stable */
				mDELAY(10);
				KD_IMGSENSOR_PROFILE("kdModulePowerOn");

#if 0
				if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = SENSOR_I2C_BUS_NUM[g_invokeSocketIdx[i]];
					spin_unlock(&kdsensor_drv_lock);
					PK_XLOG_INFO("kd_MultiSensorOpen: switch I2C BUS%d\n",
						     gI2CBusNum);
				}
#else
				if (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num2;
					spin_unlock(&kdsensor_drv_lock);
					PK_DBG("kd_MultiSensorOpen: switch I2C BUS2\n");
				} else {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num1;
					spin_unlock(&kdsensor_drv_lock);
					PK_DBG("kd_MultiSensorOpen: switch I2C BUS1\n");
				}
#endif
				/*  */
				/* set i2c slave ID */
				/* KD_SET_I2C_SLAVE_ID(i,g_invokeSocketIdx[i],IMGSENSOR_SET_I2C_ID_STATE); */
				/*  */
				ret = g_pInvokeSensorFunc[i]->SensorOpen();
				if (ERROR_NONE != ret) {
					kdCISModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM)
							   g_invokeSocketIdx[i],
							   (char *)g_invokeSensorNameStr[i], false,
							   CAMERA_HW_DRVNAME1);
					PK_ERR("SensorOpen");
					return ret;
				}
				/* set i2c slave ID */
				/* SensorOpen() will reset i2c slave ID */
				/* KD_SET_I2C_SLAVE_ID(i,g_invokeSocketIdx[i],IMGSENSOR_SET_I2C_ID_FORCE); */
			}
		}
	}
	KD_MULTI_FUNCTION_EXIT();
	return ERROR_NONE;
}

/*  */

MUINT32
kd_MultiSensorGetInfo(MUINT32 *pScenarioId[2],
		      MSDK_SENSOR_INFO_STRUCT * pSensorInfo[2],
		      MSDK_SENSOR_CONFIG_STRUCT * pSensorConfigData[2])
{
	MUINT32 ret = ERROR_NONE;
	u32 i = 0;
	MSDK_SENSOR_INFO_STRUCT SensorInfo[2];
	MSDK_SENSOR_CONFIG_STRUCT SensorConfigData[2];

	memset(&SensorInfo[0], 0, 2 * sizeof(MSDK_SENSOR_INFO_STRUCT));
	memset(&SensorConfigData[0], 0, 2 * sizeof(MSDK_SENSOR_CONFIG_STRUCT));


	KD_MULTI_FUNCTION_ENTRY();
	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		if (g_bEnableDriver[i] && g_pInvokeSensorFunc[i]) {
			if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]) {
				ret =
				    g_pInvokeSensorFunc[i]->SensorGetInfo((MSDK_SCENARIO_ID_ENUM)
									  (*pScenarioId[0]),
									  &SensorInfo[0],
									  &SensorConfigData[0]);
			} else if ((DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i])
				   || (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i])) {
				ret =
				    g_pInvokeSensorFunc[i]->SensorGetInfo((MSDK_SCENARIO_ID_ENUM)
									  (*pScenarioId[1]),
									  &SensorInfo[1],
									  &SensorConfigData[1]);
			}

			if (ERROR_NONE != ret) {
				PK_ERR("[%s]\n", __func__);
				return ret;
			}

		}
	}
	memcpy(pSensorInfo[0], &SensorInfo[0], sizeof(MSDK_SENSOR_INFO_STRUCT));
	memcpy(pSensorInfo[1], &SensorInfo[1], sizeof(MSDK_SENSOR_INFO_STRUCT));
	memcpy(pSensorConfigData[0], &SensorConfigData[0], sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	memcpy(pSensorConfigData[1], &SensorConfigData[1], sizeof(MSDK_SENSOR_CONFIG_STRUCT));



	KD_MULTI_FUNCTION_EXIT();
	return ERROR_NONE;
}

/*  */

MUINT32 kd_MultiSensorGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT * pSensorResolution[2])
{
	MUINT32 ret = ERROR_NONE;
	u32 i = 0;

	KD_MULTI_FUNCTION_ENTRY();
	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		if (g_bEnableDriver[i] && g_pInvokeSensorFunc[i]) {
			if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]) {
				ret =
				    g_pInvokeSensorFunc[i]->SensorGetResolution(pSensorResolution
										[0]);
			} else if ((DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i])
				   || (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i])) {
				ret =
				    g_pInvokeSensorFunc[i]->SensorGetResolution(pSensorResolution
										[1]);
			}

			if (ERROR_NONE != ret) {
				PK_ERR("[%s]\n", __func__);
				return ret;
			}
		}
	}

	KD_MULTI_FUNCTION_EXIT();
	return ERROR_NONE;
}


/*  */
MUINT32
kd_MultiSensorFeatureControl(CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera,
			     MSDK_SENSOR_FEATURE_ENUM FeatureId,
			     MUINT8 *pFeaturePara, MUINT32 *pFeatureParaLen)
{
	MUINT32 ret = ERROR_NONE;
	u32 i = 0;

	KD_MULTI_FUNCTION_ENTRY();

	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		if (g_bEnableDriver[i] && g_pInvokeSensorFunc[i]) {

			if (InvokeCamera == g_invokeSocketIdx[i]) {

#if 0
				if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = SENSOR_I2C_BUS_NUM[g_invokeSocketIdx[i]];
					spin_unlock(&kdsensor_drv_lock);
					PK_XLOG_INFO("kd_MultiSensorOpen: switch I2C BUS%d\n",
						     gI2CBusNum);
				}
#else
				if (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num2;
					spin_unlock(&kdsensor_drv_lock);
					/* PK_DBG("kd_MultiSensorFeatureControl: switch I2C BUS2\n"); */
				} else {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num1;
					spin_unlock(&kdsensor_drv_lock);
					/* PK_DBG("kd_MultiSensorFeatureControl: switch I2C BUS1\n"); */
				}
#endif
				/*  */
				/* set i2c slave ID */
				/* KD_SET_I2C_SLAVE_ID(i,g_invokeSocketIdx[i],IMGSENSOR_SET_I2C_ID_STATE); */
				/*  */
				ret =
				    g_pInvokeSensorFunc[i]->SensorFeatureControl(FeatureId,
										 pFeaturePara,
										 pFeatureParaLen);
				if (ERROR_NONE != ret) {
					PK_ERR("[%s]\n", __func__);
					return ret;
				}
			}
		}
	}
	/*KD_MULTI_FUNCTION_EXIT();*/
	return ERROR_NONE;
}

/*  */
MUINT32
kd_MultiSensorControl(CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera,
		      MSDK_SCENARIO_ID_ENUM ScenarioId,
		      MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		      MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	MUINT32 ret = ERROR_NONE;
	u32 i = 0;

	KD_MULTI_FUNCTION_ENTRY();
	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		if (g_bEnableDriver[i] && g_pInvokeSensorFunc[i]) {
			if (InvokeCamera == g_invokeSocketIdx[i]) {

#if 0
				if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = SENSOR_I2C_BUS_NUM[g_invokeSocketIdx[i]];
					spin_unlock(&kdsensor_drv_lock);
					PK_XLOG_INFO("kd_MultiSensorOpen: switch I2C BUS%d\n",
						     gI2CBusNum);
				}
#else
				if (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num2;
					spin_unlock(&kdsensor_drv_lock);
					/* PK_DBG("kd_MultiSensorControl: switch I2C BUS2\n"); */
				} else {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num1;
					spin_unlock(&kdsensor_drv_lock);
					/* PK_DBG("kd_MultiSensorControl: switch I2C BUS1\n"); */
				}
#endif
				/*  */
				/* set i2c slave ID */
				/* KD_SET_I2C_SLAVE_ID(i,g_invokeSocketIdx[i],IMGSENSOR_SET_I2C_ID_STATE); */
				/*  */
				g_pInvokeSensorFunc[i]->ScenarioId = ScenarioId;
				memcpy(&g_pInvokeSensorFunc[i]->imageWindow, pImageWindow,
				       sizeof(ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT));
				memcpy(&g_pInvokeSensorFunc[i]->sensorConfigData, pSensorConfigData,
				       sizeof(ACDK_SENSOR_CONFIG_STRUCT));
				ret =
				    g_pInvokeSensorFunc[i]->SensorControl(ScenarioId, pImageWindow,
									  pSensorConfigData);
				if (ERROR_NONE != ret) {
					PK_ERR("ERR:SensorControl(), i =%d\n", i);
					return ret;
				}
			}
		}
	}
	KD_MULTI_FUNCTION_EXIT();


	/* js_tst FIXME */
	/* if (DUAL_CHANNEL_I2C) { */
	/* trigger dual channel i2c */
	/* } */
	/* else { */
	if (g_bEnableDriver[1]) {	/* drive 2 or more sensor simultaneously */
		MUINT8 frameSync = 0;
		MUINT32 frameSyncSize = 0;

		kd_MultiSensorFeatureControl(g_invokeSocketIdx[1], SENSOR_FEATURE_SUSPEND,
					     &frameSync, &frameSyncSize);
		mDELAY(10);
		kd_MultiSensorFeatureControl(g_invokeSocketIdx[1], SENSOR_FEATURE_RESUME,
					     &frameSync, &frameSyncSize);
	}
	/* } */


	return ERROR_NONE;
}

/*  */
MUINT32 kd_MultiSensorClose(void)
{
	MUINT32 ret = ERROR_NONE;
	u32 i = 0;

	KD_MULTI_FUNCTION_ENTRY();
	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		if (g_bEnableDriver[i] && g_pInvokeSensorFunc[i]) {
			if (0 != (g_CurrentSensorIdx & g_invokeSocketIdx[i])) {
#if 0
				if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]
				    || DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = SENSOR_I2C_BUS_NUM[g_invokeSocketIdx[i]];
					spin_unlock(&kdsensor_drv_lock);
					PK_XLOG_INFO("kd_MultiSensorOpen: switch I2C BUS%d\n",
						     gI2CBusNum);
				}
#else


				if (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]) {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num2;
					spin_unlock(&kdsensor_drv_lock);
					PK_DBG("kd_MultiSensorClose: switch I2C BUS2\n");
				} else {
					spin_lock(&kdsensor_drv_lock);
					gI2CBusNum = camera_i2c_bus_num1;
					spin_unlock(&kdsensor_drv_lock);
					PK_DBG("kd_MultiSensorClose: switch I2C BUS1\n");
				}
#endif
				ret = g_pInvokeSensorFunc[i]->SensorClose();

				/* Change the close power flow to close power in this function & */
				/* directly call kdCISModulePowerOn to close the specific sensor */
				/* The original flow will close all opened sensors at once */
				kdCISModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM)
						   g_invokeSocketIdx[i],
						   (char *)g_invokeSensorNameStr[i], false,
						   CAMERA_HW_DRVNAME1);

				if (ERROR_NONE != ret) {
					PK_ERR("[%s]", __func__);
					return ret;
				}
			}
		}
	}
	KD_MULTI_FUNCTION_EXIT();
	return ERROR_NONE;
}

/*  */
MULTI_SENSOR_FUNCTION_STRUCT2 kd_MultiSensorFunc = {
	kd_MultiSensorOpen,
	kd_MultiSensorGetInfo,
	kd_MultiSensorGetResolution,
	kd_MultiSensorFeatureControl,
	kd_MultiSensorControl,
	kd_MultiSensorClose
};


/*******************************************************************************
* kdModulePowerOn
********************************************************************************/
int
kdModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM socketIdx[KDIMGSENSOR_MAX_INVOKE_DRIVERS],
		char sensorNameStr[KDIMGSENSOR_MAX_INVOKE_DRIVERS][32], BOOL On, char *mode_name)
{
	MINT32 ret = ERROR_NONE;
	u32 i = 0;
	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		if (g_bEnableDriver[i]) {
			/* PK_DBG("[%s][%d][%d][%s][%s]\r\n",__FUNCTION__,g_bEnableDriver[i],socketIdx[i],sensorNameStr[i],mode_name); */
			ret = kdCISModulePowerOn(socketIdx[i], sensorNameStr[i], On, mode_name);
			if (ERROR_NONE != ret) {
				PK_ERR("[%s]", __func__);
				return ret;
			}
		}
	}
	return ERROR_NONE;
}

/*******************************************************************************
* kdSetDriver
********************************************************************************/
int kdSetDriver(unsigned int *pDrvIndex)
{
	ACDK_KD_SENSOR_INIT_FUNCTION_STRUCT *pSensorList = NULL;
	u32 drvIdx[KDIMGSENSOR_MAX_INVOKE_DRIVERS] = { 0, 0 };
	u32 i;
	PK_XLOG_INFO("pDrvIndex:0x%08x/0x%08x\n", pDrvIndex[KDIMGSENSOR_INVOKE_DRIVER_0],
		     pDrvIndex[KDIMGSENSOR_INVOKE_DRIVER_1]);
	/* set driver for MAIN or SUB sensor */

	if (0 != kdGetSensorInitFuncList(&pSensorList)) {
		PK_ERR("ERROR:kdGetSensorInitFuncList()\n");
		return -EIO;
	}

	for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
		/*  */
		spin_lock(&kdsensor_drv_lock);
		g_bEnableDriver[i] = FALSE;
		g_invokeSocketIdx[i] =
		    (CAMERA_DUAL_CAMERA_SENSOR_ENUM) ((pDrvIndex[i] & KDIMGSENSOR_DUAL_MASK_MSB) >>
						      KDIMGSENSOR_DUAL_SHIFT);
		spin_unlock(&kdsensor_drv_lock);
		drvIdx[i] = (pDrvIndex[i] & KDIMGSENSOR_DUAL_MASK_LSB);
		/*  */
		if (DUAL_CAMERA_NONE_SENSOR == g_invokeSocketIdx[i]) {
			continue;
		}
#if 0
		if (DUAL_CAMERA_MAIN_SENSOR == g_invokeSocketIdx[i]
		    || DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]
		    || DUAL_CAMERA_MAIN_2_SENSOR == g_invokeSocketIdx[i]) {
			spin_lock(&kdsensor_drv_lock);
			gI2CBusNum = SENSOR_I2C_BUS_NUM[g_invokeSocketIdx[i]];
			spin_unlock(&kdsensor_drv_lock);
			PK_XLOG_INFO("kd_MultiSensorOpen: switch I2C BUS%d\n", gI2CBusNum);
		}
#else
		/*
		   FIX ME: Sub cam may use the same I2C Bus#1 as Main cam
		 */
		if (DUAL_CAMERA_SUB_SENSOR == g_invokeSocketIdx[i]) {
			spin_lock(&kdsensor_drv_lock);
			gI2CBusNum = camera_i2c_bus_num2;
			spin_unlock(&kdsensor_drv_lock);
			PK_XLOG_INFO("Sub cam uses I2C BUS#: %d\n", gI2CBusNum);
		} else {
			spin_lock(&kdsensor_drv_lock);
			gI2CBusNum = camera_i2c_bus_num1;
			spin_unlock(&kdsensor_drv_lock);
			PK_XLOG_INFO("Main cam uses I2C BUS#: %d\n", gI2CBusNum);
		}
#endif
		/* ToDo: remove print information */
		PK_XLOG_INFO("[kdSetDriver] i(%d),g_invokeSocketIdx[%d] = %d :\n", i, i, drvIdx[i]);
		PK_XLOG_INFO("[kdSetDriver] i(%d),drvIdx[%d] = %d :\n", i, i, drvIdx[i]);
		/*  */
		if (MAX_NUM_OF_SUPPORT_SENSOR > drvIdx[i]) {
			if (NULL == pSensorList[drvIdx[i]].SensorInit) {
				PK_ERR("ERROR:kdSetDriver()\n");
				return -EIO;
			}

			pSensorList[drvIdx[i]].SensorInit(&g_pInvokeSensorFunc[i]);
			if (NULL == g_pInvokeSensorFunc[i]) {
				PK_ERR("ERROR:NULL g_pSensorFunc[%d]\n", i);
				return -EIO;
			}
			/*  */
			spin_lock(&kdsensor_drv_lock);
			g_bEnableDriver[i] = TRUE;
			spin_unlock(&kdsensor_drv_lock);
			/* get sensor name */
			memcpy((char *)g_invokeSensorNameStr[i],
			       (char *)pSensorList[drvIdx[i]].drvname,
			       sizeof(pSensorList[drvIdx[i]].drvname));
			/* return sensor ID */
			/* pDrvIndex[0] = (unsigned int)pSensorList[drvIdx].SensorId; */
			PK_XLOG_INFO("[kdSetDriver] :[%d][%d][%d][%s][%d]\n", i, g_bEnableDriver[i],
				     g_invokeSocketIdx[i], g_invokeSensorNameStr[i],
				     (int)sizeof(pSensorList[drvIdx[i]].drvname));

	#ifdef CAMERA_MODULE_INFO
				if (g_invokeSocketIdx[i] == DUAL_CAMERA_MAIN_SENSOR) {
					snprintf(back_camera_module_name, sizeof(back_camera_module_name), "%s", pSensorList[drvIdx[i]].drvname);
				} else if (g_invokeSocketIdx[i] == DUAL_CAMERA_SUB_SENSOR) {
					snprintf(front_camera_module_name, sizeof(front_camera_module_name), "%s", pSensorList[drvIdx[i]].drvname);
				}
	#endif

		}
	}
	return 0;
}

int kdSetCurrentSensorIdx(unsigned int idx)
{
	g_CurrentSensorIdx = idx;
	return 0;
}

/*******************************************************************************
* kdGetSocketPostion
********************************************************************************/
int kdGetSocketPostion(unsigned int *pSocketPos)
{
	PK_XLOG_INFO("[%s][%d] \r\n", __func__, *pSocketPos);
	switch (*pSocketPos) {
	case DUAL_CAMERA_MAIN_SENSOR:
		/* ->this is a HW layout dependent */
		/* ToDo */
		*pSocketPos = IMGSENSOR_SOCKET_POS_RIGHT;
		break;
	case DUAL_CAMERA_MAIN_2_SENSOR:
		*pSocketPos = IMGSENSOR_SOCKET_POS_LEFT;
		break;
	default:
	case DUAL_CAMERA_SUB_SENSOR:
		*pSocketPos = IMGSENSOR_SOCKET_POS_NONE;
		break;
	}
	return 0;
}

/*******************************************************************************
* kdSetSensorSyncFlag
********************************************************************************/
int kdSetSensorSyncFlag(BOOL bSensorSync)
{
	spin_lock(&kdsensor_drv_lock);

	bSesnorVsyncFlag = bSensorSync;
	spin_unlock(&kdsensor_drv_lock);
/* PK_DBG("[Sensor] kdSetSensorSyncFlag:%d\n", bSesnorVsyncFlag); */

	/* strobe_VDIrq(); //cotta : added for high current solution */

	return 0;
}

/*******************************************************************************
* kdCheckSensorPowerOn
********************************************************************************/
int kdCheckSensorPowerOn(void)
{
	if (atomic_read(&g_CamHWOpening) == 0) {	/* sensor power off */
		return 0;
	} else {		/* sensor power on */
		return 1;
	}
}

/*******************************************************************************
* kdSensorSyncFunctionPtr
********************************************************************************/
/* ToDo: How to separate main/main2....who is caller? */
int kdSensorSyncFunctionPtr(void)
{
	unsigned int FeatureParaLen = 0;
	/* PK_DBG("[Sensor] kdSensorSyncFunctionPtr1:%d %d %d\n", g_NewSensorExpGain.uSensorExpDelayFrame, g_NewSensorExpGain.uSensorGainDelayFrame, g_NewSensorExpGain.uISPGainDelayFrame); */
	mutex_lock(&kdCam_Mutex);
	if (NULL == g_pSensorFunc) {
		PK_ERR("ERROR:NULL g_pSensorFunc\n");
		mutex_unlock(&kdCam_Mutex);
		return -EIO;
	}
	/* PK_DBG("[Sensor] Exposure time:%d, Gain = %d\n", g_NewSensorExpGain.u2SensorNewExpTime,g_NewSensorExpGain.u2SensorNewGain ); */
	/* exposure time */
	if (g_NewSensorExpGain.uSensorExpDelayFrame == 0) {
		FeatureParaLen = 2;
		g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_SENSOR,
						    SENSOR_FEATURE_SET_ESHUTTER,
						    (unsigned char *)
						    &g_NewSensorExpGain.u2SensorNewExpTime,
						    (unsigned int *)&FeatureParaLen);
		g_NewSensorExpGain.uSensorExpDelayFrame = 0xFF;	/* disable */
	} else if (g_NewSensorExpGain.uSensorExpDelayFrame != 0xFF) {
		g_NewSensorExpGain.uSensorExpDelayFrame--;
	}
	/* exposure gain */
	if (g_NewSensorExpGain.uSensorGainDelayFrame == 0) {
		FeatureParaLen = 2;
		g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_SENSOR,
						    SENSOR_FEATURE_SET_GAIN,
						    (unsigned char *)
						    &g_NewSensorExpGain.u2SensorNewGain,
						    (unsigned int *)&FeatureParaLen);
		g_NewSensorExpGain.uSensorGainDelayFrame = 0xFF;	/* disable */
	} else if (g_NewSensorExpGain.uSensorGainDelayFrame != 0xFF) {
		g_NewSensorExpGain.uSensorGainDelayFrame--;
	}
	/* if the delay frame is 0 or 0xFF, stop to count */
	if ((g_NewSensorExpGain.uISPGainDelayFrame != 0xFF)
	    && (g_NewSensorExpGain.uISPGainDelayFrame != 0)) {
		g_NewSensorExpGain.uISPGainDelayFrame--;
	}
	mutex_unlock(&kdCam_Mutex);
	return 0;
}

/*******************************************************************************
* kdGetRawGainInfo
********************************************************************************/
int kdGetRawGainInfoPtr(UINT16 *pRAWGain)
{
	*pRAWGain = 0x00;
	*(pRAWGain + 1) = 0x00;
	*(pRAWGain + 2) = 0x00;
	*(pRAWGain + 3) = 0x00;

	if (g_NewSensorExpGain.uISPGainDelayFrame == 0) {	/* synchronize the isp gain */
		*pRAWGain = g_NewSensorExpGain.u2ISPNewRGain;
		*(pRAWGain + 1) = g_NewSensorExpGain.u2ISPNewGrGain;
		*(pRAWGain + 2) = g_NewSensorExpGain.u2ISPNewGbGain;
		*(pRAWGain + 3) = g_NewSensorExpGain.u2ISPNewBGain;
/* PK_DBG("[Sensor] ISP Gain:%d\n", g_NewSensorExpGain.u2ISPNewRGain, g_NewSensorExpGain.u2ISPNewGrGain, */
/* g_NewSensorExpGain.u2ISPNewGbGain, g_NewSensorExpGain.u2ISPNewBGain); */
		spin_lock(&kdsensor_drv_lock);
		g_NewSensorExpGain.uISPGainDelayFrame = 0xFF;	/* disable */
		spin_unlock(&kdsensor_drv_lock);
	}

	return 0;
}




int kdSetExpGain(CAMERA_DUAL_CAMERA_SENSOR_ENUM InvokeCamera)
{
	unsigned int FeatureParaLen = 0;

	PK_DBG("[kd_sensorlist]enter kdSetExpGain\n");
	if (NULL == g_pSensorFunc) {
		PK_ERR("ERROR:NULL g_pSensorFunc\n");

		return -EIO;
	}

	setExpGainDoneFlag = 0;
	FeatureParaLen = 2;
	g_pSensorFunc->SensorFeatureControl(InvokeCamera, SENSOR_FEATURE_SET_ESHUTTER,
					    (unsigned char *)&g_NewSensorExpGain.u2SensorNewExpTime,
					    (unsigned int *)&FeatureParaLen);
	g_pSensorFunc->SensorFeatureControl(InvokeCamera, SENSOR_FEATURE_SET_GAIN,
					    (unsigned char *)&g_NewSensorExpGain.u2SensorNewGain,
					    (unsigned int *)&FeatureParaLen);

	setExpGainDoneFlag = 1;
	PK_DBG("[kd_sensorlist]before wake_up_interruptible\n");
	wake_up_interruptible(&kd_sensor_wait_queue);
	PK_DBG("[kd_sensorlist]after wake_up_interruptible\n");

	return 0;		/* No error. */

}

/*******************************************************************************
*
********************************************************************************/
static UINT32 ms_to_jiffies(MUINT32 ms)
{
	return ((ms * HZ + 512) >> 10);
}


int kdSensorSetExpGainWaitDone(int *ptime)
{
	int timeout;

	PK_DBG("[kd_sensorlist]enter kdSensorSetExpGainWaitDone: time: %d\n", *ptime);
	timeout = wait_event_interruptible_timeout(kd_sensor_wait_queue,
						   (setExpGainDoneFlag & 1), ms_to_jiffies(*ptime));

	PK_DBG("[kd_sensorlist]after wait_event_interruptible_timeout\n");
	if (timeout == 0) {
		PK_ERR("[kd_sensorlist] kdSensorSetExpGainWait: timeout=%d\n", *ptime);

		return -EAGAIN;
	}

	return 0;		/* No error. */

}




/*******************************************************************************
* adopt_CAMERA_HW_Open
********************************************************************************/
inline static int adopt_CAMERA_HW_Open(void)
{
	UINT32 err = 0;

	KD_IMGSENSOR_PROFILE_INIT();
	/* power on sensor */
	/* if (atomic_read(&g_CamHWOpend) == 0  ) { */
	/* move into SensorOpen() for 2on1 driver */
	/* turn on power */
	/* kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM*) g_invokeSocketIdx, g_invokeSensorNameStr,true, CAMERA_HW_DRVNAME); */
	/* wait for power stable */
	/* mDELAY(10); */
	/* KD_IMGSENSOR_PROFILE("kdModulePowerOn"); */
	/*  */
	if (g_pSensorFunc) {
		err = g_pSensorFunc->SensorOpen();
		if (ERROR_NONE != err) {
			/*
			   PK_DBG(" ERROR:SensorOpen(), turn off power, Mclk1(0x%x), ISPClk(0x%x), MclkPll(0x%x)\n",
			   (unsigned int)SENSOR_RD32(0x15008204),
			   (unsigned int)SENSOR_RD32(0x15000000),
			   (unsigned int)SENSOR_RD32(0x10000060)
			   ); */
			kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM *) g_invokeSocketIdx,
					g_invokeSensorNameStr, false, CAMERA_HW_DRVNAME1);
		}
	} else {
		PK_DBG(" ERROR:NULL g_pSensorFunc\n");
	}

	KD_IMGSENSOR_PROFILE("SensorOpen");
	/* } */
	/* else { */
	/* PK_ERR("adopt_CAMERA_HW_Open Fail, g_CamHWOpend = %d\n ",atomic_read(&g_CamHWOpend) ); */
	/* } */

	/* if (err == 0 ) { */
	/* atomic_set(&g_CamHWOpend, 1); */

	/* } */

	return err ? -EIO : err;
}				/* adopt_CAMERA_HW_Open() */

/*******************************************************************************
* adopt_CAMERA_HW_CheckIsAlive
********************************************************************************/
inline static int adopt_CAMERA_HW_CheckIsAlive(void)
{
	UINT32 err = 0;
	UINT32 err1 = 0;
	UINT32 i = 0;
	MUINT32 sensorID = 0;
	MUINT32 retLen = 0;

	KD_IMGSENSOR_PROFILE_INIT();
	/* power on sensor */
	err =
	    kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM *) g_invokeSocketIdx,
			    g_invokeSensorNameStr, true, CAMERA_HW_DRVNAME1);
	/* Bypass redundant search operation of getting sensor ID, if power on failed */
	if (ERROR_NONE != err) {
		PK_DBG("%s\n",
		       err ==
		       -ENODEV ? "No device in this socket position" : "kdModulePowerOn failed");
		return err;
	}
	/* wait for power stable */
	mDELAY(10);
	KD_IMGSENSOR_PROFILE("kdModulePowerOn");

	g_IsSearchSensor = 1;

	if (g_pSensorFunc) {
		for (i = KDIMGSENSOR_INVOKE_DRIVER_0; i < KDIMGSENSOR_MAX_INVOKE_DRIVERS; i++) {
			if (DUAL_CAMERA_NONE_SENSOR != g_invokeSocketIdx[i]) {
				err =
				    g_pSensorFunc->SensorFeatureControl(g_invokeSocketIdx[i],
									SENSOR_FEATURE_CHECK_SENSOR_ID,
									(MUINT8 *) &sensorID,
									&retLen);
				if (sensorID == 0) {	/* not implement this feature ID */
					PK_DBG
					    (" Not implement!!, use old open function to check\n");
					err = ERROR_SENSOR_CONNECT_FAIL;
				} else if (sensorID == 0xFFFFFFFF) {	/* fail to open the sensor */
					PK_DBG(" No Sensor Found");
					err = ERROR_SENSOR_CONNECT_FAIL;
				} else {

					PK_DBG(" Sensor found ID = 0x%x\n", sensorID);
					err = ERROR_NONE;
				}
				if (ERROR_NONE != err) {
					PK_DBG
					    ("ERROR:adopt_CAMERA_HW_CheckIsAlive(), No imgsensor alive\n");
				}
			}
		}
	} else {
		PK_DBG("ERROR:NULL g_pSensorFunc\n");
	}

	/* reset sensor state after power off */
	if (g_pSensorFunc)
        err1 = g_pSensorFunc->SensorClose();
	if (ERROR_NONE != err1) {
		PK_DBG("SensorClose\n");
	}
	/*  */
	kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM *) g_invokeSocketIdx, g_invokeSensorNameStr,
			false, CAMERA_HW_DRVNAME1);
	/*  */
	KD_IMGSENSOR_PROFILE("CheckIsAlive");

	g_IsSearchSensor = 0;

	return err ? -EIO : err;
}				/* adopt_CAMERA_HW_Open() */


/*******************************************************************************
* adopt_CAMERA_HW_GetResolution
********************************************************************************/
inline static int adopt_CAMERA_HW_GetResolution(void *pBuf)
{
/* ToDo: remove print */
	ACDK_SENSOR_PRESOLUTION_STRUCT *pBufResolution = (ACDK_SENSOR_PRESOLUTION_STRUCT *) pBuf;

	ACDK_SENSOR_RESOLUTION_INFO_STRUCT * pRes[2] = { NULL, NULL };

	PK_XLOG_INFO("[CAMERA_HW] adopt_CAMERA_HW_GetResolution\n");

	pRes[0] =
	    (ACDK_SENSOR_RESOLUTION_INFO_STRUCT *)
	    kmalloc(sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT), GFP_KERNEL);

	if (pRes[0] == NULL) {
		PK_ERR(" ioctl allocate mem failed\n");
		return -ENOMEM;
	}
	pRes[1] =
	    (ACDK_SENSOR_RESOLUTION_INFO_STRUCT *)
	    kmalloc(sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT), GFP_KERNEL);
	if (pRes[1] == NULL) {
		kfree(pRes[0]);
		PK_ERR(" ioctl allocate mem failed\n");
		return -ENOMEM;
	}


	if (g_pSensorFunc) {
		g_pSensorFunc->SensorGetResolution(pRes);
		if (copy_to_user((void __user *)(pBufResolution->pResolution[0]),
				 (void *)pRes[0], sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT)))
			PK_ERR("copy to user failed\n");

		if (copy_to_user((void __user *)(pBufResolution->pResolution[1]),
				 (void *)pRes[1], sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT)))
			PK_ERR("copy to user failed\n");
    }
    else {
		PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
    }
	if (pRes[0] != NULL) {
		kfree(pRes[0]);
	}
	if (pRes[1] != NULL) {
		kfree(pRes[1]);
	}

	return 0;
}				/* adopt_CAMERA_HW_GetResolution() */


/*******************************************************************************
* adopt_CAMERA_HW_GetInfo
********************************************************************************/
inline static int adopt_CAMERA_HW_GetInfo(void *pBuf)
{
	ACDK_SENSOR_GETINFO_STRUCT *pSensorGetInfo = (ACDK_SENSOR_GETINFO_STRUCT *) pBuf;
	MSDK_SENSOR_INFO_STRUCT info[2], *pInfo[2];
	MSDK_SENSOR_CONFIG_STRUCT config[2], *pConfig[2];
	MUINT32 *pScenarioId[2];
	u32 i = 0;

	for (i = 0; i < 2; i++) {
		pInfo[i] = &info[i];
		pConfig[i] = &config[i];
		pScenarioId[i] = &(pSensorGetInfo->ScenarioId[i]);
	}


	if (NULL == pSensorGetInfo) {
		PK_DBG("[CAMERA_HW] NULL arg.\n");
		return -EFAULT;
	}

	if ((NULL == pSensorGetInfo->pInfo[0]) || (NULL == pSensorGetInfo->pInfo[1]) ||
	    (NULL == pSensorGetInfo->pConfig[0]) || (NULL == pSensorGetInfo->pConfig[1])) {
		PK_DBG("[CAMERA_HW] NULL arg.\n");
		return -EFAULT;
	}

	if (g_pSensorFunc) {
		g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo, pConfig);
	} else {
		PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
	}



	for (i = 0; i < 2; i++) {
		/* SenorInfo */
		if (copy_to_user
		    ((void __user *)(pSensorGetInfo->pInfo[i]), (void *)pInfo[i],
		     sizeof(MSDK_SENSOR_INFO_STRUCT))) {
			PK_DBG("[CAMERA_HW][info] ioctl copy to user failed\n");
			return -EFAULT;
		}
		/* SensorConfig */
		if (copy_to_user
		    ((void __user *)(pSensorGetInfo->pConfig[i]), (void *)pConfig[i],
		     sizeof(MSDK_SENSOR_CONFIG_STRUCT))) {
			PK_DBG("[CAMERA_HW][config] ioctl copy to user failed\n");
			return -EFAULT;
		}
	}
	return 0;
}				/* adopt_CAMERA_HW_GetInfo() */

/*******************************************************************************
* adopt_CAMERA_HW_GetInfo
********************************************************************************/
MSDK_SENSOR_INFO_STRUCT ginfo[2];
MSDK_SENSOR_INFO_STRUCT ginfo1[2];
MSDK_SENSOR_INFO_STRUCT ginfo2[2];
MSDK_SENSOR_INFO_STRUCT ginfo3[2];
MSDK_SENSOR_INFO_STRUCT ginfo4[2];
/* adopt_CAMERA_HW_GetInfo() */
inline static int adopt_CAMERA_HW_GetInfo2(void *pBuf)
{
	IMAGESENSOR_GETINFO_STRUCT *pSensorGetInfo = (IMAGESENSOR_GETINFO_STRUCT *) pBuf;
	ACDK_SENSOR_INFO2_STRUCT SensorInfo = { 0 };
	MUINT32 IDNum = 0;
	MSDK_SENSOR_INFO_STRUCT *pInfo[2];
	MSDK_SENSOR_CONFIG_STRUCT config[2], *pConfig[2];
	MSDK_SENSOR_INFO_STRUCT *pInfo1[2];
	MSDK_SENSOR_CONFIG_STRUCT config1[2], *pConfig1[2];
	MSDK_SENSOR_INFO_STRUCT *pInfo2[2];
	MSDK_SENSOR_CONFIG_STRUCT config2[2], *pConfig2[2];
	MSDK_SENSOR_INFO_STRUCT *pInfo3[2];
	MSDK_SENSOR_CONFIG_STRUCT config3[2], *pConfig3[2];
	MSDK_SENSOR_INFO_STRUCT *pInfo4[2];
	MSDK_SENSOR_CONFIG_STRUCT config4[2], *pConfig4[2];
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT SensorResolution[2], *psensorResolution[2];

	MUINT32 ScenarioId[2], *pScenarioId[2];
	u32 i = 0;

	PK_DBG("[adopt_CAMERA_HW_GetInfo2]Entry\n");
	for (i = 0; i < 2; i++) {
		pInfo[i] = &ginfo[i];
		pConfig[i] = &config[i];
		pInfo1[i] = &ginfo1[i];
		pConfig1[i] = &config1[i];
		pInfo2[i] = &ginfo2[i];
		pConfig2[i] = &config2[i];
		pInfo3[i] = &ginfo3[i];
		pConfig3[i] = &config3[i];
		pInfo4[i] = &ginfo4[i];
		pConfig4[i] = &config4[i];
		psensorResolution[i] = &SensorResolution[i];
		pScenarioId[i] = &ScenarioId[i];
	}

	if (NULL == pSensorGetInfo) {
		PK_DBG("[CAMERA_HW] NULL arg.\n");
		return -EFAULT;
	}
	if (NULL == g_pSensorFunc) {
		PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
		return -EFAULT;
	}

	PK_DBG("[CAMERA_HW][Resolution] %p\n", pSensorGetInfo->pSensorResolution);

	/* TO get preview value */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo, pConfig);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo1, pConfig1);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo2, pConfig2);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo3, pConfig3);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_SLIM_VIDEO;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo4, pConfig4);
	/* To set sensor information */
	if (DUAL_CAMERA_MAIN_SENSOR == pSensorGetInfo->SensorId) {
		IDNum = 0;
	} else {
		IDNum = 1;
	}
	PK_DBG("pSensorGetInfo->SensorId = %d , IDNum = %d\n", pSensorGetInfo->SensorId, IDNum);
	/* Basic information */
	SensorInfo.SensorPreviewResolutionX = pInfo[IDNum]->SensorPreviewResolutionX;
	SensorInfo.SensorPreviewResolutionY = pInfo[IDNum]->SensorPreviewResolutionY;
	SensorInfo.SensorFullResolutionX = pInfo[IDNum]->SensorFullResolutionX;
	SensorInfo.SensorFullResolutionY = pInfo[IDNum]->SensorFullResolutionY;
	SensorInfo.SensorClockFreq = pInfo[IDNum]->SensorClockFreq;
	SensorInfo.SensorCameraPreviewFrameRate = pInfo[IDNum]->SensorCameraPreviewFrameRate;
	SensorInfo.SensorVideoFrameRate = pInfo[IDNum]->SensorVideoFrameRate;
	SensorInfo.SensorStillCaptureFrameRate = pInfo[IDNum]->SensorStillCaptureFrameRate;
	SensorInfo.SensorWebCamCaptureFrameRate = pInfo[IDNum]->SensorWebCamCaptureFrameRate;
	SensorInfo.SensorClockPolarity = pInfo[IDNum]->SensorClockPolarity;
	SensorInfo.SensorClockFallingPolarity = pInfo[IDNum]->SensorClockFallingPolarity;
	SensorInfo.SensorClockRisingCount = pInfo[IDNum]->SensorClockRisingCount;
	SensorInfo.SensorClockFallingCount = pInfo[IDNum]->SensorClockFallingCount;
	SensorInfo.SensorClockDividCount = pInfo[IDNum]->SensorClockDividCount;
	SensorInfo.SensorPixelClockCount = pInfo[IDNum]->SensorPixelClockCount;
	SensorInfo.SensorDataLatchCount = pInfo[IDNum]->SensorDataLatchCount;
	SensorInfo.SensorHsyncPolarity = pInfo[IDNum]->SensorHsyncPolarity;
	SensorInfo.SensorVsyncPolarity = pInfo[IDNum]->SensorVsyncPolarity;
	SensorInfo.SensorInterruptDelayLines = pInfo[IDNum]->SensorInterruptDelayLines;
	SensorInfo.SensorResetActiveHigh = pInfo[IDNum]->SensorResetActiveHigh;
	SensorInfo.SensorResetDelayCount = pInfo[IDNum]->SensorResetDelayCount;
	SensorInfo.SensroInterfaceType = pInfo[IDNum]->SensroInterfaceType;
	SensorInfo.SensorOutputDataFormat = pInfo[IDNum]->SensorOutputDataFormat;

	PK_DBG("SensorInfo.SensorOutputDataFormat = 0x%x , pinfo[0]=0x%x , pinfo[1]=0x%x\n",
	       SensorInfo.SensorOutputDataFormat,
	       pInfo[0]->SensorOutputDataFormat, pInfo[1]->SensorOutputDataFormat);

	SensorInfo.SensorMIPILaneNumber = pInfo[IDNum]->SensorMIPILaneNumber;
	SensorInfo.CaptureDelayFrame = pInfo[IDNum]->CaptureDelayFrame;
	SensorInfo.PreviewDelayFrame = pInfo[IDNum]->PreviewDelayFrame;
	SensorInfo.VideoDelayFrame = pInfo[IDNum]->VideoDelayFrame;
	SensorInfo.HighSpeedVideoDelayFrame = pInfo[IDNum]->HighSpeedVideoDelayFrame;
	SensorInfo.SlimVideoDelayFrame = pInfo[IDNum]->SlimVideoDelayFrame;
	SensorInfo.Custom1DelayFrame = pInfo[IDNum]->Custom1DelayFrame;
	SensorInfo.Custom2DelayFrame = pInfo[IDNum]->Custom2DelayFrame;
	SensorInfo.Custom3DelayFrame = pInfo[IDNum]->Custom3DelayFrame;
	SensorInfo.Custom4DelayFrame = pInfo[IDNum]->Custom4DelayFrame;
	SensorInfo.Custom5DelayFrame = pInfo[IDNum]->Custom5DelayFrame;
	SensorInfo.YUVAwbDelayFrame = pInfo[IDNum]->YUVAwbDelayFrame;
	SensorInfo.YUVEffectDelayFrame = pInfo[IDNum]->YUVEffectDelayFrame;
	SensorInfo.SensorGrabStartX_PRV = pInfo[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_PRV = pInfo[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_CAP = pInfo1[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_CAP = pInfo1[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_VD = pInfo2[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_VD = pInfo2[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_VD1 = pInfo3[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_VD1 = pInfo3[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_VD2 = pInfo4[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_VD2 = pInfo4[IDNum]->SensorGrabStartY;
	SensorInfo.SensorDrivingCurrent = pInfo[IDNum]->SensorDrivingCurrent;
	SensorInfo.SensorMasterClockSwitch = pInfo[IDNum]->SensorMasterClockSwitch;
	SensorInfo.AEShutDelayFrame = pInfo[IDNum]->AEShutDelayFrame;
	SensorInfo.AESensorGainDelayFrame = pInfo[IDNum]->AESensorGainDelayFrame;
	SensorInfo.AEISPGainDelayFrame = pInfo[IDNum]->AEISPGainDelayFrame;
	SensorInfo.MIPIDataLowPwr2HighSpeedTermDelayCount =
	    pInfo[IDNum]->MIPIDataLowPwr2HighSpeedTermDelayCount;
	SensorInfo.MIPIDataLowPwr2HighSpeedSettleDelayCount =
	    pInfo[IDNum]->MIPIDataLowPwr2HighSpeedSettleDelayCount;
	SensorInfo.MIPIDataLowPwr2HSSettleDelayM0 =
	    pInfo[IDNum]->MIPIDataLowPwr2HighSpeedSettleDelayCount;
	SensorInfo.MIPIDataLowPwr2HSSettleDelayM1 =
	    pInfo1[IDNum]->MIPIDataLowPwr2HighSpeedSettleDelayCount;
	SensorInfo.MIPIDataLowPwr2HSSettleDelayM2 =
	    pInfo2[IDNum]->MIPIDataLowPwr2HighSpeedSettleDelayCount;
	SensorInfo.MIPIDataLowPwr2HSSettleDelayM3 =
	    pInfo3[IDNum]->MIPIDataLowPwr2HighSpeedSettleDelayCount;
	SensorInfo.MIPIDataLowPwr2HSSettleDelayM4 =
	    pInfo4[IDNum]->MIPIDataLowPwr2HighSpeedSettleDelayCount;
	SensorInfo.MIPICLKLowPwr2HighSpeedTermDelayCount =
	    pInfo[IDNum]->MIPICLKLowPwr2HighSpeedTermDelayCount;
	SensorInfo.SensorWidthSampling = pInfo[IDNum]->SensorWidthSampling;
	SensorInfo.SensorHightSampling = pInfo[IDNum]->SensorHightSampling;
	SensorInfo.SensorPacketECCOrder = pInfo[IDNum]->SensorPacketECCOrder;
	SensorInfo.MIPIsensorType = pInfo[IDNum]->MIPIsensorType;
	SensorInfo.IHDR_LE_FirstLine = pInfo[IDNum]->IHDR_LE_FirstLine;
	SensorInfo.IHDR_Support = pInfo[IDNum]->IHDR_Support;
	SensorInfo.SensorModeNum = pInfo[IDNum]->SensorModeNum;
	SensorInfo.SettleDelayMode = pInfo[IDNum]->SettleDelayMode;
	SensorInfo.PDAF_Support = pInfo[IDNum]->PDAF_Support;
	SensorInfo.IMGSENSOR_DPCM_TYPE_PRE = pInfo[IDNum]->DPCM_INFO;
	SensorInfo.IMGSENSOR_DPCM_TYPE_CAP = pInfo1[IDNum]->DPCM_INFO;
	SensorInfo.IMGSENSOR_DPCM_TYPE_VD = pInfo2[IDNum]->DPCM_INFO;
	SensorInfo.IMGSENSOR_DPCM_TYPE_VD1 = pInfo3[IDNum]->DPCM_INFO;
	SensorInfo.IMGSENSOR_DPCM_TYPE_VD2 = pInfo4[IDNum]->DPCM_INFO;
	/*Per-Frame conrol support or not */
	SensorInfo.PerFrameCTL_Support = pInfo[IDNum]->PerFrameCTL_Support;
	/*SCAM number */
	SensorInfo.SCAM_DataNumber = pInfo[IDNum]->SCAM_DataNumber;
	SensorInfo.SCAM_DDR_En = pInfo[IDNum]->SCAM_DDR_En;
	SensorInfo.SCAM_CLK_INV = pInfo[IDNum]->SCAM_CLK_INV;
	/* TO get preview value */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CUSTOM1;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo, pConfig);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CUSTOM2;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo1, pConfig1);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CUSTOM3;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo2, pConfig2);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CUSTOM4;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo3, pConfig3);
	/*  */
	ScenarioId[0] = ScenarioId[1] = MSDK_SCENARIO_ID_CUSTOM5;
	g_pSensorFunc->SensorGetInfo(pScenarioId, pInfo4, pConfig4);
	/* To set sensor information */
	if (DUAL_CAMERA_MAIN_SENSOR == pSensorGetInfo->SensorId) {
		IDNum = 0;
	} else {
		IDNum = 1;
	}
	SensorInfo.SensorGrabStartX_CST1 = pInfo[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_CST1 = pInfo[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_CST2 = pInfo1[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_CST2 = pInfo1[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_CST3 = pInfo2[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_CST3 = pInfo2[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_CST4 = pInfo3[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_CST4 = pInfo3[IDNum]->SensorGrabStartY;
	SensorInfo.SensorGrabStartX_CST5 = pInfo4[IDNum]->SensorGrabStartX;
	SensorInfo.SensorGrabStartY_CST5 = pInfo4[IDNum]->SensorGrabStartY;

	if (copy_to_user
	    ((void __user *)(pSensorGetInfo->pInfo), (void *)(&SensorInfo),
	     sizeof(ACDK_SENSOR_INFO2_STRUCT))) {
		PK_DBG("[CAMERA_HW][info] ioctl copy to user failed\n");
		return -EFAULT;
	}
	/* Step2 : Get Resolution */
	g_pSensorFunc->SensorGetResolution(psensorResolution);
	PK_DBG("[CAMERA_HW][Pre]w=0x%x, h = 0x%x\n", SensorResolution[0].SensorPreviewWidth,
	       SensorResolution[0].SensorPreviewHeight);
	PK_DBG("[CAMERA_HW][Full]w=0x%x, h = 0x%x\n", SensorResolution[0].SensorFullWidth,
	       SensorResolution[0].SensorFullHeight);
	PK_DBG("[CAMERA_HW][VD]w=0x%x, h = 0x%x\n", SensorResolution[0].SensorVideoWidth,
	       SensorResolution[0].SensorVideoHeight);

	if (DUAL_CAMERA_MAIN_SENSOR == pSensorGetInfo->SensorId) {
		/* Resolution */
		PK_DBG("[adopt_CAMERA_HW_GetInfo2]Resolution\n");
		if (copy_to_user
		    ((void __user *)(pSensorGetInfo->pSensorResolution),
		     (void *)psensorResolution[0], sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT))) {
			PK_DBG("[CAMERA_HW][Resolution] ioctl copy to user failed\n");
			return -EFAULT;
		}
	} else {
		/* Resolution */
		PK_DBG("Sub cam , copy_to_user\n");
		if (copy_to_user
		    ((void __user *)(pSensorGetInfo->pSensorResolution),
		     (void *)psensorResolution[1], sizeof(MSDK_SENSOR_RESOLUTION_INFO_STRUCT))) {
			PK_DBG("[CAMERA_HW][Resolution] ioctl copy to user failed\n");
			return -EFAULT;
		}
	}

	return 0;
}				/* adopt_CAMERA_HW_GetInfo() */


/*******************************************************************************
* adopt_CAMERA_HW_Control
********************************************************************************/
inline static int adopt_CAMERA_HW_Control(void *pBuf)
{
	int ret = 0;
	ACDK_SENSOR_CONTROL_STRUCT *pSensorCtrl = (ACDK_SENSOR_CONTROL_STRUCT *) pBuf;
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT imageWindow;
	MSDK_SENSOR_CONFIG_STRUCT sensorConfigData;

	memset(&imageWindow, 0, sizeof(ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT));
	memset(&sensorConfigData, 0, sizeof(ACDK_SENSOR_CONFIG_STRUCT));

	if (NULL == pSensorCtrl) {
		PK_DBG("[CAMERA_HW] NULL arg.\n");
		return -EFAULT;
	}

	if (NULL == pSensorCtrl->pImageWindow || NULL == pSensorCtrl->pSensorConfigData) {
		PK_DBG("[CAMERA_HW] NULL arg.\n");
		return -EFAULT;
	}

	if (copy_from_user
	    ((void *)&imageWindow, (void *)pSensorCtrl->pImageWindow,
	     sizeof(ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT))) {
		PK_DBG("[CAMERA_HW][pFeatureData32] ioctl copy from user failed\n");
		return -EFAULT;
	}

	if (copy_from_user
	    ((void *)&sensorConfigData, (void *)pSensorCtrl->pSensorConfigData,
	     sizeof(ACDK_SENSOR_CONFIG_STRUCT))) {
		PK_DBG("[CAMERA_HW][pFeatureData32] ioctl copy from user failed\n");
		return -EFAULT;
	}
	/*  */
	if (g_pSensorFunc) {
		ret =
		    g_pSensorFunc->SensorControl(pSensorCtrl->InvokeCamera, pSensorCtrl->ScenarioId,
						 &imageWindow, &sensorConfigData);
	} else {
		PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
	}

	/*  */
	if (copy_to_user
	    ((void __user *)pSensorCtrl->pImageWindow, (void *)&imageWindow,
	     sizeof(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT))) {
		PK_DBG("[CAMERA_HW][imageWindow] ioctl copy to user failed\n");
		return -EFAULT;
	}
	/*  */
	if (copy_to_user
	    ((void __user *)pSensorCtrl->pSensorConfigData, (void *)&sensorConfigData,
	     sizeof(MSDK_SENSOR_CONFIG_STRUCT))) {
		PK_DBG("[CAMERA_HW][imageWindow] ioctl copy to user failed\n");
		return -EFAULT;
	}
	return ret;
}				/* adopt_CAMERA_HW_Control */

/*******************************************************************************
* adopt_CAMERA_HW_FeatureControl
********************************************************************************/
inline static int adopt_CAMERA_HW_FeatureControl(void *pBuf)
{
	ACDK_SENSOR_FEATURECONTROL_STRUCT *pFeatureCtrl =
	    (ACDK_SENSOR_FEATURECONTROL_STRUCT *) pBuf;
	unsigned int FeatureParaLen = 0;
	void *pFeaturePara = NULL;

	/* ACDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo = NULL; */
	ACDK_KD_SENSOR_SYNC_STRUCT *pSensorSyncInfo = NULL;
	/* char kernelGroupNamePtr[128]; */
	/* unsigned char *pUserGroupNamePtr = NULL; */
	signed int ret = 0;



	if (NULL == pFeatureCtrl) {
		PK_ERR(" NULL arg.\n");
		return -EFAULT;
	}

	if (SENSOR_FEATURE_SINGLE_FOCUS_MODE == pFeatureCtrl->FeatureId || SENSOR_FEATURE_CANCEL_AF == pFeatureCtrl->FeatureId || SENSOR_FEATURE_CONSTANT_AF == pFeatureCtrl->FeatureId || SENSOR_FEATURE_INFINITY_AF == pFeatureCtrl->FeatureId) {	/* YUV AF_init and AF_constent and AF_single has no params */
	} else {
		if (NULL == pFeatureCtrl->pFeaturePara || NULL == pFeatureCtrl->pFeatureParaLen) {
			PK_ERR(" NULL arg.\n");
			return -EFAULT;
		}
	}

	if (copy_from_user
	    ((void *)&FeatureParaLen, (void *)pFeatureCtrl->pFeatureParaLen,
	     sizeof(unsigned int))) {
		PK_ERR(" ioctl copy from user failed\n");
		return -EFAULT;
	}

	pFeaturePara = kmalloc(FeatureParaLen, GFP_KERNEL);
	if (NULL == pFeaturePara) {
		PK_ERR(" ioctl allocate mem failed\n");
		return -ENOMEM;
	}
	memset(pFeaturePara, 0x0, FeatureParaLen);

	/* copy from user */
	switch (pFeatureCtrl->FeatureId) {
	case SENSOR_FEATURE_SET_ESHUTTER:
	case SENSOR_FEATURE_SET_GAIN:
		/* reset the delay frame flag */
		spin_lock(&kdsensor_drv_lock);
		g_NewSensorExpGain.uSensorExpDelayFrame = 0xFF;
		g_NewSensorExpGain.uSensorGainDelayFrame = 0xFF;
		g_NewSensorExpGain.uISPGainDelayFrame = 0xFF;
		spin_unlock(&kdsensor_drv_lock);
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	case SENSOR_FEATURE_SET_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER:
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
	case SENSOR_FEATURE_SET_VIDEO_MODE:
	case SENSOR_FEATURE_SET_YUV_CMD:
	case SENSOR_FEATURE_MOVE_FOCUS_LENS:
	case SENSOR_FEATURE_SET_AF_WINDOW:
	case SENSOR_FEATURE_SET_CALIBRATION_DATA:
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
	case SENSOR_FEATURE_GET_EV_AWB_REF:
	case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
	case SENSOR_FEATURE_SET_AE_WINDOW:
	case SENSOR_FEATURE_GET_EXIF_INFO:
	case SENSOR_FEATURE_GET_DELAY_INFO:
	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	case SENSOR_FEATURE_SET_OB_LOCK:
	case SENSOR_FEATURE_SET_SENSOR_OTP_AWB_CMD:
	case SENSOR_FEATURE_SET_SENSOR_OTP_LSC_CMD:
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
	case SENSOR_FEATURE_SET_FRAMERATE:
	case SENSOR_FEATURE_SET_HDR:
	case SENSOR_FEATURE_GET_CROP_INFO:
	case SENSOR_FEATURE_GET_VC_INFO:
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
	case SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO:
	case SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO:	/* return TRUE:play flashlight */
	case SENSOR_FEATURE_SET_YUV_3A_CMD:	/* para: ACDK_SENSOR_3A_LOCK_ENUM */
	case SENSOR_FEATURE_SET_AWB_GAIN:
	case SENSOR_FEATURE_SET_MIN_MAX_FPS:
	case SENSOR_FEATURE_GET_PDAF_INFO:
	case SENSOR_FEATURE_GET_PDAF_DATA:
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
	case SENSOR_FEATURE_SET_ISO:
	case SENSOR_FEATURE_SET_PDAF:
		/*  */
		if (copy_from_user
		    ((void *)pFeaturePara, (void *)pFeatureCtrl->pFeaturePara, FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pFeaturePara] ioctl copy from user failed\n");
			return -EFAULT;
		}
		break;
	case SENSOR_FEATURE_SET_SENSOR_SYNC:	/* Update new sensor exposure time and gain to keep */
		if (copy_from_user
		    ((void *)pFeaturePara, (void *)pFeatureCtrl->pFeaturePara, FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pFeaturePara] ioctl copy from user failed\n");
			return -EFAULT;
		}
		/* keep the information to wait Vsync synchronize */
		pSensorSyncInfo = (ACDK_KD_SENSOR_SYNC_STRUCT *) pFeaturePara;
		spin_lock(&kdsensor_drv_lock);
		g_NewSensorExpGain.u2SensorNewExpTime = pSensorSyncInfo->u2SensorNewExpTime;
		g_NewSensorExpGain.u2SensorNewGain = pSensorSyncInfo->u2SensorNewGain;
		g_NewSensorExpGain.u2ISPNewRGain = pSensorSyncInfo->u2ISPNewRGain;
		g_NewSensorExpGain.u2ISPNewGrGain = pSensorSyncInfo->u2ISPNewGrGain;
		g_NewSensorExpGain.u2ISPNewGbGain = pSensorSyncInfo->u2ISPNewGbGain;
		g_NewSensorExpGain.u2ISPNewBGain = pSensorSyncInfo->u2ISPNewBGain;
		g_NewSensorExpGain.uSensorExpDelayFrame = pSensorSyncInfo->uSensorExpDelayFrame;
		g_NewSensorExpGain.uSensorGainDelayFrame = pSensorSyncInfo->uSensorGainDelayFrame;
		g_NewSensorExpGain.uISPGainDelayFrame = pSensorSyncInfo->uISPGainDelayFrame;
		/* AE smooth not change shutter to speed up */
		if ((0 == g_NewSensorExpGain.u2SensorNewExpTime)
		    || (0xFFFF == g_NewSensorExpGain.u2SensorNewExpTime)) {
			g_NewSensorExpGain.uSensorExpDelayFrame = 0xFF;
		}

		if (g_NewSensorExpGain.uSensorExpDelayFrame == 0) {
			FeatureParaLen = 2;
			g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
							    SENSOR_FEATURE_SET_ESHUTTER,
							    (unsigned char *)
							    &g_NewSensorExpGain.u2SensorNewExpTime,
							    (unsigned int *)&FeatureParaLen);
			g_NewSensorExpGain.uSensorExpDelayFrame = 0xFF;	/* disable */
		} else if (g_NewSensorExpGain.uSensorExpDelayFrame != 0xFF) {
			g_NewSensorExpGain.uSensorExpDelayFrame--;
		}
		/* exposure gain */
		if (g_NewSensorExpGain.uSensorGainDelayFrame == 0) {
			FeatureParaLen = 2;
			g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
							    SENSOR_FEATURE_SET_GAIN,
							    (unsigned char *)
							    &g_NewSensorExpGain.u2SensorNewGain,
							    (unsigned int *)&FeatureParaLen);
			g_NewSensorExpGain.uSensorGainDelayFrame = 0xFF;	/* disable */
		} else if (g_NewSensorExpGain.uSensorGainDelayFrame != 0xFF) {
			g_NewSensorExpGain.uSensorGainDelayFrame--;
		}
		/* if the delay frame is 0 or 0xFF, stop to count */
		if ((g_NewSensorExpGain.uISPGainDelayFrame != 0xFF)
		    && (g_NewSensorExpGain.uISPGainDelayFrame != 0)) {
			g_NewSensorExpGain.uISPGainDelayFrame--;
		}



		break;
#if 0
	case SENSOR_FEATURE_GET_GROUP_INFO:
		if (copy_from_user
		    ((void *)pFeaturePara, (void *)pFeatureCtrl->pFeaturePara, FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pFeaturePara] ioctl copy from user failed\n");
			return -EFAULT;
		}
		pSensorGroupInfo = (ACDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
		pUserGroupNamePtr = pSensorGroupInfo->GroupNamePtr;
		/*  */
		if (NULL == pUserGroupNamePtr) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW] NULL arg.\n");
			return -EFAULT;
		}
		pSensorGroupInfo->GroupNamePtr = kernelGroupNamePtr;
		break;
#endif
	case SENSOR_FEATURE_SET_ESHUTTER_GAIN:
		if (copy_from_user
		    ((void *)pFeaturePara, (void *)pFeatureCtrl->pFeaturePara, FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pFeaturePara] ioctl copy from user failed\n");
			return -EFAULT;
		}
		/* keep the information to wait Vsync synchronize */
		pSensorSyncInfo = (ACDK_KD_SENSOR_SYNC_STRUCT *) pFeaturePara;
		spin_lock(&kdsensor_drv_lock);
		g_NewSensorExpGain.u2SensorNewExpTime = pSensorSyncInfo->u2SensorNewExpTime;
		g_NewSensorExpGain.u2SensorNewGain = pSensorSyncInfo->u2SensorNewGain;
		spin_unlock(&kdsensor_drv_lock);
		kdSetExpGain(pFeatureCtrl->InvokeCamera);
		break;
		/* copy to user */
	case SENSOR_FEATURE_GET_RESOLUTION:
	case SENSOR_FEATURE_GET_PERIOD:
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_GET_CONFIG_PARA:
	case SENSOR_FEATURE_GET_GROUP_COUNT:
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*case SENSOR_FEATURE_GET_FACTORY_MODE: *//* Remove jungle-only cmdid */
		/* do nothing */
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_SINGLE_FOCUS_MODE:
	case SENSOR_FEATURE_CANCEL_AF:
	case SENSOR_FEATURE_CONSTANT_AF:
	default:
		break;
	}

	/*in case that some structure are passed from user sapce by ptr */
	switch (pFeatureCtrl->FeatureId) {
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		{
			MUINT32 *pValue = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;

			pValue = kmalloc(sizeof(MUINT32), GFP_KERNEL);
			if (pValue == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}

			memset(pValue, 0x0, sizeof(MUINT32));

			*(pFeaturePara_64 + 1) = (uintptr_t) pValue;
			PK_ERR("[CAMERA_HW] %p %p %p\n",
			       (void *)(uintptr_t) (*(pFeaturePara_64 + 1)),
			       (void *)pFeaturePara_64, (void *)(pValue));
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_ERR("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}
			*(pFeaturePara_64 + 1) = *pValue;
			kfree(pValue);
		}
		break;
	case SENSOR_FEATURE_GET_AE_STATUS:
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
	case SENSOR_FEATURE_GET_AF_STATUS:
	case SENSOR_FEATURE_GET_AWB_STATUS:
	case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
	case SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO:
	case SENSOR_FEATURE_GET_SENSOR_N3D_STREAM_TO_VSYNC_TIME:
	case SENSOR_FEATURE_GET_PERIOD:
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		{

			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}
		}
		break;
	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
	case SENSOR_FEATURE_AUTOTEST_CMD:
		{
			MUINT32 *pValue0 = NULL;
			MUINT32 *pValue1 = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;

			pValue0 = kmalloc(sizeof(MUINT32), GFP_KERNEL);
			pValue1 = kmalloc(sizeof(MUINT32), GFP_KERNEL);

			if (pValue0 == NULL || pValue1 == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pValue0);
				kfree(pValue1);
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pValue1, 0x0, sizeof(MUINT32));
			memset(pValue0, 0x0, sizeof(MUINT32));

			*(pFeaturePara_64) = (uintptr_t) pValue0;
			*(pFeaturePara_64 + 1) = (uintptr_t) pValue1;
			PK_DBG("[CAMERA_HW] %p %p %p\n",
			       (void *)(uintptr_t) (*(pFeaturePara_64 + 1)),
			       (void *)pFeaturePara_64, (void *)(pValue0));
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}
			*(pFeaturePara_64) = *pValue0;
			*(pFeaturePara_64 + 1) = *pValue1;
			kfree(pValue0);
			kfree(pValue1);
		}
		break;


	case SENSOR_FEATURE_GET_EV_AWB_REF:
		{
			SENSOR_AE_AWB_REF_STRUCT *pAeAwbRef = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;

			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64));

			pAeAwbRef = kmalloc(sizeof(SENSOR_AE_AWB_REF_STRUCT), GFP_KERNEL);
			if (pAeAwbRef == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pAeAwbRef, 0x0, sizeof(SENSOR_AE_AWB_REF_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pAeAwbRef;

			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}
			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pAeAwbRef,
			     sizeof(SENSOR_AE_AWB_REF_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pAeAwbRef);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		{
			SENSOR_WINSIZE_INFO_STRUCT *pCrop = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			pCrop = kmalloc(sizeof(SENSOR_WINSIZE_INFO_STRUCT), GFP_KERNEL);
			if (pCrop == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pCrop, 0x0, sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			*(pFeaturePara_64 + 1) = (uintptr_t) pCrop;

			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			/* PK_DBG("[CAMERA_HW]crop =%d\n",framerate); */

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pCrop,
			     sizeof(SENSOR_WINSIZE_INFO_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pCrop);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_VC_INFO:
		{
			SENSOR_VC_INFO_STRUCT *pVcInfo = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			pVcInfo = kmalloc(sizeof(SENSOR_VC_INFO_STRUCT), GFP_KERNEL);
			if (pVcInfo == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pVcInfo, 0x0, sizeof(SENSOR_VC_INFO_STRUCT));
			*(pFeaturePara_64 + 1) = (uintptr_t) pVcInfo;

			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pVcInfo,
			     sizeof(SENSOR_VC_INFO_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pVcInfo);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_PDAF_INFO:
		{

#if 1
			SET_PD_BLOCK_INFO_T *pPdInfo = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64 + 1));

			pPdInfo = kmalloc(sizeof(SET_PD_BLOCK_INFO_T), GFP_KERNEL);
			if (pPdInfo == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pPdInfo, 0x0, sizeof(SET_PD_BLOCK_INFO_T));
			*(pFeaturePara_64 + 1) = (uintptr_t) pPdInfo;
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pPdInfo,
			     sizeof(SET_PD_BLOCK_INFO_T))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pPdInfo);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;
#endif
		}
		break;

	case SENSOR_FEATURE_SET_AF_WINDOW:
	case SENSOR_FEATURE_SET_AE_WINDOW:
		{
			MUINT32 *pApWindows = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64));

			pApWindows = kmalloc(sizeof(MUINT32) * 6, GFP_KERNEL);
			if (pApWindows == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pApWindows, 0x0, sizeof(MUINT32) * 6);
			*(pFeaturePara_64) = (uintptr_t) pApWindows;

			if (copy_from_user
			    ((void *)pApWindows, (void *)usr_ptr, sizeof(MUINT32) * 6)) {
				PK_ERR("[CAMERA_HW]ERROR: copy from user fail\n");
			}
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_ERR("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}
			kfree(pApWindows);

			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_EXIF_INFO:
		{
			SENSOR_EXIF_INFO_STRUCT *pExif = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64));

			pExif = kmalloc(sizeof(SENSOR_EXIF_INFO_STRUCT), GFP_KERNEL);
			if (pExif == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pExif, 0x0, sizeof(SENSOR_EXIF_INFO_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pExif;
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pExif,
			     sizeof(SENSOR_EXIF_INFO_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pExif);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;


	case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
		{

			SENSOR_AE_AWB_CUR_STRUCT *pCurAEAWB = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64));

			pCurAEAWB = kmalloc(sizeof(SENSOR_AE_AWB_CUR_STRUCT), GFP_KERNEL);
			if (pCurAEAWB == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pCurAEAWB, 0x0, sizeof(SENSOR_AE_AWB_CUR_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pCurAEAWB;
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pCurAEAWB,
			     sizeof(SENSOR_AE_AWB_CUR_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pCurAEAWB);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;

	case SENSOR_FEATURE_GET_DELAY_INFO:
		{
			SENSOR_DELAY_INFO_STRUCT *pDelayInfo = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64));
			pDelayInfo = kmalloc(sizeof(SENSOR_DELAY_INFO_STRUCT), GFP_KERNEL);

			if (pDelayInfo == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pDelayInfo, 0x0, sizeof(SENSOR_DELAY_INFO_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pDelayInfo;
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pDelayInfo,
			     sizeof(SENSOR_DELAY_INFO_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pDelayInfo);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;
		}
		break;


	case SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO:
		{
			SENSOR_FLASHLIGHT_AE_INFO_STRUCT *pFlashInfo = NULL;
			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64));

			pFlashInfo = kmalloc(sizeof(SENSOR_FLASHLIGHT_AE_INFO_STRUCT), GFP_KERNEL);

			if (pFlashInfo == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pFlashInfo, 0x0, sizeof(SENSOR_FLASHLIGHT_AE_INFO_STRUCT));
			*(pFeaturePara_64) = (uintptr_t) pFlashInfo;
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pFlashInfo,
			     sizeof(SENSOR_FLASHLIGHT_AE_INFO_STRUCT))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pFlashInfo);
			*(pFeaturePara_64) = (uintptr_t) usr_ptr;

		}
		break;


	case SENSOR_FEATURE_GET_PDAF_DATA:
		{
			char *pPdaf_data = NULL;

			unsigned long long *pFeaturePara_64 = (unsigned long long *)pFeaturePara;
			void *usr_ptr = (void *)(uintptr_t) (*(pFeaturePara_64 + 1));
#if 1
			pPdaf_data = kmalloc(sizeof(char) * PDAF_DATA_SIZE, GFP_KERNEL);
			if (pPdaf_data == NULL) {
				PK_ERR(" ioctl allocate mem failed\n");
				kfree(pFeaturePara);
				return -ENOMEM;
			}
			memset(pPdaf_data, 0xff, sizeof(char) * PDAF_DATA_SIZE);

			if (pFeaturePara_64 != NULL)
				*(pFeaturePara_64 + 1) = (uintptr_t) pPdaf_data;
			if (g_pSensorFunc) {
				ret =
				    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
									pFeatureCtrl->FeatureId,
									(unsigned char *)
									pFeaturePara,
									(unsigned int *)
									&FeatureParaLen);
			} else {
				PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
			}

			if (copy_to_user
			    ((void __user *)usr_ptr, (void *)pPdaf_data,
			     (kal_uint32) (*(pFeaturePara_64 + 2)))) {
				PK_DBG("[CAMERA_HW]ERROR: copy_to_user fail\n");
			}
			kfree(pPdaf_data);
			*(pFeaturePara_64 + 1) = (uintptr_t) usr_ptr;

#endif
		}
		break;
	default:

		if (g_pSensorFunc) {
			ret =
			    g_pSensorFunc->SensorFeatureControl(pFeatureCtrl->InvokeCamera,
								pFeatureCtrl->FeatureId,
								(unsigned char *)pFeaturePara,
								(unsigned int *)&FeatureParaLen);
		} else {
			PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
		}

		break;
	}

	/* copy to user */
	switch (pFeatureCtrl->FeatureId) {
	case SENSOR_FEATURE_SET_ESHUTTER:
	case SENSOR_FEATURE_SET_GAIN:
	case SENSOR_FEATURE_SET_GAIN_AND_ESHUTTER:
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	case SENSOR_FEATURE_SET_REGISTER:
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_SET_ITEM_INFO:
		/* do nothing */
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_PDAF_DATA:
		break;
		/* copy to user */
	case SENSOR_FEATURE_GET_EV_AWB_REF:
	case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
	case SENSOR_FEATURE_GET_EXIF_INFO:
	case SENSOR_FEATURE_GET_DELAY_INFO:
	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
	case SENSOR_FEATURE_GET_RESOLUTION:
	case SENSOR_FEATURE_GET_PERIOD:
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	case SENSOR_FEATURE_GET_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_GET_CONFIG_PARA:
	case SENSOR_FEATURE_GET_GROUP_COUNT:
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
	case SENSOR_FEATURE_GET_AF_STATUS:
	case SENSOR_FEATURE_GET_AE_STATUS:
	case SENSOR_FEATURE_GET_AWB_STATUS:
	case SENSOR_FEATURE_GET_AF_INF:
	case SENSOR_FEATURE_GET_AF_MACRO:
	case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
	case SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO:	/* return TRUE:play flashlight */
	case SENSOR_FEATURE_SET_YUV_3A_CMD:	/* para: ACDK_SENSOR_3A_LOCK_ENUM */
	case SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO:
	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
	case SENSOR_FEATURE_SET_FRAMERATE:
	case SENSOR_FEATURE_SET_HDR:
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
	case SENSOR_FEATURE_GET_CROP_INFO:
	case SENSOR_FEATURE_GET_VC_INFO:
	case SENSOR_FEATURE_SET_MIN_MAX_FPS:
	case SENSOR_FEATURE_GET_PDAF_INFO:
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
	case SENSOR_FEATURE_SET_ISO:
	case SENSOR_FEATURE_SET_PDAF:
		/*  */
		if (copy_to_user
		    ((void __user *)pFeatureCtrl->pFeaturePara, (void *)pFeaturePara,
		     FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pSensorRegData] ioctl copy to user failed\n");
			return -EFAULT;
		}
		break;
#if 0
		/* copy from and to user */
	case SENSOR_FEATURE_GET_GROUP_INFO:
		/* copy 32 bytes */
		if (copy_to_user
		    ((void __user *)pUserGroupNamePtr, (void *)kernelGroupNamePtr,
		     sizeof(char) * 32)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pFeatureReturnPara32] ioctl copy to user failed\n");
			return -EFAULT;
		}
		pSensorGroupInfo->GroupNamePtr = pUserGroupNamePtr;
		if (copy_to_user
		    ((void __user *)pFeatureCtrl->pFeaturePara, (void *)pFeaturePara,
		     FeatureParaLen)) {
			kfree(pFeaturePara);
			PK_DBG("[CAMERA_HW][pFeatureReturnPara32] ioctl copy to user failed\n");
			return -EFAULT;
		}
		break;
#endif
	default:
		break;
	}

	kfree(pFeaturePara);
	if (copy_to_user
	    ((void __user *)pFeatureCtrl->pFeatureParaLen, (void *)&FeatureParaLen,
	     sizeof(unsigned int))) {
		PK_DBG("[CAMERA_HW][pFeatureParaLen] ioctl copy to user failed\n");
		return -EFAULT;
	}
	return ret;
}				/* adopt_CAMERA_HW_FeatureControl() */


/*******************************************************************************
* adopt_CAMERA_HW_Close
********************************************************************************/
inline static int adopt_CAMERA_HW_Close(void)
{
	/* if (atomic_read(&g_CamHWOpend) == 0) { */
	/* return 0; */
	/* } */
	/* else if(atomic_read(&g_CamHWOpend) == 1) { */
	if (g_pSensorFunc) {
		g_pSensorFunc->SensorClose();
	} else {
		PK_DBG("[CAMERA_HW]ERROR:NULL g_pSensorFunc\n");
	}
	/* power off sensor */
	/* Marked by Jessy Lee. Should close power in kd_MultiSensorClose function
	 * The following function will close all opened sensors.
	 */
	/* kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM*)g_invokeSocketIdx, g_invokeSensorNameStr, false, CAMERA_HW_DRVNAME1); */
	/* } */
	/* atomic_set(&g_CamHWOpend, 0); */

	atomic_set(&g_CamHWOpening, 0);

	/* reset the delay frame flag */
	spin_lock(&kdsensor_drv_lock);
	g_NewSensorExpGain.uSensorExpDelayFrame = 0xFF;
	g_NewSensorExpGain.uSensorGainDelayFrame = 0xFF;
	g_NewSensorExpGain.uISPGainDelayFrame = 0xFF;
	spin_unlock(&kdsensor_drv_lock);

	return 0;
}				/* adopt_CAMERA_HW_Close() */


/*******************************************************************************
* Common Clock Framework (CCF)
********************************************************************************/

#ifdef MTKCAM_USING_CCF

static inline void Get_ccf_clk(struct platform_device *pdev)
{
	if (pdev == NULL) {
		pr_err("[%s] pdev is null\n", __func__);
		return;
	}
	/* get all possible using clocks */
	g_camclk_camtg_sel = devm_clk_get(&pdev->dev, "TOP_CAMTG_SEL");
	BUG_ON(IS_ERR(g_camclk_camtg_sel));
	g_camclk_univpll_d26 = devm_clk_get(&pdev->dev, "TOP_UNIVPLL_D26");
	BUG_ON(IS_ERR(g_camclk_univpll_d26));
	g_camclk_univpll2_d2 = devm_clk_get(&pdev->dev, "TOP_UNIVPLL2_D2");
	BUG_ON(IS_ERR(g_camclk_univpll2_d2));

	return;
}

static inline void Check_ccf_clk(void)
{
	BUG_ON(IS_ERR(g_camclk_camtg_sel));
	BUG_ON(IS_ERR(g_camclk_univpll_d26));
	BUG_ON(IS_ERR(g_camclk_univpll2_d2));

	return;
}

#endif


inline static int kdSetSensorMclk(int *pBuf)
{
#ifndef CONFIG_FPGA_EARLY_PORTING

	int ret = 0;
	ACDK_SENSOR_MCLK_STRUCT *pSensorCtrl = (ACDK_SENSOR_MCLK_STRUCT *) pBuf;

	PK_DBG("[CAMERA SENSOR] kdSetSensorMclk on=%d, freq= %d\n", pSensorCtrl->on,
	       pSensorCtrl->freq);

#ifdef MTKCAM_USING_CCF
	PK_DBG("========= MTKCAM_USING_CCF =======\n");
	Check_ccf_clk();
	if (1 == pSensorCtrl->on) {
		clk_prepare_enable(g_camclk_camtg_sel);
		if (pSensorCtrl->freq == 1 /*CAM_PLL_48_GROUP */)
			clk_set_parent(g_camclk_camtg_sel, g_camclk_univpll_d26);
		else if (pSensorCtrl->freq == 2 /*CAM_PLL_52_GROUP */)
			clk_set_parent(g_camclk_camtg_sel, g_camclk_univpll2_d2);
	} else {
		clk_disable_unprepare(g_camclk_camtg_sel);
	}
	return ret;

#else
	PK_DBG("========= Old Clock =======\n");

	if (1 == pSensorCtrl->on) {
		enable_mux(MT_MUX_CAMTG, "CAMERA_SENSOR");
		clkmux_sel(MT_MUX_CAMTG, pSensorCtrl->freq, "CAMERA_SENSOR");
	} else {

		disable_mux(MT_MUX_CAMTG, "CAMERA_SENSOR");
	}
	return ret;
#endif				/* end of MTKCAM_USING_CCF */

#endif				/* end of CONFIG_FPGA_EARLY_PORTING */
}

inline static int kdSetSensorGpio(int *pBuf)
{
	int ret = 0;
#if 0
	unsigned int temp = 0;
	IMGSENSOR_GPIO_STRUCT *pSensorgpio = (IMGSENSOR_GPIO_STRUCT *) pBuf;

	PK_DBG("[CAMERA SENSOR] kdSetSensorGpio enable=%d, type=%d\n",
	       pSensorgpio->GpioEnable, pSensorgpio->SensroInterfaceType);
	/* Please use DCT to set correct GPIO setting (below message only for debug) */
	if (pSensorgpio->SensroInterfaceType == SENSORIF_PARALLEL) {
		if (pSensorgpio->GpioEnable == 1) {
			/* cmmclk & cmpclk */
			/* temp = *(mpGpioHwRegAddr + (0x680/4)); */
			/* mt65xx_reg_writel(((temp&0xFE07)|0x48), mpGpioHwRegAddr + (0x680/4)); */
			temp = SENSOR_RD32((void *)(GPIO_BASE + 0x870));
			SENSOR_WR32((void *)(GPIO_BASE + 0x870), (temp & 0x003F) | 0x1240);

			SENSOR_WR32((void *)(GPIO_BASE + 0x880), 0x1249);

			temp = SENSOR_RD32((void *)(GPIO_BASE + 0x890));
			SENSOR_WR32((void *)(GPIO_BASE + 0x890), (temp & 0xFFC0) | 0x9);


			if (pSensorgpio->SensorIndataformat == DATA_10BIT_FMT) {	/* 10bit data pin */
				temp = SENSOR_RD32((void *)(GPIO_BASE + 0x890));
				SENSOR_WR32((void *)(GPIO_BASE + 0x890), (temp & 0xFFF) | 0x1000);
				temp = SENSOR_RD32((void *)(GPIO_BASE + 0x8A0));
				SENSOR_WR32((void *)(GPIO_BASE + 0x8A0), (temp & 0xFFF8) | 0x1);
			}
		} else {
			temp = SENSOR_RD32((void *)(GPIO_BASE + 0x870));
			SENSOR_WR32((void *)(GPIO_BASE + 0x870), temp & 0x003F);

			SENSOR_WR32((void *)(GPIO_BASE + 0x880), 0x0);

			temp = SENSOR_RD32((void *)(GPIO_BASE + 0x890));
			SENSOR_WR32((void *)(GPIO_BASE + 0x890), temp & 0xFC0);

			temp = SENSOR_RD32((void *)(GPIO_BASE + 0x8A0));
			SENSOR_WR32((void *)(GPIO_BASE + 0x8A0), temp & 0xFFF8);
		}
		/* mode 1 for parallel pin  (GPIO195~214) */
		PK_DBG("GPIO195~GPIO204 0x%x, 0x%x\n", SENSOR_RD32((void *)(GPIO_BASE + 0x870)),
		       SENSOR_RD32((void *)(GPIO_BASE + 0x880)));
		PK_DBG("GPIO205~GPIO214 0x%x, 0x%x\n", SENSOR_RD32((void *)(GPIO_BASE + 0x890)),
		       SENSOR_RD32((void *)(GPIO_BASE + 0x8a0)));
	}
#endif
	return ret;
}


#ifdef CONFIG_COMPAT

#if 0
static int compat_get_acdk_sensor_getinfo_struct(COMPAT_ACDK_SENSOR_GETINFO_STRUCT __user *data32,
						 ACDK_SENSOR_GETINFO_STRUCT __user *data)
{
	MSDK_SCENARIO_ID_ENUM count;
	compat_uptr_t uptr;
	int err;

	err =
	    copy_from_user((void *)data->ScenarioId, (void *)data32->ScenarioId,
			   sizeof(MSDK_SCENARIO_ID_ENUM) * 2);
	PK_DBG_FUNC("ScenarioId[0]: %d, ScenarioId[1]: %d\n", data->ScenarioId[0],
		    data->ScenarioId[1]);

	err |=
	    copy_from_user((void *)data->pInfo, (void *)data32->pInfo, sizeof(compat_uptr_t) * 2);
	err |=
	    copy_from_user((void *)data->pInfo[0], (void *)data32->pInfo[0],
			   sizeof(ACDK_SENSOR_INFO_STRUCT));
	err |=
	    copy_from_user((void *)data->pInfo[1], (void *)data32->pInfo[1],
			   sizeof(ACDK_SENSOR_INFO_STRUCT));
	err |=
	    copy_from_user((void *)data->pConfig, (void *)data32->pConfig,
			   sizeof(compat_uptr_t) * 2);
	return err;
}

static int compat_put_acdk_sensor_getinfo_struct(COMPAT_ACDK_SENSOR_GETINFO_STRUCT __user *data32,
						 ACDK_SENSOR_GETINFO_STRUCT __user *data)
{
	MSDK_SCENARIO_ID_ENUM count;
	compat_uptr_t uptr;
	int err;

	err =
	    copy_to_user((void *)data->ScenarioId, (void *)data32->ScenarioId,
			 sizeof(MSDK_SCENARIO_ID_ENUM) * 2);
	err |= copy_to_user((void *)data->pInfo, (void *)data32->pInfo, sizeof(compat_uptr_t) * 2);
	err |=
	    copy_to_user((void *)data->pConfig, (void *)data32->pConfig, sizeof(compat_uptr_t) * 2);
	return err;
}
#endif

static int compat_get_imagesensor_getinfo_struct(COMPAT_IMAGESENSOR_GETINFO_STRUCT __user *data32,
						 IMAGESENSOR_GETINFO_STRUCT __user *data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->SensorId);
	err |= put_user(i, &data->SensorId);
	err |= get_user(p, &data32->pInfo);
	err |= put_user(compat_ptr(p), &data->pInfo);
	err |= get_user(p, &data32->pSensorResolution);
	err |= put_user(compat_ptr(p), &data->pSensorResolution);
	return err;
}

static int compat_put_imagesensor_getinfo_struct(COMPAT_IMAGESENSOR_GETINFO_STRUCT __user *data32,
						 IMAGESENSOR_GETINFO_STRUCT __user *data)
{
	/*compat_uptr_t p; */
	compat_uint_t i;
	int err;

	err = get_user(i, &data->SensorId);
	err |= put_user(i, &data32->SensorId);
	/* Assume pointer is not change */
#if 0
	err |= get_user(p, &data->pInfo);
	err |= put_user(p, &data32->pInfo);
	err |= get_user(p, &data->pSensorResolution);
	err |= put_user(p, &data32->pSensorResolution);
	*/
#endif
	    return err;
}

static int compat_get_acdk_sensor_featurecontrol_struct(COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT
							__user *data32,
							ACDK_SENSOR_FEATURECONTROL_STRUCT __user *
							data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->InvokeCamera);
	err |= put_user(i, &data->InvokeCamera);
	err |= get_user(i, &data32->FeatureId);
	err |= put_user(i, &data->FeatureId);
	err |= get_user(p, &data32->pFeaturePara);
	err |= put_user(compat_ptr(p), &data->pFeaturePara);
	err |= get_user(p, &data32->pFeatureParaLen);
	err |= put_user(compat_ptr(p), &data->pFeatureParaLen);
	return err;
}

static int compat_put_acdk_sensor_featurecontrol_struct(COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT
							__user *data32,
							ACDK_SENSOR_FEATURECONTROL_STRUCT __user *
							data)
{
	MUINT8 *p;
	MUINT32 *q;
	compat_uint_t i;
	int err;

	err = get_user(i, &data->InvokeCamera);
	err |= put_user(i, &data32->InvokeCamera);
	err |= get_user(i, &data->FeatureId);
	err |= put_user(i, &data32->FeatureId);
	/* Assume pointer is not change */

	err |= get_user(p, &data->pFeaturePara);
	err |= put_user(ptr_to_compat(p), &data32->pFeaturePara);
	err |= get_user(q, &data->pFeatureParaLen);
	err |= put_user(ptr_to_compat(q), &data32->pFeatureParaLen);

	return err;
}

static int compat_get_acdk_sensor_control_struct(COMPAT_ACDK_SENSOR_CONTROL_STRUCT __user *data32,
						 ACDK_SENSOR_CONTROL_STRUCT __user *data)
{
	compat_uptr_t p;
	compat_uint_t i;
	int err;

	err = get_user(i, &data32->InvokeCamera);
	err |= put_user(i, &data->InvokeCamera);
	err |= get_user(i, &data32->ScenarioId);
	err |= put_user(i, &data->ScenarioId);
	err |= get_user(p, &data32->pImageWindow);
	err |= put_user(compat_ptr(p), &data->pImageWindow);
	err |= get_user(p, &data32->pSensorConfigData);
	err |= put_user(compat_ptr(p), &data->pSensorConfigData);
	return err;
}

static int compat_put_acdk_sensor_control_struct(COMPAT_ACDK_SENSOR_CONTROL_STRUCT __user *data32,
						 ACDK_SENSOR_CONTROL_STRUCT __user *data)
{
	/* compat_uptr_t p; */
	compat_uint_t i;
	int err;

	err = get_user(i, &data->InvokeCamera);
	err |= put_user(i, &data32->InvokeCamera);
	err |= get_user(i, &data->ScenarioId);
	err |= put_user(i, &data32->ScenarioId);
	/* Assume pointer is not change */
#if 0
	err |= get_user(p, &data->pImageWindow);
	err |= put_user(p, &data32->pImageWindow);
	err |= get_user(p, &data->pSensorConfigData);
	err |= put_user(p, &data32->pSensorConfigData);
#endif
	return err;
}

static int compat_get_acdk_sensor_resolution_info_struct(COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT
							 __user *data32,
							 ACDK_SENSOR_PRESOLUTION_STRUCT __user *
							 data)
{
	int err;
	compat_uptr_t p;

	err = get_user(p, &data32->pResolution[0]);
	err |= put_user(compat_ptr(p), &data->pResolution[0]);
	err = get_user(p, &data32->pResolution[1]);
	err |= put_user(compat_ptr(p), &data->pResolution[1]);

	/* err = copy_from_user((void*)data, (void*)data32, sizeof(compat_uptr_t) * 2); */
	/* err = copy_from_user((void*)data[0], (void*)data32[0], sizeof(ACDK_SENSOR_RESOLUTION_INFO_STRUCT)); */
	/* err = copy_from_user((void*)data[1], (void*)data32[1], sizeof(ACDK_SENSOR_RESOLUTION_INFO_STRUCT)); */
	return err;
}

#if 0				/* Warning: unused function */
static int compat_put_acdk_sensor_resolution_info_struct(COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT
							 __user *data32,
							 ACDK_SENSOR_RESOLUTION_INFO_STRUCT __user *
							 data)
{
	int err;
	/* err = copy_to_user((void*)data, (void*)data32, sizeof(compat_uptr_t) * 2); */
	/* err = copy_to_user((void*)data[0], (void*)data32[0], sizeof(ACDK_SENSOR_RESOLUTION_INFO_STRUCT)); */
	/* err = copy_to_user((void*)data[1], (void*)data32[1], sizeof(ACDK_SENSOR_RESOLUTION_INFO_STRUCT)); */
	return err;
}
#endif


static long CAMERA_HW_Ioctl_Compat(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
		/*case COMPAT_KDIMGSENSORIOC_X_GETINFO:
		   {
		   PK_DBG_FUNC("[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_GETINFO E\n");

		   COMPAT_ACDK_SENSOR_GETINFO_STRUCT __user *data32;
		   ACDK_SENSOR_GETINFO_STRUCT __user *data;
		   int err;

		   data32 = compat_ptr(arg);
		   data = compat_alloc_user_space(sizeof(*data));
		   if (data == NULL)
		   return -EFAULT;

		   PK_DBG_FUNC("[CAMERA SENSOR] compat_get_acdk_sensor_getinfo_struct E\n");
		   err = compat_get_acdk_sensor_getinfo_struct(data32, data);
		   PK_DBG_FUNC("[CAMERA SENSOR] compat_get_acdk_sensor_getinfo_struct, err: %d L\n", err);

		   if (err)
		   return err;

		   PK_DBG_FUNC("[CAMERA SENSOR] unlocked_ioctl E\n");
		   ret = filp->f_op->unlocked_ioctl(filp, KDIMGSENSORIOC_X_GETINFO,(unsigned long)data);
		   PK_DBG_FUNC("[CAMERA SENSOR] unlocked_ioctl L\n");

		   PK_DBG_FUNC("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct E\n");
		   err = compat_put_acdk_sensor_getinfo_struct(data32, data);
		   PK_DBG_FUNC("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct, err: %d L\n", err);
		   if(err != 0)
		   PK_DBG_FUNC("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct failed\n");
		   return ret;
		   } */
	case COMPAT_KDIMGSENSORIOC_X_FEATURECONCTROL:
		{
			COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT __user *data32;
			ACDK_SENSOR_FEATURECONTROL_STRUCT __user *data;
			int err;

			/*PK_DBG("[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_FEATURECONCTROL\n");*/

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_acdk_sensor_featurecontrol_struct(data32, data);
			if (err)
				return err;

			ret =
			    filp->f_op->unlocked_ioctl(filp, KDIMGSENSORIOC_X_FEATURECONCTROL,
						       (unsigned long)data);
			err = compat_put_acdk_sensor_featurecontrol_struct(data32, data);


			if (err != 0)
				PK_ERR
				    ("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct failed\n");
			return ret;
		}
	case COMPAT_KDIMGSENSORIOC_X_CONTROL:
		{
			COMPAT_ACDK_SENSOR_CONTROL_STRUCT __user *data32;
			ACDK_SENSOR_CONTROL_STRUCT __user *data;
			int err;

			/*PK_DBG("[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_CONTROL\n");*/

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_acdk_sensor_control_struct(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp, KDIMGSENSORIOC_X_CONTROL,
						       (unsigned long)data);
			err = compat_put_acdk_sensor_control_struct(data32, data);

			if (err != 0)
				PK_ERR
				    ("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct failed\n");
			return ret;
		}
	case COMPAT_KDIMGSENSORIOC_X_GETINFO2:
		{
			COMPAT_IMAGESENSOR_GETINFO_STRUCT __user *data32;
			IMAGESENSOR_GETINFO_STRUCT __user *data;
			int err;

			/*PK_DBG("[CAMERA SENSOR] CAOMPAT_KDIMGSENSORIOC_X_GETINFO2\n");*/

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_imagesensor_getinfo_struct(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp, KDIMGSENSORIOC_X_GETINFO2,
						       (unsigned long)data);
			err = compat_put_imagesensor_getinfo_struct(data32, data);

			if (err != 0)
				PK_ERR
				    ("[CAMERA SENSOR] compat_put_acdk_sensor_getinfo_struct failed\n");
			return ret;
		}
	case COMPAT_KDIMGSENSORIOC_X_GETRESOLUTION2:
		{
			COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT __user *data32;
			ACDK_SENSOR_PRESOLUTION_STRUCT __user *data;
			int err;

			/*PK_DBG("[CAMERA SENSOR] KDIMGSENSORIOC_X_GETRESOLUTION\n");*/

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(data));
			if (data == NULL)
				return -EFAULT;
			/*PK_DBG("[CAMERA SENSOR] compat_get_acdk_sensor_resolution_info_struct\n");*/
			err = compat_get_acdk_sensor_resolution_info_struct(data32, data);
			if (err)
				return err;
			/*PK_DBG("[CAMERA SENSOR] unlocked_ioctl\n");*/
			ret =
			    filp->f_op->unlocked_ioctl(filp, KDIMGSENSORIOC_X_GETRESOLUTION2,
						       (unsigned long)data);

			/*PK_DBG("[CAMERA SENSOR] compat_get_acdk_sensor_resolution_info_struct\n");*/
			err = compat_get_acdk_sensor_resolution_info_struct(data32, data);
			if (err != 0)
				PK_ERR
				    ("[CAMERA SENSOR] compat_get_Acdk_sensor_resolution_info_struct failed\n");
			return ret;
		}
		/* Data in the following commands is not required to be converted to kernel 64-bit & user 32-bit */
	case COMPAT_KDIMGSENSORIOC_X_GETINFO:
	case KDIMGSENSORIOC_T_OPEN:
	case KDIMGSENSORIOC_T_CLOSE:
	case KDIMGSENSORIOC_T_CHECK_IS_ALIVE:
	case KDIMGSENSORIOC_X_SET_DRIVER:
	case KDIMGSENSORIOC_X_GET_SOCKET_POS:
	case KDIMGSENSORIOC_X_SET_I2CBUS:
	case KDIMGSENSORIOC_X_RELEASE_I2C_TRIGGER_LOCK:
	case KDIMGSENSORIOC_X_SET_SHUTTER_GAIN_WAIT_DONE:
	case KDIMGSENSORIOC_X_SET_MCLK_PLL:
	case KDIMGSENSORIOC_X_SET_CURRENT_SENSOR:
	case KDIMGSENSORIOC_X_SET_GPIO:
	case KDIMGSENSORIOC_X_GET_ISP_CLK:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);

	default:
		return -ENOIOCTLCMD;
	}
}


#endif

/*******************************************************************************
* CAMERA_HW_Ioctl
********************************************************************************/

static long CAMERA_HW_Ioctl(struct file *a_pstFile,
			    unsigned int a_u4Command, unsigned long a_u4Param)
{

	int i4RetValue = 0;
	void *pBuff = NULL;
	u32 *pIdx = NULL;

	mutex_lock(&kdCam_Mutex);


	if (_IOC_NONE == _IOC_DIR(a_u4Command)) {
	} else {
		pBuff = kmalloc(_IOC_SIZE(a_u4Command), GFP_KERNEL);

		if (NULL == pBuff) {
			PK_DBG("[CAMERA SENSOR] ioctl allocate mem failed\n");
			i4RetValue = -ENOMEM;
			goto CAMERA_HW_Ioctl_EXIT;
		}

		if (_IOC_WRITE & _IOC_DIR(a_u4Command)) {
			if (copy_from_user(pBuff, (void *)a_u4Param, _IOC_SIZE(a_u4Command))) {
				kfree(pBuff);
				PK_DBG("[CAMERA SENSOR] ioctl copy from user failed\n");
				i4RetValue = -EFAULT;
				goto CAMERA_HW_Ioctl_EXIT;
			}
		}
	}

	pIdx = (u32 *) pBuff;
	switch (a_u4Command) {

#if 0
	case KDIMGSENSORIOC_X_POWER_ON:
		i4RetValue =
		    kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM) *pIdx, true,
				    CAMERA_HW_DRVNAME);
		break;
	case KDIMGSENSORIOC_X_POWER_OFF:
		i4RetValue =
		    kdModulePowerOn((CAMERA_DUAL_CAMERA_SENSOR_ENUM) *pIdx, false,
				    CAMERA_HW_DRVNAME);
		break;
#endif
	case KDIMGSENSORIOC_X_SET_DRIVER:
		i4RetValue = kdSetDriver((unsigned int *)pBuff);
		break;
	case KDIMGSENSORIOC_T_OPEN:
		i4RetValue = adopt_CAMERA_HW_Open();
		break;
	case KDIMGSENSORIOC_X_GETINFO:
		i4RetValue = adopt_CAMERA_HW_GetInfo(pBuff);
		break;
	case KDIMGSENSORIOC_X_GETRESOLUTION2:
		i4RetValue = adopt_CAMERA_HW_GetResolution(pBuff);
		break;
	case KDIMGSENSORIOC_X_GETINFO2:
		i4RetValue = adopt_CAMERA_HW_GetInfo2(pBuff);
		break;
	case KDIMGSENSORIOC_X_FEATURECONCTROL:
		i4RetValue = adopt_CAMERA_HW_FeatureControl(pBuff);
		break;
	case KDIMGSENSORIOC_X_CONTROL:
		i4RetValue = adopt_CAMERA_HW_Control(pBuff);
		break;
	case KDIMGSENSORIOC_T_CLOSE:
		i4RetValue = adopt_CAMERA_HW_Close();
		break;
	case KDIMGSENSORIOC_T_CHECK_IS_ALIVE:
		i4RetValue = adopt_CAMERA_HW_CheckIsAlive();
		break;
	case KDIMGSENSORIOC_X_GET_SOCKET_POS:
		i4RetValue = kdGetSocketPostion((unsigned int *)pBuff);
		break;
	case KDIMGSENSORIOC_X_SET_I2CBUS:
		/* i4RetValue = kdSetI2CBusNum(*pIdx); */
		break;
	case KDIMGSENSORIOC_X_RELEASE_I2C_TRIGGER_LOCK:
		/* i4RetValue = kdReleaseI2CTriggerLock(); */
		break;

	case KDIMGSENSORIOC_X_SET_SHUTTER_GAIN_WAIT_DONE:
		i4RetValue = kdSensorSetExpGainWaitDone((int *)pBuff);
		break;

	case KDIMGSENSORIOC_X_SET_CURRENT_SENSOR:
		i4RetValue = kdSetCurrentSensorIdx(*pIdx);
		break;

	case KDIMGSENSORIOC_X_SET_MCLK_PLL:
		i4RetValue = kdSetSensorMclk(pBuff);
		break;

	case KDIMGSENSORIOC_X_SET_GPIO:
		i4RetValue = kdSetSensorGpio(pBuff);
		break;

	case KDIMGSENSORIOC_X_GET_ISP_CLK:
		/* PK_DBG("get_isp_clk=%d\n",get_isp_clk()); */
		/* *(unsigned int*)pBuff = get_isp_clk(); */
		break;

	default:
		PK_DBG("No such command\n");
		i4RetValue = -EPERM;
		break;

	}

	if (_IOC_READ & _IOC_DIR(a_u4Command)) {
		if (copy_to_user((void __user *)a_u4Param, pBuff, _IOC_SIZE(a_u4Command))) {
			kfree(pBuff);
			PK_DBG("[CAMERA SENSOR] ioctl copy to user failed\n");
			i4RetValue = -EFAULT;
			goto CAMERA_HW_Ioctl_EXIT;
		}
	}

	kfree(pBuff);
CAMERA_HW_Ioctl_EXIT:
	mutex_unlock(&kdCam_Mutex);
	return i4RetValue;
}

/*******************************************************************************
*
********************************************************************************/
/*  */
/* below is for linux driver system call */
/* change prefix or suffix only */
/*  */

/*******************************************************************************
 * RegisterCAMERA_HWCharDrv
 * #define
 * Main jobs:
 * 1.check for device-specified errors, device not ready.
 * 2.Initialize the device if it is opened for the first time.
 * 3.Update f_op pointer.
 * 4.Fill data structures into private_data
 * CAM_RESET
********************************************************************************/
static int CAMERA_HW_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	/* reset once in multi-open */
	if (atomic_read(&g_CamDrvOpenCnt) == 0) {
		/* default OFF state */
		/* MUST have */
		/* kdCISModulePowerOn(DUAL_CAMERA_MAIN_SENSOR,"",true,CAMERA_HW_DRVNAME1); */
		/* kdCISModulePowerOn(DUAL_CAMERA_SUB_SENSOR,"",true,CAMERA_HW_DRVNAME1); */

		/* kdCISModulePowerOn(DUAL_CAMERA_MAIN_SENSOR,"",false,CAMERA_HW_DRVNAME1); */
		/* kdCISModulePowerOn(DUAL_CAMERA_SUB_SENSOR,"",false,CAMERA_HW_DRVNAME1); */

	}
	/*  */
	atomic_inc(&g_CamDrvOpenCnt);
	return 0;
}

/*******************************************************************************
  * RegisterCAMERA_HWCharDrv
  * Main jobs:
  * 1.Deallocate anything that "open" allocated in private_data.
  * 2.Shut down the device on last close.
  * 3.Only called once on last time.
  * Q1 : Try release multiple times.
********************************************************************************/
static int CAMERA_HW_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	atomic_dec(&g_CamDrvOpenCnt);

	return 0;
}

static const struct file_operations g_stCAMERA_HW_fops = {
	.owner = THIS_MODULE,
	.open = CAMERA_HW_Open,
	.release = CAMERA_HW_Release,
	.unlocked_ioctl = CAMERA_HW_Ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = CAMERA_HW_Ioctl_Compat,
#endif

};

#define CAMERA_HW_DYNAMIC_ALLOCATE_DEVNO 1
/*******************************************************************************
* RegisterCAMERA_HWCharDrv
********************************************************************************/
inline static int RegisterCAMERA_HWCharDrv(void)
{
	struct device *sensor_device = NULL;

#if CAMERA_HW_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_CAMERA_HWdevno, 0, 1, CAMERA_HW_DRVNAME1)) {
		PK_DBG("[CAMERA SENSOR] Allocate device no failed\n");

		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_CAMERA_HWdevno, 1, CAMERA_HW_DRVNAME1)) {
		PK_DBG("[CAMERA SENSOR] Register device no failed\n");

		return -EAGAIN;
	}
#endif

	/* Allocate driver */
	g_pCAMERA_HW_CharDrv = cdev_alloc();

	if (NULL == g_pCAMERA_HW_CharDrv) {
		unregister_chrdev_region(g_CAMERA_HWdevno, 1);

		PK_DBG("[CAMERA SENSOR] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pCAMERA_HW_CharDrv, &g_stCAMERA_HW_fops);

	g_pCAMERA_HW_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pCAMERA_HW_CharDrv, g_CAMERA_HWdevno, 1)) {
		PK_DBG("[mt6516_IDP] Attatch file operation failed\n");

		unregister_chrdev_region(g_CAMERA_HWdevno, 1);

		return -EAGAIN;
	}

	sensor_class = class_create(THIS_MODULE, "sensordrv");
	if (IS_ERR(sensor_class)) {
		int ret = PTR_ERR(sensor_class);
		PK_DBG("Unable to create class, err = %d\n", ret);
		return ret;
	}
	sensor_device =
	    device_create(sensor_class, NULL, g_CAMERA_HWdevno, NULL, CAMERA_HW_DRVNAME1);

	return 0;
}

/*******************************************************************************
* UnregisterCAMERA_HWCharDrv
********************************************************************************/
inline static void UnregisterCAMERA_HWCharDrv(void)
{
	/* Release char driver */
	cdev_del(g_pCAMERA_HW_CharDrv);

	unregister_chrdev_region(g_CAMERA_HWdevno, 1);

	device_destroy(sensor_class, g_CAMERA_HWdevno);
	class_destroy(sensor_class);
}

/*******************************************************************************
 * i2c relative start
********************************************************************************/

/*******************************************************************************
* CAMERA_HW_i2c_probe
********************************************************************************/
static int CAMERA_HW_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;
	PK_DBG("[CAMERA_HW] Attach I2C\n");

	/* get sensor i2c client */
	spin_lock(&kdsensor_drv_lock);
	g_pstI2Cclient = client;
	/* set I2C clock rate */
	/* g_pstI2Cclient->timing = 100;//100k */

	spin_unlock(&kdsensor_drv_lock);

	/* Register char driver */
	i4RetValue = RegisterCAMERA_HWCharDrv();

	if (i4RetValue) {
		PK_ERR("[CAMERA_HW] register char device failed!\n");
		return i4RetValue;
	}
	/* spin_lock_init(&g_CamHWLock); */

	PK_DBG("[CAMERA_HW] Attached!!\n");
	return 0;
}


/*******************************************************************************
* CAMERA_HW_i2c_remove
********************************************************************************/
static int CAMERA_HW_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/*******************************************************************************
* i2c relative end
*****************************************************************************/



/*******************************************************************************
 * RegisterCAMERA_HWCharDrv
 * #define
 * Main jobs:
 * 1.check for device-specified errors, device not ready.
 * 2.Initialize the device if it is opened for the first time.
 * 3.Update f_op pointer.
 * 4.Fill data structures into private_data
 * CAM_RESET
********************************************************************************/
static int CAMERA_HW_Open2(struct inode *a_pstInode, struct file *a_pstFile)
{
	/*  */
	if (atomic_read(&g_CamDrvOpenCnt2) == 0) {
		/* kdCISModulePowerOn(DUAL_CAMERA_MAIN_2_SENSOR,"",true,CAMERA_HW_DRVNAME2); */

		/* kdCISModulePowerOn(DUAL_CAMERA_MAIN_2_SENSOR,"",false,CAMERA_HW_DRVNAME2); */
	}
	atomic_inc(&g_CamDrvOpenCnt2);
	return 0;
}

/*******************************************************************************
  * RegisterCAMERA_HWCharDrv
  * Main jobs:
  * 1.Deallocate anything that "open" allocated in private_data.
  * 2.Shut down the device on last close.
  * 3.Only called once on last time.
  * Q1 : Try release multiple times.
********************************************************************************/
static int CAMERA_HW_Release2(struct inode *a_pstInode, struct file *a_pstFile)
{
	atomic_dec(&g_CamDrvOpenCnt2);

	return 0;
}


static const struct file_operations g_stCAMERA_HW_fops0 = {
	.owner = THIS_MODULE,
	.open = CAMERA_HW_Open2,
	.release = CAMERA_HW_Release2,
	.unlocked_ioctl = CAMERA_HW_Ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = CAMERA_HW_Ioctl_Compat,
#endif

};



/*******************************************************************************
* RegisterCAMERA_HWCharDrv
********************************************************************************/
inline static int RegisterCAMERA_HWCharDrv2(void)
{
	struct device *sensor_device = NULL;
	UINT32 major;

#if CAMERA_HW_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_CAMERA_HWdevno2, 0, 1, CAMERA_HW_DRVNAME2)) {
		PK_DBG("[CAMERA SENSOR] Allocate device no failed\n");

		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_CAMERA_HWdevno2, 1, CAMERA_HW_DRVNAME2)) {
		PK_DBG("[CAMERA SENSOR] Register device no failed\n");

		return -EAGAIN;
	}
#endif

	major = MAJOR(g_CAMERA_HWdevno2);
	g_CAMERA_HWdevno2 = MKDEV(major, 0);

	/* Allocate driver */
	g_pCAMERA_HW_CharDrv2 = cdev_alloc();

	if (NULL == g_pCAMERA_HW_CharDrv2) {
		unregister_chrdev_region(g_CAMERA_HWdevno2, 1);

		PK_DBG("[CAMERA SENSOR] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pCAMERA_HW_CharDrv2, &g_stCAMERA_HW_fops0);

	g_pCAMERA_HW_CharDrv2->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pCAMERA_HW_CharDrv2, g_CAMERA_HWdevno2, 1)) {
		PK_DBG("[mt6516_IDP] Attatch file operation failed\n");

		unregister_chrdev_region(g_CAMERA_HWdevno2, 1);

		return -EAGAIN;
	}

	sensor2_class = class_create(THIS_MODULE, "sensordrv2");
	if (IS_ERR(sensor2_class)) {
		int ret = PTR_ERR(sensor2_class);
		PK_DBG("Unable to create class, err = %d\n", ret);
		return ret;
	}
	sensor_device =
	    device_create(sensor2_class, NULL, g_CAMERA_HWdevno2, NULL, CAMERA_HW_DRVNAME2);

	return 0;
}

inline static void UnregisterCAMERA_HWCharDrv2(void)
{
	/* Release char driver */
	cdev_del(g_pCAMERA_HW_CharDrv2);

	unregister_chrdev_region(g_CAMERA_HWdevno2, 1);

	device_destroy(sensor2_class, g_CAMERA_HWdevno2);
	class_destroy(sensor2_class);
}


/*******************************************************************************
* CAMERA_HW_i2c_probe
********************************************************************************/
static int CAMERA_HW_i2c_probe2(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;
	PK_DBG("[CAMERA_HW] Attach I2C0\n");

	spin_lock(&kdsensor_drv_lock);

	/* get sensor i2c client */
	g_pstI2Cclient2 = client;

	/* set I2C clock rate */
	/* g_pstI2Cclient2->timing = 100;//100k */
	spin_unlock(&kdsensor_drv_lock);

	/* Register char driver */
	i4RetValue = RegisterCAMERA_HWCharDrv2();

	if (i4RetValue) {
		PK_ERR("[CAMERA_HW] register char device failed!\n");
		return i4RetValue;
	}
	/* spin_lock_init(&g_CamHWLock); */

	PK_DBG("[CAMERA_HW] Attached!!\n");
	return 0;
}

/*******************************************************************************
* CAMERA_HW_i2c_remove
********************************************************************************/
static int CAMERA_HW_i2c_remove2(struct i2c_client *client)
{
	return 0;
}

/*******************************************************************************
* I2C Driver structure
********************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id CAMERA_HW_i2c_of_ids[] = {
	{.compatible = "mediatek,camera_hw_i2c",},
	{}
};
#endif
MODULE_DEVICE_TABLE(of, CAMERA_HW_i2c_of_ids);
struct i2c_driver CAMERA_HW_i2c_driver = {
	.probe = CAMERA_HW_i2c_probe,
	.remove = CAMERA_HW_i2c_remove,
	.driver = {
		   .name = CAMERA_HW_DRVNAME1,
		   .owner = THIS_MODULE,

#ifdef CONFIG_OF
		   .of_match_table = CAMERA_HW_i2c_of_ids,
#endif
		   },
	.id_table = CAMERA_HW_i2c_id,
};

#if 1
#ifdef CONFIG_OF
static const struct of_device_id CAMERA_HW2_i2c_driver_of_ids[] = {
	{.compatible = "mediatek,camera_hw2_i2c",},
	{}
};
#endif
#endif
MODULE_DEVICE_TABLE(of, CAMERA_HW2_i2c_driver_of_ids);
struct i2c_driver CAMERA_HW_i2c_driver2 = {
	.probe = CAMERA_HW_i2c_probe2,
	.remove = CAMERA_HW_i2c_remove2,
	.driver = {
		   .name = CAMERA_HW_DRVNAME2,
		   .owner = THIS_MODULE,
#if 1
#ifdef CONFIG_OF
		   .of_match_table = CAMERA_HW2_i2c_driver_of_ids,
#endif
#endif
		   },
	.id_table = CAMERA_HW_i2c_id2,
};

/*******************************************************************************
* Camera Regulator Power Control
* vol = VOL_1800 = 1800
********************************************************************************/
#ifdef MTKCAM_USING_PWRREG

BOOL CAMERA_Regulator_PowerOnOFF(struct regulator *pwrreg, BOOL IsOn, int vol)
{
	int ret = 0;
	int voltage_count = 0, high_bound_voltage = 0, low_bound_voltage = 0;

	PK_DBG("[%s] IsOn:%d , vol:%d\n", __func__, IsOn, vol);

	if (!pwrreg) {
		PK_ERR("[%s] camera power regulator is null\n", __func__);
		return FALSE;
	}

	if (IsOn) {		/* Power ON */
		voltage_count = regulator_count_voltages(pwrreg);
		if (voltage_count <= 0) {
			PK_ERR("[%s] camera power regulator fails to count, voltage_count = %d\n",
					__func__, voltage_count);
			return FALSE;
		}
		high_bound_voltage = regulator_list_voltage(pwrreg, voltage_count - 1);
		PK_DBG("[%s] high_bound_voltage = %d\n", __func__, high_bound_voltage);
		if (high_bound_voltage <= 0) {
			PK_ERR("[%s] camera power regulator fails to list, high_bound_voltage = %d\n",
				__func__, high_bound_voltage);
			return FALSE;
		}
		vol *= 1000;	/* regulator set is uV */
		ret = regulator_set_voltage(pwrreg, vol, high_bound_voltage);
		if (ret != 0) {
			PK_ERR("[%s] camera power regulator fails to set vol = %d , ret = 0x%x\n",
			       __func__, vol, ret);
			return FALSE;
		}

		ret = regulator_enable(pwrreg);
		if (ret != 0) {
			PK_ERR("[%s] camera power regulator fails to enabled, ret = 0x%x\n",
			       __func__, ret);
			return FALSE;
		}
	} else {		/* Power OFF */
		voltage_count = regulator_count_voltages(pwrreg);
		if (voltage_count <= 0) {
			pr_err
			    ("[%s] camera power regulator fails to count, voltage_count = %d\n",
			     __func__, voltage_count);
			return FALSE;
		}
		high_bound_voltage = regulator_list_voltage(pwrreg, voltage_count - 1);
		pr_err("[%s] high_bound_voltage = %d\n", __func__, high_bound_voltage);
		if (high_bound_voltage <= 0) {
			pr_err
			    ("[%s] camera power regulator fails to list, high_bound_voltage = %d\n",
			     __func__, high_bound_voltage);
			return FALSE;
		}
		low_bound_voltage = regulator_list_voltage(pwrreg, 0);
		pr_err("[%s] low_bound_voltage = %d\n", __func__, low_bound_voltage);
		if (low_bound_voltage <= 0) {
			pr_err
			    ("[%s] camera power regulator fails to list, low_bound_voltage = %d\n",
			     __func__, low_bound_voltage);
			return FALSE;
		}
		ret = regulator_set_voltage(pwrreg, low_bound_voltage, high_bound_voltage);
		if (ret != 0) {
			pr_err
			    ("[%s] camera power regulator fails to set high_vol= %d , low_vol = %d\n",
			     __func__, high_bound_voltage, low_bound_voltage);
			return FALSE;
		}

		ret = regulator_disable(pwrreg);
		if (ret != 0) {
			pr_err("[%s] camera power regulator fails to disable, ret = 0x%x\n",
			       __func__, ret);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CAMERA_Regulator_poweron(int PinIdx, int PwrType, int Voltage)
{
	/*BOOL ret; */
	struct regulator *pwr;

#if defined(CONFIG_ARCH_MTK_8173_SLOANE)
	/* sloane platform uses vcamd as a 3.3V regulator for gpio and hdmi
	 * domain.  Hence this regulator shouldn't be turned off and on.
	 * Since vcamd is used in tablets for camera, selectively returning
	 * from poweron function if platform is sloane
	 */
	return 1;
#endif

	PK_DBG("[%s] PinIndex=%d , PowerType = %d , Voltage = %d\n", __func__, PinIdx, PwrType,
		 Voltage);

	if ((PinIdx != 0) && (PinIdx != 1)) {
		PK_ERR("[%s] PinIndex=%d is invalid\n", __func__, PinIdx);
		return FALSE;
	}

	switch (PwrType) {
		/* FIX ME: MT65XX_POWER_LDO_ depends on PMIC version */
	case AVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcama : cam2_pwr_vcama);
		break;
	case DVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcamd : cam2_pwr_vcamd);
		break;
	case DOVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcamio : cam2_pwr_vcamio);
		break;
	case AFVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcamaf : cam2_pwr_vcamaf);
		break;
	default:
		PK_ERR("[%s] PwrType=%d is invalid\n", __func__, PwrType);
		pwr = NULL;
		break;
	}

	return CAMERA_Regulator_PowerOnOFF(pwr, 1, Voltage);
}
EXPORT_SYMBOL(CAMERA_Regulator_poweron);


BOOL CAMERA_Regulator_powerdown(int PinIdx, int PwrType)
{
	/*BOOL ret;*/
	struct regulator *pwr;

#if defined(CONFIG_ARCH_MTK_8173_SLOANE)
	/* sloane platform uses vcamd as a 3.3V regulator for gpio and hdmi
	 * domain.  Hence this regulator shouldn't be turned off and on.
	 * Since vcamd is used in tablets for camera, selectively returning
	 * from powerdown function if platform is sloane
	 */
	return 1;
#endif

	pr_debug("[%s] PinIndex=%d , PowerType = %d\n", __func__, PinIdx, PwrType);

	if ((PinIdx != 0) && (PinIdx != 1)) {
		pr_err("[%s] PinIndex=%d is invalid\n", __func__, PinIdx);
		return FALSE;
	}

	switch (PwrType) {
	case AVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcama : cam2_pwr_vcama);
		break;
	case DVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcamd : cam2_pwr_vcamd);
		break;
	case DOVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcamio : cam2_pwr_vcamio);
		break;
	case AFVDD:
		pwr = (PinIdx == 0 ? cam1_pwr_vcamaf : cam2_pwr_vcamaf);
		break;
	default:
		pr_err("[%s] PwrType=%d is invalid\n", __func__, PwrType);
		pwr = NULL;
		break;
	}

	return CAMERA_Regulator_PowerOnOFF(pwr, 0, 0);
}
EXPORT_SYMBOL(CAMERA_Regulator_powerdown);

#endif

#ifdef MTKCAM_USING_DTGPIO

enum CAMERA_GPIO {
	CAMERA_GPIO_RST,
	CAMERA_GPIO_PDN,
	CAMERA_GPIO_LDO,
	CAMERA_GPIO_COUNT,
};
struct Mtkcam_GPIO {
	const char *names;
	int num;
	bool bisvalid;
};
struct Mtkcam_GPIO cam_gpio[2][CAMERA_GPIO_COUNT] = {
	{			/*     {.names,         .num, .bisvalid} */
	 {"cam-1-gpio-rst", -1, 0},
	 {"cam-1-gpio-pdn", -1, 0},
	 {"cam-1-gpio-ldo", -1, 0}
	 },
	{
	 {"cam-2-gpio-rst", -1, 0},
	 {"cam-2-gpio-pdn", -1, 0},
	 {"cam-2-gpio-ldo", -1, 0}
	 }
};


int mtkcam_gpio_init(struct platform_device *pdev, struct Mtkcam_GPIO *gpio)
{
	int i = 0, ret = 0;
	struct device *dev = &pdev->dev;
	for (i = 0; i < CAMERA_GPIO_COUNT; i++) {
		gpio[i].num = of_get_named_gpio(dev->of_node, gpio[i].names, 0);

		if (gpio_is_valid(gpio[i].num)) {

			PK_DBG("cam_gpio[%d]: \"%s\"(%d)\n", i, gpio[i].names, gpio[i].num);

			ret = devm_gpio_request(dev, gpio[i].num, gpio[i].names);
			if (ret)
				PK_WARN("devm_gpio_request fail, ret=%d\n", ret);
		} else {
			PK_WARN("cam_gpio[%d]: \"%s\"(%d) is %s !!\n", i, gpio[i].names,
				gpio[i].num, (gpio[i].num == (-ENOENT) ? "non-used" : "invalid"));
		}
	}
	return ret;
}

int mtkcam_gpio_uninit(struct platform_device *pdev, struct Mtkcam_GPIO *gpio)
{
	int i = 0, ret = 0;
	struct device *dev = &pdev->dev;

	for (i = CAMERA_GPIO_COUNT - 1; i >= 0; i--) {
		if (gpio_is_valid(gpio[i].num)) {

			PK_DBG("cam_gpio[%d]: \"%s\"(%d)\n", i, gpio[i].names, gpio[i].num);

			devm_gpio_free(dev, gpio[i].num);
			if (ret)
				PK_WARN("devm_gpio_free fail %d\n", ret);
		} else {
			PK_WARN("cam_gpio[%d]: \"%s\"(%d) is %s !!\n", i, gpio[i].names,
				gpio[i].num, (gpio[i].num == (-ENOENT) ? "non-used" : "invalid"));
		}
	}
	return ret;
}
int mtkcam_gpio_set(int PinIdx, int PwrType, int Val)
{
	unsigned int gpiono = GPIO_CAMERA_INVALID;

	switch (PwrType) {
	case RST:
		gpiono = cam_gpio[PinIdx][CAMERA_GPIO_RST].num;
		break;
	case PDN:
		gpiono = cam_gpio[PinIdx][CAMERA_GPIO_PDN].num;
		break;
	case LDO:
		gpiono = cam_gpio[PinIdx][CAMERA_GPIO_LDO].num;
		break;
	default:
		PK_WARN("PwrType(%d) is invalid !!\n", PwrType);
		break;
	};

	PK_DBG("PinIdx(%d) PwrType(%d) #### set gpio#%d = %d\n", PinIdx, PwrType, gpiono, Val);

	if (gpio_is_valid(gpiono))
		return gpio_direction_output(gpiono, Val);
	else
		return gpiono;
}
EXPORT_SYMBOL(mtkcam_gpio_set);
#endif				/*  MTKCAM_USING_DTGPIO */

/*******************************************************************************
* CAMERA_HW_probe
********************************************************************************/

static int CAMERA_HW_probe(struct platform_device *pdev)
{
	int ret = 0;
#ifdef MTKCAM_USING_PWRREG	/* Camera Power Regulator Framework base on Device Tree */

	PK_DBG(" == MTKCAM_USING_PWRREG : Main ==\n");

	cam1_pwr_vcama = devm_regulator_get(&pdev->dev, "reg-vcama");
	if (IS_ERR(cam1_pwr_vcama)) {
		dev_err(&pdev->dev, "regulator vcama fail!");
		ret = PTR_ERR(cam1_pwr_vcama);
		printk(KERN_ERR "vcama  is  %ld!\n", PTR_ERR(cam1_pwr_vcama));
		return ret;
	}

	cam1_pwr_vcamd = devm_regulator_get(&pdev->dev, "reg-vcamd");
	if (IS_ERR(cam1_pwr_vcamd)) {
		dev_err(&pdev->dev, "regulator vcamd fail!");
		ret = PTR_ERR(cam1_pwr_vcamd);
		printk(KERN_ERR "vcamd  is  %ld!\n", PTR_ERR(cam1_pwr_vcamd));
		return ret;
	}

	if (hardware_id == 0) {
		printk("%s hardware_id = %d, line = %d\n", __func__, hardware_id, __LINE__);
		cam1_pwr_vcamio = devm_regulator_get(&pdev->dev, "reg-vcamio");
	} else {
		printk("%s hardware_id = %d, line = %d\n", __func__, hardware_id, __LINE__);
		cam1_pwr_vcamio = devm_regulator_get(&pdev->dev, "reg-vcamio-p2");
	}
	if (IS_ERR(cam1_pwr_vcamio)) {
		dev_err(&pdev->dev, "regulator vcamio fail!");
		ret = PTR_ERR(cam1_pwr_vcamio);
		printk(KERN_ERR "vcamio  is  %ld!\n", PTR_ERR(cam1_pwr_vcamio));
		return ret;
	}

	cam1_pwr_vcamaf = devm_regulator_get(&pdev->dev, "reg-vcamaf");
	if (IS_ERR(cam1_pwr_vcamaf)) {
		dev_err(&pdev->dev, "regulator vcamaf fail!");
		ret = PTR_ERR(cam1_pwr_vcamaf);
		printk(KERN_ERR "vcamaf  is  %ld!\n", PTR_ERR(cam1_pwr_vcamaf));
		return ret;
	}

	if (!cam1_pwr_vcama)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcama] is null\n", __func__);

	if (!cam1_pwr_vcamd)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcamd] is null\n", __func__);

	if (!cam1_pwr_vcamio)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcamio] is null\n", __func__);

	if (!cam1_pwr_vcamaf)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcamaf] is null\n", __func__);

#endif

#ifdef MTKCAM_USING_CCF
	PK_DBG(" == MTKCAM_USING_CCF ==\n");

	Get_ccf_clk(pdev);
#endif


#ifdef MTKCAM_USING_DTGPIO
	PK_DBG(" == MTKCAM_USING_DTGPIO : Main ==\n");
	mtkcam_gpio_init(pdev, cam_gpio[0]);
#endif

	return i2c_add_driver(&CAMERA_HW_i2c_driver);
}

static int CAMERA_HW_probe2(struct platform_device *pdev)
{
	int ret = 0;
#ifdef MTKCAM_USING_PWRREG	/* Camera Power Regulator Framework base on Device Tree */

	PK_DBG(" == MTKCAM_USING_PWRREG : Sub ==\n");

	cam2_pwr_vcama = devm_regulator_get(&pdev->dev, "reg-vcama");
	if (IS_ERR(cam2_pwr_vcama)) {
		dev_err(&pdev->dev, "regulator vcama fail!");
		ret = PTR_ERR(cam2_pwr_vcama);
		printk(KERN_ERR "vcama  is  %ld!\n", PTR_ERR(cam2_pwr_vcama));
		return ret;
	}

	cam2_pwr_vcamd = devm_regulator_get(&pdev->dev, "reg-vcamd");
	if (IS_ERR(cam2_pwr_vcamd)) {
		dev_err(&pdev->dev, "regulator vcamd fail!");
		ret = PTR_ERR(cam2_pwr_vcamd);
		printk(KERN_ERR "vcamd  is  %ld!\n", PTR_ERR(cam2_pwr_vcamd));
		return ret;
	}

	if (hardware_id == 0) {
		printk("%s hardware_id = %d, line = %d\n", __func__, hardware_id, __LINE__);
		cam2_pwr_vcamio = devm_regulator_get(&pdev->dev, "reg-vcamio");
	} else {
		printk("%s hardware_id = %d, line = %d\n", __func__, hardware_id, __LINE__);
		cam2_pwr_vcamio = devm_regulator_get(&pdev->dev, "reg-vcamio-p2");
	}

	if (IS_ERR(cam2_pwr_vcamio)) {
		dev_err(&pdev->dev, "regulator vcamio fail!");
		ret = PTR_ERR(cam2_pwr_vcamio);
		printk(KERN_ERR "vcamio  is  %ld!\n", PTR_ERR(cam2_pwr_vcamio));
		return ret;
	}

	cam2_pwr_vcamaf = devm_regulator_get(&pdev->dev, "reg-vcamaf");
	if (IS_ERR(cam2_pwr_vcamaf)) {
		dev_err(&pdev->dev, "regulator vcamaf fail!");
		ret = PTR_ERR(cam2_pwr_vcamaf);
		printk(KERN_ERR "vcamaf  is  %ld!\n", PTR_ERR(cam2_pwr_vcamaf));
		return ret;
	}

	if (!cam2_pwr_vcama)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcama] is null\n", __func__);

	if (!cam2_pwr_vcamd)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcamd] is null\n", __func__);

	if (!cam2_pwr_vcamio)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcamio] is null\n", __func__);

	if (!cam2_pwr_vcamaf)
		pr_err("[%s] Main camera pwr: get regulator fail,[vcamaf] is null\n", __func__);

#endif

#ifdef MTKCAM_USING_DTGPIO
	PK_DBG(" == MTKCAM_USING_DTGPIO : Sub ==\n");
	mtkcam_gpio_init(pdev, cam_gpio[1]);
#endif


	return i2c_add_driver(&CAMERA_HW_i2c_driver2);
}

/*******************************************************************************
* CAMERA_HW_remove()
********************************************************************************/
static int CAMERA_HW_remove(struct platform_device *pdev)
{
#ifdef MTKCAM_USING_DTGPIO
	mtkcam_gpio_uninit(pdev, cam_gpio[0]);
#endif

	i2c_del_driver(&CAMERA_HW_i2c_driver);
	return 0;
}

/*******************************************************************************
*CAMERA_HW_suspend()
********************************************************************************/
static int CAMERA_HW_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

/*******************************************************************************
  * CAMERA_HW_DumpReg_To_Proc()
  * Used to dump some critical sensor register
  ********************************************************************************/
static int CAMERA_HW_resume(struct platform_device *pdev)
{
	return 0;
}

/*******************************************************************************
* CAMERA_HW_remove()
********************************************************************************/
static int CAMERA_HW_remove2(struct platform_device *pdev)
{
#ifdef MTKCAM_USING_DTGPIO
	mtkcam_gpio_uninit(pdev, cam_gpio[1]);
#endif

	i2c_del_driver(&CAMERA_HW_i2c_driver2);
	return 0;
}

static int CAMERA_HW_suspend2(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

/*******************************************************************************
  * CAMERA_HW_DumpReg_To_Proc()
  * Used to dump some critical sensor register
  ********************************************************************************/
static int CAMERA_HW_resume2(struct platform_device *pdev)
{
	return 0;
}



/*******************************************************************************
* iWriteTriggerReg
********************************************************************************/
#if 0
int iWriteTriggerReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId)
{
	int i4RetValue = 0;
	int u4Index = 0;
	u8 *puDataInBytes = (u8 *) &a_u4Data;
	int retry = 3;
	char puSendCmd[6] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF), 0, 0, 0, 0 };



	SET_I2CBUS_FLAG(gI2CBusNum);

	if (gI2CBusNum == camera_i2c_bus_num1) {
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient->addr = (i2cId >> 1);
		spin_unlock(&kdsensor_drv_lock);
	} else {
		spin_lock(&kdsensor_drv_lock);
		g_pstI2Cclient2->addr = (i2cId >> 1);
		spin_unlock(&kdsensor_drv_lock);
	}


	if (a_u4Bytes > 2) {
		PK_DBG("[CAMERA SENSOR] exceed 2 bytes\n");
		return -1;
	}

	if (a_u4Data >> (a_u4Bytes << 3)) {
		PK_DBG("[CAMERA SENSOR] warning!! some data is not sent!!\n");
	}

	for (u4Index = 0; u4Index < a_u4Bytes; u4Index += 1) {
		puSendCmd[(u4Index + 2)] = puDataInBytes[(a_u4Bytes - u4Index - 1)];
	}

	do {
		if (gI2CBusNum == camera_i2c_bus_num1) {
			i4RetValue =
			    mt_i2c_master_send(g_pstI2Cclient, puSendCmd, (a_u4Bytes + 2),
					       I2C_3DCAMERA_FLAG);
			if (i4RetValue < 0) {
				PK_DBG("[CAMERA SENSOR][ERROR]set i2c bus 1 master fail\n");
				CLEAN_I2CBUS_FLAG(gI2CBusNum);
				break;
			}
		} else {
			i4RetValue =
			    mt_i2c_master_send(g_pstI2Cclient2, puSendCmd, (a_u4Bytes + 2),
					       I2C_3DCAMERA_FLAG);
			if (i4RetValue < 0) {
				PK_DBG("[CAMERA SENSOR][ERROR]set i2c bus 0 master fail\n");
				CLEAN_I2CBUS_FLAG(gI2CBusNum);
				break;
			}
		}

		if (i4RetValue != (a_u4Bytes + 2)) {
			PK_DBG("[CAMERA SENSOR] I2C send failed addr = 0x%x, data = 0x%x !!\n",
			       a_u2Addr, a_u4Data);
		} else {
			break;
		}
		uDELAY(50);
	} while ((retry--) > 0);

	return i4RetValue;
}
#endif
#if 0				/* linux-3.10 procfs API changed */
/*******************************************************************************
  * CAMERA_HW_Read_Main_Camera_Status()
  * Used to detect main camera status
  ********************************************************************************/
static int CAMERA_HW_Read_Main_Camera_Status(char *page, char **start, off_t off,
					     int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	p += sprintf(page, "%d\n", g_SensorExistStatus[0]);

	PK_DBG("g_SensorExistStatus[0] = %d\n", g_SensorExistStatus[0]);
	*start = page + off;
	len = p - page;
	if (len > off)
		len -= off;
	else
		len = 0;
	return len < count ? len : count;

}

/*******************************************************************************
  * CAMERA_HW_Read_Sub_Camera_Status()
  * Used to detect main camera status
  ********************************************************************************/
static int CAMERA_HW_Read_Sub_Camera_Status(char *page, char **start, off_t off,
					    int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;

	p += sprintf(page, "%d\n", g_SensorExistStatus[1]);

	PK_DBG(" g_SensorExistStatus[1] = %d\n", g_SensorExistStatus[1]);
	*start = page + off;
	len = p - page;
	if (len > off)
		len -= off;
	else
		len = 0;
	return len < count ? len : count;

}

/*******************************************************************************
  * CAMERA_HW_Read_3D_Camera_Status()
  * Used to detect main camera status
  ********************************************************************************/
static int CAMERA_HW_Read_3D_Camera_Status(char *page, char **start, off_t off,
					   int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;
	p += sprintf(page, "%d\n", g_SensorExistStatus[2]);

	PK_DBG("g_SensorExistStatus[2] = %d\n", g_SensorExistStatus[2]);
	*start = page + off;
	len = p - page;
	if (len > off)
		len -= off;
	else
		len = 0;
	return len < count ? len : count;

}
#endif


/*******************************************************************************
  * CAMERA_HW_DumpReg_To_Proc()
  * Used to dump some critical sensor register
  ********************************************************************************/
static ssize_t CAMERA_HW_DumpReg_To_Proc(struct file *file, char __user *data, size_t len,
					 loff_t *ppos)
{
	return 0;
}

static ssize_t CAMERA_HW_DumpReg_To_Proc2(struct file *file, char __user *data, size_t len,
					  loff_t *ppos)
{
	return 0;
}

static ssize_t CAMERA_HW_DumpReg_To_Proc3(struct file *file, char __user *data, size_t len,
					  loff_t *ppos)
{
	return 0;
}

/*******************************************************************************
  * CAMERA_HW_Reg_Debug()
  * Used for sensor register read/write by proc file
  ********************************************************************************/
static ssize_t CAMERA_HW_Reg_Debug(struct file *file, const char *buffer, size_t count,
				   loff_t *data)
{
	char regBuf[64] = { '\0' };
	u32 u4CopyBufSize = (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;
	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(regBuf, "init_switch=%d", &sensorReg.RegData) == 1) {
		main_sensor_init_setting_switch = sensorReg.RegData;
		PK_DBG("main_sensor_init_setting_switch = %d\n", main_sensor_init_setting_switch);
	} else if (sscanf(regBuf, "%x %x", &sensorReg.RegAddr, &sensorReg.RegData) == 2) {
		if (g_pSensorFunc != NULL) {
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_SENSOR,
							    SENSOR_FEATURE_SET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_SENSOR,
							    SENSOR_FEATURE_GET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			PK_DBG("write addr = 0x%08x, data = 0x%08x\n", sensorReg.RegAddr,
			       sensorReg.RegData);

			/* Save for Read Node */
			g_MainSensorReg.RegAddr = sensorReg.RegAddr;
			g_MainSensorReg.RegData = sensorReg.RegData;
		}
	} else if (sscanf(regBuf, "%x", &sensorReg.RegAddr) == 1) {
		if (g_pSensorFunc != NULL) {
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_SENSOR,
							    SENSOR_FEATURE_GET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			PK_DBG("read addr = 0x%08x, data = 0x%08x\n", sensorReg.RegAddr,
			       sensorReg.RegData);

			/* Save for Read Node */
			g_MainSensorReg.RegAddr = sensorReg.RegAddr;
			g_MainSensorReg.RegData = sensorReg.RegData;
		}
	}

	return count;
}


static ssize_t CAMERA_HW_Reg_Debug2(struct file *file, const char *buffer, size_t count,
				    loff_t *data)
{
	char regBuf[64] = { '\0' };
	u32 u4CopyBufSize = (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;
	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(regBuf, "%x %x", &sensorReg.RegAddr, &sensorReg.RegData) == 2) {
		if (g_pSensorFunc != NULL) {
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_2_SENSOR,
							    SENSOR_FEATURE_SET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_2_SENSOR,
							    SENSOR_FEATURE_GET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			PK_DBG("write addr = 0x%08x, data = 0x%08x\n", sensorReg.RegAddr,
			       sensorReg.RegData);
		}
	} else if (sscanf(regBuf, "%x", &sensorReg.RegAddr) == 1) {
		if (g_pSensorFunc != NULL) {
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_MAIN_2_SENSOR,
							    SENSOR_FEATURE_GET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			PK_DBG("read addr = 0x%08x, data = 0x%08x\n", sensorReg.RegAddr,
			       sensorReg.RegData);
		}
	}

	return count;
}

static ssize_t CAMERA_HW_Reg_Debug3(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char regBuf[64] = { '\0' };
	u32 u4CopyBufSize = (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;
	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(regBuf, "init_switch=%d", &sensorReg.RegData) == 1) {
		sub_sensor_init_setting_switch = sensorReg.RegData;
		PK_DBG("sub_sensor_init_setting_switch = %d\n", sub_sensor_init_setting_switch);
	} else if (sscanf(regBuf, "%x %x", &sensorReg.RegAddr, &sensorReg.RegData) == 2) {
		if (g_pSensorFunc != NULL) {
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_SUB_SENSOR,
							    SENSOR_FEATURE_SET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_SUB_SENSOR,
							    SENSOR_FEATURE_GET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			PK_DBG("write addr = 0x%08x, data = 0x%08x\n", sensorReg.RegAddr,
			       sensorReg.RegData);

			/* Save for Read Node */
			g_SubSensorReg.RegAddr = sensorReg.RegAddr;
			g_SubSensorReg.RegData = sensorReg.RegData;
		}
	} else if (sscanf(regBuf, "%x", &sensorReg.RegAddr) == 1) {
		if (g_pSensorFunc != NULL) {
			g_pSensorFunc->SensorFeatureControl(DUAL_CAMERA_SUB_SENSOR,
							    SENSOR_FEATURE_GET_REGISTER,
							    (MUINT8 *) &sensorReg, (MUINT32 *)
							    sizeof(MSDK_SENSOR_REG_INFO_STRUCT));
			PK_DBG("read addr = 0x%08x, data = 0x%08x\n", sensorReg.RegAddr,
			       sensorReg.RegData);

			/* Save for Read Node */
			g_SubSensorReg.RegAddr = sensorReg.RegAddr;
			g_SubSensorReg.RegData = sensorReg.RegData;
		}
	}

	return count;
}

/*=======================================================================
 * platform driver
 *=======================================================================*/
/* It seems we don't need to use device tree to register device cause we just use i2C part */
/* You can refer to CAMERA_HW_probe & CAMERA_HW_i2c_probe */
#ifdef CONFIG_OF

static const struct of_device_id CAMERA_HW_of_ids[] = {
	{.compatible = "mediatek,mt8173-camera_hw",},
	{}
};

MODULE_DEVICE_TABLE(of, CAMERA_HW_of_ids);

static const struct of_device_id CAMERA_HW2_of_ids[] = {
	{.compatible = "mediatek,mt8173-camera_hw2",},
	{}
};

MODULE_DEVICE_TABLE(of, CAMERA_HW2_of_ids);

#endif

static struct platform_driver g_stCAMERA_HW_Driver = {
	.probe = CAMERA_HW_probe,
	.remove = CAMERA_HW_remove,
	.suspend = CAMERA_HW_suspend,
	.resume = CAMERA_HW_resume,
	.driver = {
		   .name = "image_sensor",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = CAMERA_HW_of_ids,
#endif
		   }
};

static struct platform_driver g_stCAMERA_HW_Driver2 = {
	.probe = CAMERA_HW_probe2,
	.remove = CAMERA_HW_remove2,
	.suspend = CAMERA_HW_suspend2,
	.resume = CAMERA_HW_resume2,
	.driver = {
		   .name = "image_sensor_bus2",
		   .owner = THIS_MODULE,

#ifdef CONFIG_OF
		   .of_match_table = CAMERA_HW2_of_ids,
#endif

		   }
};

#ifndef CONFIG_OF
/* Old way , if no define CONFIG_OF */
static struct platform_device camerahw_platform_device = {
	.name = "image_sensor",
	.id = 0,
	.dev = {
		}
};

static struct platform_device camerahw2_platform_device = {
	.name = "image_sensor_bus2",
	.id = 0,
	.dev = {
		}
};
#endif

static struct file_operations fcamera_proc_fops = {
	.read = CAMERA_HW_DumpReg_To_Proc,
	.write = CAMERA_HW_Reg_Debug,
};

static struct file_operations fcamera_proc_fops2 = {
	.read = CAMERA_HW_DumpReg_To_Proc2,
	.write = CAMERA_HW_Reg_Debug2,
};

static struct file_operations fcamera_proc_fops3 = {
	.read = CAMERA_HW_DumpReg_To_Proc3,
	.write = CAMERA_HW_Reg_Debug3,
};

/*=======================================================================
  * CAMERA_HW_i2C_init()
  *=======================================================================*/
static int __init CAMERA_HW_i2C_init(void)
{

#if 0
	struct proc_dir_entry *prEntry;
#endif

#ifdef CAMERA_MODULE_INFO
	if (!camera_name_sysfs_inited) {
	int rc_local = 0;
	camera_name_kobj = kobject_create_and_add("camera_name", NULL);
	if (camera_name_kobj)
		rc_local = sysfs_create_group(camera_name_kobj, &camera_name_attrs_group);
		if (!camera_name_kobj || rc_local)
		PK_DBG("%s:%d failed create camera_name_kobj\n", __func__, __LINE__);
		camera_name_sysfs_inited = 1;
	}
#endif

	/* i2c_register_board_info(CAMERA_I2C_BUSNUM, &kd_camera_dev, 1); */
	PK_DBG("MTKCAM I2C Bus #%d , #%d\n", camera_i2c_bus_num1, camera_i2c_bus_num2);
	i2c_register_board_info(camera_i2c_bus_num1, &i2c_devs1, 1);
	i2c_register_board_info(camera_i2c_bus_num2, &i2c_devs2, 1);
	gI2CBusNum = camera_i2c_bus_num1;

	PK_DBG("+++\n");

#ifndef CONFIG_OF
	int ret;
	ret = platform_device_register(&camerahw_platform_device);
	if (ret) {
		PK_ERR("[camerahw_probe] platform_device_register fail\n");
		return ret;
	}

	ret = platform_device_register(&camerahw2_platform_device);
	if (ret) {
		PK_ERR("[camerahw2_probe] platform_device_register fail\n");
		return ret;
	}
#endif

	if (platform_driver_register(&g_stCAMERA_HW_Driver)) {
		PK_ERR("failed to register CAMERA_HW driver\n");
		return -ENODEV;
	}
	if (platform_driver_register(&g_stCAMERA_HW_Driver2)) {
		PK_ERR("failed to register CAMERA_HW2 driver\n");
		return -ENODEV;
	}
/* FIX-ME: linux-3.10 procfs API changed */
#if 1
	proc_create("driver/camsensor", 0, NULL, &fcamera_proc_fops);
	proc_create("driver/camsensor2", 0, NULL, &fcamera_proc_fops2);
	proc_create("driver/camsensor3", 0, NULL, &fcamera_proc_fops3);


#else
	/* Register proc file for main sensor register debug */
	prEntry = create_proc_entry("driver/camsensor", 0, NULL);
	if (prEntry) {
		prEntry->read_proc = CAMERA_HW_DumpReg_To_Proc;
		prEntry->write_proc = CAMERA_HW_Reg_Debug;
	} else {
		PK_ERR("add /proc/driver/camsensor entry fail\n");
	}

	/* Register proc file for main_2 sensor register debug */
	prEntry = create_proc_entry("driver/camsensor2", 0, NULL);
	if (prEntry) {
		prEntry->read_proc = CAMERA_HW_DumpReg_To_Proc;
		prEntry->write_proc = CAMERA_HW_Reg_Debug2;
	} else {
		PK_ERR("add /proc/driver/camsensor2 entry fail\n");
	}

	/* Register proc file for sub sensor register debug */
	prEntry = create_proc_entry("driver/camsensor3", 0, NULL);
	if (prEntry) {
		prEntry->read_proc = CAMERA_HW_DumpReg_To_Proc;
		prEntry->write_proc = CAMERA_HW_Reg_Debug3;
	} else {
		PK_ERR("add /proc/driver/camsensor entry fail\n");
	}

	/* Register proc file for main sensor register debug */
	prEntry = create_proc_entry("driver/maincam_status", 0, NULL);
	if (prEntry) {
		prEntry->read_proc = CAMERA_HW_Read_Main_Camera_Status;
		prEntry->write_proc = NULL;
	} else {
		PK_ERR("add /proc/driver/maincam_status entry fail\n");
	}

	/* Register proc file for sub sensor register debug */
	prEntry = create_proc_entry("driver/subcam_status", 0, NULL);
	if (prEntry) {
		prEntry->read_proc = CAMERA_HW_Read_Sub_Camera_Status;
		prEntry->write_proc = NULL;
	} else {
		PK_ERR("add /proc/driver/subcam_status entry fail\n");
	}

	/* Register proc file for 3d sensor register debug */
	prEntry = create_proc_entry("driver/3dcam_status", 0, NULL);
	if (prEntry) {
		prEntry->read_proc = CAMERA_HW_Read_3D_Camera_Status;
		prEntry->write_proc = NULL;
	} else {
		PK_ERR("add /proc/driver/3dcam_status entry fail\n");
	}

#endif
	atomic_set(&g_CamHWOpend, 0);
	atomic_set(&g_CamHWOpend2, 0);
	atomic_set(&g_CamDrvOpenCnt, 0);
	atomic_set(&g_CamDrvOpenCnt2, 0);
	atomic_set(&g_CamHWOpening, 0);


	PK_DBG("---\n");
	return 0;
}

/*=======================================================================
  * CAMERA_HW_i2C_exit()
  *=======================================================================*/
static void __exit CAMERA_HW_i2C_exit(void)
{
	platform_driver_unregister(&g_stCAMERA_HW_Driver);
	platform_driver_unregister(&g_stCAMERA_HW_Driver2);
}


EXPORT_SYMBOL(kdSetSensorSyncFlag);
EXPORT_SYMBOL(kdSensorSyncFunctionPtr);
EXPORT_SYMBOL(kdGetRawGainInfoPtr);

module_init(CAMERA_HW_i2C_init);
module_exit(CAMERA_HW_i2C_exit);

MODULE_DESCRIPTION("CAMERA_HW driver");
MODULE_AUTHOR("Jackie Su <jackie.su@Mediatek.com>");
MODULE_LICENSE("GPL");
