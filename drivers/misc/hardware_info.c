#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/hardware_info.h>

static char hardwareinfo_name[HARDWARE_MAX_ITEM][HARDWARE_MAX_ITEM_LONGTH];

char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];
int hardwareinfo_set_prop(int cmd, const char *name)
{
	if (cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
		return -EPERM;
	strcpy(hardwareinfo_name[cmd], name);

	return 0;
}
EXPORT_SYMBOL_GPL(hardwareinfo_set_prop);

static long hardwareinfo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, hardwareinfo_num;
	void __user *data = (void __user *)arg;

	switch (cmd) {
	case HARDWARE_LCD_GET:
		hardwareinfo_set_prop(HARDWARE_LCD, Lcm_name);
		hardwareinfo_num = HARDWARE_LCD;
		break;
	case HARDWARE_TP_GET:
		hardwareinfo_num = HARDWARE_TP;
		break;
	case HARDWARE_FLASH_GET:
		hardwareinfo_num = HARDWARE_FLASH;
		break;
	case HARDWARE_FRONT_CAM_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM;
		break;
	case HARDWARE_BACK_CAM_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM;
		break;
	case HARDWARE_BT_GET:
		hardwareinfo_set_prop(HARDWARE_BT, "Qualcomm-msm8916");
		hardwareinfo_num = HARDWARE_BT;
		break;
	case HARDWARE_WIFI_GET:
		hardwareinfo_set_prop(HARDWARE_WIFI, "Qualcomm-msm8916");
		hardwareinfo_num = HARDWARE_WIFI;
		break;
	case HARDWARE_ACCELEROMETER_GET:
		hardwareinfo_num = HARDWARE_ACCELEROMETER;
		break;
	case HARDWARE_ALSPS_GET:
		hardwareinfo_num = HARDWARE_ALSPS;
		break;
	case HARDWARE_GYROSCOPE_GET:
		hardwareinfo_num = HARDWARE_GYROSCOPE;
		break;
	case HARDWARE_MAGNETOMETER_GET:
		hardwareinfo_num = HARDWARE_MAGNETOMETER;
		break;
	case HARDWARE_GPS_GET:
		hardwareinfo_set_prop(HARDWARE_GPS, "Qualcomm-msm8916");
	    hardwareinfo_num = HARDWARE_GPS;
		break;
	case HARDWARE_FM_GET:
		hardwareinfo_set_prop(HARDWARE_FM, "Qualcomm-msm8916");
	    hardwareinfo_num = HARDWARE_FM;
		break;
	case HARDWARE_BACK_CAM_MOUDULE_ID_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM_MOUDULE_ID;
		break;
	case HARDWARE_FRONT_CAM_MODULE_ID_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM_MOUDULE_ID;
		break;
	case HARDWARE_BATTERY_ID_GET:
		hardwareinfo_num = HARDWARE_BATTERY_ID;
		break;
	case HARDWARE_BOARD_ID_GET:
		hardwareinfo_num = HARDWARE_BOARD_ID;
	break;
	default:
		ret = -EINVAL;
		goto err_out;
	}
	memset(data, 0, HARDWARE_MAX_ITEM_LONGTH);
	if (copy_to_user(data, hardwareinfo_name[hardwareinfo_num], strlen(hardwareinfo_name[hardwareinfo_num]))) {
		ret =  -EINVAL;
	}
err_out:
	return ret;
}

static struct file_operations hardwareinfo_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hardwareinfo_ioctl,
};

static struct miscdevice hardwareinfo_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hardwareinfo",
	.fops = &hardwareinfo_fops,
};

static int __init hardwareinfo_init_module(void)
{
	int ret, i;

	for (i = 0; i < HARDWARE_MAX_ITEM; i++)
		strcpy(hardwareinfo_name[i], "NULL");

	ret = misc_register(&hardwareinfo_device);
	if (ret < 0) {
		return -ENODEV;
	}
	return 0;
}

static void __exit hardwareinfo_exit_module(void)
{
	misc_deregister(&hardwareinfo_device);
}

module_init(hardwareinfo_init_module);
module_exit(hardwareinfo_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ming He <heming@wingtech.com>");
