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
#include "AD5823.h"
#include "../camera/kd_camera_hw.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

// in K2, main=3, sub=main2=1
#define LENS_I2C_BUSNUM 0
static struct i2c_board_info kd_lens_dev __initdata = { I2C_BOARD_INFO("AD5823", 0x6C) };


#define AD5823_DRVNAME "AD5823"
#define AD5823_VCM_WRITE_ID           0x18

#define AD5823_DEBUG
#ifdef AD5823_DEBUG
#define AD5823DB pr_debug
#else
#define AD5823DB(x, ...)
#endif

static spinlock_t g_AD5823_SpinLock;

static struct i2c_client *g_pstAD5823_I2Cclient;

static dev_t g_AD5823_devno;
static struct cdev *g_pAD5823_CharDrv;
static struct class *actuator_class;

static int g_s4AD5823_Opened;
static long g_i4MotorStatus;
static long g_i4Dir;
static unsigned long g_u4AD5823_INF;
static unsigned long g_u4AD5823_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int mode_init = 1;

static int g_sr = 3;

#if 0
extern s32 mt_set_gpio_mode(u32 u4Pin, u32 u4Mode);
extern s32 mt_set_gpio_out(u32 u4Pin, u32 u4PinOut);
extern s32 mt_set_gpio_dir(u32 u4Pin, u32 u4Dir);
#endif

static int s4AD5823_ReadReg(unsigned short *a_pu2Result)
{
/*
    int  i4RetValue = 0;
    char pBuff[2];

    i4RetValue = i2c_master_recv(g_pstAD5823_I2Cclient, pBuff , 2);

    if (i4RetValue < 0)
    {
	AD5823DB("[AD5823] I2C read failed!!\n");
	return -1;
    }

    *a_pu2Result = (((u16)pBuff[0]) << 4) + (pBuff[1] >> 4);
    */
	int i4RetValue = 0;
	char pBuff[2];
	char VCMMSB[1] = { (char)(0x04) };
	char VCMLSB[1] = { (char)(0x05) };

	g_pstAD5823_I2Cclient->addr = 0x0C;

	AD5823DB("[AD5823] s4AD5823_ReadReg\n");

	/* Read MSB */
	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, VCMMSB, 1);

	if (i4RetValue < 0) {
		AD5823DB("[AD5823] I2C send failed!!\n");
		return -1;
	}
	i4RetValue = i2c_master_recv(g_pstAD5823_I2Cclient, &pBuff[1], 1);

	AD5823DB("[AD5823] s4AD5823_ReadReg VCMMSB: 0x%x\n", pBuff[1]);

	/* Read LSB */
	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, VCMLSB, 1);

	if (i4RetValue < 0) {
		AD5823DB("[AD5823] I2C send failed!!\n");
		return -1;
	}


	i4RetValue = i2c_master_recv(g_pstAD5823_I2Cclient, &pBuff[0], 1);
	AD5823DB("[AD5823] s4AD5823_ReadReg VCMLSB: 0x%x\n", pBuff[0]);

	if (i4RetValue < 0) {
		AD5823DB("[AD5823] I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = ((u16) pBuff[0] + (u16) (pBuff[1] << 8));
	AD5823DB("[AD5823] s4AD5823_ReadReg *a_pu2Result: %d\n", *a_pu2Result);

	return 0;
}

static int s4AD5823_WriteReg(u16 a_u2Data)
{
/*
    int  i4RetValue = 0;

    char puSendCmd[2] = {(char)(a_u2Data >> 4) , (char)(((a_u2Data & 0xF) << 4)+g_sr)};

    //AD5823DB("[AD5823] g_sr %d, write %d\n", g_sr, a_u2Data);
    g_pstAD5823_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, puSendCmd, 2);

    if (i4RetValue < 0)
    {
	AD5823DB("[AD5823] I2C send failed!!\n");
	return -1;
    }
    */

	int i4RetValue = 0;

	/* AD5823DB("[AD5823] g_sr %d, write %d\n", g_sr, a_u2Data); */

	/* 0x04[1:0] VCM MSB data */
	/* 0x05[7:0] VCM LSB data */
	char VCMMSB[2] = { (char)(0x04), (char)((a_u2Data >> 8) & 0x03) };
	char VCMLSB[2] = { (char)(0x05), (char)(a_u2Data & 0xFF) };

	g_pstAD5823_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
	g_pstAD5823_I2Cclient->addr = 0x0C;

	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, VCMMSB, 2);
	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, VCMLSB, 2);

	if (i4RetValue < 0) {
		AD5823DB("[AD5823] I2C send failed!!\n");
		return -1;
	}

	return 0;
}

inline static int getAD5823Info(__user stAD5823_MotorInfo * pstMotorInfo)
{
	stAD5823_MotorInfo stMotorInfo;
	stMotorInfo.u4MacroPosition = g_u4AD5823_MACRO;
	stMotorInfo.u4InfPosition = g_u4AD5823_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = TRUE;

	if (g_i4MotorStatus == 1) {
		stMotorInfo.bIsMotorMoving = 1;
	} else {
		stMotorInfo.bIsMotorMoving = 0;
	}

	if (g_s4AD5823_Opened >= 1) {
		stMotorInfo.bIsMotorOpen = 1;
	} else {
		stMotorInfo.bIsMotorOpen = 0;
	}

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stAD5823_MotorInfo))) {
		AD5823DB("[AD5823] copy to user failed when getting motor information\n");
	}

	return 0;
}


static int AD5823_Mode_Init(int a)
{
	int i4RetValue = 0;
	/* char puSendCmd[2] = {(char)(a_u2Data >> 4) , (char)(((a_u2Data & 0xF) << 4)+0xF)}; */
	char Mode[2] = { (char)(0x02), (char)(0x01) };
	char MoveTime[2] = { (char)(0x03), (char)(0x4B) };
	char VCMMSB[2] = { (char)(0x04), (char)(0x05) };
	char VCMLSB[2] = { (char)(0x05), (char)(0x32) };
	AD5823DB("[AD5823] s4AD5823_mode_init :0x02\n");

	g_pstAD5823_I2Cclient->addr = 0x0C;

	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, Mode, 2);
	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, MoveTime, 2);
#if 1
	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, VCMMSB, 2);
	i4RetValue = i2c_master_send(g_pstAD5823_I2Cclient, VCMLSB, 2);
#endif
	mode_init = 0;
	return 0;
}



inline static int moveAD5823(unsigned long a_u4Position)
{
	int ret = 0;

	if (mode_init) {
		AD5823_Mode_Init(1);
	}

	if ((a_u4Position > g_u4AD5823_MACRO) || (a_u4Position < g_u4AD5823_INF)) {
		AD5823DB("[AD5823] out of range\n");
		return -EINVAL;
	}

	if (g_s4AD5823_Opened == 1) {
		unsigned short InitPos;
		ret = s4AD5823_ReadReg(&InitPos);

		spin_lock(&g_AD5823_SpinLock);
		if (ret == 0) {
			AD5823DB("[AD5823] Init Pos %6d\n", InitPos);
			g_u4CurrPosition = (unsigned long)InitPos;
		} else {
			g_u4CurrPosition = 0;
		}
		g_s4AD5823_Opened = 2;
		spin_unlock(&g_AD5823_SpinLock);
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(&g_AD5823_SpinLock);
		g_i4Dir = 1;
		spin_unlock(&g_AD5823_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(&g_AD5823_SpinLock);
		g_i4Dir = -1;
		spin_unlock(&g_AD5823_SpinLock);
	} else {
		return 0;
	}

	spin_lock(&g_AD5823_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(&g_AD5823_SpinLock);

	/* AD5823DB("[AD5823] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition); */

	spin_lock(&g_AD5823_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(&g_AD5823_SpinLock);

	if (s4AD5823_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(&g_AD5823_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(&g_AD5823_SpinLock);
	} else {
		AD5823DB("[AD5823] set I2C failed when moving the motor\n");
		spin_lock(&g_AD5823_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(&g_AD5823_SpinLock);
	}

	return 0;
}

inline static int setAD5823Inf(unsigned long a_u4Position)
{
	spin_lock(&g_AD5823_SpinLock);
	g_u4AD5823_INF = a_u4Position;
	spin_unlock(&g_AD5823_SpinLock);
	return 0;
}

inline static int setAD5823Macro(unsigned long a_u4Position)
{
	spin_lock(&g_AD5823_SpinLock);
	g_u4AD5823_MACRO = a_u4Position;
	spin_unlock(&g_AD5823_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
static long AD5823_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AD5823IOC_G_MOTORINFO:
		i4RetValue = getAD5823Info((__user stAD5823_MotorInfo *) (a_u4Param));
		break;

	case AD5823IOC_T_MOVETO:
		i4RetValue = moveAD5823(a_u4Param);
		break;

	case AD5823IOC_T_SETINFPOS:
		i4RetValue = setAD5823Inf(a_u4Param);
		break;

	case AD5823IOC_T_SETMACROPOS:
		i4RetValue = setAD5823Macro(a_u4Param);
		break;

	default:
		AD5823DB("[AD5823] No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
/* 3.Update f_op pointer. */
/* 4.Fill data structures into private_data */
/* CAM_RESET */
static int AD5823_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	AD5823DB("[AD5823] AD5823_Open - Start\n");

	spin_lock(&g_AD5823_SpinLock);

	if (g_s4AD5823_Opened) {
		spin_unlock(&g_AD5823_SpinLock);
		AD5823DB("[AD5823] the device is opened\n");
		return -EBUSY;
	}

	g_s4AD5823_Opened = 1;

	spin_unlock(&g_AD5823_SpinLock);

	AD5823DB("[AD5823] AD5823_Open - End\n");

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int AD5823_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	AD5823DB("[AD5823] AD5823_Release - Start\n");

	if (g_s4AD5823_Opened) {
		AD5823DB("[AD5823] feee\n");
		g_sr = 5;
		s4AD5823_WriteReg(200);
		msleep(10);
		s4AD5823_WriteReg(100);
		msleep(10);

		spin_lock(&g_AD5823_SpinLock);
		g_s4AD5823_Opened = 0;
		spin_unlock(&g_AD5823_SpinLock);

	}

	AD5823DB("[AD5823] AD5823_Release - End\n");

	return 0;
}

static const struct file_operations g_stAD5823_fops = {
	.owner = THIS_MODULE,
	.open = AD5823_Open,
	.release = AD5823_Release,
	.unlocked_ioctl = AD5823_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = AD5823_Ioctl,
#endif
};

inline static int Register_AD5823_CharDrv(void)
{
	struct device *vcm_device = NULL;

	AD5823DB("[AD5823] Register_AD5823_CharDrv - Start\n");

	/* Allocate char driver no. */
	if (alloc_chrdev_region(&g_AD5823_devno, 0, 1, AD5823_DRVNAME)) {
		AD5823DB("[AD5823] Allocate device no failed\n");

		return -EAGAIN;
	}
	/* Allocate driver */
	g_pAD5823_CharDrv = cdev_alloc();

	if (NULL == g_pAD5823_CharDrv) {
		unregister_chrdev_region(g_AD5823_devno, 1);

		AD5823DB("[AD5823] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pAD5823_CharDrv, &g_stAD5823_fops);

	g_pAD5823_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pAD5823_CharDrv, g_AD5823_devno, 1)) {
		AD5823DB("[AD5823] Attatch file operation failed\n");

		unregister_chrdev_region(g_AD5823_devno, 1);

		return -EAGAIN;
	}

	actuator_class = class_create(THIS_MODULE, "actuatordrvAD5823");
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);
		AD5823DB("Unable to create class, err = %d\n", ret);
		return ret;
	}

	vcm_device = device_create(actuator_class, NULL, g_AD5823_devno, NULL, AD5823_DRVNAME);

	if (NULL == vcm_device) {
		return -EIO;
	}

	AD5823DB("[AD5823] Register_AD5823_CharDrv - End\n");
	return 0;
}

inline static void Unregister_AD5823_CharDrv(void)
{
	AD5823DB("[AD5823] Unregister_AD5823_CharDrv - Start\n");

	/* Release char driver */
	cdev_del(g_pAD5823_CharDrv);

	unregister_chrdev_region(g_AD5823_devno, 1);

	device_destroy(actuator_class, g_AD5823_devno);

	class_destroy(actuator_class);

	AD5823DB("[AD5823] Unregister_AD5823_CharDrv - End\n");
}

/* //////////////////////////////////////////////////////////////////// */

static int AD5823_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int AD5823_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id AD5823_i2c_id[] = { {AD5823_DRVNAME, 0}, {} };

struct i2c_driver AD5823_i2c_driver = {
	.probe = AD5823_i2c_probe,
	.remove = AD5823_i2c_remove,
	.driver.name = AD5823_DRVNAME,
	.id_table = AD5823_i2c_id,
};

#if 0
static int AD5823_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, AD5823_DRVNAME);
	return 0;
}
#endif
static int AD5823_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/* Kirby: add new-style driver {*/
static int AD5823_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	AD5823DB("[AD5823] AD5823_i2c_probe\n");

	/* Kirby: add new-style driver { */
	g_pstAD5823_I2Cclient = client;

	g_pstAD5823_I2Cclient->addr = g_pstAD5823_I2Cclient->addr >> 1;

	/* Register char driver */
	i4RetValue = Register_AD5823_CharDrv();

	if (i4RetValue) {

		AD5823DB("[AD5823] register char device failed!\n");

		return i4RetValue;
	}

	spin_lock_init(&g_AD5823_SpinLock);

	AD5823DB("[AD5823] Attached!!\n");

	return 0;
}

static int AD5823_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&AD5823_i2c_driver);
}

static int AD5823_remove(struct platform_device *pdev)
{
	i2c_del_driver(&AD5823_i2c_driver);
	return 0;
}

static int AD5823_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int AD5823_resume(struct platform_device *pdev)
{
	return 0;
}

/* platform structure */
static struct platform_driver g_stAD5823_Driver = {
	.probe = AD5823_probe,
	.remove = AD5823_remove,
	.suspend = AD5823_suspend,
	.resume = AD5823_resume,
	.driver = {
		   .name = "lens_actuatorAD5823",
		   .owner = THIS_MODULE,
		   }
};
static struct platform_device g_stAD5823_device = {
    .name = PLATFORM_DRIVER_NAME,
    .id = 0,
    .dev = {}
};
static int __init AD5823_i2C_init(void)
{
	i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);

  if(platform_device_register(&g_stAD5823_device)){
    AD5823DB("failed to register AF driver\n");
    return -ENODEV;
  }

	if (platform_driver_register(&g_stAD5823_Driver)) {
		AD5823DB("failed to register AD5823 driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit AD5823_i2C_exit(void)
{
	platform_driver_unregister(&g_stAD5823_Driver);
}
module_init(AD5823_i2C_init);
module_exit(AD5823_i2C_exit);

MODULE_DESCRIPTION("AD5823 lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");
