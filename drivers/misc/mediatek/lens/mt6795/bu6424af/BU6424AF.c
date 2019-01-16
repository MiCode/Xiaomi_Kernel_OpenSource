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
#include "BU6424AF.h"
#include "../camera/kd_camera_hw.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

// in K2, main=3, sub=main2=1
#define LENS_I2C_BUSNUM 0
static struct i2c_board_info kd_lens_dev __initdata = { I2C_BOARD_INFO("BU6424AF", 0x18) };


#define BU6424AF_DRVNAME "BU6424AF"
#define BU6424AF_VCM_WRITE_ID           0x18

#define BU6424AF_DEBUG
#ifdef BU6424AF_DEBUG
#define BU6424AFDB pr_debug
#else
#define BU6424AFDB(x, ...)
#endif

static spinlock_t g_BU6424AF_SpinLock;

static struct i2c_client *g_pstBU6424AF_I2Cclient;

static dev_t g_BU6424AF_devno;
static struct cdev *g_pBU6424AF_CharDrv;
static struct class *actuator_class;

static int g_s4BU6424AF_Opened;
static long g_i4MotorStatus;
static long g_i4Dir;
static unsigned long g_u4BU6424AF_INF;
static unsigned long g_u4BU6424AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int g_sr = 3;

/* extern int mt_set_gpio_mode(unsigned long  u4Pin, unsigned long  u4Mode); */
/* extern int mt_set_gpio_out(unsigned long  u4Pin, unsigned long  u4PinOut); */
/* extern int mt_set_gpio_dir(unsigned long  u4Pin, unsigned long  u4Dir); */


static int s4BU6424AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	i4RetValue = i2c_master_recv(g_pstBU6424AF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		BU6424AFDB("[BU6424AF]I2C read failed!!\n");
		return -1;
	}
	*a_pu2Result = (((u16) (pBuff[0] & 0x03)) << 8) + pBuff[1];

	return 0;
}

static int s4BU6424AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;
	char puSendCmd[2] = { (char)(((a_u2Data >> 8) & 0x03) | 0xc0), (char)(a_u2Data & 0xff) };

	g_pstBU6424AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
	i4RetValue = i2c_master_send(g_pstBU6424AF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0) {
		BU6424AFDB("[BU6424AF]I2C send failed!!\n");
		return -1;
	}
	return 0;
}

inline static int getBU6424AFInfo(__user stBU6424AF_MotorInfo * pstMotorInfo)
{
	stBU6424AF_MotorInfo stMotorInfo;
	stMotorInfo.u4MacroPosition = g_u4BU6424AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4BU6424AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = TRUE;

	if (g_i4MotorStatus == 1) {
		stMotorInfo.bIsMotorMoving = 1;
	} else {
		stMotorInfo.bIsMotorMoving = 0;
	}

	if (g_s4BU6424AF_Opened >= 1) {
		stMotorInfo.bIsMotorOpen = 1;
	} else {
		stMotorInfo.bIsMotorOpen = 0;
	}

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stBU6424AF_MotorInfo))) {
		BU6424AFDB("[BU6424AF] copy to user failed when getting motor information\n");
	}

	return 0;
}

inline static int moveBU6424AF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4BU6424AF_MACRO) || (a_u4Position < g_u4BU6424AF_INF)) {
		BU6424AFDB("[BU6424AF] out of range\n");
		return -EINVAL;
	}

	if (g_s4BU6424AF_Opened == 1) {
		unsigned short InitPos;
		ret = s4BU6424AF_ReadReg(&InitPos);

		spin_lock(&g_BU6424AF_SpinLock);
		if (ret == 0) {
			BU6424AFDB("[BU6424AF] Init Pos %6d\n", InitPos);
			g_u4CurrPosition = (unsigned long)InitPos;
		} else {
			g_u4CurrPosition = 0;
		}
		g_s4BU6424AF_Opened = 2;
		spin_unlock(&g_BU6424AF_SpinLock);
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(&g_BU6424AF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(&g_BU6424AF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(&g_BU6424AF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(&g_BU6424AF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(&g_BU6424AF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(&g_BU6424AF_SpinLock);

	/* BU6424AFDB("[BU6424AF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition); */

	spin_lock(&g_BU6424AF_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(&g_BU6424AF_SpinLock);

	if (s4BU6424AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(&g_BU6424AF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(&g_BU6424AF_SpinLock);
	} else {
		BU6424AFDB("[BU6424AF] set I2C failed when moving the motor\n");
		spin_lock(&g_BU6424AF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(&g_BU6424AF_SpinLock);
	}

	return 0;
}

inline static int setBU6424AFInf(unsigned long a_u4Position)
{
	spin_lock(&g_BU6424AF_SpinLock);
	g_u4BU6424AF_INF = a_u4Position;
	spin_unlock(&g_BU6424AF_SpinLock);
	return 0;
}

inline static int setBU6424AFMacro(unsigned long a_u4Position)
{
	spin_lock(&g_BU6424AF_SpinLock);
	g_u4BU6424AF_MACRO = a_u4Position;
	spin_unlock(&g_BU6424AF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
static long BU6424AF_Ioctl(struct file *a_pstFile,
			   unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case BU6424AFIOC_G_MOTORINFO:
		i4RetValue = getBU6424AFInfo((__user stBU6424AF_MotorInfo *) (a_u4Param));
		break;

	case BU6424AFIOC_T_MOVETO:
		i4RetValue = moveBU6424AF(a_u4Param);
		break;

	case BU6424AFIOC_T_SETINFPOS:
		i4RetValue = setBU6424AFInf(a_u4Param);
		break;

	case BU6424AFIOC_T_SETMACROPOS:
		i4RetValue = setBU6424AFMacro(a_u4Param);
		break;

	default:
		BU6424AFDB("[BU6424AF] No CMD\n");
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
static int BU6424AF_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	BU6424AFDB("[BU6424AF] BU6424AF_Open - Start\n");

	spin_lock(&g_BU6424AF_SpinLock);

	if (g_s4BU6424AF_Opened) {
		spin_unlock(&g_BU6424AF_SpinLock);
		BU6424AFDB("[BU6424AF] the device is opened\n");
		return -EBUSY;
	}

	g_s4BU6424AF_Opened = 1;

	spin_unlock(&g_BU6424AF_SpinLock);

	BU6424AFDB("[BU6424AF] BU6424AF_Open - End\n");

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int BU6424AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	BU6424AFDB("[BU6424AF] BU6424AF_Release - Start\n");

	if (g_s4BU6424AF_Opened) {
		BU6424AFDB("[BU6424AF] feee\n");
		g_sr = 5;
		s4BU6424AF_WriteReg(200);
		msleep(10);
		s4BU6424AF_WriteReg(100);
		msleep(10);

		spin_lock(&g_BU6424AF_SpinLock);
		g_s4BU6424AF_Opened = 0;
		spin_unlock(&g_BU6424AF_SpinLock);

	}

	BU6424AFDB("[BU6424AF] BU6424AF_Release - End\n");

	return 0;
}

static const struct file_operations g_stBU6424AF_fops = {
	.owner = THIS_MODULE,
	.open = BU6424AF_Open,
	.release = BU6424AF_Release,
	.unlocked_ioctl = BU6424AF_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = BU6424AF_Ioctl,
#endif
};

inline static int Register_BU6424AF_CharDrv(void)
{
	struct device *vcm_device = NULL;

	BU6424AFDB("[BU6424AF] Register_BU6424AF_CharDrv - Start\n");

	/* Allocate char driver no. */
	if (alloc_chrdev_region(&g_BU6424AF_devno, 0, 1, BU6424AF_DRVNAME)) {
		BU6424AFDB("[BU6424AF] Allocate device no failed\n");

		return -EAGAIN;
	}
	/* Allocate driver */
	g_pBU6424AF_CharDrv = cdev_alloc();

	if (NULL == g_pBU6424AF_CharDrv) {
		unregister_chrdev_region(g_BU6424AF_devno, 1);

		BU6424AFDB("[BU6424AF] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pBU6424AF_CharDrv, &g_stBU6424AF_fops);

	g_pBU6424AF_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pBU6424AF_CharDrv, g_BU6424AF_devno, 1)) {
		BU6424AFDB("[BU6424AF] Attatch file operation failed\n");

		unregister_chrdev_region(g_BU6424AF_devno, 1);

		return -EAGAIN;
	}

	actuator_class = class_create(THIS_MODULE, "actuatordrvBU6424AF");
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);
		BU6424AFDB("Unable to create class, err = %d\n", ret);
		return ret;
	}

	vcm_device = device_create(actuator_class, NULL, g_BU6424AF_devno, NULL, BU6424AF_DRVNAME);

	if (NULL == vcm_device) {
		return -EIO;
	}

	BU6424AFDB("[BU6424AF] Register_BU6424AF_CharDrv - End\n");
	return 0;
}

inline static void Unregister_BU6424AF_CharDrv(void)
{
	BU6424AFDB("[BU6424AF] Unregister_BU6424AF_CharDrv - Start\n");

	/* Release char driver */
	cdev_del(g_pBU6424AF_CharDrv);

	unregister_chrdev_region(g_BU6424AF_devno, 1);

	device_destroy(actuator_class, g_BU6424AF_devno);

	class_destroy(actuator_class);

	BU6424AFDB("[BU6424AF] Unregister_BU6424AF_CharDrv - End\n");
}

/* //////////////////////////////////////////////////////////////////// */

static int BU6424AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int BU6424AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id BU6424AF_i2c_id[] = { {BU6424AF_DRVNAME, 0}, {} };

struct i2c_driver BU6424AF_i2c_driver = {
	.probe = BU6424AF_i2c_probe,
	.remove = BU6424AF_i2c_remove,
	.driver.name = BU6424AF_DRVNAME,
	.id_table = BU6424AF_i2c_id,
};

#if 0
static int BU6424AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, BU6424AF_DRVNAME);
	return 0;
}
#endif
static int BU6424AF_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/* Kirby: add new-style driver {*/
static int BU6424AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	BU6424AFDB("[BU6424AF] BU6424AF_i2c_probe\n");

	/* Kirby: add new-style driver { */
	g_pstBU6424AF_I2Cclient = client;

	g_pstBU6424AF_I2Cclient->addr = g_pstBU6424AF_I2Cclient->addr >> 1;

	/* Register char driver */
	i4RetValue = Register_BU6424AF_CharDrv();

	if (i4RetValue) {

		BU6424AFDB("[BU6424AF] register char device failed!\n");

		return i4RetValue;
	}

	spin_lock_init(&g_BU6424AF_SpinLock);

	BU6424AFDB("[BU6424AF] Attached!!\n");

	return 0;
}

static int BU6424AF_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&BU6424AF_i2c_driver);
}

static int BU6424AF_remove(struct platform_device *pdev)
{
	i2c_del_driver(&BU6424AF_i2c_driver);
	return 0;
}

static int BU6424AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int BU6424AF_resume(struct platform_device *pdev)
{
	return 0;
}

/* platform structure */
static struct platform_driver g_stBU6424AF_Driver = {
	.probe = BU6424AF_probe,
	.remove = BU6424AF_remove,
	.suspend = BU6424AF_suspend,
	.resume = BU6424AF_resume,
	.driver = {
		   .name = "lens_actuatorBU6424AF",
		   .owner = THIS_MODULE,
		   }
};
static struct platform_device g_stBU6424AF_device = {
    .name = PLATFORM_DRIVER_NAME,
    .id = 0,
    .dev = {}
};
static int __init BU6424AF_i2C_init(void)
{
	i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
  if(platform_device_register(&g_stBU6424AF_device)){
    BU6424AFDB("failed to register AF driver\n");
    return -ENODEV;
  }
	if (platform_driver_register(&g_stBU6424AF_Driver)) {
		BU6424AFDB("failed to register BU6424AF driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit BU6424AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stBU6424AF_Driver);
}
module_init(BU6424AF_i2C_init);
module_exit(BU6424AF_i2C_exit);

MODULE_DESCRIPTION("BU6424AF lens module driver");
MODULE_AUTHOR("KY Chen <vend_james-cc.wu@Mediatek.com>");
MODULE_LICENSE("GPL");
