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
#include "LC898122AF.h"
#include "../camera/kd_camera_hw.h"
#include "Ois.h"
#include "OisDef.h"
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

// in K2, main=3, sub=main2=1
#define LENS_I2C_BUSNUM 0
static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("LC898122AF", 0x48)};


#define LC898122AF_DRVNAME "LC898122AF"
#define LC898122AF_VCM_WRITE_ID           0x48

#define LC898122AF_DEBUG
#ifdef LC898122AF_DEBUG
#define LC898122AFDB printk
#else
#define LC898122AFDB(x,...)
#endif

static spinlock_t g_LC898122AF_SpinLock;

static struct i2c_client * g_pstLC898122AF_I2Cclient = NULL;

static dev_t g_LC898122AF_devno;
static struct cdev * g_pLC898122AF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4LC898122AF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4LC898122AF_INF = 0;
static unsigned long g_u4LC898122AF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;
static unsigned long g_u4InitPosition   = 100;

static int g_sr = 3;


void RegWriteA(unsigned short RegAddr, unsigned char RegData)
{
    int  i4RetValue = 0;
    char puSendCmd[3] = {(char)((RegAddr>>8)&0xFF),(char)(RegAddr&0xFF),RegData};
    LC898122AFDB("[LC898122AF]I2C w (%x %x) \n",RegAddr,RegData);

    g_pstLC898122AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898122AF_I2Cclient->addr = (LC898122AF_VCM_WRITE_ID >> 1);
    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, puSendCmd, 3);
    if (i4RetValue < 0) 
    {
        LC898122AFDB("[LC898122AF]I2C send failed!! \n");
        return;
    }
}
void RegReadA(unsigned short RegAddr, unsigned char *RegData)
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RegAddr >> 8) , (char)(RegAddr & 0xFF)};

    g_pstLC898122AF_I2Cclient->addr = (LC898122AF_VCM_WRITE_ID >> 1);

    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        LC898122AFDB("[CAMERA SENSOR] read I2C send failed!!\n");
        return;
    }

    i4RetValue = i2c_master_recv(g_pstLC898122AF_I2Cclient, (u8*)RegData, 1);

    LC898122AFDB("[LC898122AF]I2C r (%x %x) \n",RegAddr,*RegData);
    if (i4RetValue != 1) 
    {
        LC898122AFDB("[CAMERA SENSOR] I2C read failed!! \n");
        return;
    }
}
void RamWriteA( unsigned short RamAddr, unsigned short RamData )

{
    int  i4RetValue = 0;
    char puSendCmd[4] = {(char)((RamAddr >>  8)&0xFF), 
                         (char)( RamAddr       &0xFF),
                         (char)((RamData >>  8)&0xFF), 
                         (char)( RamData       &0xFF)};
    LC898122AFDB("[LC898122AF]I2C w2 (%x %x) \n",RamAddr,RamData);

    g_pstLC898122AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898122AF_I2Cclient->addr = (LC898122AF_VCM_WRITE_ID >> 1);
    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, puSendCmd, 4);
    if (i4RetValue < 0) 
    {
        LC898122AFDB("[LC898122AF]I2C send failed!! \n");
        return;
    }
}
void RamReadA( unsigned short RamAddr, void * ReadData )
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RamAddr >> 8) , (char)(RamAddr & 0xFF)};
    unsigned short  vRcvBuff=0;
	unsigned long *pRcvBuff;
    pRcvBuff =(unsigned long *)ReadData;

    g_pstLC898122AF_I2Cclient->addr = (LC898122AF_VCM_WRITE_ID >> 1);

    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        LC898122AFDB("[CAMERA SENSOR] read I2C send failed!!\n");
        return;
    }

    i4RetValue = i2c_master_recv(g_pstLC898122AF_I2Cclient, (u8*)&vRcvBuff, 2);
    if (i4RetValue != 2) 
    {
        LC898122AFDB("[CAMERA SENSOR] I2C read failed!! \n");
        return;
    }
    *pRcvBuff=    ((vRcvBuff&0xFF) <<8) + ((vRcvBuff>> 8)&0xFF) ;
    
    LC898122AFDB("[LC898122AF]I2C r2 (%x %x) \n",RamAddr,(unsigned int)*pRcvBuff);

}
void RamWrite32A(unsigned short RamAddr, unsigned long RamData )
{
    int  i4RetValue = 0;
    char puSendCmd[6] = {(char)((RamAddr >>  8)&0xFF), 
                         (char)( RamAddr       &0xFF),
                         (char)((RamData >> 24)&0xFF), 
                         (char)((RamData >> 16)&0xFF), 
                         (char)((RamData >>  8)&0xFF), 
                         (char)( RamData       &0xFF)};
    LC898122AFDB("[LC898122AF]I2C w4 (%x %x) \n",RamAddr,(unsigned int)RamData);

    
    g_pstLC898122AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898122AF_I2Cclient->addr = (LC898122AF_VCM_WRITE_ID >> 1);
    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, puSendCmd, 6);
    if (i4RetValue < 0) 
    {
        LC898122AFDB("[LC898122AF]I2C send failed!! \n");
        return;
    }
}
void RamRead32A(unsigned short RamAddr, void * ReadData )
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RamAddr >> 8) , (char)(RamAddr & 0xFF)};
    unsigned long *pRcvBuff, vRcvBuff=0;
    pRcvBuff =(unsigned long *)ReadData;

    g_pstLC898122AF_I2Cclient->addr = (LC898122AF_VCM_WRITE_ID >> 1);

    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        LC898122AFDB("[CAMERA SENSOR] read I2C send failed!!\n");
        return;
    }

    i4RetValue = i2c_master_recv(g_pstLC898122AF_I2Cclient, (u8*)&vRcvBuff, 4);
    if (i4RetValue != 4) 
    {
        LC898122AFDB("[CAMERA SENSOR] I2C read failed!! \n");
        return;
    }
    *pRcvBuff=   ((vRcvBuff     &0xFF) <<24) 
               +(((vRcvBuff>> 8)&0xFF) <<16) 
               +(((vRcvBuff>>16)&0xFF) << 8) 
               +(((vRcvBuff>>24)&0xFF)     );

        LC898122AFDB("[LC898122AF]I2C r4 (%x %x) \n",RamAddr,(unsigned int)*pRcvBuff);
}
void WitTim(unsigned short  UsWitTim )
{
    msleep(UsWitTim);
}
void LC898prtvalue(unsigned short  prtvalue )
{
    LC898122AFDB("[LC898122AF]printvalue ======%x   \n",prtvalue);
}

static unsigned char s4LC898OTP_ReadReg(unsigned short RegAddr)
{ 
    int  i4RetValue = 0;
    unsigned char pBuff; // = (unsigned char)RegAddr;
    unsigned char RegData=0xFF;


    g_pstLC898122AF_I2Cclient->addr = ( 0xA1| ((RegAddr&0x0700) >>7) ) >> 1;
    pBuff = (unsigned char)(RegAddr&0xff);
    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, &pBuff, 1);
    if (i4RetValue < 0 ) 
    {
        LC898122AFDB("[CAMERA SENSOR] read I2C send failed!!\n");
        return 0xff;
    }

    i4RetValue = i2c_master_recv(g_pstLC898122AF_I2Cclient, &RegData, 1);

    LC898122AFDB("[LC898122AF]OTPI2C r (%x %x) \n",RegAddr,RegData);
    if (i4RetValue != 1) 
    {
        LC898122AFDB("[CAMERA SENSOR] I2C read failed!! \n");
        return 0xff;
    }
    return RegData;

}
#if 0
static void s4LC898OTP_WriteReg(unsigned short RegAddr, unsigned char RegData)
{
    int  i4RetValue = 0;
    char puSendCmd[2] = {(unsigned char)RegAddr, RegData};
    LC898122AFDB("[LC898122AF]OTPI2C w (%x %x) \n",RegAddr,RegData);

    g_pstLC898122AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898122AF_I2Cclient->addr = (0xA0 >> 1);
    i4RetValue = i2c_master_send(g_pstLC898122AF_I2Cclient, puSendCmd, 2);
    if (i4RetValue < 0) 
    {
        LC898122AFDB("[LC898122AF]I2C send failed!! \n");
        return;
    }
}
#endif 
inline static int getLC898122AFInfo(__user stLC898122AF_MotorInfo * pstMotorInfo)
{
    stLC898122AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4LC898122AF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4LC898122AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

    if (g_i4MotorStatus == 1)    {stMotorInfo.bIsMotorMoving = 1;}
    else                        {stMotorInfo.bIsMotorMoving = 0;}

    if (g_s4LC898122AF_Opened >= 1)    {stMotorInfo.bIsMotorOpen = 1;}
    else                        {stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stLC898122AF_MotorInfo)))
    {
        LC898122AFDB("[LC898122AF] copy to user failed when getting motor information \n");
    }
    return 0;
}

void LC898122AF_init_drv(void)
{
    unsigned short addrotp;
    unsigned long dataotp=0;
    bool encal=false;
    IniSetAf();
    IniSet();
RamAccFixMod(ON); //16bit Fix mode
    addrotp=0x3db;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);
    encal = dataotp!=0xffff ? true : false;
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
    //if(encal) RamWriteA(0x1479,dataotp);  //Hall offset X
    
    addrotp=0x3dd;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x14F9,dataotp);  //Hall offset Y
    
    addrotp=0x3df;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x147A,dataotp);  //Hall bias X
    
    addrotp=0x3e1;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x14FA,dataotp);  //Hall bias Y
    
    addrotp=0x3e3;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x1450,dataotp);  //Hall AD offset X
    
    addrotp=0x3e5;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x14D0,dataotp);  //Hall AD offset Y
    
    addrotp=0x3e7;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x10D3,dataotp);  //Loop gain X
    
    addrotp=0x3e9;dataotp=(s4LC898OTP_ReadReg(addrotp+1)<<8)+s4LC898OTP_ReadReg(addrotp);    
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RamWriteA(0x11D3,dataotp);  //Loop gain Y
    		
RamAccFixMod(OFF); //32bit Float mode
    addrotp=0x3f0;dataotp=s4LC898OTP_ReadReg(addrotp);  
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RegWriteA(0x02a0,dataotp);  //Gyro offset X M
    addrotp=0xef;dataotp=s4LC898OTP_ReadReg(addrotp);  
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
   // if(encal) RegWriteA(0x02a1,dataotp);  //Gyro offset X L
    addrotp=0x3f2;dataotp=s4LC898OTP_ReadReg(addrotp);  
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RegWriteA(0x02a2,dataotp);  //Gyro offset Y M
    addrotp=0x3f1;dataotp=s4LC898OTP_ReadReg(addrotp);  
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    //if(encal) RegWriteA(0x02a3,dataotp);  //Gyro offset Y L
    
    addrotp=0x3f3;dataotp=s4LC898OTP_ReadReg(addrotp);  
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
   // if(encal) RegWriteA(0x0257,dataotp);//OSC
    
    addrotp=0x3f4;
    dataotp= (s4LC898OTP_ReadReg(addrotp))
            +(s4LC898OTP_ReadReg(addrotp+1)<<8)
            +(s4LC898OTP_ReadReg(addrotp+2)<<16)
            +(s4LC898OTP_ReadReg(addrotp+3)<<24); 
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);    
    //if(encal) RamWrite32A(0x1020,dataotp);  //Gyro gain X
    
    addrotp=0x3f8;
    dataotp= (s4LC898OTP_ReadReg(addrotp))
            +(s4LC898OTP_ReadReg(addrotp+1)<<8)
            +(s4LC898OTP_ReadReg(addrotp+2)<<16)
            +(s4LC898OTP_ReadReg(addrotp+3)<<24); 
    LC898122AFDB("[LC898122AFOTP]0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);    
    //if(encal) RamWrite32A(0x1120,dataotp);  //Gyro gain Y


    RamWriteA(TCODEH, g_u4InitPosition); // focus position
    //RtnCen(0);
    //msleep(100);
    //SetPanTiltMode(ON);
    //msleep(10);
    //OisEna();
    //SetH1cMod(MOVMODE);  //movie mode
   // SetH1cMod(0);          //still mode

    addrotp=0x20;dataotp=(s4LC898OTP_ReadReg(addrotp)<<8)+s4LC898OTP_ReadReg(addrotp+1);  
    LC898122AFDB("[LC898122AFOTP]AF start current 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AFOTP]AF start current 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AFOTP]AF start current 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);

    addrotp=0x22;dataotp=(s4LC898OTP_ReadReg(addrotp)<<8)+s4LC898OTP_ReadReg(addrotp+1);  
    LC898122AFDB("[LC898122AFOTP]AF Infinit 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AFOTP]AF Infinit 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AFOTP]AF Infinit 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);

    addrotp=0x24;dataotp=(s4LC898OTP_ReadReg(addrotp)<<8)+s4LC898OTP_ReadReg(addrotp+1);  
    LC898122AFDB("[LC898122AFOTP]AF Macro 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AFOTP]AF Macro 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AFOTP]AF Macro 0x%x 0x%x\n", addrotp,  (unsigned int)dataotp);
    LC898122AFDB("[LC898122AF] LC898122AF_Open - End\n");
}

inline static int moveLC898122AF(unsigned long a_u4Position)
{
    if((a_u4Position > g_u4LC898122AF_MACRO) || (a_u4Position < g_u4LC898122AF_INF))
    {
        LC898122AFDB("[LC898122AF] out of range \n");
        return -EINVAL;
    }
	
    if (g_s4LC898122AF_Opened == 1)
    {
        LC898122AF_init_drv();
        spin_lock(&g_LC898122AF_SpinLock);
        g_u4CurrPosition = g_u4InitPosition;
        g_s4LC898122AF_Opened = 2;
        spin_unlock(&g_LC898122AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_LC898122AF_SpinLock);    
        g_i4Dir = 1;
        spin_unlock(&g_LC898122AF_SpinLock);    
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_LC898122AF_SpinLock);    
        g_i4Dir = -1;
        spin_unlock(&g_LC898122AF_SpinLock);            
    }
    else   return 0; 

    spin_lock(&g_LC898122AF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    g_sr = 3;
    g_i4MotorStatus = 0;
    spin_unlock(&g_LC898122AF_SpinLock);    
	RamWriteA(TCODEH, g_u4TargetPosition);

    spin_lock(&g_LC898122AF_SpinLock);        
    g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
    spin_unlock(&g_LC898122AF_SpinLock);                

    return 0;
}

inline static int setLC898122AFInf(unsigned long a_u4Position)
{
    spin_lock(&g_LC898122AF_SpinLock);
    g_u4LC898122AF_INF = a_u4Position;
    spin_unlock(&g_LC898122AF_SpinLock);    
    return 0;
}

inline static int setLC898122AFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_LC898122AF_SpinLock);
    g_u4LC898122AF_MACRO = a_u4Position;
    spin_unlock(&g_LC898122AF_SpinLock);    
    return 0;    
}

////////////////////////////////////////////////////////////////
static long LC898122AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case LC898122AFIOC_G_MOTORINFO :
            i4RetValue = getLC898122AFInfo((__user stLC898122AF_MotorInfo *)(a_u4Param));
        break;

        case LC898122AFIOC_T_MOVETO :
            i4RetValue = moveLC898122AF(a_u4Param);
        break;
 
        case LC898122AFIOC_T_SETINFPOS :
            i4RetValue = setLC898122AFInf(a_u4Param);
        break;

        case LC898122AFIOC_T_SETMACROPOS :
            i4RetValue = setLC898122AFMacro(a_u4Param);
        break;
        
        default :
              LC898122AFDB("[LC898122AF] No CMD \n");
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
static int LC898122AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    LC898122AFDB("[LC898122AF] LC898122AF_Open - Start\n");
    if(g_s4LC898122AF_Opened)
    {    
        LC898122AFDB("[LC898122AF] the device is opened \n");
        return -EBUSY;
    }
	
	spin_lock(&g_LC898122AF_SpinLock);
    g_s4LC898122AF_Opened = 1;
    spin_unlock(&g_LC898122AF_SpinLock);
    LC898122AFDB("[LC898122AF] LC898122AF_Open - End\n");
    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int LC898122AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    LC898122AFDB("[LC898122AF] LC898122AF_Release - Start\n");

    if (g_s4LC898122AF_Opened)
    {
        LC898122AFDB("[LC898122AF] feee \n");
        g_sr = 5;
				//RamWriteA(TCODEH, 100); // focus position
        //msleep(10);
			//	RamWriteA(TCODEH, 50); // focus position
       // msleep(10);
                                            
        spin_lock(&g_LC898122AF_SpinLock);
        g_s4LC898122AF_Opened = 0;
        spin_unlock(&g_LC898122AF_SpinLock);

    }
    LC898122AFDB("[LC898122AF] LC898122AF_Release - End\n");
    //RtnCen(0);
    //SrvCon(X_DIR,OFF);
    //SrvCon(Y_DIR,OFF);
    return 0;
}

static const struct file_operations g_stLC898122AF_fops = 
{
    .owner = THIS_MODULE,
    .open = LC898122AF_Open,
    .release = LC898122AF_Release,
    .unlocked_ioctl = LC898122AF_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = LC898122AF_Ioctl,
#endif
};

inline static int Register_LC898122AF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    LC898122AFDB("[LC898122AF] Register_LC898122AF_CharDrv - Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_LC898122AF_devno, 0, 1,LC898122AF_DRVNAME) )
    {
        LC898122AFDB("[LC898122AF] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pLC898122AF_CharDrv = cdev_alloc();

    if(NULL == g_pLC898122AF_CharDrv)
    {
        unregister_chrdev_region(g_LC898122AF_devno, 1);

        LC898122AFDB("[LC898122AF] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pLC898122AF_CharDrv, &g_stLC898122AF_fops);

    g_pLC898122AF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pLC898122AF_CharDrv, g_LC898122AF_devno, 1))
    {
        LC898122AFDB("[LC898122AF] Attatch file operation failed\n");

        unregister_chrdev_region(g_LC898122AF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv_OIS");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        LC898122AFDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_LC898122AF_devno, NULL, LC898122AF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    LC898122AFDB("[LC898122AF] Register_LC898122AF_CharDrv - End\n");    
    return 0;
}

inline static void Unregister_LC898122AF_CharDrv(void)
{
    LC898122AFDB("[LC898122AF] Unregister_LC898122AF_CharDrv - Start\n");

    //Release char driver
    cdev_del(g_pLC898122AF_CharDrv);

    unregister_chrdev_region(g_LC898122AF_devno, 1);
    
    device_destroy(actuator_class, g_LC898122AF_devno);

    class_destroy(actuator_class);

    LC898122AFDB("[LC898122AF] Unregister_LC898122AF_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int LC898122AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int LC898122AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id LC898122AF_i2c_id[] = {{LC898122AF_DRVNAME,0},{}};   
struct i2c_driver LC898122AF_i2c_driver = {                       
    .probe = LC898122AF_i2c_probe,                                   
    .remove = LC898122AF_i2c_remove,                           
    .driver.name = LC898122AF_DRVNAME,                 
    .id_table = LC898122AF_i2c_id,                             
};  

#if 0 
static int LC898122AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, LC898122AF_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int LC898122AF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int LC898122AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    LC898122AFDB("[LC898122AF] LC898122AF_i2c_probe\n");

    /* Kirby: add new-style driver { */
    g_pstLC898122AF_I2Cclient = client;
    
    g_pstLC898122AF_I2Cclient->addr = g_pstLC898122AF_I2Cclient->addr >> 1;
    
    //Register char driver
    i4RetValue = Register_LC898122AF_CharDrv();

    if(i4RetValue){

        LC898122AFDB("[LC898122AF] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_LC898122AF_SpinLock);

    LC898122AFDB("[LC898122AF] Attached!! \n");

    return 0;
}

static int LC898122AF_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&LC898122AF_i2c_driver);
}

static int LC898122AF_remove(struct platform_device *pdev)
{
    i2c_del_driver(&LC898122AF_i2c_driver);
    return 0;
}

static int LC898122AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int LC898122AF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stLC898122AF_Driver = {
    .probe        = LC898122AF_probe,
    .remove    = LC898122AF_remove,
    .suspend    = LC898122AF_suspend,
    .resume    = LC898122AF_resume,
    .driver        = {
        .name    = "lens_actuator_ois",
        .owner    = THIS_MODULE,
    }
};
static struct platform_device g_stAF_device = {
    .name = "lens_actuator_ois",
    .id = 0,
    .dev = {}
};

static int __init LC898122AF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);

    if(platform_device_register(&g_stAF_device)){
        LC898122AFDB("failed to register AF driver\n");
        return -ENODEV;
    }

    if(platform_driver_register(&g_stLC898122AF_Driver)){
        LC898122AFDB("failed to register LC898122AF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit LC898122AF_i2C_exit(void)
{
    platform_driver_unregister(&g_stLC898122AF_Driver);
}

module_init(LC898122AF_i2C_init);
module_exit(LC898122AF_i2C_exit);

MODULE_DESCRIPTION("LC898122AF lens module driver");
MODULE_AUTHOR("KY Chen <vend_james-cc.wu@Mediatek.com>");
MODULE_LICENSE("GPL");

