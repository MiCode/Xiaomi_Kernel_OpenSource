#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/proc_fs.h>

#include <linux/hardware_info.h>

#include <linux/platform_device.h>

char Ctp_name[HARDWARE_MAX_ITEM_LONGTH];
char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
char Sar_name[HARDWARE_MAX_ITEM_LONGTH];//bug 417945 , add sar info, chenrongli.wt, 20181218
char board_id[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
char smart_pa_name[HARDWARE_MAX_ITEM_LONGTH];//Bug 493560  tsx.wt 20191112 add for smart pa ic info
char hardware_id[HARDWARE_MAX_ITEM_LONGTH];
char nfc_version[HARDWARE_MAX_ITEM_LONGTH];
char fuelgauge_name[HARDWARE_MAX_ITEM_LONGTH];//Bug493560,lili5.wt,ADD,20191112,Add fuel gauge information in factory mode
static char hardwareinfo_name[HARDWARE_MAX_ITEM][HARDWARE_MAX_ITEM_LONGTH];
static char hardwareinfo_camera[HARDWARE_CAMERA_MASK_MAX][HARDWARE_MAX_ITEM_LONGTH];

char* hardwareinfo_items[HARDWARE_MAX_ITEM] =
{
	"LCD",
	"TP",
	"MEMORY",
	"CAM_FRONT",
	"CAM_BACK",
	"BT",
	"WIFI",
	"GSENSOR",
	"PLSENSOR",
	"GYROSENSOR",
	"MSENSOR",
	"SAR", //bug 417945 , add sar info, chenrongli.wt, 20181218
	"GPS",
	"FM",
	"BATTERY",
	"CAM_M_BACK",
	"CAM_M_FRONT",
	"BOARD_ID",
	"HARDWARE_ID"
};


int hardwareinfo_set_prop(int cmd, const char *name)
{
	if(cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
		return -1;

	strcpy(hardwareinfo_name[cmd], name);

	return 0;
}

int __weak tid_hardware_info_get(char *buf, int size)
{
	snprintf(buf, size, "touch info interface is not ready\n");

	return 0;
}

EXPORT_SYMBOL_GPL(hardwareinfo_set_prop);

//Bug 432505 njm@wt, 20191011 start
//for mtk platform
static char* boardid_get(void)
{
	char* s1= "";
	char* s2="not found";
        char *ptr =NULL;

	s1 = strstr(saved_command_line, "board_id=");
	if(!s1) {
		printk("board_id not found in cmdline\n");
		return s2;
	}
	s1 += strlen("board_id=");
	ptr=s1;
	while(*ptr != ' ' && *ptr != '\0') {
		ptr++;
	}
	strncpy(board_id, s1, ptr-s1);
	board_id[ptr-s1]='\0';
	s1 = board_id;
	//printk("board_id found in cmdline : %s\n", board_id);

	return s1;
}

static char* hwid_get(void)
{
	char* s1= "";
	char* s2="not found";
	char *ptr =NULL;

	s1 = strstr(saved_command_line, "hw_id=");
	if(!s1) {
		printk("hw_id not found in cmdline\n");
		return s2;
	}
	s1 += strlen("hw_id=");
	ptr=s1;
	while(*ptr != ' ' && *ptr != '\0') {
		ptr++;
	}
	strncpy(hardware_id, (const char *)s1,ptr-s1);
	hardware_id[ptr-s1]='\0';
	s1 = hardware_id;
	//printk("hw_id found in cmdline s1=: %s\n", s1);
	//Bug 438050 njm@wt, 20191011 end

	return s1;
}
//Bug 432505 njm@wt, 20191011 end

//provide camera data sending and reveiving
bool camerainfo_centre(int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *data = (void __user *)arg;

	if(cmd >= HARDWARE_CAMERA_MASK_MAX || cmd < HARDWARE_BACK_CAM_M ) {
		pr_err("camera info error cmd:%d  ",cmd);
		return -EINVAL;
	}

	if(!strcmp(hardwareinfo_camera[cmd],"NULL" )) {
		pr_info("copy from user");
		if(copy_from_user(hardwareinfo_camera[cmd], data, HARDWARE_MAX_ITEM_LONGTH)) {
			pr_err("%d camera data copy_from_user fail",cmd);
			ret =  -EINVAL;
		}
	} else {
		pr_info("copy to user");
		if (copy_to_user(data, hardwareinfo_camera[cmd], HARDWARE_MAX_ITEM_LONGTH)) {
			pr_err("%d camera data copy_from_user fail",cmd);
			ret =  -EINVAL;
		}
	}
	return ret;
}

static long hardwareinfo_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	int ret = 0, hardwareinfo_num = HARDWARE_MAX_ITEM;
	void __user *data = (void __user *)arg;
	//char info[HARDWARE_MAX_ITEM_LONGTH];

	switch (cmd) {
	case HARDWARE_LCD_GET:
		hardwareinfo_set_prop(HARDWARE_LCD, Lcm_name);//req  wuzhenzhen.wt 20140901 add for hardware info
		hardwareinfo_num = HARDWARE_LCD;
		break;
	case HARDWARE_TP_GET:
		hardwareinfo_set_prop(HARDWARE_TP, Ctp_name);
		hardwareinfo_num = HARDWARE_TP;
		//tid_hardware_info_get(hardwareinfo_name[hardwareinfo_num],
		//				ARRAY_SIZE(hardwareinfo_name[0]));
		break;
	case HARDWARE_BACK_CAM:
		ret = camerainfo_centre(HARDWARE_BACK_CAM_M,arg);
		goto set_ok;
	case HARDWARE_FRONT_CAM:
		ret = camerainfo_centre(HARDWARE_FRONT_CAM_M,arg);
		goto set_ok;
	case HARDWARE_ULTRA_CAM:
		ret = camerainfo_centre(HARDWARE_ULTRA_CAM_M,arg);
		goto set_ok;
	case HARDWARE_DEPTH_CAM:
		ret = camerainfo_centre(HARDWARE_DEPTH_CAM_M,arg);
		goto set_ok;
	case HARDWARE_MACRO_CAM:
		ret = camerainfo_centre(HARDWARE_MACRO_CAM_M,arg);
		goto set_ok;
	case HARDWARE_BACK_CAM_MODULE_ID:
		ret = camerainfo_centre(HARDWARE_BACK_CAM_MODULE_ID_M,arg);
		goto set_ok;
	case HARDWARE_FRONT_CAM_MDULE_ID:
		ret = camerainfo_centre(HARDWARE_FRONT_CAM_MDULE_ID_M,arg);
		goto set_ok;
	case HARDWARE_ULTRA_CAM_MODULE_ID:
		ret = camerainfo_centre(HARDWARE_ULTRA_CAM_MODULE_ID_M,arg);
		goto set_ok;
	case HARDWARE_DEPTH_CAM_MODULE_ID:
		ret = camerainfo_centre(HARDWARE_DEPTH_CAM_MODULE_ID_M,arg);
		goto set_ok;
	case HARDWARE_MACRO_CAM_MODULE_ID:
		ret = camerainfo_centre(HARDWARE_MACRO_CAM_MODULE_ID_M,arg);
		goto set_ok;
	case HARDWARE_BACK_CAM_EFUSEID:
		ret = camerainfo_centre(HARDWARE_BACK_CAM_EFUSEID_M,arg);
		goto set_ok;
	case HARDWARE_FRONT_CAM_EFUSEID:
		ret = camerainfo_centre(HARDWARE_FRONT_CAM_EFUSEID_M,arg);
		goto set_ok;
	case HARDWARE_ULTRA_CAM_EFUSEID:
		ret = camerainfo_centre(HARDWARE_ULTRA_CAM_EFUSEID_M,arg);
		goto set_ok;
	case HARDWARE_DEPTH_CAM_EFUSEID:
		ret = camerainfo_centre(HARDWARE_DEPTH_CAM_EFUSEID_M,arg);
		goto set_ok;
	case HARDWARE_MACRO_CAM_EFUSEID:
		ret = camerainfo_centre(HARDWARE_MACRO_CAM_EFUSEID_M,arg);
		goto set_ok;
	case HARDWARE_BACK_CAM_SENSORID:
		ret = camerainfo_centre(HARDWARE_BACK_CAM_SENSORID_M,arg);
		goto set_ok;
	case HARDWARE_FRONT_CAM_SENSORID:
		ret = camerainfo_centre(HARDWARE_FRONT_CAM_SENSORID_M,arg);
		goto set_ok;
	case HARDWARE_ULTRA_CAM_SENSORID:
		ret = camerainfo_centre(HARDWARE_ULTRA_CAM_SENSORID_M,arg);
		goto set_ok;
	case HARDWARE_DEPTH_CAM_SENSORID:
		ret = camerainfo_centre(HARDWARE_DEPTH_CAM_SENSORID_M,arg);
		goto set_ok;
	case HARDWARE_MACRO_CAM_SENSORID:
		ret = camerainfo_centre(HARDWARE_MACRO_CAM_SENSORID_M,arg);
		goto set_ok;
	case HARDWARE_BT_GET:
		hardwareinfo_set_prop(HARDWARE_BT, "MT6631");
		hardwareinfo_num = HARDWARE_BT;
		break;
	case HARDWARE_WIFI_GET:
		hardwareinfo_set_prop(HARDWARE_WIFI, "MT6631");
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
    case HARDWARE_NFC_GET:
		hardwareinfo_set_prop(HARDWARE_NFC, nfc_version);
		hardwareinfo_num = HARDWARE_NFC;
		break;
	/*bug 417945 , add sar info, chenrongli.wt, 20181218, begin*/
	case HARDWARE_SAR_GET:
		//hardwareinfo_set_prop(HARDWARE_SAR, Sar_name);
		hardwareinfo_num = HARDWARE_SAR;
		break;
	/*bug 417945 , add sar info, chenrongli.wt, 20181218, end*/
	case HARDWARE_GPS_GET:
		hardwareinfo_set_prop(HARDWARE_GPS, "MT6631");
	    hardwareinfo_num = HARDWARE_GPS;
		break;
	case HARDWARE_FM_GET:
		hardwareinfo_set_prop(HARDWARE_FM, "MT6631");
	    hardwareinfo_num = HARDWARE_FM;
		break;
	case HARDWARE_BATTERY_ID_GET:
		hardwareinfo_num = HARDWARE_BATTERY_ID;
		break;
	//+bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	case HARDWARE_CHARGER_IC_INFO_GET:
		hardwareinfo_num = HARDWARE_CHARGER_IC_INFO;
		break;
	//-bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	case HARDWARE_SUB_CHARGER_MASTER_IC_INFO_GET:
		hardwareinfo_num = HARDWARE_SUB_CHARGER_MASTER_IC_INFO;
		break;
	case HARDWARE_SUB_CHARGER_SLAVE_IC_INFO_GET:
		hardwareinfo_num = HARDWARE_SUB_CHARGER_SLAVE_IC_INFO;
		break;
	/*Bug 493560 tsx.wt add smart pa hw info 20191112 begin */
	case HARDWARE_SMARTPA_IC_GET:
		hardwareinfo_set_prop(HARDWARE_SMART_PA_ID, smart_pa_name);
		hardwareinfo_num = HARDWARE_SMART_PA_ID;
		break;
	/*Bug 493560 end */
	case HARDWARE_BOARD_ID_GET:
		//Bug 432505 njm@wt, 20190314 start
		boardid_get();
		//Bug 432505 njm@wt, 20190314 end
		hardwareinfo_set_prop(HARDWARE_BOARD_ID, board_id);
		hardwareinfo_num = HARDWARE_BOARD_ID;
		break;
		//Bug 432505 njm@wt, 20190314 start
	case HARDWARE_HARDWARE_ID_GET:
		hwid_get();
		hardwareinfo_set_prop(HARDWARE_HARDWARE_ID, hardware_id);
		hardwareinfo_num = HARDWARE_HARDWARE_ID;
		break;
		//Bug 432505 njm@wt, 20190314 end
	//+Bug493560,lili5.wt,ADD,20191112,Add fuel gauge information in factory mode
	case HARDWARE_BMS_GAUGE_GET:
		//hardwareinfo_set_prop(HARDWARE_BMS_GAUGE_ID, fuelgauge_name);
		hardwareinfo_num = HARDWARE_BMS_GAUGE_ID;
		break;
	//-Bug493560,lili5.wt,ADD,20191112,Add fuel gauge information in factory mode
	default:
		ret = -EINVAL;
		goto err_out;
	}
	//memset(data, 0, HARDWARE_MAX_ITEM_LONGTH);//clear the buffer
	if (copy_to_user(data, hardwareinfo_name[hardwareinfo_num], strlen(hardwareinfo_name[hardwareinfo_num]))){
		//printk("%s, copy to usr error\n", __func__);
		ret =  -EINVAL;
	}
set_ok:
err_out:
	return ret;
}

static ssize_t show_boardinfo(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buffer[HARDWARE_MAX_ITEM_LONGTH];
	int buf_size = 0;

	for(i = 0; i < HARDWARE_MAX_ITEM; i++)
	{
		memset(temp_buffer, 0, HARDWARE_MAX_ITEM_LONGTH);
		if(i == HARDWARE_LCD)
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], Lcm_name);
		}
		else if(i == HARDWARE_BT || i == HARDWARE_WIFI || i == HARDWARE_GPS || i == HARDWARE_FM)
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], "Qualcomm");
		}
		else
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], hardwareinfo_name[i]);
		}
		strcat(buf, temp_buffer);
		buf_size +=strlen(temp_buffer);
	}

	return buf_size;
}
static DEVICE_ATTR(boardinfo, S_IRUGO, show_boardinfo, NULL);


static int boardinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;

	printk("%s: start\n", __func__);

	rc = device_create_file(dev, &dev_attr_boardinfo);
	if (rc < 0)
		return rc;

	dev_info(dev, "%s: ok\n", __func__);

	return 0;
}

static int boardinfo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_boardinfo);
	dev_info(&pdev->dev, "%s\n", __func__);
	return 0;
}


static struct file_operations hardwareinfo_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.unlocked_ioctl = hardwareinfo_ioctl,
	.compat_ioctl = hardwareinfo_ioctl,
};

static struct miscdevice hardwareinfo_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hardwareinfo",
	.fops = &hardwareinfo_fops,
};

#if 0
static struct of_device_id boardinfo_of_match[] = {
	{ .compatible = "wt:boardinfo", },
	{}
};
#endif

static struct platform_driver boardinfo_driver = {
	.driver = {
		.name	= "boardinfo",
		.owner	= THIS_MODULE,
		//.of_match_table = boardinfo_of_match,
	},
	.probe		= boardinfo_probe,
	.remove		= boardinfo_remove,
};



static int __init hardwareinfo_init_module(void)
{
	int ret, i;

	for(i = 0; i < HARDWARE_MAX_ITEM; i++)
		strcpy(hardwareinfo_name[i], "NULL");

	for(i = 0; i < HARDWARE_CAMERA_MASK_MAX; i++)
		strcpy(hardwareinfo_camera[i], "NULL");


	ret = misc_register(&hardwareinfo_device);
	if(ret < 0){
		return -ENODEV;
	}

	ret = platform_driver_register(&boardinfo_driver);
	if(ret != 0)
	{
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
