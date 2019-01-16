/*
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
#include "OV8825AF.h"
#include "../camera/kd_camera_hw.h"
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
// in K2, main=3, sub=main2=1
#define LENS_I2C_BUSNUM 3
static struct i2c_board_info kd_lens_dev __initdata = { I2C_BOARD_INFO("OV8825AF", 0x18) };


#define OV8825AF_DRVNAME "OV8825AF"
#define OV8825AF_VCM_WRITE_ID           0x6c

#define OV8825AF_DEBUG
#ifdef OV8825AF_DEBUG
#define OV8825AFDB pr_debug
#else
#define OV8825AFDB(x, ...)
#endif

static spinlock_t g_OV8825AF_SpinLock;
extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
#define OV8825AF_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, OV8825AF_VCM_WRITE_ID)

kal_uint16 OV8825AF_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	iReadReg((u16) addr, (u8 *) &get_byte, OV8825AF_VCM_WRITE_ID);
	return get_byte;
}

static struct i2c_client *g_pstOV8825AF_I2Cclient;

static dev_t g_OV8825AF_devno;
static struct cdev *g_pOV8825AF_CharDrv;
static struct class *actuator_class;

static int g_s4OV8825AF_Opened;
static long g_i4MotorStatus;
static long g_i4Dir;
static unsigned long g_u4OV8825AF_INF;
static unsigned long g_u4OV8825AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int g_sr = 3;

/* extern s32 mt_set_gpio_mode(u32 u4Pin, u32 u4Mode); */
/* extern s32 mt_set_gpio_out(u32 u4Pin, u32 u4PinOut); */
/* extern s32 mt_set_gpio_dir(u32 u4Pin, u32 u4Dir); */


static int s4OV8825AF_ReadReg(unsigned short *a_pu2Result)
{
	int temp = 0;
	/* char pBuff[2]; */

	temp = (OV8825AF_read_cmos_sensor(0x3618) + (OV8825AF_read_cmos_sensor(0x3619) << 8)) >> 4;

	*a_pu2Result = temp;
	OV8825AFDB("s4OV8825AF_ReadReg = %d\n", temp);
	return 0;
}

static int s4OV8825AF_WriteReg(u16 a_u2Data)
{
	u16 temp, SlewRate = 0;
	OV8825AFDB("s4OV8825AF_WriteReg = %d\n", a_u2Data);

	temp = (a_u2Data << 4) + 0 + SlewRate;
	OV8825AFDB("-----stemp=(a_u2Data<<4)+8+SlewRate = %d-----\n", temp);

	OV8825AF_write_cmos_sensor(0x3619, (temp >> 8) & 0xff);

	OV8825AF_write_cmos_sensor(0x3618, temp & 0xff);

	return 0;
}

inline static int getOV8825AFInfo(__user stOV8825AF_MotorInfo * pstMotorInfo)
{
	stOV8825AF_MotorInfo stMotorInfo;
	stMotorInfo.u4MacroPosition = g_u4OV8825AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4OV8825AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	if (g_i4MotorStatus == 1) {
		stMotorInfo.bIsMotorMoving = 1;
	} else {
		stMotorInfo.bIsMotorMoving = 0;
	}

	if (g_s4OV8825AF_Opened >= 1) {
		stMotorInfo.bIsMotorOpen = 1;
	} else {
		stMotorInfo.bIsMotorOpen = 0;
	}

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stOV8825AF_MotorInfo))) {
		OV8825AFDB("[OV8825AF] copy to user failed when getting motor information\n");
	}

	return 0;
}

inline static int moveOV8825AF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4OV8825AF_MACRO) || (a_u4Position < g_u4OV8825AF_INF)) {
		OV8825AFDB("[OV8825AF] out of range\n");
		return -EINVAL;
	}

	if (g_s4OV8825AF_Opened == 1) {
		unsigned short InitPos;
		ret = s4OV8825AF_ReadReg(&InitPos);


		if (ret == 0) {
			OV8825AFDB("[OV8825AF] Init Pos %6d\n", InitPos);

			spin_lock(&g_OV8825AF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(&g_OV8825AF_SpinLock);
		} else {
			spin_lock(&g_OV8825AF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(&g_OV8825AF_SpinLock);
		}
		spin_lock(&g_OV8825AF_SpinLock);
		g_s4OV8825AF_Opened = 2;
		spin_unlock(&g_OV8825AF_SpinLock);
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(&g_OV8825AF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(&g_OV8825AF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(&g_OV8825AF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(&g_OV8825AF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(&g_OV8825AF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(&g_OV8825AF_SpinLock);

	/* OV8825AFDB("[OV8825AF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition); */

	spin_lock(&g_OV8825AF_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(&g_OV8825AF_SpinLock);

	if (s4OV8825AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(&g_OV8825AF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(&g_OV8825AF_SpinLock);
	} else {
		OV8825AFDB("[OV8825AF] set I2C failed when moving the motor\n");
		spin_lock(&g_OV8825AF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(&g_OV8825AF_SpinLock);
	}

	return 0;
}

inline static int setOV8825AFInf(unsigned long a_u4Position)
{
	spin_lock(&g_OV8825AF_SpinLock);
	g_u4OV8825AF_INF = a_u4Position;
	spin_unlock(&g_OV8825AF_SpinLock);
	return 0;
}

inline static int setOV8825AFMacro(unsigned long a_u4Position)
{
	spin_lock(&g_OV8825AF_SpinLock);
	g_u4OV8825AF_MACRO = a_u4Position;
	spin_unlock(&g_OV8825AF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
static long OV8825AF_Ioctl(struct file *a_pstFile,
			   unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case OV8825AFIOC_G_MOTORINFO:
		i4RetValue = getOV8825AFInfo((__user stOV8825AF_MotorInfo *) (a_u4Param));
		break;

	case OV8825AFIOC_T_MOVETO:
		i4RetValue = moveOV8825AF(a_u4Param);
		break;

	case OV8825AFIOC_T_SETINFPOS:
		i4RetValue = setOV8825AFInf(a_u4Param);
		break;

	case OV8825AFIOC_T_SETMACROPOS:
		i4RetValue = setOV8825AFMacro(a_u4Param);
		break;

	default:
		OV8825AFDB("[OV8825AF] No CMD\n");
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
static int OV8825AF_Open(struct inode *a_pstInode, struct file *a_pstFile)
{

	if (g_s4OV8825AF_Opened) {
		OV8825AFDB("[OV8825AF] the device is opened\n");
		return -EBUSY;
	}

	spin_lock(&g_OV8825AF_SpinLock);
	g_s4OV8825AF_Opened = 1;
	spin_unlock(&g_OV8825AF_SpinLock);

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int OV8825AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	if (g_s4OV8825AF_Opened) {
		OV8825AFDB("[OV8825AF] feee\n");
		g_sr = 5;

		if (g_u4CurrPosition > 700) {
			s4OV8825AF_WriteReg(700);
			msleep(3);
		}

		if (g_u4CurrPosition > 600) {
			s4OV8825AF_WriteReg(600);
			msleep(3);
		}

		if (g_u4CurrPosition > 500) {
			s4OV8825AF_WriteReg(500);
			msleep(3);
		}

		if (g_u4CurrPosition > 400) {
			s4OV8825AF_WriteReg(400);
			msleep(3);
		}

		if (g_u4CurrPosition > 300) {
			s4OV8825AF_WriteReg(300);
			msleep(3);
		}

		if (g_u4CurrPosition > 200) {
			s4OV8825AF_WriteReg(200);
			msleep(3);
		}

		if (g_u4CurrPosition > 100) {
			s4OV8825AF_WriteReg(100);
			msleep(3);
		}

		spin_lock(&g_OV8825AF_SpinLock);
		g_s4OV8825AF_Opened = 0;
		spin_unlock(&g_OV8825AF_SpinLock);

	}

	return 0;
}

static const struct file_operations g_stOV8825AF_fops = {
	.owner = THIS_MODULE,
	.open = OV8825AF_Open,
	.release = OV8825AF_Release,
	.unlocked_ioctl = OV8825AF_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = OV8825AF_Ioctl,
#endif
};

inline static int Register_OV8825AF_CharDrv(void)
{
	struct device *vcm_device = NULL;

	/* Allocate char driver no. */
	if (alloc_chrdev_region(&g_OV8825AF_devno, 0, 1, OV8825AF_DRVNAME)) {
		OV8825AFDB("[OV8825AF] Allocate device no failed\n");

		return -EAGAIN;
	}
	/* Allocate driver */
	g_pOV8825AF_CharDrv = cdev_alloc();

	if (NULL == g_pOV8825AF_CharDrv) {
		unregister_chrdev_region(g_OV8825AF_devno, 1);

		OV8825AFDB("[OV8825AF] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pOV8825AF_CharDrv, &g_stOV8825AF_fops);

	g_pOV8825AF_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pOV8825AF_CharDrv, g_OV8825AF_devno, 1)) {
		OV8825AFDB("[OV8825AF] Attatch file operation failed\n");

		unregister_chrdev_region(g_OV8825AF_devno, 1);

		return -EAGAIN;
	}

	actuator_class = class_create(THIS_MODULE, "actuatordrvOV8825AF");
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);
		OV8825AFDB("Unable to create class, err = %d\n", ret);
		return ret;
	}

	vcm_device = device_create(actuator_class, NULL, g_OV8825AF_devno, NULL, OV8825AF_DRVNAME);

	if (NULL == vcm_device) {
		return -EIO;
	}

	return 0;
}

inline static void Unregister_OV8825AF_CharDrv(void)
{
	/* Release char driver */
	cdev_del(g_pOV8825AF_CharDrv);

	unregister_chrdev_region(g_OV8825AF_devno, 1);

	device_destroy(actuator_class, g_OV8825AF_devno);

	class_destroy(actuator_class);
}

/* //////////////////////////////////////////////////////////////////// */

static int OV8825AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int OV8825AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id OV8825AF_i2c_id[] = { {OV8825AF_DRVNAME, 0}, {} };

struct i2c_driver OV8825AF_i2c_driver = {
	.probe = OV8825AF_i2c_probe,
	.remove = OV8825AF_i2c_remove,
	.driver.name = OV8825AF_DRVNAME,
	.id_table = OV8825AF_i2c_id,
};

#if 0
static int OV8825AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, OV8825AF_DRVNAME);
	return 0;
}
#endif
static int OV8825AF_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/* Kirby: add new-style driver {*/
static int OV8825AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	OV8825AFDB("[OV8825AF] Attach I2C\n");

	/* Kirby: add new-style driver { */
	g_pstOV8825AF_I2Cclient = client;

	/* g_pstOV8825AF_I2Cclient->addr = g_pstOV8825AF_I2Cclient->addr >> 1; */

	/* Register char driver */
	i4RetValue = Register_OV8825AF_CharDrv();

	if (i4RetValue) {

		OV8825AFDB("[OV8825AF] register char device failed!\n");

		return i4RetValue;
	}

	spin_lock_init(&g_OV8825AF_SpinLock);

	OV8825AFDB("[OV8825AF] Attached!!\n");

	return 0;
}

static int OV8825AF_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&OV8825AF_i2c_driver);
}

static int OV8825AF_remove(struct platform_device *pdev)
{
	i2c_del_driver(&OV8825AF_i2c_driver);
	return 0;
}

static int OV8825AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int OV8825AF_resume(struct platform_device *pdev)
{
	return 0;
}

/* platform structure */
static struct platform_driver g_stOV8825AF_Driver = {
	.probe = OV8825AF_probe,
	.remove = OV8825AF_remove,
	.suspend = OV8825AF_suspend,
	.resume = OV8825AF_resume,
	.driver = {
		   .name = "lens_actuatorOV8825AF",
		   .owner = THIS_MODULE,
		   }
};
static struct platform_device g_stOV8825AF_device = {
    .name = "lens_actuatorOV8825AF",
    .id = 0,
    .dev = {}
};
static int __init OV8825AF_i2C_init(void)
{
	i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
  if(platform_device_register(&g_stOV8825AF_device)){
    OV8825AFDB("failed to register AF driver\n");
    return -ENODEV;
  }
	if (platform_driver_register(&g_stOV8825AF_Driver)) {
		OV8825AFDB("failed to register OV8825AF driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit OV8825AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stOV8825AF_Driver);
}
module_init(OV8825AF_i2C_init);
module_exit(OV8825AF_i2C_exit);

MODULE_DESCRIPTION("OV8825AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");
