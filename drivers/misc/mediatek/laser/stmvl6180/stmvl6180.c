/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense Time-of-Flight sensor
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "../camera/kd_camera_hw.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "stmvl6180.h"


#ifndef ABS
#define ABS(x)              (((x) > 0) ? (x) : (-(x)))
#endif

#define VL6180_OFFSET_CALIB	0x02
#define VL6180_XTALK_CALIB		0x03

#define VL6180_MAGIC 'A'

/*#define VL6180_IOCTL_INIT 		_IO(VL6180_MAGIC, 0x01)
//#define VL6180_IOCTL_XTALKCALB		_IO(VL6180_MAGIC, 0x02)
//#define VL6180_IOCTL_OFFCALB		_IO(VL6180_MAGIC, 0x03)
//#define VL6180_IOCTL_STOP		_IO(VL6180_MAGIC, 0x05)
#define VL6180_IOCTL_GETOFFCALB	_IOR(VL6180_MAGIC, 0x02, unsigned int)
#define VL6180_IOCTL_GETXTALKCALB	_IOR(VL6180_MAGIC, 0x03, unsigned int)

//#define VL6180_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
//#define VL6180_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL6180_IOCTL_GETDATA 		_IOR(VL6180_MAGIC, 0x0a, unsigned int)
//#define VL6180_IOCTL_GETDATAS 		_IOR('p', 0x0b, VL6180x_RangeData_t)
*/

#define VL6180_IOCTL_INIT 			_IO(VL6180_MAGIC, 	0x01)
#define VL6180_IOCTL_GETOFFCALB	_IOR(VL6180_MAGIC, 	VL6180_OFFSET_CALIB, int)
#define VL6180_IOCTL_GETXTALKCALB	_IOR(VL6180_MAGIC, 	VL6180_XTALK_CALIB, int)
#define VL6180_IOCTL_SETOFFCALB	_IOW(VL6180_MAGIC, 	0x04, int)
#define VL6180_IOCTL_SETXTALKCALB	_IOW(VL6180_MAGIC, 	0x05, int)
#define VL6180_IOCTL_GETDATA 		_IOR(VL6180_MAGIC, 	0x0a, int)


#define LASER_I2C_BUSNUM 0

#define LASER_DRVNAME "stmvl6180"
#define I2C_SLAVE_ADDRESS        0x52
#define I2C_REGISTER_ID            0x52
#define PLATFORM_DRIVER_NAME "laser_actuator_stmvl6180"
#define LASER_DRIVER_CLASS_NAME "actuatordrv_stmvl6180"

static struct i2c_board_info __initdata kd_laser_dev={ I2C_BOARD_INFO(LASER_DRVNAME, I2C_REGISTER_ID)};

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO,    LASER_DRVNAME, "[%s] " format, __FUNCTION__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static spinlock_t g_Laser_SpinLock;

static struct i2c_client * g_pstLaser_I2Cclient = NULL;

static dev_t g_Laser_devno;
static struct cdev * g_pLaser_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int    g_s4Laser_Opened = 0;

static int g_Laser_OffsetCalib = 0xFFFFFFFF;
static int g_Laser_XTalkCalib = 0xFFFFFFFF;


/////////////////////////////////////////

stmvl6180x_dev vl6180x_dev = NULL;


 int s4VL6180_ReadRegByte(u16 addr, u8 *data)
{
    u8 pu_send_cmd[2] = {(u8)(addr >> 8) , (u8)(addr & 0xFF) };

    g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	
    if (i2c_master_send(g_pstLaser_I2Cclient, pu_send_cmd, 2) < 0)
    {
        //LOG_INF("I2C read 1 failed!! \n");
        return -1;
    }
	
    if (i2c_master_recv(g_pstLaser_I2Cclient, data , 1) < 0)
    {
        //LOG_INF("I2C read 2 failed!! \n");
        return -1;
    }

    //LOG_INF("I2C read addr 0x%x data 0x%x\n", addr,*data );
	
    return 0;
}

int s4VL6180_WriteRegByte(u16 addr, u8 data)
{
    u8 puSendCmd[3] = {	(u8)((addr >>  8)&0xFF), 
                                      	(u8)( addr       &0xFF),
                       			(u8)( data       &0xFF)};
	
    g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	
    if (i2c_master_send(g_pstLaser_I2Cclient, puSendCmd , 3) < 0)
    {
        //LOG_INF("I2C write failed!! \n");
        return -1;
    }
	
    //LOG_INF("I2C write addr 0x%x data 0x%x\n", addr, data );
	
    return 0;
}

 int s4VL6180_ReadRegWord(u16 addr, u16 *data)
{
    u8 pu_send_cmd[2] = {(u8)(addr >> 8) , (u8)(addr & 0xFF) };
    u16 vData;
	
    g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	
    if (i2c_master_send(g_pstLaser_I2Cclient, pu_send_cmd, 2) < 0)
    {
        //LOG_INF("I2C read 1 failed!! \n");
        return -1;
    }
	
    if (i2c_master_recv(g_pstLaser_I2Cclient, data , 2) < 0)
    {
        //LOG_INF("I2C read 2 failed!! \n");
        return -1;
    }
	
    vData = *data;	
	
    *data = ( (vData & 0xFF) << 8 ) + ( (vData >> 8) & 0xFF ) ;
		
    //LOG_INF("I2C read addr 0x%x data 0x%x\n", addr,*data );
	
    return 0;
}



int s4VL6180_WriteRegWord(u16 addr, u16 data)
{
    char puSendCmd[4] = {	(u8)((addr >>  8)&0xFF), 
                         			(u8)( addr       &0xFF),
                         			(u8)((data >>  8)&0xFF), 
		                         	(u8)( data       &0xFF)};
	
	
    g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	
    if (i2c_master_send(g_pstLaser_I2Cclient, puSendCmd , 4) < 0)
    {
        //LOG_INF("I2C write failed!! \n");
        return -1;
    }
	
    //LOG_INF("I2C write addr 0x%x data 0x%x\n", addr, data );
	
    return 0;
}

int s4VL6180_ReadRegDWord(u16 addr, u32 *data)
{
    u8 pu_send_cmd[2] = {(u8)(addr >> 8) , (u8)(addr & 0xFF) };
	
    u32 vData;
	
    g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	if (i2c_master_send(g_pstLaser_I2Cclient, pu_send_cmd, 2) < 0)
    {
        //LOG_INF("I2C read 1 failed!! \n");
        return -1;
    }
    if (i2c_master_recv(g_pstLaser_I2Cclient, data , 4) < 0)
    {
        //LOG_INF("I2C read 2 failed!! \n");
        return -1;
    }

    vData = *data;
	
    *data= (	(vData        &0xFF) <<24) +
		    (((vData>> 8)&0xFF) <<16) +
		    (((vData>>16)&0xFF) << 8) +
		    (((vData>>24)&0xFF) );
	
    //LOG_INF("I2C read addr 0x%x data 0x%x\n", addr,*data );

    return 0;
}

int s4VL6180_WriteRegDWord(u16 addr, u32 data)
{
    char puSendCmd[6] = {	(u8)((addr >>  8)&0xFF), 
			                     (u8)( addr       &0xFF),
                     	    		(u8)((data >> 24)&0xFF), 
                         			(u8)((data >> 16)&0xFF), 
                         			(u8)((data >>  8)&0xFF), 
                         			(u8)( data       &0xFF)};
	
    g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
	
    if (i2c_master_send(g_pstLaser_I2Cclient, puSendCmd , 6) < 0)
    {
        //LOG_INF("I2C write failed!! \n");
        return -1;
    }
	
    //LOG_INF("I2C write addr 0x%x data 0x%x\n", addr, data );
	
    return 0;
}


// TODO:  Wrong Case - If the range value has kept the same ,  we can't use it. In Calibration mode, the range value maybe keep the same.
int VL6180x_RangeCheckMeasurement(int RangeValue)
{
	static int RangeArray[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	static int ArrayCnt = 0;
	int Idx = 0;
	int CheckRes = 0;

	for( Idx = 0; Idx < 16; Idx++ )
	{
		if( RangeArray[Idx] == RangeValue )
		{
			CheckRes++;
		}
	}

	RangeArray[ArrayCnt] = RangeValue;
	ArrayCnt++;
	ArrayCnt &= 0xF;
		
	if( CheckRes == 16 )
	{
		RangeValue = 765;
		LOG_INF("Laser Check Data Failed \n");
	}

	return RangeValue;
}

extern void VL6180x_SetOffset(VL6180xDev_t dev, int8_t offset);

void VL6180x_SystemInit(int Scaling, int EnWAF, int CalibMode)
{
	//VL6180x_WaitDeviceBooted(vl6180x_dev);
	VL6180x_InitData(vl6180x_dev);
	
	VL6180x_Prepare(vl6180x_dev);
	VL6180x_UpscaleSetScaling(vl6180x_dev, Scaling);
	VL6180x_FilterSetState(vl6180x_dev, EnWAF); // turn on wrap around filter
	VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
	VL6180x_RangeClearInterrupt(vl6180x_dev);

	//Calibration Data
	if( CalibMode == VL6180_OFFSET_CALIB )
	{
		VL6180x_WrWord(vl6180x_dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, 0);
		VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);
		g_Laser_OffsetCalib = 0xFFFFFFFF;
	}
	else if( CalibMode == VL6180_XTALK_CALIB )
	{
		VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);
		g_Laser_XTalkCalib = 0xFFFFFFFF;
	}
	
	if( g_Laser_OffsetCalib != 0xFFFFFFFF )
	{
		VL6180x_SetOffset(vl6180x_dev, g_Laser_OffsetCalib);
		LOG_INF("VL6180 Set Offset Calibration: Set the offset value as %d\n", g_Laser_OffsetCalib);
	}
	if( g_Laser_XTalkCalib != 0xFFFFFFFF )
	{
		VL6180x_SetXTalkCompensationRate(vl6180x_dev, g_Laser_XTalkCalib);
		LOG_INF("VL6180 Set XTalk Calibration: Set the XTalk value as %d\n", g_Laser_XTalkCalib);
	}
	
	VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);		
}

int VL6180x_GetRangeValue(VL6180x_RangeData_t *Range/*int *RangeValue, int *SingleRate*/)
{
	int Result = 1;

	int ParamVal = 765;
	
	u8 u8status=0;

	#if 0

	VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status);
	
	if (u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY)
	{
		VL6180x_RangeGetMeasurement(vl6180x_dev, Range);
		VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
		
		ParamVal = Range->range_mm;
		
		/*if( ParamVal < 400 )
		{
			ParamVal = VL6180x_RangeCheckMeasurement(ParamVal);
		}*/
		//LOG_INF("Laser Range : %d / %d \n", Range->range_mm, Range->DMax); 
	}
	else
	{
		ParamVal = 765;
		LOG_INF("Laser Get Data Failed \n");
	}	

	#else

	int Count = 0;
	
	while(1)
	{
		VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status);

		if ( u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY )
		{
			VL6180x_RangeGetMeasurement(vl6180x_dev, Range);

			ParamVal = Range->range_mm;

			VL6180x_RangeClearInterrupt(vl6180x_dev);
			
			break;
		}

		if( Count > 10 )
		{
			ParamVal = 765;

			LOG_INF("Laser Get Data Failed \n");

			VL6180x_RangeClearInterrupt(vl6180x_dev);
			
			break;
		}

		msleep(3);
		
		Count++;
	}

	VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
	
	#endif
	

	if( ParamVal == 765 )
	{
		Result = 0;
	}
	
	return Result;
}

#define N_MEASURE_AVG   20
#define OFFSET_CALIB_TARGET_DISTANCE 100 // 100mm
#define XTALK_CALIB_TARGET_DISTANCE 400 // 400mm


////////////////////////////////////////////////////////////////
static long Laser_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    	long i4RetValue = 0;
	
		
	switch(a_u4Command)
	{
		case VL6180_IOCTL_INIT:	   /* init.  */
			if( g_s4Laser_Opened == 1 )
			{
				u8 Device_Model_ID = 0;
					
				s4VL6180_ReadRegByte(VL6180_MODEL_ID_REG, &Device_Model_ID);

				LOG_INF("STM VL6180 ID : %x\n", Device_Model_ID);
				
				if( Device_Model_ID != 0xb4 )
				{
					LOG_INF("Not found STM VL6180\n");

					return -1;
				}
				#if 0
				//VL6180x_WaitDeviceBooted(vl6180x_dev);
  				VL6180x_InitData(vl6180x_dev);
		
				
				VL6180x_Prepare(vl6180x_dev);
				VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
				VL6180x_FilterSetState(vl6180x_dev, 0/*1*/); // turn on wrap around filter
				VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
				VL6180x_RangeClearInterrupt(vl6180x_dev);
				            
				VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
				#endif
				
				/*spin_lock(&g_Laser_SpinLock);
        			g_s4Laser_Opened = 2;
        			spin_unlock(&g_Laser_SpinLock);*/
			}
			break;

		case VL6180_IOCTL_GETDATA:
			if( g_s4Laser_Opened == 1 )
			{
				#if 1
				VL6180x_SystemInit(3, 1, 0);
				#else
				//VL6180x_WaitDeviceBooted(vl6180x_dev);
  				VL6180x_InitData(vl6180x_dev);
		
				VL6180x_Prepare(vl6180x_dev);
				VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
				VL6180x_FilterSetState(vl6180x_dev, 1); // turn on wrap around filter
				VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
				VL6180x_RangeClearInterrupt(vl6180x_dev);
				            
				VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
				#endif

				spin_lock(&g_Laser_SpinLock);
        			g_s4Laser_Opened = 2;
        			spin_unlock(&g_Laser_SpinLock);
			}
			else	if( g_s4Laser_Opened == 2 )	
			{
				void __user *p_u4Param = (void __user *)a_u4Param;

				int ParamVal;
				
				VL6180x_RangeData_t Range;
				
				#if 1
				VL6180x_GetRangeValue(&Range);
				ParamVal = Range.range_mm;

				//LOG_INF("Laser Range : %d / %d \n", Range.range_mm, Range.DMax); 
				#else
				u8 u8status=0, u8ResultPos=0;
				VL6180x_RangeData_t Range;

				
				VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status);
				
				if (u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY)
				{
					VL6180x_RangeGetMeasurement(vl6180x_dev, &Range);
					VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
					ParamVal = Range.range_mm;

					if( ParamVal < 400 )
					{
						ParamVal = VL6180x_RangeCheckMeasurement(ParamVal);
					}
					
					//LOG_INF("Laser Range : %d / %d \n", Range.range_mm, Range.DMax); 
				}
				else
				{
					ParamVal = 765;
					LOG_INF("Laser Get Data Failed \n");
				}
				#endif
					
				if(copy_to_user(p_u4Param , &ParamVal , sizeof(unsigned int)))
 			      {
 			 	     LOG_INF("copy to user failed when getting motor information \n");
 			      }

				//spin_unlock(&g_Laser_SpinLock);  //system breakdown 
				
			}
			break;

		case VL6180_IOCTL_GETOFFCALB:  //Offset Calibrate place white target at 100mm from glass
			{
				void __user *p_u4Param = (void __user *)a_u4Param;
				
				int i = 0;
				int RangeSum =0,RangeAvg=0;
				int OffsetInt =0;
				VL6180x_RangeData_t Range;
				
				spin_lock(&g_Laser_SpinLock);
        			g_s4Laser_Opened = 3;
        			spin_unlock(&g_Laser_SpinLock);

  				VL6180x_SystemInit(3, 1, VL6180_OFFSET_CALIB);	
		
				for( i = 0; i < N_MEASURE_AVG; )
				{
					if( VL6180x_GetRangeValue(&Range) )	
					{
						LOG_INF("VL6180 Offset Calibration Orignal: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
						i++;
						RangeSum += Range.range_mm;
					}
				}

				RangeAvg = RangeSum / N_MEASURE_AVG;

				if( RangeAvg >= ( OFFSET_CALIB_TARGET_DISTANCE - 3 ) && RangeAvg <= ( OFFSET_CALIB_TARGET_DISTANCE + 3) )
				{
					LOG_INF("VL6180 Offset Calibration: Original offset is OK, finish offset calibration\n");
				}
				else
				{	
					LOG_INF("VL6180 Offset Calibration: Start offset calibration\n");

					VL6180x_SystemInit(1, 0, VL6180_OFFSET_CALIB);	

					RangeSum = 0;

					for( i = 0; i < N_MEASURE_AVG; )
					{
						if( VL6180x_GetRangeValue(&Range) )	
						{
							LOG_INF("VL6180 Offset Calibration: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
							i++;
							RangeSum += Range.range_mm;
						}
						msleep(50);
					}

					RangeAvg = RangeSum / N_MEASURE_AVG;

					LOG_INF("VL6180 Offset Calibration: Get the average Range as %d\n", RangeAvg);

					OffsetInt = OFFSET_CALIB_TARGET_DISTANCE - RangeAvg;
				
					LOG_INF("VL6180 Offset Calibration: Set the offset value(pre-scaling) as %d\n", OffsetInt);

					if( ABS(OffsetInt) > 127 ) // offset value : ~128 ~ 127
					{
						OffsetInt = 0xFFFFFFFF;
						i4RetValue = -1;
					}
					
					g_Laser_OffsetCalib = OffsetInt;
					//VL6180x_SetOffset(vl6180x_dev, OffsetInt);	
					VL6180x_SystemInit(3, 1, 0);

					LOG_INF("VL6180 Offset Calibration: End\n");
				}	
				
				spin_lock(&g_Laser_SpinLock);
        			g_s4Laser_Opened = 2;
        			spin_unlock(&g_Laser_SpinLock);

				if(copy_to_user(p_u4Param , &OffsetInt , sizeof(int)))
 			      {
 			 	     LOG_INF("copy to user failed when getting VL6180_IOCTL_GETOFFCALB \n");
 			      }
			}
			break;
		case VL6180_IOCTL_SETOFFCALB:
			{
				int OffsetInt =(int)a_u4Param;

				#if 1
				g_Laser_OffsetCalib = OffsetInt;

				LOG_INF("g_Laser_OffsetCalib : %d\n", g_Laser_OffsetCalib);
				
				#else
				VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP);
				
				VL6180x_RangeClearInterrupt(vl6180x_dev);
				
				VL6180x_SystemInit(3, 1, 0);

				LOG_INF("OffsetInt : %d\n", OffsetInt);

				VL6180x_SetOffset(vl6180x_dev,(int)OffsetInt);
				#endif
			}	
			break;

		case VL6180_IOCTL_GETXTALKCALB: // Place a dark target at 400mm ~ Lower reflectance target recommended, e.g. 17% gray card.
			{
				void __user *p_u4Param = (void __user *)a_u4Param;
				
				int i=0;
				int RangeSum =0;
				int RateSum = 0;
				int XtalkInt =0;

				VL6180x_RangeData_t Range;

				spin_lock(&g_Laser_SpinLock);
        			g_s4Laser_Opened = 3;
        			spin_unlock(&g_Laser_SpinLock);
				
				VL6180x_SystemInit(3, 1, VL6180_XTALK_CALIB);	

				for( i = 0; i < N_MEASURE_AVG; )
				{
					if( VL6180x_GetRangeValue(&Range) )	
					{
						RangeSum += Range.range_mm;
						RateSum += Range.signalRate_mcps;
						LOG_INF("VL6180 XTalk Calibration: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
						i++;
					}
				}
				
				XtalkInt = ( RateSum * ( N_MEASURE_AVG * XTALK_CALIB_TARGET_DISTANCE - RangeSum ) ) /( N_MEASURE_AVG * XTALK_CALIB_TARGET_DISTANCE * N_MEASURE_AVG) ;

				g_Laser_XTalkCalib = XtalkInt;

				// TODO:  If g_Laser_XTalkCalib is negative,  laser don't get range.
				if( g_Laser_XTalkCalib < 0 )
				{
					i4RetValue = -1;
					g_Laser_XTalkCalib = 0xFFFFFFFF;
				}
				
				//VL6180x_SetXTalkCompensationRate(vl6180x_dev, g_Laser_XTalkCalib);
				VL6180x_SystemInit(3, 1, 0);

				LOG_INF("VL6180 XTalk Calibration: End\n");
					
				spin_lock(&g_Laser_SpinLock);
        			g_s4Laser_Opened = 2;
        			spin_unlock(&g_Laser_SpinLock);

				if(copy_to_user(p_u4Param , &XtalkInt , sizeof(int)))
 			      {
 			 	     LOG_INF("copy to user failed when getting VL6180_IOCTL_GETOFFCALB \n");
 			      }
			}
			break;

		case VL6180_IOCTL_SETXTALKCALB:
			{
				int XtalkInt =(int)a_u4Param;

				#if 1
				g_Laser_XTalkCalib = XtalkInt;

				LOG_INF("g_Laser_XTalkCalib : %d\n", g_Laser_XTalkCalib);
				#else
				VL6180x_SystemInit(3, 1, 0);

				LOG_INF("XtalkInt : %d\n", XtalkInt);

				VL6180x_SetXTalkCompensationRate(vl6180x_dev, XtalkInt);
				#endif
			}
			break;
		default :
			LOG_INF("No CMD \n");
			i4RetValue = -EPERM;
			break;
	}

    return i4RetValue;
}

//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
// 3.Update f_op pointer.
// 4.Fill data structures into private_data
//CAM_RESET
static int Laser_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    LOG_INF("Start \n");

    if( g_s4Laser_Opened )
    {
        LOG_INF("The device is opened \n");
        return -EBUSY;
    }

    spin_lock(&g_Laser_SpinLock);
    g_s4Laser_Opened = 1;
    spin_unlock(&g_Laser_SpinLock);

    LOG_INF("End \n");

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int Laser_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    LOG_INF("Start \n");

    if (g_s4Laser_Opened)
    {
        LOG_INF("Free \n");

       spin_lock(&g_Laser_SpinLock);
       g_s4Laser_Opened = 0;
       spin_unlock(&g_Laser_SpinLock);
    }

    LOG_INF("End \n");

    return 0;
}

static const struct file_operations g_stLaser_fops =
{
    .owner = THIS_MODULE,
    .open = Laser_Open,
    .release = Laser_Release,
    .unlocked_ioctl = Laser_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = Laser_Ioctl,
#endif
};

inline static int Register_Laser_CharDrv(void)
{
    struct device* laser_device = NULL;

    LOG_INF("Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_Laser_devno, 0, 1,LASER_DRVNAME) )
    {
        LOG_INF("Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pLaser_CharDrv = cdev_alloc();

    if(NULL == g_pLaser_CharDrv)
    {
        unregister_chrdev_region(g_Laser_devno, 1);

        LOG_INF("Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pLaser_CharDrv, &g_stLaser_fops);

    g_pLaser_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pLaser_CharDrv, g_Laser_devno, 1))
    {
        LOG_INF("Attatch file operation failed\n");

        unregister_chrdev_region(g_Laser_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, LASER_DRIVER_CLASS_NAME);
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        LOG_INF("Unable to create class, err = %d\n", ret);
        return ret;
    }

    laser_device = device_create(actuator_class, NULL, g_Laser_devno, NULL, LASER_DRVNAME);

    if(NULL == laser_device)
    {
        return -EIO;
    }

    LOG_INF("End\n");
    return 0;
}

inline static void UnRegister_Laser_CharDrv(void)
{
    LOG_INF("Start\n");

    //Release char driver
    cdev_del(g_pLaser_CharDrv);

    unregister_chrdev_region(g_Laser_devno, 1);

    device_destroy(actuator_class, g_Laser_devno);

    class_destroy(actuator_class);

    LOG_INF("End\n");
}

//////////////////////////////////////////////////////////////////////

static int Laser_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int Laser_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id Laser_i2c_id[] = {{LASER_DRVNAME, 0},{}};
static struct i2c_driver Laser_i2c_driver = {
    .probe = Laser_i2c_probe,
    .remove = Laser_i2c_remove,
    .driver.name = LASER_DRVNAME,
    .id_table = Laser_i2c_id,
};

#if 0
static int Laser_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, LASER_DRVNAME);
    return 0;
}
#endif
static int Laser_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int Laser_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    LOG_INF("Start\n");

    /* Kirby: add new-style driver { */
    g_pstLaser_I2Cclient = client;

    g_pstLaser_I2Cclient->addr = I2C_SLAVE_ADDRESS;

    g_pstLaser_I2Cclient->addr = g_pstLaser_I2Cclient->addr >> 1;

    //Register char driver
    i4RetValue = Register_Laser_CharDrv();

    if(i4RetValue){

        LOG_INF(" register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_Laser_SpinLock);

    LOG_INF("Attached!! \n");

    return 0;
}

static int Laser_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&Laser_i2c_driver);
}

static int Laser_remove(struct platform_device *pdev)
{
    i2c_del_driver(&Laser_i2c_driver);
    return 0;
}

static int Laser_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int Laser_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stLaser_Driver = {
    .probe        = Laser_probe,
    .remove    = Laser_remove,
    .suspend    = Laser_suspend,
    .resume    = Laser_resume,
    .driver        = {
        .name    = PLATFORM_DRIVER_NAME,
        .owner    = THIS_MODULE,
    }
};
static struct platform_device g_stLaser_Device = {
    .name = PLATFORM_DRIVER_NAME,
    .id = 0,
    .dev = {}
};

static int __init STMVL6180_i2c_init(void)
{
    i2c_register_board_info(LASER_I2C_BUSNUM, &kd_laser_dev, 1);

    if(platform_device_register(&g_stLaser_Device)){
        LOG_INF("failed to register Laser driver\n");
        return -ENODEV;
    }

    if(platform_driver_register(&g_stLaser_Driver)){
        LOG_INF("Failed to register Laser driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit STMVL6180_i2c_exit(void)
{
    platform_driver_unregister(&g_stLaser_Driver);
}

module_init(STMVL6180_i2c_init);
module_exit(STMVL6180_i2c_exit);

MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_LICENSE("GPL");



