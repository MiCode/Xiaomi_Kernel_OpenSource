/*
 * MD218A voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "BU64745GWZAF.h"
#include "../camera/kd_camera_hw.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

// in K2, main=3, sub=main2=1
#define LENS_I2C_BUSNUM 0

#define AF_DRVNAME "BU64745GWZAF"
#define I2C_SLAVE_ADDRESS        0xE8
#define I2C_REGISTER_ID            0x22
#define PLATFORM_DRIVER_NAME "lens_actuator_bu64745gwzaf"
#define AF_DRIVER_CLASS_NAME "actuatordrv_bu64745gwzaf"

static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO(AF_DRVNAME, I2C_REGISTER_ID)};

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO,    AF_DRVNAME, "[%s] " format, __FUNCTION__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static spinlock_t g_AF_SpinLock;

static struct i2c_client * g_pstAF_I2Cclient = NULL;

static dev_t g_AF_devno;
static struct cdev * g_pAF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int    g_s4AF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4AF_INF = 0;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition    = 0;
static unsigned long g_u4InitPosition   = 100;

static int g_sr = 3;


static void    WR_I2C( CL_UBYTE slvadr, CL_UBYTE size, CL_UBYTE *dat )
{

}
// *********************************************************
// Read Data from Slave device via I2C master device
// ---------------------------------------------------------
// <Function>
//        I2C master read data from the I2C slave device.
//        This function relate to your own circuit.
//
// <Input>
//        CL_UBYTE    slvadr    I2C slave adr
//        CL_UBYTE    size    Transfer Size
//        CL_UBYTE    *dat    data matrix
//
// <Output>
//        CL_UWORD    16bit data read from I2C Slave device
//
// <Description>
//    if size == 1
//        [S][SlaveAdr][W]+[dat[0]]+         [RS][SlaveAdr][R]+[RD_DAT0]+[RD_DAT1][P]
//    if size == 2
//        [S][SlaveAdr][W]+[dat[0]]+[dat[1]]+[RS][SlaveAdr][R]+[RD_DAT0]+[RD_DAT1][P]
//
// *********************************************************
static CL_UWORD    RD_I2C( CL_UBYTE slvadr, CL_UBYTE size, CL_UBYTE *dat )
{
    CL_UWORD    read_data = 0;

    /* Please write your source code here. */

    /* This is for ROHM's microprocessor
    {
        // Sample Code

        CL_UBYTE ret[2];
        SInEx(slvadr, dat, size, ret, 2);

        read_data = ret[0] * 0x100 + ret[1];
        printf("call RD_I2C (0x%02X, 0x%04X)\n", dat[0], read_data );
    }
     */
    return read_data;
}


//  *****************************************************
//  **** Write to the Memory register < 84h >
//  **** ------------------------------------------------
//  **** CL_UBYTE    adr    Memory Address
//  **** CL_UWORD    dat    Write data
//  *****************************************************
static void    I2C_write_FBAF( CL_UBYTE u08_adr, CL_UWORD u16_dat)
{
    int  i4RetValue = 0;
    char puSendCmd[6] = {(char)(u08_adr&0xFF),
                         (char)((u16_dat >>  8)&0xFF),
                         (char)( u16_dat       &0xFF)};
    LOG_INF("I2C w4 (%x %x) \n",u08_adr,u16_dat);

    spin_lock(&g_AF_SpinLock);
    //g_pstAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstAF_I2Cclient->ext_flag = (g_pstAF_I2Cclient->ext_flag)&(~I2C_DMA_FLAG);
    g_pstAF_I2Cclient->addr = (I2C_SLAVE_ADDRESS >> 1);
        spin_unlock(&g_AF_SpinLock);
    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
    if (i4RetValue < 0)
    {
        LOG_INF("I2C send failed!! \n");
        return;
    }
}

//  *****************************************************
//  **** Read from the Peripheral register < 82h >
//  **** ------------------------------------------------
//  **** CL_UBYTE    adr    Peripheral Address
//  **** CL_UWORD    dat    Read data
//  *****************************************************
static CL_UWORD    I2C_read__FBAF( CL_UBYTE u08_adr )
{
        int  i4RetValue = 0;
    char pBuff[1] = {(char)(u08_adr & 0xFF)};
    CL_UWORD vRcvBuff=0;
        spin_lock(&g_AF_SpinLock);
    g_pstAF_I2Cclient->addr = (I2C_SLAVE_ADDRESS >> 1);
         g_pstAF_I2Cclient->ext_flag = (g_pstAF_I2Cclient->ext_flag)&(~I2C_DMA_FLAG);
        spin_unlock(&g_AF_SpinLock);

    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, pBuff, 1);
    if (i4RetValue < 0 )
    {
        LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
        return i4RetValue;
    }

    i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (u8*)&vRcvBuff, 2);
    if (i4RetValue != 2)
    {
        LOG_INF("[CAMERA SENSOR] I2C read failed!! \n");
        return i4RetValue;
    }
    //vRcvBuff=    ((vRcvBuff&0xFF) <<8) + ((vRcvBuff>> 8)&0xFF) ;
      vRcvBuff=((vRcvBuff<<8)&0xff00)|((vRcvBuff>>8)&0x00ff);

    LOG_INF("I2C r2 (%x %x) \n",u08_adr,vRcvBuff);

    return vRcvBuff;

}


//  *****************************************************
// Driver Control Function
//  *****************************************************
 void    I2C_func_PON______( ){
    I2C_write_FBAF( 0xFF, 0x8000 );
}

 void    I2C_func_POFF_____( ){
    I2C_write_FBAF( 0xFF, 0x0000 );
}

 void    I2C_func_DSP_START( ){
    I2C_write_FBAF( 0x7E, 0x0000 );
}

 void    I2C_func_CEF_WRITE( CL_UBYTE u08_adr, CL_UWORD u16_dat){
    I2C_write_FBAF( u08_adr | 0x80, u16_dat );
}

static void    EQ_CONTROL( CL_UBYTE u08_mode ){
    I2C_func_CEF_WRITE( 0x30, u08_mode );
}

//  *****************************************************
// Non Volatile Memory Control Function
//  *****************************************************
static void    I2C_func_NVL_ENABLE( void ){
    I2C_write_FBAF( 0x60, 0x0100 );
}

static CL_UWORD    I2C_func_NVL__READ( CL_UBYTE u08_adr ){

    CL_UWORD    u16_dat;

              I2C_write_FBAF( u08_adr, 0x4000 );
    u16_dat = I2C_read__FBAF( u08_adr );
//     LogPut(string.format(" %2Xh\n",BitAnd(u16_dat,0xFF)))

    return u16_dat;
}

static void _CHG_H_GAIN( CL_UWORD u16_gainsel ){
    CL_UWORD s16_temp_dat;

    s16_temp_dat = I2C_read__FBAF( 0x24 );
    s16_temp_dat &= 0x0FFF;
    s16_temp_dat |= u16_gainsel;
    I2C_write_FBAF( 0x24, s16_temp_dat );

}

//  *****************************************************
//  DSP Coefficient Parameter
//  *****************************************************
static void    func_INITIALIZE( void ){

    //  *****************************************************
    //  **** Initialize Peripheral
    //  *****************************************************
//    I2C_write_FBAF(     0x00, 0x0775 )    // CLOCK
//    I2C_write_FBAF(     0x02, 0x0010 )    // INTFRQ
    I2C_write_FBAF(     0x37, 0x0008 );    // Analog Block Power ON

    I2C_write_FBAF(     0x25, 0x0003 );    // ADC Averaging ON
    I2C_write_FBAF(     0x26, 0x0100 );    // Ch0 Hall Current DAC 1.6mA
    I2C_write_FBAF(     0x27, 0x0200 );    // Ch1 Hall PreAmp  OFFSET DAC
    I2C_write_FBAF(     0x28, 0x0200 );    // Ch2 Hall PostAmp OFFSET DAC
    I2C_write_FBAF(     0x29, 0x0200 );    // Ch3 Current Amp  OFFSET DAC

    I2C_write_FBAF(     0x24, 0x40F0 );    // ADC Enable & Hall Gain
    I2C_write_FBAF(     0x22, 0xC2F8 );    // BTL Modulation Setting
    I2C_write_FBAF(     0x23, 0x0311 );    // BTL Driver Control

    //  *****************************************************
    //  **** Initialize COEFFICIENT
    //  *****************************************************
     I2C_func_CEF_WRITE( 0x27, 0x0000 );    // M_wGAS
    // INPUT
     I2C_func_CEF_WRITE( 0x20, 0x0000 );    // M_HOFS
    I2C_func_CEF_WRITE( 0x25, 0xE000 );    // M_KgLPG
    I2C_func_CEF_WRITE( 0x21, 0xC000 );    // M_KgHG

    I2C_func_CEF_WRITE( 0x2D, 0x8800 );    // Kg2D
    I2C_func_CEF_WRITE( 0x00, 0x7FFF );    // Kg00    // HIGH 2nd
    I2C_func_CEF_WRITE( 0x01, 0x5000 );    // Kg01
    I2C_func_CEF_WRITE( 0x02, 0x4000 );    // Kg02
    I2C_func_CEF_WRITE( 0x03, 0x4000 );    // Kg03
    I2C_func_CEF_WRITE( 0x08, 0x0000 );    // Kg08    // LOW 1st
    I2C_func_CEF_WRITE( 0x09, 0x6000 );    // Kg09
    I2C_func_CEF_WRITE( 0x0A, 0x4000 );    // Kg0A
    I2C_func_CEF_WRITE( 0x0B, 0x4000 );    // Kg0B
    I2C_func_CEF_WRITE( 0x10, 0x0000 );    // Kg10    // LOW 2nd
    I2C_func_CEF_WRITE( 0x11, 0x7FFE );    // Kg11
    I2C_func_CEF_WRITE( 0x12, 0x4000 );    // Kg12
    I2C_func_CEF_WRITE( 0x13, 0xD000 );    // Kg13

    // Step Profile LPF
    I2C_func_CEF_WRITE( 0x38, 0x1000 );    // M_PRFCEF

     I2C_func_CEF_WRITE( 0x27, 0x0000 );    // Delay FF
     I2C_func_CEF_WRITE( 0x2B, 0x0000 );    // Delay FF
     I2C_func_CEF_WRITE( 0x04, 0x0000 );    // Delay FF
     I2C_func_CEF_WRITE( 0x0C, 0x0000 );    // Delay FF
     I2C_func_CEF_WRITE( 0x14, 0x0000 );    // Delay FF
     I2C_func_CEF_WRITE( 0x1D, 0x0000 );    // Delay FF
     I2C_func_CEF_WRITE( 0x1E, 0x0000 );    // Delay FF
    I2C_func_CEF_WRITE( 0x2A, 0x0000 );    // Delay FF

    I2C_func_CEF_WRITE( 0x18, 0x7FFF );    // Kg18
     I2C_func_CEF_WRITE( 0x19, 0x0000 );    // Kg19
     I2C_func_CEF_WRITE( 0x1A, 0x0000 );    // Kg1A
     I2C_func_CEF_WRITE( 0x1B, 0x0000 );    // Kg1B
     I2C_func_CEF_WRITE( 0x1C, 0x0000 );    // Kg1C

    // Current Limmiter
    I2C_func_CEF_WRITE( 0x0E, 0x1000 );    // EXCEL); C_LMT 90mA

    // Current FeedBack
     I2C_func_CEF_WRITE( 0x16, 0x0000 );    //        CUROFS
    I2C_func_CEF_WRITE( 0x17, 0x36E2 );    // EXCEL); Kf0A    Current Compensation 100mA Full Scale
    I2C_func_CEF_WRITE( 0x29, 0x4000 );    //        Kf0C    Integral Gain

     I2C_func_CEF_WRITE( 0x31, 0x0000 );    //        M_PRFCNT     8bit  b15:8
    I2C_func_CEF_WRITE( 0x32, 0x0800 );    //        M_PRFNUM     8bit  b15:8 KMT

//     // Triangle
     I2C_func_CEF_WRITE( 0x06, 0x0000 );    // TRILMT
     I2C_func_CEF_WRITE( 0x07, 0x0000 );    // KgWv

    // Contorl
//    I2C_func_CEF_WRITE( 0x30, 0x0010 );    // FBAF control 5bit b4:0 Current Loop Off
    I2C_func_CEF_WRITE( 0x34, 0x0002 );    // PRE    SHIFT 4bit b3:0
    I2C_func_CEF_WRITE( 0x35, 0x0002 );    // Post   SHIFT 4bit b3:0
    I2C_func_CEF_WRITE( 0x37, 0x0008 );    // OUTPUT SHIFT 4bit b3:0
    I2C_func_CEF_WRITE( 0x36, 0x0002 );    // fs control   4bit b3:0
}


// ///////////////////////////////////////////////////
// Adjustment Data read
// ///////////////////////////////////////////////////
static CL_BOOL        func_ReLOAD_FACT_DATA( void ){
    CL_UBYTE    dat_H, dat_L;

    I2C_func_NVL_ENABLE( );

    dat_L = I2C_func_NVL__READ( 0x40 );    // TABLE_FORMAT

    if    ( ( dat_L == 0xFF ) || ( dat_L == 0x00 ) ){
        return -1;
    }

    dat_L      = I2C_func_NVL__READ( 0x41 );    // gl_AMPGAIN
    gl_AMPGAIN = dat_L << 8;
    _CHG_H_GAIN( gl_AMPGAIN );                    // Analog Amp Gain

    dat_L      = I2C_func_NVL__READ( 0x42 );    // gl_PREOFST
    gl_PREOFST = dat_L << 2;
    I2C_write_FBAF( 0x27, gl_PREOFST );            // Pre Amp offset

    dat_L = I2C_func_NVL__READ( 0x43 );            // gl_POSTOFS
    gl_POSTOFS = dat_L << 2;
    I2C_write_FBAF( 0x28, gl_POSTOFS);            // Pre Amp offset
                                                // gl_HOFS
    dat_H = I2C_func_NVL__READ( 0x44 );            // HOFS_H
    dat_L = I2C_func_NVL__READ( 0x45 );            // HOFS__L
    gl_HOFS = ( dat_H << 8 ) | dat_L;
    I2C_func_CEF_WRITE( 0x20, gl_HOFS );        // Digital Offset
                                                // gl_KgHG
    dat_H = I2C_func_NVL__READ( 0x46 );            // KgHG_H
    dat_L = I2C_func_NVL__READ( 0x47 );            // KgHG__L
    gl_KgHG = ( dat_H << 8 ) | dat_L;
    I2C_func_CEF_WRITE( 0x21, gl_KgHG   );        // Digital Amplifier
                                                // gl_KgLPG
    dat_H = I2C_func_NVL__READ( 0x48 );            // KgLPG_H
    dat_L = I2C_func_NVL__READ( 0x49 );            // KgLPG__L
    gl_KgLPG = ( dat_H << 8 ) | dat_L;
    I2C_func_CEF_WRITE( 0x25, gl_KgLPG  );        // Digital Amplifier
                                                // gl_Kf0A
    dat_H = I2C_func_NVL__READ( 0x4A );            // Kf0A_H
    dat_L = I2C_func_NVL__READ( 0x4B );            // Kf0A__L
    gl_Kf0A = ( dat_H << 8 ) | dat_L;
    I2C_func_CEF_WRITE( 0x17, gl_Kf0A );        // Digital Amplifier
                                                // gl_CUROFS
    dat_H = I2C_func_NVL__READ( 0x4C );            // CUROFS_H
    dat_L = I2C_func_NVL__READ( 0x4D );            // CUROFS__L
    gl_CUROFS = ( dat_H << 8 ) | dat_L;
    I2C_func_CEF_WRITE( 0x16, gl_CUROFS );

    dat_H = I2C_func_NVL__READ( 0x4E );            // PARITY_1
    dat_L = I2C_func_NVL__READ( 0x4F );            // PARITY_2

    return 0;
}


static int s4AF_ReadReg(unsigned short * a_pu2Result)
{

    return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
     return 0;
}
inline static int getAFInfo(__user stBU64745GWZAF_MotorInfo * pstMotorInfo)
{
    stBU64745GWZAF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4AF_MACRO;
    stMotorInfo.u4InfPosition      = g_u4AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

    if (g_i4MotorStatus == 1)    {stMotorInfo.bIsMotorMoving = 1;}
    else                        {stMotorInfo.bIsMotorMoving = 0;}

    if (g_s4AF_Opened >= 1)    {stMotorInfo.bIsMotorOpen = 1;}
    else                        {stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stBU64745GWZAF_MotorInfo)))
    {
        LOG_INF("copy to user failed when getting motor information \n");
    }

    return 0;
}
void init_drv(void)
{
    I2C_func_POFF_____( );                    // reset the IC
    I2C_func_PON______( );                    // Power ON

    func_INITIALIZE( );                        // initialize the IC
    func_ReLOAD_FACT_DATA( );                // ReLoad Adjustment Data
    I2C_func_DSP_START( );                    // CLAF Function Enable
    EQ_CONTROL( 0x0004 );
}


inline static int moveAF(unsigned long a_u4Position)
{
    if((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF))
    {
        LOG_INF("out of range \n");
        return -EINVAL;
    }

    if (g_s4AF_Opened == 1)
    {
        init_drv();
        spin_lock(&g_AF_SpinLock);
        g_u4CurrPosition=g_u4InitPosition;
        g_s4AF_Opened = 2;
        spin_unlock(&g_AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_AF_SpinLock);
        g_i4Dir = 1;
        spin_unlock(&g_AF_SpinLock);
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_AF_SpinLock);
        g_i4Dir = -1;
        spin_unlock(&g_AF_SpinLock);
    }
    else                                        {return 0;}

    spin_lock(&g_AF_SpinLock);
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_AF_SpinLock);

    LOG_INF("move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_AF_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
             g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
              spin_unlock(&g_AF_SpinLock);
           I2C_write_FBAF( 0x11, g_u4TargetPosition );

    return 0;
}

inline static int setAFInf(unsigned long a_u4Position)
{
    spin_lock(&g_AF_SpinLock);
    g_u4AF_INF = a_u4Position;
    spin_unlock(&g_AF_SpinLock);
    return 0;
}

inline static int setAFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_AF_SpinLock);
    g_u4AF_MACRO = a_u4Position;
    spin_unlock(&g_AF_SpinLock);
    return 0;
}

////////////////////////////////////////////////////////////////
static long AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case BU64745GWZAFIOC_G_MOTORINFO :
            i4RetValue = getAFInfo((__user stBU64745GWZAF_MotorInfo *)(a_u4Param));
        break;

        case BU64745GWZAFIOC_T_MOVETO :
            i4RetValue = moveAF(a_u4Param);
        break;

        case BU64745GWZAFIOC_T_SETINFPOS :
            i4RetValue = setAFInf(a_u4Param);
        break;

        case BU64745GWZAFIOC_T_SETMACROPOS :
            i4RetValue = setAFMacro(a_u4Param);
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
static int AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    LOG_INF("Start \n");


    if(g_s4AF_Opened)
    {
        LOG_INF("The device is opened \n");
        return -EBUSY;
    }

    spin_lock(&g_AF_SpinLock);
    g_s4AF_Opened = 1;
    spin_unlock(&g_AF_SpinLock);

    LOG_INF("End \n");
    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    LOG_INF("Start \n");

    if (g_s4AF_Opened)
    {
        LOG_INF("Free \n");
        g_sr = 5;
        s4AF_WriteReg(200);
        msleep(10);
        s4AF_WriteReg(100);
        msleep(10);

        spin_lock(&g_AF_SpinLock);
        g_s4AF_Opened = 0;
         g_sr = 5;
        spin_unlock(&g_AF_SpinLock);
    }

    LOG_INF("End \n");

    return 0;
}

static const struct file_operations g_stAF_fops =
{
    .owner = THIS_MODULE,
    .open = AF_Open,
    .release = AF_Release,
    .unlocked_ioctl = AF_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = AF_Ioctl,
#endif
};

inline static int Register_AF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    LOG_INF("Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_AF_devno, 0, 1,AF_DRVNAME) )
    {
        LOG_INF("Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pAF_CharDrv = cdev_alloc();

    if(NULL == g_pAF_CharDrv)
    {
        unregister_chrdev_region(g_AF_devno, 1);

        LOG_INF("Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pAF_CharDrv, &g_stAF_fops);

    g_pAF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pAF_CharDrv, g_AF_devno, 1))
    {
        LOG_INF("Attatch file operation failed\n");

        unregister_chrdev_region(g_AF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, AF_DRIVER_CLASS_NAME);
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        LOG_INF("Unable to create class, err = %d\n", ret);
        return ret;
    }

    vcm_device = device_create(actuator_class, NULL, g_AF_devno, NULL, AF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }

    LOG_INF("End\n");
    return 0;
}

inline static void Unregister_AF_CharDrv(void)
{
    LOG_INF("Start\n");

    //Release char driver
    cdev_del(g_pAF_CharDrv);

    unregister_chrdev_region(g_AF_devno, 1);

    device_destroy(actuator_class, g_AF_devno);

    class_destroy(actuator_class);

    LOG_INF("End\n");
}

//////////////////////////////////////////////////////////////////////

static int AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id AF_i2c_id[] = {{AF_DRVNAME, 0},{}};
static struct i2c_driver AF_i2c_driver = {
    .probe = AF_i2c_probe,
    .remove = AF_i2c_remove,
    .driver.name = AF_DRVNAME,
    .id_table = AF_i2c_id,
};

#if 0
static int AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, AF_DRVNAME);
    return 0;
}
#endif
static int AF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    LOG_INF("Start\n");

    /* Kirby: add new-style driver { */
    g_pstAF_I2Cclient = client;

    g_pstAF_I2Cclient->addr = I2C_SLAVE_ADDRESS;

    g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

    //Register char driver
    i4RetValue = Register_AF_CharDrv();

    if(i4RetValue){

        LOG_INF(" register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_AF_SpinLock);

    LOG_INF("Attached!! \n");

    return 0;
}

static int AF_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&AF_i2c_driver);
}

static int AF_remove(struct platform_device *pdev)
{
    i2c_del_driver(&AF_i2c_driver);
    return 0;
}

static int AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int AF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stAF_Driver = {
    .probe        = AF_probe,
    .remove    = AF_remove,
    .suspend    = AF_suspend,
    .resume    = AF_resume,
    .driver        = {
        .name    = PLATFORM_DRIVER_NAME,
        .owner    = THIS_MODULE,
    }
};
static struct platform_device g_stAF_device = {
    .name = PLATFORM_DRIVER_NAME,
    .id = 0,
    .dev = {}
};

static int __init BU64745GWZAF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);

    if(platform_device_register(&g_stAF_device)){
        LOG_INF("failed to register AF driver\n");
        return -ENODEV;
    }

    if(platform_driver_register(&g_stAF_Driver)){
        LOG_INF("Failed to register AF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit BU64745GWZAF_i2C_exit(void)
{
    platform_driver_unregister(&g_stAF_Driver);
}

module_init(BU64745GWZAF_i2C_init);
module_exit(BU64745GWZAF_i2C_exit);

MODULE_DESCRIPTION("BU64745GWZAF lens module driver");
MODULE_AUTHOR("KY Chen <vend_james-cc.wu@Mediatek.com>");
MODULE_LICENSE("GPL");


