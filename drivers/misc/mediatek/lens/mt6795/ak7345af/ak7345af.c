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
#include "AK7345AF.h"
#include "../camera/kd_camera_hw.h"

#define LENS_I2C_BUSNUM 1
static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("AK7345AF", 0x18)};


#define AK7345AF_DRVNAME "AK7345AF"
#define AK7345AF_VCM_WRITE_ID           0x18

#define AK7345AF_DEBUG
#ifdef AK7345AF_DEBUG
#define AK7345AFDB printk
#else
#define AK7345AFDB(x,...)
#endif

static spinlock_t g_AK7345AF_SpinLock;

static struct i2c_client * g_pstAK7345AF_I2Cclient = NULL;

static dev_t g_AK7345AF_devno;
static struct cdev * g_pAK7345AF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4AK7345AF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4AK7345AF_INF = 0;
static unsigned long g_u4AK7345AF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

static int g_sr = 3;

//extern s32 mt_set_gpio_mode(u32 u4Pin, u32 u4Mode);
//extern s32 mt_set_gpio_out(u32 u4Pin, u32 u4PinOut);
//extern s32 mt_set_gpio_dir(u32 u4Pin, u32 u4Dir);


static int s4AK7345AF_ReadReg(u16 a_u2Addr, unsigned short * a_pu2Result)
{
    int  i4RetValue = 0;
    char pBuff;

    if (i2c_master_send(g_pstAK7345AF_I2Cclient, &a_u2Addr, 1) !=1) 
    {
        AK7345AFDB("[AK7345AF] I2C read failed(in send)!! \n");
        return -1;
    }

    if (i2c_master_recv(g_pstAK7345AF_I2Cclient, &pBuff , 1) !=1)
    {
        AK7345AFDB("[AK7345AF] I2C read failed!! \n");
        return -1;
    }
    *a_pu2Result = pBuff;
    return 0;
}
static int s4AK7345AF_WriteReg(u16 a_u2Addr, u16 a_u2Data)
{
    int  i4RetValue = 0;
    char puSendCmd[2] = {(char)a_u2Addr , (char)a_u2Data};
	
    g_pstAK7345AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    if (i2c_master_send(g_pstAK7345AF_I2Cclient, puSendCmd, 2) < 0) 
    {
        AK7345AFDB("[AK7345AF] I2C send failed!! \n");
        return -1;
    }

    return 0;
}

inline static int getAK7345AFInfo(__user stAK7345AF_MotorInfo * pstMotorInfo)
{
    stAK7345AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4AK7345AF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4AK7345AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

	if (g_i4MotorStatus == 1)	{stMotorInfo.bIsMotorMoving = 1;}
	else						{stMotorInfo.bIsMotorMoving = 0;}

	if (g_s4AK7345AF_Opened >= 1)	{stMotorInfo.bIsMotorOpen = 1;}
	else						{stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stAK7345AF_MotorInfo)))
    {
        AK7345AFDB("[AK7345AF] copy to user failed when getting motor information \n");
    }

    return 0;
}

inline static int moveAK7345AF(unsigned long a_u4Position)
{
    int ret = 0;
    
    if((a_u4Position > g_u4AK7345AF_MACRO) || (a_u4Position < g_u4AK7345AF_INF))
    {
        AK7345AFDB("[AK7345AF] out of range \n");
        return -EINVAL;
    }

    if (g_s4AK7345AF_Opened == 1)
    {
        unsigned short InitPos,InitPosM,InitPosL;

		//00:active mode	10:Standby mode    x1:Sleep mode
		s4AK7345AF_WriteReg(0x02,0x00);//from Standby mode to Active mode
		msleep(10); 
        s4AK7345AF_ReadReg(0x0,&InitPosM);
	    ret = s4AK7345AF_ReadReg(0x1,&InitPosL);
		InitPos = ((InitPosM&0xFF)<<1) +  ((InitPosL>>7)&1);

        if(ret == 0)
        {
            AK7345AFDB("[AK7345AF] Init Pos %6d \n", InitPos);
        spin_lock(&g_AK7345AF_SpinLock);
            g_u4CurrPosition = (unsigned long)InitPos;
        spin_unlock(&g_AK7345AF_SpinLock);
        }
        else
        {		
        spin_lock(&g_AK7345AF_SpinLock);
            g_u4CurrPosition = 0;
        spin_unlock(&g_AK7345AF_SpinLock);
        }
        spin_lock(&g_AK7345AF_SpinLock);
        g_s4AK7345AF_Opened = 2;
        spin_unlock(&g_AK7345AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_AK7345AF_SpinLock);	
        g_i4Dir = 1;
        spin_unlock(&g_AK7345AF_SpinLock);	
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_AK7345AF_SpinLock);	
        g_i4Dir = -1;
        spin_unlock(&g_AK7345AF_SpinLock);			
    }
    else										{return 0;}

    spin_lock(&g_AK7345AF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_AK7345AF_SpinLock);	

    //AK7345AFDB("[AK7345AF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_AK7345AF_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
            spin_unlock(&g_AK7345AF_SpinLock);	
		
	s4AK7345AF_WriteReg(0x02,0x00);
    if( s4AK7345AF_WriteReg(0x0, (u16)((g_u4TargetPosition>>2)&0xff)) == 0
     && s4AK7345AF_WriteReg(0x1, (u16)(((g_u4TargetPosition>>1)&1)<<7)) == 0)
            {
                spin_lock(&g_AK7345AF_SpinLock);		
                g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
                spin_unlock(&g_AK7345AF_SpinLock);				
            }
            else
            {
                AK7345AFDB("[AK7345AF] set I2C failed when moving the motor \n");			
                spin_lock(&g_AK7345AF_SpinLock);
                g_i4MotorStatus = -1;
                spin_unlock(&g_AK7345AF_SpinLock);				
            }

    return 0;
}

inline static int setAK7345AFInf(unsigned long a_u4Position)
{
    spin_lock(&g_AK7345AF_SpinLock);
    g_u4AK7345AF_INF = a_u4Position;
    spin_unlock(&g_AK7345AF_SpinLock);	
    return 0;
}

inline static int setAK7345AFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_AK7345AF_SpinLock);
    g_u4AK7345AF_MACRO = a_u4Position;
    spin_unlock(&g_AK7345AF_SpinLock);	
    return 0;	
}

////////////////////////////////////////////////////////////////
static long AK7345AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case AK7345AFIOC_G_MOTORINFO :
            i4RetValue = getAK7345AFInfo((__user stAK7345AF_MotorInfo *)(a_u4Param));
        break;

        case AK7345AFIOC_T_MOVETO :
            i4RetValue = moveAK7345AF(a_u4Param);
        break;
 
        case AK7345AFIOC_T_SETINFPOS :
            i4RetValue = setAK7345AFInf(a_u4Param);
        break;

        case AK7345AFIOC_T_SETMACROPOS :
            i4RetValue = setAK7345AFMacro(a_u4Param);
        break;
		
        default :
      	    AK7345AFDB("[AK7345AF] No CMD \n");
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
static int AK7345AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    AK7345AFDB("[AK7345AF] AK7345AF_Open - Start\n");


    if(g_s4AK7345AF_Opened)
    {
        AK7345AFDB("[AK7345AF] the device is opened \n");
        return -EBUSY;
    }
    spin_lock(&g_AK7345AF_SpinLock);
    g_s4AK7345AF_Opened = 1;
    spin_unlock(&g_AK7345AF_SpinLock);

    AK7345AFDB("[AK7345AF] AK7345AF_Open - End\n");

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int AK7345AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    AK7345AFDB("[AK7345AF] AK7345AF_Release - Start\n");

    if (g_s4AK7345AF_Opened)
    {
        AK7345AFDB("[AK7345AF] feee \n");
        g_sr = 5;
        msleep(10);  	    	    
        spin_lock(&g_AK7345AF_SpinLock);
        g_s4AK7345AF_Opened = 0;
        spin_unlock(&g_AK7345AF_SpinLock);

    }

    AK7345AFDB("[AK7345AF] AK7345AF_Release - End\n");

    return 0;
}

static const struct file_operations g_stAK7345AF_fops = 
{
    .owner = THIS_MODULE,
    .open = AK7345AF_Open,
    .release = AK7345AF_Release,
    .unlocked_ioctl = AK7345AF_Ioctl
};

inline static int Register_AK7345AF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    AK7345AFDB("[AK7345AF] Register_AK7345AF_CharDrv - Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_AK7345AF_devno, 0, 1,AK7345AF_DRVNAME) )
    {
        AK7345AFDB("[AK7345AF] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pAK7345AF_CharDrv = cdev_alloc();

    if(NULL == g_pAK7345AF_CharDrv)
    {
        unregister_chrdev_region(g_AK7345AF_devno, 1);

        AK7345AFDB("[AK7345AF] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pAK7345AF_CharDrv, &g_stAK7345AF_fops);

    g_pAK7345AF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pAK7345AF_CharDrv, g_AK7345AF_devno, 1))
    {
        AK7345AFDB("[AK7345AF] Attatch file operation failed\n");

        unregister_chrdev_region(g_AK7345AF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        AK7345AFDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_AK7345AF_devno, NULL, AK7345AF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    AK7345AFDB("[AK7345AF] Register_AK7345AF_CharDrv - End\n");    
    return 0;
}

inline static void Unregister_AK7345AF_CharDrv(void)
{
    AK7345AFDB("[AK7345AF] Unregister_AK7345AF_CharDrv - Start\n");

    //Release char driver
    cdev_del(g_pAK7345AF_CharDrv);

    unregister_chrdev_region(g_AK7345AF_devno, 1);
    
    device_destroy(actuator_class, g_AK7345AF_devno);

    class_destroy(actuator_class);

    AK7345AFDB("[AK7345AF] Unregister_AK7345AF_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int AK7345AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int AK7345AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id AK7345AF_i2c_id[] = {{AK7345AF_DRVNAME,0},{}};   
struct i2c_driver AK7345AF_i2c_driver = {                       
    .probe = AK7345AF_i2c_probe,                                   
    .remove = AK7345AF_i2c_remove,                           
    .driver.name = AK7345AF_DRVNAME,                 
    .id_table = AK7345AF_i2c_id,                             
};  

#if 0 
static int AK7345AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, AK7345AF_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int AK7345AF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int AK7345AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    AK7345AFDB("[AK7345AF] AK7345AF_i2c_probe\n");

    /* Kirby: add new-style driver { */
    g_pstAK7345AF_I2Cclient = client;
    
    g_pstAK7345AF_I2Cclient->addr = g_pstAK7345AF_I2Cclient->addr >> 1;
    
    //Register char driver
    i4RetValue = Register_AK7345AF_CharDrv();

    if(i4RetValue){

        AK7345AFDB("[AK7345AF] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_AK7345AF_SpinLock);

    AK7345AFDB("[AK7345AF] Attached!! \n");

    return 0;
}

static int AK7345AF_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&AK7345AF_i2c_driver);
}

static int AK7345AF_remove(struct platform_device *pdev)
{
    i2c_del_driver(&AK7345AF_i2c_driver);
    return 0;
}

static int AK7345AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int AK7345AF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stAK7345AF_Driver = {
    .probe		= AK7345AF_probe,
    .remove	= AK7345AF_remove,
    .suspend	= AK7345AF_suspend,
    .resume	= AK7345AF_resume,
    .driver		= {
        .name	= "lens_actuator",
        .owner	= THIS_MODULE,
    }
};

static int __init AK7345AF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
	
    if(platform_driver_register(&g_stAK7345AF_Driver)){
        AK7345AFDB("failed to register AK7345AF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit AK7345AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stAK7345AF_Driver);
}

module_init(AK7345AF_i2C_init);
module_exit(AK7345AF_i2C_exit);

MODULE_DESCRIPTION("AK7345AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


