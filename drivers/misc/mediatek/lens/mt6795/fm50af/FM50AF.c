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
#include "FM50AF.h"
#include "../camera/kd_camera_hw.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

// in K2, main=3, sub=main2=1
#define LENS_I2C_BUSNUM 3

#define AF_DRVNAME "FM50AF"
#define I2C_SLAVE_ADDRESS        0x18
#define I2C_REGISTER_ID            0x18
#define PLATFORM_DRIVER_NAME "lens_actuator_fm50af"
#define AF_DRIVER_CLASS_NAME "actuatordrv_fm50af"

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

static int g_sr = 3;

static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
    int  i4RetValue = 0;
    char pBuff[2];

    i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff , 2);

    if (i4RetValue < 0)
    {
        LOG_INF("I2C read failed!! \n");
        return -1;
    }

    *a_pu2Result = (((u16)pBuff[0]) << 4) + (pBuff[1] >> 4);

    return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
    int  i4RetValue = 0;

    char puSendCmd[2] = {(char)(a_u2Data >> 4) , (char)(((a_u2Data & 0xF) << 4)+g_sr)};

    //LOG_INF("g_sr %d, write %d \n", g_sr, a_u2Data);
    g_pstAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

    if (i4RetValue < 0)
    {
        LOG_INF("I2C send failed!! \n");
        return -1;
    }

    return 0;
}

inline static int getAFInfo(__user stFM50AF_MotorInfo * pstMotorInfo)
{
    stFM50AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4AF_MACRO;
    stMotorInfo.u4InfPosition      = g_u4AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

    if (g_i4MotorStatus == 1)    {stMotorInfo.bIsMotorMoving = 1;}
    else                        {stMotorInfo.bIsMotorMoving = 0;}

    if (g_s4AF_Opened >= 1)    {stMotorInfo.bIsMotorOpen = 1;}
    else                        {stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stFM50AF_MotorInfo)))
    {
        LOG_INF("copy to user failed when getting motor information \n");
    }

    return 0;
}

#ifdef LensdrvCM3
inline static int getAFMETA(__user stFM50AF_MotorMETAInfo * pstMotorMETAInfo)
{
    stFM50AF_MotorMETAInfo stMotorMETAInfo;
    stMotorMETAInfo.Aperture=2.8;       //fn
    stMotorMETAInfo.Facing=1;
    stMotorMETAInfo.FilterDensity=1;   //X
    stMotorMETAInfo.FocalDistance=1.0;    //diopters
    stMotorMETAInfo.FocalLength=34.0;  //mm
    stMotorMETAInfo.FocusRange=1.0;    //diopters
    stMotorMETAInfo.InfoAvalibleApertures=2.8;
    stMotorMETAInfo.InfoAvalibleFilterDensity=1;
    stMotorMETAInfo.InfoAvalibleFocalLength=34.0;
    stMotorMETAInfo.InfoAvalibleHypeDistance=1.0;
    stMotorMETAInfo.InfoAvalibleMinFocusDistance=1.0;
    stMotorMETAInfo.InfoAvalibleOptStabilization=0;
    stMotorMETAInfo.OpticalAxisAng[0]=0.0;
    stMotorMETAInfo.OpticalAxisAng[1]=0.0;
    stMotorMETAInfo.Position[0]=0.0;
    stMotorMETAInfo.Position[1]=0.0;
    stMotorMETAInfo.Position[2]=0.0;
    stMotorMETAInfo.State=0;
    stMotorMETAInfo.u4OIS_Mode=0;

    if(copy_to_user(pstMotorMETAInfo , &stMotorMETAInfo , sizeof(stFM50AF_MotorMETAInfo)))
    {
        LOG_INF("copy to user failed when getting motor information \n");
    }

    return 0;
}
#endif

inline static int moveAF(unsigned long a_u4Position)
{
    int ret = 0;

    if((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF))
    {
        LOG_INF("out of range \n");
        return -EINVAL;
    }

    if (g_s4AF_Opened == 1)
    {
        unsigned short InitPos;
        ret = s4AF_ReadReg(&InitPos);

        if(ret == 0)
        {
            LOG_INF("Init Pos %6d \n", InitPos);

            spin_lock(&g_AF_SpinLock);
            g_u4CurrPosition = (unsigned long)InitPos;
            spin_unlock(&g_AF_SpinLock);

        }
        else
        {
            spin_lock(&g_AF_SpinLock);
            g_u4CurrPosition = 0;
            spin_unlock(&g_AF_SpinLock);
        }

        spin_lock(&g_AF_SpinLock);
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

    //LOG_INF("move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_AF_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
            spin_unlock(&g_AF_SpinLock);

            if(s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0)
            {
                spin_lock(&g_AF_SpinLock);
                g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
                spin_unlock(&g_AF_SpinLock);
            }
            else
            {
                LOG_INF("set I2C failed when moving the motor \n");

                spin_lock(&g_AF_SpinLock);
                g_i4MotorStatus = -1;
                spin_unlock(&g_AF_SpinLock);
            }

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
        case FM50AFIOC_G_MOTORINFO :
            i4RetValue = getAFInfo((__user stFM50AF_MotorInfo *)(a_u4Param));
        break;
        #ifdef LensdrvCM3
        case FM50AFIOC_G_MOTORMETAINFO :
            i4RetValue = getAFMETA((__user stFM50AF_MotorMETAInfo *)(a_u4Param));
        break;
        #endif
        case FM50AFIOC_T_MOVETO :
            i4RetValue = moveAF(a_u4Param);
        break;

        case FM50AFIOC_T_SETINFPOS :
            i4RetValue = setAFInf(a_u4Param);
        break;

        case FM50AFIOC_T_SETMACROPOS :
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

static int __init FM50AF_i2C_init(void)
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

static void __exit FM50AF_i2C_exit(void)
{
    platform_driver_unregister(&g_stAF_Driver);
}

module_init(FM50AF_i2C_init);
module_exit(FM50AF_i2C_exit);

MODULE_DESCRIPTION("FM50AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


