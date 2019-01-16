/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   PC Huang (MTK02204)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 01 04 2012 hao.wang
 * [ALPS00109603] getsensorid func check in
 * .
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#if !defined(MTK_NATIVE_3D_SUPPORT) //2D
  #define S5K4ECGX_MIPIYUV_MAIN_2_USE_HW_I2C
#else //MTK_NATIVE_3D_SUPPORT
  #define S5K4ECGX_MIPIYUV_MAIN_2_USE_HW_I2C

  #ifdef S5K4ECGX_MIPI_MAIN_2_USE_HW_I2C
    #define S5K4ECGX_MIPI_SUPPORT_N3D
  #endif
#endif

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
//Daniel
//#include <linux/slab.h>
#include <linux/xlog.h>
#include <asm/atomic.h>
//#include <asm/uaccess.h> //copy from user
//#include <linux/miscdevice.h>
//#include <mach/mt6516_pll.h>
#include <asm/io.h>
//#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"


#include "s5k4ecgxmipi_yuv_Sensor.h"
#include "s5k4ecgxmipi_yuv_Camera_Sensor_para.h"
#include "s5k4ecgxmipi_yuv_CameraCustomized.h"

#define PRE_CLK 80
#define S5K4ECGX_MIPI_DEBUG
#ifdef S5K4ECGX_MIPI_DEBUG
#define LOG_TAG "[4EC]"
#define SENSORDB(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG , LOG_TAG, fmt, ##arg)
//#define SENSORDB(fmt, arg...)  printk(KERN_ERR fmt, ##arg)
//#define SENSORDB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "4EC", fmt, ##arg)
#else
#define SENSORDB(x,...)
#endif
#define Sleep(ms) msleep(ms)



#define S5K4ECGX_MIPI_AF_Enable 1

#define S5K4ECGX_TEST_PATTERN_CHECKSUM (0x65fdd80e)

//Sophie Add: Need to check
static DEFINE_SPINLOCK(s5k4ecgx_mipi_drv_lock);

static DEFINE_SPINLOCK(s5k4ecgx_mipi_rw_lock);


static kal_uint32  S5K4ECGX_MIPI_sensor_pclk = 390 * 1000000;
MSDK_SCENARIO_ID_ENUM S5K4ECGXCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT S5K4ECGX_PreviewWin[5] = {0};

// global
static MSDK_SENSOR_CONFIG_STRUCT S5K4ECGXSensorConfigData;
//
kal_uint8 S5K4ECGX_MIPI_sensor_write_I2C_address = S5K4ECGX_WRITE_ID;
kal_uint8 S5K4ECGX_MIPI_sensor_read_I2C_address = S5K4ECGX_READ_ID;
kal_uint8 S5K4ECGX_MIPI_sensor_socket = DUAL_CAMERA_NONE_SENSOR;
struct S5K4ECGX_MIPI_sensor_struct S5K4ECGX_Driver;
unsigned int S5K4ECGX_Preview_enabled = 0;
unsigned int s5k4ec_cap_enable = 0;
UINT32 S5K4ECGX_MIPI_GetSensorID(UINT32 *sensorID);

/*
unsigned short S5K4ECGX_MIPI_DEFAULT_AE_TABLE[32] =
{
    0x101, 0x101, 0x101, 0x101, 0x101, 0x201, 0x102, 0x101,
    0x101, 0x202, 0x202, 0x101, 0x101, 0x802, 0x208, 0x101,
    0x201, 0xa04, 0x40a, 0x102, 0x402, 0x805, 0x508, 0x204,
    0x403, 0x505, 0x505, 0x304, 0x302, 0x303, 0x303, 0x203
};
*/

unsigned short S5K4ECGX_MIPI_DEFAULT_AE_TABLE[32] =
{
    0x101, 0x101, 0x101, 0x101, 0x101, 0x201, 0x102, 0x101,
    0x101, 0x202, 0x202, 0x101, 0x101, 0x502, 0x205, 0x101,
    0x201, 0x504, 0x405, 0x102, 0x402, 0x505, 0x505, 0x204,
    0x403, 0x505, 0x505, 0x304, 0x302, 0x303, 0x303, 0x203
};



extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId);
extern int iMultiWriteReg(u8 *pData, u16 lens, u16 i2cId);

static kal_uint16 S5K4ECGX_write_cmos_sensor_wID(kal_uint32 addr, kal_uint32 para, kal_uint32 id)
{
   char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};
   iWriteRegI2C(puSendCmd , 4,id);
   //SENSORDB("[Write]:addr=0x%x, para=0x%x, ID=0x%x\r\n", addr, para, id);
}

static kal_uint16 S5K4ECGX_read_cmos_sensor_wID(kal_uint32 addr, kal_uint32 id)
{
   kal_uint16 get_byte=0;
   char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
   iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,2,id);
   return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}

static kal_uint16 S5K4ECGX_read_cmos_sensor(kal_uint32 addr)
{
   kal_uint16 get_byte=0;
   char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
   iReadRegI2C(puSendCmd , 2, (u8*)&get_byte, 2, S5K4ECGX_MIPI_sensor_write_I2C_address);
   return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static kal_uint16 S5K4ECGX_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
   char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};
   iWriteRegI2C(puSendCmd , 4,S5K4ECGX_MIPI_sensor_write_I2C_address);
   //SENSORDB("[Write]:id=0x%x, addr=0x%x, para=0x%x\r\n", S5K4ECGX_MIPI_sensor_write_I2C_address, addr, para);
}


//#define USE_I2C_BURST_WRITE
#ifdef USE_I2C_BURST_WRITE
#define I2C_BUFFER_LEN 254 //MAX data to send by MT6572 i2c dma mode is 255 bytes
#define BLOCK_I2C_DATA_WRITE iBurstWriteReg
#else
#define I2C_BUFFER_LEN 8   // MT6572 i2s bus master fifo length is 8 bytes
#define BLOCK_I2C_DATA_WRITE iWriteRegI2C
#endif

// {addr, data} pair in para
// len is the total length of addr+data
// Using I2C multiple/burst write if the addr doesn't change
static kal_uint16 S5K4ECGX_table_write_cmos_sensor(kal_uint16* para, kal_uint32 len)
{
   char puSendCmd[I2C_BUFFER_LEN]; //at most 2 bytes address and 6 bytes data for multiple write. MTK i2c master has only 8bytes fifo.
   kal_uint32 tosend, IDX;
   kal_uint16 addr, addr_last, data;

   tosend = 0;
   IDX = 0;
   while(IDX < len)
   {
       addr = para[IDX];

       if (tosend == 0) // new (addr, data) to send
       {
           puSendCmd[tosend++] = (char)(addr >> 8);
           puSendCmd[tosend++] = (char)(addr & 0xFF);
           data = para[IDX+1];
           puSendCmd[tosend++] = (char)(data >> 8);
           puSendCmd[tosend++] = (char)(data & 0xFF);
           IDX += 2;
           addr_last = addr;
       }
       else if (addr == addr_last) // to multiple write the data to the same address
       {
           data = para[IDX+1];
           puSendCmd[tosend++] = (char)(data >> 8);
           puSendCmd[tosend++] = (char)(data & 0xFF);
           IDX += 2;
       }
       // to send out the data if the sen buffer is full or last data or to program to the different address.
       if (tosend == I2C_BUFFER_LEN || IDX == len || addr != addr_last)
       {
           BLOCK_I2C_DATA_WRITE(puSendCmd , tosend, S5K4ECGX_MIPI_sensor_write_I2C_address);
           tosend = 0;
       }
   }
   return 0;
}

int SEN_RUN_TEST_PATTERN = 0;

unsigned int
S5K4ECGX_MIPI_GetExposureTime(void)
{
    unsigned int interval = 0;

    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E, 0x2C28);
    interval  = S5K4ECGX_read_cmos_sensor(0x0F12);
    interval += S5K4ECGX_read_cmos_sensor(0x0F12) << 16 ;
    interval /= 400; //ms
    //interval = 40;
    if (interval < 30)
    {
       interval = 30;
    }


    if (interval > 500)
    {
       interval = 500;
    }

    SENSORDB("[4EC] FrameTime = %d ms\n", interval);
    return interval;
}



UINT32 S5K4ECGX_MIPI_SetTestPatternMode(kal_bool bEnable)
{
    //SENSORDB("[S5K4ECGX_MIPI_SetTestPatternMode] Fail: Not Support. Test pattern enable:%d\n", bEnable);
    //output color bar
    S5K4ECGX_write_cmos_sensor(0x0028,0xd000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x3100);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0002);
    S5K4ECGX_write_cmos_sensor(0xFCFC,0xd000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    return ERROR_NONE;
}


/***********************************************************
**    AF Control Start
***********************************************************/
void S5K4ECGX_MIPI_set_scene_mode(UINT16 para);
BOOL S5K4ECGX_MIPI_set_param_wb(UINT16 para);



static void
S5K4ECGX_MIPI_AE_Lock(void)
{
    //SENSORDB("[4EC]AE_LOCK\n");
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x2C5E);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);
    return;
}


static void
S5K4ECGX_MIPI_AE_UnLock(void)
{
    //SENSORDB("[4EC]AE_UnLOCK\n");
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x2C5E);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);
    return;
}

static void
S5K4ECGX_MIPI_AE_On(void)
{

    kal_uint16 Status_3A=0;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
    Status_3A = S5K4ECGX_read_cmos_sensor(0x0F12);


    // enable AE
    Status_3A = Status_3A | 0x00000002;
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);//
    S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
    return;
}

static void
S5K4ECGX_MIPI_AE_Off(void)
{
    kal_uint16 Status_3A=0;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
    Status_3A = S5K4ECGX_read_cmos_sensor(0x0F12);


    // disable AE
    Status_3A = Status_3A & ~(0x00000002);
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);//
    S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
    return;
}



static void
S5K4ECGX_MIPI_AWB_Lock(void)
{
    //SENSORDB("[4EC]AWB_LOCK\n");
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x2C66);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);
    return;
}


static void
S5K4ECGX_MIPI_AWB_UnLock(void)
{
#if 0
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x2C66);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);
    if (!((SCENE_MODE_OFF == S5K4ECGX_Driver.sceneMode) || (SCENE_MODE_NORMAL ==
    S5K4ECGX_Driver.sceneMode) || (SCENE_MODE_HDR == S5K4ECGX_Driver.sceneMode)))
    {
       S5K4ECGX_MIPI_set_scene_mode(S5K4ECGX_Driver.sceneMode);
    }
    else
    {
        if (!((AWB_MODE_OFF == S5K4ECYX_MIPICurrentStatus.iWB) ||
            (AWB_MODE_AUTO == S5K4ECYX_MIPICurrentStatus.iWB)))
        {
            S5K4ECGX_MIPI_set_param_wb(S5K4ECYX_MIPICurrentStatus.iWB);
        }
    }
    if (!((AWB_MODE_OFF == S5K4ECYX_MIPICurrentStatus.iWB) ||
        (AWB_MODE_AUTO == S5K4ECYX_MIPICurrentStatus.iWB)))
    {
        S5K4ECGX_MIPI_set_param_wb(S5K4ECYX_MIPICurrentStatus.iWB);
    }
#endif
    if (((AWB_MODE_OFF == S5K4ECYX_MIPICurrentStatus.iWB) ||
        (AWB_MODE_AUTO == S5K4ECYX_MIPICurrentStatus.iWB)))
    {
        S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002A,0x2C66);
        S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);
    }

    return;
}

static void
S5K4ECGX_MIPI_AWB_On(void)
{

    kal_uint16 Status_3A=0;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
    Status_3A = S5K4ECGX_read_cmos_sensor(0x0F12);


    // enable AWB
    Status_3A = Status_3A | 0x00000008;
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);//
    S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
    return;
}

static void
S5K4ECGX_MIPI_AWB_Off(void)
{
    kal_uint16 Status_3A=0;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
    Status_3A = S5K4ECGX_read_cmos_sensor(0x0F12);


    // disable AWB
    Status_3A = Status_3A & ~(0x00000008);
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);//
    S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
    return;
}



static void
S5K4ECGX_MIPI_AE_ExpCurveGain_Config(unsigned int SceneMode)
{

    switch (SceneMode)
    {
       case SCENE_MODE_NIGHTSCENE:
           //AE Table
           S5K4ECGX_write_cmos_sensor(0x002A, 0x0638);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_0_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x1478);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_1_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A0A);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_2_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x6810);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_3_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x6810);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_4_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0xD020);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_5_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0428);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_6_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A80);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_7_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A80);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_8_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A80);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_9_
           break;
       case SCENE_MODE_SPORTS:
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x0638);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_0_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A3C);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_1_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0D05);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_2_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_3_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_4_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_5_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_6_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_7_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_8_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_9_
           break;
       default:
           S5K4ECGX_write_cmos_sensor(0x002A, 0x0638);   //0638
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //0001
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000   //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_0_
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A3C);   //0A3C
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0D05);   //0D05
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);   //3408
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);   //3408
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x6810);   //6810
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x8214);   //8214
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0xC350);   //C350
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0xD4C0);   //C350
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //0000
           S5K4ECGX_write_cmos_sensor(0x0F12, 0xD4C0);   //C350
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //0000
           break;
    }
    return;
}




void S5K4ECGX_MIPI_AE_Rollback_Weight_Table(void)
{
    unsigned short *pae_table = 0;
    unsigned int i;

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    if (S5K4ECGX_Driver.userAskAeLock)
    {
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
        return;
    }
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    //SENSORDB("[4EC]AE_Rollback_Weight_Table\n");

    pae_table = (unsigned short *) (&S5K4ECGX_MIPI_DEFAULT_AE_TABLE[0]);

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x1492);//0x2E08
    for (i = 0; i < 32; i++)
    {
       S5K4ECGX_write_cmos_sensor(0x0F12, pae_table[i]);
    }

    S5K4ECGX_write_cmos_sensor(0x0028 ,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A ,0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0001);  //REG_TC_GP_PrevConfigChanged

    SENSORDB("[4EC]Rollback_Weight_Table\n");

    S5K4ECGX_write_cmos_sensor(0x0028 ,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A ,0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0001);  //REG_TC_GP_PrevConfigChanged

    return;
}


static void
S5K4ECGX_MIPI_AE_Dump_WeightTbl(void)
{
    //debug only, readback
    unsigned int val[32];
    unsigned int offset = 0, i;
    unsigned short *pae_table = 0;

    pae_table = (unsigned short *) (&S5K4ECGX_Driver.ae_table[0]);
    for (offset = 0; offset < 32; offset+=8)
    {
        SENSORDB("[4EC] AETbl SW_Tbl[%d~%d]: %4x, %4x, %4x, %4x\n", offset, offset+3,
        pae_table[offset+0], pae_table[offset+1], pae_table[offset+2], pae_table[offset+3]);
        SENSORDB("[4EC] AETbl SW_Tbl[%d~%d]: %4x, %4x, %4x, %4x\n", offset+4, offset+7,
        pae_table[offset+4], pae_table[offset+5], pae_table[offset+6], pae_table[offset+7]);
    }

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x1492); //0x2E08
    for (i = 0; i < 32; i++)
    {
        val[i] = S5K4ECGX_read_cmos_sensor(0x0F12);
    }

    for (offset = 0; offset < 32; offset+=8)
    {
        SENSORDB("[4EC] AETbl HW_RdBack[%d~%d]: %x, %x, %x, %x\n", offset, offset+3,
        val[offset+0], val[offset+1], val[offset+2], val[offset+3]);
        SENSORDB("[4EC] AETbl HW_RdBack[%d~%d]: %x, %x, %x, %x\n", offset+4, offset+7,
        val[offset+4], val[offset+5], val[offset+6], val[offset+7]);
    }
    return;
}



unsigned short
S5K4ECGX_MIPI_AE_Get_WeightVal(unsigned int x, unsigned int y)
{
    unsigned short val = 0;
    //debug only, readback
    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E ,0x2E08 + (y * 8) + (x & 0xFFFE));
    val = S5K4ECGX_read_cmos_sensor(0x0F12);

    return val;
}


static void
S5K4ECGX_MIPI_AE_Set_Window2HW(void)
{
    //unsigned int x0, y0, x1, y1;
    //unsigned char *tablePtr = 0;
    //unsigned int  stepX, stepY;
    unsigned short *pae_table = 0;
    unsigned int i;
    //unsigned int exposureTime;


    //SENSORDB("[4eC] AE_Set_Window2HW Original(%d, %d), Map(%d, %d)\n", x0, y0, x1, y1);

    pae_table = (unsigned short *) (&S5K4ECGX_Driver.ae_table[0]);
    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);

#if 0
    //Fast the AE convergence...
    S5K4ECGX_write_cmos_sensor(0x002A,0x0588);//0x0588
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);
#endif

    //Update AE weighting table
    S5K4ECGX_write_cmos_sensor(0x002A,0x1492);//0x2E08
    for (i = 0; i < 32; i++)
    {
       S5K4ECGX_write_cmos_sensor(0x0F12, pae_table[i]);
    }

    //Update to FW
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);  //REG_TC_GP_PrevConfigChanged

#if 0
    //Wait 1 frame to make the modification change effective...
    exposureTime = S5K4ECGX_MIPI_GetExposureTime();
    Sleep(exposureTime);

    //wait until AE stable.
    i = 30;
    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x2E08); //0x2E08
    while (i--)
    {
        if (S5K4ECGX_read_cmos_sensor(0x0F12))
        {
            break;
        }
        else
        {
            Sleep(3);
        }
    }

    //Reset the setting for AE convergence...
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x0588);//0x0588
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0002);
#endif

    //SENSORDB("[4EC]AE_Set_Window2HW\n");
    //S5K4ECGX_MIPI_AE_Dump_WeightTbl();
    return;
}



#define S5K4ECGX_MIPI_AE_MAX_WEIGHT_VAL 0xF
static S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AE_Set_Window(
    uintptr_t zone_addr,
    unsigned int prevW,
    unsigned int prevH)
{
    unsigned int x0, y0, x1, y1, width, height, i, j, xx, yy;
    unsigned int* ptr = (unsigned int*)zone_addr;
    unsigned int srcW_maxW; //source window's max width
    unsigned int srcW_maxH; //source window's max height
    unsigned char ae_table[8][8] = {0};
    unsigned int  stepX, stepY;
    unsigned char aeStateOnOriginalSet;
    x0 = *ptr       ;
    y0 = *(ptr + 1) ;
    x1 = *(ptr + 2) ;
    y1 = *(ptr + 3) ;
    width = *(ptr + 4);
    height = *(ptr + 5);
    srcW_maxW = width;
    srcW_maxH = height;

    SENSORDB("[4EC] AE_Set_Window 3AWin: (%d,%d)~(%d,%d)\n",x0, y0, x1, y1);

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    aeStateOnOriginalSet = 0;
    if ((x0 == x1) && (y0 == y1))
    {
        aeStateOnOriginalSet = 1;
        memcpy((unsigned char*)&ae_table[0][0], (unsigned char*)&S5K4ECGX_MIPI_DEFAULT_AE_TABLE[0], 64);
        SENSORDB("[4EC] AE_Set_Window aeStateOnOriginalSet = 1\n");
    }
    else
    {
        //SENSORDB("[4EC] AE_Set_Window aeStateOnOriginalSet = 0\n");
    }
    S5K4ECGX_Driver.apAEWindows[0] = x0;
    S5K4ECGX_Driver.apAEWindows[1] = y0;
    S5K4ECGX_Driver.apAEWindows[2] = x1;
    S5K4ECGX_Driver.apAEWindows[3] = y1;
    S5K4ECGX_Driver.apAEWindows[5] = width;
    S5K4ECGX_Driver.apAEWindows[6] = height;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);


    if (width == 0)
    {
        width = 320;
    }

    if (height == 0)
    {
        height = 240;
    }

    if (width > prevW)
    {
        width = prevW;
    }

    if (height > prevH)
    {
        height = prevH;
    }


    if (x0 >= srcW_maxW)
    {
        x0 = srcW_maxW - 1;
    }

    if (x1 >= srcW_maxW)
    {
        x1 = srcW_maxW - 1;
    }

    if (y0 >= srcW_maxH)
    {
        y0 = srcW_maxH - 1;
    }

    if (y1 >= srcW_maxH)
    {
        y1 = srcW_maxH - 1;
    }


    srcW_maxW = width;
    srcW_maxH = height;


    x0 = x0 * (prevW / srcW_maxW);
    y0 = y0 * (prevH / srcW_maxH);
    x1 = x1 * (prevW / srcW_maxW);
    y1 = y1 * (prevH / srcW_maxH);


    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.aeWindows[0] = x0;
    S5K4ECGX_Driver.aeWindows[1] = y0;
    S5K4ECGX_Driver.aeWindows[2] = x1;
    S5K4ECGX_Driver.aeWindows[3] = y1;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    x0 = (x0 + x1) / 2;
    y0 = (y0 + y1) / 2;
    stepX = prevW / 8;
    stepY = prevH / 8;

    if (stepX == 0)
    {
       stepX = 1;
    }
    if (stepY == 0)
    {
       stepY = 1;
    }

    x1 = x0 / stepX;
    y1 = y0 / stepY;
    if (x1 >= 8) x1 = 7;
    if (y1 >= 8) y1 = 7;


    SENSORDB("[4EC] AE_Set_Window 3AMapPnt: (%d,%d)\n", x1, y1);
    if (0 == aeStateOnOriginalSet)
    {
        memset(ae_table, 0x0, sizeof(ae_table));
        if (y1 != 0)
        {
            if (x1 != 0)
            {
                 for (i = y1 - 1; i <= y1 + 1; i++)
                 {
                     yy = (i > 7) ? 7:i;
                     for (j = x1 - 1; j <= x1 + 1; j++)
                     {
                         xx = (j > 7) ? 7:j;
                         ae_table[yy][xx] = 2;
                     }
                 }
            }
            else
            {
                for (i = y1 - 1; i <= y1 + 1; i++)
                {
                    yy = (i > 7) ? 7:i;
                    for (j = x1; j <= x1 + 2; j++)
                    {
                        xx = (j > 7) ? 7:j;
                        ae_table[yy][xx] = 2;
                    }
                }
            }
        }
        else
        {
            if (x1 != 0)
            {
                 for (i = y1; i <= y1 + 2; i++)
                 {
                     yy = (i > 7) ? 7:i;
                     for (j = x1 - 1; j <= x1 + 1; j++)
                     {
                         xx = (j > 7) ? 7:j;
                         ae_table[yy][xx] = 2;
                     }
                 }
            }
            else
            {
                for (i = y1; i <= y1 + 2; i++)
                {
                    yy = (i > 7) ? 7:i;
                    for (j = x1; j <= x1 + 2; j++)
                    {
                        xx = (j > 7) ? 7:j;
                        ae_table[yy][xx] = 2;
                    }
                }
            }
        }

        ae_table[y1][x1] = S5K4ECGX_MIPI_AE_MAX_WEIGHT_VAL;
   }


    spin_lock(&s5k4ecgx_mipi_drv_lock);
    if ((x1 == S5K4ECGX_Driver.mapAEWindows[0]) &&
        (y1 == S5K4ECGX_Driver.mapAEWindows[1]))
    {
        if (!(memcmp(&S5K4ECGX_Driver.ae_table[0], &ae_table[0][0], 64)))
        {
           //Table is the same...
           spin_unlock(&s5k4ecgx_mipi_drv_lock);
           SENSORDB("[4EC] AE_Set_Window: NewSet is the same as PrevSet.....\n");
           S5K4ECGX_MIPI_AE_Dump_WeightTbl();
           return S5K4ECGX_AAA_AF_STATUS_OK;
        }
    }
    S5K4ECGX_Driver.mapAEWindows[0] = x1;
    S5K4ECGX_Driver.mapAEWindows[1] = y1;
    memcpy(&S5K4ECGX_Driver.ae_table[0], &(ae_table[0][0]), sizeof(ae_table));
    spin_unlock(&s5k4ecgx_mipi_drv_lock);


    S5K4ECGX_MIPI_AE_Set_Window2HW();
    S5K4ECGX_MIPI_AE_Dump_WeightTbl();
    //SENSORDB("S5K4ECGX ~~~~S5K4ECGX_MIPI_AE_Set_Window: (%d,%d)~(%d,%d)\n",x0, y0, x1, y1);
    return S5K4ECGX_AAA_AF_STATUS_OK;
}




static void
S5K4ECGX_MIPI_AF_Get_Max_Num_Focus_Areas(unsigned int *pFeatureReturnPara32)
{
    *pFeatureReturnPara32 = 1;
    //SENSORDB("S5K4ECGX *pFeatureReturnPara32 = %d\n",  *pFeatureReturnPara32);
}

static void
S5K4ECGX_MIPI_AE_Get_Max_Num_Metering_Areas(unsigned int *pFeatureReturnPara32)
{
    *pFeatureReturnPara32 = 1;
    //SENSORDB("S5K4ECGX_MIPI_AE_Get_Max_Num_Metering_Areas *pFeatureReturnPara32 = %d\n",  *pFeatureReturnPara32);
}

static void
S5K4ECGX_MIPI_AF_Get_Inf(unsigned int *pFeatureReturnPara32)
{
    *pFeatureReturnPara32 = 0;
}

static void
S5K4ECGX_MIPI_AF_Get_Macro(unsigned int *pFeatureReturnPara32)
{
    *pFeatureReturnPara32 = 0;//255;
}



static void
S5K4ECGX_MIPI_AF_rollbackWinSet(void)
{
    S5K4ECGX_MIPI_AF_WIN_T  *AfWindows;

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    AfWindows = &S5K4ECGX_Driver.orignalAfWindows;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x029C);
    S5K4ECGX_write_cmos_sensor(0x0F12,AfWindows->inWx);
    S5K4ECGX_write_cmos_sensor(0x002A,0x029E);
    S5K4ECGX_write_cmos_sensor(0x0F12,AfWindows->inWy);

    S5K4ECGX_write_cmos_sensor(0x002A,0x0294);
    S5K4ECGX_write_cmos_sensor(0x0F12,AfWindows->outWx);
    S5K4ECGX_write_cmos_sensor(0x002A,0x0296);
    S5K4ECGX_write_cmos_sensor(0x0F12,AfWindows->outWy);

    //Update AF Window
    S5K4ECGX_write_cmos_sensor(0x002A,0x02A4);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);
    return;
}



S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Init(void)
{
    unsigned int backupAFWindDone = 1;

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    // have done in init_function
    if (S5K4ECGX_AF_STATE_UNINIT == S5K4ECGX_Driver.afState)
    {
        S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_IDLE;
        backupAFWindDone = 0;
    }
    S5K4ECGX_Driver.afMode = S5K4ECGX_AF_MODE_RSVD;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    if (!backupAFWindDone)
    {
        S5K4ECGX_MIPI_AF_WIN_T  *AfWindows;

        spin_lock(&s5k4ecgx_mipi_drv_lock);
        AfWindows = &S5K4ECGX_Driver.orignalAfWindows;
        spin_unlock(&s5k4ecgx_mipi_drv_lock);

        S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
        S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002E,0x029C);
        AfWindows->inWx = S5K4ECGX_read_cmos_sensor(0x0F12);
        AfWindows->inWy = S5K4ECGX_read_cmos_sensor(0x0F12);

        S5K4ECGX_write_cmos_sensor(0x002E,0x0294);
        AfWindows->outWx = S5K4ECGX_read_cmos_sensor(0x0F12);
        AfWindows->outWy = S5K4ECGX_read_cmos_sensor(0x0F12);

    }

    return S5K4ECGX_AAA_AF_STATUS_OK;
}




S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Set_Window2HW(void)
{
    unsigned int inWx1, inWy1, inWw1, inWh1; // x, y, width, height
    unsigned int outWx1, outWy1, outWw1, outWh1; // x, y, width, height

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    if (S5K4ECGX_Driver.afStateOnOriginalSet)
    {
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
        return S5K4ECGX_AAA_AF_STATUS_OK;
    }

    inWx1  = S5K4ECGX_Driver.afWindows.inWx;
    inWy1  = S5K4ECGX_Driver.afWindows.inWy;
    inWw1  = S5K4ECGX_Driver.afWindows.inWw;
    inWh1  = S5K4ECGX_Driver.afWindows.inWh;
    outWx1 = S5K4ECGX_Driver.afWindows.outWx;
    outWy1 = S5K4ECGX_Driver.afWindows.outWy;
    outWw1 = S5K4ECGX_Driver.afWindows.outWw;
    outWh1 = S5K4ECGX_Driver.afWindows.outWh;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);


    //SENSORDB("[4EC] AF_Set_Window2HW: In:(%d, %d), Out(%d, %d)\n", inWx1, inWy1, outWx1, outWy1);
    //S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x029C);
    S5K4ECGX_write_cmos_sensor(0x0F12,inWx1);
    S5K4ECGX_write_cmos_sensor(0x002A,0x029E);
    S5K4ECGX_write_cmos_sensor(0x0F12,inWy1);

    S5K4ECGX_write_cmos_sensor(0x002A,0x0294);
    S5K4ECGX_write_cmos_sensor(0x0F12,outWx1);
    S5K4ECGX_write_cmos_sensor(0x002A,0x0296);
    S5K4ECGX_write_cmos_sensor(0x0F12,outWy1);

    //For the size part, FW will update automatically
    //Update AF Window
    S5K4ECGX_write_cmos_sensor(0x002A,0x02A4);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);

    return S5K4ECGX_AAA_AF_STATUS_OK;
}





unsigned int orig_inWw = 0;
unsigned int orig_inWh = 0;
unsigned int orig_outWw = 0;
unsigned int orig_outWh = 0;
#define S5K4ECGX_FAF_TOLERANCE    100
#define S5K4ECGX_AF_MIX_ACCURACY  0x200000 //0x8000 //Perfer: 0x2x0000
static S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Set_Window(
    uintptr_t zone_addr,
    unsigned int prevW,
    unsigned int prevH)
{
    unsigned int x0, y0, x1, y1, FD_XS, FD_YS;
    unsigned int* ptr = (unsigned int*)zone_addr;
    unsigned int srcW_maxW = S5K4ECGX_MIPI_AF_CALLER_WINDOW_WIDTH;
    unsigned int srcW_maxH = S5K4ECGX_MIPI_AF_CALLER_WINDOW_HEIGHT;
    unsigned int af_win_idx = 1, frameTime;
    unsigned int af_resolution = 0;

    x0 = *ptr       ;
    y0 = *(ptr + 1) ;
    x1 = *(ptr + 2) ;
    y1 = *(ptr + 3) ;
    FD_XS = *(ptr + 4);
    FD_YS = *(ptr + 5);
    if (FD_XS == 0)
    {
        FD_XS = 320;
    }

    if (FD_YS == 0)
    {
        FD_YS = 240;
    }

    if (FD_XS > prevW)
    {
        FD_XS = prevW;
    }

    if (FD_YS > prevH)
    {
        FD_YS = prevH;
    }


    SENSORDB("[4EC] AF_Set_Window AP's setting: (%d,%d)~(%d,%d).size:(%d,%d)\n",x0, y0, x1, y1, FD_XS, FD_YS);

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.afStateOnOriginalSet = 0;
    if ((x0 == x1) && (y0 == y1))
    {
        S5K4ECGX_Driver.afStateOnOriginalSet = 1;
    }
    spin_unlock(&s5k4ecgx_mipi_drv_lock);


    srcW_maxW = FD_XS;
    srcW_maxH = FD_YS;

    if (x0 >= srcW_maxW)
    {
        x0 = srcW_maxW - 1;
    }

    if (x1 >= srcW_maxW)
    {
        x1 = srcW_maxW - 1;
    }

    if (y0 >= srcW_maxH)
    {
        y0 = srcW_maxH - 1;
    }

    if (y1 >= srcW_maxH)
    {
        y1 = srcW_maxH - 1;
    }

    x0 = (x0 + x1) / 2;
    y0 = (y0 + y1) / 2;

    //Map 320x240 coordinate to preview size window
    x0 = x0 * (prevW / srcW_maxW);
    y0 = y0 * (prevH / srcW_maxH);


    {
        unsigned int inWw0, inWh0, outWw0, outWh0;
        unsigned int inWx1, inWy1, inWw1, inWh1;     // x, y, width, height
        unsigned int outWx1, outWy1, outWw1, outWh1; // x, y, width, height

        if ((orig_inWw == 0) || (orig_inWh == 0))
        {
            //Calculate Inner & Outer Window Size

            S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
            S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
            S5K4ECGX_write_cmos_sensor(0x002E,0x02A0);
            inWw0 = S5K4ECGX_read_cmos_sensor(0x0F12);
            orig_inWw = inWw0 * prevW / 1024;

            S5K4ECGX_write_cmos_sensor(0x002E,0x02A2);
            inWh0 = S5K4ECGX_read_cmos_sensor(0x0F12);
            orig_inWh = inWh0 * prevH / 1024;

            S5K4ECGX_write_cmos_sensor(0x002E,0x0298);
            outWw0 = S5K4ECGX_read_cmos_sensor(0x0F12);
            orig_outWw = outWw0 * prevW / 1024;

            S5K4ECGX_write_cmos_sensor(0x002E,0x029A);
            outWh0 = S5K4ECGX_read_cmos_sensor(0x0F12);
            orig_outWh = outWh0 * prevH / 1024;
        }

        inWw1 = orig_inWw;
        inWh1 = orig_inWh;
        outWw1 = orig_outWw;
        outWh1 = orig_outWh;


        //Set X axis
        if (x0 <= (inWw1/2))
        {
            inWx1 = 0;
            outWx1 = 0;
        }
        else if (x0 <= (outWw1 / 2))
        {
            inWx1 = x0 - (inWw1 / 2);
            outWx1 = 0;
        }
        else if (x0 >= ((prevW-1) - (inWw1 / 2)))
        {
            inWx1  = (prevW-1) - inWw1;
            outWx1 = (prevW-1) - outWw1;
        }
        else if (x0 >= ((prevW-1) - (outWw1 / 2)))
        {
            inWx1  = x0 - (inWw1/2);
            outWx1 = (prevW-1) - outWw1;
        }
        else
        {
            inWx1  = x0 - (inWw1/2);
            outWx1 = x0 - (outWw1/2);
        }


        //Set Y axis
        if (y0 <= (inWh1/2))
        {
            inWy1 = 0;
            outWy1 = 0;
        }
        else if (y0 <= (outWh1/2))
        {
            inWy1 = y0 - (inWh1/2);
            outWy1 = 0;
        }
        else if (y0 >= ((prevH-1) - (inWh1/2)))
        {
            inWy1  = (prevH-1) - inWh1;
            outWy1 = (prevH-1) - outWh1;
        }
        else if (y0 >= ((prevH-1) - (outWh1/2)))
        {
            inWy1  = y0 - (inWh1/2);
            outWy1 = (prevH-1) - outWh1;
        }
        else
        {
            inWy1  = y0 - (inWh1/2);
            outWy1 = y0 - (outWh1/2);
        }

        inWx1  =  inWx1 * 1024 / (prevW);
        inWy1  =  inWy1 * 1024 / (prevH);
        outWx1 = outWx1 * 1024 / (prevW);
        outWy1 = outWy1 * 1024 / (prevH);


         //restore
        spin_lock(&s5k4ecgx_mipi_drv_lock);
        //check if we really need to update AF window? the tolerance shall less than 10 pixels
        //on the both x/y direction for inner & ouuter window
        if (!((inWx1 > (S5K4ECGX_Driver.afWindows.inWx + S5K4ECGX_FAF_TOLERANCE)) ||
            ((inWx1 + S5K4ECGX_FAF_TOLERANCE) < S5K4ECGX_Driver.afWindows.inWx) ||
            (inWy1 > (S5K4ECGX_Driver.afWindows.inWy + S5K4ECGX_FAF_TOLERANCE)) ||
            ((inWy1 + S5K4ECGX_FAF_TOLERANCE) < S5K4ECGX_Driver.afWindows.inWy) ||

            (outWx1 > (S5K4ECGX_Driver.afWindows.outWx + S5K4ECGX_FAF_TOLERANCE)) ||
            ((outWx1 + S5K4ECGX_FAF_TOLERANCE) < S5K4ECGX_Driver.afWindows.outWx) ||
            (outWy1 > (S5K4ECGX_Driver.afWindows.outWy + S5K4ECGX_FAF_TOLERANCE)) ||
            ((outWy1 + S5K4ECGX_FAF_TOLERANCE) < S5K4ECGX_Driver.afWindows.outWy)))
        {
             //The AF window is very near the previous', thus we keep the
             //previous setting
             spin_unlock(&s5k4ecgx_mipi_drv_lock);

             //S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
             //S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
             //S5K4ECGX_write_cmos_sensor(0x002E,0x2F98); //0x2F9A
             //af_resolution = S5K4ECGX_read_cmos_sensor(0x0F12);  //0x2F98
             //af_resolution += S5K4ECGX_read_cmos_sensor(0x0F12)<<16; //0x2F9A
             //if (af_resolution > S5K4ECGX_AF_MIX_ACCURACY)
            {
                SENSORDB("[4EC] AF window is very near the previous'.......\n");
                return S5K4ECGX_AAA_AF_STATUS_OK;
            }
        }
        else
        {
            spin_unlock(&s5k4ecgx_mipi_drv_lock);
        }

        spin_lock(&s5k4ecgx_mipi_drv_lock);
        S5K4ECGX_Driver.afWindows.inWx = inWx1;
        S5K4ECGX_Driver.afWindows.inWy = inWy1;
        S5K4ECGX_Driver.afWindows.inWw = inWw1;
        S5K4ECGX_Driver.afWindows.inWh = inWh1;
        S5K4ECGX_Driver.afWindows.outWx = outWx1;
        S5K4ECGX_Driver.afWindows.outWy = outWy1;
        S5K4ECGX_Driver.afWindows.outWw = outWw1;
        S5K4ECGX_Driver.afWindows.outWh = outWh1;
        spin_unlock(&s5k4ecgx_mipi_drv_lock);

    }

    /*SENSORDB("[4EC] AF window inXY:(%d,%d), inWH:(%d,%d), outXY:(%d,%d), outWH: (%d,%d)\n",
    S5K4ECGX_Driver.afWindows.inWx, S5K4ECGX_Driver.afWindows.inWy,
    S5K4ECGX_Driver.afWindows.inWw, S5K4ECGX_Driver.afWindows.inWh,
    S5K4ECGX_Driver.afWindows.outWx, S5K4ECGX_Driver.afWindows.outWy,
    S5K4ECGX_Driver.afWindows.outWw, S5K4ECGX_Driver.afWindows.outWh);*/


    //SENSORDB("[4EC] AF window: FrameTime=%d ms \n", frameTime);

    frameTime = S5K4ECGX_MIPI_GetExposureTime();
    //frameTime = 30;

    if (S5K4ECGX_Driver.afStateOnOriginalSet)
    {
        //Rollback AE/AF setting for CAF mode starting
        SENSORDB("[4EC] AF window afStateOnOriginalSet = 1.......\n");
        S5K4ECGX_MIPI_AF_rollbackWinSet();
    }
    else
    {
        SENSORDB("[4EC] AF window Update New Window.......\n");
        S5K4ECGX_MIPI_AF_Set_Window2HW();
    }
    Sleep(frameTime); // delay 1 frame

    return S5K4ECGX_AAA_AF_STATUS_OK;
}




S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Stop(void)
{

#if 0
#if defined(S5K4ECGX_MIPI_AF_Enable)

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_IDLE;

    ////Lock AE
    if ((!S5K4ECGX_Driver.userAskAeLock) &&
        (S5K4ECGX_AF_MODE_SINGLE == S5K4ECGX_Driver.afMode))
    {
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
        S5K4ECGX_MIPI_AE_UnLock();
    }
    else
    {
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
    }
    SENSORDB("S5K4ECGX ~~~~S5K4ECGX_MIPI_AF_Stop---\n");
#endif
#endif

    return S5K4ECGX_AAA_AF_STATUS_OK;
}


/*
static void
S5K4ECGX_MIPI_AF_ShowLensPosition(void)
{
    unsigned int lens_cur_pos, lens_best_pos, lens_prev_best_pos;
    unsigned int lens_tlb_idx;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x15E8);
    lens_tlb_idx = S5K4ECGX_read_cmos_sensor(0x0F12);

    S5K4ECGX_write_cmos_sensor(0x002E,0x2EE8);
    lens_cur_pos = S5K4ECGX_read_cmos_sensor(0x0F12);

    S5K4ECGX_write_cmos_sensor(0x002E,0x2EEA);
    lens_best_pos = S5K4ECGX_read_cmos_sensor(0x0F12);

    S5K4ECGX_write_cmos_sensor(0x002E,0x2EEC);
    lens_prev_best_pos = S5K4ECGX_read_cmos_sensor(0x0F12);

    //SENSORDB("~~~~AF_GetLensPosition: lens_tlb_idx=%d; cur_pos=%x, best_pos=%x, prev_best_pos=%x\n", lens_tlb_idx, lens_cur_pos, lens_best_pos, lens_prev_best_pos);

    return;
}
*/


static S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_CancelFocus(void)
{
    signed int loop_iter = 35;
    unsigned int af_status;
    unsigned int frameTime;

    SENSORDB("[4EC] CancelFocus+\n");

    if (S5K4ECGX_AF_MODE_RSVD == S5K4ECGX_Driver.afMode)
    {
        //have been aborted
        return S5K4ECGX_AAA_AF_STATUS_OK;
    }

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);

    af_status = 1;
    while(loop_iter--)
    {
        S5K4ECGX_write_cmos_sensor(0x002E,0x2EEE);
        af_status = S5K4ECGX_read_cmos_sensor(0x0F12);
        if ((af_status == 1) || (af_status == 6) || (af_status == 7))
        {
            //Wait for Focusing or Scene Detecting done.
            Sleep(2);
        }
    }


    S5K4ECGX_write_cmos_sensor(0x002E,0x2EEE);
    af_status = S5K4ECGX_read_cmos_sensor(0x028C);

    frameTime = S5K4ECGX_MIPI_GetExposureTime();
    //SENSORDB("[4EC] AF window: FrameTime=%d ms \n", frameTime);

    if (1 != af_status) //!aborting
    {
        S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
        S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002A,0x028C);
        S5K4ECGX_write_cmos_sensor(0x0F12,0x0001); //abort
        Sleep(frameTime);
    }

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.afPrevMode = S5K4ECGX_Driver.afMode;
    S5K4ECGX_Driver.afMode = S5K4ECGX_AF_MODE_RSVD;
    S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_DONE;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);
    //SENSORDB("[4EC] CancelFocus-\n");

    return S5K4ECGX_AAA_AF_STATUS_OK;
}




S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Start(S5K4ECGX_AF_MODE_ENUM mode)
{
    unsigned int af_status;
    unsigned int frameTime;
    //signed int loop_iter = 200;

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    if (S5K4ECGX_Driver.userAskAeLock)
    {
        //AE lock is for Panorama mode, in this case, AF should also be Locked.
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
        SENSORDB("[4EC] AF_Start+: Return due2 AE Been Loked\n");
        return S5K4ECGX_AAA_AF_STATUS_OK;
    }


    if (mode == S5K4ECGX_AF_MODE_SINGLE)
    {
        SENSORDB("[4EC] SAF_Start+\n\n");
        S5K4ECGX_Driver.afMode = S5K4ECGX_AF_MODE_SINGLE;
        S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_ENTERING;
    }
    else
    {
        SENSORDB("[4EC] CAF_Start+\n\n");
        if (S5K4ECGX_AF_MODE_CONTINUOUS == S5K4ECGX_Driver.afMode)
        {
            spin_unlock(&s5k4ecgx_mipi_drv_lock);
            SENSORDB("[4EC] CAF_Start: Been at this Mode...\n");
            return;
        }
        S5K4ECGX_Driver.afMode = S5K4ECGX_AF_MODE_CONTINUOUS;
        S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_ENTERING;
    }
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    frameTime = S5K4ECGX_MIPI_GetExposureTime();
    //SENSORDB("[4EC] AF_Start+, frameTime=%d ms\n", frameTime);


    if (mode == S5K4ECGX_AF_MODE_SINGLE)
    {
        S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
        S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
        S5K4ECGX_write_cmos_sensor(0x002A, 0x163E);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0); //af_search_usPeakThr
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080); //af_search_usPeakThrLow
        S5K4ECGX_write_cmos_sensor(0x002A, 0x15E8);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010); //af_pos_usTableLastInd
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0018); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0028); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0038); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0048); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0050); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0058); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0068); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0078); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0088); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0090); //af_pos_usTable
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0098); //af_pos_usTable

    }
    else
    {
            S5K4ECGX_write_cmos_sensor(0x002A, 0x163E);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x00a0);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070); //D0 //#af_search_usPeakThrLow,  Continous AF tuning point
            S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
            S5K4ECGX_write_cmos_sensor(0x002A, 0x15E8);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A); //#af_pos_usTableLastInd// table0 ~ table24,  25 Steps
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0018); //#af_pos_usTable_0_ // af_pos_usTable
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030); //#af_pos_usTable_1_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0048); //#af_pos_usTable_2_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0058); //#af_pos_usTable_3_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0068); //#af_pos_usTable_4_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070); //#af_pos_usTable_5_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0078); //#af_pos_usTable_6_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080); //#af_pos_usTable_7_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0088); //#af_pos_usTable_8_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0090); //#af_pos_usTable_9_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0098); //#af_pos_usTable_10_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0090); //#af_pos_usTable_11_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0098); //#af_pos_usTable_12_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0054); //#af_pos_usTable_13_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0058); //#af_pos_usTable_14_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x005C); //#af_pos_usTable_15_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060); //#af_pos_usTable_16_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0064); //#af_pos_usTable_17_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0068); //#af_pos_usTable_18_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x006C); //#af_pos_usTable_19_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070); //#af_pos_usTable_20_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0074); //#af_pos_usTable_21_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0078); //#af_pos_usTable_22_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x007C); //#af_pos_usTable_23_
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080); //#af_pos_usTable_24_
    }


    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x028C);
    if (mode == S5K4ECGX_AF_MODE_SINGLE)
    {
        S5K4ECGX_write_cmos_sensor(0x0F12,0x0005);
    }
    else
    {
        S5K4ECGX_write_cmos_sensor(0x0F12,0x0006); //contitous mode
    }
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_ENTERED;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    Sleep(frameTime * 2); //delay 2 frames
    return;
}



static S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Move_to(unsigned int a_u2MovePosition)//??how many bits for ov3640??
{
    return S5K4ECGX_AAA_AF_STATUS_OK;
}




static S5K4ECGX_AAA_STATUS_ENUM
S5K4ECGX_MIPI_AF_Get_Status(unsigned int *pFeatureReturnPara32)
{
    S5K4ECGX_AF_STATE_ENUM af_state;
    S5K4ECGX_AF_MODE_ENUM af_Mode;
    unsigned int af_1stSearch_status;
    unsigned int frameTime;

    //return SENSOR_AF_FOCUSED;


    if (S5K4ECGX_AF_STATE_ENTERING == S5K4ECGX_Driver.afState)
    {
        SENSORDB("\n[4EC]AFGet_Status: ENTERING State\n");
        *pFeatureReturnPara32 = SENSOR_AF_IDLE;
        return S5K4ECGX_AAA_AF_STATUS_OK;
    }


    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x2EEE);
    af_1stSearch_status = S5K4ECGX_read_cmos_sensor(0x0F12);

    {
        switch (af_1stSearch_status)
        {
            case 0:
                *pFeatureReturnPara32 = SENSOR_AF_IDLE;//;
                //SENSORDB("\n[4EC] AF--- IDLE\n");
                break;
            case 1:
                *pFeatureReturnPara32 = SENSOR_AF_FOCUSING;
                //SENSORDB("\n[4EC] AF--- AF_FOCUSING\n");
                break;

            case 2:
                *pFeatureReturnPara32 = SENSOR_AF_FOCUSED;
                //SENSORDB("\n[4EC] AF--- FOCUSED\n");
                break;

            case 3: // the 1st search is low confidence
                *pFeatureReturnPara32 = SENSOR_AF_ERROR;//SENSOR_AF_ERROR;
                //SENSORDB("\n[4EC] AF--- LOW_CONFIDENCE\n");
                break;

            case 4: // canceld
                *pFeatureReturnPara32 = SENSOR_AF_IDLE;//SENSOR_AF_IDLE;
                spin_lock(&s5k4ecgx_mipi_drv_lock);
                S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_DONE;
                spin_unlock(&s5k4ecgx_mipi_drv_lock);
                //SENSORDB("\n[4EC] AF--- CANCELLED\n");
                break;

            case 6: //restart AE
                *pFeatureReturnPara32 = SENSOR_AF_SCENE_DETECTING;//SENSOR_AF_SCENE_DETECTING;
                //SENSORDB("\n[4EC] AF--- Restart_AE\n");
                break;

            case 7: //restart Scene
                *pFeatureReturnPara32 = SENSOR_AF_SCENE_DETECTING;//SENSOR_AF_SCENE_DETECTING;
                //SENSORDB("\n[4EC] AF--- Restart_Scene\n");
                break;

            default:
                *pFeatureReturnPara32 = SENSOR_AF_SCENE_DETECTING;
                SENSORDB("\n[4EC] AF--- default. Status:%x\n", af_1stSearch_status);
                {
                    //read sensor id here to check sensor is died or not?
                    UINT32 sensorID = 0;
                    S5K4ECGX_MIPI_GetSensorID(&sensorID);
                }
                break;
        }

        return S5K4ECGX_AAA_AF_STATUS_OK;
    }

    *pFeatureReturnPara32 = SENSOR_AF_FOCUSED;
     return S5K4ECGX_AAA_AF_STATUS_OK;
}



static void S5K4ECGX_MIPI_set_AF_infinite(kal_bool is_AF_OFF)
{
#if 0
    if(is_AF_OFF){
      S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
      S5K4ECGX_write_cmos_sensor(0x002a, 0x028E);
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
      Sleep(100);
      S5K4ECGX_write_cmos_sensor(0x002a, 0x028C);
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    } else {
      S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
      S5K4ECGX_write_cmos_sensor(0x002a, 0x028C);
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    }
#endif

}
/***********************************************************
      AF Control End                                      **
***********************************************************/





/***********************************************************
**    Strob Control Start
***********************************************************/

/*************************************************************************
* FUNCTION
*    S5K4ECGXReadShutter
*
* DESCRIPTION
*    This function Read Shutter from sensor
*
* PARAMETERS
*    Shutter: integration time
*
* RETURNS
*    None
*
* LOCAL AFFECTED
*
*************************************************************************/
static kal_uint32 S5K4ECGX_MIPI_ReadShutter(void)
{
    unsigned int interval = 0;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x2C28);
    interval  = S5K4ECGX_read_cmos_sensor(0x0F12);
    interval |= (S5K4ECGX_read_cmos_sensor(0x0F12) << 16);

    // reg is in terms of 1000/400 us
    interval = interval * 5 / 2; //us

    SENSORDB("[4EC] Shutter = %d us\n", interval);
    return interval;
}


void S5K4ECGX_MIPI_SetShutter(kal_uint32 iShutter)
{
    // iShutter is in terms of 32us
    iShutter *= 32;
    unsigned int exposureTime = iShutter >> 3; // to hardware reg

    SENSORDB("[4EC] SetdShutter+, iShutter=%d us, 0x%08x\n", iShutter, exposureTime);
    //Change to Manual AE
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x04E6);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0779); //Manual AE enable
    S5K4ECGX_Driver.manualAEStart = 1;

    S5K4ECGX_write_cmos_sensor(0x002A,0x04AC);
    S5K4ECGX_write_cmos_sensor(0x0F12,exposureTime&0xFFFF); //Exposure time
    S5K4ECGX_write_cmos_sensor(0x002A,0x04AE);
    S5K4ECGX_write_cmos_sensor(0x0F12,exposureTime>>16); //Exposure time
    S5K4ECGX_write_cmos_sensor(0x002A,0x04B0);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001); //Exposure time update

    //S5K4ECGX_write_cmos_sensor(0x002A,0x04B2);
    //S5K4ECGX_write_cmos_sensor(0x0F12,totalGain);   //Total gain
    //S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);
        return;
}


/*************************************************************************
* FUNCTION
*    S5K4ECGX_MIPI_ReadGain
* DESCRIPTION
*    This function get gain from sensor
* PARAMETERS
*    None
* RETURNS
*    Gain: base on 0x40
* LOCAL AFFECTED
*************************************************************************/
static kal_uint32 S5K4ECGX_MIPI_ReadGain(void)
{

    //S5K4ECGX_MIPI_GetAutoISOValue();

    unsigned int A_Gain, D_Gain, ISOValue;
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E, 0x2BC4);
    A_Gain = S5K4ECGX_read_cmos_sensor(0x0F12);
    D_Gain = S5K4ECGX_read_cmos_sensor(0x0F12);

    ISOValue = ((A_Gain * D_Gain) >> 8);

    SENSORDB("[4EC] ReadGain+, isoSpeed=%d\n", ISOValue);
/*
    switch (S5K4ECGX_Driver.isoSpeed)
    {
       case AE_ISO_200:
          return 200;

       case AE_ISO_400:
       return 400;

       case AE_ISO_100:
       default:
       return 100;
}
*/
    return ISOValue;
}



/*************************************************************************
* FUNCTION
*    S5K4ECGX_MIPI_SetGain
* DESCRIPTION
*    This function set gain to sensor
* PARAMETERS
*    None
* RETURNS
*    Gain: base on 0x40
* LOCAL AFFECTED
*************************************************************************/
static void S5K4ECGX_MIPI_SetGain(kal_uint32 iGain)
{
    // Cal. Method : ((A-Gain*D-Gain)/100h)/2
    // A-Gain , D-Gain Read value is hex value.
    //   ISO 50  : 100(HEX)
    //   ISO 100 : 100 ~ 1FF(HEX)
    //   ISO 200 : 200 ~ 37F(HEX)
    //   ISO 400 : over 380(HEX)


    unsigned int totalGain = iGain;
    /*
    switch (iGain)
    {
       case 200:
        totalGain = 0x2BF; //(0x200 + 0x37F) / 2;
        break;

       case 400:
        totalGain = 0x380;
        break;

       case 100:
       default:
        totalGain = 0x17F;
        break;
    }
    */
    SENSORDB("[4EC] SetGain+, isoGain=%d\n", totalGain);

    //Change to Manual AE
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x04E6);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0779); //Manual AE enable

    //S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A,0x04B2);
    S5K4ECGX_write_cmos_sensor(0x0F12,totalGain);   //Total gain
    S5K4ECGX_write_cmos_sensor(0x002A,0x04B4);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);

#if 0
    switch (iGain)
    {
        case 200://AE_ISO_200:
             //ISO 200
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0484);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D6);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_FlickerQuant
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_FlickerQuantChanged

             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0380);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);  //lt_bUseSecISODgain
             break;
        case 400://AE_ISO_400:
             //ISO 400
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x08D2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C84);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x10D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D6);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_FlickerQuant
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_FlickerQuantChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0700);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);  //lt_bUseSecISODgain
             break;
        default:
            case 100://AE_ISO_100:
             //ISO 100
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D6);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_FlickerQuant
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_FlickerQuantChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C0);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);  //lt_bUseSecISODgain
             break;
    }
#endif

    return;
}


void S5K4ECGX_MIPI_get_exposure_gain()
{
    kal_uint32 again = 0, dgain = 0, evTime = 0;

    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E, 0x2C28);
    evTime  = S5K4ECGX_read_cmos_sensor(0x0F12);
    evTime += S5K4ECGX_read_cmos_sensor(0x0F12) << 16 ;
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.currentExposureTime = evTime >> 2;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x2BC4);
    again = S5K4ECGX_read_cmos_sensor(0x0F12); //A gain
    dgain = S5K4ECGX_read_cmos_sensor(0x0F12); //D gain

    spin_lock(&s5k4ecgx_mipi_drv_lock);

    S5K4ECGX_Driver.currentAxDGain = (dgain * again) >> 8;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

}


void S5K4ECGX_MIPI_GetAEFlashlightInfo(uintptr_t infoAddr)
{
    SENSOR_FLASHLIGHT_AE_INFO_STRUCT* pAeInfo = (SENSOR_FLASHLIGHT_AE_INFO_STRUCT*) infoAddr;

    pAeInfo->Exposuretime = S5K4ECGX_MIPI_ReadShutter();
    pAeInfo->Gain = S5K4ECGX_MIPI_ReadGain();
    pAeInfo->u4Fno = 28;
    pAeInfo->GAIN_BASE = 0x100;

    return;
}



#define FLASH_BV_THRESHOLD 0x25
static void S5K4ECGX_MIPI_FlashTriggerCheck(unsigned int *pFeatureReturnPara32)
{
    unsigned int NormBr;

    //S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x29A4);
    NormBr = S5K4ECGX_read_cmos_sensor(0x0F12);

    if (NormBr > FLASH_BV_THRESHOLD)
    {
       *pFeatureReturnPara32 = FALSE;
        return;
    }

    *pFeatureReturnPara32 = TRUE;
    return;
}


/***********************************************************
     Strob Control End                                    **
***********************************************************/



/***********************************************************
     JPEG Sensor Start                                    **
***********************************************************/
static
void S5K4ECGX_MIPI_Config_JPEG_Capture(ACDK_SENSOR_JPEG_OUTPUT_PARA *jpeg_para)
{
#if defined(__CAPTURE_JPEG_OUTPUT__)
    /*
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0476);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_TC_BRC_BRC_type //0x5:enable BRC
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0478);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x005F); //REG_TC_BRC_usPrevQuality
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x005F); //REG_TC_BRC_usCaptureQuality
    S5K4ECGX_write_cmos_sensor(0x002A, 0x047c);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_TC_THUMB_Thumb_bActive
    */

    S5K4ECGX_write_cmos_sensor(0x002A, 0x0398);
    S5K4ECGX_write_cmos_sensor(0x0F12, jpeg_para->tgtWidth);
    S5K4ECGX_write_cmos_sensor(0x0F12, jpeg_para->tgtHeight);

    S5K4ECGX_write_cmos_sensor(0x002A, 0x0478);
    S5K4ECGX_write_cmos_sensor(0x0F12, jpeg_para->quality); //REG_TC_BRC_usPrevQuality

#endif

    return;
}


#define JPEG_MARKER_SKIP_CODE    0x00  // For the case of 0xFF00
#define JPEG_MARKER_START_CODE   0XFF

#define JPEG_MARKER_SOF(I)       (0xC0 + I)

/* Start of Frame markers, non-differential, Huffman coding */
#define JPEG_MARKER_SOF0         0XC0
#define JPEG_MARKER_SOF1         0XC1
#define JPEG_MARKER_SOF2         0XC2
#define JPEG_MARKER_SOF3         0XC3

/* Start of Frame markers, differential, Huffman coding */
#define JPEG_MARKER_SOF5         0XC5
#define JPEG_MARKER_SOF6         0XC6
#define JPEG_MARKER_SOF7         0XC7

/* Start of Frame markers, non-differential, arithmetic coding */
#define JPEG_MARKER_JPG0         0XC8
#define JPEG_MARKER_SOF9         0XC9
#define JPEG_MARKER_SOF10        0XCA
#define JPEG_MARKER_SOF11        0XCB

/* Start of Frame markers, differential, arithmetic coding */
#define JPEG_MARKER_SOF13        0xCD
#define JPEG_MARKER_SOF14        0xCE
#define JPEG_MARKER_SOF15        0xCF

/* Huffman table specification */
#define JPEG_MARKER_DHT          0xC4  /* Define Huffman table(s) */

/* Arithmetic coding conditioning specification */
#define JPEG_MARKER_DAC          0xCC  /* Define arithmetic coding conditioning(s) */

/* Restart interval termination */
#define JPEG_MARKER_RST(I)       (0xD0 + I)
#define JPEG_MARKER_RST0         0xD0
#define JPEG_MARKER_RST1         0xD1
#define JPEG_MARKER_RST2         0xD2
#define JPEG_MARKER_RST3         0xD3
#define JPEG_MARKER_RST4         0xD4
#define JPEG_MARKER_RST5         0xD5
#define JPEG_MARKER_RST6         0xD6
#define JPEG_MARKER_RST7         0xD7

#define JPEG_MARKER_SOI          0xD8
#define JPEG_MARKER_EOI          0xD9
#define JPEG_MARKER_SOS          0xDA
#define JPEG_MARKER_DQT          0xDB
#define JPEG_MARKER_DNL          0xDC
#define JPEG_MARKER_DRI          0xDD
#define JPEG_MARKER_DHP          0xDE
#define JPEG_MARKER_EXP          0xDF

#define JPEG_MARKER_APP(I)       (0xE0 + I)

#define JPEG_MARKER_JPG(I)       (0xF0 + I)

#define JPEG_MARKER_TEM          0x01

#define JPEG_MARKER_COM          0xFE

/// Decoder
typedef enum {
   JPEG_PARSE_STATE_STOP = 0,
   JPEG_PARSE_STATE_WAITING_FOR_SOI,

   JPEG_PARSE_STATE_WAITING_FOR_MARKER,
   JPEG_PARSE_STATE_WAITING_FOR_LENGTH,
   JPEG_PARSE_STATE_WAITING_FOR_DATA,

   JPEG_PARSE_STATE_ERROR,
   JPEG_PARSE_STATE_COMPLETE
} JPEG_PARSE_STATE_ENUM;


unsigned int
jpegParserParseImage(unsigned char* srcBuf, unsigned int bufSize, unsigned int *eoiOffset)
{
   unsigned char marker[2] = {0};
   unsigned char *rdPtr = srcBuf;
   unsigned char *endPtr = srcBuf + bufSize;
   unsigned int   parseState = JPEG_PARSE_STATE_WAITING_FOR_SOI;
   unsigned short curSegmentLength;
   unsigned int   offsetOfEncounter100Zeros = 0;
   unsigned char  array100Zeros[100] ={0};

    if (JPEG_MARKER_START_CODE != rdPtr[0] || JPEG_MARKER_SOI != rdPtr[1])
    {
        //invalid file, return error status
        SENSORDB("[4EC] jpegParserParseImage: invalid file ###############\n");
        *eoiOffset = 2560 * 1920;
        return FALSE;
    }



    rdPtr += 636;
    SENSORDB("[4EC] jpegParserParseImage:A (rd,end)=(0x%x,0x%x)\n", rdPtr, endPtr);


    while (rdPtr < endPtr)
    {
        if (JPEG_MARKER_START_CODE == rdPtr[0])
        {
            if (JPEG_MARKER_EOI == rdPtr[1])
            {
                rdPtr += 2;
                SENSORDB("[4EC] jpegParserParseImage B:Encounter FFD9: rdPtr=0x%x\n", rdPtr);
                break;
            }
            rdPtr += 1;
        }
        else
        {
            if ((offsetOfEncounter100Zeros == 0) &&
                ((rdPtr[0] == 0) && (rdPtr[1] == 0)))
            {
                if (!(memcmp(array100Zeros, rdPtr, 100)))
                {
                    SENSORDB("[4EC] jpegParserParseImage:C offsetOfEncounter100Zeros = 1: rdPtr=0x%x\n", rdPtr);
                    offsetOfEncounter100Zeros = rdPtr;
                }
            }
            rdPtr += 1;
        }
    }

    if (rdPtr >= endPtr)
    {
       //the JPEG file miss EOI marker
       rdPtr = offsetOfEncounter100Zeros;
       rdPtr[0] = 0xFF;
       rdPtr[1] = 0xD9;
       rdPtr += 2;
       SENSORDB("[4EC] jpegParserParseImage:D offsetOfEncounter100Zeros = 1: rdPtr=0x%x\n", rdPtr);
    }

    SENSORDB("[4EC] jpegParserParseImage:E Marker:(%x,%x); (rd,end)=(0x%x,0x%x)\n",rdPtr[-2], rdPtr[-1], rdPtr, endPtr);


   *eoiOffset = (rdPtr - srcBuf);
   SENSORDB("[4EC] jpegParserParseImage:eoiOffset =%x\n",*eoiOffset);

   return TRUE;
}



void S5K4ECGX_MIPI_JPEG_Capture_Parser(
     uintptr_t jpegFileAddr, UINT32 jpegMaxBufSize, ACDK_SENSOR_JPEG_INFO *jpeg_info)
{
    unsigned int eoiOffset = 0;
#if defined(__CAPTURE_JPEG_OUTPUT__)

    unsigned int* ptr;
    ptr = (unsigned int*)jpegFileAddr;
    //SENSORDB("[4EC] JPEG_Capture_Parser jpegMaxBufSize=0x%x", jpegMaxBufSize);
    //SENSORDB("[4EC] JPEG_Capture_Parser Buf=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x.", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);

    //To Do: Parsing the header information
    jpegParserParseImage((unsigned char*)jpegFileAddr, jpegMaxBufSize, &eoiOffset);

    jpeg_info->u4FileSize = eoiOffset;
    jpeg_info->u4SrcW = S5K4ECGX_Driver.jpegSensorPara.tgtWidth;
    jpeg_info->u4SrcH = S5K4ECGX_Driver.jpegSensorPara.tgtHeight;

    SENSORDB("[4EC] JPEG_Capture_Parser jpegMaxBufSize=0x%x, W=%d, H=%d\n", jpegMaxBufSize, jpeg_info->u4SrcW, jpeg_info->u4SrcH);
#else
    jpeg_info->u4FileSize = 2560 * 1920 * 1; //bytes//u4FileSize;
    jpeg_info->u4SrcW = S5K4ECGX_Driver.jpegSensorPara.tgtWidth;
    jpeg_info->u4SrcH = S5K4ECGX_Driver.jpegSensorPara.tgtHeight;
#endif

    return;
}


/***********************************************************
     JPEG Sensor End                                      **
***********************************************************/


#define S5K4EC_PREVIEW_MODE             0
#define S5K4EC_VIDEO_MODE               1
#define S5K4EC_PREVIEW_FULLSIZE_MODE    2

#define MIPI_CLK0_SYS_OP_RATE   0x4074
#define MIPI_CLK0_MIN           0x59D8
#define MIPI_CLK0_MAX           0x59D8

#define MIPI_CLK1_SYS_OP_RATE   0x4074 //0x4F1A; //for 15fps capture
#define MIPI_CLK1_MIN           0x59D8
#define MIPI_CLK1_MAX           0x59D8
#define MIPI_CAP_CLK_IDX        1

static void S5K4ECGX_MIPI_Init_Setting(void)
{
    SENSORDB("[4EC] Sensor Init...\n");
    // FOR 4EC EVT1.1
    // ARM Initiation
    //$MIPI[Width:1280,Height:960,Format:YUV422,Lane:2,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:2,DataRate:600]


#if defined(__CAPTURE_JPEG_OUTPUT__)
    #define OUTPUT_FMT  9 //JPEG
#else
    #define OUTPUT_FMT  5 //YUV
#endif
    //S5K4ECGX_Driver.aeState = S5K4ECGX_AE_STATE_UNLOCK;
    S5K4ECGX_Driver.userAskAeLock = 0;//FALSE
    S5K4ECGX_Driver.afMode = S5K4ECGX_AF_MODE_RSVD;
    S5K4ECGX_Driver.afState = S5K4ECGX_AF_STATE_UNINIT;
    S5K4ECGX_Driver.afStateOnOriginalSet = 1;//True
    S5K4ECGX_Driver.Night_Mode = 0;
    S5K4ECGX_Driver.videoFrameRate = 30;
    S5K4ECGX_Driver.sceneMode = SCENE_MODE_OFF;
    S5K4ECGX_Driver.isoSpeed = AE_ISO_100;
    S5K4ECGX_Driver.awbMode = AWB_MODE_AUTO;
    S5K4ECGX_Driver.capExposureTime = 60;
    S5K4ECGX_Driver.aeWindows[0] = 0;
    S5K4ECGX_Driver.aeWindows[1] = 0;
    S5K4ECGX_Driver.aeWindows[2] = 0;
    S5K4ECGX_Driver.aeWindows[3] = 0;
    S5K4ECGX_Driver.jpegSensorPara.tgtWidth = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV;
    S5K4ECGX_Driver.jpegSensorPara.tgtHeight = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV;
    S5K4ECGX_Driver.jpegSensorPara.quality = 100;


    S5K4ECGX_MIPI_sensor_pclk = MIPI_CLK0_MAX * 4000 * 2; //(reg_val * 4k * 2)
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0010, 0x0001);  //S/W Reset
    S5K4ECGX_write_cmos_sensor(0x1030, 0x0000);  //contint_host_int
    S5K4ECGX_write_cmos_sensor(0x0014, 0x0001);  //sw_load_complete - Release CORE (Arm) from reset state
    Sleep(50);

    //==================================================================================
    //02.ETC Setting
    //==================================================================================
    S5K4ECGX_write_cmos_sensor(0x0028, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1082);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //cregs_d0_d4_cd10 //D4[9:8], D3[7:6], D2[5:4], D1[3:2], D0[1:0]
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //cregs_d5_d9_cd10 //D9[9:8], D8[7:6], D7[5:4], D6[3:2], D5[1:0]
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1088);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //cregs_clks_output_cd10 //SDA[11:10], SCL[9:8], PCLK[7:6], VSYNC[3:2], HSYNC[1:0]


    //==================================================================================
    // 03.Analog Setting & ASP Control
    //==================================================================================
    {
        static const kal_uint16 addr_data_pair[] =
        {
        //This register is for FACTORY ONLY.
        //If you change it without prior notification
        //YOU are RESPONSIBLE for the FAILURE that will happen in the future
            0x002A ,0x007A,
            0x0F12 ,0x0000,
            0x002A ,0xE406,
            0x0F12 ,0x0092,
            0x002A ,0xE410,
            0x0F12 ,0x3804, //[15:8]fadlc_filter_co_b, [7:0]fadlc_filter_co_a
            0x002A ,0xE41A,
            0x0F12 ,0x0010,
            0x002A ,0xE420,
            0x0F12 ,0x0003, //adlc_fadlc_filter_refresh
            0x0F12 ,0x0060, //adlc_filter_level_diff_threshold
            0x002A ,0xE42E,
            0x0F12 ,0x0004, //dithered l-ADLC(4bit)
            0x002A ,0xF400,
            0x0F12 ,0x5A3C, //[15:8]stx_width, [7:0]dstx_width
            0x0F12 ,0x0023, //[14]binning_test [13]gain_mode [11:12]row_id [10]cfpn_test [9]pd_pix [8]teg_en, [7]adc_res, [6]smp_en, [5]ldb_en, [4]ld_en, [3]clp_en [2]srx_en, [1]dshut_en, [0]dcds_en
            0x0F12 ,0x8080, //CDS option
            0x0F12 ,0x03AF, //[11:6]rst_mx, [5:0]sig_mx
            0x0F12 ,0x000A, //Avg mode
            0x0F12 ,0xAA54, //x1~x1.49:No MS, x1.5~x3.99:MS2, x4~x16:MS4
            0x0F12 ,0x0040, //RMP option [6]1: RES gain
            0x0F12 ,0x464E, //[14]msoff_en, [13:8]off_rst, [7:0]adc_sat
            0x0F12 ,0x0240, //bist_sig_width_e
            0x0F12 ,0x0240, //bist_sig_width_o
            0x0F12 ,0x0040, //[9]dbs_bist_en, [8:0]bist_rst_width
            0x0F12 ,0x1000, //[15]aac_en, [14]GCLK_DIV2_EN, [13:10]dl_cont [9:8]dbs_mode, [7:0]dbs_option
            0x0F12 ,0x55FF, //bias [15:12]pix, [11:8]pix_bst [7:4]comp2, [3:0]comp1
            0x0F12 ,0xD000, //[15:8]clp_lvl, [7:0]ref_option, [5]pix_bst_en
            0x0F12 ,0x0010, //[7:0]monit
            0x0F12 ,0x0202, //[15:8]dbr_tune_tgsl, [7:0]dbr_tune_pix
            0x0F12 ,0x0401, //[15:8]dbr_tune_ntg, [7:0]dbr_tune_rg
            0x0F12 ,0x0022, //[15:8]reg_option, [7:4]rosc_tune_ncp, [3:0]rosc_tune_cp
            0x0F12 ,0x0088, //PD [8]inrush_ctrl, [7]fblv, [6]reg_ntg, [5]reg_tgsl, [4]reg_rg, [3]reg_pix, [2]ncp_rosc, [1]cp_rosc, [0]cp
            0x0F12 ,0x009F, //[9]capa_ctrl_en, [8:7]fb_lv, [6:5]dbr_clk_sel, [4:0]cp_capa
            0x0F12 ,0x0000, //[15:0]blst_en_cintr
            0x0F12 ,0x1800, //[11]blst_en, [10]rfpn_test, [9]sl_off, [8]tx_off, [7:0]rdv_option
            0x0F12 ,0x0088, //[15:0]pmg_reg_tune
            0x0F12 ,0x0000, //[15:1]analog_dummy, [0]pd_reg_test
            0x0F12 ,0x2428, //[13:11]srx_gap1, [10:8]srx_gap0, [7:0]stx_gap
            0x0F12 ,0x0000, //[0]atx_option
            0x0F12 ,0x03EE, //aig_avg_half
            0x0F12 ,0x0000, //[0]hvs_test_reg
            0x0F12 ,0x0000, //[0]dbus_bist_auto
            0x0F12 ,0x0000, //[7:0]dbr_option
            0x002A ,0xF552,
            0x0F12 ,0x0708, //[7:0]lat_st, [15:8]lat_width
            0x0F12 ,0x080C, //[7:0]hold_st, [15:8]hold_width
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }

    //=================================================================================
    // 04.Trap and Patch  Driver IC DW9714  //update by Chris 20130326
    //=================================================================================
    {
        static const kal_uint16 addr_data_pair[] =
        {
        // Start of Patch data
             0x0028, 0x7000,
             0x002A, 0x3AF8,
             0x0F12 ,0xB5F8,     //  70003AF8
             0x0F12 ,0x4B41,     //  70003AFA
             0x0F12 ,0x4941,     //  70003AFC
             0x0F12 ,0x4842,     //  70003AFE
             0x0F12 ,0x2200,     //  70003B00
             0x0F12 ,0xC008,     //  70003B02
             0x0F12 ,0x6001,     //  70003B04
             0x0F12 ,0x4941,     //  70003B06
             0x0F12 ,0x4841,     //  70003B08
             0x0F12 ,0x2401,     //  70003B0A
             0x0F12 ,0xF000,     //  70003B0C
             0x0F12 ,0xFC5E,     //  70003B0E
             0x0F12 ,0x4940,     //  70003B10
             0x0F12 ,0x4841,     //  70003B12
             0x0F12 ,0x2702,     //  70003B14
             0x0F12 ,0x0022,     //  70003B16
             0x0F12 ,0xF000,     //  70003B18
             0x0F12 ,0xFC58,     //  70003B1A
             0x0F12 ,0x0260,     //  70003B1C
             0x0F12 ,0x4C3F,     //  70003B1E
             0x0F12 ,0x8020,     //  70003B20
             0x0F12 ,0x2600,     //  70003B22
             0x0F12 ,0x8066,     //  70003B24
             0x0F12 ,0x493E,     //  70003B26
             0x0F12 ,0x483E,     //  70003B28
             0x0F12 ,0x6041,     //  70003B2A
             0x0F12 ,0x493E,     //  70003B2C
             0x0F12 ,0x483F,     //  70003B2E
             0x0F12 ,0x003A,     //  70003B30
             0x0F12 ,0x2503,     //  70003B32
             0x0F12 ,0xF000,     //  70003B34
             0x0F12 ,0xFC4A,     //  70003B36
             0x0F12 ,0x483A,     //  70003B38
             0x0F12 ,0x493D,     //  70003B3A
             0x0F12 ,0x30C0,     //  70003B3C
             0x0F12 ,0x63C1,     //  70003B3E
             0x0F12 ,0x4F38,     //  70003B40
             0x0F12 ,0x483C,     //  70003B42
             0x0F12 ,0x3F80,     //  70003B44
             0x0F12 ,0x6438,     //  70003B46
             0x0F12 ,0x483B,     //  70003B48
             0x0F12 ,0x493C,     //  70003B4A
             0x0F12 ,0x6388,     //  70003B4C
             0x0F12 ,0x002A,     //  70003B4E
             0x0F12 ,0x493B,     //  70003B50
             0x0F12 ,0x483C,     //  70003B52
             0x0F12 ,0x2504,     //  70003B54
             0x0F12 ,0xF000,     //  70003B56
             0x0F12 ,0xFC39,     //  70003B58
             0x0F12 ,0x002A,     //  70003B5A
             0x0F12 ,0x493A,     //  70003B5C
             0x0F12 ,0x483B,     //  70003B5E
             0x0F12 ,0x2505,     //  70003B60
             0x0F12 ,0xF000,     //  70003B62
             0x0F12 ,0xF89D,     //  70003B64
             0x0F12 ,0x4839,     //  70003B66
             0x0F12 ,0x002A,     //  70003B68
             0x0F12 ,0x4939,     //  70003B6A
             0x0F12 ,0x2506,     //  70003B6C
             0x0F12 ,0x1D80,     //  70003B6E
             0x0F12 ,0xF000,     //  70003B70
             0x0F12 ,0xF896,     //  70003B72
             0x0F12 ,0x4835,     //  70003B74
             0x0F12 ,0x002A,     //  70003B76
             0x0F12 ,0x4936,     //  70003B78
             0x0F12 ,0x2507,     //  70003B7A
             0x0F12 ,0x300C,     //  70003B7C
             0x0F12 ,0xF000,     //  70003B7E
             0x0F12 ,0xF88F,     //  70003B80
             0x0F12 ,0x4832,     //  70003B82
             0x0F12 ,0x002A,     //  70003B84
             0x0F12 ,0x4934,     //  70003B86
             0x0F12 ,0x2508,     //  70003B88
             0x0F12 ,0x3010,     //  70003B8A
             0x0F12 ,0xF000,     //  70003B8C
             0x0F12 ,0xF888,     //  70003B8E
             0x0F12 ,0x002A,     //  70003B90
             0x0F12 ,0x4932,     //  70003B92
             0x0F12 ,0x4832,     //  70003B94
             0x0F12 ,0x2509,     //  70003B96
             0x0F12 ,0xF000,     //  70003B98
             0x0F12 ,0xFC18,     //  70003B9A
             0x0F12 ,0x002A,     //  70003B9C
             0x0F12 ,0x4931,     //  70003B9E
             0x0F12 ,0x4831,     //  70003BA0
             0x0F12 ,0x250A,     //  70003BA2
             0x0F12 ,0xF000,     //  70003BA4
             0x0F12 ,0xFC12,     //  70003BA6
             0x0F12 ,0x002A,     //  70003BA8
             0x0F12 ,0x4930,     //  70003BAA
             0x0F12 ,0x4830,     //  70003BAC
             0x0F12 ,0x250B,     //  70003BAE
             0x0F12 ,0xF000,     //  70003BB0
             0x0F12 ,0xFC0C,     //  70003BB2
             0x0F12 ,0x002A,     //  70003BB4
             0x0F12 ,0x492F,     //  70003BB6
             0x0F12 ,0x482F,     //  70003BB8
             0x0F12 ,0x250C,     //  70003BBA
             0x0F12 ,0xF000,     //  70003BBC
             0x0F12 ,0xFC06,     //  70003BBE
             0x0F12 ,0x002A,     //  70003BC0
             0x0F12 ,0x492E,     //  70003BC2
             0x0F12 ,0x482E,     //  70003BC4
             0x0F12 ,0x250D,     //  70003BC6
             0x0F12 ,0xF000,     //  70003BC8
             0x0F12 ,0xFC00,     //  70003BCA
             0x0F12 ,0x002A,     //  70003BCC
             0x0F12 ,0x492D,     //  70003BCE
             0x0F12 ,0x482D,     //  70003BD0
             0x0F12 ,0x250E,     //  70003BD2
             0x0F12 ,0xF000,     //  70003BD4
             0x0F12 ,0xFBFA,     //  70003BD6
             0x0F12 ,0x8626,     //  70003BD8
             0x0F12 ,0x20FF,     //  70003BDA
             0x0F12 ,0x1C40,     //  70003BDC
             0x0F12 ,0x8660,     //  70003BDE
             0x0F12 ,0x482A,     //  70003BE0
             0x0F12 ,0x64F8,     //  70003BE2
             0x0F12 ,0x492A,     //  70003BE4
             0x0F12 ,0x482B,     //  70003BE6
             0x0F12 ,0x002A,     //  70003BE8
             0x0F12 ,0x240F,     //  70003BEA
             0x0F12 ,0xF000,     //  70003BEC
             0x0F12 ,0xFBEE,     //  70003BEE
             0x0F12 ,0x4929,     //  70003BF0
             0x0F12 ,0x482A,     //  70003BF2
             0x0F12 ,0x0022,     //  70003BF4
             0x0F12 ,0xF000,     //  70003BF6
             0x0F12 ,0xFBE9,     //  70003BF8
             0x0F12 ,0xBCF8,     //  70003BFA
             0x0F12 ,0xBC08,     //  70003BFC
             0x0F12 ,0x4718,     //  70003BFE
             0x0F12 ,0x017B,     //  70003C00
             0x0F12 ,0x4EC2,     //  70003C02
             0x0F12 ,0x237F,     //  70003C04
             0x0F12 ,0x0000,     //  70003C06
             0x0F12 ,0x1F90,     //  70003C08
             0x0F12 ,0x7000,     //  70003C0A
             0x0F12 ,0x3CB9,     //  70003C0C
             0x0F12 ,0x7000,     //  70003C0E
             0x0F12 ,0xE38B,     //  70003C10
             0x0F12 ,0x0000,     //  70003C12
             0x0F12 ,0x3CF1,     //  70003C14
             0x0F12 ,0x7000,     //  70003C16
             0x0F12 ,0xC3B1,     //  70003C18
             0x0F12 ,0x0000,     //  70003C1A
             0x0F12 ,0x4780,     //  70003C1C
             0x0F12 ,0x7000,     //  70003C1E
             0x0F12 ,0x3D4F,     //  70003C20
             0x0F12 ,0x7000,     //  70003C22
             0x0F12 ,0x0080,     //  70003C24
             0x0F12 ,0x7000,     //  70003C26
             0x0F12 ,0x3D8B,     //  70003C28
             0x0F12 ,0x7000,     //  70003C2A
             0x0F12 ,0xB49D,     //  70003C2C
             0x0F12 ,0x0000,     //  70003C2E
             0x0F12 ,0x3E37,     //  70003C30
             0x0F12 ,0x7000,     //  70003C32
             0x0F12 ,0x3DEB,     //  70003C34
             0x0F12 ,0x7000,     //  70003C36
             0x0F12 ,0xFFFF,     //  70003C38
             0x0F12 ,0x00FF,     //  70003C3A
             0x0F12 ,0x17E0,     //  70003C3C
             0x0F12 ,0x7000,     //  70003C3E
             0x0F12 ,0x3FB3,     //  70003C40
             0x0F12 ,0x7000,     //  70003C42
             0x0F12 ,0x053D,     //  70003C44
             0x0F12 ,0x0000,     //  70003C46
             0x0F12 ,0x0000,     //  70003C48
             0x0F12 ,0x0A89,     //  70003C4A
             0x0F12 ,0x6CD2,     //  70003C4C
             0x0F12 ,0x0000,     //  70003C4E
             0x0F12 ,0x02C9,     //  70003C50
             0x0F12 ,0x0000,     //  70003C52
             0x0F12 ,0x0000,     //  70003C54
             0x0F12 ,0x0A9A,     //  70003C56
             0x0F12 ,0x0000,     //  70003C58
             0x0F12 ,0x02D2,     //  70003C5A
             0x0F12 ,0x4001,     //  70003C5C
             0x0F12 ,0x7000,     //  70003C5E
             0x0F12 ,0x9E65,     //  70003C60
             0x0F12 ,0x0000,     //  70003C62
             0x0F12 ,0x4075,     //  70003C64
             0x0F12 ,0x7000,     //  70003C66
             0x0F12 ,0x7C49,     //  70003C68
             0x0F12 ,0x0000,     //  70003C6A
             0x0F12 ,0x40E9,     //  70003C6C
             0x0F12 ,0x7000,     //  70003C6E
             0x0F12 ,0x7C63,     //  70003C70
             0x0F12 ,0x0000,     //  70003C72
             0x0F12 ,0x4105,     //  70003C74
             0x0F12 ,0x7000,     //  70003C76
             0x0F12 ,0x8F01,     //  70003C78
             0x0F12 ,0x0000,     //  70003C7A
             0x0F12 ,0x41A7,     //  70003C7C
             0x0F12 ,0x7000,     //  70003C7E
             0x0F12 ,0x7F3F,     //  70003C80
             0x0F12 ,0x0000,     //  70003C82
             0x0F12 ,0x4235,     //  70003C84
             0x0F12 ,0x7000,     //  70003C86
             0x0F12 ,0x98C5,     //  70003C88
             0x0F12 ,0x0000,     //  70003C8A
             0x0F12 ,0x42FB,     //  70003C8C
             0x0F12 ,0x7000,     //  70003C8E
             0x0F12 ,0x4351,     //  70003C90
             0x0F12 ,0x7000,     //  70003C92
             0x0F12 ,0xA70B,     //  70003C94
             0x0F12 ,0x0000,     //  70003C96
             0x0F12 ,0x4373,     //  70003C98
             0x0F12 ,0x7000,     //  70003C9A
             0x0F12 ,0x400D,     //  70003C9C
             0x0F12 ,0x0000,     //  70003C9E
             0x0F12 ,0xB570,     //  70003CA0
             0x0F12 ,0x000C,     //  70003CA2
             0x0F12 ,0x0015,     //  70003CA4
             0x0F12 ,0x0029,     //  70003CA6
             0x0F12 ,0xF000,     //  70003CA8
             0x0F12 ,0xFB98,     //  70003CAA
             0x0F12 ,0x49F8,     //  70003CAC
             0x0F12 ,0x00A8,     //  70003CAE
             0x0F12 ,0x500C,     //  70003CB0
             0x0F12 ,0xBC70,     //  70003CB2
             0x0F12 ,0xBC08,     //  70003CB4
             0x0F12 ,0x4718,     //  70003CB6
             0x0F12 ,0x6808,     //  70003CB8
             0x0F12 ,0x0400,     //  70003CBA
             0x0F12 ,0x0C00,     //  70003CBC
             0x0F12 ,0x6849,     //  70003CBE
             0x0F12 ,0x0409,     //  70003CC0
             0x0F12 ,0x0C09,     //  70003CC2
             0x0F12 ,0x4AF3,     //  70003CC4
             0x0F12 ,0x8992,     //  70003CC6
             0x0F12 ,0x2A00,     //  70003CC8
             0x0F12 ,0xD00D,     //  70003CCA
             0x0F12 ,0x2300,     //  70003CCC
             0x0F12 ,0x1A89,     //  70003CCE
             0x0F12 ,0xD400,     //  70003CD0
             0x0F12 ,0x000B,     //  70003CD2
             0x0F12 ,0x0419,     //  70003CD4
             0x0F12 ,0x0C09,     //  70003CD6
             0x0F12 ,0x23FF,     //  70003CD8
             0x0F12 ,0x33C1,     //  70003CDA
             0x0F12 ,0x1810,     //  70003CDC
             0x0F12 ,0x4298,     //  70003CDE
             0x0F12 ,0xD800,     //  70003CE0
             0x0F12 ,0x0003,     //  70003CE2
             0x0F12 ,0x0418,     //  70003CE4
             0x0F12 ,0x0C00,     //  70003CE6
             0x0F12 ,0x4AEB,     //  70003CE8
             0x0F12 ,0x8150,     //  70003CEA
             0x0F12 ,0x8191,     //  70003CEC
             0x0F12 ,0x4770,     //  70003CEE
             0x0F12 ,0xB5F3,     //  70003CF0
             0x0F12 ,0x0004,     //  70003CF2
             0x0F12 ,0xB081,     //  70003CF4
             0x0F12 ,0x9802,     //  70003CF6
             0x0F12 ,0x6800,     //  70003CF8
             0x0F12 ,0x0600,     //  70003CFA
             0x0F12 ,0x0E00,     //  70003CFC
             0x0F12 ,0x2201,     //  70003CFE
             0x0F12 ,0x0015,     //  70003D00
             0x0F12 ,0x0021,     //  70003D02
             0x0F12 ,0x3910,     //  70003D04
             0x0F12 ,0x408A,     //  70003D06
             0x0F12 ,0x40A5,     //  70003D08
             0x0F12 ,0x4FE4,     //  70003D0A
             0x0F12 ,0x0016,     //  70003D0C
             0x0F12 ,0x2C10,     //  70003D0E
             0x0F12 ,0xDA03,     //  70003D10
             0x0F12 ,0x8839,     //  70003D12
             0x0F12 ,0x43A9,     //  70003D14
             0x0F12 ,0x8039,     //  70003D16
             0x0F12 ,0xE002,     //  70003D18
             0x0F12 ,0x8879,     //  70003D1A
             0x0F12 ,0x43B1,     //  70003D1C
             0x0F12 ,0x8079,     //  70003D1E
             0x0F12 ,0xF000,     //  70003D20
             0x0F12 ,0xFB64,     //  70003D22
             0x0F12 ,0x2C10,     //  70003D24
             0x0F12 ,0xDA03,     //  70003D26
             0x0F12 ,0x8839,     //  70003D28
             0x0F12 ,0x4329,     //  70003D2A
             0x0F12 ,0x8039,     //  70003D2C
             0x0F12 ,0xE002,     //  70003D2E
             0x0F12 ,0x8879,     //  70003D30
             0x0F12 ,0x4331,     //  70003D32
             0x0F12 ,0x8079,     //  70003D34
             0x0F12 ,0x49DA,     //  70003D36
             0x0F12 ,0x8809,     //  70003D38
             0x0F12 ,0x2900,     //  70003D3A
             0x0F12 ,0xD102,     //  70003D3C
             0x0F12 ,0xF000,     //  70003D3E
             0x0F12 ,0xFB5D,     //  70003D40
             0x0F12 ,0x2000,     //  70003D42
             0x0F12 ,0x9902,     //  70003D44
             0x0F12 ,0x6008,     //  70003D46
             0x0F12 ,0xBCFE,     //  70003D48
             0x0F12 ,0xBC08,     //  70003D4A
             0x0F12 ,0x4718,     //  70003D4C
             0x0F12 ,0xB538,     //  70003D4E
             0x0F12 ,0x9C04,     //  70003D50
             0x0F12 ,0x0015,     //  70003D52
             0x0F12 ,0x002A,     //  70003D54
             0x0F12 ,0x9400,     //  70003D56
             0x0F12 ,0xF000,     //  70003D58
             0x0F12 ,0xFB58,     //  70003D5A
             0x0F12 ,0x4AD1,     //  70003D5C
             0x0F12 ,0x8811,     //  70003D5E
             0x0F12 ,0x2900,     //  70003D60
             0x0F12 ,0xD00F,     //  70003D62
             0x0F12 ,0x8820,     //  70003D64
             0x0F12 ,0x4281,     //  70003D66
             0x0F12 ,0xD20C,     //  70003D68
             0x0F12 ,0x8861,     //  70003D6A
             0x0F12 ,0x8853,     //  70003D6C
             0x0F12 ,0x4299,     //  70003D6E
             0x0F12 ,0xD200,     //  70003D70
             0x0F12 ,0x1E40,     //  70003D72
             0x0F12 ,0x0400,     //  70003D74
             0x0F12 ,0x0C00,     //  70003D76
             0x0F12 ,0x8020,     //  70003D78
             0x0F12 ,0x8851,     //  70003D7A
             0x0F12 ,0x8061,     //  70003D7C
             0x0F12 ,0x4368,     //  70003D7E
             0x0F12 ,0x1840,     //  70003D80
             0x0F12 ,0x6060,     //  70003D82
             0x0F12 ,0xBC38,     //  70003D84
             0x0F12 ,0xBC08,     //  70003D86
             0x0F12 ,0x4718,     //  70003D88
             0x0F12 ,0xB5F8,     //  70003D8A
             0x0F12 ,0x0004,     //  70003D8C
             0x0F12 ,0x6808,     //  70003D8E
             0x0F12 ,0x0400,     //  70003D90
             0x0F12 ,0x0C00,     //  70003D92
             0x0F12 ,0x2201,     //  70003D94
             0x0F12 ,0x0015,     //  70003D96
             0x0F12 ,0x0021,     //  70003D98
             0x0F12 ,0x3910,     //  70003D9A
             0x0F12 ,0x408A,     //  70003D9C
             0x0F12 ,0x40A5,     //  70003D9E
             0x0F12 ,0x4FBE,     //  70003DA0
             0x0F12 ,0x0016,     //  70003DA2
             0x0F12 ,0x2C10,     //  70003DA4
             0x0F12 ,0xDA03,     //  70003DA6
             0x0F12 ,0x8839,     //  70003DA8
             0x0F12 ,0x43A9,     //  70003DAA
             0x0F12 ,0x8039,     //  70003DAC
             0x0F12 ,0xE002,     //  70003DAE
             0x0F12 ,0x8879,     //  70003DB0
             0x0F12 ,0x43B1,     //  70003DB2
             0x0F12 ,0x8079,     //  70003DB4
             0x0F12 ,0xF000,     //  70003DB6
             0x0F12 ,0xFB31,     //  70003DB8
             0x0F12 ,0x2C10,     //  70003DBA
             0x0F12 ,0xDA03,     //  70003DBC
             0x0F12 ,0x8838,     //  70003DBE
             0x0F12 ,0x4328,     //  70003DC0
             0x0F12 ,0x8038,     //  70003DC2
             0x0F12 ,0xE002,     //  70003DC4
             0x0F12 ,0x8878,     //  70003DC6
             0x0F12 ,0x4330,     //  70003DC8
             0x0F12 ,0x8078,     //  70003DCA
             0x0F12 ,0x48B6,     //  70003DCC
             0x0F12 ,0x8800,     //  70003DCE
             0x0F12 ,0x0400,     //  70003DD0
             0x0F12 ,0xD507,     //  70003DD2
             0x0F12 ,0x4BB5,     //  70003DD4
             0x0F12 ,0x7819,     //  70003DD6
             0x0F12 ,0x4AB5,     //  70003DD8
             0x0F12 ,0x7810,     //  70003DDA
             0x0F12 ,0x7018,     //  70003DDC
             0x0F12 ,0x7011,     //  70003DDE
             0x0F12 ,0x49B4,     //  70003DE0
             0x0F12 ,0x8188,     //  70003DE2
             0x0F12 ,0xBCF8,     //  70003DE4
             0x0F12 ,0xBC08,     //  70003DE6
             0x0F12 ,0x4718,     //  70003DE8
             0x0F12 ,0xB538,     //  70003DEA
             0x0F12 ,0x48B2,     //  70003DEC
             0x0F12 ,0x4669,     //  70003DEE
             0x0F12 ,0xF000,     //  70003DF0
             0x0F12 ,0xFB1C,     //  70003DF2
             0x0F12 ,0x48B1,     //  70003DF4
             0x0F12 ,0x49B0,     //  70003DF6
             0x0F12 ,0x69C2,     //  70003DF8
             0x0F12 ,0x2400,     //  70003DFA
             0x0F12 ,0x31A8,     //  70003DFC
             0x0F12 ,0x2A00,     //  70003DFE
             0x0F12 ,0xD008,     //  70003E00
             0x0F12 ,0x61C4,     //  70003E02
             0x0F12 ,0x684A,     //  70003E04
             0x0F12 ,0x6242,     //  70003E06
             0x0F12 ,0x6282,     //  70003E08
             0x0F12 ,0x466B,     //  70003E0A
             0x0F12 ,0x881A,     //  70003E0C
             0x0F12 ,0x6302,     //  70003E0E
             0x0F12 ,0x885A,     //  70003E10
             0x0F12 ,0x6342,     //  70003E12
             0x0F12 ,0x6A02,     //  70003E14
             0x0F12 ,0x2A00,     //  70003E16
             0x0F12 ,0xD00A,     //  70003E18
             0x0F12 ,0x6204,     //  70003E1A
             0x0F12 ,0x6849,     //  70003E1C
             0x0F12 ,0x6281,     //  70003E1E
             0x0F12 ,0x466B,     //  70003E20
             0x0F12 ,0x8819,     //  70003E22
             0x0F12 ,0x6301,     //  70003E24
             0x0F12 ,0x8859,     //  70003E26
             0x0F12 ,0x6341,     //  70003E28
             0x0F12 ,0x49A5,     //  70003E2A
             0x0F12 ,0x88C9,     //  70003E2C
             0x0F12 ,0x63C1,     //  70003E2E
             0x0F12 ,0xF000,     //  70003E30
             0x0F12 ,0xFB04,     //  70003E32
             0x0F12 ,0xE7A6,     //  70003E34
             0x0F12 ,0xB5F0,     //  70003E36
             0x0F12 ,0xB08B,     //  70003E38
             0x0F12 ,0x20FF,     //  70003E3A
             0x0F12 ,0x1C40,     //  70003E3C
             0x0F12 ,0x49A1,     //  70003E3E
             0x0F12 ,0x89CC,     //  70003E40
             0x0F12 ,0x4E9E,     //  70003E42
             0x0F12 ,0x6AB1,     //  70003E44
             0x0F12 ,0x4284,     //  70003E46
             0x0F12 ,0xD101,     //  70003E48
             0x0F12 ,0x489F,     //  70003E4A
             0x0F12 ,0x6081,     //  70003E4C
             0x0F12 ,0x6A70,     //  70003E4E
             0x0F12 ,0x0200,     //  70003E50
             0x0F12 ,0xF000,     //  70003E52
             0x0F12 ,0xFAFB,     //  70003E54
             0x0F12 ,0x0400,     //  70003E56
             0x0F12 ,0x0C00,     //  70003E58
             0x0F12 ,0x4A96,     //  70003E5A
             0x0F12 ,0x8A11,     //  70003E5C
             0x0F12 ,0x9109,     //  70003E5E
             0x0F12 ,0x2101,     //  70003E60
             0x0F12 ,0x0349,     //  70003E62
             0x0F12 ,0x4288,     //  70003E64
             0x0F12 ,0xD200,     //  70003E66
             0x0F12 ,0x0001,     //  70003E68
             0x0F12 ,0x4A92,     //  70003E6A
             0x0F12 ,0x8211,     //  70003E6C
             0x0F12 ,0x4D97,     //  70003E6E
             0x0F12 ,0x8829,     //  70003E70
             0x0F12 ,0x9108,     //  70003E72
             0x0F12 ,0x4A8B,     //  70003E74
             0x0F12 ,0x2303,     //  70003E76
             0x0F12 ,0x3222,     //  70003E78
             0x0F12 ,0x1F91,     //  70003E7A
             0x0F12 ,0xF000,     //  70003E7C
             0x0F12 ,0xFAEC,     //  70003E7E
             0x0F12 ,0x8028,     //  70003E80
             0x0F12 ,0x488E,     //  70003E82
             0x0F12 ,0x4987,     //  70003E84
             0x0F12 ,0x6BC2,     //  70003E86
             0x0F12 ,0x6AC0,     //  70003E88
             0x0F12 ,0x4282,     //  70003E8A
             0x0F12 ,0xD201,     //  70003E8C
             0x0F12 ,0x8CC8,     //  70003E8E
             0x0F12 ,0x8028,     //  70003E90
             0x0F12 ,0x88E8,     //  70003E92
             0x0F12 ,0x9007,     //  70003E94
             0x0F12 ,0x2240,     //  70003E96
             0x0F12 ,0x4310,     //  70003E98
             0x0F12 ,0x80E8,     //  70003E9A
             0x0F12 ,0x2000,     //  70003E9C
             0x0F12 ,0x0041,     //  70003E9E
             0x0F12 ,0x194B,     //  70003EA0
             0x0F12 ,0x001E,     //  70003EA2
             0x0F12 ,0x3680,     //  70003EA4
             0x0F12 ,0x8BB2,     //  70003EA6
             0x0F12 ,0xAF04,     //  70003EA8
             0x0F12 ,0x527A,     //  70003EAA
             0x0F12 ,0x4A7D,     //  70003EAC
             0x0F12 ,0x188A,     //  70003EAE
             0x0F12 ,0x8897,     //  70003EB0
             0x0F12 ,0x83B7,     //  70003EB2
             0x0F12 ,0x33A0,     //  70003EB4
             0x0F12 ,0x891F,     //  70003EB6
             0x0F12 ,0xAE01,     //  70003EB8
             0x0F12 ,0x5277,     //  70003EBA
             0x0F12 ,0x8A11,     //  70003EBC
             0x0F12 ,0x8119,     //  70003EBE
             0x0F12 ,0x1C40,     //  70003EC0
             0x0F12 ,0x0400,     //  70003EC2
             0x0F12 ,0x0C00,     //  70003EC4
             0x0F12 ,0x2806,     //  70003EC6
             0x0F12 ,0xD3E9,     //  70003EC8
             0x0F12 ,0xF000,     //  70003ECA
             0x0F12 ,0xFACD,     //  70003ECC
             0x0F12 ,0xF000,     //  70003ECE
             0x0F12 ,0xFAD3,     //  70003ED0
             0x0F12 ,0x4F79,     //  70003ED2
             0x0F12 ,0x37A8,     //  70003ED4
             0x0F12 ,0x2800,     //  70003ED6
             0x0F12 ,0xD10A,     //  70003ED8
             0x0F12 ,0x1FE0,     //  70003EDA
             0x0F12 ,0x38FD,     //  70003EDC
             0x0F12 ,0xD001,     //  70003EDE
             0x0F12 ,0x1CC0,     //  70003EE0
             0x0F12 ,0xD105,     //  70003EE2
             0x0F12 ,0x4874,     //  70003EE4
             0x0F12 ,0x8829,     //  70003EE6
             0x0F12 ,0x3818,     //  70003EE8
             0x0F12 ,0x6840,     //  70003EEA
             0x0F12 ,0x4348,     //  70003EEC
             0x0F12 ,0x6078,     //  70003EEE
             0x0F12 ,0x4972,     //  70003EF0
             0x0F12 ,0x6878,     //  70003EF2
             0x0F12 ,0x6B89,     //  70003EF4
             0x0F12 ,0x4288,     //  70003EF6
             0x0F12 ,0xD300,     //  70003EF8
             0x0F12 ,0x0008,     //  70003EFA
             0x0F12 ,0x6078,     //  70003EFC
             0x0F12 ,0x2000,     //  70003EFE
             0x0F12 ,0x0041,     //  70003F00
             0x0F12 ,0xAA04,     //  70003F02
             0x0F12 ,0x5A53,     //  70003F04
             0x0F12 ,0x194A,     //  70003F06
             0x0F12 ,0x269C,     //  70003F08
             0x0F12 ,0x52B3,     //  70003F0A
             0x0F12 ,0xAB01,     //  70003F0C
             0x0F12 ,0x5A59,     //  70003F0E
             0x0F12 ,0x32A0,     //  70003F10
             0x0F12 ,0x8111,     //  70003F12
             0x0F12 ,0x1C40,     //  70003F14
             0x0F12 ,0x0400,     //  70003F16
             0x0F12 ,0x0C00,     //  70003F18
             0x0F12 ,0x2806,     //  70003F1A
             0x0F12 ,0xD3F0,     //  70003F1C
             0x0F12 ,0x4965,     //  70003F1E
             0x0F12 ,0x9809,     //  70003F20
             0x0F12 ,0x8208,     //  70003F22
             0x0F12 ,0x9808,     //  70003F24
             0x0F12 ,0x8028,     //  70003F26
             0x0F12 ,0x9807,     //  70003F28
             0x0F12 ,0x80E8,     //  70003F2A
             0x0F12 ,0x1FE0,     //  70003F2C
             0x0F12 ,0x38FD,     //  70003F2E
             0x0F12 ,0xD13B,     //  70003F30
             0x0F12 ,0x4D64,     //  70003F32
             0x0F12 ,0x89E8,     //  70003F34
             0x0F12 ,0x1FC1,     //  70003F36
             0x0F12 ,0x39FF,     //  70003F38
             0x0F12 ,0xD136,     //  70003F3A
             0x0F12 ,0x4C5F,     //  70003F3C
             0x0F12 ,0x8AE0,     //  70003F3E
             0x0F12 ,0xF000,     //  70003F40
             0x0F12 ,0xFAA2,     //  70003F42
             0x0F12 ,0x0006,     //  70003F44
             0x0F12 ,0x8B20,     //  70003F46
             0x0F12 ,0xF000,     //  70003F48
             0x0F12 ,0xFAA6,     //  70003F4A
             0x0F12 ,0x9000,     //  70003F4C
             0x0F12 ,0x6AA1,     //  70003F4E
             0x0F12 ,0x6878,     //  70003F50
             0x0F12 ,0x1809,     //  70003F52
             0x0F12 ,0x0200,     //  70003F54
             0x0F12 ,0xF000,     //  70003F56
             0x0F12 ,0xFA79,     //  70003F58
             0x0F12 ,0x0400,     //  70003F5A
             0x0F12 ,0x0C00,     //  70003F5C
             0x0F12 ,0x0022,     //  70003F5E
             0x0F12 ,0x3246,     //  70003F60
             0x0F12 ,0x0011,     //  70003F62
             0x0F12 ,0x310A,     //  70003F64
             0x0F12 ,0x2305,     //  70003F66
             0x0F12 ,0xF000,     //  70003F68
             0x0F12 ,0xFA76,     //  70003F6A
             0x0F12 ,0x66E8,     //  70003F6C
             0x0F12 ,0x6B23,     //  70003F6E
             0x0F12 ,0x0002,     //  70003F70
             0x0F12 ,0x0031,     //  70003F72
             0x0F12 ,0x0018,     //  70003F74
             0x0F12 ,0xF000,     //  70003F76
             0x0F12 ,0xFA97,     //  70003F78
             0x0F12 ,0x466B,     //  70003F7A
             0x0F12 ,0x8518,     //  70003F7C
             0x0F12 ,0x6EEA,     //  70003F7E
             0x0F12 ,0x6B60,     //  70003F80
             0x0F12 ,0x9900,     //  70003F82
             0x0F12 ,0xF000,     //  70003F84
             0x0F12 ,0xFA90,     //  70003F86
             0x0F12 ,0x466B,     //  70003F88
             0x0F12 ,0x8558,     //  70003F8A
             0x0F12 ,0x0029,     //  70003F8C
             0x0F12 ,0x980A,     //  70003F8E
             0x0F12 ,0x3170,     //  70003F90
             0x0F12 ,0xF000,     //  70003F92
             0x0F12 ,0xFA91,     //  70003F94
             0x0F12 ,0x0028,     //  70003F96
             0x0F12 ,0x3060,     //  70003F98
             0x0F12 ,0x8A02,     //  70003F9A
             0x0F12 ,0x4946,     //  70003F9C
             0x0F12 ,0x3128,     //  70003F9E
             0x0F12 ,0x808A,     //  70003FA0
             0x0F12 ,0x8A42,     //  70003FA2
             0x0F12 ,0x80CA,     //  70003FA4
             0x0F12 ,0x8A80,     //  70003FA6
             0x0F12 ,0x8108,     //  70003FA8
             0x0F12 ,0xB00B,     //  70003FAA
             0x0F12 ,0xBCF0,     //  70003FAC
             0x0F12 ,0xBC08,     //  70003FAE
             0x0F12 ,0x4718,     //  70003FB0
             0x0F12 ,0xB570,     //  70003FB2
             0x0F12 ,0x2400,     //  70003FB4
             0x0F12 ,0x4D46,     //  70003FB6
             0x0F12 ,0x4846,     //  70003FB8
             0x0F12 ,0x8881,     //  70003FBA
             0x0F12 ,0x4846,     //  70003FBC
             0x0F12 ,0x8041,     //  70003FBE
             0x0F12 ,0x2101,     //  70003FC0
             0x0F12 ,0x8001,     //  70003FC2
             0x0F12 ,0xF000,     //  70003FC4
             0x0F12 ,0xFA80,     //  70003FC6
             0x0F12 ,0x4842,     //  70003FC8
             0x0F12 ,0x3820,     //  70003FCA
             0x0F12 ,0x8BC0,     //  70003FCC
             0x0F12 ,0xF000,     //  70003FCE
             0x0F12 ,0xFA83,     //  70003FD0
             0x0F12 ,0x4B42,     //  70003FD2
             0x0F12 ,0x220D,     //  70003FD4
             0x0F12 ,0x0712,     //  70003FD6
             0x0F12 ,0x18A8,     //  70003FD8
             0x0F12 ,0x8806,     //  70003FDA
             0x0F12 ,0x00E1,     //  70003FDC
             0x0F12 ,0x18C9,     //  70003FDE
             0x0F12 ,0x81CE,     //  70003FE0
             0x0F12 ,0x8846,     //  70003FE2
             0x0F12 ,0x818E,     //  70003FE4
             0x0F12 ,0x8886,     //  70003FE6
             0x0F12 ,0x824E,     //  70003FE8
             0x0F12 ,0x88C0,     //  70003FEA
             0x0F12 ,0x8208,     //  70003FEC
             0x0F12 ,0x3508,     //  70003FEE
             0x0F12 ,0x042D,     //  70003FF0
             0x0F12 ,0x0C2D,     //  70003FF2
             0x0F12 ,0x1C64,     //  70003FF4
             0x0F12 ,0x0424,     //  70003FF6
             0x0F12 ,0x0C24,     //  70003FF8
             0x0F12 ,0x2C07,     //  70003FFA
             0x0F12 ,0xD3EC,     //  70003FFC
             0x0F12 ,0xE658,     //  70003FFE
             0x0F12 ,0xB510,     //  70004000
             0x0F12 ,0x4834,     //  70004002
             0x0F12 ,0x4C34,     //  70004004
             0x0F12 ,0x88C0,     //  70004006
             0x0F12 ,0x8060,     //  70004008
             0x0F12 ,0x2001,     //  7000400A
             0x0F12 ,0x8020,     //  7000400C
             0x0F12 ,0x4831,     //  7000400E
             0x0F12 ,0x3820,     //  70004010
             0x0F12 ,0x8BC0,     //  70004012
             0x0F12 ,0xF000,     //  70004014
             0x0F12 ,0xFA60,     //  70004016
             0x0F12 ,0x88E0,     //  70004018
             0x0F12 ,0x4A31,     //  7000401A
             0x0F12 ,0x2800,     //  7000401C
             0x0F12 ,0xD003,     //  7000401E
             0x0F12 ,0x4930,     //  70004020
             0x0F12 ,0x8849,     //  70004022
             0x0F12 ,0x2900,     //  70004024
             0x0F12 ,0xD009,     //  70004026
             0x0F12 ,0x2001,     //  70004028
             0x0F12 ,0x03C0,     //  7000402A
             0x0F12 ,0x8050,     //  7000402C
             0x0F12 ,0x80D0,     //  7000402E
             0x0F12 ,0x2000,     //  70004030
             0x0F12 ,0x8090,     //  70004032
             0x0F12 ,0x8110,     //  70004034
             0x0F12 ,0xBC10,     //  70004036
             0x0F12 ,0xBC08,     //  70004038
             0x0F12 ,0x4718,     //  7000403A
             0x0F12 ,0x8050,     //  7000403C
             0x0F12 ,0x8920,     //  7000403E
             0x0F12 ,0x80D0,     //  70004040
             0x0F12 ,0x8960,     //  70004042
             0x0F12 ,0x0400,     //  70004044
             0x0F12 ,0x1400,     //  70004046
             0x0F12 ,0x8090,     //  70004048
             0x0F12 ,0x89A1,     //  7000404A
             0x0F12 ,0x0409,     //  7000404C
             0x0F12 ,0x1409,     //  7000404E
             0x0F12 ,0x8111,     //  70004050
             0x0F12 ,0x89E3,     //  70004052
             0x0F12 ,0x8A24,     //  70004054
             0x0F12 ,0x2B00,     //  70004056
             0x0F12 ,0xD104,     //  70004058
             0x0F12 ,0x17C3,     //  7000405A
             0x0F12 ,0x0F5B,     //  7000405C
             0x0F12 ,0x1818,     //  7000405E
             0x0F12 ,0x10C0,     //  70004060
             0x0F12 ,0x8090,     //  70004062
             0x0F12 ,0x2C00,     //  70004064
             0x0F12 ,0xD1E6,     //  70004066
             0x0F12 ,0x17C8,     //  70004068
             0x0F12 ,0x0F40,     //  7000406A
             0x0F12 ,0x1840,     //  7000406C
             0x0F12 ,0x10C0,     //  7000406E
             0x0F12 ,0x8110,     //  70004070
             0x0F12 ,0xE7E0,     //  70004072
             0x0F12 ,0xB510,     //  70004074
             0x0F12 ,0x000C,     //  70004076
             0x0F12 ,0x4919,     //  70004078
             0x0F12 ,0x2204,     //  7000407A
             0x0F12 ,0x6820,     //  7000407C
             0x0F12 ,0x5E8A,     //  7000407E
             0x0F12 ,0x0140,     //  70004080
             0x0F12 ,0x1A80,     //  70004082
             0x0F12 ,0x0280,     //  70004084
             0x0F12 ,0x8849,     //  70004086
             0x0F12 ,0xF000,     //  70004088
             0x0F12 ,0xFA2E,     //  7000408A
             0x0F12 ,0x6020,     //  7000408C
             0x0F12 ,0xE7D2,     //  7000408E
             0x0F12 ,0x38D4,     //  70004090
             0x0F12 ,0x7000,     //  70004092
             0x0F12 ,0x17D0,     //  70004094
             0x0F12 ,0x7000,     //  70004096
             0x0F12 ,0x5000,     //  70004098
             0x0F12 ,0xD000,     //  7000409A
             0x0F12 ,0x1100,     //  7000409C
             0x0F12 ,0xD000,     //  7000409E
             0x0F12 ,0x171A,     //  700040A0
             0x0F12 ,0x7000,     //  700040A2
             0x0F12 ,0x4780,     //  700040A4
             0x0F12 ,0x7000,     //  700040A6
             0x0F12 ,0x2FCA,     //  700040A8
             0x0F12 ,0x7000,     //  700040AA
             0x0F12 ,0x2FC5,     //  700040AC
             0x0F12 ,0x7000,     //  700040AE
             0x0F12 ,0x2FC6,     //  700040B0
             0x0F12 ,0x7000,     //  700040B2
             0x0F12 ,0x2ED8,     //  700040B4
             0x0F12 ,0x7000,     //  700040B6
             0x0F12 ,0x2BD0,     //  700040B8
             0x0F12 ,0x7000,     //  700040BA
             0x0F12 ,0x17E0,     //  700040BC
             0x0F12 ,0x7000,     //  700040BE
             0x0F12 ,0x2DE8,     //  700040C0
             0x0F12 ,0x7000,     //  700040C2
             0x0F12 ,0x37E0,     //  700040C4
             0x0F12 ,0x7000,     //  700040C6
             0x0F12 ,0x210C,     //  700040C8
             0x0F12 ,0x7000,     //  700040CA
             0x0F12 ,0x1484,     //  700040CC
             0x0F12 ,0x7000,     //  700040CE
             0x0F12 ,0xA006,     //  700040D0
             0x0F12 ,0x0000,     //  700040D2
             0x0F12 ,0x0724,     //  700040D4
             0x0F12 ,0x7000,     //  700040D6
             0x0F12 ,0xA000,     //  700040D8
             0x0F12 ,0xD000,     //  700040DA
             0x0F12 ,0x2270,     //  700040DC
             0x0F12 ,0x7000,     //  700040DE
             0x0F12 ,0x2558,     //  700040E0
             0x0F12 ,0x7000,     //  700040E2
             0x0F12 ,0x146C,     //  700040E4
             0x0F12 ,0x7000,     //  700040E6
             0x0F12 ,0xB510,     //  700040E8
             0x0F12 ,0x000C,     //  700040EA
             0x0F12 ,0x49AC,     //  700040EC
             0x0F12 ,0x2208,     //  700040EE
             0x0F12 ,0x6820,     //  700040F0
             0x0F12 ,0x5E8A,     //  700040F2
             0x0F12 ,0x0140,     //  700040F4
             0x0F12 ,0x1A80,     //  700040F6
             0x0F12 ,0x0280,     //  700040F8
             0x0F12 ,0x88C9,     //  700040FA
             0x0F12 ,0xF000,     //  700040FC
             0x0F12 ,0xF9F4,     //  700040FE
             0x0F12 ,0x6020,     //  70004100
             0x0F12 ,0xE798,     //  70004102
             0x0F12 ,0xB5FE,     //  70004104
             0x0F12 ,0x000C,     //  70004106
             0x0F12 ,0x6825,     //  70004108
             0x0F12 ,0x6866,     //  7000410A
             0x0F12 ,0x68A0,     //  7000410C
             0x0F12 ,0x9001,     //  7000410E
             0x0F12 ,0x68E7,     //  70004110
             0x0F12 ,0x1BA8,     //  70004112
             0x0F12 ,0x42B5,     //  70004114
             0x0F12 ,0xDA00,     //  70004116
             0x0F12 ,0x1B70,     //  70004118
             0x0F12 ,0x9000,     //  7000411A
             0x0F12 ,0x49A0,     //  7000411C
             0x0F12 ,0x48A1,     //  7000411E
             0x0F12 ,0x884A,     //  70004120
             0x0F12 ,0x8843,     //  70004122
             0x0F12 ,0x435A,     //  70004124
             0x0F12 ,0x2304,     //  70004126
             0x0F12 ,0x5ECB,     //  70004128
             0x0F12 ,0x0A92,     //  7000412A
             0x0F12 ,0x18D2,     //  7000412C
             0x0F12 ,0x02D2,     //  7000412E
             0x0F12 ,0x0C12,     //  70004130
             0x0F12 ,0x88CB,     //  70004132
             0x0F12 ,0x8880,     //  70004134
             0x0F12 ,0x4343,     //  70004136
             0x0F12 ,0x0A98,     //  70004138
             0x0F12 ,0x2308,     //  7000413A
             0x0F12 ,0x5ECB,     //  7000413C
             0x0F12 ,0x18C0,     //  7000413E
             0x0F12 ,0x02C0,     //  70004140
             0x0F12 ,0x0C00,     //  70004142
             0x0F12 ,0x0411,     //  70004144
             0x0F12 ,0x0400,     //  70004146
             0x0F12 ,0x1409,     //  70004148
             0x0F12 ,0x1400,     //  7000414A
             0x0F12 ,0x1A08,     //  7000414C
             0x0F12 ,0x4995,     //  7000414E
             0x0F12 ,0x39E0,     //  70004150
             0x0F12 ,0x6148,     //  70004152
             0x0F12 ,0x9801,     //  70004154
             0x0F12 ,0x3040,     //  70004156
             0x0F12 ,0x7880,     //  70004158
             0x0F12 ,0x2800,     //  7000415A
             0x0F12 ,0xD103,     //  7000415C
             0x0F12 ,0x9801,     //  7000415E
             0x0F12 ,0x0029,     //  70004160
             0x0F12 ,0xF000,     //  70004162
             0x0F12 ,0xF9C7,     //  70004164
             0x0F12 ,0x8839,     //  70004166
             0x0F12 ,0x9800,     //  70004168
             0x0F12 ,0x4281,     //  7000416A
             0x0F12 ,0xD814,     //  7000416C
             0x0F12 ,0x8879,     //  7000416E
             0x0F12 ,0x9800,     //  70004170
             0x0F12 ,0x4281,     //  70004172
             0x0F12 ,0xD20C,     //  70004174
             0x0F12 ,0x9801,     //  70004176
             0x0F12 ,0x0029,     //  70004178
             0x0F12 ,0xF000,     //  7000417A
             0x0F12 ,0xF9C3,     //  7000417C
             0x0F12 ,0x9801,     //  7000417E
             0x0F12 ,0x0029,     //  70004180
             0x0F12 ,0xF000,     //  70004182
             0x0F12 ,0xF9BF,     //  70004184
             0x0F12 ,0x9801,     //  70004186
             0x0F12 ,0x0029,     //  70004188
             0x0F12 ,0xF000,     //  7000418A
             0x0F12 ,0xF9BB,     //  7000418C
             0x0F12 ,0xE003,     //  7000418E
             0x0F12 ,0x9801,     //  70004190
             0x0F12 ,0x0029,     //  70004192
             0x0F12 ,0xF000,     //  70004194
             0x0F12 ,0xF9B6,     //  70004196
             0x0F12 ,0x9801,     //  70004198
             0x0F12 ,0x0032,     //  7000419A
             0x0F12 ,0x0039,     //  7000419C
             0x0F12 ,0xF000,     //  7000419E
             0x0F12 ,0xF9B9,     //  700041A0
             0x0F12 ,0x6020,     //  700041A2
             0x0F12 ,0xE5D0,     //  700041A4
             0x0F12 ,0xB57C,     //  700041A6
             0x0F12 ,0x487F,     //  700041A8
             0x0F12 ,0xA901,     //  700041AA
             0x0F12 ,0x0004,     //  700041AC
             0x0F12 ,0xF000,     //  700041AE
             0x0F12 ,0xF93D,     //  700041B0
             0x0F12 ,0x466B,     //  700041B2
             0x0F12 ,0x88D9,     //  700041B4
             0x0F12 ,0x8898,     //  700041B6
             0x0F12 ,0x4B7A,     //  700041B8
             0x0F12 ,0x3346,     //  700041BA
             0x0F12 ,0x1E9A,     //  700041BC
             0x0F12 ,0xF000,     //  700041BE
             0x0F12 ,0xF9B1,     //  700041C0
             0x0F12 ,0x4879,     //  700041C2
             0x0F12 ,0x4977,     //  700041C4
             0x0F12 ,0x3812,     //  700041C6
             0x0F12 ,0x3140,     //  700041C8
             0x0F12 ,0x8A42,     //  700041CA
             0x0F12 ,0x888B,     //  700041CC
             0x0F12 ,0x18D2,     //  700041CE
             0x0F12 ,0x8242,     //  700041D0
             0x0F12 ,0x8AC2,     //  700041D2
             0x0F12 ,0x88C9,     //  700041D4
             0x0F12 ,0x1851,     //  700041D6
             0x0F12 ,0x82C1,     //  700041D8
             0x0F12 ,0x0020,     //  700041DA
             0x0F12 ,0x4669,     //  700041DC
             0x0F12 ,0xF000,     //  700041DE
             0x0F12 ,0xF925,     //  700041E0
             0x0F12 ,0x4872,     //  700041E2
             0x0F12 ,0x214D,     //  700041E4
             0x0F12 ,0x8301,     //  700041E6
             0x0F12 ,0x2196,     //  700041E8
             0x0F12 ,0x8381,     //  700041EA
             0x0F12 ,0x211D,     //  700041EC
             0x0F12 ,0x3020,     //  700041EE
             0x0F12 ,0x8001,     //  700041F0
             0x0F12 ,0xF000,     //  700041F2
             0x0F12 ,0xF99F,     //  700041F4
             0x0F12 ,0xF000,     //  700041F6
             0x0F12 ,0xF9A5,     //  700041F8
             0x0F12 ,0x486D,     //  700041FA
             0x0F12 ,0x4C6D,     //  700041FC
             0x0F12 ,0x6E00,     //  700041FE
             0x0F12 ,0x60E0,     //  70004200
             0x0F12 ,0x466B,     //  70004202
             0x0F12 ,0x8818,     //  70004204
             0x0F12 ,0x8859,     //  70004206
             0x0F12 ,0x0025,     //  70004208
             0x0F12 ,0x1A40,     //  7000420A
             0x0F12 ,0x3540,     //  7000420C
             0x0F12 ,0x61A8,     //  7000420E
             0x0F12 ,0x4864,     //  70004210
             0x0F12 ,0x9900,     //  70004212
             0x0F12 ,0x3060,     //  70004214
             0x0F12 ,0xF000,     //  70004216
             0x0F12 ,0xF99D,     //  70004218
             0x0F12 ,0x466B,     //  7000421A
             0x0F12 ,0x8819,     //  7000421C
             0x0F12 ,0x1DE0,     //  7000421E
             0x0F12 ,0x30F9,     //  70004220
             0x0F12 ,0x8741,     //  70004222
             0x0F12 ,0x8859,     //  70004224
             0x0F12 ,0x8781,     //  70004226
             0x0F12 ,0x2000,     //  70004228
             0x0F12 ,0x71A0,     //  7000422A
             0x0F12 ,0x74A8,     //  7000422C
             0x0F12 ,0xBC7C,     //  7000422E
             0x0F12 ,0xBC08,     //  70004230
             0x0F12 ,0x4718,     //  70004232
             0x0F12 ,0xB5F8,     //  70004234
             0x0F12 ,0x0005,     //  70004236
             0x0F12 ,0x6808,     //  70004238
             0x0F12 ,0x0400,     //  7000423A
             0x0F12 ,0x0C00,     //  7000423C
             0x0F12 ,0x684A,     //  7000423E
             0x0F12 ,0x0412,     //  70004240
             0x0F12 ,0x0C12,     //  70004242
             0x0F12 ,0x688E,     //  70004244
             0x0F12 ,0x68CC,     //  70004246
             0x0F12 ,0x4955,     //  70004248
             0x0F12 ,0x884B,     //  7000424A
             0x0F12 ,0x4343,     //  7000424C
             0x0F12 ,0x0A98,     //  7000424E
             0x0F12 ,0x2304,     //  70004250
             0x0F12 ,0x5ECB,     //  70004252
             0x0F12 ,0x18C0,     //  70004254
             0x0F12 ,0x02C0,     //  70004256
             0x0F12 ,0x0C00,     //  70004258
             0x0F12 ,0x88CB,     //  7000425A
             0x0F12 ,0x4353,     //  7000425C
             0x0F12 ,0x0A9A,     //  7000425E
             0x0F12 ,0x2308,     //  70004260
             0x0F12 ,0x5ECB,     //  70004262
             0x0F12 ,0x18D1,     //  70004264
             0x0F12 ,0x02C9,     //  70004266
             0x0F12 ,0x0C09,     //  70004268
             0x0F12 ,0x2701,     //  7000426A
             0x0F12 ,0x003A,     //  7000426C
             0x0F12 ,0x40AA,     //  7000426E
             0x0F12 ,0x9200,     //  70004270
             0x0F12 ,0x002A,     //  70004272
             0x0F12 ,0x3A10,     //  70004274
             0x0F12 ,0x4097,     //  70004276
             0x0F12 ,0x2D10,     //  70004278
             0x0F12 ,0xDA06,     //  7000427A
             0x0F12 ,0x4A4E,     //  7000427C
             0x0F12 ,0x9B00,     //  7000427E
             0x0F12 ,0x8812,     //  70004280
             0x0F12 ,0x439A,     //  70004282
             0x0F12 ,0x4B4C,     //  70004284
             0x0F12 ,0x801A,     //  70004286
             0x0F12 ,0xE003,     //  70004288
             0x0F12 ,0x4B4B,     //  7000428A
             0x0F12 ,0x885A,     //  7000428C
             0x0F12 ,0x43BA,     //  7000428E
             0x0F12 ,0x805A,     //  70004290
             0x0F12 ,0x0023,     //  70004292
             0x0F12 ,0x0032,     //  70004294
             0x0F12 ,0xF000,     //  70004296
             0x0F12 ,0xF945,     //  70004298
             0x0F12 ,0x2D10,     //  7000429A
             0x0F12 ,0xDA05,     //  7000429C
             0x0F12 ,0x4946,     //  7000429E
             0x0F12 ,0x9A00,     //  700042A0
             0x0F12 ,0x8808,     //  700042A2
             0x0F12 ,0x4310,     //  700042A4
             0x0F12 ,0x8008,     //  700042A6
             0x0F12 ,0xE003,     //  700042A8
             0x0F12 ,0x4843,     //  700042AA
             0x0F12 ,0x8841,     //  700042AC
             0x0F12 ,0x4339,     //  700042AE
             0x0F12 ,0x8041,     //  700042B0
             0x0F12 ,0x4D40,     //  700042B2
             0x0F12 ,0x2000,     //  700042B4
             0x0F12 ,0x3580,     //  700042B6
             0x0F12 ,0x88AA,     //  700042B8
             0x0F12 ,0x5E30,     //  700042BA
             0x0F12 ,0x2100,     //  700042BC
             0x0F12 ,0xF000,     //  700042BE
             0x0F12 ,0xF951,     //  700042C0
             0x0F12 ,0x8030,     //  700042C2
             0x0F12 ,0x2000,     //  700042C4
             0x0F12 ,0x88AA,     //  700042C6
             0x0F12 ,0x5E20,     //  700042C8
             0x0F12 ,0x2100,     //  700042CA
             0x0F12 ,0xF000,     //  700042CC
             0x0F12 ,0xF94A,     //  700042CE
             0x0F12 ,0x8020,     //  700042D0
             0x0F12 ,0xE587,     //  700042D2
             0x0F12 ,0xB510,     //  700042D4
             0x0F12 ,0xF000,     //  700042D6
             0x0F12 ,0xF94D,     //  700042D8
             0x0F12 ,0x4A38,     //  700042DA
             0x0F12 ,0x8D50,     //  700042DC
             0x0F12 ,0x2800,     //  700042DE
             0x0F12 ,0xD007,     //  700042E0
             0x0F12 ,0x4933,     //  700042E2
             0x0F12 ,0x31C0,     //  700042E4
             0x0F12 ,0x684B,     //  700042E6
             0x0F12 ,0x4935,     //  700042E8
             0x0F12 ,0x4283,     //  700042EA
             0x0F12 ,0xD202,     //  700042EC
             0x0F12 ,0x8D90,     //  700042EE
             0x0F12 ,0x81C8,     //  700042F0
             0x0F12 ,0xE6A0,     //  700042F2
             0x0F12 ,0x8DD0,     //  700042F4
             0x0F12 ,0x81C8,     //  700042F6
             0x0F12 ,0xE69D,     //  700042F8
             0x0F12 ,0xB5F8,     //  700042FA
             0x0F12 ,0xF000,     //  700042FC
             0x0F12 ,0xF942,     //  700042FE
             0x0F12 ,0x4D2E,     //  70004300
             0x0F12 ,0x8E28,     //  70004302
             0x0F12 ,0x2800,     //  70004304
             0x0F12 ,0xD01F,     //  70004306
             0x0F12 ,0x4E2E,     //  70004308
             0x0F12 ,0x4829,     //  7000430A
             0x0F12 ,0x68B4,     //  7000430C
             0x0F12 ,0x6800,     //  7000430E
             0x0F12 ,0x4284,     //  70004310
             0x0F12 ,0xD903,     //  70004312
             0x0F12 ,0x1A21,     //  70004314
             0x0F12 ,0x0849,     //  70004316
             0x0F12 ,0x1847,     //  70004318
             0x0F12 ,0xE006,     //  7000431A
             0x0F12 ,0x4284,     //  7000431C
             0x0F12 ,0xD203,     //  7000431E
             0x0F12 ,0x1B01,     //  70004320
             0x0F12 ,0x0849,     //  70004322
             0x0F12 ,0x1A47,     //  70004324
             0x0F12 ,0xE000,     //  70004326
             0x0F12 ,0x0027,     //  70004328
             0x0F12 ,0x0020,     //  7000432A
             0x0F12 ,0x4920,     //  7000432C
             0x0F12 ,0x3120,     //  7000432E
             0x0F12 ,0x7A0C,     //  70004330
             0x0F12 ,0x2C00,     //  70004332
             0x0F12 ,0xD004,     //  70004334
             0x0F12 ,0x0200,     //  70004336
             0x0F12 ,0x0039,     //  70004338
             0x0F12 ,0xF000,     //  7000433A
             0x0F12 ,0xF887,     //  7000433C
             0x0F12 ,0x8668,     //  7000433E
             0x0F12 ,0x2C00,     //  70004340
             0x0F12 ,0xD000,     //  70004342
             0x0F12 ,0x60B7,     //  70004344
             0x0F12 ,0xE54D,     //  70004346
             0x0F12 ,0x20FF,     //  70004348
             0x0F12 ,0x1C40,     //  7000434A
             0x0F12 ,0x8668,     //  7000434C
             0x0F12 ,0xE549,     //  7000434E
             0x0F12 ,0xB510,     //  70004350
             0x0F12 ,0x000C,     //  70004352
             0x0F12 ,0x6820,     //  70004354
             0x0F12 ,0x0400,     //  70004356
             0x0F12 ,0x0C00,     //  70004358
             0x0F12 ,0x4918,     //  7000435A
             0x0F12 ,0x8E0A,     //  7000435C
             0x0F12 ,0x2A00,     //  7000435E
             0x0F12 ,0xD003,     //  70004360
             0x0F12 ,0x8E49,     //  70004362
             0x0F12 ,0x0200,     //  70004364
             0x0F12 ,0xF000,     //  70004366
             0x0F12 ,0xF871,     //  70004368
             0x0F12 ,0x6020,     //  7000436A
             0x0F12 ,0x0400,     //  7000436C
             0x0F12 ,0x0C00,     //  7000436E
             0x0F12 ,0xE661,     //  70004370
             0x0F12 ,0xB570,     //  70004372
             0x0F12 ,0x680C,     //  70004374
             0x0F12 ,0x4D14,     //  70004376
             0x0F12 ,0x0020,     //  70004378
             0x0F12 ,0x6F29,     //  7000437A
             0x0F12 ,0xF000,     //  7000437C
             0x0F12 ,0xF90A,     //  7000437E
             0x0F12 ,0x6F69,     //  70004380
             0x0F12 ,0x1D20,     //  70004382
             0x0F12 ,0xF000,     //  70004384
             0x0F12 ,0xF906,     //  70004386
             0x0F12 ,0x480C,     //  70004388
             0x0F12 ,0x8E00,     //  7000438A
             0x0F12 ,0x2800,     //  7000438C
             0x0F12 ,0xD006,     //  7000438E
             0x0F12 ,0x4907,     //  70004390
             0x0F12 ,0x2214,     //  70004392
             0x0F12 ,0x3168,     //  70004394
             0x0F12 ,0x0008,     //  70004396
             0x0F12 ,0x383C,     //  70004398
             0x0F12 ,0xF000,     //  7000439A
             0x0F12 ,0xF903,     //  7000439C
             0x0F12 ,0xE488,     //  7000439E
             0x0F12 ,0x2558,     //  700043A0
             0x0F12 ,0x7000,     //  700043A2
             0x0F12 ,0x2AB8,     //  700043A4
             0x0F12 ,0x7000,     //  700043A6
             0x0F12 ,0x145E,     //  700043A8
             0x0F12 ,0x7000,     //  700043AA
             0x0F12 ,0x2698,     //  700043AC
             0x0F12 ,0x7000,     //  700043AE
             0x0F12 ,0x2BB8,     //  700043B0
             0x0F12 ,0x7000,     //  700043B2
             0x0F12 ,0x2998,     //  700043B4
             0x0F12 ,0x7000,     //  700043B6
             0x0F12 ,0x1100,     //  700043B8
             0x0F12 ,0xD000,     //  700043BA
             0x0F12 ,0x4780,     //  700043BC
             0x0F12 ,0x7000,     //  700043BE
             0x0F12 ,0xE200,     //  700043C0
             0x0F12 ,0xD000,     //  700043C2
             0x0F12 ,0x210C,     //  700043C4
             0x0F12 ,0x7000,     //  700043C6
             0x0F12 ,0x0000,     //  700043C8
             0x0F12 ,0x7000,     //  700043CA
             0x0F12 ,0x4778,     //  700043CC
             0x0F12 ,0x46C0,     //  700043CE
             0x0F12 ,0xC000,     //  700043D0
             0x0F12 ,0xE59F,     //  700043D2
             0x0F12 ,0xFF1C,     //  700043D4
             0x0F12 ,0xE12F,     //  700043D6
             0x0F12 ,0x1789,     //  700043D8
             0x0F12 ,0x0001,     //  700043DA
             0x0F12 ,0x4778,     //  700043DC
             0x0F12 ,0x46C0,     //  700043DE
             0x0F12 ,0xC000,     //  700043E0
             0x0F12 ,0xE59F,     //  700043E2
             0x0F12 ,0xFF1C,     //  700043E4
             0x0F12 ,0xE12F,     //  700043E6
             0x0F12 ,0x16F1,     //  700043E8
             0x0F12 ,0x0001,     //  700043EA
             0x0F12 ,0x4778,     //  700043EC
             0x0F12 ,0x46C0,     //  700043EE
             0x0F12 ,0xC000,     //  700043F0
             0x0F12 ,0xE59F,     //  700043F2
             0x0F12 ,0xFF1C,     //  700043F4
             0x0F12 ,0xE12F,     //  700043F6
             0x0F12 ,0xC3B1,     //  700043F8
             0x0F12 ,0x0000,     //  700043FA
             0x0F12 ,0x4778,     //  700043FC
             0x0F12 ,0x46C0,     //  700043FE
             0x0F12 ,0xC000,     //  70004400
             0x0F12 ,0xE59F,     //  70004402
             0x0F12 ,0xFF1C,     //  70004404
             0x0F12 ,0xE12F,     //  70004406
             0x0F12 ,0xC36D,     //  70004408
             0x0F12 ,0x0000,     //  7000440A
             0x0F12 ,0x4778,     //  7000440C
             0x0F12 ,0x46C0,     //  7000440E
             0x0F12 ,0xC000,     //  70004410
             0x0F12 ,0xE59F,     //  70004412
             0x0F12 ,0xFF1C,     //  70004414
             0x0F12 ,0xE12F,     //  70004416
             0x0F12 ,0xF6D7,     //  70004418
             0x0F12 ,0x0000,     //  7000441A
             0x0F12 ,0x4778,     //  7000441C
             0x0F12 ,0x46C0,     //  7000441E
             0x0F12 ,0xC000,     //  70004420
             0x0F12 ,0xE59F,     //  70004422
             0x0F12 ,0xFF1C,     //  70004424
             0x0F12 ,0xE12F,     //  70004426
             0x0F12 ,0xB49D,     //  70004428
             0x0F12 ,0x0000,     //  7000442A
             0x0F12 ,0x4778,     //  7000442C
             0x0F12 ,0x46C0,     //  7000442E
             0x0F12 ,0xC000,     //  70004430
             0x0F12 ,0xE59F,     //  70004432
             0x0F12 ,0xFF1C,     //  70004434
             0x0F12 ,0xE12F,     //  70004436
             0x0F12 ,0x7EDF,     //  70004438
             0x0F12 ,0x0000,     //  7000443A
             0x0F12 ,0x4778,     //  7000443C
             0x0F12 ,0x46C0,     //  7000443E
             0x0F12 ,0xC000,     //  70004440
             0x0F12 ,0xE59F,     //  70004442
             0x0F12 ,0xFF1C,     //  70004444
             0x0F12 ,0xE12F,     //  70004446
             0x0F12 ,0x448D,     //  70004448
             0x0F12 ,0x0000,     //  7000444A
             0x0F12 ,0x4778,     //  7000444C
             0x0F12 ,0x46C0,     //  7000444E
             0x0F12 ,0xF004,     //  70004450
             0x0F12 ,0xE51F,     //  70004452
             0x0F12 ,0x29EC,     //  70004454
             0x0F12 ,0x0001,     //  70004456
             0x0F12 ,0x4778,     //  70004458
             0x0F12 ,0x46C0,     //  7000445A
             0x0F12 ,0xC000,     //  7000445C
             0x0F12 ,0xE59F,     //  7000445E
             0x0F12 ,0xFF1C,     //  70004460
             0x0F12 ,0xE12F,     //  70004462
             0x0F12 ,0x2EF1,     //  70004464
             0x0F12 ,0x0000,     //  70004466
             0x0F12 ,0x4778,     //  70004468
             0x0F12 ,0x46C0,     //  7000446A
             0x0F12 ,0xC000,     //  7000446C
             0x0F12 ,0xE59F,     //  7000446E
             0x0F12 ,0xFF1C,     //  70004470
             0x0F12 ,0xE12F,     //  70004472
             0x0F12 ,0xEE03,     //  70004474
             0x0F12 ,0x0000,     //  70004476
             0x0F12 ,0x4778,     //  70004478
             0x0F12 ,0x46C0,     //  7000447A
             0x0F12 ,0xC000,     //  7000447C
             0x0F12 ,0xE59F,     //  7000447E
             0x0F12 ,0xFF1C,     //  70004480
             0x0F12 ,0xE12F,     //  70004482
             0x0F12 ,0xA58B,     //  70004484
             0x0F12 ,0x0000,     //  70004486
             0x0F12 ,0x4778,     //  70004488
             0x0F12 ,0x46C0,     //  7000448A
             0x0F12 ,0xC000,     //  7000448C
             0x0F12 ,0xE59F,     //  7000448E
             0x0F12 ,0xFF1C,     //  70004490
             0x0F12 ,0xE12F,     //  70004492
             0x0F12 ,0x7C49,     //  70004494
             0x0F12 ,0x0000,     //  70004496
             0x0F12 ,0x4778,     //  70004498
             0x0F12 ,0x46C0,     //  7000449A
             0x0F12 ,0xC000,     //  7000449C
             0x0F12 ,0xE59F,     //  7000449E
             0x0F12 ,0xFF1C,     //  700044A0
             0x0F12 ,0xE12F,     //  700044A2
             0x0F12 ,0x7C63,     //  700044A4
             0x0F12 ,0x0000,     //  700044A6
             0x0F12 ,0x4778,     //  700044A8
             0x0F12 ,0x46C0,     //  700044AA
             0x0F12 ,0xC000,     //  700044AC
             0x0F12 ,0xE59F,     //  700044AE
             0x0F12 ,0xFF1C,     //  700044B0
             0x0F12 ,0xE12F,     //  700044B2
             0x0F12 ,0x2DB7,     //  700044B4
             0x0F12 ,0x0000,     //  700044B6
             0x0F12 ,0x4778,     //  700044B8
             0x0F12 ,0x46C0,     //  700044BA
             0x0F12 ,0xC000,     //  700044BC
             0x0F12 ,0xE59F,     //  700044BE
             0x0F12 ,0xFF1C,     //  700044C0
             0x0F12 ,0xE12F,     //  700044C2
             0x0F12 ,0xEB3D,     //  700044C4
             0x0F12 ,0x0000,     //  700044C6
             0x0F12 ,0x4778,     //  700044C8
             0x0F12 ,0x46C0,     //  700044CA
             0x0F12 ,0xC000,     //  700044CC
             0x0F12 ,0xE59F,     //  700044CE
             0x0F12 ,0xFF1C,     //  700044D0
             0x0F12 ,0xE12F,     //  700044D2
             0x0F12 ,0xF061,     //  700044D4
             0x0F12 ,0x0000,     //  700044D6
             0x0F12 ,0x4778,     //  700044D8
             0x0F12 ,0x46C0,     //  700044DA
             0x0F12 ,0xC000,     //  700044DC
             0x0F12 ,0xE59F,     //  700044DE
             0x0F12 ,0xFF1C,     //  700044E0
             0x0F12 ,0xE12F,     //  700044E2
             0x0F12 ,0xF0EF,     //  700044E4
             0x0F12 ,0x0000,     //  700044E6
             0x0F12 ,0x4778,     //  700044E8
             0x0F12 ,0x46C0,     //  700044EA
             0x0F12 ,0xF004,     //  700044EC
             0x0F12 ,0xE51F,     //  700044EE
             0x0F12 ,0x2824,     //  700044F0
             0x0F12 ,0x0001,     //  700044F2
             0x0F12 ,0x4778,     //  700044F4
             0x0F12 ,0x46C0,     //  700044F6
             0x0F12 ,0xC000,     //  700044F8
             0x0F12 ,0xE59F,     //  700044FA
             0x0F12 ,0xFF1C,     //  700044FC
             0x0F12 ,0xE12F,     //  700044FE
             0x0F12 ,0x8EDD,     //  70004500
             0x0F12 ,0x0000,     //  70004502
             0x0F12 ,0x4778,     //  70004504
             0x0F12 ,0x46C0,     //  70004506
             0x0F12 ,0xC000,     //  70004508
             0x0F12 ,0xE59F,     //  7000450A
             0x0F12 ,0xFF1C,     //  7000450C
             0x0F12 ,0xE12F,     //  7000450E
             0x0F12 ,0x8DCB,     //  70004510
             0x0F12 ,0x0000,     //  70004512
             0x0F12 ,0x4778,     //  70004514
             0x0F12 ,0x46C0,     //  70004516
             0x0F12 ,0xC000,     //  70004518
             0x0F12 ,0xE59F,     //  7000451A
             0x0F12 ,0xFF1C,     //  7000451C
             0x0F12 ,0xE12F,     //  7000451E
             0x0F12 ,0x8E17,     //  70004520
             0x0F12 ,0x0000,     //  70004522
             0x0F12 ,0x4778,     //  70004524
             0x0F12 ,0x46C0,     //  70004526
             0x0F12 ,0xC000,     //  70004528
             0x0F12 ,0xE59F,     //  7000452A
             0x0F12 ,0xFF1C,     //  7000452C
             0x0F12 ,0xE12F,     //  7000452E
             0x0F12 ,0x98C5,     //  70004530
             0x0F12 ,0x0000,     //  70004532
             0x0F12 ,0x4778,     //  70004534
             0x0F12 ,0x46C0,     //  70004536
             0x0F12 ,0xC000,     //  70004538
             0x0F12 ,0xE59F,     //  7000453A
             0x0F12 ,0xFF1C,     //  7000453C
             0x0F12 ,0xE12F,     //  7000453E
             0x0F12 ,0x7C7D,     //  70004540
             0x0F12 ,0x0000,     //  70004542
             0x0F12 ,0x4778,     //  70004544
             0x0F12 ,0x46C0,     //  70004546
             0x0F12 ,0xC000,     //  70004548
             0x0F12 ,0xE59F,     //  7000454A
             0x0F12 ,0xFF1C,     //  7000454C
             0x0F12 ,0xE12F,     //  7000454E
             0x0F12 ,0x7E31,     //  70004550
             0x0F12 ,0x0000,     //  70004552
             0x0F12 ,0x4778,     //  70004554
             0x0F12 ,0x46C0,     //  70004556
             0x0F12 ,0xC000,     //  70004558
             0x0F12 ,0xE59F,     //  7000455A
             0x0F12 ,0xFF1C,     //  7000455C
             0x0F12 ,0xE12F,     //  7000455E
             0x0F12 ,0x7EAB,     //  70004560
             0x0F12 ,0x0000,     //  70004562
             0x0F12 ,0x4778,     //  70004564
             0x0F12 ,0x46C0,     //  70004566
             0x0F12 ,0xC000,     //  70004568
             0x0F12 ,0xE59F,     //  7000456A
             0x0F12 ,0xFF1C,     //  7000456C
             0x0F12 ,0xE12F,     //  7000456E
             0x0F12 ,0x7501,     //  70004570
             0x0F12 ,0x0000,     //  70004572
             0x0F12 ,0x4778,     //  70004574
             0x0F12 ,0x46C0,     //  70004576
             0x0F12 ,0xC000,     //  70004578
             0x0F12 ,0xE59F,     //  7000457A
             0x0F12 ,0xFF1C,     //  7000457C
             0x0F12 ,0xE12F,     //  7000457E
             0x0F12 ,0xF63F,     //  70004580
             0x0F12 ,0x0000,     //  70004582
             0x0F12 ,0x4778,     //  70004584
             0x0F12 ,0x46C0,     //  70004586
             0x0F12 ,0xC000,     //  70004588
             0x0F12 ,0xE59F,     //  7000458A
             0x0F12 ,0xFF1C,     //  7000458C
             0x0F12 ,0xE12F,     //  7000458E
             0x0F12 ,0x3D0B,     //  70004590
             0x0F12 ,0x0000,     //  70004592
             0x0F12 ,0x4778,     //  70004594
             0x0F12 ,0x46C0,     //  70004596
             0x0F12 ,0xC000,     //  70004598
             0x0F12 ,0xE59F,     //  7000459A
             0x0F12 ,0xFF1C,     //  7000459C
             0x0F12 ,0xE12F,     //  7000459E
             0x0F12 ,0x29BF,     //  700045A0
             0x0F12 ,0x0001,     //  700045A2
             0x0F12 ,0x4778,     //  700045A4
             0x0F12 ,0x46C0,     //  700045A6
             0x0F12 ,0xF004,     //  700045A8
             0x0F12 ,0xE51F,     //  700045AA
             0x0F12 ,0x26D8,     //  700045AC
             0x0F12 ,0x0001     //  700045AE
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }

    // End of Patch Data(Last : 700046CEh)
    // Total Size 3032 (0x0BD8)
    // Addr : 3AF8 , Size : 3030(BD6h)

    // TNP_USER_MBCV_CONTROL
    // TNP_4EC_MBR_TUNE
    // TNP_4EC_FORBIDDEN_TUNE
    // TNP_AF_FINESEARCH_DRIVEBACK
    // TNP_FLASH_ALG
    // TNP_GAS_ALPHA_OTP
    // TNP_AWB_MODUL_COMP
    // TNP_AWB_INIT_QUEUE
    // TNP_AWB_GRID_LOWBR
    // TNP_AWB_GRID_MODULECOMP
    // TNP_AFD_MOTO
    // TNP_ADLC_TUNE
    // TNP_1FRAME_AE
    // TNP_TG_OFF_CFG_CHG_IN_SPOOF_MODE


    //==================================================================================
    // 05.OTP Control
    //==================================================================================
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0722);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); ///skl_OTP_usWaitTime This register should be positioned in fornt of D0001000
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0726);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //skl_bUseOTPfunc This is OTP on/off function
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //ash_bUseOTPData
    S5K4ECGX_write_cmos_sensor(0x002A, 0x146E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //awbb_otp_disable
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08DC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //ash_bUseGasAlphaOTP
    //OTP on
    //002A    0722
    //0F12    0100 ///skl_OTP_usWaitTime This register should be positioned in fornt of D0001000
    //002A    0726
    //0F12    0001 //skl_bUseOTPfunc This is OTP on/off function
    //002A    08D6
    //0F12    0001 //ash_bUseOTPData
    //002A    146E
    //0F12    0000 //awbb_otp_disable
    //002A    08DC
    //0F12    0000 //ash_bUseGasAlphaOTP

    S5K4ECGX_write_cmos_sensor(0x0028, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);

    //==================================================================================
    // 06.Gas_Anti Shading
    //==================================================================================
    {
        static const kal_uint16 addr_data_pair[] =
        {
        // Refer Mon_AWB_RotGain
             0x0028, 0x7000,
             0x002A, 0x08B4,
             0x0F12, 0x0001, //wbt_bUseOutdoorASH
             0x002A, 0x08BC,
             0x0F12, 0x00C0, //TVAR_ash_AwbAshCord_0_ 2300K
             0x0F12, 0x00DF, //TVAR_ash_AwbAshCord_1_ 2750K
             0x0F12, 0x0100, //TVAR_ash_AwbAshCord_2_ 3300K
             0x0F12, 0x0125, //TVAR_ash_AwbAshCord_3_ 4150K
             0x0F12, 0x015F, //TVAR_ash_AwbAshCord_4_ 5250K
             0x0F12, 0x017C, //TVAR_ash_AwbAshCord_5_ 6400K
             0x0F12, 0x0194, //TVAR_ash_AwbAshCord_6_ 7500K
             0x002A, 0x08F6,
             0x0F12, 0x5000, //TVAR_ash_GASAlpha_0__0_ R  // 2300K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_0__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_0__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_0__3_ B
             0x0F12, 0x5000, //20130513, 4000, //TVAR_ash_GASAlpha_1__0_ R  // 2750K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_1__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_1__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_1__3_ B
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_2__0_ R  // 3300K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_2__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_2__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_2__3_ B
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_3__0_ R  // 4150K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_3__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_3__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_3__3_ B
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_4__0_ R  // 5250K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_4__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_4__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_4__3_ B
             0x0F12, 0x4300, //TVAR_ash_GASAlpha_5__0_ R  // 6400K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_5__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_5__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_5__3_ B
             0x0F12, 0x5000, //TVAR_ash_GASAlpha_6__0_ R  // 7500K
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_6__1_ GR
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_6__2_ GB
             0x0F12, 0x4000, //TVAR_ash_GASAlpha_6__3_ B
        //Outdoor GAS Alpha
             0x0F12, 0x4500,
             0x0F12, 0x4000,
             0x0F12, 0x4000,
             0x0F12, 0x4000,
             0x002A, 0x08F4,
             0x0F12, 0x0001, //ash_bUseGasAlpha


        //GAS High table   If OTP is used, GAS Setting Should be deleted. //
        //BENI 1.1 module 101018//
             0x002A, 0x0D26,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x000F,
             0x0F12, 0x000F,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x000F,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x000F,
             0x0F12, 0x000F,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F0F,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F00,
             0x0F12, 0x0000,
             0x0F12, 0x0F00,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,
             0x0F12, 0x000F,
             0x0F12, 0x000F,
             0x0F12, 0x0000,
             0x0F12, 0x000F,
             0x0F12, 0x0F0F,

       // TVAR_ash_pGAS_low
             0x002A, 0x0DB6,
             0x0F12, 0x88A2,
             0x0F12, 0xEF5B,
             0x0F12, 0xF576,
             0x0F12, 0x2242,
             0x0F12, 0xEC90,
             0x0F12, 0xFCB2,
             0x0F12, 0xD726,
             0x0F12, 0xF77C,
             0x0F12, 0x1CCB,
             0x0F12, 0xDB4D,
             0x0F12, 0x0948,
             0x0F12, 0x13C2,
             0x0F12, 0x0A14,
             0x0F12, 0x017A,
             0x0F12, 0xE9B4,
             0x0F12, 0x190D,
             0x0F12, 0x16E5,
             0x0F12, 0xCAB2,
             0x0F12, 0x18CD,
             0x0F12, 0x0A84,
             0x0F12, 0x097E,
             0x0F12, 0xF076,
             0x0F12, 0xE849,
             0x0F12, 0x2CFC,
             0x0F12, 0xE460,
             0x0F12, 0xEE89,
             0x0F12, 0x0693,
             0x0F12, 0x06B4,
             0x0F12, 0xF16E,
             0x0F12, 0x12B6,
             0x0F12, 0x0F99,
             0x0F12, 0x0F3B,
             0x0F12, 0xE728,
             0x0F12, 0x19BB,
             0x0F12, 0x058E,
             0x0F12, 0xDA99,
             0x0F12, 0x952B,
             0x0F12, 0xE6F0,
             0x0F12, 0x0163,
             0x0F12, 0x1376,
             0x0F12, 0xFC0E,
             0x0F12, 0xF3A2,
             0x0F12, 0xCE5D,
             0x0F12, 0xFA86,
             0x0F12, 0x11D3,
             0x0F12, 0xEB02,
             0x0F12, 0xFE43,
             0x0F12, 0x17ED,
             0x0F12, 0x1320,
             0x0F12, 0x0156,
             0x0F12, 0xF4FF,
             0x0F12, 0x0ACA,
             0x0F12, 0x162B,
             0x0F12, 0xD2D8,
             0x0F12, 0x0F4F,
             0x0F12, 0x0178,
             0x0F12, 0x0AD1,
             0x0F12, 0xEDE5,
             0x0F12, 0xFBA5,
             0x0F12, 0x1A69,
             0x0F12, 0xF30F,
             0x0F12, 0xFC58,
             0x0F12, 0xF92D,
             0x0F12, 0x131C,
             0x0F12, 0xE607,
             0x0F12, 0x1564,
             0x0F12, 0x02A8,
             0x0F12, 0x08B5,
             0x0F12, 0xF04C,
             0x0F12, 0x15D0,
             0x0F12, 0xFAD0,
             0x0F12, 0xEB70,
             0x0F12, 0x8564,
             0x0F12, 0xE967,
             0x0F12, 0xFFFF,
             0x0F12, 0x16A8,
             0x0F12, 0xEFD6,
             0x0F12, 0x01AF,
             0x0F12, 0xD7AD,
             0x0F12, 0x01A2,
             0x0F12, 0x0A4E,
             0x0F12, 0xF1CE,
             0x0F12, 0xFA95,
             0x0F12, 0x143F,
             0x0F12, 0x1046,
             0x0F12, 0xF6A1,
             0x0F12, 0xF7BB,
             0x0F12, 0x0E8D,
             0x0F12, 0x11A3,
             0x0F12, 0xDB43,
             0x0F12, 0x1459,
             0x0F12, 0x0FFA,
             0x0F12, 0x0731,
             0x0F12, 0xEC67,
             0x0F12, 0xF7CA,
             0x0F12, 0x1682,
             0x0F12, 0xDF77,
             0x0F12, 0xEEA5,
             0x0F12, 0xFF71,
             0x0F12, 0x08FF,
             0x0F12, 0xF8FA,
             0x0F12, 0x138E,
             0x0F12, 0x16FE,
             0x0F12, 0x0BA0,
             0x0F12, 0xF297,
             0x0F12, 0x1717,
             0x0F12, 0xF5BB,
             0x0F12, 0xE6B7,
             0x0F12, 0x87A3,
             0x0F12, 0xECB4,
             0x0F12, 0xF8A1,
             0x0F12, 0x1D23,
             0x0F12, 0xF35F,
             0x0F12, 0xF7C7,
             0x0F12, 0xD9ED,
             0x0F12, 0xF792,
             0x0F12, 0x1E98,
             0x0F12, 0xD734,
             0x0F12, 0x0BA1,
             0x0F12, 0x14E3,
             0x0F12, 0x0BB9,
             0x0F12, 0x0279,
             0x0F12, 0xDEC5,
             0x0F12, 0x2EDC,
             0x0F12, 0x010A,
             0x0F12, 0xD36F,
             0x0F12, 0x1A6A,
             0x0F12, 0x03F6,
             0x0F12, 0x1AE5,
             0x0F12, 0xD3FB,
             0x0F12, 0xFFFA,
             0x0F12, 0x26A0,
             0x0F12, 0xDF98,
             0x0F12, 0xF8DC,
             0x0F12, 0xF675,
             0x0F12, 0x168E,
             0x0F12, 0xEFC9,
             0x0F12, 0x0A42,
             0x0F12, 0x11D3,
             0x0F12, 0x08BE,
             0x0F12, 0xEF30,
             0x0F12, 0x1785,
             0x0F12, 0xFBF7,
             0x0F12, 0xE573,
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }
    //==================================================================================
    // 07. Analog Setting 2
    //==================================================================================
    {
        static const kal_uint16 addr_data_pair[] =
        {
        //This register is for FACTORY ONLY.
        //If you change it without prior notification
        //YOU are RESPONSIBLE for the FAILURE that will happen in the future
        //For subsampling Size
             0x002A, 0x18BC,
             0x0F12, 0x0004,
             0x0F12, 0x05B6,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0001,
             0x0F12, 0x05BA,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0007,
             0x0F12, 0x05BA,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x01F4,
             0x0F12, 0x024E,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x01F4,
             0x0F12, 0x05B6,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x01F4,
             0x0F12, 0x05BA,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x01F4,
             0x0F12, 0x024F,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0075,
             0x0F12, 0x00CF,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0075,
             0x0F12, 0x00D6,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0004,
             0x0F12, 0x01F4,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x00F0,
             0x0F12, 0x01F4,
             0x0F12, 0x029E,
             0x0F12, 0x05B2,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x01F8,
             0x0F12, 0x0228,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0208,
             0x0F12, 0x0238,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0218,
             0x0F12, 0x0238,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0001,
             0x0F12, 0x0009,
             0x0F12, 0x00DE,
             0x0F12, 0x05C0,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x00DF,
             0x0F12, 0x00E4,
             0x0F12, 0x01F8,
             0x0F12, 0x01FD,
             0x0F12, 0x05B6,
             0x0F12, 0x05BB,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x01F8,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0077,
             0x0F12, 0x007E,
             0x0F12, 0x024F,
             0x0F12, 0x025E,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
        // For Capture
             0x0F12, 0x0004,
             0x0F12, 0x09D1,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0001,
             0x0F12, 0x09D5,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0008,
             0x0F12, 0x09D5,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02AA,
             0x0F12, 0x0326,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02AA,
             0x0F12, 0x09D1,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02AA,
             0x0F12, 0x09D5,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02AA,
             0x0F12, 0x0327,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0008,
             0x0F12, 0x0084,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0008,
             0x0F12, 0x008D,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0008,
             0x0F12, 0x02AA,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x00AA,
             0x0F12, 0x02AA,
             0x0F12, 0x03AD,
             0x0F12, 0x09CD,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02AE,
             0x0F12, 0x02DE,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02BE,
             0x0F12, 0x02EE,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02CE,
             0x0F12, 0x02EE,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0001,
             0x0F12, 0x0009,
             0x0F12, 0x0095,
             0x0F12, 0x09DB,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0096,
             0x0F12, 0x009B,
             0x0F12, 0x02AE,
             0x0F12, 0x02B3,
             0x0F12, 0x09D1,
             0x0F12, 0x09D6,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x02AE,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0009,
             0x0F12, 0x0010,
             0x0F12, 0x0327,
             0x0F12, 0x0336,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x002A, 0x1AF8,
             0x0F12, 0x5A3C, //senHal_TuneStr_AngTuneData1_2_D000F400 register at subsampling
             0x002A, 0x1896,
             0x0F12, 0x0002, //senHal_SamplingType 0002 03EE: PLA setting
             0x0F12, 0x0000, //senHal_SamplingMode 0 : 2 PLA / 1 : 4PLA
             0x0F12, 0x0001, //senHal_PLAOption  [0] VPLA enable  [1] HPLA enable
             0x002A, 0x1B00,
             0x0F12, 0xF428,
             0x0F12, 0xFFFF,
             0x0F12, 0x0000,
             0x002A, 0x189E,
             0x0F12, 0x0FB0, //senHal_ExpMinPixels
             0x002A, 0x18AC,
             0x0F12, 0x0060,   //senHal_uAddColsBin
             0x0F12, 0x0060, //senHal_uAddColsNoBin
             0x0F12, 0x07DC, //senHal_uMinColsBin
             0x0F12, 0x05C0, //senHal_uMinColsNoBin
             0x002A, 0x1AEA,
             0x0F12, 0x8080, //senHal_SubF404Tune
             0x0F12, 0x0080, //senHal_FullF404Tune
             0x002A, 0x1AE0,
             0x0F12, 0x0000, //senHal_bSenAAC
             0x002A, 0x1A72,
             0x0F12, 0x0000, //senHal_bSRX SRX off
             0x002A, 0x18A2,
             0x0F12, 0x0004, //senHal_NExpLinesCheckFine extend Forbidden area line
             0x002A, 0x1A6A,
             0x0F12, 0x009A, //senHal_usForbiddenRightOfs extend right Forbidden area line
             0x002A, 0x385E,
             0x0F12, 0x024C, //Mon_Sen_uExpPixelsOfs
             0x002A, 0x0EE6,
             0x0F12, 0x0000, //setot_bUseDigitalHbin
             0x002A, 0x1B2A,
             0x0F12, 0x0300, //70001B2A //senHal_TuneStr2_usAngTuneGainTh
             0x0F12, 0x00D6, //70001B2C //senHal_TuneStr2_AngTuneF4CA_0_
             0x0F12, 0x008D, //70001B2E //senHal_TuneStr2_AngTuneF4CA_1_
             0x0F12, 0x00CF, //70001B30 //senHal_TuneStr2_AngTuneF4C2_0_
             0x0F12, 0x0084, //70001B32 //senHal_TuneStr2_AngTuneF4C2_1_
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }
    //==================================================================================
    // 08.AF Setting
    //==================================================================================

    {
        static const kal_uint16 addr_data_pair[] =
        {
        //AF interface setting
             0x002A, 0x01FC,
             0x0F12, 0x0001, //REG_TC_IPRM_LedGpio, for Flash control
        //s002A1720
        //s0F120100 //afd_usFlags, Low voltage AF enable
             0x0F12, 0x0003, //REG_TC_IPRM_CM_Init_AfModeType, VCM IIC
             0x0F12, 0x0000, //REG_TC_IPRM_CM_Init_PwmConfig1
             0x002A, 0x0204,
             0x0F12, 0x0061, //REG_TC_IPRM_CM_Init_GpioConfig1, AF Enable GPIO 6
             0x002A, 0x020C,
             0x0F12, 0x2F0C, //REG_TC_IPRM_CM_Init_Mi2cBit
             0x0F12, 0x0190, //REG_TC_IPRM_CM_Init_Mi2cRateKhz, IIC Speed

        //AF Window Settings
             0x002A, 0x0294,
             0x0F12, 0x0100, //REG_TC_AF_FstWinStartX
             0x0F12, 0x00E3, //REG_TC_AF_FstWinStartY
             0x0F12, 0x0200, //REG_TC_AF_FstWinSizeX
             0x0F12, 0x0238, //REG_TC_AF_FstWinSizeY
             0x0F12, 0x018C, //REG_TC_AF_ScndWinStartX
             0x0F12, 0x0166, //REG_TC_AF_ScndWinStartY
             0x0F12, 0x00E6, //REG_TC_AF_ScndWinSizeX
             0x0F12, 0x0132, //REG_TC_AF_ScndWinSizeY
             0x0F12, 0x0001, //REG_TC_AF_WinSizesUpdated
        //2nd search setting
             0x002A, 0x070E,
             0x0F12, 0x00C0, //skl_af_StatOvlpExpFactor
             0x002A, 0x071E,
             0x0F12, 0x0000, //skl_af_bAfStatOff
             0x002A, 0x163C,
             0x0F12, 0x0000, //af_search_usAeStable
             0x002A, 0x1648,
             0x0F12, 0x9002, //af_search_usSingleAfFlags
             0x002A, 0x1652,
             0x0F12, 0x0002, //af_search_usFinePeakCount
             0x0F12, 0x0000, //af_search_usFineMaxScale
             0x002A, 0x15E0,
             0x0F12, 0x0403, //af_pos_usFineStepNumSize
             0x002A, 0x1656,
             0x0F12, 0x0000, //af_search_usCapturePolicy
        //Peak Threshold
             0x002A, 0x164C,
             0x0F12, 0x0003, //af_search_usMinPeakSamples
             0x002A, 0x163E,
             0x0F12, 0x00C0, //af_search_usPeakThr
             0x0F12, 0x0080, //af_search_usPeakThrLow
             0x002A, 0x47A8,
             0x0F12, 0x0080, //TNP, Macro Threshold register
        //Home Pos
             0x002A, 0x15D4,
             0x0F12, 0x0000, //af_pos_usHomePos
             0x0F12, 0xD000, //af_pos_usLowConfPos
        //AF statistics
             0x002A, 0x169A,
             0x0F12, 0xFF95, //af_search_usConfCheckOrder_1_
             0x002A, 0x166A,
             0x0F12, 0x0280, //af_search_usConfThr_4_
             0x002A, 0x1676,
             0x0F12, 0x03A0, //af_search_usConfThr_10_
             0x0F12, 0x0320, //af_search_usConfThr_11_
             0x002A, 0x16BC,
             0x0F12, 0x0030, //af_stat_usMinStatVal
             0x002A, 0x16E0,
             0x0F12, 0x0060, //af_scene_usSceneLowNormBrThr
             0x002A, 0x16D4,
             0x0F12, 0x0010, //af_stat_usBpfThresh


        //AF Lens Position Table Settings
             0x002A, 0x15E8,
             0x0F12, 0x0010, //af_pos_usTableLastInd
             0x0F12, 0x0018, //af_pos_usTable
             0x0F12, 0x0020, //af_pos_usTable
             0x0F12, 0x0028, //af_pos_usTable
             0x0F12, 0x0030, //af_pos_usTable
             0x0F12, 0x0038, //af_pos_usTable
             0x0F12, 0x0040, //af_pos_usTable
             0x0F12, 0x0048, //af_pos_usTable
             0x0F12, 0x0050, //af_pos_usTable
             0x0F12, 0x0058, //af_pos_usTable
             0x0F12, 0x0060, //af_pos_usTable
             0x0F12, 0x0068, //af_pos_usTable
             0x0F12, 0x0070, //af_pos_usTable
             0x0F12, 0x0078, //af_pos_usTable
             0x0F12, 0x0080, //af_pos_usTable
             0x0F12, 0x0088, //af_pos_usTable
             0x0F12, 0x0090, //af_pos_usTable
             0x0F12, 0x0098, //af_pos_usTable


        //VCM AF driver with PWM/I2C
             0x002A, 0x1722,
             0x0F12, 0x8000, //afd_usParam[0] I2C power down command
             0x0F12, 0x0006, //afd_usParam[1] Position Right Shift
             0x0F12, 0x3FF0, //afd_usParam[2] I2C Data Mask
             0x0F12, 0x03E8, //afd_usParam[3] PWM Period
             0x0F12, 0x0000, //afd_usParam[4] PWM Divider
             0x0F12, 0x0020, //afd_usParam[5] SlowMotion Delay 4. reduce lens collision noise.
             0x0F12, 0x0010, //afd_usParam[6] SlowMotion Threshold
             0x0F12, 0x0008, //afd_usParam[7] Signal Shaping
             0x0F12, 0x0040, //afd_usParam[8] Signal Shaping level
             0x0F12, 0x0080, //afd_usParam[9] Signal Shaping level
             0x0F12, 0x00C0, //afd_usParam[10] Signal Shaping level
             0x0F12, 0x00E0, //afd_usParam[11] Signal Shaping level
             0x002A, 0x028C,
             0x0F12, 0x0003, //REG_TC_AF_AfCmd
    //==================================================================================
    // 09.AWB-BASIC setting
    //==================================================================================

        // AWB init Start point
             0x002A, 0x145E,
             0x0F12, 0x0580, //awbb_GainsInit_0_
             0x0F12, 0x0428, //awbb_GainsInit_1_
             0x0F12, 0x07B0, //awbb_GainsInit_2_
        // AWB Convergence Speed
             0x0F12, 0x0008, //awbb_WpFilterMinThr
             0x0F12, 0x0190, //awbb_WpFilterMaxThr
             0x0F12, 0x00A0, //awbb_WpFilterCoef
             0x0F12, 0x0004, //awbb_WpFilterSize
             0x0F12, 0x0002, //awbb_GridEnable
             0x002A, 0x144E,
             0x0F12, 0x0000, //awbb_RGainOff
             0x0F12, 0x0000, //awbb_BGainOff
             0x0F12, 0x0000, //awbb_GGainOff
             0x0F12, 0x00C2, //awbb_Alpha_Comp_Mode
             0x0F12, 0x0002, //awbb_Rpl_InvalidOutDoor
             0x0F12, 0x0001, //awbb_UseGrThrCorr
             0x0F12, 0x0074, //awbb_Use_Filters
             0x0F12, 0x0001, //awbb_CorrectMinNumPatches
        // White Locus
             0x002A, 0x11F0,
             0x0F12, 0x012C, //awbb_IntcR
             0x0F12, 0x0121, //awbb_IntcB
             0x0F12, 0x02DF, //awbb_GLocusR
             0x0F12, 0x0314, //awbb_GLocusB
             0x002A, 0x120E,
             0x0F12, 0x0000, //awbb_MovingScale10
             0x0F12, 0x05FD, //awbb_GamutWidthThr1
             0x0F12, 0x036B, //awbb_GamutHeightThr1
             0x0F12, 0x0020, //awbb_GamutWidthThr2
             0x0F12, 0x001A, //awbb_GamutHeightThr2
             0x002A, 0x1278,
             0x0F12, 0xFEF7, //awbb_SCDetectionMap_SEC_StartR_B
             0x0F12, 0x0021, //awbb_SCDetectionMap_SEC_StepR_B
             0x0F12, 0x07D0, //awbb_SCDetectionMap_SEC_SunnyNB
             0x0F12, 0x07D0, //awbb_SCDetectionMap_SEC_StepNB
             0x0F12, 0x01C8, //awbb_SCDetectionMap_SEC_LowTempR_B
             0x0F12, 0x0096, //awbb_SCDetectionMap_SEC_SunnyNBZone
             0x0F12, 0x0004, //awbb_SCDetectionMap_SEC_LowTempR_BZone
             0x002A, 0x1224,
             0x0F12, 0x0032, //awbb_LowBr
             0x0F12, 0x001E, //awbb_LowBr_NBzone
             0x0F12, 0x00E2, //awbb_YThreshHigh
             0x0F12, 0x0010, //awbb_YThreshLow_Norm
             0x0F12, 0x0002, //awbb_YThreshLow_Low
             0x002A, 0x2BA4,
             0x0F12, 0x0004, //Mon_AWB_ByPassMode
             0x002A, 0x11FC,
             0x0F12, 0x000C, //awbb_MinNumOfFinalPatches
             0x002A, 0x1208,
             0x0F12, 0x0020, //awbb_MinNumOfChromaclassifpatches
        // Indoor Zone
             0x002A, 0x101C,
             0x0F12, 0x0360, //awbb_IndoorGrZones_m_BGrid_0__m_left
             0x0F12, 0x036C, //awbb_IndoorGrZones_m_BGrid_0__m_right
             0x0F12, 0x0320, //awbb_IndoorGrZones_m_BGrid_1__m_left
             0x0F12, 0x038A, //awbb_IndoorGrZones_m_BGrid_1__m_right
             0x0F12, 0x02E8, //awbb_IndoorGrZones_m_BGrid_2__m_left
             0x0F12, 0x0380, //awbb_IndoorGrZones_m_BGrid_2__m_right
             0x0F12, 0x02BE, //awbb_IndoorGrZones_m_BGrid_3__m_left
             0x0F12, 0x035A, //awbb_IndoorGrZones_m_BGrid_3__m_right
             0x0F12, 0x0298, //awbb_IndoorGrZones_m_BGrid_4__m_left
             0x0F12, 0x0334, //awbb_IndoorGrZones_m_BGrid_4__m_right
             0x0F12, 0x0272, //awbb_IndoorGrZones_m_BGrid_5__m_left
             0x0F12, 0x030E, //awbb_IndoorGrZones_m_BGrid_5__m_right
             0x0F12, 0x024C, //awbb_IndoorGrZones_m_BGrid_6__m_left
             0x0F12, 0x02EA, //awbb_IndoorGrZones_m_BGrid_6__m_right
             0x0F12, 0x0230, //awbb_IndoorGrZones_m_BGrid_7__m_left
             0x0F12, 0x02CC, //awbb_IndoorGrZones_m_BGrid_7__m_right
             0x0F12, 0x0214, //awbb_IndoorGrZones_m_BGrid_8__m_left
             0x0F12, 0x02B0, //awbb_IndoorGrZones_m_BGrid_8__m_right
             0x0F12, 0x01F8, //awbb_IndoorGrZones_m_BGrid_9__m_left
             0x0F12, 0x0294, //awbb_IndoorGrZones_m_BGrid_9__m_right
             0x0F12, 0x01DC, //awbb_IndoorGrZones_m_BGrid_10__m_left
             0x0F12, 0x0278, //awbb_IndoorGrZones_m_BGrid_10__m_right
             0x0F12, 0x01C0, //awbb_IndoorGrZones_m_BGrid_11__m_left
             0x0F12, 0x0264, //awbb_IndoorGrZones_m_BGrid_11__m_right
             0x0F12, 0x01AA, //awbb_IndoorGrZones_m_BGrid_12__m_left
             0x0F12, 0x0250, //awbb_IndoorGrZones_m_BGrid_12__m_right
             0x0F12, 0x0196, //awbb_IndoorGrZones_m_BGrid_13__m_left
             0x0F12, 0x023C, //awbb_IndoorGrZones_m_BGrid_13__m_right
             0x0F12, 0x0180, //awbb_IndoorGrZones_m_BGrid_14__m_left
             0x0F12, 0x0228, //awbb_IndoorGrZones_m_BGrid_14__m_right
             0x0F12, 0x016C, //awbb_IndoorGrZones_m_BGrid_15__m_left
             0x0F12, 0x0214, //awbb_IndoorGrZones_m_BGrid_15__m_right
             0x0F12, 0x0168, //awbb_IndoorGrZones_m_BGrid_16__m_left
             0x0F12, 0x0200, //awbb_IndoorGrZones_m_BGrid_16__m_right
             0x0F12, 0x0172, //awbb_IndoorGrZones_m_BGrid_17__m_left
             0x0F12, 0x01EC, //awbb_IndoorGrZones_m_BGrid_17__m_right
             0x0F12, 0x019A, //awbb_IndoorGrZones_m_BGrid_18__m_left
             0x0F12, 0x01D8, //awbb_IndoorGrZones_m_BGrid_18__m_right
             0x0F12, 0x0000, //awbb_IndoorGrZones_m_BGrid_19__m_left
             0x0F12, 0x0000, //awbb_IndoorGrZones_m_BGrid_19__m_right
             0x0F12, 0x0005, //awbb_IndoorGrZones_m_GridStep
             0x002A, 0x1070,
             0x0F12, 0x0013, //awbb_IndoorGrZones_ZInfo_m_GridSz
             0x002A, 0x1074,
             0x0F12, 0x00EC, //awbb_IndoorGrZones_m_Boffs
        // Outdoor Zone
             0x002A, 0x1078,
             0x0F12, 0x0232, //awbb_OutdoorGrZones_m_BGrid_0__m_left
             0x0F12, 0x025A, //awbb_OutdoorGrZones_m_BGrid_0__m_right
             0x0F12, 0x021E, //awbb_OutdoorGrZones_m_BGrid_1__m_left
             0x0F12, 0x0274, //awbb_OutdoorGrZones_m_BGrid_1__m_right
             0x0F12, 0x020E, //awbb_OutdoorGrZones_m_BGrid_2__m_left
             0x0F12, 0x028E, //awbb_OutdoorGrZones_m_BGrid_2__m_right
             0x0F12, 0x0200, //awbb_OutdoorGrZones_m_BGrid_3__m_left
             0x0F12, 0x0290, //awbb_OutdoorGrZones_m_BGrid_3__m_right
             0x0F12, 0x01F4, //awbb_OutdoorGrZones_m_BGrid_4__m_left
             0x0F12, 0x0286, //awbb_OutdoorGrZones_m_BGrid_4__m_right
             0x0F12, 0x01E8, //awbb_OutdoorGrZones_m_BGrid_5__m_left
             0x0F12, 0x027E, //awbb_OutdoorGrZones_m_BGrid_5__m_right
             0x0F12, 0x01DE, //awbb_OutdoorGrZones_m_BGrid_6__m_left
             0x0F12, 0x0274, //awbb_OutdoorGrZones_m_BGrid_6__m_right
             0x0F12, 0x01D2, //awbb_OutdoorGrZones_m_BGrid_7__m_left
             0x0F12, 0x0268, //awbb_OutdoorGrZones_m_BGrid_7__m_right
             0x0F12, 0x01D0, //awbb_OutdoorGrZones_m_BGrid_8__m_left
             0x0F12, 0x025E, //awbb_OutdoorGrZones_m_BGrid_8__m_right
             0x0F12, 0x01D6, //awbb_OutdoorGrZones_m_BGrid_9__m_left
             0x0F12, 0x0252, //awbb_OutdoorGrZones_m_BGrid_9__m_right
             0x0F12, 0x01E2, //awbb_OutdoorGrZones_m_BGrid_10__m_left
             0x0F12, 0x0248, //awbb_OutdoorGrZones_m_BGrid_10__m_right
             0x0F12, 0x01F4, //awbb_OutdoorGrZones_m_BGrid_11__m_left
             0x0F12, 0x021A, //awbb_OutdoorGrZones_m_BGrid_11__m_right
             0x0F12, 0x0004, //awbb_OutdoorGrZones_m_GridStep
             0x002A, 0x10AC,
             0x0F12, 0x000C, //awbb_OutdoorGrZones_ZInfo_m_GridSz
             0x002A, 0x10B0,
             0x0F12, 0x01DA, //awbb_OutdoorGrZones_m_Boffs
        // Low Brightness Zone
             0x002A, 0x10B4,
             0x0F12, 0x0348, //awbb_LowBrGrZones_m_BGrid_0__m_left
             0x0F12, 0x03B6, //awbb_LowBrGrZones_m_BGrid_0__m_right
             0x0F12, 0x02B8, //awbb_LowBrGrZones_m_BGrid_1__m_left
             0x0F12, 0x03B6, //awbb_LowBrGrZones_m_BGrid_1__m_right
             0x0F12, 0x0258, //awbb_LowBrGrZones_m_BGrid_2__m_left
             0x0F12, 0x038E, //awbb_LowBrGrZones_m_BGrid_2__m_right
             0x0F12, 0x0212, //awbb_LowBrGrZones_m_BGrid_3__m_left
             0x0F12, 0x0348, //awbb_LowBrGrZones_m_BGrid_3__m_right
             0x0F12, 0x01CC, //awbb_LowBrGrZones_m_BGrid_4__m_left
             0x0F12, 0x030C, //awbb_LowBrGrZones_m_BGrid_4__m_right
             0x0F12, 0x01A2, //awbb_LowBrGrZones_m_BGrid_5__m_left
             0x0F12, 0x02D2, //awbb_LowBrGrZones_m_BGrid_5__m_right
             0x0F12, 0x0170, //awbb_LowBrGrZones_m_BGrid_6__m_left
             0x0F12, 0x02A6, //awbb_LowBrGrZones_m_BGrid_6__m_right
             0x0F12, 0x014C, //awbb_LowBrGrZones_m_BGrid_7__m_left
             0x0F12, 0x0280, //awbb_LowBrGrZones_m_BGrid_7__m_right
             0x0F12, 0x0128, //awbb_LowBrGrZones_m_BGrid_8__m_left
             0x0F12, 0x025C, //awbb_LowBrGrZones_m_BGrid_8__m_right
             0x0F12, 0x0146, //awbb_LowBrGrZones_m_BGrid_9__m_left
             0x0F12, 0x0236, //awbb_LowBrGrZones_m_BGrid_9__m_right
             0x0F12, 0x0164, //awbb_LowBrGrZones_m_BGrid_10__m_left
             0x0F12, 0x0212, //awbb_LowBrGrZones_m_BGrid_10__m_right
             0x0F12, 0x0000, //awbb_LowBrGrZones_m_BGrid_11__m_left
             0x0F12, 0x0000, //awbb_LowBrGrZones_m_BGrid_11__m_right
             0x0F12, 0x0006, //awbb_LowBrGrZones_m_GridStep
             0x002A, 0x10E8,
             0x0F12, 0x000B, //awbb_LowBrGrZones_ZInfo_m_GridSz
             0x002A, 0x10EC,
             0x0F12, 0x00D2, //awbb_LowBrGrZones_m_Boffs

        // Low Temp. Zone
             0x002A, 0x10F0,
             0x0F12, 0x039A,
             0x0F12, 0x0000, //awbb_CrclLowT_R_c
             0x0F12, 0x00FE,
             0x0F12, 0x0000, //awbb_CrclLowT_B_c
             0x0F12, 0x2284,
             0x0F12, 0x0000, //awbb_CrclLowT_Rad_c

        //AWB - GridCorrection
             0x002A, 0x1434,
             0x0F12, 0x02C1, //awbb_GridConst_1_0_
             0x0F12, 0x033A, //awbb_GridConst_1_1_
             0x0F12, 0x038A, //awbb_GridConst_1_2_
             0x0F12, 0x101A, //awbb_GridConst_2_0_
             0x0F12, 0x1075, //awbb_GridConst_2_1_
             0x0F12, 0x113D, //awbb_GridConst_2_2_
             0x0F12, 0x113F, //awbb_GridConst_2_3_
             0x0F12, 0x11AF, //awbb_GridConst_2_4_
             0x0F12, 0x11F0, //awbb_GridConst_2_5_
             0x0F12, 0x00B2, //awbb_GridCoeff_R_1
             0x0F12, 0x00B8, //awbb_GridCoeff_B_1
             0x0F12, 0x00CA, //awbb_GridCoeff_R_2
             0x0F12, 0x009D, //awbb_GridCoeff_B_2

        // Indoor Grid Offset
             0x002A, 0x13A4,
             0x0F12, 0x0000, //awbb_GridCorr_R_0__0_
             0x0F12, 0x0000, //awbb_GridCorr_R_0__1_
             0x0F12, 0x0000, //awbb_GridCorr_R_0__2_
             0x0F12, 0x0000, //awbb_GridCorr_R_0__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_0__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_0__5_
             0x0F12, 0x0000, //awbb_GridCorr_R_1__0_
             0x0F12, 0x0000, //awbb_GridCorr_R_1__1_
             0x0F12, 0x0000, //awbb_GridCorr_R_1__2_
             0x0F12, 0x0000, //awbb_GridCorr_R_1__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_1__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_1__5_
             0x0F12, 0x0000, //awbb_GridCorr_R_2__0_
             0x0F12, 0x0000, //awbb_GridCorr_R_2__1_
             0x0F12, 0x0000, //awbb_GridCorr_R_2__2_
             0x0F12, 0x0000, //awbb_GridCorr_R_2__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_2__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_2__5_
             0x0F12, 0x0000, //awbb_GridCorr_B_0__0_
             0x0F12, 0x0000, //awbb_GridCorr_B_0__1_
             0x0F12, 0x0000, //awbb_GridCorr_B_0__2_
             0x0F12, 0x0000, //awbb_GridCorr_B_0__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_0__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_0__5_
             0x0F12, 0x0000, //awbb_GridCorr_B_1__0_
             0x0F12, 0x0000, //awbb_GridCorr_B_1__1_
             0x0F12, 0x0000, //awbb_GridCorr_B_1__2_
             0x0F12, 0x0000, //awbb_GridCorr_B_1__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_1__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_1__5_
             0x0F12, 0x0000, //awbb_GridCorr_B_2__0_
             0x0F12, 0x0000, //awbb_GridCorr_B_2__1_
             0x0F12, 0x0000, //awbb_GridCorr_B_2__2_
             0x0F12, 0x0000, //awbb_GridCorr_B_2__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_2__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_2__5_

        // Outdoor Grid Offset
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__0_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__1_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__2_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__5_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__0_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__1_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__2_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__5_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__0_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__1_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__2_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__5_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__0_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__1_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__2_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__5_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__0_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__1_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__2_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__5_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__0_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__1_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__2_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__5_
#if 0
             0x002A, 0x13A4,
             0x0F12, 0xFFE0, //awbb_GridCorr_R_0__0_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_0__1_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_0__2_
             0x0F12, 0xFFA0, //awbb_GridCorr_R_0__3_
             0x0F12, 0xFFEE, //awbb_GridCorr_R_0__4_
             0x0F12, 0x0096, //awbb_GridCorr_R_0__5_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_1__0_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_1__1_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_1__2_
             0x0F12, 0xFFA0, //awbb_GridCorr_R_1__3_
             0x0F12, 0xFFEE, //awbb_GridCorr_R_1__4_
             0x0F12, 0x0096, //awbb_GridCorr_R_1__5_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_2__0_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_2__1_
             0x0F12, 0xFFE0, //awbb_GridCorr_R_2__2_
             0x0F12, 0xFFA0, //awbb_GridCorr_R_2__3_
             0x0F12, 0xFFEE, //awbb_GridCorr_R_2__4_
             0x0F12, 0x0096, //awbb_GridCorr_R_2__5_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_0__0_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_0__1_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_0__2_
             0x0F12, 0xFF38, //awbb_GridCorr_B_0__3_
             0x0F12, 0xFEF2, //awbb_GridCorr_B_0__4_
             0x0F12, 0xFE5C, //awbb_GridCorr_B_0__5_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_1__0_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_1__1_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_1__2_
             0x0F12, 0xFF38, //awbb_GridCorr_B_1__3_
             0x0F12, 0xFEF2, //awbb_GridCorr_B_1__4_
             0x0F12, 0xFE5C, //awbb_GridCorr_B_1__5_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_2__0_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_2__1_
             0x0F12, 0xFFC0, //awbb_GridCorr_B_2__2_
             0x0F12, 0xFF38, //awbb_GridCorr_B_2__3_
             0x0F12, 0xFEF2, //awbb_GridCorr_B_2__4_
             0x0F12, 0xFE5C, //awbb_GridCorr_B_2__5_

        // Outdoor Grid Offset
             0x0F12, 0xFFC0, //awbb_GridCorr_R_Out_0__0_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_0__1_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_0__2_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_0__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_0__5_
             0x0F12, 0xFFC0, //awbb_GridCorr_R_Out_1__0_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_1__1_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_1__2_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_1__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_1__5_
             0x0F12, 0xFFC0, //awbb_GridCorr_R_Out_2__0_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_2__1_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_2__2_
             0x0F12, 0xFFD0, //awbb_GridCorr_R_Out_2__3_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__4_
             0x0F12, 0x0000, //awbb_GridCorr_R_Out_2__5_
             0x0F12, 0x0010, //awbb_GridCorr_B_Out_0__0_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_0__1_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_0__2_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_0__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_0__5_
             0x0F12, 0x0010, //awbb_GridCorr_B_Out_1__0_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_1__1_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_1__2_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_1__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_1__5_
             0x0F12, 0x0010, //awbb_GridCorr_B_Out_2__0_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_2__1_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_2__2_
             0x0F12, 0xFFD0, //awbb_GridCorr_B_Out_2__3_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__4_
             0x0F12, 0x0000, //awbb_GridCorr_B_Out_2__5_
#endif
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }

    S5K4ECGX_MIPI_AF_Init();

    //==================================================================================
    // 11.Auto Flicker Detection
    //==================================================================================
    {
        static const kal_uint16 addr_data_pair[] =
        {
             0x002A, 0x0F30,
             0x0F12, 0x0001, //AFC_D_ConvAccelerPower
        //Auto Flicker (60Mhz start)
             0x002A, 0x0F2A,
             0x0F12, 0x0000,  //AFC_Default60Hz 0001:60Hz 0000h:50Hz
             0x002A, 0x04E6,
             0x0F12, 0x077F,  //REG_TC_DBG

    //==================================================================================
    // 12.AE Setting
    //==================================================================================
        //AE Target
             0x002A, 0x1484,
             0x0F12, 0x0030,   //TVAR_ae_BrAve
        //ae_StatMode bit[3] BLC has to be bypassed to prevent AE weight change especially backlight scene
             0x002A, 0x148A,
             0x0F12, 0x000F,   //ae_StatMode
             0x002A, 0x058C,
             0x0F12, 0x3520,
             0x0F12, 0x0000,   //lt_uMaxExp1
             0x0F12, 0xD4C0,
             0x0F12, 0x0001,   //lt_uMaxExp2
             0x0F12, 0x3520,
             0x0F12, 0x0000,   //lt_uCapMaxExp1
             0x0F12, 0xD4C0,
             0x0F12, 0x0001,   //lt_uCapMaxExp2
             0x002A, 0x059C,
             0x0F12, 0x0470,   //lt_uMaxAnGain1
             0x0F12, 0x0C00,   //lt_uMaxAnGain2
             0x0F12, 0x0100,   //lt_uMaxDigGain
             0x0F12, 0x1000,   //lt_uMaxTotGain
             0x002A, 0x0544,
             0x0F12, 0x0111,   //lt_uLimitHigh
             0x0F12, 0x00EF,   //lt_uLimitLow
             0x002A, 0x0608,
             0x0F12, 0x0001,   //lt_ExpGain_uSubsamplingmode
             0x0F12, 0x0001,   //lt_ExpGain_uNonSubsampling
             0x0F12, 0x0800,   //lt_ExpGain_ExpCurveGainMaxStr
             0x0F12, 0x0100,   //0100   //lt_ExpGain_ExpCurveGainMaxStr_0__uMaxDigGain
             0x0F12, 0x0001,   //0001
             0x0F12, 0x0000,   //0000   //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpIn_0_
             0x0F12, 0x0A3C,   //0A3C
             0x0F12, 0x0000,   //0000
             0x0F12, 0x0D05,   //0D05
             0x0F12, 0x0000,   //0000
             0x0F12, 0x4008,   //4008
             0x0F12, 0x0000,   //0000
             0x0F12, 0x7000,   //7400  //?? //700Lux
             0x0F12, 0x0000,   //0000
             0x0F12, 0x9C00,   //C000  //?? //9C00->9F->A5 //400Lux
             0x0F12, 0x0000,   //0000
             0x0F12, 0xAD00,   //AD00
             0x0F12, 0x0001,   //0001
             0x0F12, 0xF1D4,   //F1D4
             0x0F12, 0x0002,   //0002
             0x0F12, 0xDC00,   //DC00
             0x0F12, 0x0005,   //0005
             0x0F12, 0xDC00,   //DC00
             0x0F12, 0x0005,   //0005         //
             0x002A, 0x0638,   //0638
             0x0F12, 0x0001,   //0001
             0x0F12, 0x0000,   //0000   //lt_ExpGain_ExpCurveGainMaxStr_0__ulExpOut_0_
             0x0F12, 0x0A3C,   //0A3C
             0x0F12, 0x0000,   //0000
             0x0F12, 0x0D05,   //0D05
             0x0F12, 0x0000,   //0000
             0x0F12, 0x3408,   //3408
             0x0F12, 0x0000,   //0000
             0x0F12, 0x3408,   //3408
             0x0F12, 0x0000,   //0000
             0x0F12, 0x6810,   //6810
             0x0F12, 0x0000,   //0000
             0x0F12, 0x8214,   //8214
             0x0F12, 0x0000,   //0000
             0x0F12, 0xC350,   //C350
             0x0F12, 0x0000,   //0000
             0x0F12, 0xD4C0,   //C350
             0x0F12, 0x0001,   //0000
             0x0F12, 0xD4C0,   //C350
             0x0F12, 0x0001,   //0000
             0x002A, 0x0660,
             0x0F12, 0x0650,   //lt_ExpGain_ExpCurveGainMaxStr_1_
             0x0F12, 0x0100,   //lt_ExpGain_ExpCurveGainMaxStr_1__uMaxDigGain
             0x002A, 0x06B8,
             0x0F12, 0x452C,
             0x0F12, 0x000A,   //0005   //lt_uMaxLei
             0x002A, 0x05D0,
             0x0F12, 0x0000,   //lt_mbr_Peak_behind
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }

    //==================================================================================
    // 13.AE Weight (Normal)
    //==================================================================================
    S5K4ECGX_MIPI_AE_Rollback_Weight_Table();

    {
        static const kal_uint16 addr_data_pair[] =
        {
             //==================================================================================
             // 14.Flash Setting
             //==================================================================================
             0x002A, 0x0484,
             0x0F12, 0x0002,     //capture flash on
             0x002A, 0x183A,
             0x0F12, 0x0001,     //one frame AE
             0x002A, 0x17F6,
             0x0F12, 0x023C,     //AWB R point
             0x0F12, 0x0248,     //AWB B point
             0x002A, 0x1840,
             0x0F12, 0x0001,     // Fls AE tune start
             0x0F12, 0x0100,     // fls_afl_FlsAFIn  Rin
             0x0F12, 0x0120,
             0x0F12, 0x0180,
             0x0F12, 0x0200,
             0x0F12, 0x0400,
             0x0F12, 0x0800,
             0x0F12, 0x0A00,
             0x0F12, 0x1000,
             0x0F12, 0x0100,     // fls_afl_FlsAFOut  Rout
             0x0F12, 0x00A0,
             0x0F12, 0x0090,
             0x0F12, 0x0080,
             0x0F12, 0x0070,
             0x0F12, 0x0045,
             0x0F12, 0x0030,
             0x0F12, 0x0010,
             0x002A, 0x1884,
             0x0F12, 0x0100,     // fls_afl_FlsNBOut  flash NB default
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x002A, 0x1826,
             0x0F12, 0x0100,     // fls_afl_FlashWP_Weight  flash NB default
             0x0F12, 0x00C0,
             0x0F12, 0x0080,
             0x0F12, 0x000A,
             0x0F12, 0x0000,
             0x0F12, 0x0030,     // fls_afl_FlashWP_Weight  flash NB default
             0x0F12, 0x0040,
             0x0F12, 0x0048,
             0x0F12, 0x0050,
             0x0F12, 0x0060,
             0x002A, 0x4784,
             0x0F12, 0x00A0,     // TNP_Regs_FlsWeightRIn  weight tune start in
             0x0F12, 0x00C0,
             0x0F12, 0x00D0,
             0x0F12, 0x0100,
             0x0F12, 0x0200,
             0x0F12, 0x0300,
             0x0F12, 0x0088,     // TNP_Regs_FlsWeightROut  weight tune start out
             0x0F12, 0x00B0,
             0x0F12, 0x00C0,
             0x0F12, 0x0100,
             0x0F12, 0x0200,
             0x0F12, 0x0300,
             0x0F12, 0x0120,     //Fls  BRIn
             0x0F12, 0x0150,
             0x0F12, 0x0200,
             0x0F12, 0x003C,     // Fls  BROut
             0x0F12, 0x003B,
             0x0F12, 0x0026,     //brightness



    //==================================================================================
    // 15.CCM Setting
    //==================================================================================
             0x002A, 0x08A6,
             0x0F12, 0x00C0, //SARR_AwbCcmCord[0]
             0x0F12, 0x0100, //SARR_AwbCcmCord[1]
             0x0F12, 0x0125, //SARR_AwbCcmCord[2]
             0x0F12, 0x015F, //SARR_AwbCcmCord[3]
             0x0F12, 0x017C, //SARR_AwbCcmCord[4]
             0x0F12, 0x0194, //SARR_AwbCcmCord[5]
             0x002A, 0x0898,
             0x0F12, 0x4800, //TVAR_wbt_pBaseCcms
             0x0F12, 0x7000,
             0x002A, 0x08A0,
             0x0F12, 0x48D8, //TVAR_wbt_pOutdoorCcm
             0x0F12, 0x7000,
        //Horizon
             0x002A, 0x4800,
             0x0F12, 0x0208, //TVAR_wbt_pBaseCcms[0]
             0x0F12, 0xFFB5, //TVAR_wbt_pBaseCcms[1]
             0x0F12, 0xFFE8, //TVAR_wbt_pBaseCcms[2]
             0x0F12, 0xFF20, //TVAR_wbt_pBaseCcms[3]
             0x0F12, 0x01BF, //TVAR_wbt_pBaseCcms[4]
             0x0F12, 0xFF53, //TVAR_wbt_pBaseCcms[5]
             0x0F12, 0x0022, //TVAR_wbt_pBaseCcms[6]
             0x0F12, 0xFFEA, //TVAR_wbt_pBaseCcms[7]
             0x0F12, 0x01C2, //TVAR_wbt_pBaseCcms[8]
             0x0F12, 0x00C6, //TVAR_wbt_pBaseCcms[9]
             0x0F12, 0x0095, //TVAR_wbt_pBaseCcms[10]
             0x0F12, 0xFEFD, //TVAR_wbt_pBaseCcms[11]
             0x0F12, 0x0206, //TVAR_wbt_pBaseCcms[12]
             0x0F12, 0xFF7F, //TVAR_wbt_pBaseCcms[13]
             0x0F12, 0x0191, //TVAR_wbt_pBaseCcms[14]
             0x0F12, 0xFF06, //TVAR_wbt_pBaseCcms[15]
             0x0F12, 0x01BA, //TVAR_wbt_pBaseCcms[16]
             0x0F12, 0x0108, //TVAR_wbt_pBaseCcms[17]

        // INCA A
             0x0F12, 0x0208, //TVAR_wbt_pBaseCcms[18]
             0x0F12, 0xFFB5, //TVAR_wbt_pBaseCcms[19]
             0x0F12, 0xFFE8, //TVAR_wbt_pBaseCcms[20]
             0x0F12, 0xFF20, //TVAR_wbt_pBaseCcms[21]
             0x0F12, 0x01BF, //TVAR_wbt_pBaseCcms[22]
             0x0F12, 0xFF53, //TVAR_wbt_pBaseCcms[23]
             0x0F12, 0x0022, //TVAR_wbt_pBaseCcms[24]
             0x0F12, 0xFFEA, //TVAR_wbt_pBaseCcms[25]
             0x0F12, 0x01C2, //TVAR_wbt_pBaseCcms[26]
             0x0F12, 0x00C6, //TVAR_wbt_pBaseCcms[27]
             0x0F12, 0x0095, //TVAR_wbt_pBaseCcms[28]
             0x0F12, 0xFEFD, //TVAR_wbt_pBaseCcms[29]
             0x0F12, 0x0206, //TVAR_wbt_pBaseCcms[30]
             0x0F12, 0xFF7F, //TVAR_wbt_pBaseCcms[31]
             0x0F12, 0x0191, //TVAR_wbt_pBaseCcms[32]
             0x0F12, 0xFF06, //TVAR_wbt_pBaseCcms[33]
             0x0F12, 0x01BA, //TVAR_wbt_pBaseCcms[34]
             0x0F12, 0x0108, //TVAR_wbt_pBaseCcms[35]
        //Warm White
             0x0F12, 0x0208, //TVAR_wbt_pBaseCcms[36]
             0x0F12, 0xFFB5, //TVAR_wbt_pBaseCcms[37]
             0x0F12, 0xFFE8, //TVAR_wbt_pBaseCcms[38]
             0x0F12, 0xFF20, //TVAR_wbt_pBaseCcms[39]
             0x0F12, 0x01BF, //TVAR_wbt_pBaseCcms[40]
             0x0F12, 0xFF53, //TVAR_wbt_pBaseCcms[41]
             0x0F12, 0x0022, //TVAR_wbt_pBaseCcms[42]
             0x0F12, 0xFFEA, //TVAR_wbt_pBaseCcms[43]
             0x0F12, 0x01C2, //TVAR_wbt_pBaseCcms[44]
             0x0F12, 0x00C6, //TVAR_wbt_pBaseCcms[45]
             0x0F12, 0x0095, //TVAR_wbt_pBaseCcms[46]
             0x0F12, 0xFEFD, //TVAR_wbt_pBaseCcms[47]
             0x0F12, 0x0206, //TVAR_wbt_pBaseCcms[48]
             0x0F12, 0xFF7F, //TVAR_wbt_pBaseCcms[49]
             0x0F12, 0x0191, //TVAR_wbt_pBaseCcms[50]
             0x0F12, 0xFF06, //TVAR_wbt_pBaseCcms[51]
             0x0F12, 0x01BA, //TVAR_wbt_pBaseCcms[52]
             0x0F12, 0x0108, //TVAR_wbt_pBaseCcms[53]
        //Cool White
             0x0F12, 0x0204, //TVAR_wbt_pBaseCcms[54]
             0x0F12, 0xFFB2, //TVAR_wbt_pBaseCcms[55]
             0x0F12, 0xFFF5, //TVAR_wbt_pBaseCcms[56]
             0x0F12, 0xFEF1, //TVAR_wbt_pBaseCcms[57]
             0x0F12, 0x014E, //TVAR_wbt_pBaseCcms[58]
             0x0F12, 0xFF18, //TVAR_wbt_pBaseCcms[59]
             0x0F12, 0xFFE6, //TVAR_wbt_pBaseCcms[60]
             0x0F12, 0xFFDD, //TVAR_wbt_pBaseCcms[61]
             0x0F12, 0x01B2, //TVAR_wbt_pBaseCcms[62]
             0x0F12, 0x00F2, //TVAR_wbt_pBaseCcms[63]
             0x0F12, 0x00CA, //TVAR_wbt_pBaseCcms[64]
             0x0F12, 0xFF48, //TVAR_wbt_pBaseCcms[65]
             0x0F12, 0x0151, //TVAR_wbt_pBaseCcms[66]
             0x0F12, 0xFF50, //TVAR_wbt_pBaseCcms[67]
             0x0F12, 0x0147, //TVAR_wbt_pBaseCcms[68]
             0x0F12, 0xFF75, //TVAR_wbt_pBaseCcms[69]
             0x0F12, 0x0187, //TVAR_wbt_pBaseCcms[70]
             0x0F12, 0x01BF, //TVAR_wbt_pBaseCcms[71]
        //D50
             0x0F12, 0x0204, //TVAR_wbt_pBaseCcms[72]
             0x0F12, 0xFFB2, //TVAR_wbt_pBaseCcms[73]
             0x0F12, 0xFFF5, //TVAR_wbt_pBaseCcms[74]
             0x0F12, 0xFEF1, //TVAR_wbt_pBaseCcms[75]
             0x0F12, 0x014E, //TVAR_wbt_pBaseCcms[76]
             0x0F12, 0xFF18, //TVAR_wbt_pBaseCcms[77]
             0x0F12, 0xFFE6, //TVAR_wbt_pBaseCcms[78]
             0x0F12, 0xFFDD, //TVAR_wbt_pBaseCcms[79]
             0x0F12, 0x01B2, //TVAR_wbt_pBaseCcms[80]
             0x0F12, 0x00F2, //TVAR_wbt_pBaseCcms[81]
             0x0F12, 0x00CA, //TVAR_wbt_pBaseCcms[82]
             0x0F12, 0xFF48, //TVAR_wbt_pBaseCcms[83]
             0x0F12, 0x0151, //TVAR_wbt_pBaseCcms[84]
             0x0F12, 0xFF50, //TVAR_wbt_pBaseCcms[85]
             0x0F12, 0x0147, //TVAR_wbt_pBaseCcms[86]
             0x0F12, 0xFF75, //TVAR_wbt_pBaseCcms[87]
             0x0F12, 0x0187, //TVAR_wbt_pBaseCcms[88]
             0x0F12, 0x01BF, //TVAR_wbt_pBaseCcms[89]
        //D65
             0x0F12, 0x0204, //TVAR_wbt_pBaseCcms[90]
             0x0F12, 0xFFB2, //TVAR_wbt_pBaseCcms[91]
             0x0F12, 0xFFF5, //TVAR_wbt_pBaseCcms[92]
             0x0F12, 0xFEF1, //TVAR_wbt_pBaseCcms[93]
             0x0F12, 0x014E, //TVAR_wbt_pBaseCcms[94]
             0x0F12, 0xFF18, //TVAR_wbt_pBaseCcms[95]
             0x0F12, 0xFFE6, //TVAR_wbt_pBaseCcms[96]
             0x0F12, 0xFFDD, //TVAR_wbt_pBaseCcms[97]
             0x0F12, 0x01B2, //TVAR_wbt_pBaseCcms[98]
             0x0F12, 0x00F2, //TVAR_wbt_pBaseCcms[99]
             0x0F12, 0x00CA, //TVAR_wbt_pBaseCcms[100]
             0x0F12, 0xFF48, //TVAR_wbt_pBaseCcms[101]
             0x0F12, 0x0151, //TVAR_wbt_pBaseCcms[102]
             0x0F12, 0xFF50, //TVAR_wbt_pBaseCcms[103]
             0x0F12, 0x0147, //TVAR_wbt_pBaseCcms[104]
             0x0F12, 0xFF75, //TVAR_wbt_pBaseCcms[105]
             0x0F12, 0x0187, //TVAR_wbt_pBaseCcms[106]
             0x0F12, 0x01BF, //TVAR_wbt_pBaseCcms[107]
        //Outdoor
             0x0F12, 0x01E5, //TVAR_wbt_pOutdoorCcm[0]
             0x0F12, 0xFFA4, //TVAR_wbt_pOutdoorCcm[1]
             0x0F12, 0xFFDC, //TVAR_wbt_pOutdoorCcm[2]
             0x0F12, 0xFE90, //TVAR_wbt_pOutdoorCcm[3]
             0x0F12, 0x013F, //TVAR_wbt_pOutdoorCcm[4]
             0x0F12, 0xFF1B, //TVAR_wbt_pOutdoorCcm[5]
             0x0F12, 0xFFD2, //TVAR_wbt_pOutdoorCcm[6]
             0x0F12, 0xFFDF, //TVAR_wbt_pOutdoorCcm[7]
             0x0F12, 0x0236, //TVAR_wbt_pOutdoorCcm[8]
             0x0F12, 0x00EC, //TVAR_wbt_pOutdoorCcm[9]
             0x0F12, 0x00F8, //TVAR_wbt_pOutdoorCcm[10]
             0x0F12, 0xFF34, //TVAR_wbt_pOutdoorCcm[11]
             0x0F12, 0x01CE, //TVAR_wbt_pOutdoorCcm[12]
             0x0F12, 0xFF83, //TVAR_wbt_pOutdoorCcm[13]
             0x0F12, 0x0195, //TVAR_wbt_pOutdoorCcm[14]
             0x0F12, 0xFEF3, //TVAR_wbt_pOutdoorCcm[15]
             0x0F12, 0x0126, //TVAR_wbt_pOutdoorCcm[16]
             0x0F12, 0x0162, //TVAR_wbt_pOutdoorCcm[17]

    //==================================================================================
    // 16.GAMMA
    //==================================================================================
             0x002A,  0x0734,
             0x0F12,  0x0000,   //0000  //saRR_usDualGammaLutRGBIndoor[0][0]
             0x0F12,  0x0007,   //000A  //saRR_usDualGammaLutRGBIndoor[0][1]
             0x0F12,  0x0012,   //0016  //saRR_usDualGammaLutRGBIndoor[0][2]
             0x0F12,  0x0028,   //0030  //saRR_usDualGammaLutRGBIndoor[0][3]
             0x0F12,  0x0050,   //0066  //saRR_usDualGammaLutRGBIndoor[0][4]
             0x0F12,  0x00D5,   //00D5  //saRR_usDualGammaLutRGBIndoor[0][5]
             0x0F12,  0x0138,   //0138  //saRR_usDualGammaLutRGBIndoor[0][6]
             0x0F12,  0x0163,   //0163  //saRR_usDualGammaLutRGBIndoor[0][7]
             0x0F12,  0x0189,   //0189  //saRR_usDualGammaLutRGBIndoor[0][8]
             0x0F12,  0x01C6,   //01C6  //saRR_usDualGammaLutRGBIndoor[0][9]
             0x0F12,  0x01F8,   //01F8  //saRR_usDualGammaLutRGBIndoor[0][10]
             0x0F12,  0x0222,   //0222  //saRR_usDualGammaLutRGBIndoor[0][11]
             0x0F12,  0x0247,   //0247  //saRR_usDualGammaLutRGBIndoor[0][12]
             0x0F12,  0x0282,   //0282  //saRR_usDualGammaLutRGBIndoor[0][13]
             0x0F12,  0x02B5,   //02B5  //saRR_usDualGammaLutRGBIndoor[0][14]
             0x0F12,  0x0305,   //030F  //saRR_usDualGammaLutRGBIndoor[0][15]
             0x0F12,  0x034C,   //035F  //saRR_usDualGammaLutRGBIndoor[0][16]
             0x0F12,  0x0388,   //03A2  //saRR_usDualGammaLutRGBIndoor[0][17]
             0x0F12,  0x03B8,   //03D8  //saRR_usDualGammaLutRGBIndoor[0][18]
             0x0F12,  0x03E8,   //03FF  //saRR_usDualGammaLutRGBIndoor[0][19]
             0x0F12,  0x0000,   //0000  //saRR_usDualGammaLutRGBIndoor[1][0]
             0x0F12,  0x0007,   //000A  //saRR_usDualGammaLutRGBIndoor[1][1]
             0x0F12,  0x0012,   //0016  //saRR_usDualGammaLutRGBIndoor[1][2]
             0x0F12,  0x0028,   //0030  //saRR_usDualGammaLutRGBIndoor[1][3]
             0x0F12,  0x0050,   //0066  //saRR_usDualGammaLutRGBIndoor[1][4]
             0x0F12,  0x00D5,   //00D5  //saRR_usDualGammaLutRGBIndoor[1][5]
             0x0F12,  0x0138,   //0138  //saRR_usDualGammaLutRGBIndoor[1][6]
             0x0F12,  0x0163,   //0163  //saRR_usDualGammaLutRGBIndoor[1][7]
             0x0F12,  0x0189,   //0189  //saRR_usDualGammaLutRGBIndoor[1][8]
             0x0F12,  0x01C6,   //01C6  //saRR_usDualGammaLutRGBIndoor[1][9]
             0x0F12,  0x01F8,   //01F8  //saRR_usDualGammaLutRGBIndoor[1][10]
             0x0F12,  0x0222,   //0222  //saRR_usDualGammaLutRGBIndoor[1][11]
             0x0F12,  0x0247,   //0247  //saRR_usDualGammaLutRGBIndoor[1][12]
             0x0F12,  0x0282,   //0282  //saRR_usDualGammaLutRGBIndoor[1][13]
             0x0F12,  0x02B5,   //02B5  //saRR_usDualGammaLutRGBIndoor[1][14]
             0x0F12,  0x0305,   //030F  //saRR_usDualGammaLutRGBIndoor[1][15]
             0x0F12,  0x034C,   //035F  //saRR_usDualGammaLutRGBIndoor[1][16]
             0x0F12,  0x0388,   //03A2  //saRR_usDualGammaLutRGBIndoor[1][17]
             0x0F12,  0x03B8,   //03D8  //saRR_usDualGammaLutRGBIndoor[1][18]
             0x0F12,  0x03E8,   //03FF  //saRR_usDualGammaLutRGBIndoor[1][19]
             0x0F12,  0x0000,   //0000  //saRR_usDualGammaLutRGBIndoor[2][0]
             0x0F12,  0x0007,   //000A  //saRR_usDualGammaLutRGBIndoor[2][1]
             0x0F12,  0x0012,   //0016  //saRR_usDualGammaLutRGBIndoor[2][2]
             0x0F12,  0x0028,   //0030  //saRR_usDualGammaLutRGBIndoor[2][3]
             0x0F12,  0x0050,   //0066  //saRR_usDualGammaLutRGBIndoor[2][4]
             0x0F12,  0x00D5,   //00D5  //saRR_usDualGammaLutRGBIndoor[2][5]
             0x0F12,  0x0138,   //0138  //saRR_usDualGammaLutRGBIndoor[2][6]
             0x0F12,  0x0163,   //0163  //saRR_usDualGammaLutRGBIndoor[2][7]
             0x0F12,  0x0189,   //0189  //saRR_usDualGammaLutRGBIndoor[2][8]
             0x0F12,  0x01C6,   //01C6  //saRR_usDualGammaLutRGBIndoor[2][9]
             0x0F12,  0x01F8,   //01F8  //saRR_usDualGammaLutRGBIndoor[2][10]
             0x0F12,  0x0222,   //0222  //saRR_usDualGammaLutRGBIndoor[2][11]
             0x0F12,  0x0247,   //0247  //saRR_usDualGammaLutRGBIndoor[2][12]
             0x0F12,  0x0282,   //0282  //saRR_usDualGammaLutRGBIndoor[2][13]
             0x0F12,  0x02B5,   //02B5  //saRR_usDualGammaLutRGBIndoor[2][14]
             0x0F12,  0x0305,   //030F  //saRR_usDualGammaLutRGBIndoor[2][15]
             0x0F12,  0x034C,   //035F  //saRR_usDualGammaLutRGBIndoor[2][16]
             0x0F12,  0x0388,   //03A2  //saRR_usDualGammaLutRGBIndoor[2][17]
             0x0F12,  0x03B8,   //03D8  //saRR_usDualGammaLutRGBIndoor[2][18]
             0x0F12,  0x03E8,   //03FF  //saRR_usDualGammaLutRGBIndoor[2][19]
             0x0F12,  0x0000,  //saRR_usDualGammaLutRGBOutdoor[0][0]
             0x0F12,  0x000B,  //saRR_usDualGammaLutRGBOutdoor[0][1]
             0x0F12,  0x0019,  //saRR_usDualGammaLutRGBOutdoor[0][2]
             0x0F12,  0x0036,  //saRR_usDualGammaLutRGBOutdoor[0][3]
             0x0F12,  0x006F,  //saRR_usDualGammaLutRGBOutdoor[0][4]
             0x0F12,  0x00D8,  //saRR_usDualGammaLutRGBOutdoor[0][5]
             0x0F12,  0x0135,  //saRR_usDualGammaLutRGBOutdoor[0][6]
             0x0F12,  0x015F,  //saRR_usDualGammaLutRGBOutdoor[0][7]
             0x0F12,  0x0185,  //saRR_usDualGammaLutRGBOutdoor[0][8]
             0x0F12,  0x01C1,  //saRR_usDualGammaLutRGBOutdoor[0][9]
             0x0F12,  0x01F3,  //saRR_usDualGammaLutRGBOutdoor[0][10]
             0x0F12,  0x0220,  //saRR_usDualGammaLutRGBOutdoor[0][11]
             0x0F12,  0x024A,  //saRR_usDualGammaLutRGBOutdoor[0][12]
             0x0F12,  0x0291,  //saRR_usDualGammaLutRGBOutdoor[0][13]
             0x0F12,  0x02D0,  //saRR_usDualGammaLutRGBOutdoor[0][14]
             0x0F12,  0x032A,  //saRR_usDualGammaLutRGBOutdoor[0][15]
             0x0F12,  0x036A,  //saRR_usDualGammaLutRGBOutdoor[0][16]
             0x0F12,  0x039F,  //saRR_usDualGammaLutRGBOutdoor[0][17]
             0x0F12,  0x03CC,  //saRR_usDualGammaLutRGBOutdoor[0][18]
             0x0F12,  0x03F9,  //saRR_usDualGammaLutRGBOutdoor[0][19]
             0x0F12,  0x0000,  //saRR_usDualGammaLutRGBOutdoor[1][0]
             0x0F12,  0x000B,  //saRR_usDualGammaLutRGBOutdoor[1][1]
             0x0F12,  0x0019,  //saRR_usDualGammaLutRGBOutdoor[1][2]
             0x0F12,  0x0036,  //saRR_usDualGammaLutRGBOutdoor[1][3]
             0x0F12,  0x006F,  //saRR_usDualGammaLutRGBOutdoor[1][4]
             0x0F12,  0x00D8,  //saRR_usDualGammaLutRGBOutdoor[1][5]
             0x0F12,  0x0135,  //saRR_usDualGammaLutRGBOutdoor[1][6]
             0x0F12,  0x015F,  //saRR_usDualGammaLutRGBOutdoor[1][7]
             0x0F12,  0x0185,  //saRR_usDualGammaLutRGBOutdoor[1][8]
             0x0F12,  0x01C1,  //saRR_usDualGammaLutRGBOutdoor[1][9]
             0x0F12,  0x01F3,  //saRR_usDualGammaLutRGBOutdoor[1][10]
             0x0F12,  0x0220,  //saRR_usDualGammaLutRGBOutdoor[1][11]
             0x0F12,  0x024A,  //saRR_usDualGammaLutRGBOutdoor[1][12]
             0x0F12,  0x0291,  //saRR_usDualGammaLutRGBOutdoor[1][13]
             0x0F12,  0x02D0,  //saRR_usDualGammaLutRGBOutdoor[1][14]
             0x0F12,  0x032A,  //saRR_usDualGammaLutRGBOutdoor[1][15]
             0x0F12,  0x036A,  //saRR_usDualGammaLutRGBOutdoor[1][16]
             0x0F12,  0x039F,  //saRR_usDualGammaLutRGBOutdoor[1][17]
             0x0F12,  0x03CC,  //saRR_usDualGammaLutRGBOutdoor[1][18]
             0x0F12,  0x03F9,  //saRR_usDualGammaLutRGBOutdoor[1][19]
             0x0F12,  0x0000,  //saRR_usDualGammaLutRGBOutdoor[2][0]
             0x0F12,  0x000B,  //saRR_usDualGammaLutRGBOutdoor[2][1]
             0x0F12,  0x0019,  //saRR_usDualGammaLutRGBOutdoor[2][2]
             0x0F12,  0x0036,  //saRR_usDualGammaLutRGBOutdoor[2][3]
             0x0F12,  0x006F,  //saRR_usDualGammaLutRGBOutdoor[2][4]
             0x0F12,  0x00D8,  //saRR_usDualGammaLutRGBOutdoor[2][5]
             0x0F12,  0x0135,  //saRR_usDualGammaLutRGBOutdoor[2][6]
             0x0F12,  0x015F,  //saRR_usDualGammaLutRGBOutdoor[2][7]
             0x0F12,  0x0185,  //saRR_usDualGammaLutRGBOutdoor[2][8]
             0x0F12,  0x01C1,  //saRR_usDualGammaLutRGBOutdoor[2][9]
             0x0F12,  0x01F3,  //saRR_usDualGammaLutRGBOutdoor[2][10]
             0x0F12,  0x0220,  //saRR_usDualGammaLutRGBOutdoor[2][11]
             0x0F12,  0x024A,  //saRR_usDualGammaLutRGBOutdoor[2][12]
             0x0F12,  0x0291,  //saRR_usDualGammaLutRGBOutdoor[2][13]
             0x0F12,  0x02D0,  //saRR_usDualGammaLutRGBOutdoor[2][14]
             0x0F12,  0x032A,  //saRR_usDualGammaLutRGBOutdoor[2][15]
             0x0F12,  0x036A,  //saRR_usDualGammaLutRGBOutdoor[2][16]
             0x0F12,  0x039F,  //saRR_usDualGammaLutRGBOutdoor[2][17]
             0x0F12,  0x03CC,  //saRR_usDualGammaLutRGBOutdoor[2][18]
             0x0F12,  0x03F9,  //saRR_usDualGammaLutRGBOutdoor[2][19]



    //==================================================================================
    // 17.AFIT
    //==================================================================================
             0x002A, 0x0944,
             0x0F12, 0x0050, //afit_uNoiseIndInDoor
             0x0F12, 0x00B0, //afit_uNoiseIndInDoor
             0x0F12, 0x0196, //afit_uNoiseIndInDoor
             0x0F12, 0x0245, //afit_uNoiseIndInDoor
             0x0F12, 0x0300, //afit_uNoiseIndInDoor
             0x002A, 0x0938,
             0x0F12, 0x0000, // on/off AFIT by NB option
             0x0F12, 0x0014, //SARR_uNormBrInDoor
             0x0F12, 0x00D2, //SARR_uNormBrInDoor
             0x0F12, 0x0384, //SARR_uNormBrInDoor
             0x0F12, 0x07D0, //SARR_uNormBrInDoor
             0x0F12, 0x1388, //SARR_uNormBrInDoor
             0x002A, 0x0976,
             0x0F12, 0x0070, //afit_usGamutTh
             0x0F12, 0x0005, //afit_usNeargrayOffset
             0x0F12, 0x0000, //afit_bUseSenBpr
             0x0F12, 0x01CC, //afit_usBprThr_0_
             0x0F12, 0x01CC, //afit_usBprThr_1_
             0x0F12, 0x01CC, //afit_usBprThr_2_
             0x0F12, 0x01CC, //afit_usBprThr_3_
             0x0F12, 0x01CC, //afit_usBprThr_4_
             0x0F12, 0x0180, //afit_NIContrastAFITValue
             0x0F12, 0x0196, //afit_NIContrastTh
             0x002A, 0x098C,
             0x0F12, 0x0000, //7000098C//AFIT16_BRIGHTNESS
             0x0F12, 0x0000, //7000098E//AFIT16_CONTRAST
             0x0F12, 0x0000, //70000990//AFIT16_SATURATION
             0x0F12, 0x0000, //70000992//AFIT16_SHARP_BLUR
             0x0F12, 0x0000, //70000994//AFIT16_GLAMOUR
             0x0F12, 0x00C0, //70000996//AFIT16_bnr_edge_high
             0x0F12, 0x0064, //70000998//AFIT16_postdmsc_iLowBright
             0x0F12, 0x0384, //7000099A//AFIT16_postdmsc_iHighBright
             0x0F12, 0x005F, //7000099C//AFIT16_postdmsc_iLowSat
             0x0F12, 0x01F4, //7000099E//AFIT16_postdmsc_iHighSat
             0x0F12, 0x0070, //700009A0//AFIT16_postdmsc_iTune
             0x0F12, 0x0040, //700009A2//AFIT16_yuvemix_mNegRanges_0
             0x0F12, 0x00A0, //700009A4//AFIT16_yuvemix_mNegRanges_1
             0x0F12, 0x0100, //700009A6//AFIT16_yuvemix_mNegRanges_2
             0x0F12, 0x0010, //700009A8//AFIT16_yuvemix_mPosRanges_0
             0x0F12, 0x0040, //700009AA//AFIT16_yuvemix_mPosRanges_1
             0x0F12, 0x00A0, //700009AC//AFIT16_yuvemix_mPosRanges_2
             0x0F12, 0x1430, //700009AE//AFIT8_bnr_edge_low  [7:0] AFIT8_bnr_repl_thresh
             0x0F12, 0x0201, //700009B0//AFIT8_bnr_repl_force  [7:0] AFIT8_bnr_iHotThreshHigh
             0x0F12, 0x0204, //700009B2//AFIT8_bnr_iHotThreshLow   [7:0] AFIT8_bnr_iColdThreshHigh
             0x0F12, 0x3604, //700009B4//AFIT8_bnr_iColdThreshLow   [7:0] AFIT8_bnr_DispTH_Low
             0x0F12, 0x032A, //700009B6//AFIT8_bnr_DispTH_High   [7:0] AFIT8_bnr_DISP_Limit_Low
             0x0F12, 0x0403, //700009B8//AFIT8_bnr_DISP_Limit_High   [7:0] AFIT8_bnr_iDistSigmaMin
             0x0F12, 0x1B06, //700009BA//AFIT8_bnr_iDistSigmaMax   [7:0] AFIT8_bnr_iDiffSigmaLow
             0x0F12, 0x6015, //700009BC//AFIT8_bnr_iDiffSigmaHigh   [7:0] AFIT8_bnr_iNormalizedSTD_TH
             0x0F12, 0x00C0, //700009BE//AFIT8_bnr_iNormalizedSTD_Limit [7:0] AFIT8_bnr_iDirNRTune
             0x0F12, 0x6080, //700009C0//AFIT8_bnr_iDirMinThres [7:0] AFIT8_bnr_iDirFltDiffThresHigh
             0x0F12, 0x4080, //700009C2//AFIT8_bnr_iDirFltDiffThresLow   [7:0] AFIT8_bnr_iDirSmoothPowerHigh
             0x0F12, 0x0640, //700009C4//AFIT8_bnr_iDirSmoothPowerLow   [7:0] AFIT8_bnr_iLowMaxSlopeAllowed
             0x0F12, 0x0306, //700009C6//AFIT8_bnr_iHighMaxSlopeAllowed [7:0] AFIT8_bnr_iLowSlopeThresh
             0x0F12, 0x2003, //700009C8//AFIT8_bnr_iHighSlopeThresh [7:0] AFIT8_bnr_iSlopenessTH
             0x0F12, 0xFF01, //700009CA//AFIT8_bnr_iSlopeBlurStrength   [7:0] AFIT8_bnr_iSlopenessLimit
             0x0F12, 0x0000, //700009CC//AFIT8_bnr_AddNoisePower1   [7:0] AFIT8_bnr_AddNoisePower2
             0x0F12, 0x0400, //700009CE//AFIT8_bnr_iRadialTune   [7:0] AFIT8_bnr_iRadialPower
             0x0F12, 0x365A, //700009D0//AFIT8_bnr_iRadialLimit [7:0] AFIT8_ee_iFSMagThLow
             0x0F12, 0x102A, //700009D2//AFIT8_ee_iFSMagThHigh   [7:0] AFIT8_ee_iFSVarThLow
             0x0F12, 0x000B, //700009D4//AFIT8_ee_iFSVarThHigh   [7:0] AFIT8_ee_iFSThLow
             0x0F12, 0x0600, //700009D6//AFIT8_ee_iFSThHigh [7:0] AFIT8_ee_iFSmagPower
             0x0F12, 0x5A0F, //700009D8//AFIT8_ee_iFSVarCountTh [7:0] AFIT8_ee_iRadialLimit
             0x0F12, 0x0505, //700009DA//AFIT8_ee_iRadialPower   [7:0] AFIT8_ee_iSmoothEdgeSlope
             0x0F12, 0x1802, //700009DC//AFIT8_ee_iROADThres   [7:0] AFIT8_ee_iROADMaxNR
             0x0F12, 0x0000, //700009DE//AFIT8_ee_iROADSubMaxNR [7:0] AFIT8_ee_iROADSubThres
             0x0F12, 0x2006, //700009E0//AFIT8_ee_iROADNeiThres [7:0] AFIT8_ee_iROADNeiMaxNR
             0x0F12, 0x3028, //700009E2//AFIT8_ee_iSmoothEdgeThres   [7:0] AFIT8_ee_iMSharpen
             0x0F12, 0x0418, //700009E4//AFIT8_ee_iWSharpen [7:0] AFIT8_ee_iMShThresh
             0x0F12, 0x0101, //700009E6//AFIT8_ee_iWShThresh   [7:0] AFIT8_ee_iReduceNegative
             0x0F12, 0x0800, //700009E8//AFIT8_ee_iEmbossCentAdd   [7:0] AFIT8_ee_iShDespeckle
             0x0F12, 0x1804, //700009EA//AFIT8_ee_iReduceEdgeThresh [7:0] AFIT8_dmsc_iEnhThresh
             0x0F12, 0x4008, //700009EC//AFIT8_dmsc_iDesatThresh   [7:0] AFIT8_dmsc_iDemBlurHigh
             0x0F12, 0x0540, //700009EE//AFIT8_dmsc_iDemBlurLow [7:0] AFIT8_dmsc_iDemBlurRange
             0x0F12, 0x8006, //700009F0//AFIT8_dmsc_iDecisionThresh [7:0] AFIT8_dmsc_iCentGrad
             0x0F12, 0x0020, //700009F2//AFIT8_dmsc_iMonochrom   [7:0] AFIT8_dmsc_iGBDenoiseVal
             0x0F12, 0x0000, //700009F4//AFIT8_dmsc_iGRDenoiseVal   [7:0] AFIT8_dmsc_iEdgeDesatThrHigh
             0x0F12, 0x1800, //700009F6//AFIT8_dmsc_iEdgeDesatThrLow   [7:0] AFIT8_dmsc_iEdgeDesat
             0x0F12, 0x0000, //700009F8//AFIT8_dmsc_iNearGrayDesat   [7:0] AFIT8_dmsc_iEdgeDesatLimit
             0x0F12, 0x1E10, //700009FA//AFIT8_postdmsc_iBCoeff [7:0] AFIT8_postdmsc_iGCoeff
             0x0F12, 0x000B, //700009FC//AFIT8_postdmsc_iWideMult   [7:0] AFIT8_yuvemix_mNegSlopes_0
             0x0F12, 0x0607, //700009FE//AFIT8_yuvemix_mNegSlopes_1 [7:0] AFIT8_yuvemix_mNegSlopes_2
             0x0F12, 0x0005, //70000A00//AFIT8_yuvemix_mNegSlopes_3 [7:0] AFIT8_yuvemix_mPosSlopes_0
             0x0F12, 0x0607, //70000A02//AFIT8_yuvemix_mPosSlopes_1 [7:0] AFIT8_yuvemix_mPosSlopes_2
             0x0F12, 0x0405, //70000A04//AFIT8_yuvemix_mPosSlopes_3 [7:0] AFIT8_yuviirnr_iXSupportY
             0x0F12, 0x0205, //70000A06//AFIT8_yuviirnr_iXSupportUV [7:0] AFIT8_yuviirnr_iLowYNorm
             0x0F12, 0x0304, //70000A08//AFIT8_yuviirnr_iHighYNorm   [7:0] AFIT8_yuviirnr_iLowUVNorm
             0x0F12, 0x0409, //70000A0A//AFIT8_yuviirnr_iHighUVNorm [7:0] AFIT8_yuviirnr_iYNormShift
             0x0F12, 0x0306, //70000A0C//AFIT8_yuviirnr_iUVNormShift   [7:0] AFIT8_yuviirnr_iVertLength_Y
             0x0F12, 0x0407, //70000A0E//AFIT8_yuviirnr_iVertLength_UV   [7:0] AFIT8_yuviirnr_iDiffThreshL_Y
             0x0F12, 0x1C04, //70000A10//AFIT8_yuviirnr_iDiffThreshH_Y   [7:0] AFIT8_yuviirnr_iDiffThreshL_UV
             0x0F12, 0x0214, //70000A12//AFIT8_yuviirnr_iDiffThreshH_UV [7:0] AFIT8_yuviirnr_iMaxThreshL_Y
             0x0F12, 0x1002, //70000A14//AFIT8_yuviirnr_iMaxThreshH_Y   [7:0] AFIT8_yuviirnr_iMaxThreshL_UV
             0x0F12, 0x0610, //70000A16//AFIT8_yuviirnr_iMaxThreshH_UV   [7:0] AFIT8_yuviirnr_iYNRStrengthL
             0x0F12, 0x1A02, //70000A18//AFIT8_yuviirnr_iYNRStrengthH   [7:0] AFIT8_yuviirnr_iUVNRStrengthL
             0x0F12, 0x4A18, //70000A1A//AFIT8_yuviirnr_iUVNRStrengthH   [7:0] AFIT8_byr_gras_iShadingPower
             0x0F12, 0x0080, //70000A1C//AFIT8_RGBGamma2_iLinearity [7:0] AFIT8_RGBGamma2_iDarkReduce
             0x0F12, 0x0048, //70000A1E//AFIT8_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
             0x0F12, 0x0180, //70000A20//AFIT8_RGB2YUV_iRGBGain [7:0] AFIT8_bnr_nClustLevel_H
             0x0F12, 0x0A0A, //70000A22//AFIT8_bnr_iClustMulT_H [7:0] AFIT8_bnr_iClustMulT_C
             0x0F12, 0x0101, //70000A24//AFIT8_bnr_iClustThresh_H   [7:0] AFIT8_bnr_iClustThresh_C
             0x0F12, 0x2A36, //70000A26//AFIT8_bnr_iDenThreshLow   [7:0] AFIT8_bnr_iDenThreshHigh
             0x0F12, 0x6024, //70000A28//AFIT8_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
             0x0F12, 0x2A36, //70000A2A//AFIT8_ee_iLowShDenoise [7:0] AFIT8_ee_iHighShDenoise
             0x0F12, 0xFFFF, //70000A2C//AFIT8_ee_iLowSharpClamp   [7:0] AFIT8_ee_iHighSharpClamp
             0x0F12, 0x0808, //70000A2E//AFIT8_ee_iReduceEdgeMinMult   [7:0] AFIT8_ee_iReduceEdgeSlope
             0x0F12, 0x0A01, //70000A30//AFIT8_bnr_nClustLevel_H_Bin   [7:0] AFIT8_bnr_iClustMulT_H_Bin
             0x0F12, 0x010A, //70000A32//AFIT8_bnr_iClustMulT_C_Bin [7:0] AFIT8_bnr_iClustThresh_H_Bin
             0x0F12, 0x3601, //70000A34//AFIT8_bnr_iClustThresh_C_Bin   [7:0] AFIT8_bnr_iDenThreshLow_Bin
             0x0F12, 0x242A, //70000A36//AFIT8_bnr_iDenThreshHigh_Bin   [7:0] AFIT8_ee_iLowSharpPower_Bin
             0x0F12, 0x3660, //70000A38//AFIT8_ee_iHighSharpPower_Bin   [7:0] AFIT8_ee_iLowShDenoise_Bin
             0x0F12, 0xFF2A, //70000A3A//AFIT8_ee_iHighShDenoise_Bin   [7:0] AFIT8_ee_iLowSharpClamp_Bin
             0x0F12, 0x08FF, //70000A3C//AFIT8_ee_iHighSharpClamp_Bin   [7:0] AFIT8_ee_iReduceEdgeMinMult_Bin
             0x0F12, 0x0008, //70000A3E//AFIT8_ee_iReduceEdgeSlope_Bin [7:0]
             0x0F12, 0x0001, //70000A40//AFITB_bnr_nClustLevel_C    [0]
             0x0F12, 0x0000, //70000A42//AFIT16_BRIGHTNESS
             0x0F12, 0x0000, //70000A44//AFIT16_CONTRAST
             0x0F12, 0x0000, //70000A46//AFIT16_SATURATION
             0x0F12, 0x0000, //70000A48//AFIT16_SHARP_BLUR
             0x0F12, 0x0000, //70000A4A//AFIT16_GLAMOUR
             0x0F12, 0x00C0, //70000A4C//AFIT16_bnr_edge_high
             0x0F12, 0x0064, //70000A4E//AFIT16_postdmsc_iLowBright
             0x0F12, 0x0384, //70000A50//AFIT16_postdmsc_iHighBright
             0x0F12, 0x0051, //70000A52//AFIT16_postdmsc_iLowSat
             0x0F12, 0x01F4, //70000A54//AFIT16_postdmsc_iHighSat
             0x0F12, 0x0070, //70000A56//AFIT16_postdmsc_iTune
             0x0F12, 0x0040, //70000A58//AFIT16_yuvemix_mNegRanges_0
             0x0F12, 0x00A0, //70000A5A//AFIT16_yuvemix_mNegRanges_1
             0x0F12, 0x0100, //70000A5C//AFIT16_yuvemix_mNegRanges_2
             0x0F12, 0x0010, //70000A5E//AFIT16_yuvemix_mPosRanges_0
             0x0F12, 0x0060, //70000A60//AFIT16_yuvemix_mPosRanges_1
             0x0F12, 0x0100, //70000A62//AFIT16_yuvemix_mPosRanges_2
             0x0F12, 0x1430, //70000A64//AFIT8_bnr_edge_low  [7:0] AFIT8_bnr_repl_thresh
             0x0F12, 0x0201, //70000A66//AFIT8_bnr_repl_force  [7:0] AFIT8_bnr_iHotThreshHigh
             0x0F12, 0x0204, //70000A68//AFIT8_bnr_iHotThreshLow   [7:0] AFIT8_bnr_iColdThreshHigh
             0x0F12, 0x2404, //70000A6A//AFIT8_bnr_iColdThreshLow   [7:0] AFIT8_bnr_DispTH_Low
             0x0F12, 0x031B, //70000A6C//AFIT8_bnr_DispTH_High   [7:0] AFIT8_bnr_DISP_Limit_Low
             0x0F12, 0x0103, //70000A6E//AFIT8_bnr_DISP_Limit_High   [7:0] AFIT8_bnr_iDistSigmaMin
             0x0F12, 0x1205, //70000A70//AFIT8_bnr_iDistSigmaMax   [7:0] AFIT8_bnr_iDiffSigmaLow
             0x0F12, 0x400D, //70000A72//AFIT8_bnr_iDiffSigmaHigh   [7:0] AFIT8_bnr_iNormalizedSTD_TH
             0x0F12, 0x0080, //70000A74//AFIT8_bnr_iNormalizedSTD_Limit [7:0] AFIT8_bnr_iDirNRTune
             0x0F12, 0x2080, //70000A76//AFIT8_bnr_iDirMinThres [7:0] AFIT8_bnr_iDirFltDiffThresHigh
             0x0F12, 0x3040, //70000A78//AFIT8_bnr_iDirFltDiffThresLow   [7:0] AFIT8_bnr_iDirSmoothPowerHigh
             0x0F12, 0x0630, //70000A7A//AFIT8_bnr_iDirSmoothPowerLow   [7:0] AFIT8_bnr_iLowMaxSlopeAllowed
             0x0F12, 0x0306, //70000A7C//AFIT8_bnr_iHighMaxSlopeAllowed [7:0] AFIT8_bnr_iLowSlopeThresh
             0x0F12, 0x2003, //70000A7E//AFIT8_bnr_iHighSlopeThresh [7:0] AFIT8_bnr_iSlopenessTH
             0x0F12, 0xFF01, //70000A80//AFIT8_bnr_iSlopeBlurStrength   [7:0] AFIT8_bnr_iSlopenessLimit
             0x0F12, 0x0404, //70000A82//AFIT8_bnr_AddNoisePower1   [7:0] AFIT8_bnr_AddNoisePower2
             0x0F12, 0x0300, //70000A84//AFIT8_bnr_iRadialTune   [7:0] AFIT8_bnr_iRadialPower
             0x0F12, 0x245A, //70000A86//AFIT8_bnr_iRadialLimit [7:0] AFIT8_ee_iFSMagThLow
             0x0F12, 0x1018, //70000A88//AFIT8_ee_iFSMagThHigh   [7:0] AFIT8_ee_iFSVarThLow
             0x0F12, 0x000B, //70000A8A//AFIT8_ee_iFSVarThHigh   [7:0] AFIT8_ee_iFSThLow
             0x0F12, 0x0B00, //70000A8C//AFIT8_ee_iFSThHigh [7:0] AFIT8_ee_iFSmagPower
             0x0F12, 0x5A0F, //70000A8E//AFIT8_ee_iFSVarCountTh [7:0] AFIT8_ee_iRadialLimit
             0x0F12, 0x0505, //70000A90//AFIT8_ee_iRadialPower   [7:0] AFIT8_ee_iSmoothEdgeSlope
             0x0F12, 0x1802, //70000A92//AFIT8_ee_iROADThres   [7:0] AFIT8_ee_iROADMaxNR
             0x0F12, 0x0000, //70000A94//AFIT8_ee_iROADSubMaxNR [7:0] AFIT8_ee_iROADSubThres
             0x0F12, 0x2006, //70000A96//AFIT8_ee_iROADNeiThres [7:0] AFIT8_ee_iROADNeiMaxNR
             0x0F12, 0x3428, //70000A98//AFIT8_ee_iSmoothEdgeThres   [7:0] AFIT8_ee_iMSharpen
             0x0F12, 0x041C, //70000A9A//AFIT8_ee_iWSharpen [7:0] AFIT8_ee_iMShThresh
             0x0F12, 0x0101, //70000A9C//AFIT8_ee_iWShThresh   [7:0] AFIT8_ee_iReduceNegative
             0x0F12, 0x0800, //70000A9E//AFIT8_ee_iEmbossCentAdd   [7:0] AFIT8_ee_iShDespeckle
             0x0F12, 0x1004, //70000AA0//AFIT8_ee_iReduceEdgeThresh [7:0] AFIT8_dmsc_iEnhThresh
             0x0F12, 0x4008, //70000AA2//AFIT8_dmsc_iDesatThresh   [7:0] AFIT8_dmsc_iDemBlurHigh
             0x0F12, 0x0540, //70000AA4//AFIT8_dmsc_iDemBlurLow [7:0] AFIT8_dmsc_iDemBlurRange
             0x0F12, 0x8006, //70000AA6//AFIT8_dmsc_iDecisionThresh [7:0] AFIT8_dmsc_iCentGrad
             0x0F12, 0x0020, //70000AA8//AFIT8_dmsc_iMonochrom   [7:0] AFIT8_dmsc_iGBDenoiseVal
             0x0F12, 0x0000, //70000AAA//AFIT8_dmsc_iGRDenoiseVal   [7:0] AFIT8_dmsc_iEdgeDesatThrHigh
             0x0F12, 0x1800, //70000AAC//AFIT8_dmsc_iEdgeDesatThrLow   [7:0] AFIT8_dmsc_iEdgeDesat
             0x0F12, 0x0000, //70000AAE//AFIT8_dmsc_iNearGrayDesat   [7:0] AFIT8_dmsc_iEdgeDesatLimit
             0x0F12, 0x1E10, //70000AB0//AFIT8_postdmsc_iBCoeff [7:0] AFIT8_postdmsc_iGCoeff
             0x0F12, 0x000B, //70000AB2//AFIT8_postdmsc_iWideMult   [7:0] AFIT8_yuvemix_mNegSlopes_0
             0x0F12, 0x0607, //70000AB4//AFIT8_yuvemix_mNegSlopes_1 [7:0] AFIT8_yuvemix_mNegSlopes_2
             0x0F12, 0x0005, //70000AB6//AFIT8_yuvemix_mNegSlopes_3 [7:0] AFIT8_yuvemix_mPosSlopes_0
             0x0F12, 0x0607, //70000AB8//AFIT8_yuvemix_mPosSlopes_1 [7:0] AFIT8_yuvemix_mPosSlopes_2
             0x0F12, 0x0405, //70000ABA//AFIT8_yuvemix_mPosSlopes_3 [7:0] AFIT8_yuviirnr_iXSupportY
             0x0F12, 0x0205, //70000ABC//AFIT8_yuviirnr_iXSupportUV [7:0] AFIT8_yuviirnr_iLowYNorm
             0x0F12, 0x0304, //70000ABE//AFIT8_yuviirnr_iHighYNorm   [7:0] AFIT8_yuviirnr_iLowUVNorm
             0x0F12, 0x0409, //70000AC0//AFIT8_yuviirnr_iHighUVNorm [7:0] AFIT8_yuviirnr_iYNormShift
             0x0F12, 0x0306, //70000AC2//AFIT8_yuviirnr_iUVNormShift   [7:0] AFIT8_yuviirnr_iVertLength_Y
             0x0F12, 0x0407, //70000AC4//AFIT8_yuviirnr_iVertLength_UV   [7:0] AFIT8_yuviirnr_iDiffThreshL_Y
             0x0F12, 0x1F04, //70000AC6//AFIT8_yuviirnr_iDiffThreshH_Y   [7:0] AFIT8_yuviirnr_iDiffThreshL_UV
             0x0F12, 0x0218, //70000AC8//AFIT8_yuviirnr_iDiffThreshH_UV [7:0] AFIT8_yuviirnr_iMaxThreshL_Y
             0x0F12, 0x1102, //70000ACA//AFIT8_yuviirnr_iMaxThreshH_Y   [7:0] AFIT8_yuviirnr_iMaxThreshL_UV
             0x0F12, 0x0611, //70000ACC//AFIT8_yuviirnr_iMaxThreshH_UV   [7:0] AFIT8_yuviirnr_iYNRStrengthL
             0x0F12, 0x1A02, //70000ACE//AFIT8_yuviirnr_iYNRStrengthH   [7:0] AFIT8_yuviirnr_iUVNRStrengthL
             0x0F12, 0x8018, //70000AD0//AFIT8_yuviirnr_iUVNRStrengthH   [7:0] AFIT8_byr_gras_iShadingPower
             0x0F12, 0x0080, //70000AD2//AFIT8_RGBGamma2_iLinearity [7:0] AFIT8_RGBGamma2_iDarkReduce
             0x0F12, 0x0080, //70000AD4//AFIT8_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
             0x0F12, 0x0180, //70000AD6//AFIT8_RGB2YUV_iRGBGain [7:0] AFIT8_bnr_nClustLevel_H
             0x0F12, 0x0A0A, //70000AD8//AFIT8_bnr_iClustMulT_H [7:0] AFIT8_bnr_iClustMulT_C
             0x0F12, 0x0101, //70000ADA//AFIT8_bnr_iClustThresh_H   [7:0] AFIT8_bnr_iClustThresh_C
             0x0F12, 0x1B24, //70000ADC//AFIT8_bnr_iDenThreshLow   [7:0] AFIT8_bnr_iDenThreshHigh
             0x0F12, 0x6024, //70000ADE//AFIT8_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
             0x0F12, 0x1D22, //70000AE0//AFIT8_ee_iLowShDenoise [7:0] AFIT8_ee_iHighShDenoise
             0x0F12, 0xFFFF, //70000AE2//AFIT8_ee_iLowSharpClamp   [7:0] AFIT8_ee_iHighSharpClamp
             0x0F12, 0x0808, //70000AE4//AFIT8_ee_iReduceEdgeMinMult   [7:0] AFIT8_ee_iReduceEdgeSlope
             0x0F12, 0x0A01, //70000AE6//AFIT8_bnr_nClustLevel_H_Bin   [7:0] AFIT8_bnr_iClustMulT_H_Bin
             0x0F12, 0x010A, //70000AE8//AFIT8_bnr_iClustMulT_C_Bin [7:0] AFIT8_bnr_iClustThresh_H_Bin
             0x0F12, 0x2401, //70000AEA//AFIT8_bnr_iClustThresh_C_Bin   [7:0] AFIT8_bnr_iDenThreshLow_Bin
             0x0F12, 0x241B, //70000AEC//AFIT8_bnr_iDenThreshHigh_Bin   [7:0] AFIT8_ee_iLowSharpPower_Bin
             0x0F12, 0x1E60, //70000AEE//AFIT8_ee_iHighSharpPower_Bin   [7:0] AFIT8_ee_iLowShDenoise_Bin
             0x0F12, 0xFF18, //70000AF0//AFIT8_ee_iHighShDenoise_Bin   [7:0] AFIT8_ee_iLowSharpClamp_Bin
             0x0F12, 0x08FF, //70000AF2//AFIT8_ee_iHighSharpClamp_Bin   [7:0] AFIT8_ee_iReduceEdgeMinMult_Bin
             0x0F12, 0x0008, //70000AF4//AFIT8_ee_iReduceEdgeSlope_Bin [7:0]
             0x0F12, 0x0001, //70000AF6//AFITB_bnr_nClustLevel_C    [0]
             0x0F12, 0x0000, //70000AF8//AFIT16_BRIGHTNESS
             0x0F12, 0x0000, //70000AFA//AFIT16_CONTRAST
             0x0F12, 0x0000, //70000AFC//AFIT16_SATURATION
             0x0F12, 0x0000, //70000AFE//AFIT16_SHARP_BLUR
             0x0F12, 0x0000, //70000B00//AFIT16_GLAMOUR
             0x0F12, 0x00C0, //70000B02//AFIT16_bnr_edge_high
             0x0F12, 0x0064, //70000B04//AFIT16_postdmsc_iLowBright
             0x0F12, 0x0384, //70000B06//AFIT16_postdmsc_iHighBright
             0x0F12, 0x0043, //70000B08//AFIT16_postdmsc_iLowSat
             0x0F12, 0x01F4, //70000B0A//AFIT16_postdmsc_iHighSat
             0x0F12, 0x0070, //70000B0C//AFIT16_postdmsc_iTune
             0x0F12, 0x0040, //70000B0E//AFIT16_yuvemix_mNegRanges_0
             0x0F12, 0x00A0, //70000B10//AFIT16_yuvemix_mNegRanges_1
             0x0F12, 0x0100, //70000B12//AFIT16_yuvemix_mNegRanges_2
             0x0F12, 0x0010, //70000B14//AFIT16_yuvemix_mPosRanges_0
             0x0F12, 0x0060, //70000B16//AFIT16_yuvemix_mPosRanges_1
             0x0F12, 0x0100, //70000B18//AFIT16_yuvemix_mPosRanges_2
             0x0F12, 0x1430, //70000B1A//AFIT8_bnr_edge_low  [7:0] AFIT8_bnr_repl_thresh
             0x0F12, 0x0201, //70000B1C//AFIT8_bnr_repl_force  [7:0] AFIT8_bnr_iHotThreshHigh
             0x0F12, 0x0204, //70000B1E//AFIT8_bnr_iHotThreshLow   [7:0] AFIT8_bnr_iColdThreshHigh
             0x0F12, 0x1B04, //70000B20//AFIT8_bnr_iColdThreshLow   [7:0] AFIT8_bnr_DispTH_Low
             0x0F12, 0x0312, //70000B22//AFIT8_bnr_DispTH_High   [7:0] AFIT8_bnr_DISP_Limit_Low
             0x0F12, 0x0003, //70000B24//AFIT8_bnr_DISP_Limit_High   [7:0] AFIT8_bnr_iDistSigmaMin
             0x0F12, 0x0C03, //70000B26//AFIT8_bnr_iDistSigmaMax   [7:0] AFIT8_bnr_iDiffSigmaLow
             0x0F12, 0x2806, //70000B28//AFIT8_bnr_iDiffSigmaHigh   [7:0] AFIT8_bnr_iNormalizedSTD_TH
             0x0F12, 0x0060, //70000B2A//AFIT8_bnr_iNormalizedSTD_Limit [7:0] AFIT8_bnr_iDirNRTune
             0x0F12, 0x1580, //70000B2C//AFIT8_bnr_iDirMinThres [7:0] AFIT8_bnr_iDirFltDiffThresHigh
             0x0F12, 0x2020, //70000B2E//AFIT8_bnr_iDirFltDiffThresLow   [7:0] AFIT8_bnr_iDirSmoothPowerHigh
             0x0F12, 0x0620, //70000B30//AFIT8_bnr_iDirSmoothPowerLow   [7:0] AFIT8_bnr_iLowMaxSlopeAllowed
             0x0F12, 0x0306, //70000B32//AFIT8_bnr_iHighMaxSlopeAllowed [7:0] AFIT8_bnr_iLowSlopeThresh
             0x0F12, 0x2003, //70000B34//AFIT8_bnr_iHighSlopeThresh [7:0] AFIT8_bnr_iSlopenessTH
             0x0F12, 0xFF01, //70000B36//AFIT8_bnr_iSlopeBlurStrength   [7:0] AFIT8_bnr_iSlopenessLimit
             0x0F12, 0x0404, //70000B38//AFIT8_bnr_AddNoisePower1   [7:0] AFIT8_bnr_AddNoisePower2
             0x0F12, 0x0300, //70000B3A//AFIT8_bnr_iRadialTune   [7:0] AFIT8_bnr_iRadialPower
             0x0F12, 0x145A, //70000B3C//AFIT8_bnr_iRadialLimit [7:0] AFIT8_ee_iFSMagThLow
             0x0F12, 0x1010, //70000B3E//AFIT8_ee_iFSMagThHigh   [7:0] AFIT8_ee_iFSVarThLow
             0x0F12, 0x000B, //70000B40//AFIT8_ee_iFSVarThHigh   [7:0] AFIT8_ee_iFSThLow
             0x0F12, 0x0E00, //70000B42//AFIT8_ee_iFSThHigh [7:0] AFIT8_ee_iFSmagPower
             0x0F12, 0x5A0F, //70000B44//AFIT8_ee_iFSVarCountTh [7:0] AFIT8_ee_iRadialLimit
             0x0F12, 0x0504, //70000B46//AFIT8_ee_iRadialPower   [7:0] AFIT8_ee_iSmoothEdgeSlope
             0x0F12, 0x1802, //70000B48//AFIT8_ee_iROADThres   [7:0] AFIT8_ee_iROADMaxNR
             0x0F12, 0x0000, //70000B4A//AFIT8_ee_iROADSubMaxNR [7:0] AFIT8_ee_iROADSubThres
             0x0F12, 0x2006, //70000B4C//AFIT8_ee_iROADNeiThres [7:0] AFIT8_ee_iROADNeiMaxNR
             0x0F12, 0x3828, //70000B4E//AFIT8_ee_iSmoothEdgeThres   [7:0] AFIT8_ee_iMSharpen
             0x0F12, 0x0428, //70000B50//AFIT8_ee_iWSharpen [7:0] AFIT8_ee_iMShThresh
             0x0F12, 0x0101, //70000B52//AFIT8_ee_iWShThresh   [7:0] AFIT8_ee_iReduceNegative
             0x0F12, 0x8000, //70000B54//AFIT8_ee_iEmbossCentAdd   [7:0] AFIT8_ee_iShDespeckle
             0x0F12, 0x0A04, //70000B56//AFIT8_ee_iReduceEdgeThresh [7:0] AFIT8_dmsc_iEnhThresh
             0x0F12, 0x4008, //70000B58//AFIT8_dmsc_iDesatThresh   [7:0] AFIT8_dmsc_iDemBlurHigh
             0x0F12, 0x0540, //70000B5A//AFIT8_dmsc_iDemBlurLow [7:0] AFIT8_dmsc_iDemBlurRange
             0x0F12, 0x8006, //70000B5C//AFIT8_dmsc_iDecisionThresh [7:0] AFIT8_dmsc_iCentGrad
             0x0F12, 0x0020, //70000B5E//AFIT8_dmsc_iMonochrom   [7:0] AFIT8_dmsc_iGBDenoiseVal
             0x0F12, 0x0000, //70000B60//AFIT8_dmsc_iGRDenoiseVal   [7:0] AFIT8_dmsc_iEdgeDesatThrHigh
             0x0F12, 0x1800, //70000B62//AFIT8_dmsc_iEdgeDesatThrLow   [7:0] AFIT8_dmsc_iEdgeDesat
             0x0F12, 0x0000, //70000B64//AFIT8_dmsc_iNearGrayDesat   [7:0] AFIT8_dmsc_iEdgeDesatLimit
             0x0F12, 0x1E10, //70000B66//AFIT8_postdmsc_iBCoeff [7:0] AFIT8_postdmsc_iGCoeff
             0x0F12, 0x000B, //70000B68//AFIT8_postdmsc_iWideMult   [7:0] AFIT8_yuvemix_mNegSlopes_0
             0x0F12, 0x0607, //70000B6A//AFIT8_yuvemix_mNegSlopes_1 [7:0] AFIT8_yuvemix_mNegSlopes_2
             0x0F12, 0x0005, //70000B6C//AFIT8_yuvemix_mNegSlopes_3 [7:0] AFIT8_yuvemix_mPosSlopes_0
             0x0F12, 0x0607, //70000B6E//AFIT8_yuvemix_mPosSlopes_1 [7:0] AFIT8_yuvemix_mPosSlopes_2
             0x0F12, 0x0405, //70000B70//AFIT8_yuvemix_mPosSlopes_3 [7:0] AFIT8_yuviirnr_iXSupportY
             0x0F12, 0x0207, //70000B72//AFIT8_yuviirnr_iXSupportUV [7:0] AFIT8_yuviirnr_iLowYNorm
             0x0F12, 0x0304, //70000B74//AFIT8_yuviirnr_iHighYNorm   [7:0] AFIT8_yuviirnr_iLowUVNorm
             0x0F12, 0x0409, //70000B76//AFIT8_yuviirnr_iHighUVNorm [7:0] AFIT8_yuviirnr_iYNormShift
             0x0F12, 0x0306, //70000B78//AFIT8_yuviirnr_iUVNormShift   [7:0] AFIT8_yuviirnr_iVertLength_Y
             0x0F12, 0x0407, //70000B7A//AFIT8_yuviirnr_iVertLength_UV   [7:0] AFIT8_yuviirnr_iDiffThreshL_Y
             0x0F12, 0x2404, //70000B7C//AFIT8_yuviirnr_iDiffThreshH_Y   [7:0] AFIT8_yuviirnr_iDiffThreshL_UV
             0x0F12, 0x0221, //70000B7E//AFIT8_yuviirnr_iDiffThreshH_UV [7:0] AFIT8_yuviirnr_iMaxThreshL_Y
             0x0F12, 0x1202, //70000B80//AFIT8_yuviirnr_iMaxThreshH_Y   [7:0] AFIT8_yuviirnr_iMaxThreshL_UV
             0x0F12, 0x0613, //70000B82//AFIT8_yuviirnr_iMaxThreshH_UV   [7:0] AFIT8_yuviirnr_iYNRStrengthL
             0x0F12, 0x1A02, //70000B84//AFIT8_yuviirnr_iYNRStrengthH   [7:0] AFIT8_yuviirnr_iUVNRStrengthL
             0x0F12, 0x8018, //70000B86//AFIT8_yuviirnr_iUVNRStrengthH   [7:0] AFIT8_byr_gras_iShadingPower
             0x0F12, 0x0080, //70000B88//AFIT8_RGBGamma2_iLinearity [7:0] AFIT8_RGBGamma2_iDarkReduce
             0x0F12, 0x0080, //70000B8A//AFIT8_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
             0x0F12, 0x0180, //70000B8C//AFIT8_RGB2YUV_iRGBGain [7:0] AFIT8_bnr_nClustLevel_H
             0x0F12, 0x0A0A, //70000B8E//AFIT8_bnr_iClustMulT_H [7:0] AFIT8_bnr_iClustMulT_C
             0x0F12, 0x0101, //70000B90//AFIT8_bnr_iClustThresh_H   [7:0] AFIT8_bnr_iClustThresh_C
             0x0F12, 0x141D, //70000B92//AFIT8_bnr_iDenThreshLow   [7:0] AFIT8_bnr_iDenThreshHigh
             0x0F12, 0x8030, //70000B94//AFIT8_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
             0x0F12, 0x0C0C, //70000B96//AFIT8_ee_iLowShDenoise [7:0] AFIT8_ee_iHighShDenoise
             0x0F12, 0xFFFF, //70000B98//AFIT8_ee_iLowSharpClamp   [7:0] AFIT8_ee_iHighSharpClamp
             0x0F12, 0x0808, //70000B9A//AFIT8_ee_iReduceEdgeMinMult   [7:0] AFIT8_ee_iReduceEdgeSlope
             0x0F12, 0x0A01, //70000B9C//AFIT8_bnr_nClustLevel_H_Bin   [7:0] AFIT8_bnr_iClustMulT_H_Bin
             0x0F12, 0x010A, //70000B9E//AFIT8_bnr_iClustMulT_C_Bin [7:0] AFIT8_bnr_iClustThresh_H_Bin
             0x0F12, 0x1B01, //70000BA0//AFIT8_bnr_iClustThresh_C_Bin   [7:0] AFIT8_bnr_iDenThreshLow_Bin
             0x0F12, 0x3012, //70000BA2//AFIT8_bnr_iDenThreshHigh_Bin   [7:0] AFIT8_ee_iLowSharpPower_Bin
             0x0F12, 0x0C80, //70000BA4//AFIT8_ee_iHighSharpPower_Bin   [7:0] AFIT8_ee_iLowShDenoise_Bin
             0x0F12, 0xFF0C, //70000BA6//AFIT8_ee_iHighShDenoise_Bin   [7:0] AFIT8_ee_iLowSharpClamp_Bin
             0x0F12, 0x08FF, //70000BA8//AFIT8_ee_iHighSharpClamp_Bin   [7:0] AFIT8_ee_iReduceEdgeMinMult_Bin
             0x0F12, 0x0008, //70000BAA//AFIT8_ee_iReduceEdgeSlope_Bin [7:0]
             0x0F12, 0x0001, //70000BAC//AFITB_bnr_nClustLevel_C    [0]
             0x0F12, 0x0000, //70000BAE//AFIT16_BRIGHTNESS
             0x0F12, 0x0000, //70000BB0//AFIT16_CONTRAST
             0x0F12, 0x0000, //70000BB2//AFIT16_SATURATION
             0x0F12, 0x0000, //70000BB4//AFIT16_SHARP_BLUR
             0x0F12, 0x0000, //70000BB6//AFIT16_GLAMOUR
             0x0F12, 0x00C0, //70000BB8//AFIT16_bnr_edge_high
             0x0F12, 0x0064, //70000BBA//AFIT16_postdmsc_iLowBright
             0x0F12, 0x0384, //70000BBC//AFIT16_postdmsc_iHighBright
             0x0F12, 0x0032, //70000BBE//AFIT16_postdmsc_iLowSat
             0x0F12, 0x01F4, //70000BC0//AFIT16_postdmsc_iHighSat
             0x0F12, 0x0070, //70000BC2//AFIT16_postdmsc_iTune
             0x0F12, 0x0040, //70000BC4//AFIT16_yuvemix_mNegRanges_0
             0x0F12, 0x00A0, //70000BC6//AFIT16_yuvemix_mNegRanges_1
             0x0F12, 0x0100, //70000BC8//AFIT16_yuvemix_mNegRanges_2
             0x0F12, 0x0010, //70000BCA//AFIT16_yuvemix_mPosRanges_0
             0x0F12, 0x0060, //70000BCC//AFIT16_yuvemix_mPosRanges_1
             0x0F12, 0x0100, //70000BCE//AFIT16_yuvemix_mPosRanges_2
             0x0F12, 0x1430, //70000BD0//AFIT8_bnr_edge_low  [7:0] AFIT8_bnr_repl_thresh
             0x0F12, 0x0201, //70000BD2//AFIT8_bnr_repl_force  [7:0] AFIT8_bnr_iHotThreshHigh
             0x0F12, 0x0204, //70000BD4//AFIT8_bnr_iHotThreshLow   [7:0] AFIT8_bnr_iColdThreshHigh
             0x0F12, 0x1504, //70000BD6//AFIT8_bnr_iColdThreshLow   [7:0] AFIT8_bnr_DispTH_Low
             0x0F12, 0x030F, //70000BD8//AFIT8_bnr_DispTH_High   [7:0] AFIT8_bnr_DISP_Limit_Low
             0x0F12, 0x0003, //70000BDA//AFIT8_bnr_DISP_Limit_High   [7:0] AFIT8_bnr_iDistSigmaMin
             0x0F12, 0x0902, //70000BDC//AFIT8_bnr_iDistSigmaMax   [7:0] AFIT8_bnr_iDiffSigmaLow
             0x0F12, 0x2004, //70000BDE//AFIT8_bnr_iDiffSigmaHigh   [7:0] AFIT8_bnr_iNormalizedSTD_TH
             0x0F12, 0x0050, //70000BE0//AFIT8_bnr_iNormalizedSTD_Limit [7:0] AFIT8_bnr_iDirNRTune
             0x0F12, 0x1140, //70000BE2//AFIT8_bnr_iDirMinThres [7:0] AFIT8_bnr_iDirFltDiffThresHigh
             0x0F12, 0x201C, //70000BE4//AFIT8_bnr_iDirFltDiffThresLow   [7:0] AFIT8_bnr_iDirSmoothPowerHigh
             0x0F12, 0x0620, //70000BE6//AFIT8_bnr_iDirSmoothPowerLow   [7:0] AFIT8_bnr_iLowMaxSlopeAllowed
             0x0F12, 0x0306, //70000BE8//AFIT8_bnr_iHighMaxSlopeAllowed [7:0] AFIT8_bnr_iLowSlopeThresh
             0x0F12, 0x2003, //70000BEA//AFIT8_bnr_iHighSlopeThresh [7:0] AFIT8_bnr_iSlopenessTH
             0x0F12, 0xFF01, //70000BEC//AFIT8_bnr_iSlopeBlurStrength   [7:0] AFIT8_bnr_iSlopenessLimit
             0x0F12, 0x0404, //70000BEE//AFIT8_bnr_AddNoisePower1   [7:0] AFIT8_bnr_AddNoisePower2
             0x0F12, 0x0300, //70000BF0//AFIT8_bnr_iRadialTune   [7:0] AFIT8_bnr_iRadialPower
             0x0F12, 0x145A, //70000BF2//AFIT8_bnr_iRadialLimit [7:0] AFIT8_ee_iFSMagThLow
             0x0F12, 0x1010, //70000BF4//AFIT8_ee_iFSMagThHigh   [7:0] AFIT8_ee_iFSVarThLow
             0x0F12, 0x000B, //70000BF6//AFIT8_ee_iFSVarThHigh   [7:0] AFIT8_ee_iFSThLow
             0x0F12, 0x1000, //70000BF8//AFIT8_ee_iFSThHigh [7:0] AFIT8_ee_iFSmagPower
             0x0F12, 0x5A0F, //70000BFA//AFIT8_ee_iFSVarCountTh [7:0] AFIT8_ee_iRadialLimit
             0x0F12, 0x0503, //70000BFC//AFIT8_ee_iRadialPower   [7:0] AFIT8_ee_iSmoothEdgeSlope
             0x0F12, 0x1802, //70000BFE//AFIT8_ee_iROADThres   [7:0] AFIT8_ee_iROADMaxNR
             0x0F12, 0x0000, //70000C00//AFIT8_ee_iROADSubMaxNR [7:0] AFIT8_ee_iROADSubThres
             0x0F12, 0x2006, //70000C02//AFIT8_ee_iROADNeiThres [7:0] AFIT8_ee_iROADNeiMaxNR
             0x0F12, 0x3C28, //70000C04//AFIT8_ee_iSmoothEdgeThres   [7:0] AFIT8_ee_iMSharpen
             0x0F12, 0x042C, //70000C06//AFIT8_ee_iWSharpen [7:0] AFIT8_ee_iMShThresh
             0x0F12, 0x0101, //70000C08//AFIT8_ee_iWShThresh   [7:0] AFIT8_ee_iReduceNegative
             0x0F12, 0xFF00, //70000C0A//AFIT8_ee_iEmbossCentAdd   [7:0] AFIT8_ee_iShDespeckle
             0x0F12, 0x0904, //70000C0C//AFIT8_ee_iReduceEdgeThresh [7:0] AFIT8_dmsc_iEnhThresh
             0x0F12, 0x4008, //70000C0E//AFIT8_dmsc_iDesatThresh   [7:0] AFIT8_dmsc_iDemBlurHigh
             0x0F12, 0x0540, //70000C10//AFIT8_dmsc_iDemBlurLow [7:0] AFIT8_dmsc_iDemBlurRange
             0x0F12, 0x8006, //70000C12//AFIT8_dmsc_iDecisionThresh [7:0] AFIT8_dmsc_iCentGrad
             0x0F12, 0x0020, //70000C14//AFIT8_dmsc_iMonochrom   [7:0] AFIT8_dmsc_iGBDenoiseVal
             0x0F12, 0x0000, //70000C16//AFIT8_dmsc_iGRDenoiseVal   [7:0] AFIT8_dmsc_iEdgeDesatThrHigh
             0x0F12, 0x1800, //70000C18//AFIT8_dmsc_iEdgeDesatThrLow   [7:0] AFIT8_dmsc_iEdgeDesat
             0x0F12, 0x0000, //70000C1A//AFIT8_dmsc_iNearGrayDesat   [7:0] AFIT8_dmsc_iEdgeDesatLimit
             0x0F12, 0x1E10, //70000C1C//AFIT8_postdmsc_iBCoeff [7:0] AFIT8_postdmsc_iGCoeff
             0x0F12, 0x000B, //70000C1E//AFIT8_postdmsc_iWideMult   [7:0] AFIT8_yuvemix_mNegSlopes_0
             0x0F12, 0x0607, //70000C20//AFIT8_yuvemix_mNegSlopes_1 [7:0] AFIT8_yuvemix_mNegSlopes_2
             0x0F12, 0x0005, //70000C22//AFIT8_yuvemix_mNegSlopes_3 [7:0] AFIT8_yuvemix_mPosSlopes_0
             0x0F12, 0x0607, //70000C24//AFIT8_yuvemix_mPosSlopes_1 [7:0] AFIT8_yuvemix_mPosSlopes_2
             0x0F12, 0x0405, //70000C26//AFIT8_yuvemix_mPosSlopes_3 [7:0] AFIT8_yuviirnr_iXSupportY
             0x0F12, 0x0206, //70000C28//AFIT8_yuviirnr_iXSupportUV [7:0] AFIT8_yuviirnr_iLowYNorm
             0x0F12, 0x0304, //70000C2A//AFIT8_yuviirnr_iHighYNorm   [7:0] AFIT8_yuviirnr_iLowUVNorm
             0x0F12, 0x0409, //70000C2C//AFIT8_yuviirnr_iHighUVNorm [7:0] AFIT8_yuviirnr_iYNormShift
             0x0F12, 0x0305, //70000C2E//AFIT8_yuviirnr_iUVNormShift   [7:0] AFIT8_yuviirnr_iVertLength_Y
             0x0F12, 0x0406, //70000C30//AFIT8_yuviirnr_iVertLength_UV   [7:0] AFIT8_yuviirnr_iDiffThreshL_Y
             0x0F12, 0x2804, //70000C32//AFIT8_yuviirnr_iDiffThreshH_Y   [7:0] AFIT8_yuviirnr_iDiffThreshL_UV
             0x0F12, 0x0228, //70000C34//AFIT8_yuviirnr_iDiffThreshH_UV [7:0] AFIT8_yuviirnr_iMaxThreshL_Y
             0x0F12, 0x1402, //70000C36//AFIT8_yuviirnr_iMaxThreshH_Y   [7:0] AFIT8_yuviirnr_iMaxThreshL_UV
             0x0F12, 0x0618, //70000C38//AFIT8_yuviirnr_iMaxThreshH_UV   [7:0] AFIT8_yuviirnr_iYNRStrengthL
             0x0F12, 0x1A02, //70000C3A//AFIT8_yuviirnr_iYNRStrengthH   [7:0] AFIT8_yuviirnr_iUVNRStrengthL
             0x0F12, 0x8018, //70000C3C//AFIT8_yuviirnr_iUVNRStrengthH   [7:0] AFIT8_byr_gras_iShadingPower
             0x0F12, 0x0080, //70000C3E//AFIT8_RGBGamma2_iLinearity [7:0] AFIT8_RGBGamma2_iDarkReduce
             0x0F12, 0x0080, //70000C40//AFIT8_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
             0x0F12, 0x0180, //70000C42//AFIT8_RGB2YUV_iRGBGain [7:0] AFIT8_bnr_nClustLevel_H
             0x0F12, 0x0A0A, //70000C44//AFIT8_bnr_iClustMulT_H [7:0] AFIT8_bnr_iClustMulT_C
             0x0F12, 0x0101, //70000C46//AFIT8_bnr_iClustThresh_H   [7:0] AFIT8_bnr_iClustThresh_C
             0x0F12, 0x1117, //70000C48//AFIT8_bnr_iDenThreshLow   [7:0] AFIT8_bnr_iDenThreshHigh
             0x0F12, 0x8030, //70000C4A//AFIT8_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
             0x0F12, 0x0A0A, //70000C4C//AFIT8_ee_iLowShDenoise [7:0] AFIT8_ee_iHighShDenoise
             0x0F12, 0xFFFF, //70000C4E//AFIT8_ee_iLowSharpClamp   [7:0] AFIT8_ee_iHighSharpClamp
             0x0F12, 0x0808, //70000C50//AFIT8_ee_iReduceEdgeMinMult   [7:0] AFIT8_ee_iReduceEdgeSlope
             0x0F12, 0x0A01, //70000C52//AFIT8_bnr_nClustLevel_H_Bin   [7:0] AFIT8_bnr_iClustMulT_H_Bin
             0x0F12, 0x010A, //70000C54//AFIT8_bnr_iClustMulT_C_Bin [7:0] AFIT8_bnr_iClustThresh_H_Bin
             0x0F12, 0x1501, //70000C56//AFIT8_bnr_iClustThresh_C_Bin   [7:0] AFIT8_bnr_iDenThreshLow_Bin
             0x0F12, 0x3012, //70000C58//AFIT8_bnr_iDenThreshHigh_Bin   [7:0] AFIT8_ee_iLowSharpPower_Bin
             0x0F12, 0x0C80, //70000C5A//AFIT8_ee_iHighSharpPower_Bin   [7:0] AFIT8_ee_iLowShDenoise_Bin
             0x0F12, 0xFF0A, //70000C5C//AFIT8_ee_iHighShDenoise_Bin   [7:0] AFIT8_ee_iLowSharpClamp_Bin
             0x0F12, 0x08FF, //70000C5E//AFIT8_ee_iHighSharpClamp_Bin   [7:0] AFIT8_ee_iReduceEdgeMinMult_Bin
             0x0F12, 0x0008, //70000C60//AFIT8_ee_iReduceEdgeSlope_Bin [7:0]
             0x0F12, 0x0001, //70000C62//AFITB_bnr_nClustLevel_C    [0]
             0x0F12, 0x0000, //70000C64//AFIT16_BRIGHTNESS
             0x0F12, 0x0000, //70000C66//AFIT16_CONTRAST
             0x0F12, 0x0000, //70000C68//AFIT16_SATURATION
             0x0F12, 0x0000, //70000C6A//AFIT16_SHARP_BLUR
             0x0F12, 0x0000, //70000C6C//AFIT16_GLAMOUR
             0x0F12, 0x00C0, //70000C6E//AFIT16_bnr_edge_high
             0x0F12, 0x0064, //70000C70//AFIT16_postdmsc_iLowBright
             0x0F12, 0x0384, //70000C72//AFIT16_postdmsc_iHighBright
             0x0F12, 0x0032, //70000C74//AFIT16_postdmsc_iLowSat
             0x0F12, 0x01F4, //70000C76//AFIT16_postdmsc_iHighSat
             0x0F12, 0x0070, //70000C78//AFIT16_postdmsc_iTune
             0x0F12, 0x0040, //70000C7A//AFIT16_yuvemix_mNegRanges_0
             0x0F12, 0x00A0, //70000C7C//AFIT16_yuvemix_mNegRanges_1
             0x0F12, 0x0100, //70000C7E//AFIT16_yuvemix_mNegRanges_2
             0x0F12, 0x0010, //70000C80//AFIT16_yuvemix_mPosRanges_0
             0x0F12, 0x0060, //70000C82//AFIT16_yuvemix_mPosRanges_1
             0x0F12, 0x0100, //70000C84//AFIT16_yuvemix_mPosRanges_2
             0x0F12, 0x1430, //70000C86//AFIT8_bnr_edge_low  [7:0] AFIT8_bnr_repl_thresh
             0x0F12, 0x0201, //70000C88//AFIT8_bnr_repl_force  [7:0] AFIT8_bnr_iHotThreshHigh
             0x0F12, 0x0204, //70000C8A//AFIT8_bnr_iHotThreshLow   [7:0] AFIT8_bnr_iColdThreshHigh
             0x0F12, 0x0F04, //70000C8C//AFIT8_bnr_iColdThreshLow   [7:0] AFIT8_bnr_DispTH_Low
             0x0F12, 0x030C, //70000C8E//AFIT8_bnr_DispTH_High   [7:0] AFIT8_bnr_DISP_Limit_Low
             0x0F12, 0x0003, //70000C90//AFIT8_bnr_DISP_Limit_High   [7:0] AFIT8_bnr_iDistSigmaMin
             0x0F12, 0x0602, //70000C92//AFIT8_bnr_iDistSigmaMax   [7:0] AFIT8_bnr_iDiffSigmaLow
             0x0F12, 0x1803, //70000C94//AFIT8_bnr_iDiffSigmaHigh   [7:0] AFIT8_bnr_iNormalizedSTD_TH
             0x0F12, 0x0040, //70000C96//AFIT8_bnr_iNormalizedSTD_Limit [7:0] AFIT8_bnr_iDirNRTune
             0x0F12, 0x0E20, //70000C98//AFIT8_bnr_iDirMinThres [7:0] AFIT8_bnr_iDirFltDiffThresHigh
             0x0F12, 0x2018, //70000C9A//AFIT8_bnr_iDirFltDiffThresLow   [7:0] AFIT8_bnr_iDirSmoothPowerHigh
             0x0F12, 0x0620, //70000C9C//AFIT8_bnr_iDirSmoothPowerLow   [7:0] AFIT8_bnr_iLowMaxSlopeAllowed
             0x0F12, 0x0306, //70000C9E//AFIT8_bnr_iHighMaxSlopeAllowed [7:0] AFIT8_bnr_iLowSlopeThresh
             0x0F12, 0x2003, //70000CA0//AFIT8_bnr_iHighSlopeThresh [7:0] AFIT8_bnr_iSlopenessTH
             0x0F12, 0xFF01, //70000CA2//AFIT8_bnr_iSlopeBlurStrength   [7:0] AFIT8_bnr_iSlopenessLimit
             0x0F12, 0x0404, //70000CA4//AFIT8_bnr_AddNoisePower1   [7:0] AFIT8_bnr_AddNoisePower2
             0x0F12, 0x0200, //70000CA6//AFIT8_bnr_iRadialTune   [7:0] AFIT8_bnr_iRadialPower
             0x0F12, 0x145A, //70000CA8//AFIT8_bnr_iRadialLimit [7:0] AFIT8_ee_iFSMagThLow
             0x0F12, 0x1010, //70000CAA//AFIT8_ee_iFSMagThHigh   [7:0] AFIT8_ee_iFSVarThLow
             0x0F12, 0x000B, //70000CAC//AFIT8_ee_iFSVarThHigh   [7:0] AFIT8_ee_iFSThLow
             0x0F12, 0x1200, //70000CAE//AFIT8_ee_iFSThHigh [7:0] AFIT8_ee_iFSmagPower
             0x0F12, 0x5A0F, //70000CB0//AFIT8_ee_iFSVarCountTh [7:0] AFIT8_ee_iRadialLimit
             0x0F12, 0x0502, //70000CB2//AFIT8_ee_iRadialPower   [7:0] AFIT8_ee_iSmoothEdgeSlope
             0x0F12, 0x1802, //70000CB4//AFIT8_ee_iROADThres   [7:0] AFIT8_ee_iROADMaxNR
             0x0F12, 0x0000, //70000CB6//AFIT8_ee_iROADSubMaxNR [7:0] AFIT8_ee_iROADSubThres
             0x0F12, 0x2006, //70000CB8//AFIT8_ee_iROADNeiThres [7:0] AFIT8_ee_iROADNeiMaxNR
             0x0F12, 0x4028, //70000CBA//AFIT8_ee_iSmoothEdgeThres   [7:0] AFIT8_ee_iMSharpen
             0x0F12, 0x0430, //70000CBC//AFIT8_ee_iWSharpen [7:0] AFIT8_ee_iMShThresh
             0x0F12, 0x0101, //70000CBE//AFIT8_ee_iWShThresh   [7:0] AFIT8_ee_iReduceNegative
             0x0F12, 0xFF00, //70000CC0//AFIT8_ee_iEmbossCentAdd   [7:0] AFIT8_ee_iShDespeckle
             0x0F12, 0x0804, //70000CC2//AFIT8_ee_iReduceEdgeThresh [7:0] AFIT8_dmsc_iEnhThresh
             0x0F12, 0x4008, //70000CC4//AFIT8_dmsc_iDesatThresh   [7:0] AFIT8_dmsc_iDemBlurHigh
             0x0F12, 0x0540, //70000CC6//AFIT8_dmsc_iDemBlurLow [7:0] AFIT8_dmsc_iDemBlurRange
             0x0F12, 0x8006, //70000CC8//AFIT8_dmsc_iDecisionThresh [7:0] AFIT8_dmsc_iCentGrad
             0x0F12, 0x0020, //70000CCA//AFIT8_dmsc_iMonochrom   [7:0] AFIT8_dmsc_iGBDenoiseVal
             0x0F12, 0x0000, //70000CCC//AFIT8_dmsc_iGRDenoiseVal   [7:0] AFIT8_dmsc_iEdgeDesatThrHigh
             0x0F12, 0x1800, //70000CCE//AFIT8_dmsc_iEdgeDesatThrLow   [7:0] AFIT8_dmsc_iEdgeDesat
             0x0F12, 0x0000, //70000CD0//AFIT8_dmsc_iNearGrayDesat   [7:0] AFIT8_dmsc_iEdgeDesatLimit
             0x0F12, 0x1E10, //70000CD2//AFIT8_postdmsc_iBCoeff [7:0] AFIT8_postdmsc_iGCoeff
             0x0F12, 0x000B, //70000CD4//AFIT8_postdmsc_iWideMult   [7:0] AFIT8_yuvemix_mNegSlopes_0
             0x0F12, 0x0607, //70000CD6//AFIT8_yuvemix_mNegSlopes_1 [7:0] AFIT8_yuvemix_mNegSlopes_2
             0x0F12, 0x0005, //70000CD8//AFIT8_yuvemix_mNegSlopes_3 [7:0] AFIT8_yuvemix_mPosSlopes_0
             0x0F12, 0x0607, //70000CDA//AFIT8_yuvemix_mPosSlopes_1 [7:0] AFIT8_yuvemix_mPosSlopes_2
             0x0F12, 0x0405, //70000CDC//AFIT8_yuvemix_mPosSlopes_3 [7:0] AFIT8_yuviirnr_iXSupportY
             0x0F12, 0x0205, //70000CDE//AFIT8_yuviirnr_iXSupportUV [7:0] AFIT8_yuviirnr_iLowYNorm
             0x0F12, 0x0304, //70000CE0//AFIT8_yuviirnr_iHighYNorm   [7:0] AFIT8_yuviirnr_iLowUVNorm
             0x0F12, 0x0409, //70000CE2//AFIT8_yuviirnr_iHighUVNorm [7:0] AFIT8_yuviirnr_iYNormShift
             0x0F12, 0x0306, //70000CE4//AFIT8_yuviirnr_iUVNormShift   [7:0] AFIT8_yuviirnr_iVertLength_Y
             0x0F12, 0x0407, //70000CE6//AFIT8_yuviirnr_iVertLength_UV   [7:0] AFIT8_yuviirnr_iDiffThreshL_Y
             0x0F12, 0x2C04, //70000CE8//AFIT8_yuviirnr_iDiffThreshH_Y   [7:0] AFIT8_yuviirnr_iDiffThreshL_UV
             0x0F12, 0x022C, //70000CEA//AFIT8_yuviirnr_iDiffThreshH_UV [7:0] AFIT8_yuviirnr_iMaxThreshL_Y
             0x0F12, 0x1402, //70000CEC//AFIT8_yuviirnr_iMaxThreshH_Y   [7:0] AFIT8_yuviirnr_iMaxThreshL_UV
             0x0F12, 0x0618, //70000CEE//AFIT8_yuviirnr_iMaxThreshH_UV   [7:0] AFIT8_yuviirnr_iYNRStrengthL
             0x0F12, 0x1A02, //70000CF0//AFIT8_yuviirnr_iYNRStrengthH   [7:0] AFIT8_yuviirnr_iUVNRStrengthL
             0x0F12, 0x8018, //70000CF2//AFIT8_yuviirnr_iUVNRStrengthH   [7:0] AFIT8_byr_gras_iShadingPower
             0x0F12, 0x0080, //70000CF4//AFIT8_RGBGamma2_iLinearity [7:0] AFIT8_RGBGamma2_iDarkReduce
             0x0F12, 0x0080, //70000CF6//AFIT8_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
             0x0F12, 0x0180, //70000CF8//AFIT8_RGB2YUV_iRGBGain [7:0] AFIT8_bnr_nClustLevel_H
             0x0F12, 0x0A0A, //70000CFA//AFIT8_bnr_iClustMulT_H [7:0] AFIT8_bnr_iClustMulT_C
             0x0F12, 0x0101, //70000CFC//AFIT8_bnr_iClustThresh_H   [7:0] AFIT8_bnr_iClustThresh_C
             0x0F12, 0x0C0F, //70000CFE//AFIT8_bnr_iDenThreshLow   [7:0] AFIT8_bnr_iDenThreshHigh
             0x0F12, 0x8030, //70000D00//AFIT8_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
             0x0F12, 0x0808, //70000D02//AFIT8_ee_iLowShDenoise [7:0] AFIT8_ee_iHighShDenoise
             0x0F12, 0xFFFF, //70000D04//AFIT8_ee_iLowSharpClamp   [7:0] AFIT8_ee_iHighSharpClamp
             0x0F12, 0x0808, //70000D06//AFIT8_ee_iReduceEdgeMinMult   [7:0] AFIT8_ee_iReduceEdgeSlope
             0x0F12, 0x0A01, //70000D08//AFIT8_bnr_nClustLevel_H_Bin   [7:0] AFIT8_bnr_iClustMulT_H_Bin
             0x0F12, 0x010A, //70000D0A//AFIT8_bnr_iClustMulT_C_Bin [7:0] AFIT8_bnr_iClustThresh_H_Bin
             0x0F12, 0x0F01, //70000D0C//AFIT8_bnr_iClustThresh_C_Bin   [7:0] AFIT8_bnr_iDenThreshLow_Bin
             0x0F12, 0x3012, //70000D0E//AFIT8_bnr_iDenThreshHigh_Bin   [7:0] AFIT8_ee_iLowSharpPower_Bin
             0x0F12, 0x0C80, //70000D10//AFIT8_ee_iHighSharpPower_Bin   [7:0] AFIT8_ee_iLowShDenoise_Bin
             0x0F12, 0xFF08, //70000D12//AFIT8_ee_iHighShDenoise_Bin   [7:0] AFIT8_ee_iLowSharpClamp_Bin
             0x0F12, 0x08FF, //70000D14//AFIT8_ee_iHighSharpClamp_Bin   [7:0] AFIT8_ee_iReduceEdgeMinMult_Bin
             0x0F12, 0x0008, //70000D16//AFIT8_ee_iReduceEdgeSlope_Bin [7:0]
             0x0F12, 0x0001,   //70000D18 AFITB_bnr_nClustLevel_C    [0]   bWideWide[1]
             0x0F12, 0x23CE, //70000D19//ConstAfitBaseVals
             0x0F12, 0xFDC8, //70000D1A//ConstAfitBaseVals
             0x0F12, 0x112E, //70000D1B//ConstAfitBaseVals
             0x0F12, 0x93A5, //70000D1C//ConstAfitBaseVals
             0x0F12, 0xFE67, //70000D1D//ConstAfitBaseVals
             0x0F12, 0x0000, //70000D1E//ConstAfitBaseVals

             0x002A ,0x0A1E,
             0x0F12 ,0x0350, //0x0028 AfitBaseVals_0__73_ 0040
    //==================================================================================
    // 18.JPEG Thumnail Setting
    //==================================================================================
#if defined(__CAPTURE_JPEG_OUTPUT__)
             //1. Enable Bit-Rate Control
             //2. Disable Thumbnail
             //3. Quality 95%
             0x002A, 0x0476,
             0x0F12, 0x0000, //REG_TC_BRC_BRC_type //0x5
             0x002A, 0x0478,
             0x0F12, 0x005F, //REG_TC_BRC_usPrevQuality
             0x0F12, 0x005F, //REG_TC_BRC_usCaptureQuality
             0x002A, 0x047c,
             0x0F12, 0x0000, //REG_TC_THUMB_Thumb_bActive
#endif

             //s002A0478
             //s0F12005F //REG_TC_BRC_usPrevQuality
             //s0F12005F //REG_TC_BRC_usCaptureQuality
             //s0F120001 //REG_TC_THUMB_Thumb_bActive
             //s0F120280 //REG_TC_THUMB_Thumb_uWidth
             //s0F1201E0 //REG_TC_THUMB_Thumb_uHeight
             //s0F120005 //REG_TC_THUMB_Thumb_Format
             //s002A17DC
             //s0F120054 //jpeg_ManualMBCV
             //s002A1AE4
             //s0F12001C //senHal_bExtraAddLine
             //s002A0284
             //s0F120001 //REG_TC_GP_bBypassScalerJpg
             //s002A028A
             //s0F120000 //REG_TC_GP_bUse1FrameCaptureMode
             //s002A1CC2 //DRx_uDRxWeight for AutoCont function
             //s0F120100
             //s0F120100
             //s0F120100
             //s0F120100

             0x002A, 0x0EE2,      //System Setting
             0x0F12, 0x0010,      //0x5DC0

    //==================================================================================
    // 10.Clock Setting
    //==================================================================================
    //For MCLK=24MHz, PCLK=5DC0
             0x002A, 0x01F8,      //System Setting
             0x0F12, (S5K4ECGX_MCLK*1000), //0x5DC0
             0x002A, 0x0212,
             0x0F12, 0x0000,   //0x212:REG_TC_IPRM_UseNPviClocks
             0x0F12, 0x0002,   //0x214:REG_TC_IPRM_UseNMipiClocks
             0x0F12, 0x0002,   //0x216:REG_TC_IPRM_NumberOfMipiLanes

             0x002A, 0x021A,
             0x0F12, MIPI_CLK0_SYS_OP_RATE,  //0x4F1A//0x3A98//0x32c8);
             0x0F12, MIPI_CLK0_MAX, //REG_TC_IPRM_MinOutRate4KHz_0 PCLK Min : 81Mhz
             0x0F12, MIPI_CLK0_MIN, //REG_TC_IPRM_MaxOutRate4KHz_0 PCLK Max : 81Mhz

             0x0F12, MIPI_CLK1_SYS_OP_RATE,  //0x32c8//0x4F1A;
             0x0F12, MIPI_CLK1_MIN, //REG_TC_IPRM_MinOutRate4KHz_1 PCLK Min : 81Mhz
             0x0F12, MIPI_CLK1_MAX, //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 81Mhz

    //==================================================================================
    // 19.Input Size Setting
    //==================================================================================
    //Input Size
             0x002A, 0x0250,
             0x0F12, 0x0A00, //REG_TC_GP_PrevReqInputWidth
             0x0F12, 0x0780, //REG_TC_GP_PrevReqInputHeight
             0x0F12, 0x0010, //REG_TC_GP_PrevInputWidthOfs
             0x0F12, 0x000C, //REG_TC_GP_PrevInputHeightOfs
             0x0F12, 0x0A00, //REG_TC_GP_CapReqInputWidth
             0x0F12, 0x0780, //REG_TC_GP_CapReqInputHeight
             0x0F12, 0x0010, //REG_TC_GP_CapInputWidthOfs
             0x0F12, 0x000C, //REG_TC_GP_CapInputHeightOfs
             0x002A, 0x0494,
             0x0F12, 0x0A00, //REG_TC_PZOOM_ZoomInputWidth
             0x0F12, 0x0780, //REG_TC_PZOOM_ZoomInputHeight
             0x0F12, 0x0000, //REG_TC_PZOOM_ZoomInputWidthOfs
             0x0F12, 0x0000, //REG_TC_PZOOM_ZoomInputHeightOfs
             0x0F12, 0x0A00, //REG_TC_CZOOM_ZoomInputWidth
             0x0F12, 0x0780, //REG_TC_CZOOM_ZoomInputHeight
             0x0F12, 0x0000, //REG_TC_CZOOM_ZoomInputWidthOfs
             0x0F12, 0x0000, //REG_TC_CZOOM_ZoomInputHeightOfs
             0x002A, 0x0262,
             0x0F12, 0x0001, //REG_TC_GP_bUseReqInputInPre
             0x0F12, 0x0001, //REG_TC_GP_bUseReqInputInCap


    //Preview config[0]: normal mode
    //91MHz, 1280x960, Dynamic mode 30~7.5fps
             0x002A, 0x02A6,   //Configuration Setting//Normal mode(VGA preview 30~15fps)
             0x0F12, S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV,   //REG_0TC_PCFG_usWidth: 1280
             0x0F12, S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV,   //REG_0TC_PCFG_usHeight: 960
             0x0F12, 0x0005,   //REG_0TC_PCFG_Format 5 YUV 7 Raw 9 JPG
             0x0F12, MIPI_CLK0_MAX,  //REG_0TC_PCFG_usMaxOut4KHzRate PCLK Min : xxMhz
             0x0F12, MIPI_CLK0_MIN,  //REG_0TC_PCFG_usMinOut4KHzRate PCLK Max : xxMhz
             0x0F12, 0x0100,   //REG_0TC_PCFG_OutClkPerPix88   0x100: 256
             0x0F12, 0x0300,   //REG_0TC_PCFG_uBpp88
             0x0F12, 0x0002,   //REG_0TC_PCFG_PVIMask
             0x0F12, 0x0000,   //REG_0TC_PCFG_OIFMask
             0x0F12, 0x01E0,   //REG_0TC_PCFG_usJpegPacketSize
             0x0F12, 0x0000,   //REG_0TC_PCFG_usJpegTotalPackets
             0x0F12, 0x0000,   //REG_0TC_PCFG_uClockInd
             0x0F12, 0x0000,   //REG_0TC_PCFG_usFrTimeType
             0x0F12, 0x0001,   //REG_0TC_PCFG_FrRateQualityType
             0x0F12, 0x0535, //REG_0TC_PCFG_usMaxFrTimeMsecMult10
             0x0F12, 0x014D, //REG_0TC_PCFG_usMinFrTimeMsecMult10
             0x002A, 0x02D0,
             0x0F12, 0x0000,   //REG_0TC_PCFG_uPrevMirror
             0x0F12, 0x0000,   //REG_0TC_PCFG_uCaptureMirror
             0x0F12, 0x0000,   //REG_0TC_PCFG_uRotation
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }

    S5K4ECGX_PreviewWin[0].GrabStartX = 0;
    S5K4ECGX_PreviewWin[0].GrabStartY = 0;
    S5K4ECGX_PreviewWin[0].ExposureWindowWidth = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV;
    S5K4ECGX_PreviewWin[0].ExposureWindowHeight = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV;

    {
        static const kal_uint16 addr_data_pair[] =
        {
             //Preview config[1]: video mode
             //91MHz, 1280x960, Fix 30fps
             0x002A, 0x02D6, //Night mode(VGA preview 30~4fps)
             0x0F12, 1280,   //REG_1TC_PCFG_usWidth
             0x0F12, 960,    //REG_1TC_PCFG_usHeight
             0x0F12, 0x0005,   //REG_1TC_PCFG_Format 5 YUV 7 Raw 9 JPG
             0x0F12, MIPI_CLK0_MAX,  //REG_1TC_PCFG_usMaxOut4KHzRate PCLK Min : xxMhz
             0x0F12, MIPI_CLK0_MIN,  //REG_1TC_PCFG_usMinOut4KHzRate PCLK Max : xxMhz
             0x0F12, 0x0100,   //REG_1TC_PCFG_OutClkPerPix88
             0x0F12, 0x0300,   //REG_1TC_PCFG_uBpp88
             0x0F12, 0x0002,   //REG_1TC_PCFG_PVIMask
             0x0F12, 0x0000,   //REG_1TC_PCFG_OIFMask
             0x0F12, 0x01E0,   //REG_1TC_PCFG_usJpegPacketSize
             0x0F12, 0x0000,   //REG_1TC_PCFG_usJpegTotalPackets
             0x0F12, 0x0000,   //REG_1TC_PCFG_uClockInd
             0x0F12, 0x0002,   //REG_1TC_PCFG_usFrTimeType
             0x0F12, 0x0001,   //REG_1TC_PCFG_FrRateQualityType
             0x0F12, 0x014d,   //REG_1TC_PCFG_usMaxFrTimeMsecMult10
             0x0F12, 0x014d,   //REG_1TC_PCFG_usMinFrTimeMsecMult10
             0x002A, 0x0300,
             0x0F12, 0x0000,   //REG_1TC_PCFG_uPrevMirror
             0x0F12, 0x0000,   //REG_1TC_PCFG_uCaptureMirror
             0x0F12, 0x0000,   //REG_1TC_PCFG_uRotation
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }
    S5K4ECGX_PreviewWin[1].GrabStartX = 0;
    S5K4ECGX_PreviewWin[1].GrabStartY = 0;
    S5K4ECGX_PreviewWin[1].ExposureWindowWidth = 0x0500;
    S5K4ECGX_PreviewWin[1].ExposureWindowHeight = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV;


    {
        static const kal_uint16 addr_data_pair[] =
        {
             //Preview config[2]:
             //91MHz, 2560x1920, ZSD 12 ~ 7.5fps
             0x002A, 0x0306,   //Night mode(VGA preview 30~4fps)
             0x0F12, 2560,   //REG_2TC_PCFG_usWidth
             0x0F12, 1920,   //REG_2TC_PCFG_usHeight
             0x0F12, 0x0005,   //REG_2TC_PCFG_Format 5 YUV 7 Raw 9 JPG
             0x0F12, MIPI_CLK0_MAX,  //REG_2TC_PCFG_usMaxOut4KHzRate PCLK Min : xxMhz
             0x0F12, MIPI_CLK0_MIN,  //REG_2TC_PCFG_usMinOut4KHzRate PCLK Max : xxMhz
             0x0F12, 0x0100,   //REG_2TC_PCFG_OutClkPerPix88
             0x0F12, 0x0300,   //REG_2TC_PCFG_uBpp88
             0x0F12, 0x0002,   //REG_2TC_PCFG_PVIMask
             0x0F12, 0x0000,   //REG_2TC_PCFG_OIFMask
             0x0F12, 0x01E0,   //REG_2TC_PCFG_usJpegPacketSize
             0x0F12, 0x0000,   //REG_2TC_PCFG_usJpegTotalPackets
             0x0F12, 0x0000,   //REG_2TC_PCFG_uClockInd
             0x0F12, 0x0000,   //REG_2TC_PCFG_usFrTimeType
             0x0F12, 0x0002,   //REG_2TC_PCFG_FrRateQualityType
             0x0F12, 0x0535,   //REG_2TC_PCFG_usMaxFrTimeMsecMult10
             0x0F12, 0x0341,   //REG_2TC_PCFG_usMinFrTimeMsecMult10
             0x002A, 0x0330,
             0x0F12, 0x0000,   //REG_2TC_PCFG_uPrevMirror
             0x0F12, 0x0000,   //REG_2TC_PCFG_uCaptureMirror
             0x0F12, 0x0000,   //REG_2TC_PCFG_uRotation
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }
    S5K4ECGX_PreviewWin[2].GrabStartX = 0;
    S5K4ECGX_PreviewWin[2].GrabStartY = 0;
    S5K4ECGX_PreviewWin[2].ExposureWindowWidth = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV;
    S5K4ECGX_PreviewWin[2].ExposureWindowHeight = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV;



    {
        static const kal_uint16 addr_data_pair[]=
        {
             //Capture config[0]: 12 ~ 7.5fps
             0x002A, 0x0396,  //Normal mode Capture(7.5fps)
             0x0F12, 0x0001,   //REG_0TC_CCFG_uCaptureMode//[Sophie Add]
             0x002A, 0x0398,
             0x0F12, S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV,   //REG_0TC_CCFG_usWidth (5M)
             0x0F12, S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV,   //REG_0TC_CCFG_usHeight
             0x0F12, OUTPUT_FMT,   //REG_0TC_PCFG_Format 5 YUV 7 Raw 9 JPG
             0x0F12, MIPI_CLK1_MAX,  //REG_0TC_CCFG_usMaxOut4KHzRate PCLK Min : xxMhz
             0x0F12, MIPI_CLK1_MIN,  //REG_0TC_CCFG_usMinOut4KHzRate PCLK Max : xxMhz
             0x0F12, 0x0100,   //REG_0TC_CCFG_OutClkPerPix88
             0x0F12, 0x0300,   //REG_0TC_CCFG_uBpp88
             0x0F12, 0x0002,   //REG_0TC_CCFG_PVIMask //[Sophie Add]
             0x0F12, 0x0070,   //REG_0TC_CCFG_OIFMask
             0x0F12, 0x0810,   //REG_0TC_CCFG_usJpegPacketSize
             0x0F12, 0x0000,   //REG_0TC_CCFG_usJpegTotalPackets
             0x0F12, MIPI_CAP_CLK_IDX,   //REG_0TC_CCFG_uClockInd
             0x0F12, 0x0000,   //REG_0TC_CCFG_usFrTimeType
             0x0F12, 0x0002,   //REG_0TC_CCFG_FrRateQualityType
             0x0F12, 0x0535,   //REG_0TC_CCFG_usMaxFrTimeMsecMult10 0x029A
             0x0F12, 0x0341,   //REG_0TC_CCFG_usMinFrTimeMsecMult10


             0x002A, 0x0250,
             0x0F12, 0x0A00,   //REG_TC_GP_PrevReqInputWidth
             0x0F12, 0x0780,   //REG_TC_GP_PrevReqInputHeight
             0x0F12, 0x0010,   //REG_TC_GP_PrevInputWidthOfs
             0x0F12, 0x000C,   //REG_TC_GP_PrevInputHeightOfs
             0x0F12, 0x0A00,   //REG_TC_GP_CapReqInputWidth
             0x0F12, 0x0780,   //REG_TC_GP_CapReqInputHeight
             0x0F12, 0x0010,   //REG_TC_GP_CapInputWidthOfs
             0x0F12, 0x000C,   //REG_TC_GP_CapInputHeightOfs
             0x002A, 0x0494,
             0x0F12, 0x0A00,
             0x0F12, 0x0780,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x0F12, 0x0A00,
             0x0F12, 0x0780,
             0x0F12, 0x0000,
             0x0F12, 0x0000,
             0x002A, 0x0262,
             0x0F12, 0x0001,
             0x0F12, 0x0001,
             0x002A, 0x1CC2,   //DRx_uDRxWeight for AutoCont function
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x0F12, 0x0100,
             0x002A, 0x022C,
             0x0F12, 0x0001,   //REG_TC_IPRM_InitParamsUpdated

    //==================================================================================
    // 21.Select Cofigration Display
    //==================================================================================
    //PREVIEW
    //Select preview 0
             0x002A, 0x0266,
             0x0F12, 0x0000, //REG_TC_GP_ActivePrevConfig
             0x002A, 0x026A,
             0x0F12, 0x0001, //REG_TC_GP_PrevOpenAfterChange
             0x002A, 0x0268,
             0x0F12, 0x0001, //REG_TC_GP_PrevConfigChanged
             0x002A, 0x026E,
             0x0F12, 0x0000, //REG_TC_GP_ActiveCapConfig
             0x002A, 0x026A,
             0x0F12, 0x0001, //REG_TC_GP_CapOpenAfterChange
             0x002A, 0x0270,
             0x0F12, 0x0001, //REG_TC_GP_CapConfigChanged
             0x002A, 0x024E,
             0x0F12, 0x0001, //REG_TC_GP_NewConfigSync
             0x002A, 0x023E,
             0x0F12, 0x0001, //REG_TC_GP_EnablePreview
             0x0F12, 0x0001, //REG_TC_GP_EnablePreviewChanged
        };
        S5K4ECGX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
    }

   //===================================================================================
   // 22. ESD Check
   //===================================================================================
   //S5K4ECGX_write_cmos_sensor(0x002A, 0x01A8);
   //S5K4ECGX_write_cmos_sensor(0x0F12, 0xAAAA);

   //===================================================================================
   // 23. Brightness min/Max
   //===================================================================================
   //S5K4ECGX_write_cmos_sensor(0x0028 ,0x147C);
   //S5K4ECGX_write_cmos_sensor(0x002A ,0x01AA);
   //S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0180);   //bp_uMaxBrightnessFactor
   //S5K4ECGX_write_cmos_sensor(0x0028 ,0x1482);
   //S5K4ECGX_write_cmos_sensor(0x002A ,0x01AC);
   //S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0180);   //bp_uMinBrightnessFactor

    SENSORDB("Sensor Init Done\n");
   return;
}



static void S5K4ECGX_MIPI_enb_preview(void)
{
   SENSORDB("[4EC] Enable preview...\n");
   S5K4ECGX_write_cmos_sensor(0x002A, 0x023E);
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Enable Preview output
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Sync FW with Enable preview

   //SENSORDB("[4EC] Enable preview done...\n");
}

static void S5K4ECGX_Init_Setting(void)
{
    printk("[4EC Parallel] Sensor Init...\n");
                // FOR 4EC EVT1.1
                // ARM Initiation

    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0010, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x1030, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0014, 0x0001);
    mdelay(50);
    S5K4ECGX_write_cmos_sensor(0x0028, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1082);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0155);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0155);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0055);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05D5);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x100E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x007A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xE406);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0092);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xE410);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3804);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xE41A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xE420);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xE42E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xF400);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A3C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0023);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03AF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAA54);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x464E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0240);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0240);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x55FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0401);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0088);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x009F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0088);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2428);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03EE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0xF552);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0708);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x080C);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x18BC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05B6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0007);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05B6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0075);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00CF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0075);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x029E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0228);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0208);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0238);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0218);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0238);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0009);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00DE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00DF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00E4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01FD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05B6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05BB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0077);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x007E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x025E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0326);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0327);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0084);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x008D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03AD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09CD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02DE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02BE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02EE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02CE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02EE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0009);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0095);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09DB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0096);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x009B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02B3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0009);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0327);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0336);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1AF8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A3C);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1896);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x189E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0FB0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x18AC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05C0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1AEA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1AE0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1A72);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x18A2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1A6A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x009A);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x385E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024C);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0EE6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1B2A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x008D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00CF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0084);

    //==================================================================================
    //Gas_Anti Shading_Otp.no OTP)
    //==================================================================================
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0722);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //skl_OTP_usWaitTime This register should be positioned in fornt of D0001000
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0726);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //skl_bUseOTPfunc This is OTP on/off function
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //ash_bUseOTPData
    S5K4ECGX_write_cmos_sensor(0x002A, 0x146E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //awbb_otp_disable
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08DC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //ash_bUseGasAlphaOTP

    // TVAR_ash_pGAS_high
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0D26);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F00);

    // TVAR_ash_pGAS_low
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0DB6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x94C8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE78D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF985);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2237);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEA86);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x013A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xADE0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B8F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1276);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD850);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B55);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x158F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5E3B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF62B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE9A1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2952);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x073E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC818);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA6F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFD77);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0DB0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE9D3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF39F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3BCD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3CA4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x133B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A3C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF18D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF760);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF6B3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF5AE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE8D4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF00F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1834);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x186A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE0AE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA83C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDFE0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFED4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x20B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE8B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0541);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA78D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B62);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1364);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD247);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x17C7);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B8A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5817);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF488);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEC21);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2E7E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF8AF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD353);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBBCB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE7F9);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFE21);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x360B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x247E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0E82);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x14DB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEF9E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF488);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF513);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x040F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEAA4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE8A7);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1ECC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1084);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE896);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x994F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDFC8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFF6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2643);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD8D2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x11C8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAB93);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x149D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x09A7);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD77A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x170D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B56);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5E42);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEB77);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF5A5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2301);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC746);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB08D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x06FE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0335);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xED10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xED93);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x48AB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2982);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B93);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF758);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF8DA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE6DA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03F5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xED1E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xECBF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1528);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x15E8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEBFE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x99D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE4DE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFD52);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1CBC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF06E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFD9B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAC4A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0D08);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1285);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD2A1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1426);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1132);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5DDB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF3CE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE728);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3A5E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xED32);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD64C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB144);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF27);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD985);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1209);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2B0C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2A96);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x12A1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x16F9);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEEDF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEDD3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFC25);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x021C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE89F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE2E5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2854);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0BE7);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE768);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08B4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //wbt_bUseOutdoorASH
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08BC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0); //TVAR_ash_AwbAshCord_0_ 2300K
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00DF); //TVAR_ash_AwbAshCord_1_ 2750K
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //TVAR_ash_AwbAshCord_2_ 3300K
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0125); //TVAR_ash_AwbAshCord_3_ 4150K
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x015F); //TVAR_ash_AwbAshCord_4_ 5250K
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x017C); //TVAR_ash_AwbAshCord_5_ 6400K
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0194); //TVAR_ash_AwbAshCord_6_ 7500K
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08F6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3B00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);

    //Outdoor Gas Alpha
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4500);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //ash_bUseGasAlpha



    S5K4ECGX_write_cmos_sensor(0x002A, 0x189E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0FB0);       //#senHal_ExpMinPixels
    S5K4ECGX_write_cmos_sensor(0x002A, 0x18AC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);       //#senHal_uAddColsBin
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);       //#senHal_uAddColsNoBin
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05C0);       //#senHal_uMinColsBin
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05C0);       //#senHal_uMinColsNoBin
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1AEA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8080);       //#senHal_SubF404Tune
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);       //#senHal_FullF404Tune
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1AE0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);       //#senHal_bSenAAC
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1A72);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);       //#senHal_bSRX  //SRX off
    S5K4ECGX_write_cmos_sensor(0x002A, 0x18A2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);       //#senHal_NExpLinesCheckFine     //extend Forbidden area line
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1A6A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x009A);       //#senHal_usForbiddenRightOfs    //extend right Forbidden area line
    S5K4ECGX_write_cmos_sensor(0x002A, 0x385E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024C);       //#Mon_Sen_uExpPixelsOfs
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0EE6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);       //#setot_bUseDigitalHbin
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1B2A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);  //senHal_TuneStr2_usAngTuneGainTh
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D6);  //senHal_TuneStr2_AngTuneF4CA_0_
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x008D);  //senHal_TuneStr2_AngTuneF4CA_1_
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00CF);  //senHal_TuneStr2_AngTuneF4C2_0_
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0084);  //senHal_TuneStr2_AngTuneF4C2_1_

       ///////////////////////////////////////////////////////////////////////////

       // OTP setting

       ///////////////////////////////////////////////////////////////////////////

    S5K4ECGX_write_cmos_sensor(0x002A, 0x0722);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);       //#skl_OTP_usWaitTime  // This register should be positioned in fornt of D0001000. // Check
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0726);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //skl_bUseOTPfunc OTP shading is used,this reg should be 1  //
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08D6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);       //#ash_bUseOTPData          // If OTP for shading is used, this register should be enable. ( default : disable)
    S5K4ECGX_write_cmos_sensor(0x002A, 0x146E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);       //#awbb_otp_disable         // If OTP for AWB is used, this register should be enable.
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08DC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);       //#ash_bUseGasAlphaOTP          // If OTP alpha for shading is used, this register should be enable.

       ///////////////////////////////////////////////////////////////////////////

       // TnP setting

       ///////////////////////////////////////////////////////////////////////////

       // Start of Patch data

    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x3AF8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB570);    // 70003AF8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4B3D);    // 70003AFA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A3D);    // 70003AFC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483E);    // 70003AFE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2100);    // 70003B00
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC008);    // 70003B02
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6002);    // 70003B04
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);    // 70003B06
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x493C);    // 70003B08
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483D);    // 70003B0A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2401);    // 70003B0C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B0E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFC3B);    // 70003B10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x493C);    // 70003B12
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483C);    // 70003B14
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003B16
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2502);    // 70003B18
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B1A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFC35);    // 70003B1C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483B);    // 70003B1E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0261);    // 70003B20
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8001);    // 70003B22
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2100);    // 70003B24
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8041);    // 70003B26
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4939);    // 70003B28
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483A);    // 70003B2A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6041);    // 70003B2C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x493A);    // 70003B2E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483A);    // 70003B30
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2403);    // 70003B32
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003B34
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B36
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFC27);    // 70003B38
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4939);    // 70003B3A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4839);    // 70003B3C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6001);    // 70003B3E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4939);    // 70003B40
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3840);    // 70003B42
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x63C1);    // 70003B44
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4933);    // 70003B46
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4838);    // 70003B48
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3980);    // 70003B4A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6408);    // 70003B4C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4838);    // 70003B4E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4938);    // 70003B50
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6388);    // 70003B52
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4938);    // 70003B54
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4839);    // 70003B56
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003B58
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2504);    // 70003B5A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B5C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFC14);    // 70003B5E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4937);    // 70003B60
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4838);    // 70003B62
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2405);    // 70003B64
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003B66
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B68
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF890);    // 70003B6A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4835);    // 70003B6C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4936);    // 70003B6E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003B70
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2506);    // 70003B72
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1D80);    // 70003B74
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B76
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF889);    // 70003B78
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4832);    // 70003B7A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4933);    // 70003B7C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2407);    // 70003B7E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003B80
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x300C);    // 70003B82
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B84
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF882);    // 70003B86
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482E);    // 70003B88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4931);    // 70003B8A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003B8C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2508);    // 70003B8E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3010);    // 70003B90
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B92
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF87B);    // 70003B94
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492F);    // 70003B96
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482F);    // 70003B98
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2409);    // 70003B9A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003B9C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003B9E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBF3);    // 70003BA0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492E);    // 70003BA2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482E);    // 70003BA4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003BA6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x250A);    // 70003BA8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003BAA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBED);    // 70003BAC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492D);    // 70003BAE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482D);    // 70003BB0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x240B);    // 70003BB2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003BB4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003BB6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBE7);    // 70003BB8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492C);    // 70003BBA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482C);    // 70003BBC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x250C);    // 70003BBE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003BC0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003BC2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBE1);    // 70003BC4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492B);    // 70003BC6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482B);    // 70003BC8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003BCA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x240D);    // 70003BCC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003BCE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBDB);    // 70003BD0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492A);    // 70003BD2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482A);    // 70003BD4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x250E);    // 70003BD6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003BD8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003BDA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBD5);    // 70003BDC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4929);    // 70003BDE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4829);    // 70003BE0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003BE2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003BE4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFBD0);    // 70003BE6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC70);    // 70003BE8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70003BEA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70003BEC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003BEE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0184);    // 70003BF0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4EC2);    // 70003BF2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03FF);    // 70003BF4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);    // 70003BF6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1F90);    // 70003BF8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003BFA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3CA5);    // 70003BFC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003BFE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE38B);    // 70003C00
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C02
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3CDD);    // 70003C04
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C06
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC3B1);    // 70003C08
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C0A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4780);    // 70003C0C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C0E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3D3B);    // 70003C10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C12
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);    // 70003C14
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C16
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3D77);    // 70003C18
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C1A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB49D);    // 70003C1C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C1E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3F9F);    // 70003C20
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C22
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);    // 70003C24
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C26
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3E23);    // 70003C28
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C2A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3DD7);    // 70003C2C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C2E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF);    // 70003C30
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00FF);    // 70003C32
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x17E0);    // 70003C34
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C36
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3FBD);    // 70003C38
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C3A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x053D);    // 70003C3C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C3E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C40
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A89);    // 70003C42
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6CD2);    // 70003C44
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C46
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C9);    // 70003C48
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C4A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C4C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A9A);    // 70003C4E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C50
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D2);    // 70003C52
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x400B);    // 70003C54
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C56
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9E65);    // 70003C58
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C5A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x40DD);    // 70003C5C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C5E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7C49);    // 70003C60
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C62
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x40F9);    // 70003C64
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C66
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7C63);    // 70003C68
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C6A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4115);    // 70003C6C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C6E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8F01);    // 70003C70
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C72
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x41B7);    // 70003C74
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C76
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7F3F);    // 70003C78
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C7A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4245);    // 70003C7C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C7E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x98C5);    // 70003C80
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C82
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x42FB);    // 70003C84
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70003C86
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xCD59);    // 70003C88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70003C8A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB570);    // 70003C8C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);    // 70003C8E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0015);    // 70003C90
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 70003C92
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003C94
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFB80);    // 70003C96
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x49F9);    // 70003C98
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A8);    // 70003C9A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x500C);    // 70003C9C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC70);    // 70003C9E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70003CA0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70003CA2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6808);    // 70003CA4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003CA6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003CA8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6849);    // 70003CAA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);    // 70003CAC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C09);    // 70003CAE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4AF4);    // 70003CB0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8992);    // 70003CB2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2A00);    // 70003CB4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD00D);    // 70003CB6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2300);    // 70003CB8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A89);    // 70003CBA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD400);    // 70003CBC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);    // 70003CBE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0419);    // 70003CC0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C09);    // 70003CC2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x23FF);    // 70003CC4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x33C1);    // 70003CC6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1810);    // 70003CC8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4298);    // 70003CCA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD800);    // 70003CCC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);    // 70003CCE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0418);    // 70003CD0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003CD2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4AEC);    // 70003CD4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8150);    // 70003CD6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8191);    // 70003CD8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4770);    // 70003CDA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB5F3);    // 70003CDC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);    // 70003CDE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB081);    // 70003CE0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9802);    // 70003CE2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6800);    // 70003CE4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0600);    // 70003CE6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0E00);    // 70003CE8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2201);    // 70003CEA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0015);    // 70003CEC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0021);    // 70003CEE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3910);    // 70003CF0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x408A);    // 70003CF2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x40A5);    // 70003CF4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4FE5);    // 70003CF6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0016);    // 70003CF8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C10);    // 70003CFA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA03);    // 70003CFC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8839);    // 70003CFE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43A9);    // 70003D00
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8039);    // 70003D02
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE002);    // 70003D04
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8879);    // 70003D06
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43B1);    // 70003D08
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8079);    // 70003D0A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003D0C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFB4C);    // 70003D0E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C10);    // 70003D10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA03);    // 70003D12
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8839);    // 70003D14
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4329);    // 70003D16
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8039);    // 70003D18
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE002);    // 70003D1A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8879);    // 70003D1C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4331);    // 70003D1E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8079);    // 70003D20
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x49DB);    // 70003D22
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8809);    // 70003D24
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2900);    // 70003D26
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD102);    // 70003D28
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003D2A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFB45);    // 70003D2C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 70003D2E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9902);    // 70003D30
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6008);    // 70003D32
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBCFE);    // 70003D34
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70003D36
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70003D38
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB538);    // 70003D3A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9C04);    // 70003D3C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0015);    // 70003D3E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70003D40
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9400);    // 70003D42
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003D44
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFB40);    // 70003D46
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4AD2);    // 70003D48
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8811);    // 70003D4A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2900);    // 70003D4C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD00F);    // 70003D4E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8820);    // 70003D50
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4281);    // 70003D52
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD20C);    // 70003D54
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8861);    // 70003D56
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8853);    // 70003D58
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4299);    // 70003D5A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD200);    // 70003D5C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E40);    // 70003D5E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003D60
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003D62
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8020);    // 70003D64
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8851);    // 70003D66
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8061);    // 70003D68
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4368);    // 70003D6A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1840);    // 70003D6C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6060);    // 70003D6E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC38);    // 70003D70
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70003D72
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70003D74
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB5F8);    // 70003D76
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);    // 70003D78
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6808);    // 70003D7A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003D7C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003D7E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2201);    // 70003D80
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0015);    // 70003D82
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0021);    // 70003D84
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3910);    // 70003D86
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x408A);    // 70003D88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x40A5);    // 70003D8A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4FBF);    // 70003D8C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0016);    // 70003D8E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C10);    // 70003D90
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA03);    // 70003D92
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8839);    // 70003D94
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43A9);    // 70003D96
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8039);    // 70003D98
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE002);    // 70003D9A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8879);    // 70003D9C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43B1);    // 70003D9E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8079);    // 70003DA0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003DA2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFB19);    // 70003DA4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C10);    // 70003DA6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA03);    // 70003DA8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8838);    // 70003DAA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4328);    // 70003DAC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8038);    // 70003DAE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE002);    // 70003DB0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8878);    // 70003DB2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4330);    // 70003DB4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8078);    // 70003DB6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x48B7);    // 70003DB8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8800);    // 70003DBA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003DBC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD507);    // 70003DBE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4BB6);    // 70003DC0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7819);    // 70003DC2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4AB6);    // 70003DC4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7810);    // 70003DC6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7018);    // 70003DC8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7011);    // 70003DCA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x49B5);    // 70003DCC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8188);    // 70003DCE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBCF8);    // 70003DD0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70003DD2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70003DD4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB538);    // 70003DD6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x48B3);    // 70003DD8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4669);    // 70003DDA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003DDC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFB04);    // 70003DDE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x48B2);    // 70003DE0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x49B1);    // 70003DE2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x69C2);    // 70003DE4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2400);    // 70003DE6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x31A8);    // 70003DE8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2A00);    // 70003DEA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD008);    // 70003DEC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x61C4);    // 70003DEE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x684A);    // 70003DF0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6242);    // 70003DF2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6282);    // 70003DF4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 70003DF6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x881A);    // 70003DF8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6302);    // 70003DFA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x885A);    // 70003DFC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6342);    // 70003DFE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6A02);    // 70003E00
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2A00);    // 70003E02
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD00A);    // 70003E04
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6204);    // 70003E06
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6849);    // 70003E08
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6281);    // 70003E0A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 70003E0C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8819);    // 70003E0E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6301);    // 70003E10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8859);    // 70003E12
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6341);    // 70003E14
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x49A6);    // 70003E16
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88C9);    // 70003E18
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x63C1);    // 70003E1A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003E1C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFAEC);    // 70003E1E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE7A6);    // 70003E20
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB5F0);    // 70003E22
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB08B);    // 70003E24
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x20FF);    // 70003E26
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1C40);    // 70003E28
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x49A2);    // 70003E2A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x89CC);    // 70003E2C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4E9F);    // 70003E2E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6AB1);    // 70003E30
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4284);    // 70003E32
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD101);    // 70003E34
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x48A0);    // 70003E36
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6081);    // 70003E38
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6A70);    // 70003E3A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);    // 70003E3C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003E3E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFAE3);    // 70003E40
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003E42
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003E44
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A97);    // 70003E46
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A11);    // 70003E48
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9109);    // 70003E4A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2101);    // 70003E4C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0349);    // 70003E4E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4288);    // 70003E50
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD200);    // 70003E52
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);    // 70003E54
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A93);    // 70003E56
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8211);    // 70003E58
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4D98);    // 70003E5A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8829);    // 70003E5C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9108);    // 70003E5E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A8C);    // 70003E60
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2303);    // 70003E62
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3222);    // 70003E64
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1F91);    // 70003E66
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003E68
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFAD4);    // 70003E6A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8028);    // 70003E6C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x488F);    // 70003E6E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4988);    // 70003E70
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6BC2);    // 70003E72
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6AC0);    // 70003E74
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4282);    // 70003E76
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD201);    // 70003E78
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8CC8);    // 70003E7A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8028);    // 70003E7C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88E8);    // 70003E7E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9007);    // 70003E80
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2240);    // 70003E82
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4310);    // 70003E84
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x80E8);    // 70003E86
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 70003E88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0041);    // 70003E8A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x194B);    // 70003E8C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x001E);    // 70003E8E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3680);    // 70003E90
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8BB2);    // 70003E92
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAF04);    // 70003E94
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x527A);    // 70003E96
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A7E);    // 70003E98
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x188A);    // 70003E9A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8897);    // 70003E9C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x83B7);    // 70003E9E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x33A0);    // 70003EA0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x891F);    // 70003EA2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAE01);    // 70003EA4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5277);    // 70003EA6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A11);    // 70003EA8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8119);    // 70003EAA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1C40);    // 70003EAC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003EAE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003EB0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2806);    // 70003EB2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD3E9);    // 70003EB4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003EB6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFAB5);    // 70003EB8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003EBA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFABB);    // 70003EBC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4F7A);    // 70003EBE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x37A8);    // 70003EC0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2800);    // 70003EC2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD10A);    // 70003EC4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1FE0);    // 70003EC6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x38FD);    // 70003EC8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD001);    // 70003ECA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1CC0);    // 70003ECC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD105);    // 70003ECE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4875);    // 70003ED0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8829);    // 70003ED2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3818);    // 70003ED4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6840);    // 70003ED6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4348);    // 70003ED8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6078);    // 70003EDA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4973);    // 70003EDC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6878);    // 70003EDE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6B89);    // 70003EE0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4288);    // 70003EE2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD300);    // 70003EE4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);    // 70003EE6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6078);    // 70003EE8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 70003EEA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0041);    // 70003EEC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAA04);    // 70003EEE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A53);    // 70003EF0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x194A);    // 70003EF2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x269C);    // 70003EF4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x52B3);    // 70003EF6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAB01);    // 70003EF8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A59);    // 70003EFA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x32A0);    // 70003EFC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8111);    // 70003EFE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1C40);    // 70003F00
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003F02
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003F04
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2806);    // 70003F06
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD3F0);    // 70003F08
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4966);    // 70003F0A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9809);    // 70003F0C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8208);    // 70003F0E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9808);    // 70003F10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8028);    // 70003F12
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9807);    // 70003F14
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x80E8);    // 70003F16
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1FE0);    // 70003F18
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x38FD);    // 70003F1A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD13B);    // 70003F1C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4D65);    // 70003F1E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x89E8);    // 70003F20
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1FC1);    // 70003F22
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x39FF);    // 70003F24
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD136);    // 70003F26
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4C60);    // 70003F28
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8AE0);    // 70003F2A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F2C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA8A);    // 70003F2E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006);    // 70003F30
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8B20);    // 70003F32
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F34
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA8E);    // 70003F36
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9000);    // 70003F38
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6AA1);    // 70003F3A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6878);    // 70003F3C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1809);    // 70003F3E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);    // 70003F40
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F42
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA61);    // 70003F44
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70003F46
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70003F48
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);    // 70003F4A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3246);    // 70003F4C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0011);    // 70003F4E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x310A);    // 70003F50
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2305);    // 70003F52
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F54
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA5E);    // 70003F56
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x66E8);    // 70003F58
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6B23);    // 70003F5A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);    // 70003F5C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0031);    // 70003F5E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0018);    // 70003F60
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F62
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA7F);    // 70003F64
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 70003F66
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8518);    // 70003F68
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6EEA);    // 70003F6A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6B60);    // 70003F6C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9900);    // 70003F6E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F70
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA78);    // 70003F72
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 70003F74
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8558);    // 70003F76
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 70003F78
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x980A);    // 70003F7A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3170);    // 70003F7C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003F7E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA79);    // 70003F80
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0028);    // 70003F82
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3060);    // 70003F84
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A02);    // 70003F86
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4947);    // 70003F88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3128);    // 70003F8A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x808A);    // 70003F8C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A42);    // 70003F8E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x80CA);    // 70003F90
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A80);    // 70003F92
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8108);    // 70003F94
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB00B);    // 70003F96
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBCF0);    // 70003F98
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70003F9A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70003F9C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4845);    // 70003F9E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3060);    // 70003FA0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8881);    // 70003FA2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2900);    // 70003FA4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD007);    // 70003FA6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2100);    // 70003FA8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8081);    // 70003FAA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4944);    // 70003FAC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x20FF);    // 70003FAE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1C40);    // 70003FB0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8048);    // 70003FB2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2001);    // 70003FB4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4770);    // 70003FB6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 70003FB8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4770);    // 70003FBA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB570);    // 70003FBC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2400);    // 70003FBE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4D40);    // 70003FC0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4841);    // 70003FC2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8881);    // 70003FC4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4841);    // 70003FC6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8041);    // 70003FC8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2101);    // 70003FCA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8001);    // 70003FCC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003FCE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA59);    // 70003FD0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x483D);    // 70003FD2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3820);    // 70003FD4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8BC0);    // 70003FD6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70003FD8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA5C);    // 70003FDA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4B3C);    // 70003FDC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x220D);    // 70003FDE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0712);    // 70003FE0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18A8);    // 70003FE2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8806);    // 70003FE4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00E1);    // 70003FE6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18C9);    // 70003FE8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x81CE);    // 70003FEA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8846);    // 70003FEC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x818E);    // 70003FEE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8886);    // 70003FF0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x824E);    // 70003FF2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88C0);    // 70003FF4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8208);    // 70003FF6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3508);    // 70003FF8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x042D);    // 70003FFA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C2D);    // 70003FFC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1C64);    // 70003FFE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0424);    // 70004000
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C24);    // 70004002
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C07);    // 70004004
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD3EC);    // 70004006
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE649);    // 70004008
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB510);    // 7000400A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482E);    // 7000400C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4C2F);    // 7000400E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88C0);    // 70004010
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8060);    // 70004012
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2001);    // 70004014
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8020);    // 70004016
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482B);    // 70004018
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3820);    // 7000401A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8BC0);    // 7000401C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 7000401E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFA39);    // 70004020
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88E0);    // 70004022
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A2B);    // 70004024
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2800);    // 70004026
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD003);    // 70004028
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x492B);    // 7000402A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8849);    // 7000402C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2900);    // 7000402E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD009);    // 70004030
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2001);    // 70004032
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03C0);    // 70004034
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8050);    // 70004036
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x80D0);    // 70004038
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 7000403A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8090);    // 7000403C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8110);    // 7000403E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC10);    // 70004040
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70004042
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70004044
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8050);    // 70004046
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8920);    // 70004048
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x80D0);    // 7000404A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8960);    // 7000404C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 7000404E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1400);    // 70004050
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8090);    // 70004052
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x89A1);    // 70004054
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);    // 70004056
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1409);    // 70004058
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8111);    // 7000405A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x89E3);    // 7000405C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A24);    // 7000405E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2B00);    // 70004060
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD104);    // 70004062
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x17C3);    // 70004064
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F5B);    // 70004066
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1818);    // 70004068
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x10C0);    // 7000406A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8090);    // 7000406C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C00);    // 7000406E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD1E6);    // 70004070
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x17C8);    // 70004072
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F40);    // 70004074
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1840);    // 70004076
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x10C0);    // 70004078
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8110);    // 7000407A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE7E0);    // 7000407C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 7000407E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x38D4);    // 70004080
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004082
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x17D0);    // 70004084
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004086
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5000);    // 70004088
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 7000408A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1100);    // 7000408C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 7000408E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x171A);    // 70004090
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004092
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4780);    // 70004094
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004096
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2FCA);    // 70004098
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000409A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2FC5);    // 7000409C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000409E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2FC6);    // 700040A0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040A2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2ED8);    // 700040A4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040A6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2BD0);    // 700040A8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040AA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x17E0);    // 700040AC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040AE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2DE8);    // 700040B0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040B2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x37E0);    // 700040B4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040B6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x210C);    // 700040B8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040BA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1484);    // 700040BC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040BE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC100);    // 700040C0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 700040C2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA006);    // 700040C4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700040C6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0724);    // 700040C8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040CA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA000);    // 700040CC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 700040CE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2270);    // 700040D0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040D2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2558);    // 700040D4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040D6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x146C);    // 700040D8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 700040DA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB510);    // 700040DC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);    // 700040DE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x499D);    // 700040E0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2204);    // 700040E2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6820);    // 700040E4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5E8A);    // 700040E6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0140);    // 700040E8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A80);    // 700040EA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0280);    // 700040EC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8849);    // 700040EE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700040F0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF9D8);    // 700040F2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6020);    // 700040F4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE7A3);    // 700040F6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB510);    // 700040F8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);    // 700040FA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4996);    // 700040FC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2208);    // 700040FE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6820);    // 70004100
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5E8A);    // 70004102
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0140);    // 70004104
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A80);    // 70004106
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0280);    // 70004108
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88C9);    // 7000410A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 7000410C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF9CA);    // 7000410E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6020);    // 70004110
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE795);    // 70004112
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB5FE);    // 70004114
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);    // 70004116
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6825);    // 70004118
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6866);    // 7000411A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x68A0);    // 7000411C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9001);    // 7000411E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x68E7);    // 70004120
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1BA8);    // 70004122
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x42B5);    // 70004124
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA00);    // 70004126
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1B70);    // 70004128
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9000);    // 7000412A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x498A);    // 7000412C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x488B);    // 7000412E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x884A);    // 70004130
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8843);    // 70004132
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x435A);    // 70004134
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2304);    // 70004136
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5ECB);    // 70004138
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A92);    // 7000413A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18D2);    // 7000413C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D2);    // 7000413E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C12);    // 70004140
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88CB);    // 70004142
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8880);    // 70004144
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4343);    // 70004146
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A98);    // 70004148
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2308);    // 7000414A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5ECB);    // 7000414C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18C0);    // 7000414E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C0);    // 70004150
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70004152
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0411);    // 70004154
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 70004156
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1409);    // 70004158
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1400);    // 7000415A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A08);    // 7000415C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x497F);    // 7000415E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x39E0);    // 70004160
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6148);    // 70004162
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 70004164
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3040);    // 70004166
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7880);    // 70004168
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2800);    // 7000416A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD103);    // 7000416C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 7000416E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 70004170
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70004172
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF99D);    // 70004174
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8839);    // 70004176
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9800);    // 70004178
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4281);    // 7000417A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD814);    // 7000417C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8879);    // 7000417E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9800);    // 70004180
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4281);    // 70004182
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD20C);    // 70004184
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 70004186
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 70004188
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 7000418A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF999);    // 7000418C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 7000418E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 70004190
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70004192
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF995);    // 70004194
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 70004196
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 70004198
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 7000419A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF991);    // 7000419C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE003);    // 7000419E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 700041A0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0029);    // 700041A2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700041A4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF98C);    // 700041A6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9801);    // 700041A8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0032);    // 700041AA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0039);    // 700041AC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700041AE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF98F);    // 700041B0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6020);    // 700041B2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE5BE);    // 700041B4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB57C);    // 700041B6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4869);    // 700041B8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA901);    // 700041BA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);    // 700041BC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700041BE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF913);    // 700041C0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 700041C2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88D9);    // 700041C4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8898);    // 700041C6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4B64);    // 700041C8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3346);    // 700041CA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E9A);    // 700041CC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700041CE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF987);    // 700041D0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4863);    // 700041D2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4961);    // 700041D4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3812);    // 700041D6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3140);    // 700041D8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8A42);    // 700041DA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x888B);    // 700041DC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18D2);    // 700041DE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8242);    // 700041E0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8AC2);    // 700041E2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88C9);    // 700041E4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1851);    // 700041E6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x82C1);    // 700041E8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);    // 700041EA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4669);    // 700041EC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700041EE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF8FB);    // 700041F0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x485C);    // 700041F2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x214D);    // 700041F4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8301);    // 700041F6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2196);    // 700041F8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8381);    // 700041FA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x211D);    // 700041FC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3020);    // 700041FE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8001);    // 70004200
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70004202
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF975);    // 70004204
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70004206
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF97B);    // 70004208
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4857);    // 7000420A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4C57);    // 7000420C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6E00);    // 7000420E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x60E0);    // 70004210
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 70004212
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8818);    // 70004214
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8859);    // 70004216
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0025);    // 70004218
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A40);    // 7000421A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3540);    // 7000421C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x61A8);    // 7000421E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x484E);    // 70004220
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9900);    // 70004222
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3060);    // 70004224
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70004226
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF973);    // 70004228
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x466B);    // 7000422A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8819);    // 7000422C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1DE0);    // 7000422E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x30F9);    // 70004230
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8741);    // 70004232
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8859);    // 70004234
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8781);    // 70004236
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 70004238
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x71A0);    // 7000423A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x74A8);    // 7000423C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC7C);    // 7000423E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xBC08);    // 70004240
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4718);    // 70004242
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB5F8);    // 70004244
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);    // 70004246
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6808);    // 70004248
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);    // 7000424A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 7000424C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x684A);    // 7000424E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0412);    // 70004250
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C12);    // 70004252
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x688E);    // 70004254
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x68CC);    // 70004256
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x493F);    // 70004258
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x884B);    // 7000425A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4343);    // 7000425C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A98);    // 7000425E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2304);    // 70004260
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5ECB);    // 70004262
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18C0);    // 70004264
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C0);    // 70004266
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);    // 70004268
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88CB);    // 7000426A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4353);    // 7000426C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A9A);    // 7000426E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2308);    // 70004270
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5ECB);    // 70004272
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x18D1);    // 70004274
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C9);    // 70004276
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C09);    // 70004278
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2701);    // 7000427A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x003A);    // 7000427C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x40AA);    // 7000427E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9200);    // 70004280
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);    // 70004282
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3A10);    // 70004284
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4097);    // 70004286
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2D10);    // 70004288
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA06);    // 7000428A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A38);    // 7000428C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9B00);    // 7000428E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8812);    // 70004290
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x439A);    // 70004292
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4B36);    // 70004294
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x801A);    // 70004296
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE003);    // 70004298
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4B35);    // 7000429A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x885A);    // 7000429C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43BA);    // 7000429E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x805A);    // 700042A0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0023);    // 700042A2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0032);    // 700042A4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700042A6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF91B);    // 700042A8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2D10);    // 700042AA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA05);    // 700042AC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4930);    // 700042AE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9A00);    // 700042B0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8808);    // 700042B2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4310);    // 700042B4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8008);    // 700042B6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE003);    // 700042B8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x482D);    // 700042BA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8841);    // 700042BC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4339);    // 700042BE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8041);    // 700042C0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4D2A);    // 700042C2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 700042C4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3580);    // 700042C6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88AA);    // 700042C8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5E30);    // 700042CA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2100);    // 700042CC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700042CE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF927);    // 700042D0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8030);    // 700042D2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 700042D4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x88AA);    // 700042D6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5E20);    // 700042D8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2100);    // 700042DA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 700042DC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF920);    // 700042DE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8020);    // 700042E0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE575);    // 700042E2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4823);    // 700042E4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2103);    // 700042E6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x81C1);    // 700042E8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A23);    // 700042EA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2100);    // 700042EC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8011);    // 700042EE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2101);    // 700042F0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8181);    // 700042F2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2102);    // 700042F4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x81C1);    // 700042F6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4770);    // 700042F8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB5F3);    // 700042FA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);    // 700042FC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB081);    // 700042FE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2101);    // 70004300
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000D);    // 70004302
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);    // 70004304
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3810);    // 70004306
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4081);    // 70004308
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x40A5);    // 7000430A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4F18);    // 7000430C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000E);    // 7000430E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C10);    // 70004310
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA03);    // 70004312
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8838);    // 70004314
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43A8);    // 70004316
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8038);    // 70004318
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE002);    // 7000431A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8878);    // 7000431C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x43B0);    // 7000431E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8078);    // 70004320
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF000);    // 70004322
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF905);    // 70004324
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4B15);    // 70004326
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7018);    // 70004328
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C10);    // 7000432A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDA03);    // 7000432C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8838);    // 7000432E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4328);    // 70004330
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8038);    // 70004332
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE002);    // 70004334
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8878);    // 70004336
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4330);    // 70004338
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8078);    // 7000433A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4C10);    // 7000433C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7820);    // 7000433E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2800);    // 70004340
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD005);    // 70004342
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF7FF);    // 70004344
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFCE);    // 70004346
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2000);    // 70004348
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7020);    // 7000434A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x490D);    // 7000434C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7008);    // 7000434E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7818);    // 70004350
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9902);    // 70004352
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7008);    // 70004354
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE4ED);    // 70004356
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2558);    // 70004358
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000435A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2AB8);    // 7000435C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000435E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x145E);    // 70004360
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004362
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2698);    // 70004364
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004366
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2BB8);    // 70004368
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000436A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2998);    // 7000436C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000436E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1100);    // 70004370
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 70004372
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1040);    // 70004374
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 70004376
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9100);    // 70004378
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);    // 7000437A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3044);    // 7000437C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 7000437E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3898);    // 70004380
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004382
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3076);    // 70004384
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);    // 70004386
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004388
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 7000438A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 7000438C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000438E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 70004390
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 70004392
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1789);    // 70004394
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);    // 70004396
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004398
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 7000439A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 7000439C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000439E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700043A0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700043A2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x16F1);    // 700043A4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);    // 700043A6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700043A8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700043AA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700043AC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700043AE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700043B0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700043B2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC3B1);    // 700043B4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700043B6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700043B8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700043BA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700043BC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700043BE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700043C0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700043C2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC36D);    // 700043C4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700043C6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700043C8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700043CA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700043CC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700043CE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700043D0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700043D2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF6D7);    // 700043D4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700043D6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700043D8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700043DA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700043DC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700043DE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700043E0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700043E2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xB49D);    // 700043E4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700043E6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700043E8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700043EA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700043EC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700043EE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700043F0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700043F2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7EDF);    // 700043F4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700043F6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700043F8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700043FA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700043FC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700043FE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 70004400
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 70004402
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x448D);    // 70004404
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004406
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004408
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 7000440A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF004);    // 7000440C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE51F);    // 7000440E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x29EC);    // 70004410
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);    // 70004412
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004414
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004416
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004418
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000441A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000441C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000441E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2EF1);    // 70004420
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004422
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004424
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004426
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004428
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000442A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000442C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000442E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEE03);    // 70004430
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004432
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004434
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004436
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004438
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000443A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000443C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000443E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xA58B);    // 70004440
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004442
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004444
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004446
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004448
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000444A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000444C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000444E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7C49);    // 70004450
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004452
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004454
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004456
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004458
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000445A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000445C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000445E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7C63);    // 70004460
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004462
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004464
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004466
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004468
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000446A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000446C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000446E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2DB7);    // 70004470
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004472
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004474
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004476
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004478
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000447A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000447C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000447E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xEB3D);    // 70004480
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004482
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004484
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004486
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004488
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000448A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000448C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000448E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF061);    // 70004490
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 70004492
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004494
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004496
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004498
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 7000449A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 7000449C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000449E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF0EF);    // 700044A0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700044A2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700044A4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700044A6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF004);    // 700044A8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE51F);    // 700044AA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2824);    // 700044AC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);    // 700044AE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700044B0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700044B2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700044B4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700044B6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700044B8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700044BA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8EDD);    // 700044BC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700044BE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700044C0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700044C2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700044C4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700044C6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700044C8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700044CA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8DCB);    // 700044CC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700044CE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700044D0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700044D2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700044D4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700044D6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700044D8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700044DA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8E17);    // 700044DC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700044DE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700044E0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700044E2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700044E4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700044E6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700044E8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700044EA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x98C5);    // 700044EC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700044EE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 700044F0
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 700044F2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 700044F4
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 700044F6
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 700044F8
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 700044FA
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7C7D);    // 700044FC
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 700044FE
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004500
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004502
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004504
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 70004506
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 70004508
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000450A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7E31);    // 7000450C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 7000450E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004510
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004512
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004514
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 70004516
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 70004518
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000451A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7EAB);    // 7000451C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 7000451E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004520
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004522
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004524
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 70004526
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 70004528
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000452A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7501);    // 7000452C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 7000452E
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4778);    // 70004530
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x46C0);    // 70004532
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC000);    // 70004534
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE59F);    // 70004536
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1C);    // 70004538
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xE12F);    // 7000453A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xCD59);    // 7000453C
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);    // 7000453E
           // End of Patch Data(Last : 7000453Eh)
           // Total Size 2632 (0x0A48)
           // Addr : 3AF8 , Size : 2630(A46h)

           //TNP_USER_MBCV_CONTROL
           //TNP_4EC_MBR_TUNE
           //TNP_4EC_FORBIDDEN_TUNE
           //TNP_AF_FINESEARCH_DRIVEBACK
           //TNP_FLASH_ALG
           //TNP_GAS_ALPHA_OTP
           //TNP_AWB_MODUL_COMP
           //TNP_AWB_INIT_QUEUE
           //TNP_AWB_GRID_LOWBR
           //TNP_AWB_GRID_MODULECOMP
           // TNP_AF_INIT_ON_SEARCH


    S5K4ECGX_write_cmos_sensor(0x0028, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x01FC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x01FE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0204);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0061);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x020C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2F0C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0190);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0294);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00E3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0238);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0166);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0074);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0132);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x070E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00FF);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x071E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x163C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1648);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9002);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1652);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x15E0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0801);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x164C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x163E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00E5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00CC);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x15D4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x169A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF95);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x166A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0280);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1676);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0320);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x16BC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x16E0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x16D4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1656);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x15E6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x003C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0015);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0032);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0038);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x003E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0044);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x004A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0050);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0056);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x005C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0062);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0068);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x006E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0074);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x007A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0086);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x008C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0092);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0098);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x009E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00B0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1722);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3FF0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03E8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0009);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00E0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x028C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08B4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08BC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00DF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0125);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x015F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x017C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0194);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08F6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3B00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4500);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1492);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0102);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0302);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0203);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0102);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0302);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0203);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0102);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0102);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1484);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x003C);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x148A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000F);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x058C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3520);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD4c0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3520);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD4c0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x059C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0470);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0544);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0111);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00EF);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0F2A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x077F);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0F30);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0608);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A3C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0D05);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9C00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xAD00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xF1D4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDC00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xDC00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0638);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A3C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0D05);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3408);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6810);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8214);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xC350);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD4C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xD4c0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0660);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0650);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x06B8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x452C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x05D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x145E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0580);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0428);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x07B0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x11F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0120);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0121);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x101C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x037C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x038E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x033C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02FE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x036C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0352);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x028E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x026A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0254);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02A8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0242);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x021A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0298);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01D4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0290);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0276);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01D2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0260);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x023A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000E);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1074);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0126);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1078);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0272);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x025A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02BC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x023C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02BE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x022E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02BC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0224);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02B6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0218);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02AA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0210);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x020C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0296);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x020A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x028C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0212);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x027E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0234);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0256);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x10AC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x10B0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01D8);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x10B4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0350);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0422);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0452);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0278);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x041C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0230);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03EE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0392);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0340);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0194);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0302);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x016E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02C2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0148);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0286);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x018A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0242);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x10E8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x10EC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0106);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x10F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0380);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0168);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2D90);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1464);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0190);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1228);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x122C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x122A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x120A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x05D5);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x120E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0771);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0036);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x002A);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1278);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEF7);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0021);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0AF0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0AF0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x018F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0096);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000E);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1224);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0032);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x001E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x2BA4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0006);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x146C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1434);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02CE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0347);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03C2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x10A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x10A1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1185);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1186);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x11E5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x11E6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00AB);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0093);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x13A4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFEC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF66);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFEC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF66);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFEC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF66);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFC0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1208);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x144E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0734);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0016);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0066);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0138);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0163);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0189);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0222);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0247);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0282);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02B5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x030F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x035F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0016);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0066);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0138);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0163);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0189);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0222);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0247);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0282);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02B5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x030F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x035F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0016);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0066);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0138);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0163);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0189);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0222);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0247);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0282);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02B5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x030F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x035F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0019);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0036);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x006F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0135);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x015F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0185);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0220);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0291);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x032A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x036A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x039F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03F9);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0019);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0036);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x006F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0135);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x015F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0185);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0220);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0291);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x032A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x036A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x039F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03F9);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0019);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0036);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x006F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0135);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x015F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0185);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0220);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x024A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0291);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x032A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x036A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x039F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03F9);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08A6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0125);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x015F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x017C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0194);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0898);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x08A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x48D8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x4800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0208);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFB5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF20);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF53);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFEA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0095);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEFD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0206);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF7F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0191);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF06);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0108);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0208);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFB5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF20);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF53);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFEA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0095);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEFD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0206);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF7F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0191);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF06);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0108);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0208);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFB5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF20);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF53);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0022);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFEA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0095);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEFD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0206);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF7F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0191);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF06);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BA);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0108);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0203);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFBD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEF1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF18);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFDD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF49);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0151);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF50);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0147);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF75);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0187);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0203);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFBD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEF1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF18);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFDD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF49);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0151);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF50);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0147);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF75);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0187);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0203);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFBD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEF1);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF18);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFE6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFDD);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01B2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00F0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF49);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0151);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF50);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0147);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF75);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0187);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01BF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFA4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFDC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFE90);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x013F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF1B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFD2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFDF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0236);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00EC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00F8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF34);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF83);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0195);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFEF3);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0126);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0162);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0944);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0050);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00B0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0196);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0245);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x097A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01CC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0196);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0976);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x098C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0064);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x005F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1430);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0204);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3604);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x032A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0403);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1B06);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6015);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0640);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x365A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x102A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0600);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0505);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1802);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3028);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0418);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1804);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0540);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0405);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0205);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0304);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0407);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1C04);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0214);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1002);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0610);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A02);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4A18);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0348);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2A36);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2A36);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0808);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x010A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3601);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x242A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3660);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF2A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x08FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0064);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0051);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1430);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0204);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2404);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x031B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0103);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1205);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x400D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0630);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0404);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x245A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0B00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0505);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1802);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3428);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x041C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1004);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0540);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0405);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0205);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0304);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0407);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1F04);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0218);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1102);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0611);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A02);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0380);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1B24);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1D22);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0808);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x010A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2401);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x241B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E60);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF18);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x08FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0064);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0043);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1430);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0204);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1B04);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0312);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C03);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2806);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1580);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0620);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0404);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x145A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0E00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0504);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1802);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3828);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0428);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A04);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0540);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0405);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0207);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0304);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0407);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2404);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0221);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1202);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0613);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A02);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x141D);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C0C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0808);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x010A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1B01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2412);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C60);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF0C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x08FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0064);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0032);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1430);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0204);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1504);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x030F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0902);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2004);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0050);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1140);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x201C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0620);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0404);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x145A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0503);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1802);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3C28);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x042C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0904);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0540);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0405);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0206);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0304);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0305);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0406);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2804);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0228);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1402);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0618);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A02);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1117);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0808);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x010A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1501);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x240F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A60);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x08FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0064);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0032);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01F4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1430);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0201);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0204);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F04);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x030C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0602);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1803);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0E20);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0620);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2003);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0404);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x145A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x5A0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0502);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1802);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4028);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0430);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0804);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x4008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0540);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8006);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0020);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1E10);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0607);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0405);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0205);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0304);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0409);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0306);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0407);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x2C04);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x022C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1402);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0618);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x1A02);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x8018);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0101);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C0F);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0808);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0808);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x010A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0F01);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x240C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0860);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFF08);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x08FF);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0008);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x23CE);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFDC8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x112E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x93A5);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0xFE67);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);

       //20120521 by Caval
    //  For MCLK=40MHz, PCLK=86MHz
    #if 0
    S5K4ECGX_write_cmos_sensor(0x002A, 0x01F8);   //System Setting
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x9C40);   //REG_TC_IPRM_InClockLSBs MCLK: 40Mhz
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0212);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);   //REG_TC_IPRM_UseNPviClocks
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_IPRM_UseNMipiClocks
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_IPRM_NumberOfMipiLanes
    S5K4ECGX_write_cmos_sensor(0x002A, 0x021A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3A98);   //REG_TC_IPRM_OpClk4KHz_0 SCLK: 60Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MinOutRate4KHz_0    PCLK Min : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MaxOutRate4KHz_0    PCLK Max : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x278D);   //REG_TC_IPRM_OpClk4KHz_1 SCLK     : 40.5Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MinOutRate4KHz_1    PCLK Min : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 88Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x53fc);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 88Mhz
    #endif
    //  For MCLK=26MHz, PCLK=91MHz
    #if 1
    S5K4ECGX_write_cmos_sensor(0x002A, 0x01F8);   //System Setting
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x6590);   //REG_TC_IPRM_InClockLSBs MCLK: 26Mhz
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0212);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);   //REG_TC_IPRM_UseNPviClocks
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_IPRM_UseNMipiClocks
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_IPRM_NumberOfMipiLanes
    S5K4ECGX_write_cmos_sensor(0x002A, 0x021A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3A98);   //REG_TC_IPRM_OpClk4KHz_0 SCLK: 60Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MinOutRate4KHz_0    PCLK Min : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MaxOutRate4KHz_0    PCLK Max : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x278D);   //REG_TC_IPRM_OpClk4KHz_1 SCLK     : 40.5Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MinOutRate4KHz_1    PCLK Min : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 91Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 91Mhz
    #endif
    //  For MCLK=30MHz, PCLK=90MHz
    #if 0
    S5K4ECGX_write_cmos_sensor(0x002A, 0x01F8);   //System Setting
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x7530);   //REG_TC_IPRM_InClockLSBs MCLK: 30Mhz
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0212);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);   //REG_TC_IPRM_UseNPviClocks
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_IPRM_UseNMipiClocks
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_IPRM_NumberOfMipiLanes
    S5K4ECGX_write_cmos_sensor(0x002A, 0x021A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x3A98);   //REG_TC_IPRM_OpClk4KHz_0 SCLK: 60Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MinOutRate4KHz_0    PCLK Min : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MaxOutRate4KHz_0    PCLK Max : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x278D);   //REG_TC_IPRM_OpClk4KHz_1 SCLK     : 40.5Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MinOutRate4KHz_1    PCLK Min : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 90Mhz
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x57E4);   //REG_TC_IPRM_MaxOutRate4KHz_1 PCLK Max : 90Mhz
    #endif


    S5K4ECGX_write_cmos_sensor(0x002A, 0x022C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_TC_IPRM_InitParamsUpdated
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0478);   //ETC Setting
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x005F);   //REG_TC_BRC_usPrevQuality
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x005F);   //REG_TC_BRC_usCaptureQuality
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_TC_THUMB_Thumb_bActive
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0280);   //REG_TC_THUMB_Thumb_uWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0);   //REG_TC_THUMB_Thumb_uHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);   //REG_TC_THUMB_Thumb_Format
    S5K4ECGX_write_cmos_sensor(0x002A, 0x17DC);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0054);   //jpeg_ManualMBCV
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1AE4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x001C);   //senHal_bExtraAddLine
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0284);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_TC_GP_bBypassScalerJpg
    S5K4ECGX_write_cmos_sensor(0x002A, 0x028A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_TC_GP_bUse1FrameCaptureMode 0 Continuous mode 1 Single frame mode
       //Preview config[0]
       //91MHz, 1280x960, Dynamic 10~30fps
    S5K4ECGX_write_cmos_sensor(0x002A, 0x02A6);   //Configuration Setting//Normal mode(VGA preview 30~15fps)
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500);   //REG_0TC_PCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03c0);   //REG_0TC_PCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);   //REG_0TC_PCFG_Format 5 YUV   7 Raw   9 JPG
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_0TC_PCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_0TC_PCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);   //REG_0TC_PCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);   //REG_0TC_PCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042);   //REG_0TC_PCFG_PVIMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0);   //REG_0TC_PCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_0TC_PCFG_FrRateQualityType
    //S5K4ECGX_write_cmos_sensor(0x0F12, 0x03E8);   //REG_0TC_PCFG_usMaxFrTimeMsecMult10
    //S5K4ECGX_write_cmos_sensor(0x0F12, 0x014D);   //REG_0TC_PCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01A0);   //REG_0TC_PCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01A0);   //REG_0TC_PCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x002A, 0x02D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uPrevMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uCaptureMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uRotation
       //Preview config[1]
       //91MHz, 1280x960, Dynamic 5~10fps
    S5K4ECGX_write_cmos_sensor(0x002A, 0x02D6);   //Night mode(VGA preview 30~4fps)
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500);   //REG_1TC_PCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03c0);   //REG_1TC_PCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);   //REG_1TC_PCFG_Format 5 YUV   7 Raw   9 JPG
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_1TC_PCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_1TC_PCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);   //REG_1TC_PCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);   //REG_1TC_PCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042);   //REG_1TC_PCFG_PVIMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0);   //REG_1TC_PCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_1TC_PCFG_FrRateQualityType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);   //REG_1TC_PCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03e8);   //REG_1TC_PCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_uPrevMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_uCaptureMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_PCFG_uRotation
       //Preview config[2]
       //91MHz, 1280x960, Fix 30fps
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0306);   //Configuration Setting//Normal mode(VGA preview 30~15fps)
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500);   //REG_0TC_PCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03c0);   //REG_0TC_PCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);   //REG_0TC_PCFG_Format 5 YUV   7 Raw   9 JPG
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_0TC_PCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_0TC_PCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);   //REG_0TC_PCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);   //REG_0TC_PCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042);   //REG_0TC_PCFG_PVIMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0);   //REG_0TC_PCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_0TC_PCFG_FrRateQualityType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014D);   //REG_0TC_PCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014D);   //REG_0TC_PCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x002A, 0x02D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uPrevMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uCaptureMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_PCFG_uRotation

       //Preview config[3]
       //91MHz, 1280x720, fixed 30fps
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0336);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500); //REG_2TC_PCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D0); //REG_2TC_PCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005); //REG_2TC_PCFG_Format
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE); //REG_2TC_PCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE); //REG_2TC_PCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //REG_2TC_PCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300); //REG_2TC_PCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042); //REG_2TC_PCFG_PVIMask       //YUYV
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_2TC_PCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0); //REG_2TC_PCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_2TC_PCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_2TC_PCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_2TC_PCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_2TC_PCFG_FrRateQualityType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014D); //REG_2TC_PCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x014D); //REG_2TC_PCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0330);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //#REG_2TC_PCFG_uPrevMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //#REG_2TC_PCFG_uCaptureMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //#REG_2TC_PCFG_uRotation
       //Preview config[4]
       //91MHz, 1280x720, fixed 10fps(for normal video)
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0366);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500); //REG_3TC_PCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x02D0); //REG_3TC_PCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005); //REG_3TC_PCFG_Format
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE); //REG_3TC_PCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE); //REG_3TC_PCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //REG_3TC_PCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300); //REG_3TC_PCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042); //REG_3TC_PCFG_PVIMask       //YUYV
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_3TC_PCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0); //REG_3TC_PCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_3TC_PCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_3TC_PCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_3TC_PCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_3TC_PCFG_FrRateQualityType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03E8); //REG_3TC_PCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03E8); //REG_3TC_PCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0360);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //#REG_3TC_PCFG_uPrevMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //#REG_3TC_PCFG_uCaptureMirror
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //#REG_3TC_PCFG_uRotation
       //Capture config[0]
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0396);  //Normal mode Capture(7.5fps)
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_CCFG_uCaptureMode
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);   //REG_0TC_CCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);   //REG_0TC_CCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);   //REG_0TC_CCFG_Format
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_0TC_CCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_0TC_CCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);   //REG_0TC_CCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);   //REG_0TC_CCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042);   //REG_0TC_CCFG_PVIMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_CCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03C0);   //REG_0TC_CCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0E80);   //REG_0TC_CCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_CCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_0TC_CCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_0TC_CCFG_FrRateQualityType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0570);   //REG_0TC_CCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500);   //REG_0TC_CCFG_usMinFrTimeMsecMult10
       //Capture config[1]
    S5K4ECGX_write_cmos_sensor(0x002A, 0x03C2);   //Night mode Capture(7.5~4fps)
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_CCFG_uCaptureMode
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);   //REG_1TC_CCFG_usWidth
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);   //REG_1TC_CCFG_usHeight
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);   //REG_1TC_CCFG_Format
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_1TC_CCFG_usMaxOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x58DE);   //REG_1TC_CCFG_usMinOut4KHzRate
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);   //REG_1TC_CCFG_OutClkPerPix88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);   //REG_1TC_CCFG_uBpp88
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0042);   //REG_1TC_CCFG_PVIMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_CCFG_OIFMask
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x03c0);   //REG_1TC_CCFG_usJpegPacketSize
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0E80);   //REG_1TC_CCFG_usJpegTotalPackets
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);   //REG_1TC_CCFG_uClockInd
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);   //REG_1TC_CCFG_usFrTimeType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);   //REG_1TC_CCFG_FrRateQualityType
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0570);   //REG_1TC_CCFG_usMaxFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0500);   //REG_1TC_CCFG_usMinFrTimeMsecMult10
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xd000);
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);                     //AFIT 0
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0250);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0494);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0262);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);


      //unknown
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1CC2);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x01A8);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A0A);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x147C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0170);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1482);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0);

    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0484);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0002);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x183A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x17F6);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x023C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0248);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1840);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0120);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0400);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0800);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0090);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0070);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0045);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1884);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x1826);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0080);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0040);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0048);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0050);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0060);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x4784);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00A0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0088);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00B0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x00C0);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0300);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x479C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0120);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0150);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x003C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x003B);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0026);
       //Select preview 0
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0266);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x026A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x024E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0270);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
}

static void S5K4ECGX_enb_preview(){

        printk("[4EC Parallel] Enable preview...\n");
    S5K4ECGX_write_cmos_sensor(0x002A, 0x023E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
        printk("[4EC Parallel] Enable preview done...\n");
}


/*************************************************************************
* FUNCTION
*    S5K4ECGXGetEvAwbRef
*
* DESCRIPTION
*    This function get sensor Ev/Awb (EV05/EV13) for auto scene detect
*
* PARAMETERS
*    Ref
*
* RETURNS
*    None
*
* LOCAL AFFECTED
*
*************************************************************************/
static void S5K4ECGX_MIPI_GetEvAwbRef(PSENSOR_AE_AWB_REF_STRUCT Ref)  //???
{
    Ref->SensorAERef.AeRefLV05Shutter = 3816; //0xc6c
    Ref->SensorAERef.AeRefLV05Gain = 896; /* 4.1x, 128 base */
    Ref->SensorAERef.AeRefLV13Shutter = 99;   //0x88
    Ref->SensorAERef.AeRefLV13Gain = 1 * 128; /* 2x, 128 base */
    Ref->SensorAwbGainRef.AwbRefD65Rgain = 210; //0xc4/* 1.58x, 128 base */
    Ref->SensorAwbGainRef.AwbRefD65Bgain = 149; //0xa6/* 1.23x, 128 base */
    Ref->SensorAwbGainRef.AwbRefCWFRgain = 179; //0xb9/* 1.4453125x, 128 base */
    Ref->SensorAwbGainRef.AwbRefCWFBgain = 267; //0xf1/* 1.8828125x, 128 base */
}
/*************************************************************************
* FUNCTION
*    S5K4ECGXGetCurAeAwbInfo
*
* DESCRIPTION
*    This function get sensor cur Ae/Awb for auto scene detect
*
* PARAMETERS
*    Info
*
* RETURNS
*    None
*
* LOCAL AFFECTED
*
*************************************************************************/
static void S5K4ECGX_MIPI_GetCurAeAwbInfo(PSENSOR_AE_AWB_CUR_STRUCT Info)
{
    //Info->SensorAECur.AeCurShutter = S5K4ECGX_MIPI_ReadShutter();
    //Info->SensorAECur.AeCurGain = S5K4ECGX_MIPI_ReadGain(); /* 128 base */
    //S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);

    //Info->SensorAwbGainCur.AwbCurRgain = S5K4ECGX_read_cmos_sensor(0x2bd0)/8; //   (sensorGain/1024)*128//
    //Info->SensorAwbGainCur.AwbCurBgain = S5K4ECGX_read_cmos_sensor(0x2bd4)/8; /* 128 base */
}



UINT32 S5K4ECGX_MIPI_GetSensorID(UINT32 *sensorID)
{
   int  retry = 3;

   //SENSORDB("S5K4ECGXGetSensorID+ \n");
   // check if sensor ID correct
   do {
        S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
        S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002E,0x01A4);//id register
        *sensorID = S5K4ECGX_read_cmos_sensor(0x0F12);
        SENSORDB("[4EC]Read Sensor ID = 0x%04x, Currect Sensor ID = 0x%04x\n", *sensorID, S5K4ECGX_SENSOR_ID);

        if (*sensorID == S5K4ECGX_SENSOR_ID)
        {
            SENSORDB("[4EC]ID = 0x%04x\n", *sensorID);
            break;
        }
        SENSORDB("[4EC]Read Sensor ID Fail = 0x%04x, Currect Sensor ID = 0x%04x\n", *sensorID, S5K4ECGX_SENSOR_ID);
        retry--;
   } while (retry > 0);

   if (*sensorID != S5K4ECGX_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF;
        SENSORDB("[4EC] Read Sensor ID Fail = 0x%04x, Currect Sensor ID = 0x%04x\n", *sensorID, S5K4ECGX_SENSOR_ID);
        return ERROR_SENSOR_CONNECT_FAIL;
   }

   //SENSORDB("S5K4ECGXGetSensorID- \n");

   return ERROR_NONE;
} /* S5K4ECGXGetSensorID() */




/*************************************************************************
* FUNCTION
* S5K4ECOpen
*
* DESCRIPTION
* This function initialize the registers of CMOS sensor
*
* PARAMETERS
* None
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/

UINT32 S5K4ECGX_MIPI_Open(void)
{
    kal_uint32 sensor_id=0;

    SENSORDB("[Enter]:S5K4ECGX_MIPI_Open:\r\n");
    S5K4ECGX_MIPI_GetSensorID(&sensor_id);
    if (0xFFFFFFFF == sensor_id)
    {
        SENSORDB("[Camera & Sensor] S5K4EC Read Sensor ID Fail\n");
        return ERROR_SENSOR_CONNECT_FAIL;
    }

#ifdef MIPI_INTERFACE
    S5K4ECGX_MIPI_Init_Setting();
    S5K4ECGX_MIPI_enb_preview();
#else // Parallel
    S5K4ECGX_Init_Setting();
    S5K4ECGX_enb_preview();
#endif
    S5K4ECGX_Driver.shutter = 0x4EA;
    S5K4ECGX_Driver.sensorGain = 0x1f;
    S5K4ECGX_Driver.Dummy_Pixels = 0;
    S5K4ECGX_Driver.Dummy_Lines = 0;


    return ERROR_NONE;
} /* S5K4ECGXOpen() */




/*************************************************************************
* FUNCTION
* S5K4ECGXClose
*
* DESCRIPTION
* This function is to turn off sensor module power.
*
* PARAMETERS
* None
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K4ECGX_MIPI_Close(void)
{

    return ERROR_NONE;
} /* S5K4ECGXClose() */



void S5K4ECGX_MIPI_GetAutoISOValue(void)
{
    // Cal. Method : ((A-Gain*D-Gain)/100h)/2
    // A-Gain , D-Gain Read value is hex value.
    //   ISO 50  : 100(HEX)
    //   ISO 100 : 100 ~ 1FF(HEX)
    //   ISO 200 : 200 ~ 37F(HEX)
    //   ISO 400 : over 380(HEX)
    if (AE_ISO_AUTO != S5K4ECGX_Driver.isoSpeed)
    {
       return;
    }
    unsigned int A_Gain, D_Gain, ISOValue;
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E, 0x2BC4);
    A_Gain = S5K4ECGX_read_cmos_sensor(0x0F12);
    D_Gain = S5K4ECGX_read_cmos_sensor(0x0F12);

    ISOValue = ((A_Gain * D_Gain) >> 9);
    spin_lock(&s5k4ecgx_mipi_drv_lock);
#if 0
    if (ISOValue == 256)
    {
       S5K4ECGX_Driver.isoSpeed = AE_ISO_50;
    }
    else if ((ISOValue >= 257) && (ISOValue <= 511 ))
    {
       S5K4ECGX_Driver.isoSpeed = AE_ISO_100;
    }
#endif
#if 0
    if ((ISOValue >= 200) && (ISOValue < 896 ))
    {
       S5K4ECGX_Driver.isoSpeed = AE_ISO_200;
    }
    else if (ISOValue >= 896)
    {
       S5K4ECGX_Driver.isoSpeed = AE_ISO_400;
    }
    else
    {
       S5K4ECGX_Driver.isoSpeed = AE_ISO_100;
    }
#endif
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    SENSORDB("[4EC] Auto ISO Value = %d \n", ISOValue);
}


void S5K4ECGX_MIPI_GetActiveConfigNum(unsigned int *pActiveConfigNum)
{
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    MSDK_SCENARIO_ID_ENUM scen = S5K4ECGXCurrentScenarioId;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);


    if (MSDK_SCENARIO_ID_VIDEO_PREVIEW == scen)
    {
        *pActiveConfigNum = S5K4EC_VIDEO_MODE;
    }
    else if (MSDK_SCENARIO_ID_CAMERA_PREVIEW == scen)
    {
        *pActiveConfigNum = S5K4EC_PREVIEW_MODE;
    }
    else
    {
        *pActiveConfigNum = S5K4EC_PREVIEW_FULLSIZE_MODE;
    }

    return;
}


void S5K4ECGX_MIPI_SetFrameRate(MSDK_SCENARIO_ID_ENUM scen, UINT16 u2FrameRate)
{

    //spin_lock(&s5k4ecgx_mipi_drv_lock);
    //MSDK_SCENARIO_ID_ENUM scen = S5K4ECGXCurrentScenarioId;
    //spin_unlock(&s5k4ecgx_mipi_drv_lock);
    UINT32 u4frameTime = (1000 * 10) / u2FrameRate;

    if (15 >= u2FrameRate)
    {
       switch (scen)
       {
           default:
           case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            S5K4ECGX_write_cmos_sensor(0x002A, 0x02C2);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0594); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x029A); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
           case MSDK_SCENARIO_ID_VIDEO_PREVIEW: //15fps
            S5K4ECGX_write_cmos_sensor(0x002A ,0x02F2);
            S5K4ECGX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K4ECGX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
           case MSDK_SCENARIO_ID_CAMERA_ZSD:  // independent from scene mode.
            S5K4ECGX_write_cmos_sensor(0x002A ,0x0322);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0594); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0341); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
        }
    }
    else
    {
      switch (scen)
      {
         default:
         case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            S5K4ECGX_write_cmos_sensor(0x002A, 0x02C2);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0535); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x014D); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
         case MSDK_SCENARIO_ID_VIDEO_PREVIEW: //30fps
            S5K4ECGX_write_cmos_sensor(0x002A ,0x02F2);
            S5K4ECGX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K4ECGX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
         case MSDK_SCENARIO_ID_CAMERA_ZSD:
            S5K4ECGX_write_cmos_sensor(0x002A ,0x0322);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0535); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x0341); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
      }
    }


    S5K4ECGX_write_cmos_sensor(0x002A  ,0x026A);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_PrevOpenAfterChange
    S5K4ECGX_write_cmos_sensor(0x002A  ,0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_PrevConfigChanged
    S5K4ECGX_write_cmos_sensor(0x002A  ,0x024E);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_NewConfigSync
    //start preview
    //SENSORDB("[Enter]:S5K4ECGX Preview_Mode_Setting: Start Preview\r\n");
    S5K4ECGX_write_cmos_sensor(0x0028  ,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A  ,0x023E);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_EnablePreview
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_EnablePreviewChanged
    return;
}



static void S5K4ECGX_MIPI_Preview_Mode_Setting(kal_uint8 preview_mode)
{
    unsigned int cap_en = 0;
    unsigned int frameTime;

    if(SEN_RUN_TEST_PATTERN)
    {
        S5K4ECGX_MIPI_SetTestPatternMode(1);
    }
    SENSORDB("\n[4EC] Preview_Mode_Setting+: PVmode=%d\r\n", preview_mode);
    if (preview_mode > S5K4EC_PREVIEW_FULLSIZE_MODE)
    {
        preview_mode = S5K4EC_PREVIEW_MODE;
    }

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    cap_en = s5k4ec_cap_enable;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    if (cap_en)
    {
        //S5K4ECGX_MIPI_AE_UnLock();

        // stop Capture
        //unsigned int frameTime;
#if 0
        S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
        S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002E,0x2C28);
        frameTime = (S5K4ECGX_read_cmos_sensor(0x0F12) & 0xFFFF) ;
        frameTime += (S5K4ECGX_read_cmos_sensor(0x0F12) & 0xFFFF)<<16 ;
        frameTime /= 400; //ms
        //frameTime = 40;
#endif
        frameTime = S5K4ECGX_MIPI_GetExposureTime();
        SENSORDB("[4EC] Preview_Mode_Setting: frameTime=%d ms\n", frameTime);

        S5K4ECGX_write_cmos_sensor(0x0028 ,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002A ,0x023E);
        S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000); //REG_TC_GP_EnablePreview
        S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0001); //REG_TC_GP_EnablePreviewChanged
        S5K4ECGX_write_cmos_sensor(0x002A ,0x0242);
        S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000); //REG_TC_GP_EnableCapture
        S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0001); //REG_TC_GP_EnableCaptureChanged


        spin_lock(&s5k4ecgx_mipi_drv_lock);
        s5k4ec_cap_enable = 0;
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
        //SENSORDB("[4EC] Preview_Mode_Setting: delay %d ms\r\n", frameTime);
        Sleep(frameTime);//(200);
    }

    //FCFCD000
    //SENSORDB("Preview_Mode_Setting: ReconfigPreview Size\r\n");
    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028 ,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A ,0x18AC);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0060); //senHal_uAddColsBin
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0060); //senHal_uAddColsNoBin
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x07DC); //senHal_uMinColsBin
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x05C0); //senHal_uMinColsNoBin

    S5K4ECGX_write_cmos_sensor(0x002A ,0x0250);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0A00); //REG_TC_GP_CapReqInputWidth
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0780); //REG_TC_GP_CapReqInputHeight
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0010); //REG_TC_GP_CapInputWidthOfs
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x000C); //REG_TC_GP_CapInputHeightOfs

    S5K4ECGX_write_cmos_sensor(0x002A ,0x0494);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0A00); //REG_TC_PZOOM_ZoomInputWidth
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0780); //REG_TC_PZOOM_ZoomInputHeight
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000); //REG_TC_PZOOM_ZoomInputWidthOfs
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000); //REG_TC_PZOOM_ZoomInputHeightOfs
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0A00); //REG_TC_CZOOM_ZoomInputWidth
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0780); //REG_TC_CZOOM_ZoomInputHeight
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000); //REG_TC_CZOOM_ZoomInputWidthOfs
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000); //REG_TC_CZOOM_ZoomInputHeightOfs

    S5K4ECGX_write_cmos_sensor(0x002A ,0x0262);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0001); // #REG_TC_GP_bUseReqInputInPre
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0001); //REG_TC_GP_bUseReqInputInCap

    S5K4ECGX_write_cmos_sensor(0x002A ,0x0AD4);
    S5K4ECGX_write_cmos_sensor(0x0F12 ,0x003C); // AfitBaseVals_1__73_ 0060   Why??

    //Select preview
    S5K4ECGX_write_cmos_sensor(0x002A  ,0x0266);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,preview_mode);  //REG_TC_GP_ActivePrevConfig
    S5K4ECGX_write_cmos_sensor(0x002A  ,0x026A);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_PrevOpenAfterChange

    S5K4ECGX_write_cmos_sensor(0x002A  ,0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_PrevConfigChanged

    S5K4ECGX_write_cmos_sensor(0x002A  ,0x024E);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_NewConfigSync


    //start preview
    //SENSORDB("[Enter]:S5K4ECGX Preview_Mode_Setting: Start Preview\r\n");
    S5K4ECGX_write_cmos_sensor(0x0028  ,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A  ,0x023E);
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_EnablePreview
    S5K4ECGX_write_cmos_sensor(0x0F12  ,0x0001);  //REG_TC_GP_EnablePreviewChanged
    return;
}

static void S5K4ECGX_Preview_Mode_Setting(kal_uint8 preview_mode )
{

        SENSORDB("[Enter]:Preview mode: mode=%d\r\n", preview_mode);

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xd000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);

    S5K4ECGX_write_cmos_sensor(0x002A, 0x0250);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0494);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0262);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);

    S5K4ECGX_write_cmos_sensor(0x002A, 0x0266);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000+preview_mode);      //#REG_TC_GP_ActivePrevConfig
    S5K4ECGX_write_cmos_sensor(0x002A, 0x026A);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);       //#REG_TC_GP_PrevOpenAfterChange
    S5K4ECGX_write_cmos_sensor(0x002A, 0x024E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);       //#REG_TC_GP_NewConfigSync
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0268);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);       //#REG_TC_GP_PrevConfigChanged
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0270);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);       //#REG_TC_GP_CapConfigChanged
    S5K4ECGX_write_cmos_sensor(0x002A, 0x023E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);       //#REG_TC_GP_EnablePreview
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);

        SENSORDB("[Exit]:Preview mode:\r\n");

}


UINT32 S5K4ECGX_MIPI_StopPreview(void)
{
  unsigned int status = 1;
  unsigned int prev_en = 1;

    {
      unsigned int frameTime;
      //SENSORDB("[Exit]:S5K4ECGX StopPreview +\r\n");
#if 0
      S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
      S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
      S5K4ECGX_write_cmos_sensor(0x002E,0x2C28);
      frameTime = (S5K4ECGX_read_cmos_sensor(0x0F12) & 0xFFFF) ;
      frameTime += (S5K4ECGX_read_cmos_sensor(0x0F12) & 0xFFFF)<<16 ;
      frameTime /= 400; //ms
      //frameTime = 40;
#endif
      frameTime = S5K4ECGX_MIPI_GetExposureTime();
      SENSORDB("[4EC] StopPreview: frameTime=%d ms\n", frameTime);

      S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
      S5K4ECGX_write_cmos_sensor(0x002A, 0x023E);
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_TC_GP_EnablePreview
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_GP_EnablePreviewChanged
      S5K4ECGX_write_cmos_sensor(0x002A, 0x0242);
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_TC_GP_EnableCapture
      S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_GP_EnableCaptureChanged

      Sleep(frameTime);//(200);
      //S5K4ECGX_Preview_enabled= 0;
      SENSORDB("[4EC]StopPreview- \n");
    }
    return 1;
}



static void S5K4ECGX_MIPI_Capture_Mode_Setting(kal_uint8 capture_mode)
{
    unsigned int status = 0;
    unsigned int frameTime;

    SENSORDB("[4EC] Capture_Mode_Setting+\n");

    S5K4ECGX_MIPI_AF_CancelFocus(); //Lock AF //Debug

    S5K4ECGX_MIPI_StopPreview();

    //<CAMTUNING_INIT>

    //capture
    //static const u32 s5k4ecgx_5M_Capture[] = {
    //SENSORDB("[Exit]:S5K4ECGX Capture_Mode_Setting: Reconfig\r\n");


    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0258);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00); //REG_TC_GP_CapReqInputWidth //2560
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780); //REG_TC_GP_CapReqInputHeight //1920
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0010); //REG_TC_GP_CapInputWidthOfs //(2592-2560)/2
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x000C); //REG_TC_GP_CapInputHeightOfs //(1944-1920)/2
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0264);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_GP_bUseReqInputInCap

    S5K4ECGX_write_cmos_sensor(0x002A, 0x03B4);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0535);   //REG_0TC_CCFG_usMaxFrTimeMsecMult10 0x029A
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0341);   //REG_0TC_CCFG_usMinFrTimeMsecMult10

    S5K4ECGX_write_cmos_sensor(0x002A, 0x049C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0A00); //REG_TC_PZOOM_CapZoomReqInputWidth //2560
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0780); //REG_TC_PZOOM_CapZoomReqInputHeight //1920
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_TC_PZOOM_CapZoomReqInputWidthOfs
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000); //REG_TC_PZOOM_CapZoomReqInputHeightOfs

    //* Kilsung.Hur 20121209 no need for YUV.
    S5K4ECGX_write_cmos_sensor(0x002A, 0x047C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_THUMB_Thumb_bActive
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0280); //REG_TC_THUMB_Thumb_uWidth //640
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x01E0); //REG_TC_THUMB_Thumb_uHeight //480

    S5K4ECGX_MIPI_Config_JPEG_Capture(&S5K4ECGX_Driver.jpegSensorPara);

    //mipi format
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x026e);
    S5K4ECGX_write_cmos_sensor(0x0f12, 0x0000); //REG_TC_GP_ActiveCapConfig

    //SENSORDB("[Exit]:S5K4ECGX Capture_Mode_Setting: Start Preview\r\n");
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0242);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_GP_EnableCapture
    S5K4ECGX_write_cmos_sensor(0x002A, 0x024E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_GP_NewConfigSync
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0244);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //REG_TC_GP_EnableCaptureChanged
    Sleep(5);

    frameTime = S5K4ECGX_MIPI_GetExposureTime();
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.capExposureTime = frameTime;
    s5k4ec_cap_enable = 1;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    S5K4ECGX_MIPI_GetAutoISOValue();

    SENSORDB("[4EC] Capture_Mode_Setting-\n");
}




static void S5K4ECGX_MIPI_HVMirror(kal_uint8 image_mirror)
{
    /********************************************************
    Preview:Mirror: 0x02d0 bit[0],Flip :    0x02d0 bit[1]
    Capture:Mirror: 0x02d2 bit[0],Flip :    0x02d2 bit[1]
    *********************************************************/

    //SENSORDB("[4EC] Mirror+\r\n");

    //if(S5K4ECYX_MIPICurrentStatus.iMirror == image_mirror)
    //    return;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xd000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);


    switch (image_mirror)
    {
        case IMAGE_NORMAL:
        default:
            S5K4ECGX_write_cmos_sensor(0x002A,  0x02D0);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_0TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_0TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0300);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_1TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_1TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0330);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_2TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_2TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0360);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_3TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_3TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0390);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_4TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0000);  //#REG_4TC_PCFG_uCaptureMirror
            break;

        case IMAGE_H_MIRROR:
            S5K4ECGX_write_cmos_sensor(0x002A,  0x02D0);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_0TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_0TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0300);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_1TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_1TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0330);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_2TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_2TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0360);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_3TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_3TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0390);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_4TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0001);  //#REG_4TC_PCFG_uCaptureMirror
            break;

        case IMAGE_V_MIRROR:
            S5K4ECGX_write_cmos_sensor(0x002A,  0x02D0);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_0TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_0TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0300);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_1TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_1TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0330);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_2TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_2TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0360);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_3TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_3TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0390);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_4TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0002);  //#REG_4TC_PCFG_uCaptureMirror
            break;

        case IMAGE_HV_MIRROR:
            S5K4ECGX_write_cmos_sensor(0x002A,  0x02D0);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_0TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_0TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0300);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_1TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_1TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0330);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_2TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_2TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0360);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_3TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_3TC_PCFG_uCaptureMirror

            S5K4ECGX_write_cmos_sensor(0x002A,  0x0390);
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_4TC_PCFG_uPrevMirror
            S5K4ECGX_write_cmos_sensor(0x0F12,  0x0003);  //#REG_4TC_PCFG_uCaptureMirror
            break;
    }



    //spin_lock(&s5k4ecgx_mipi_drv_lock);
    //S5K4ECYX_MIPICurrentStatus.iMirror = image_mirror;
    //spin_unlock(&s5k4ecgx_mipi_drv_lock);
}




void S5K4ECGX_MIPI_SceneOffMode()
{
    MSDK_SCENARIO_ID_ENUM scen;
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    scen = S5K4ECGXCurrentScenarioId;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

   //SENSORDB("[4EC]SceneOffMode+\r\n");

   //S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
   //S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
   //S5K4ECGX_write_cmos_sensor(0x002A, 0x1484);
   //S5K4ECGX_write_cmos_sensor(0x0F12, 0x0030);  //TVAR_ae_BrAve

   //Frame Rate!!
   S5K4ECGX_write_cmos_sensor(0x002A, 0x02C2);
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0535);  //7.5FPS// REG_0TC_PCFG_usMaxFrTimeMsecMult10 //029Ah:15fps
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x014d);  // 30FPS  REG_0TC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps

   S5K4ECGX_write_cmos_sensor(0x002A, 0x0322);
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0535);  //REG_2TC_PCFG_usMaxFrTimeMsecMult10 //0535h:7.5fps
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0341);  // REG_2TC_PCFG_usMinFrTimeMsecMult10 //

   S5K4ECGX_write_cmos_sensor(0x002A, 0x03B4);
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0535);  //REG_0TC_CCFG_usMaxFrTimeMsecMult10 //0535h:7.5fps
   S5K4ECGX_write_cmos_sensor(0x0F12, 0x0341);  // 12FPS // REG_0TC_CCFG_usMinFrTimeMsecMult10 //029Ah:15fps

   if (((AWB_MODE_OFF == S5K4ECYX_MIPICurrentStatus.iWB) ||
        (AWB_MODE_AUTO == S5K4ECYX_MIPICurrentStatus.iWB)))
   {
       S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
       S5K4ECGX_write_cmos_sensor(0x0F12, 0x077F);  //REG_TC_DBG_AutoAlgEnBits
   }

    return;
}




/*************************************************************************
* FUNCTION
* S5K4ECGXPreview
*
* DESCRIPTION
* This function start the sensor preview.
*
* PARAMETERS
*  *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static UINT32 S5K4ECGX_MIPI_Preview(
    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    unsigned int scene = 0;
    unsigned int width, height;
    SENSORDB("[4EC]Preview+ PV_Mode=%d\n", sensor_config_data->SensorOperationMode);

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    width = S5K4ECGX_Driver.Preview_Width;
    height = S5K4ECGX_Driver.Preview_Height;

    scene = S5K4ECGXCurrentScenarioId;
    S5K4ECGX_Driver.Period_PixelNum = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV + S5K4ECGX_Driver.Dummy_Pixels;
    S5K4ECGX_Driver.Period_LineNum = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV + S5K4ECGX_Driver.Dummy_Lines;
    S5K4ECGX_Driver.Preview_Width = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV;
    S5K4ECGX_Driver.Preview_Height = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV;
    S5K4ECGX_Driver.Camco_mode = S5K4ECGX_CAM_PREVIEW;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    if (MSDK_SCENARIO_ID_CAMERA_PREVIEW == scene)
    {
       S5K4ECGX_MIPI_Preview_Mode_Setting(S5K4EC_PREVIEW_MODE);
       if ((width == S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV) ||
           (height == S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV))
       {
           //Reset AE table due to Preview Size Changed
           #if 0
           SENSORDB("[4EC] Reset AE table");
           S5K4ECGX_MIPI_AE_Set_Window(S5K4ECGX_Driver.apAEWindows,
                                       S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV,
                                       S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV);
           #endif
       }
    }
    else
    {
       S5K4ECGX_MIPI_Preview_Mode_Setting(S5K4EC_VIDEO_MODE);
    }

    image_window->GrabStartX = S5K4ECGX_PV_X_START;
    image_window->GrabStartY = S5K4ECGX_PV_Y_START;
    image_window->ExposureWindowWidth = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH - 2 * S5K4ECGX_PV_X_START;
    image_window->ExposureWindowHeight = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT - 2 * S5K4ECGX_PV_Y_START;

    memcpy(&S5K4ECGXSensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    SENSORDB("[4EC]Preview-\n");
    return ERROR_NONE;
} /* S5K4ECGXPreview */

/*************************************************************************
* FUNCTION
*   S5K4ECGXPreview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static UINT32 S5K4ECGXPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

    SENSORDB("[Enter]:S5K4ECGX preview func:");

    S5K4ECGX_MIPI_HVMirror(sensor_config_data->SensorImageMirror);
    S5K4ECGX_Preview_Mode_Setting(0); ////MODE0=10-30FPS

    image_window->GrabStartX = S5K4ECGX_PV_X_START;
    image_window->GrabStartY = S5K4ECGX_PV_Y_START;
    image_window->ExposureWindowWidth = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH-S5K4ECGX_PV_X_START;
    image_window->ExposureWindowHeight = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT-S5K4ECGX_PV_Y_START;

    memcpy(&S5K4ECGXSensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    SENSORDB("[Exit]:S5K4ECGX preview func\n");
    return ERROR_NONE;
}

static UINT32 S5K4ECGX_MIPI_Preview_ZSD(
    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    unsigned int width, height;


    SENSORDB("[4EC]ZSD+");
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    //S5K4ECGX_Driver.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
    width = S5K4ECGX_Driver.Preview_Width;
    height = S5K4ECGX_Driver.Preview_Height;
    S5K4ECGX_Driver.Period_PixelNum = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV + S5K4ECGX_Driver.Dummy_Pixels;
    S5K4ECGX_Driver.Period_LineNum = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV + S5K4ECGX_Driver.Dummy_Lines;
    S5K4ECGX_Driver.Preview_Width = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV;
    S5K4ECGX_Driver.Preview_Height = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV;
    S5K4ECGX_Driver.Camco_mode = S5K4ECGX_CAM_PREVIEW;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    if ((width == S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV) ||
        (height == S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV))
    {
        //Reset AE table due to Preview Size Changed
        S5K4ECGX_MIPI_AE_Set_Window(S5K4ECGX_Driver.apAEWindows,
                                    S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV,
                                    S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV);
    }

    S5K4ECGX_MIPI_Preview_Mode_Setting(S5K4EC_PREVIEW_FULLSIZE_MODE); ////MODE0=10-30FPS

    image_window->GrabStartX = S5K4ECGX_FULL_X_START;
    image_window->GrabStartY = S5K4ECGX_FULL_Y_START;
    image_window->ExposureWindowWidth = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV - 2 * S5K4ECGX_PV_X_START;
    image_window->ExposureWindowHeight = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV - 2 * S5K4ECGX_PV_Y_START;

    memcpy(&S5K4ECGXSensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    SENSORDB("[4EC]ZSD-");
    return ERROR_NONE;
} /* S5K4ECGXPreview */




UINT32 S5K4ECGX_MIPI_Capture(
    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    kal_uint32 shutter = S5K4ECGX_Driver.shutter;
    kal_uint32 gain = 0;

    if (SCENE_MODE_HDR == S5K4ECGX_Driver.sceneMode)
    {
        S5K4ECGX_MIPI_get_exposure_gain();
    }
    //if(S5K4ECGXCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG)
    //{
    //    SENSORDB("[4EC]Capture BusrtShot!!!\n");
    //}

    //Record Preview shutter & gain
    //shutter = S5K4ECGX_MIPI_ReadShutter();
    //gain = S5K4ECGX_MIPI_ReadGain();

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.shutter = shutter;
    S5K4ECGX_Driver.sensorGain = gain;
    S5K4ECGX_Driver.Camco_mode = S5K4ECGX_CAM_CAPTURE;
    S5K4ECGX_Driver.StartX = S5K4ECGX_FULL_X_START;//1; //to be removed?
    S5K4ECGX_Driver.StartY = S5K4ECGX_FULL_Y_START;//1; //to be removed?
    spin_unlock(&s5k4ecgx_mipi_drv_lock);;

    //when entry capture mode,will auto close ae,awb .
    SENSORDB("[4EC]Capture+\r\n");

    S5K4ECGX_MIPI_Capture_Mode_Setting(0);

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.iGrabWidth = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV -2 * S5K4ECGX_FULL_X_START;
    S5K4ECGX_Driver.iGrabheight = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV -2 * S5K4ECGX_FULL_Y_START;
    image_window->GrabStartX = S5K4ECGX_FULL_X_START;
    image_window->GrabStartY = S5K4ECGX_FULL_Y_START;
    image_window->ExposureWindowWidth = S5K4ECGX_Driver.iGrabWidth;
    image_window->ExposureWindowHeight = S5K4ECGX_Driver.iGrabheight;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);;

#if defined(__CAPTURE_JPEG_OUTPUT__)

    if (S5K4ECGX_Driver.manualAEStart)
    {
        //delay 2 frames for frame stable
        //do this in sensor driver in order to cover some limitations
        //Sleep(S5K4ECGX_MIPI_GetExposureTime()*2);
    }

#endif

}


UINT32 S5K4ECGX_MIPI_GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    //SENSORDB("[Enter]:S5K4ECGX get Resolution func\n");

    pSensorResolution->SensorFullWidth=S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV - 2*S5K4ECGX_FULL_X_START;
    pSensorResolution->SensorFullHeight=S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV - 2*S5K4ECGX_FULL_Y_START;
    switch(S5K4ECGXCurrentScenarioId)
    {
       case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorResolution->SensorPreviewWidth   = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV - 2*S5K4ECGX_FULL_X_START;
            pSensorResolution->SensorPreviewHeight  = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV - 2*S5K4ECGX_FULL_Y_START;
            break;
       default:
            pSensorResolution->SensorPreviewWidth   = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV - 2*S5K4ECGX_PV_X_START;
            pSensorResolution->SensorPreviewHeight  = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV - 2*S5K4ECGX_PV_Y_START;
            pSensorResolution->SensorVideoWidth  = pSensorResolution->SensorPreviewWidth;
            pSensorResolution->SensorVideoHeight = pSensorResolution->SensorPreviewHeight;
            break;
    }

    pSensorResolution->Sensor3DFullWidth = 0;
    pSensorResolution->Sensor3DFullHeight= 0;
    pSensorResolution->Sensor3DPreviewWidth = 0;
    pSensorResolution->Sensor3DPreviewHeight = 0;
    //SENSORDB("[Exit]:S5K4ECGX get Resolution func\n");
    return ERROR_NONE;
} /* NSXC301HS5K4ECGXGetResolution() */


UINT32 S5K4ECGX_MIPI_GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
    MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    //SENSORDB("[Enter]:S5K4ECGX getInfo func: ScenarioId = %d\n",ScenarioId);

    switch(ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
          pSensorInfo->SensorPreviewResolutionX = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV;
          pSensorInfo->SensorPreviewResolutionY = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV;
          pSensorInfo->SensorCameraPreviewFrameRate=15;
          break;
        default:
          pSensorInfo->SensorPreviewResolutionX = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV;
          pSensorInfo->SensorPreviewResolutionY = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV;
          pSensorInfo->SensorCameraPreviewFrameRate=30;
          break;
    }

    pSensorInfo->SensorFullResolutionX = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV;
    pSensorInfo->SensorFullResolutionY = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV;
    //pSensorInfo->SensorCameraPreviewFrameRate= 30; //12
    pSensorInfo->SensorVideoFrameRate = 30;
    pSensorInfo->SensorStillCaptureFrameRate = 5;
    pSensorInfo->SensorWebCamCaptureFrameRate = 15;
    pSensorInfo->SensorResetActiveHigh=FALSE;//low is to reset
    pSensorInfo->SensorResetDelayCount=4;  //4ms

    pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_YUYV;
    pSensorInfo->SensorCaptureOutputJPEG = FALSE;
#if defined(__CAPTURE_JPEG_OUTPUT__)
    pSensorInfo->SensorCaptureOutputJPEG = TRUE;
#endif
    pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

#ifdef MIPI_INTERFACE
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
#else
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_HIGH;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
#endif
    pSensorInfo->SensorInterruptDelayLines = 1;

#ifdef MIPI_INTERFACE
      pSensorInfo->SensorInterruptDelayLines = 2;
      pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
#else
      pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_PARALLEL;
#endif

    pSensorInfo->SensorMasterClockSwitch = 0;
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_4MA;
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    pSensorInfo->CaptureDelayFrame = 3; //(0 == S5K4ECGX_Driver.manualAEStart) ? 3 : 2;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);
    pSensorInfo->PreviewDelayFrame = 0;
    pSensorInfo->VideoDelayFrame = 0;

    //Sophie Add for 72
    pSensorInfo->YUVAwbDelayFrame = 5;
    pSensorInfo->YUVEffectDelayFrame = 3;

    //Samuel Add for 72 HDR Ev capture
    pSensorInfo->AEShutDelayFrame = 0;

    //Sophie: Maigh need to remove the assignments?
    //pSensorInfo->SensorDriver3D = SENSOR_3D_NOT_SUPPORT;
    //SENSORDB("[Enter]:2005still frame=%d\r\n", pSensorInfo->SensorStillCaptureFrameRate);

//Sophie Add:
#ifdef MIPI_INTERFACE  //copy from 8AA
    pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
    pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4;
    pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    pSensorInfo->SensorWidthSampling = 0;   // 0 is default 1x
    pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x
    pSensorInfo->SensorPacketECCOrder = 1;
#endif

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        //case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
             pSensorInfo->SensorClockFreq = S5K4ECGX_MCLK;
             pSensorInfo->SensorClockDividCount = 5;
             pSensorInfo->SensorClockRisingCount = 0;
             pSensorInfo->SensorClockFallingCount = 2;
             pSensorInfo->SensorPixelClockCount = 3;
             pSensorInfo->SensorDataLatchCount = 2;
             pSensorInfo->SensorGrabStartX = S5K4ECGX_PV_X_START;
             pSensorInfo->SensorGrabStartY = S5K4ECGX_PV_Y_START;
             break;

        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        //case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
             pSensorInfo->SensorClockFreq = S5K4ECGX_MCLK;
             pSensorInfo->SensorClockDividCount = 5;
             pSensorInfo->SensorClockRisingCount = 0;
             pSensorInfo->SensorClockFallingCount = 2;
             pSensorInfo->SensorPixelClockCount = 3;
             pSensorInfo->SensorDataLatchCount = 2;
             pSensorInfo->SensorGrabStartX = S5K4ECGX_FULL_X_START;
             pSensorInfo->SensorGrabStartY = S5K4ECGX_FULL_Y_START;
             break;

        default:
             pSensorInfo->SensorClockFreq = S5K4ECGX_MCLK;
             pSensorInfo->SensorClockDividCount = 5;
             pSensorInfo->SensorClockRisingCount = 0;
             pSensorInfo->SensorClockFallingCount = 2;
             pSensorInfo->SensorPixelClockCount =3;
             pSensorInfo->SensorDataLatchCount =2;
             pSensorInfo->SensorGrabStartX = S5K4ECGX_PV_X_START;
             pSensorInfo->SensorGrabStartY = S5K4ECGX_PV_Y_START;
             break;
    }

   // SENSORDB("[Exit]:S5K4ECGX getInfo func\n");
    return ERROR_NONE;
}  /* NSXC301HS5K4ECGXGetInfo() */


/*************************************************************************
* FUNCTION
* S5K4ECGX_set_param_effect
*
* DESCRIPTION
* effect setting.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL S5K4ECGX_MIPI_set_param_effect(UINT16 para)
{

   SENSORDB("[4EC] set_param_effect func:para = %d\n",para);
   switch (para)
   {
      case MEFFECT_NEGATIVE:
          S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
          S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
          S5K4ECGX_write_cmos_sensor(0x002A,0x023c);
          S5K4ECGX_write_cmos_sensor(0x0F12,0x0003);  //REG_TC_GP_SpecialEffects
          break;
      case MEFFECT_SEPIA:
          S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
          S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
          S5K4ECGX_write_cmos_sensor(0x002A,0x023c);
          S5K4ECGX_write_cmos_sensor(0x0F12,0x0004);  //REG_TC_GP_SpecialEffects
          break;
      case MEFFECT_SEPIABLUE:
          S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
          S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
          S5K4ECGX_write_cmos_sensor(0x002A,0x023c);
          S5K4ECGX_write_cmos_sensor(0x0F12,0x0007);  //REG_TC_GP_SpecialEffects
          break;
      case MEFFECT_SEPIAGREEN:
          S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
          S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
          S5K4ECGX_write_cmos_sensor(0x002A,0x023c);
          S5K4ECGX_write_cmos_sensor(0x0F12,0x0008);  //REG_TC_GP_SpecialEffects
          break;
      case MEFFECT_MONO:
          S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
          S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
          S5K4ECGX_write_cmos_sensor(0x002A,0x023c);
          S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);  //REG_TC_GP_SpecialEffects
          break;
      default:
    case MEFFECT_OFF:
      S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
      S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
      S5K4ECGX_write_cmos_sensor(0x002A,0x023c);
      S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);  //REG_TC_GP_SpecialEffects
      break;
   }
   return KAL_TRUE;
} /* S5K4ECGX_set_param_effect */





UINT32 S5K4ECGX_MIPI_Control(
    MSDK_SCENARIO_ID_ENUM ScenarioId,
    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
   SENSORDB("[4EC]Control Scenario = 0x%04x\n",ScenarioId);

   spin_lock(&s5k4ecgx_mipi_drv_lock);
   S5K4ECGXCurrentScenarioId = ScenarioId;
   spin_unlock(&s5k4ecgx_mipi_drv_lock);

   switch (ScenarioId)
   {
      case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
      case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
#ifdef MIPI_INTERFACE
         S5K4ECGX_MIPI_Preview(pImageWindow, pSensorConfigData);
#else
         S5K4ECGXPreview(pImageWindow, pSensorConfigData);
#endif
         break;
      case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
         S5K4ECGX_MIPI_Capture(pImageWindow, pSensorConfigData);
         break;
      case MSDK_SCENARIO_ID_CAMERA_ZSD:
     S5K4ECGX_MIPI_Preview_ZSD(pImageWindow, pSensorConfigData);
         break;
      default:
         break;
   }

   //SENSORDB("[Exit]:S5K4ECGX_MIPI_Control  func\n");
   return ERROR_NONE;
} /* S5K4ECGXControl() */

/**
 * Purpose: Set AF to be infinite
 *
 * Note:
 *  [Add] Jessy Lee, 2014/03/18
 */
UINT32 S5K4ECGX_MIPI_AF_Infinity()
{
    SENSORDB("[Enter] S5K4ECGX Set_infinity_af()\n");

    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x028E);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);
    Sleep(100);
    S5K4ECGX_write_cmos_sensor(0x002a, 0x028C);
    S5K4ECGX_write_cmos_sensor(0x0F12, 0x0004);
}

/**
 * Purpose: Get AE status
 *
 * Note:
 *  [Add] Jessy Lee, 2014/03/18
 *  [Modify] Jessy Lee, 2014/04/13 ~ 2014/04/14
 */
static void S5K4ECGX_MIPI_AE_Get_Status(UINT32 *pFeatureReturnPara32)
{
    SENSORDB("[Enter] S5K4ECGX GET_AE_STATUS\n");

    //*pFeatureReturnPara32 = S5K4ECGX_Driver.aeState;
    unsigned int aStatus; // [b:1] 1: AE enable, 0: AE disable; [b:3] 1: AWB enable, 0: AWB disable
    unsigned int aeStable; // 1: stable, 0: unstable
    kal_bool aeEnable;

    /* Detect AE is enable or disable
     * If AE disable -> inactive
     * If AE enable -> active:
     *  (a) If AE lock -> locked;
     *  (b) Else, if AE is stable -> converged;
     *  (c)       if AE is unstable -> searching
     */

    // [TODO] aeEnable needs implement & add get ae on/off state Jessy @2014/04/10

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
    aStatus = S5K4ECGX_read_cmos_sensor(0x0F12); // [b:1] 1: AE enable, 0: AE disable; [b:3] 1: AWB enable, 0: AWB disable
    aeEnable = (aStatus & (1 << 1)) == 0 ? false : true;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x2C74);
    aeStable = S5K4ECGX_read_cmos_sensor(0x0F12); //stable:1 or unstable:0

    // AE disable
    if(false == aeEnable)
    {
        *pFeatureReturnPara32 = SENSOR_AE_IDLE;
    }
    else if(true == S5K4ECGX_Driver.userAskAeLock)
    {
        *pFeatureReturnPara32 = SENSOR_AE_LOCKED;
    }
    else if(1 == aeStable)
    {
        *pFeatureReturnPara32 = SENSOR_AE_EXPOSED;
    }
    else
    {
        *pFeatureReturnPara32 = SENSOR_AE_EXPOSING;
    }

}

/**
 * Purpose: Get AWB status
 *
 * Note:
 *  [Add] Jessy Lee, 2014/03/18
 *  [Modify] Jessy Lee, 2014/04/13 ~ 2014/04/14
 */
static void S5K4ECGX_MIPI_AWB_Get_Status(UINT32 *pFeatureReturnPara32)
{
    SENSORDB("[Enter] S5K4ECGX GET_AWB_STATUS\n");

    //*pFeatureReturnPara32 = S5K4ECGX_Driver.aeState;
    unsigned int aStatus; // [b:1] 1: AE enable, 0: AE disable; [b:3] 1: AWB enable, 0: AWB disable
    unsigned int awbStable; // 1: stable, 0: unstable
    kal_bool awbEnable;

    /* Detect AWB is enable or disable
     * If AWB disable -> inactive
     * If AWB enable -> active:
     *  (a) If AWB lock -> locked;
     *  (b) Else, if AWB is stable -> converged;
     *  (c)       if AWB is unstable -> searching
     */

    // [TODO] aeEnable needs implement & add get ae on/off state Jessy @2014/04/10

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
    aStatus = S5K4ECGX_read_cmos_sensor(0x0F12); // [b:1] 1: AE enable, 0: AE disable; [b:3] 1: AWB enable, 0: AWB disable
    awbEnable = (aStatus & (1 << 3)) == 0 ? false : true;

    S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
    S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002E,0x2C78);
    awbStable = S5K4ECGX_read_cmos_sensor(0x0F12); //stable:1 or unstable:0

    // AWB disable
    if(false == awbEnable)
    {
        *pFeatureReturnPara32 = SENSOR_AWB_IDLE;
    }
    else if(true == S5K4ECGX_Driver.userAskAwbLock)
    {
        *pFeatureReturnPara32 = SENSOR_AWB_LOCKED;
    }
    else if(1 == awbEnable)
    {
        *pFeatureReturnPara32 = SENSOR_AWB_BALANCED;
    }
    else
    {
        *pFeatureReturnPara32 = SENSOR_AWB_BALANCING;
    }

}



/*************************************************************************
* FUNCTION
* S5K4ECGX_set_param_wb
*
* DESCRIPTION
* wb setting.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/

BOOL S5K4ECGX_MIPI_set_param_wb(UINT16 para)
{

    //This sensor need more time to balance AWB,
    //we suggest higher fps or drop some frame to avoid garbage color when preview initial
    //SENSORDB("[Enter]S5K4ECGX set_param_wb func:para = %d\n",para);
    kal_uint16 Status_3A=0;
    kal_int16 iter = 30;

    while ((Status_3A==0) && (iter-- > 0))
    {

        S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
        S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002E,0x04E6);
        Status_3A=S5K4ECGX_read_cmos_sensor(0x0F12); //Index number of active capture configuration //Normal capture//

        Sleep(1);
    }
    SENSORDB("[Enter]S5K4ECGX AWB_MODE\n");

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.awbMode = para;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    //We use AWB_MODE_OFF to lock AWB, and the others to unlock AWB.
    /*
    if (para != AWB_MODE_OFF)
    {
        spin_lock(&s5k4ecgx_mipi_drv_lock);
        S5K4ECGX_Driver.userAskAwbLock = FALSE;
        spin_unlock(&s5k4ecgx_mipi_drv_lock);
        S5K4ECGX_MIPI_AWB_UnLock();
    }
    */


    switch (para)
    {
      case AWB_MODE_AUTO:
      {
           Status_3A = (Status_3A | 0x8); // Enable AWB
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);//
           S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);//
           //S5K4ECGX_write_cmos_sensor(0x0F12, 0x077F);//
           //SENSORDB("[Enter]S5K4ECGX AWB_MODE_AUTO\n");
      }
      break;

      case AWB_MODE_OFF:
      /*
      {
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.userAskAwbLock = TRUE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AWB_Lock();
      }
      */
      break;

      case AWB_MODE_CLOUDY_DAYLIGHT:
      {
           Status_3A = (Status_3A & 0xFFF7); // Disable AWB
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
           S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
           //S5K4ECGX_write_cmos_sensor(0x0F12, 0x0777);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04BA);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x01D0); //Reg_sf_user_Rgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_RgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //0400Reg_sf_user_Ggain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_GgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0140); //0460Reg_sf_user_Bgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_BgainChanged
           //SENSORDB("[Enter]S5K4ECGX AWB_MODE_CLOUDY_DAYLIGHT\n");
      }
      break;
      case AWB_MODE_DAYLIGHT:
      {
           Status_3A = (Status_3A & 0xFFF7); // Disable AWB
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
           S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
           //S5K4ECGX_write_cmos_sensor(0x0F12, 0x0777);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04BA);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0190); //Reg_sf_user_Rgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_RgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //0400Reg_sf_user_Ggain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_GgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0150); //0460Reg_sf_user_Bgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_BgainChanged
           //SENSORDB("[Enter]S5K4ECGX AWB_MODE_DAYLIGHT\n");

      }
      break;
      case AWB_MODE_INCANDESCENT:
      {
           Status_3A = (Status_3A & 0xFFF7); // Disable AWB
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
           S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
           //S5K4ECGX_write_cmos_sensor(0x0F12, 0x0777);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04BA);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0118); //0575Reg_sf_user_Rgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_RgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //0400Reg_sf_user_Ggain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_GgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x026F); //0800Reg_sf_user_Bgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_BgainChanged
           //SENSORDB("[Enter]S5K4ECGX AWB_MODE_INCANDESCENT\n");
      }
      break;
      case AWB_MODE_FLUORESCENT:
      {
           Status_3A = (Status_3A & 0xFFF7); // Disable AWB
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
           S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
           //S5K4ECGX_write_cmos_sensor(0x0F12, 0x0777);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04BA);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0180); //0400Reg_sf_user_Rgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_RgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100); //0400Reg_sf_user_Ggain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_GgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0240); //Reg_sf_user_Bgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_BgainChanged
           //SENSORDB("[Enter]S5K4ECGX AWB_MODE_FLUORESCENT\n");
      }
      break;
      case AWB_MODE_TUNGSTEN:
      {
           Status_3A = (Status_3A & 0xFFF7); // Disable AWB
           S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04E6);
           S5K4ECGX_write_cmos_sensor(0x0F12, Status_3A);
           //S5K4ECGX_write_cmos_sensor(0x0F12, 0x0777);
           S5K4ECGX_write_cmos_sensor(0x002A, 0x04BA);
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0110); //0400Reg_sf_user_Rgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_RgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0120); //0400Reg_sf_user_Ggain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_GgainChanged
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0320); //Reg_sf_user_Bgain
           S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001); //Reg_sf_user_BgainChanged
           //SENSORDB("[Enter]S5K4ECGX AWB_MODE_TUNGSTEN\n");
      }
      break;
      default:
        //SENSORDB("[Enter]:S5K4ECGX AWB_MODE_ default\n");
        break;//return FALSE;
    }


    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECYX_MIPICurrentStatus.iWB = para;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    //SENSORDB("S5K4ECGX Status_3A = 0x%x\n",Status_3A);
    return TRUE;
} /* S5K4ECGX_set_param_wb */


/*************************************************************************
* FUNCTION
* S5K4ECGX_set_param_banding
*
* DESCRIPTION
* banding setting.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL S5K4ECGX_MIPI_set_param_banding(UINT16 para)
{
   //SENSORDB("[Enter]S5K4ECGX set_param_banding func:para = %d\n",para);
   kal_uint16 Status_3A=0;
   kal_int16 iter = 30;

   if(S5K4ECGX_Driver.Banding == para)
       return TRUE;

   spin_lock(&s5k4ecgx_mipi_drv_lock);
   S5K4ECGX_Driver.Banding = para;
   spin_unlock(&s5k4ecgx_mipi_drv_lock);;


   while ((Status_3A==0) && (iter-- > 0))
   {

      S5K4ECGX_write_cmos_sensor(0xFCFC,0xd000);
      S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
      S5K4ECGX_write_cmos_sensor(0x002E,0x04E6); //REG_TC_DBG_AutoAlgEnBits
      Status_3A=S5K4ECGX_read_cmos_sensor(0x0F12);

      Sleep(1);
   }



   switch (para)
   {
       case AE_FLICKER_MODE_50HZ:
       {
          Status_3A = (Status_3A & 0xFFDF); // disable auto-flicker first
          S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
          S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);
          S5K4ECGX_write_cmos_sensor(0x0f12, Status_3A);
          //S5K4ECGX_write_cmos_sensor(0x0f12, 0x075f);
          S5K4ECGX_write_cmos_sensor(0x002a, 0x04d6);
          S5K4ECGX_write_cmos_sensor(0x0f12, 0x0001); //enable 50MHz
          S5K4ECGX_write_cmos_sensor(0x0f12, 0x0001); //update flicker info to FW
          //SENSORDB("[Enter]S5K4ECGX AE_FLICKER_MODE_50HZ\n");
       }
       break;

       case AE_FLICKER_MODE_60HZ:
       {
          Status_3A = (Status_3A & 0xFFDF); // disable auto-flicker
          S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
          S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);
          S5K4ECGX_write_cmos_sensor(0x0f12, Status_3A);
          //S5K4ECGX_write_cmos_sensor(0x0f12, 0x075f);
          S5K4ECGX_write_cmos_sensor(0x002a, 0x04d6);
          S5K4ECGX_write_cmos_sensor(0x0f12, 0x0002); //enable 60MHz
          S5K4ECGX_write_cmos_sensor(0x0f12, 0x0001); //update flicker info to FW
          //SENSORDB("[Enter]S5K4ECGX AE_FLICKER_MODE_60HZ\n");

       }
       break;

       case AE_FLICKER_MODE_OFF:
       {
           Status_3A = (Status_3A & 0xFFDF); // disable auto-flicker first
           S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
           S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);
           S5K4ECGX_write_cmos_sensor(0x0f12, Status_3A);
           //S5K4ECGX_write_cmos_sensor(0x0f12, 0x075f);
           S5K4ECGX_write_cmos_sensor(0x002a, 0x04d6);
           S5K4ECGX_write_cmos_sensor(0x0f12, 0x0000); //disable AFC
           S5K4ECGX_write_cmos_sensor(0x0f12, 0x0001); //update flicker info to FW
           //SENSORDB("[Enter]S5K4ECGX AE_FLICKER_MODE_OFF\n");
       }
       break;

       case AE_FLICKER_MODE_AUTO:
       default:
       {
          //auto flicker
          Status_3A = (Status_3A | 0x5); // Enable auto-flicker
          S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
          S5K4ECGX_write_cmos_sensor(0x002a, 0x04e6);
          S5K4ECGX_write_cmos_sensor(0x0f12, Status_3A);
          //S5K4ECGX_write_cmos_sensor(0x0f12, 0x07FF);
          //SENSORDB("[Enter]S5K4ECGX AE_FLICKER_MODE_AUTO\n");
          break;
       }
   }


   SENSORDB("S5K4ECGXparam_banding Status_3A = 0x%x\n",Status_3A);
   return KAL_TRUE;
} /* S5K4ECGX_set_param_banding */



//This function is optimized version for setting EV value in order to meet
/*************************************************************************
* FUNCTION
* S5K4ECGX_set_param_exposure_for_HDR
*
* DESCRIPTION
* exposure setting.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
#define S5K4ECGX_MIPI_MAX_AXD_GAIN (12 << 8) //max gain = 12
#define S5K4ECGX_MIPI_MAX_EXPOSURE_TIME ( 133 * 100) // max exposure time = 133ms

//This function is optimized version for setting EV value in order to meet
BOOL S5K4ECGX_MIPI_set_param_exposure_for_HDR(UINT16 para)
{
    kal_uint32 totalGain = 0, exposureTime = 0;
    spin_lock(&s5k4ecgx_mipi_drv_lock);

    SENSORDB("[4EC] S5K4ECGX_MIPI_set_param_exposure_for_HDR\n");
    if (0 == S5K4ECGX_Driver.manualAEStart)
    {
        S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
        S5K4ECGX_write_cmos_sensor(0x002A,0x04E6);
        S5K4ECGX_write_cmos_sensor(0x0F12,0x0779); //Manual AE enable
        S5K4ECGX_Driver.manualAEStart = 1;
    }

  switch (para)
  {
     case AE_EV_COMP_20:  //+2 EV
     case AE_EV_COMP_10:  // +1 EV
           totalGain = S5K4ECGX_Driver.currentAxDGain << 1;
           exposureTime = S5K4ECGX_Driver.currentExposureTime << 1;
           SENSORDB("[4EC] HDR AE_EV_COMP_20\n");
           break;
     case AE_EV_COMP_00:  // +0 EV
           totalGain = S5K4ECGX_Driver.currentAxDGain;
           exposureTime = S5K4ECGX_Driver.currentExposureTime;
           SENSORDB("[4EC] HDR AE_EV_COMP_00\n");
           break;
     case AE_EV_COMP_n10:  // -1 EV
     case AE_EV_COMP_n20:  // -2 EV
           totalGain = S5K4ECGX_Driver.currentAxDGain >> 1;
           exposureTime = S5K4ECGX_Driver.currentExposureTime >> 1;
           SENSORDB("[4EC] HDR AE_EV_COMP_n20\n");
           break;
     default:
           break;//return FALSE;
  }
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    totalGain = (totalGain > S5K4ECGX_MIPI_MAX_AXD_GAIN) ? S5K4ECGX_MIPI_MAX_AXD_GAIN : totalGain;
    exposureTime = (exposureTime > S5K4ECGX_MIPI_MAX_EXPOSURE_TIME) ? S5K4ECGX_MIPI_MAX_EXPOSURE_TIME : exposureTime;

    SENSORDB("[4EC] HDR set totalGain=0x%x gain(x%d)\n", totalGain, totalGain>>8);
    SENSORDB("[4EC] HDR set exposureTime=0x%x exposureTime=%dms \n", exposureTime, exposureTime/100);

    S5K4ECGX_write_cmos_sensor(0x002A,0x04AC);
    S5K4ECGX_write_cmos_sensor(0x0F12,exposureTime); //Exposure time
    S5K4ECGX_write_cmos_sensor(0x002A,0x04B0);
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001); //Exposure time update
    S5K4ECGX_write_cmos_sensor(0x002A,0x04B2);
    S5K4ECGX_write_cmos_sensor(0x0F12,totalGain);   //Total gain
    S5K4ECGX_write_cmos_sensor(0x0F12,0x0001);

    return TRUE;
}

/*************************************************************************
* FUNCTION
* S5K4ECGX_set_param_exposure
*
* DESCRIPTION
* exposure setting.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL S5K4ECGX_MIPI_set_param_exposure(UINT16 para)
{
   kal_uint16 base_target = 0;

   SENSORDB("[Enter]S5K4ECGX set_param_exposure func:para = %d\n",para);

   spin_lock(&s5k4ecgx_mipi_drv_lock);
   if (SCENE_MODE_HDR == S5K4ECGX_Driver.sceneMode &&
       S5K4ECGX_CAM_CAPTURE == S5K4ECGX_Driver.Camco_mode)
   {
       spin_unlock(&s5k4ecgx_mipi_drv_lock);
       S5K4ECGX_MIPI_set_param_exposure_for_HDR(para);
       return TRUE;
   }
   spin_unlock(&s5k4ecgx_mipi_drv_lock);

   switch (para)
   {
      case AE_EV_COMP_30:  //+3 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0280);  //TVAR_ae_BrAve
           break;
      case AE_EV_COMP_20:  //+2 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x01E0);  //TVAR_ae_BrAve
           S5K4ECGX_write_cmos_sensor(0x002A,0x098C);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000F);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0A42);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000F);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0AF8);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000F);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0BAE);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000F);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0C64);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000F);  //

           break;
      case AE_EV_COMP_10:  // +1 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0160);  //TVAR_ae_BrAve

           S5K4ECGX_write_cmos_sensor(0x002A,0x098C);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000A);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0A42);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000A);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0AF8);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000A);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0BAE);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000A);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0C64);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x000A);  //

           break;
      case AE_EV_COMP_00:  // +0 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0100);  //TVAR_ae_BrAve

           S5K4ECGX_write_cmos_sensor(0x002A,0x098C);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0A42);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0AF8);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0BAE);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0C64);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);  //

           break;
      case AE_EV_COMP_n10:  // -1 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x00B0);  //TVAR_ae_BrAve

           S5K4ECGX_write_cmos_sensor(0x002A,0x098C);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFFB);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0A42);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFFB);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0AF8);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFFB);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0BAE);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFFB);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0C64);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFFB);  //
           break;
      case AE_EV_COMP_n20:  // -2 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0080);  //TVAR_ae_BrAve

           S5K4ECGX_write_cmos_sensor(0x002A,0x098C);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFF6);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0A42);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFF6);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0AF8);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFF6);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0BAE);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFF6);  //
           S5K4ECGX_write_cmos_sensor(0x002A,0x0C64);  //
           S5K4ECGX_write_cmos_sensor(0x0F12,0xFFF6);  //
           break;
      case AE_EV_COMP_n30:   //-3 EV
           S5K4ECGX_write_cmos_sensor(0xFCFC,0xD000);
           S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
           S5K4ECGX_write_cmos_sensor(0x002A,0x023A);  //UserExposureVal88
           S5K4ECGX_write_cmos_sensor(0x0F12,0x0050);  //TVAR_ae_BrAve
           break;
      default:
           break;//return FALSE;
   }


   return TRUE;

} /* S5K4ECGX_set_param_exposure */


void S5K4ECGX_MIPI_get_AEAWB_lock(UINT32 *pAElockRet32,UINT32 *pAWBlockRet32)
{
    *pAElockRet32 = 1;
    *pAWBlockRet32 = 1;
    SENSORDB("[4EC]GetAEAWBLock,AE=%d ,AWB=%d\n,",*pAElockRet32,*pAWBlockRet32);
}



void S5K4ECGX_MIPI_set_brightness(UINT16 para)
{
    SENSORDB("[4EC]Set_Brightness\n");

    S5K4ECGX_write_cmos_sensor(0xFCFC ,0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A ,0x0230);

    switch (para)
    {
        case ISP_BRIGHT_LOW:
             S5K4ECGX_write_cmos_sensor(0x0F12 ,0xFF81);
             break;
        case ISP_BRIGHT_HIGH:
             S5K4ECGX_write_cmos_sensor(0x0F12 ,0x007F);
             break;
        case ISP_BRIGHT_MIDDLE:
        default:
             S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000);
             break;
    }


    return;
}



void S5K4ECGX_MIPI_set_contrast(UINT16 para)
{
    SENSORDB("[4EC]Set_Contrast\n");

    S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000),
    S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A, 0x0232);  //UserExposureVal88

    switch (para)
    {
        case ISP_CONTRAST_LOW:
             S5K4ECGX_write_cmos_sensor(0x0F12,0xFFC0);
             break;
        case ISP_CONTRAST_HIGH:
             S5K4ECGX_write_cmos_sensor(0x0F12,0x0040);
             break;
        case ISP_CONTRAST_MIDDLE:
        default:
             S5K4ECGX_write_cmos_sensor(0x0F12,0x0000);
             break;
    }

    return;
}



void S5K4ECGX_MIPI_set_iso(UINT16 para)
{

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGX_Driver.isoSpeed = para;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);
    SENSORDB("[4EC]Set_Iso\n");

    switch (para)
    {
        case AE_ISO_100:
             //ISO 100
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D6);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_FlickerQuant
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_FlickerQuantChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x01C0);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);  //lt_bUseSecISODgain
             break;
        case AE_ISO_200:
             //ISO 200
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x03A2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0484);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D6);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_FlickerQuant
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_FlickerQuantChanged

             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0380);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);  //lt_bUseSecISODgain
             break;
        case AE_ISO_400:
             //ISO 400
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x08D2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0C84);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x10D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D6);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_FlickerQuant
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_FlickerQuantChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0700);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0100);  //lt_bUseSecISODgain
             break;
        default:
        case AE_ISO_AUTO:
             //ISO Auto
             S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0938);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //afit_bUseNB_Afit
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0014);  //SARR_uNormBrInDoor_0_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x00D2);  //SARR_uNormBrInDoor_1_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0384);  //SARR_uNormBrInDoor_2_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x07D0);  //SARR_uNormBrInDoor_3_
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x1388);  //SARR_uNormBrInDoor_4_
             S5K4ECGX_write_cmos_sensor(0x002A, 0x0F2A);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //AFC_Default60Hz //  00:50Hz 01:60Hz
             S5K4ECGX_write_cmos_sensor(0x002A, 0x04D0);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_IsoType
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0000);  //REG_SF_USER_IsoVal
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0001);  //REG_SF_USER_IsoChanged
             S5K4ECGX_write_cmos_sensor(0x002A, 0x06C2);
             S5K4ECGX_write_cmos_sensor(0x0F12, 0x0200);  //lt_bUseSecISODgain
             break;
    }
    return;
}


void S5K4ECGX_MIPI_set_saturation(UINT16 para)
{

    SENSORDB("[4EC]Set_saturation\n");


    S5K4ECGX_write_cmos_sensor(0xFCFC ,0xD000);
    S5K4ECGX_write_cmos_sensor(0x0028 ,0x7000);
    S5K4ECGX_write_cmos_sensor(0x002A ,0x0234);
    switch (para)
    {
        case ISP_SAT_HIGH:
             S5K4ECGX_write_cmos_sensor(0x0F12 ,0x007F);  //REG_TC_UserSaturation
             break;
        case ISP_SAT_LOW:
             S5K4ECGX_write_cmos_sensor(0x0F12 ,0xFF81);  //REG_TC_UserSaturation
             break;
        case ISP_SAT_MIDDLE:
        default:
             S5K4ECGX_write_cmos_sensor(0x0F12 ,0x0000);  //REG_TC_UserSaturation
             break;
    }
     return;
}



void S5K4ECGX_MIPI_set_scene_mode(UINT16 para)
{
    unsigned int activeConfigNum = 0;
    unsigned int prevSceneMode;
    unsigned int oscar_iSaturation = 0;

    S5K4ECGX_MIPI_GetActiveConfigNum(&activeConfigNum);
    spin_lock(&s5k4ecgx_mipi_drv_lock);
    prevSceneMode = S5K4ECGX_Driver.sceneMode;
    S5K4ECGX_Driver.sceneMode = para;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);


    //Portrait and Landscape mode will modify the sharpness setting
    //here we will check if we need to rollback related setting
    if (((SCENE_MODE_PORTRAIT == prevSceneMode) &&
         (SCENE_MODE_PORTRAIT != para))   ||
        ((SCENE_MODE_LANDSCAPE == prevSceneMode) &&
         (SCENE_MODE_LANDSCAPE != para)))
    {
        //Rollback Default Sharpness setting
        S5K4ECGX_write_cmos_sensor(0x002A, 0x0A28);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);  //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
        S5K4ECGX_write_cmos_sensor(0x002A, 0x0ADE);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);  //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
        S5K4ECGX_write_cmos_sensor(0x002A, 0x0B94);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);  //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
        S5K4ECGX_write_cmos_sensor(0x002A, 0x0C4A);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);  //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
        S5K4ECGX_write_cmos_sensor(0x002A, 0x0D00);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x6024);  //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
    }


    //Rollback the setting only for night mode...
    if (((SCENE_MODE_NIGHTSCENE == prevSceneMode) &&
         (SCENE_MODE_NIGHTSCENE != para)))
    {
        S5K4ECGX_write_cmos_sensor(0x002A, 0x06B8);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x452C);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0005);  //lt_uMaxLei
        S5K4ECGX_write_cmos_sensor(0x002A, 0x0A1E);
        S5K4ECGX_write_cmos_sensor(0x0F12, 0x0350);  //_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
    }

    //Night mode and Sport mode will modfiy AE weighting table
    //Here we will determine if we nedd to rollback these setting
    //to default or not....
    if (((SCENE_MODE_NIGHTSCENE == prevSceneMode) &&
         (SCENE_MODE_NIGHTSCENE != para)) ||
        ((SCENE_MODE_SPORTS == prevSceneMode) &&
         (SCENE_MODE_SPORTS != para)))
    {
        S5K4ECGX_MIPI_AE_ExpCurveGain_Config(para);
    }
    else if ((SCENE_MODE_NIGHTSCENE == para) ||
             (SCENE_MODE_SPORTS == para))
    {
        S5K4ECGX_MIPI_AE_ExpCurveGain_Config(para);
    }


    if (SCENE_MODE_NIGHTSCENE != para)
    {
        S5K4ECGX_MIPI_SetFrameRate(S5K4ECGXCurrentScenarioId, 30);
    }
    else
    {
        //For preview mode, the fps is dynamic: 7~30 fps.
        //we will not change the FPS on Preview Night mode
        //due to the original range: 7~30fps can fit the night mode requirement
        //S5K4ECGX_MIPI_SetFrameRate(S5K4ECGXCurrentScenarioId, 15);
    }




    switch (para)
    {
        case SCENE_MODE_PORTRAIT:
            SENSORDB("[4EC]SCENE_MODE_PORTRAIT\n");
            // ==========================================================
            //  CAMERA_SCENE_PORTRAIT (Auto/Center/Br0/Auto/Sharp-1/Sat0)
            // ==========================================================
            //enhance sharpness
            S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
            S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0A28);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x2020); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0ADE);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x2020); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0B94);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x2020); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0C4A);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x2020); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0D00);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x2020); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            break;
        case SCENE_MODE_LANDSCAPE:
            SENSORDB("[4EC]SCENE_MODE_LANDSCAPE\n");
            // ==========================================================
            //  CAMERA_SCENE_LANDSCAPE (Auto/Matrix/Br0/Auto/Sharp+1/Sat+1)
            // ==========================================================
            //S5K4ECGX_MIPI_AE_Rollback_Weight_Table();
            S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0A28);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0xE082); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0ADE);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0xE082); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0B94);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0xE082); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0C4A);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0xE082); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0D00);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0xE082); //_ee_iLowSharpPower   [7:0] AFIT8_ee_iHighSharpPower
            break;
        //case SCENE_MODE_NIGHT:
        case SCENE_MODE_NIGHTSCENE:
            SENSORDB("[4EC]SCENE_MODE_NIGHTSCENE\n");
            // ==========================================================
            //  CAMERA_SCENE_NIGHT (Night/Center/Br0/Auto/Sharp0/Sat0)
            // ==========================================================
            {
                S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
                S5K4ECGX_write_cmos_sensor(0x002C,0x7000);
                S5K4ECGX_write_cmos_sensor(0x002E,0x0A1E);
                oscar_iSaturation = S5K4ECGX_read_cmos_sensor(0x0F12);
                SENSORDB("[4EC]SCENE_MODE_NIGHTSCENE: oscar_iSaturation:%x\n", oscar_iSaturation);
            }

            S5K4ECGX_write_cmos_sensor(0xFCFC, 0xD000);
            S5K4ECGX_write_cmos_sensor(0x0028, 0x7000);
            S5K4ECGX_write_cmos_sensor(0x002A, 0x06B8);
            S5K4ECGX_write_cmos_sensor(0x0F12, 0xFFFF); //lt_uMaxLei
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x00FF); //lt_usMinExp
            S5K4ECGX_write_cmos_sensor(0x002A, 0x0A1E);
            //This setting is a tradeoff between color noise and color saturation.
            //S5K4ECGX_write_cmos_sensor(0x0F12, 0x1500); //0x15C0//_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
            S5K4ECGX_write_cmos_sensor(0x0F12, 0x1550); //0x15C0//_ccm_oscar_iSaturation   [7:0] AFIT8_RGB2YUV_iYOffset
            break;

        case SCENE_MODE_SUNSET:
            SENSORDB("[4EC]SCENE_MODE_SUNSET\n");
            // ==========================================================
            //  CAMERA_SCENE_SUNSET (Auto/Center/Br0/Daylight/Sharp0/Sat0)
            // ==========================================================
            break;
        case SCENE_MODE_SPORTS:
            SENSORDB("[4EC]SCENE_MODE_SPORTS\n");
            // ==========================================================
            //  CAMERA_SCENE_SPORTS (Sport/Center/Br0/Auto/Sharp0/Sat0)
            // ==========================================================
            //Fixed AE
            break;

        case SCENE_MODE_HDR:
            SENSORDB("[4EC]SCENE_MODE_HDR\n");
            spin_lock(&s5k4ecgx_mipi_drv_lock);
            if (1 == S5K4ECGX_Driver.manualAEStart)
            {
                S5K4ECGX_write_cmos_sensor(0x0028,0x7000);
                S5K4ECGX_write_cmos_sensor(0x002A,0x04E6);
                S5K4ECGX_write_cmos_sensor(0x0F12,0x077F); //Manual AE disable
                S5K4ECGX_Driver.manualAEStart = 0;
                S5K4ECGX_Driver.currentExposureTime = 0;
                S5K4ECGX_Driver.currentAxDGain = 0;
            }
            spin_unlock(&s5k4ecgx_mipi_drv_lock);
            break;

        case SCENE_MODE_OFF:
            SENSORDB("[4EC]SCENE_MODE_OFF\n");
        default:
            SENSORDB("[4EC]SCENE_MODE default: %d\n", para);
            S5K4ECGX_MIPI_SceneOffMode(); ////MODE0=10-30FPS
            break;
    }
    return;
}




UINT32 S5K4ECGX_MIPI_SensorSetting(FEATURE_ID iCmd, UINT32 iPara)
{
    //SENSORDB("[4EC] SensorSetting\n");
    //return TRUE;

    switch (iCmd)
    {
        case FID_SCENE_MODE:     //auto mode or night mode
            SENSORDB("[4EC] FID_SCENE_MODE\n");
            S5K4ECGX_MIPI_set_scene_mode(iPara);
            break;
        case FID_AWB_MODE:
            SENSORDB("[4EC]FID_AWB_MODE: para=%d\n", iPara);
            S5K4ECGX_MIPI_set_param_wb(iPara);
            break;
        case FID_COLOR_EFFECT:
            SENSORDB("[4EC]FID_COLOR_EFFECT para=%d\n", iPara);
            S5K4ECGX_MIPI_set_param_effect(iPara);
            break;
        case FID_AE_EV:
            SENSORDB("[4EC]FID_AE_EV para=%d\n", iPara);
            S5K4ECGX_MIPI_set_param_exposure(iPara);
            break;
        case FID_AE_FLICKER:
            SENSORDB("[4EC]FID_AE_FLICKER para=%d\n", iPara);
            S5K4ECGX_MIPI_set_param_banding(iPara);
            break;
        case FID_AE_SCENE_MODE:
            SENSORDB("[4EC]FID_AE_SCENE_MODE para=%d\n", iPara);
           /* if (iPara == AE_MODE_OFF)
            {
                spin_lock(&s5k4ecgx_mipi_drv_lock);
                S5K4ECGX_Driver.userAskAeLock = TRUE;
                spin_unlock(&s5k4ecgx_mipi_drv_lock);
               S5K4ECGX_MIPI_AE_Lock();
            }
            else {
                spin_lock(&s5k4ecgx_mipi_drv_lock);
                S5K4ECGX_Driver.userAskAeLock = FALSE;
                spin_unlock(&s5k4ecgx_mipi_drv_lock);
               S5K4ECGX_MIPI_AE_UnLock();
            }*/
            break;
        case FID_ZOOM_FACTOR:
            SENSORDB("[4EC]FID_ZOOM_FACTOR para=%d\n", iPara);
            break;
        case FID_ISP_CONTRAST:
            SENSORDB("[4EC]FID_ISP_CONTRAST:%d\n",iPara);
            S5K4ECGX_MIPI_set_contrast(iPara);
            break;
        case FID_ISP_BRIGHT:
            SENSORDB("[4EC]FID_ISP_BRIGHT:%d\n",iPara);
            S5K4ECGX_MIPI_set_brightness(iPara);
            break;
        case FID_ISP_SAT:
            SENSORDB("[4EC]FID_ISP_SAT:%d\n",iPara);
            S5K4ECGX_MIPI_set_saturation(iPara);
            break;
        case FID_AE_ISO:
            SENSORDB("[4EC]FID_AE_ISO:%d\n",iPara);
            S5K4ECGX_MIPI_set_iso(iPara);
            break;
        default:
            SENSORDB("[4EC]SensorSetting Default, FID=%d\n", iCmd);
            break;
    }
    return TRUE;
}   /* S5K4ECGX_MIPISensorSetting */



void S5K4ECGX_MIPI_SetVideoMode(UINT16 u2FrameRate)
{
    SENSORDB("[4EC] SetVideoMode+ :FrameRate= %d\n",u2FrameRate);

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGXCurrentScenarioId = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
    S5K4ECGX_Driver.videoFrameRate = u2FrameRate;
    S5K4ECGX_Driver.Period_PixelNum = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV + S5K4ECGX_Driver.Dummy_Pixels;
    S5K4ECGX_Driver.Period_LineNum = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV + S5K4ECGX_Driver.Dummy_Lines;
    S5K4ECGX_Driver.Preview_Width = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV;
    S5K4ECGX_Driver.Preview_Height = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    S5K4ECGX_MIPI_SetFrameRate(MSDK_SCENARIO_ID_VIDEO_PREVIEW, u2FrameRate);
    return;
}

void S5K4ECGX_MIPI_SetMaxMinFps(UINT32 u2MinFrameRate, UINT32 u2MaxFrameRate)
{
    SENSORDB("[4EC] S5K4ECGX_MIPI_SetMaxMinFps+ :FrameRate= %d %d\n",u2MinFrameRate,u2MaxFrameRate);

    spin_lock(&s5k4ecgx_mipi_drv_lock);
    S5K4ECGXCurrentScenarioId = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
    S5K4ECGX_Driver.videoFrameRate = u2MaxFrameRate;
    S5K4ECGX_Driver.Period_PixelNum = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV + S5K4ECGX_Driver.Dummy_Pixels;
    S5K4ECGX_Driver.Period_LineNum = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV + S5K4ECGX_Driver.Dummy_Lines;
    S5K4ECGX_Driver.Preview_Width = S5K4ECGX_IMAGE_SENSOR_PV_WIDTH_DRV;
    S5K4ECGX_Driver.Preview_Height = S5K4ECGX_IMAGE_SENSOR_PV_HEIGHT_DRV;
    spin_unlock(&s5k4ecgx_mipi_drv_lock);

    S5K4ECGX_MIPI_SetFrameRate(MSDK_SCENARIO_ID_VIDEO_PREVIEW, u2MaxFrameRate);
    return;
}

void S5K4ECGX_MIPI_GetExifInfo(uintptr_t exifAddr)
{
    SENSOR_EXIF_INFO_STRUCT* pExifInfo = (SENSOR_EXIF_INFO_STRUCT*)exifAddr;
    pExifInfo->FNumber = 28;
    pExifInfo->AEISOSpeed = S5K4ECGX_Driver.isoSpeed;
    pExifInfo->AWBMode = S5K4ECGX_Driver.awbMode;
    pExifInfo->CapExposureTime = S5K4ECGX_Driver.capExposureTime;
    pExifInfo->FlashLightTimeus = 0;
    pExifInfo->RealISOValue = (S5K4ECGX_MIPI_ReadGain()*57) >> 8;
        //S5K4ECGX_Driver.isoSpeed;
}




void S5K4ECGX_MIPI_GetDelayInfo(uintptr_t delayAddr)
{
    SENSOR_DELAY_INFO_STRUCT* pDelayInfo = (SENSOR_DELAY_INFO_STRUCT*)delayAddr;
    pDelayInfo->InitDelay = 3;
    pDelayInfo->EffectDelay = 0;
    pDelayInfo->AwbDelay = 3;
    pDelayInfo->AFSwitchDelayFrame = 50;
}



UINT32 S5K4ECGX_MIPI_SetMaxFramerateByScenario(
  MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
    kal_uint32 pclk;
    kal_int16 dummyLine;
    kal_uint16 lineLength,frameHeight;
#if 0
    SENSORDB("S5K4ECGX_MIPI_SetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
    switch (scenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
             pclk = S5K4ECGX_MIPI_sensor_pclk;
             lineLength = S5K3H7Y_PV_PERIOD_PIXEL_NUMS;
             frameHeight = (10 * pclk)/frameRate/lineLength;
             dummyLine = frameHeight - S5K3H7Y_PV_PERIOD_LINE_NUMS;
             s5k3h7y.sensorMode = SENSOR_MODE_PREVIEW;
             S5K3H7Y_SetDummy(0, dummyLine);
             break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
             pclk = S5K4ECGX_MIPI_sensor_pclk;
             lineLength = S5K3H7Y_PV_PERIOD_PIXEL_NUMS;
             frameHeight = (10 * pclk)/frameRate/lineLength;
             dummyLine = frameHeight - S5K3H7Y_PV_PERIOD_LINE_NUMS;
             s5k3h7y.sensorMode = SENSOR_MODE_PREVIEW;
             S5K3H7Y_SetDummy(0, dummyLine);
             break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
             pclk = S5K4ECGX_MIPI_sensor_pclk;
             lineLength = S5K3H7Y_FULL_PERIOD_PIXEL_NUMS;
             frameHeight = (10 * pclk)/frameRate/lineLength;
             dummyLine = frameHeight - S5K3H7Y_FULL_PERIOD_LINE_NUMS;
             s5k3h7y.sensorMode = SENSOR_MODE_CAPTURE;
             S5K3H7Y_SetDummy(0, dummyLine);
             break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
             break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
             break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added
             break;
        default:
             break;
    }
#endif
  return ERROR_NONE;
}


UINT32 S5K4ECGX_MIPI_GetDefaultFramerateByScenario(
  MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate)
{
    switch (scenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
             *pframeRate = 300;
             break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
             *pframeRate = 300;
             break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added
             *pframeRate = 300;
             break;
        default:
          break;
    }

  return ERROR_NONE;
}



void S5K4ECGX_MIPI_3ACtrl(ACDK_SENSOR_3A_LOCK_ENUM action)
{
   switch (action)
   {
      case SENSOR_3A_AE_LOCK:
          SENSORDB("[4EC] SENSOR_3A_AE_LOCK\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.userAskAeLock = TRUE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AE_Lock();
      break;

      case SENSOR_3A_AE_UNLOCK:
          SENSORDB("[4EC] SENSOR_3A_AE_UNLOCK\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.userAskAeLock = FALSE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AE_UnLock();
      break;

      case SENSOR_3A_AE_ON:
          SENSORDB("[4EC] SENSOR_3A_AE_ON\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.aeEnable= TRUE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AE_On();
      break;

      case SENSOR_3A_AE_OFF:
          SENSORDB("[4EC] SENSOR_3A_AE_OFF\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.aeEnable= FALSE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AE_Off();
      break;

      case SENSOR_3A_AWB_LOCK:
          SENSORDB("[4EC] SENSOR_3A_AWB_LOCK\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.userAskAwbLock = TRUE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AWB_Lock();
      break;

      case SENSOR_3A_AWB_UNLOCK:
          SENSORDB("[4EC] SENSOR_3A_AWB_UNLOCK\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.userAskAwbLock = FALSE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AWB_UnLock();
      break;

      case SENSOR_3A_AWB_ON:
          SENSORDB("[4EC] SENSOR_3A_AWB_ON\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.awbEnable= TRUE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AWB_On();
      break;

      case SENSOR_3A_AWB_OFF:
          SENSORDB("[4EC] SENSOR_3A_AWB_OFF\n");
          spin_lock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_Driver.awbEnable= FALSE;
          spin_unlock(&s5k4ecgx_mipi_drv_lock);
          S5K4ECGX_MIPI_AWB_Off();
      break;

      default:
      break;
   }
   return;
}



UINT32 S5K4ECGX_MIPI_FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
               UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 u2Temp = 0;
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    unsigned long long *pFeatureData=(unsigned long long *) pFeaturePara;
    unsigned long long *pFeatureReturnPara=(unsigned long long *) pFeaturePara;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;
    //SENSORDB("S5K4ECGX_MIPI_FeatureControl+++ ID=%d\n", FeatureId);

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
             SENSORDB("[4EC] F_GET_RESOLUTION\n");
             *pFeatureReturnPara16++ = S5K4ECGX_IMAGE_SENSOR_FULL_WIDTH_DRV;
             *pFeatureReturnPara16 = S5K4ECGX_IMAGE_SENSOR_FULL_HEIGHT_DRV;
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_GET_PERIOD:
             SENSORDB("[4EC] F_GET_PERIOD\n");
             *pFeatureReturnPara16++ = S5K4ECGX_Driver.Period_PixelNum;
             *pFeatureReturnPara16 = S5K4ECGX_Driver.Period_LineNum;
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
             SENSORDB("[4EC] F_PIXEL_CLOCK_FREQ\n");
             *pFeatureReturnPara32 = S5K4ECGX_MIPI_sensor_pclk;
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_SET_NIGHTMODE:
             SENSORDB("[4EC] F_SET_NIGHTMODE\n");
             //S5K4ECGX_MIPI_NightMode((BOOL) *pFeatureData);
             //PhaseOut this ID
             break;



        /**********************Strobe Ctrl Start *******************************/
        case SENSOR_FEATURE_SET_ESHUTTER:
             SENSORDB("[4EC] F_SET_ESHUTTER: Not Support\n");
             S5K4ECGX_MIPI_SetShutter(*pFeatureData);
             break;

        case SENSOR_FEATURE_SET_GAIN:
             SENSORDB("[4EC] F_SET_GAIN: Not Support\n");
             S5K4ECGX_MIPI_SetGain((UINT16)*pFeatureData);
             break;

        case SENSOR_FEATURE_SET_GAIN_AND_ESHUTTER:
            SENSORDB("[4EC] F_SET_GAIN_AND_ESHUTTER\n");
            S5K4ECGX_MIPI_SetGain(*pFeatureData);
            S5K4ECGX_MIPI_SetShutter(*pFeatureData++);
            break;

        case SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO:
             SENSORDB("[4EC] F_GET_AE_FLASHLIGHT_INFO: Not Support\n");
             S5K4ECGX_MIPI_GetAEFlashlightInfo((uintptr_t)*pFeatureData);
             break;

        case SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO:
             S5K4ECGX_MIPI_FlashTriggerCheck(pFeatureReturnPara32);
             SENSORDB("[4EC] F_GET_TRIGGER_FLASHLIGHT_INFO: %d\n", *pFeatureReturnPara32);
             break;

        case SENSOR_FEATURE_SET_FLASHLIGHT:
             SENSORDB("S5K4ECGX SENSOR_FEATURE_SET_FLASHLIGHT\n");
             break;
        /**********************Strobe Ctrl End *******************************/


        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
             SENSORDB("[4EC] F_SET_ISP_MASTER_CLOCK_FREQ\n");
             //spin_lock(&s5k4ecgx_mipi_drv_lock);
             //S5K4ECGX_MIPI_isp_master_clock = *pFeatureData32;
             //spin_unlock(&s5k4ecgx_mipi_drv_lock);
             break;

        case SENSOR_FEATURE_SET_REGISTER:
             SENSORDB("[4EC] F_SET_REGISTER\n");
             S5K4ECGX_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
             break;

        case SENSOR_FEATURE_GET_REGISTER:
             SENSORDB("[4EC] F_GET_REGISTER\n");
             pSensorRegData->RegData = S5K4ECGX_read_cmos_sensor(pSensorRegData->RegAddr);
             break;

        case SENSOR_FEATURE_GET_CONFIG_PARA:
             SENSORDB("[4EC] F_ET_CONFIG_PARA\n");
             memcpy(pSensorConfigData, &S5K4ECGXSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
             *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
             break;

        case SENSOR_FEATURE_SET_CCT_REGISTER: //phase out?
        case SENSOR_FEATURE_GET_CCT_REGISTER: //phase out?
        case SENSOR_FEATURE_SET_ENG_REGISTER: //phase out?
        case SENSOR_FEATURE_GET_ENG_REGISTER: //phase out?
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT: //phase out?
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:  //phase out?
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA: //phase out?
        case SENSOR_FEATURE_GET_GROUP_INFO:
        case SENSOR_FEATURE_GET_ITEM_INFO:
        case SENSOR_FEATURE_SET_ITEM_INFO:
        case SENSOR_FEATURE_GET_ENG_INFO:
             //SENSORDB("[4EC] F_GET_ENG_INFO\n");
             break;

        case SENSOR_FEATURE_GET_GROUP_COUNT:
             *pFeatureReturnPara32++=0;
             *pFeatureParaLen=4;
             //SENSORDB("[4EC] F_GET_GROUP_COUNT\n");
             break;

        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
             //SENSORDB("[4EC] F_GET_LENS_DRIVER_ID\n");
             // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
             // if EEPROM does not exist in camera module.
             *pFeatureReturnPara32= LENS_DRIVER_ID_DO_NOT_CARE;
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_SET_YUV_CMD:
             //SENSORDB("[4EC] F_SET_YUV_CMD ID:%d\n", *pFeatureData32);
             S5K4ECGX_MIPI_SensorSetting((FEATURE_ID)*pFeatureData, *(pFeatureData+1));
             break;

        case SENSOR_FEATURE_SET_YUV_3A_CMD:
             SENSORDB("[4EC] SENSOR_FEATURE_SET_YUV_3A_CMD ID:%d\n", (UINT32)*pFeatureData);
             S5K4ECGX_MIPI_3ACtrl((ACDK_SENSOR_3A_LOCK_ENUM)*pFeatureData);
             break;

        case SENSOR_FEATURE_SET_VIDEO_MODE:
             SENSORDB("[4EC] F_SET_VIDEO_MODE\n");
             S5K4ECGX_MIPI_SetVideoMode(*pFeatureData);
             break;

        case SENSOR_FEATURE_CHECK_SENSOR_ID:
             SENSORDB("[4EC] F_CHECK_SENSOR_ID\n");
             S5K4ECGX_MIPI_GetSensorID(pFeatureReturnPara32);
             break;

        case SENSOR_FEATURE_GET_EV_AWB_REF:
             SENSORDB("[4EC] F_GET_EV_AWB_REF\n");
             S5K4ECGX_MIPI_GetEvAwbRef((uintptr_t)*pFeatureData);
             break;

        case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
             SENSORDB("[4EC] F_GET_SHUTTER_GAIN_AWB_GAIN\n");
             S5K4ECGX_MIPI_GetCurAeAwbInfo((uintptr_t)*pFeatureData);
             break;

  #ifdef MIPI_INTERFACE
        case SENSOR_FEATURE_GET_EXIF_INFO:
             //SENSORDB("[4EC] F_GET_EXIF_INFO\n");
             S5K4ECGX_MIPI_GetExifInfo((uintptr_t)*pFeatureData);
             break;
  #endif

        case SENSOR_FEATURE_GET_DELAY_INFO:
             SENSORDB("[4EC] F_GET_DELAY_INFO\n");
             S5K4ECGX_MIPI_GetDelayInfo((uintptr_t)*pFeatureData);
             break;

        case SENSOR_FEATURE_SET_SLAVE_I2C_ID:
             //SENSORDB("[4EC] F_SET_SLAVE_I2C_ID:[%d]\n",*pFeatureData32);
             S5K4ECGX_MIPI_sensor_socket = *pFeatureData;
             break;

        case SENSOR_FEATURE_SET_TEST_PATTERN:
             SENSORDB("[4EC] F_SET_TEST_PATTERN\n");
             S5K4ECGX_MIPI_SetTestPatternMode((BOOL)*pFeatureData);
             break;

        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
             SENSORDB("[4EC] F_SET_MAX_FRAME_RATE_BY_SCENARIO: FAIL: NOT Support\n");
             S5K4ECGX_MIPI_SetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData, *(pFeatureData+1));
             break;

        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
             SENSORDB("[4EC] F_GET_DEFAULT_FRAME_RATE_BY_SCENARIO\n");
             S5K4ECGX_MIPI_GetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData, (MUINT32 *)(uintptr_t)(*(pFeatureData+1)));
             break;

        //below is AF control
        case SENSOR_FEATURE_INITIALIZE_AF:
             SENSORDB("[4EC] F_INITIALIZE_AF\n");
             S5K4ECGX_MIPI_AF_Init();
             break;

        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
             SENSORDB("[4EC] F_MOVE_FOCUS_LENS\n");
             S5K4ECGX_MIPI_AF_Move_to(*pFeatureData16); // not implement yet.
             break;

        case SENSOR_FEATURE_GET_AF_STATUS:
            S5K4ECGX_MIPI_AF_Get_Status(pFeatureReturnPara32);
            //SENSORDB("[4EC] F_GET_AF_STATUS=%d\n", *pFeatureReturnPara32);
            *pFeatureParaLen = 4;
             break;

        case SENSOR_FEATURE_SINGLE_FOCUS_MODE:
             SENSORDB("[4EC] F_SINGLE_FOCUS_MODE\n");
             S5K4ECGX_MIPI_AF_Start(S5K4ECGX_AF_MODE_SINGLE);
             break;

        case SENSOR_FEATURE_CONSTANT_AF:
             SENSORDB("[4EC] F_CONSTANT_AF\n");
             S5K4ECGX_MIPI_AF_Start(S5K4ECGX_AF_MODE_CONTINUOUS);
             break;

        case SENSOR_FEATURE_CANCEL_AF:
             SENSORDB("[4EC] F_CANCEL_AF\n");
             S5K4ECGX_MIPI_AF_CancelFocus();
             break;

        case SENSOR_FEATURE_INFINITY_AF:
            SENSORDB("[4EC] F_INFINITY_AF\n");
            S5K4ECGX_MIPI_AF_Infinity();
            break;

        case SENSOR_FEATURE_GET_AE_STATUS:
            SENSORDB("[4EC] F_GET_AE_STATUS\n");
            S5K4ECGX_MIPI_AE_Get_Status(pFeatureReturnPara32);
            *pFeatureParaLen=4;
            break;

        case SENSOR_FEATURE_GET_AWB_STATUS:
            SENSORDB("[4EC] F_GET_AWB_STATUS\n");
            S5K4ECGX_MIPI_AWB_Get_Status(pFeatureReturnPara32);
            *pFeatureParaLen=4;
            break;

        case SENSOR_FEATURE_GET_AF_INF:
             SENSORDB("[4EC] F_GET_AF_INF\n");
             S5K4ECGX_MIPI_AF_Get_Inf(pFeatureReturnPara32);
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_GET_AF_MACRO:
             S5K4ECGX_MIPI_AF_Get_Macro(pFeatureReturnPara32);
             SENSORDB("[4EC] F_GET_AF_MACRO=%d\n", *pFeatureReturnPara32);
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
             S5K4ECGX_MIPI_AF_Get_Max_Num_Focus_Areas(pFeatureReturnPara32);
             SENSORDB("[4EC] F_GET_AF_MAX_NUM_FOCUS_AREAS=%d\n", *pFeatureReturnPara32);
             *pFeatureParaLen=4;
             break;

        case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
             S5K4ECGX_MIPI_AE_Get_Max_Num_Metering_Areas(pFeatureReturnPara32);
             SENSORDB("[4EC] F_GET_AE_MAX_NUM_METERING_AREAS=%d\n", *pFeatureReturnPara32);
             *pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_SET_AF_WINDOW:
             SENSORDB("[4EC] F_SET_AF_WINDOW\n");
             S5K4ECGX_MIPI_AF_Set_Window((uintptr_t)*pFeatureData, S5K4ECGX_Driver.Preview_Width, S5K4ECGX_Driver.Preview_Height);
             break;

        case SENSOR_FEATURE_SET_AE_WINDOW:
             SENSORDB("[4EC] F_SET_AE_WINDOW\n");
             S5K4ECGX_MIPI_AE_Set_Window((uintptr_t)*pFeatureData, S5K4ECGX_Driver.Preview_Width, S5K4ECGX_Driver.Preview_Height);
             break;

        case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
             SENSORDB("[4EC] F_GET_AE_AWB_LOCK_INFO\n");
             S5K4ECGX_MIPI_get_AEAWB_lock((uintptr_t)(*pFeatureData),(uintptr_t)(*(pFeatureData+1)));
             break;
         case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing
             *pFeatureReturnPara32= S5K4ECGX_TEST_PATTERN_CHECKSUM;
             *pFeatureParaLen=4;
             break;

#if defined(__CAPTURE_JPEG_OUTPUT__)
        /*case SENSOR_FEATURE_GET_YUV_CAPTURE_OUTPUT_JPEG:
             *pFeatureReturnPara32 = TRUE;
             *pFeatureParaLen=4;
              break;
        */
        case SENSOR_FEATURE_GET_YUV_JPEG_INFO:
            SENSORDB("[4EC]GET_YUV_JPEG_INFO");
            {
               UINT32*                para =  (UINT32*)pFeaturePara;
               UINT8                 *jpegFileAddr = para[0];
               //UINT32                 maxBufSize =  para[1];
               ACDK_SENSOR_JPEG_INFO *jpegInfo = (ACDK_SENSOR_JPEG_INFO*) para[1];

               UINT32                 maxBufSize =  2560 * 1920;

               SENSORDB("[4EC]GET_YUV_JPEG_INFO: jpegFileAddr=0x%p, maxBufSize=%d, infoAddr=0x%p\n", jpegFileAddr, maxBufSize, jpegInfo);
               S5K4ECGX_MIPI_JPEG_Capture_Parser(jpegFileAddr, maxBufSize, &S5K4ECGX_Driver.jpegSensorInfo);
               memcpy(jpegInfo, &S5K4ECGX_Driver.jpegSensorInfo, sizeof(ACDK_SENSOR_JPEG_INFO));
               *pFeatureParaLen = sizeof(ACDK_SENSOR_JPEG_INFO);
            }
             break;

        case SENSOR_FEATURE_SET_YUV_JPEG_PARA:
            SENSORDB("[4EC]SET_JPEG_PARA \n");
            {
                UINT32* para =  (UINT32*)pFeaturePara;
                ACDK_SENSOR_JPEG_OUTPUT_PARA *jpegPara = (ACDK_SENSOR_JPEG_OUTPUT_PARA*) para[0];
                SENSORDB("[4EC]width=%d, hegith=%d, Quality=%d\n", jpegPara->tgtWidth, jpegPara->tgtHeight,jpegPara->quality);
                if ((jpegPara->tgtHeight == 0) || (jpegPara->tgtWidth == 0) || (jpegPara->quality == 0) || (jpegPara->quality > 100))
                {
                    SENSORDB("[4EC]SET_JPEG_PARA: Invalid Para!\n");
                    return ERROR_INVALID_PARA;
                }
                memcpy(&S5K4ECGX_Driver.jpegSensorPara, jpegPara, sizeof(ACDK_SENSOR_JPEG_OUTPUT_PARA));
                SENSORDB("[4EC]SET_JPEG_PARA S5K4ECGX_Driver.jpegSensorPara width=%d, hegith=%d, Quality=%d\n",
                       S5K4ECGX_Driver.jpegSensorPara.tgtWidth, S5K4ECGX_Driver.jpegSensorPara.tgtHeight,S5K4ECGX_Driver.jpegSensorPara.quality);
            }
             break;
#endif
        case SENSOR_FEATURE_SET_MIN_MAX_FPS:
             SENSORDB("SENSOR_FEATURE_SET_MIN_MAX_FPS:[%d,%d]\n",(UINT32)*pFeatureData,(UINT32)*(pFeatureData+1));
             S5K4ECGX_MIPI_SetMaxMinFps((UINT32)*pFeatureData, (UINT32)*(pFeatureData+1));

             break;

        default:
             SENSORDB("[4EC]FeatureControl default\n");
             break;
    }

    //SENSORDB("S5K4ECGX_MIPI_FeatureControl---\n");
    return ERROR_NONE;
} /* S5K4ECGXFeatureControl() */


SENSOR_FUNCTION_STRUCT  SensorFuncS5K4ECGX_MIPI=
{
    S5K4ECGX_MIPI_Open,             // get sensor id, set initial setting to sesnor
    S5K4ECGX_MIPI_GetInfo,          // get sensor capbility,
    S5K4ECGX_MIPI_GetResolution,    // get sensor capure/preview resolution
    S5K4ECGX_MIPI_FeatureControl,   // set shutter/gain, set/read register
    S5K4ECGX_MIPI_Control,          // change mode to preview/capture/video
    S5K4ECGX_MIPI_Close             // close, do nothing currently
};

UINT32 S5K4ECGX_MIPI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
  /* To Do : Check Sensor status here */
  if (pfFunc!=NULL)
     *pfFunc=&SensorFuncS5K4ECGX_MIPI;

  return ERROR_NONE;
} /* SensorInit() */

