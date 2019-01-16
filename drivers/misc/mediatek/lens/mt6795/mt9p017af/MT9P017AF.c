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
#include "MT9P017AF.h"
#include "../camera/kd_camera_hw.h"
/* #include "kd_cust_lens.h" */

/* #include <mach/mt6573_pll.h> */
/* #include <mach/mt6573_gpt.h> */
/* #include <mach/mt6573_gpio.h> */
#include <linux/xlog.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define MT9P017AF_DRVNAME "MT9P017AF"
#define MT9P017AF_VCM_WRITE_ID           0x18

#define MT9P017AF_DEBUG
#ifdef MT9P017AF_DEBUG
#define MT9P017AFDB pr_debug
#else
#define MT9P017AFDB(x, ...)
#endif

static spinlock_t g_MT9P017AF_SpinLock;
/* Kirby: remove old-style driver
static unsigned short g_pu2Normal_MT9P017AF_i2c[] = {MT9P017AF_VCM_WRITE_ID , I2C_CLIENT_END};
static unsigned short g_u2Ignore_MT9P017AF = I2C_CLIENT_END;

static struct i2c_client_address_data g_stMT9P017AF_Addr_data = {
    .normal_i2c = g_pu2Normal_MT9P017AF_i2c,
    .probe = &g_u2Ignore_MT9P017AF,
    .ignore = &g_u2Ignore_MT9P017AF
};*/

static struct i2c_client *g_pstMT9P017AF_I2Cclient;

static dev_t g_MT9P017AF_devno;
static struct cdev *g_pMT9P017AF_CharDrv;
static struct class *actuator_class;

static int g_s4MT9P017AF_Opened;
static long g_i4MotorStatus;
static long g_i4Dir;
static long g_i4Position;
static unsigned long g_u4MT9P017AF_INF;
static unsigned long g_u4MT9P017AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;
/* static struct work_struct g_stWork;     // --- Work queue --- */
/* static XGPT_CONFIG    g_GPTconfig;            // --- Interrupt Config --- */


extern s32 mt_set_gpio_mode(u32 u4Pin, u32 u4Mode);
extern s32 mt_set_gpio_out(u32 u4Pin, u32 u4PinOut);
extern s32 mt_set_gpio_dir(u32 u4Pin, u32 u4Dir);

extern void MT9P017MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para);
extern kal_uint16 MT9P017MIPI_read_cmos_sensor(kal_uint32 addr);


static int s4MT9P017AF_ReadReg(unsigned short *a_pu2Result)
{
	/*int  i4RetValue = 0;
	   char pBuff[2];

	   i4RetValue = i2c_master_recv(g_pstMT9P017AF_I2Cclient, pBuff , 2);

	   if (i4RetValue < 0)
	   {
	   MT9P017AFDB("[MT9P017AF] I2C read failed!!\n");
	   return -1;
	   } */

	*a_pu2Result = MT9P017MIPI_read_cmos_sensor(0x30f2) << 2;

	return 0;
}

static int s4MT9P017AF_WriteReg(u16 a_u2Data)
{
	/* int  i4RetValue = 0;

	   char puSendCmd[2] = {(char)(a_u2Data >> 4) , (char)((a_u2Data & 0xF) << 4)};

	   //mt_set_gpio_out(97,1);
	   i4RetValue = i2c_master_send(g_pstMT9P017AF_I2Cclient, puSendCmd, 2);
	   //mt_set_gpio_out(97,0);

	   if (i4RetValue < 0)
	   {
	   MT9P017AFDB("[MT9P017AF] I2C send failed!!\n");
	   return -1;
	   }
	 */
	MT9P017AFDB("s4MT9P017AF_WriteReg =0x%x\n", a_u2Data);

	a_u2Data = a_u2Data >> 2;

	MT9P017MIPI_write_cmos_sensor(0x30f2, a_u2Data);

	return 0;
}



inline static int getMT9P017AFInfo(__user stMT9P017AF_MotorInfo * pstMotorInfo)
{
	stMT9P017AF_MotorInfo stMotorInfo;
	stMotorInfo.u4MacroPosition = g_u4MT9P017AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4MT9P017AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	if (g_i4MotorStatus == 1) {
		stMotorInfo.bIsMotorMoving = TRUE;
	} else {
		stMotorInfo.bIsMotorMoving = FALSE;
	}

	if (g_s4MT9P017AF_Opened >= 1) {
		stMotorInfo.bIsMotorOpen = TRUE;
	} else {
		stMotorInfo.bIsMotorOpen = FALSE;
	}

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stMT9P017AF_MotorInfo))) {
		MT9P017AFDB("[MT9P017AF] copy to user failed when getting motor information\n");
	}

	return 0;
}

inline static int moveMT9P017AF(unsigned long a_u4Position)
{
	if ((a_u4Position > g_u4MT9P017AF_MACRO) || (a_u4Position < g_u4MT9P017AF_INF)) {
		MT9P017AFDB("[MT9P017AF] out of range\n");
		return -EINVAL;
	}

	if (g_s4MT9P017AF_Opened == 1) {
		unsigned short InitPos;

		if (s4MT9P017AF_ReadReg(&InitPos) == 0) {
			MT9P017AFDB("[MT9P017AF] Init Pos %6d\n", InitPos);

			g_u4CurrPosition = (unsigned long)InitPos;
		} else {
			g_u4CurrPosition = 0;
		}

		g_s4MT9P017AF_Opened = 2;
	}

	if (g_u4CurrPosition < a_u4Position) {
		g_i4Dir = 1;
	} else if (g_u4CurrPosition > a_u4Position) {
		g_i4Dir = -1;
	} else {
		return 0;
	}

	if (1) {
		g_i4Position = (long)g_u4CurrPosition;
		g_u4TargetPosition = a_u4Position;

		if (g_i4Dir == 1) {
			/* if ((g_u4TargetPosition - g_u4CurrPosition)<60) */
			{
				g_i4MotorStatus = 0;
				if (s4MT9P017AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
					g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
				} else {
					MT9P017AFDB
					    ("[MT9P017AF] set I2C failed when moving the motor\n");
					g_i4MotorStatus = -1;
				}
			}
			/* else */
			/* { */
			/* g_i4MotorStatus = 1; */
			/* } */
		} else if (g_i4Dir == -1) {
			/* if ((g_u4CurrPosition - g_u4TargetPosition)<60) */
			{
				g_i4MotorStatus = 0;
				if (s4MT9P017AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
					g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
				} else {
					MT9P017AFDB
					    ("[MT9P017AF] set I2C failed when moving the motor\n");
					g_i4MotorStatus = -1;
				}
			}
			/* else */
			/* { */
			/* g_i4MotorStatus = 1; */
			/* } */
		}
	} else {
		g_i4Position = (long)g_u4CurrPosition;
		g_u4TargetPosition = a_u4Position;
		g_i4MotorStatus = 1;
	}

	return 0;
}

inline static int setMT9P017AFInf(unsigned long a_u4Position)
{
	g_u4MT9P017AF_INF = a_u4Position;
	return 0;
}

inline static int setMT9P017AFMacro(unsigned long a_u4Position)
{
	g_u4MT9P017AF_MACRO = a_u4Position;
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
static long MT9P017AF_Ioctl(struct file *a_pstFile,
			    unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case MT9P017AFIOC_G_MOTORINFO:
		i4RetValue = getMT9P017AFInfo((__user stMT9P017AF_MotorInfo *) (a_u4Param));
		break;

	case MT9P017AFIOC_T_MOVETO:
		i4RetValue = moveMT9P017AF(a_u4Param);
		break;

	case MT9P017AFIOC_T_SETINFPOS:
		i4RetValue = setMT9P017AFInf(a_u4Param);
		break;

	case MT9P017AFIOC_T_SETMACROPOS:
		i4RetValue = setMT9P017AFMacro(a_u4Param);
		break;

	default:
		MT9P017AFDB("[MT9P017AF] No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/*
static void MT9P017AF_WORK(struct work_struct *work)
{
    g_i4Position += (25 * g_i4Dir);

    if ((g_i4Dir == 1) && (g_i4Position >= (long)g_u4TargetPosition))
	{
	g_i4Position = (long)g_u4TargetPosition;
	g_i4MotorStatus = 0;
    }

    if ((g_i4Dir == -1) && (g_i4Position <= (long)g_u4TargetPosition))
    {
	g_i4Position = (long)g_u4TargetPosition;
	g_i4MotorStatus = 0;
    }

    if(s4MT9P017AF_WriteReg((unsigned short)g_i4Position) == 0)
    {
	g_u4CurrPosition = (unsigned long)g_i4Position;
    }
    else
    {
	MT9P017AFDB("[MT9P017AF] set I2C failed when moving the motor\n");
	g_i4MotorStatus = -1;
    }
}

static void MT9P017AF_ISR(UINT16 a_input)
{
	if (g_i4MotorStatus == 1)
	{
		schedule_work(&g_stWork);
	}
}
*/
/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
/* 3.Update f_op pointer. */
/* 4.Fill data structures into private_data */
/* CAM_RESET */
static int MT9P017AF_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_MT9P017AF_SpinLock);

	if (g_s4MT9P017AF_Opened) {
		spin_unlock(&g_MT9P017AF_SpinLock);
		MT9P017AFDB("[MT9P017AF] the device is opened\n");
		return -EBUSY;
	}

	g_s4MT9P017AF_Opened = 1;

	spin_unlock(&g_MT9P017AF_SpinLock);

	/* --- Config Interrupt --- */
	/* g_GPTconfig.num = XGPT7; */
	/* g_GPTconfig.mode = XGPT_REPEAT; */
	/* g_GPTconfig.clkDiv = XGPT_CLK_DIV_1;//32K */
	/* g_GPTconfig.u4Compare = 32*2; // 2ms */
	/* g_GPTconfig.bIrqEnable = TRUE; */

	/* XGPT_Reset(g_GPTconfig.num); */
	/* XGPT_Init(g_GPTconfig.num, MT9P017AF_ISR); */

	/* if (XGPT_Config(g_GPTconfig) == FALSE) */
	/* { */
	/* MT9P017AFDB("[MT9P017AF] ISR Config Fail\n"); */
	/* return -EPERM; */
	/* } */

	/* XGPT_Start(g_GPTconfig.num); */

	/* --- WorkQueue --- */
	/* INIT_WORK(&g_stWork,MT9P017AF_WORK); */

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int MT9P017AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	unsigned int cnt = 0;

	if (g_s4MT9P017AF_Opened) {
		moveMT9P017AF(g_u4MT9P017AF_INF);

		while (g_i4MotorStatus) {
			msleep(1);
			cnt++;
			if (cnt > 1000) {
				break;
			}
		}

		spin_lock(&g_MT9P017AF_SpinLock);

		g_s4MT9P017AF_Opened = 0;

		spin_unlock(&g_MT9P017AF_SpinLock);

		/* hwPowerDown(CAMERA_POWER_VCAM_A,"kd_camera_hw"); */

		/* XGPT_Stop(g_GPTconfig.num); */
	}

	return 0;
}

static const struct file_operations g_stMT9P017AF_fops = {
	.owner = THIS_MODULE,
	.open = MT9P017AF_Open,
	.release = MT9P017AF_Release,
	.unlocked_ioctl = MT9P017AF_Ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = MT9P017AF_Ioctl,
#endif
};

inline static int Register_MT9P017AF_CharDrv(void)
{
	struct device *vcm_device = NULL;

	/* Allocate char driver no. */
	if (alloc_chrdev_region(&g_MT9P017AF_devno, 0, 1, MT9P017AF_DRVNAME)) {
		MT9P017AFDB("[MT9P017AF] Allocate device no failed\n");

		return -EAGAIN;
	}
	/* Allocate driver */
	g_pMT9P017AF_CharDrv = cdev_alloc();

	if (NULL == g_pMT9P017AF_CharDrv) {
		unregister_chrdev_region(g_MT9P017AF_devno, 1);

		MT9P017AFDB("[MT9P017AF] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pMT9P017AF_CharDrv, &g_stMT9P017AF_fops);

	g_pMT9P017AF_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pMT9P017AF_CharDrv, g_MT9P017AF_devno, 1)) {
		MT9P017AFDB("[MT9P017AF] Attatch file operation failed\n");

		unregister_chrdev_region(g_MT9P017AF_devno, 1);

		return -EAGAIN;
	}

	actuator_class = class_create(THIS_MODULE, "actuatordrvMT9P017AF");
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);
		MT9P017AFDB("Unable to create class, err = %d\n", ret);
		return ret;
	}

	vcm_device =
	    device_create(actuator_class, NULL, g_MT9P017AF_devno, NULL, MT9P017AF_DRVNAME);

	if (NULL == vcm_device) {
		return -EIO;
	}

	return 0;
}

inline static void Unregister_MT9P017AF_CharDrv(void)
{
	/* Release char driver */
	cdev_del(g_pMT9P017AF_CharDrv);

	unregister_chrdev_region(g_MT9P017AF_devno, 1);

	device_destroy(actuator_class, g_MT9P017AF_devno);

	class_destroy(actuator_class);
}

/* //////////////////////////////////////////////////////////////////// */
/* Kirby: remove old-style driver
static int MT9P017AF_i2c_attach(struct i2c_adapter * a_pstAdapter);
static int MT9P017AF_i2c_detach_client(struct i2c_client * a_pstClient);
static struct i2c_driver MT9P017AF_i2c_driver = {
    .driver = {
    .name = MT9P017AF_DRVNAME,
    },
    //.attach_adapter = MT9P017AF_i2c_attach,
    //.detach_client = MT9P017AF_i2c_detach_client
};*/

/* Kirby: add new-style driver { */
static int MT9P017AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int MT9P017AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int MT9P017AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id MT9P017AF_i2c_id[] = { {MT9P017AF_DRVNAME, 0}, {} };
static unsigned short force[] =
    { IMG_SENSOR_I2C_GROUP_ID, MT9P017AF_VCM_WRITE_ID, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };
static struct i2c_client_address_data addr_data = {.forces = forces, };

struct i2c_driver MT9P017AF_i2c_driver = {
	.probe = MT9P017AF_i2c_probe,
	.remove = MT9P017AF_i2c_remove,
	.detect = MT9P017AF_i2c_detect,
	.driver.name = MT9P017AF_DRVNAME,
	.id_table = MT9P017AF_i2c_id,
	.address_data = &addr_data,
};

static int MT9P017AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, MT9P017AF_DRVNAME);
	return 0;
}

static int MT9P017AF_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/* Kirby: } */


/* Kirby: remove old-style driver
int MT9P017AF_i2c_foundproc(struct i2c_adapter * a_pstAdapter, int a_i4Address, int a_i4Kind)
*/
/* Kirby: add new-style driver {*/
static int MT9P017AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
/* Kirby: } */
{
	int i4RetValue = 0;

	MT9P017AFDB("[MT9P017AF] Attach I2C\n");

	/* Kirby: remove old-style driver
	   //Check I2C driver capability
	   if (!i2c_check_functionality(a_pstAdapter, I2C_FUNC_SMBUS_BYTE_DATA))
	   {
	   MT9P017AFDB("[MT9P017AF] I2C port cannot support the format\n");
	   return -EPERM;
	   }

	   if (!(g_pstMT9P017AF_I2Cclient = kzalloc(sizeof(struct i2c_client), GFP_KERNEL)))
	   {
	   return -ENOMEM;
	   }

	   g_pstMT9P017AF_I2Cclient->addr = a_i4Address;
	   g_pstMT9P017AF_I2Cclient->adapter = a_pstAdapter;
	   g_pstMT9P017AF_I2Cclient->driver = &MT9P017AF_i2c_driver;
	   g_pstMT9P017AF_I2Cclient->flags = 0;

	   strncpy(g_pstMT9P017AF_I2Cclient->name, MT9P017AF_DRVNAME, I2C_NAME_SIZE);

	   if(i2c_attach_client(g_pstMT9P017AF_I2Cclient))
	   {
	   kfree(g_pstMT9P017AF_I2Cclient);
	   }
	 */
	/* Kirby: add new-style driver { */
	g_pstMT9P017AF_I2Cclient = client;
	/* Kirby: } */

	/* Register char driver */
	i4RetValue = Register_MT9P017AF_CharDrv();

	if (i4RetValue) {

		MT9P017AFDB("[MT9P017AF] register char device failed!\n");

		/* Kirby: remove old-style driver
		   kfree(g_pstMT9P017AF_I2Cclient); */

		return i4RetValue;
	}

	spin_lock_init(&g_MT9P017AF_SpinLock);

	MT9P017AFDB("[MT9P017AF] Attached!!\n");

	return 0;
}

/* Kirby: remove old-style driver
static int MT9P017AF_i2c_attach(struct i2c_adapter * a_pstAdapter)
{

    if(a_pstAdapter->id == 0)
    {
	 return i2c_probe(a_pstAdapter, &g_stMT9P017AF_Addr_data ,  MT9P017AF_i2c_foundproc);
    }

    return -1;

}

static int MT9P017AF_i2c_detach_client(struct i2c_client * a_pstClient)
{
    int i4RetValue = 0;

    Unregister_MT9P017AF_CharDrv();

    //detach client
    i4RetValue = i2c_detach_client(a_pstClient);
    if(i4RetValue)
    {
	dev_err(&a_pstClient->dev, "Client deregistration failed, client not detached.\n");
	return i4RetValue;
    }

    kfree(i2c_get_clientdata(a_pstClient));

    return 0;
}*/

static int MT9P017AF_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&MT9P017AF_i2c_driver);
}

static int MT9P017AF_remove(struct platform_device *pdev)
{
	i2c_del_driver(&MT9P017AF_i2c_driver);
	return 0;
}

static int MT9P017AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
/* int retVal = 0; */
/* retVal = hwPowerDown(MT6516_POWER_VCAM_A,MT9P017AF_DRVNAME); */

	return 0;
}

static int MT9P017AF_resume(struct platform_device *pdev)
{
/*
    if(TRUE != hwPowerOn(MT6516_POWER_VCAM_A, VOL_2800,MT9P017AF_DRVNAME))
    {
	MT9P017AFDB("[MT9P017AF] failed to resume MT9P017AF\n");
	return -EIO;
    }
*/
	return 0;
}

/* platform structure */
static struct platform_driver g_stMT9P017AF_Driver = {
	.probe = MT9P017AF_probe,
	.remove = MT9P017AF_remove,
	.suspend = MT9P017AF_suspend,
	.resume = MT9P017AF_resume,
	.driver = {
		   .name = "lens_actuatorMT9P017AF",
		   .owner = THIS_MODULE,
		   }
};
static struct platform_device g_stMT9P017AF_device = {
    .name = "lens_actuatorMT9P017AF",
    .id = 0,
    .dev = {}
};
static int __init MT9P017AF_i2C_init(void)
{
	if(platform_device_register(&g_stMT9P017AF_device)){
    MT9P017AFDB("failed to register AF driver\n");
    return -ENODEV;
	}
	if (platform_driver_register(&g_stMT9P017AF_Driver)) {
		MT9P017AFDB("failed to register MT9P017AF driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit MT9P017AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stMT9P017AF_Driver);
}
module_init(MT9P017AF_i2C_init);
module_exit(MT9P017AF_i2C_exit);

MODULE_DESCRIPTION("MT9P017AF lens module driver");
MODULE_AUTHOR("Gipi Lin <Gipi.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");
