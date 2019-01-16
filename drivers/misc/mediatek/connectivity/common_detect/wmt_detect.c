/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <mach/mtk_wcn_cmb_stub.h>



#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-DETECT]"

#include "wmt_detect.h"

#if MTK_WCN_REMOVE_KO
#include "conn_drv_init.h"
#endif



#define WMT_DETECT_MAJOR 154
#define WMT_DETECT_DEV_NUM 1
#define WMT_DETECT_DRVIER_NAME "mtk_wcn_detect"
#define WMT_DETECT_DEVICE_NAME "wmtdetect"

struct class *pDetectClass = NULL;
struct device *pDetectDev = NULL;
static int gWmtDetectMajor = WMT_DETECT_MAJOR;
static struct cdev gWmtDetectCdev;
unsigned int gWmtDetectDbgLvl = WMT_DETECT_LOG_INFO;

#ifdef MTK_WCN_SOC_CHIP_SUPPORT
extern unsigned int wmt_plat_get_soc_chipid(void);
#else
extern unsigned int wmt_plat_get_soc_chipid(void)
{
	WMT_DETECT_INFO_FUNC("no soc chip supported, due to MTK_WCN_SOC_CHIP_SUPPORT is not set.\n");
	return -1;
}
#endif

static int wmt_detect_open(struct inode *inode, struct file *file)
{
	WMT_DETECT_INFO_FUNC("open major %d minor %d (pid %d)\n",
    imajor(inode),
    iminor(inode),
    current->pid
    );

	return 0;
}

static int wmt_detect_close(struct inode *inode, struct file *file)
{
    WMT_DETECT_INFO_FUNC("close major %d minor %d (pid %d)\n",
        imajor(inode),
        iminor(inode),
        current->pid
        );


    return 0;
}

static ssize_t wmt_detect_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)

{
    WMT_DETECT_INFO_FUNC(" ++\n");
	WMT_DETECT_INFO_FUNC(" --\n");

    return 0;
}

ssize_t
wmt_detect_write (
    struct file *filp,
    const char __user *buf,
    size_t count,
    loff_t *f_pos
    )
{
	WMT_DETECT_INFO_FUNC(" ++\n");
	WMT_DETECT_INFO_FUNC(" --\n");

    return 0;
}

static long wmt_detect_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int retval = 0;

    WMT_DETECT_INFO_FUNC("cmd (%d),arg(%ld)\n", cmd,arg);

	switch(cmd){
		case COMBO_IOCTL_GET_CHIP_ID:
			/*just get chipid from sdio-detect module*/
			/*check if external combo chip exists or not*/
			/*if yes, just return combo chip id*/
			/*if no, get soc chipid*/
			retval = mtk_wcn_wmt_chipid_query();
		break;
		
		case COMBO_IOCTL_SET_CHIP_ID:
			mtk_wcn_wmt_set_chipid(arg);
			
		break;
		
		case COMBO_IOCTL_EXT_CHIP_PWR_ON:
			retval = wmt_detect_ext_chip_pwr_on();
		break;
		
		case COMBO_IOCTL_EXT_CHIP_DETECT:
			retval = wmt_detect_ext_chip_detect();
		break;
		
		case COMBO_IOCTL_EXT_CHIP_PWR_OFF:
			retval = wmt_detect_ext_chip_pwr_off();
		break;
		
		case COMBO_IOCTL_DO_SDIO_AUDOK:
			retval = sdio_detect_do_autok(arg);
		break;
			
		case COMBO_IOCTL_GET_SOC_CHIP_ID:
			retval = wmt_plat_get_soc_chipid();
				/*get soc chipid by HAL interface*/
		break;
			
		case COMBO_IOCTL_MODULE_CLEANUP:
			#if (MTK_WCN_REMOVE_KO)
			/*deinit SDIO-DETECT module*/
				retval = sdio_detect_exit();
			#else
				WMT_DETECT_INFO_FUNC("no MTK_WCN_REMOVE_KO defined\n");
			#endif
		break;
			
		case COMBO_IOCTL_DO_MODULE_INIT:
			#if (MTK_WCN_REMOVE_KO)
			/*deinit SDIO-DETECT module*/
				retval = do_connectivity_driver_init(arg);
			#else
				WMT_DETECT_INFO_FUNC("no MTK_WCN_REMOVE_KO defined\n");
			#endif
			break;

		default:
			WMT_DETECT_WARN_FUNC("unknown cmd (%d)\n", cmd);
			retval = 0;
		break;
	}
	return retval;
}

struct file_operations gWmtDetectFops = {
    .open = wmt_detect_open,
    .release = wmt_detect_close,
    .read = wmt_detect_read,
    .write = wmt_detect_write,
    .unlocked_ioctl = wmt_detect_unlocked_ioctl,
};

int wmt_detect_ext_chip_pwr_on(void)
{
	/*pre power on external chip*/
	//wmt_plat_pwr_ctrl(FUNC_ON);
	WMT_DETECT_INFO_FUNC("++\n");
	if (0 != wmt_detect_chip_pwr_ctrl(1))
	{
		return -1;
	}
	if (0 != wmt_detect_sdio_pwr_ctrl(1))
	{
		return -2;
	}
	return 0;
}

int wmt_detect_ext_chip_pwr_off(void)
{
	/*pre power off external chip*/
	//wmt_plat_pwr_ctrl(FUNC_OFF);
	WMT_DETECT_INFO_FUNC("--\n");
	wmt_detect_sdio_pwr_ctrl(0);
	return wmt_detect_chip_pwr_ctrl(0);
}

int wmt_detect_ext_chip_detect(void)
{
	int iRet = -1;
	unsigned int chipId = -1;
	/*if there is no external combo chip, return -1*/
	int bgfEintStatus = -1;
	WMT_DETECT_INFO_FUNC("++\n");
	/*wait for a stable time*/
	msleep (10);
	
	/*read BGF_EINT_PIN status*/
	bgfEintStatus = wmt_detect_read_ext_cmb_status();;
	
	if (0 == bgfEintStatus)
	{
		/*external chip does not exist*/
		WMT_DETECT_INFO_FUNC("external combo chip not detected\n");
	}
	else if (1 == bgfEintStatus)
	{
		/*combo chip exists*/
		WMT_DETECT_INFO_FUNC("external combo chip detected\n");
		
		/*detect chipid by sdio_detect module*/
		chipId = sdio_detect_query_chipid(1);
		if (0 <= hif_sdio_is_chipid_valid(chipId))
		{
			WMT_DETECT_INFO_FUNC("valid external combo chip id (0x%x)\n", chipId);
		}
		else
		{
			WMT_DETECT_INFO_FUNC("invalid external combo chip id (0x%x)\n", chipId);
		}
		iRet = 0;
	}
	else
	{
		/*Error exists*/
		WMT_DETECT_ERR_FUNC("error happens when detecting combo chip\n");
	}
	WMT_DETECT_INFO_FUNC("--\n");
	/*return 0*/
	return iRet;

	/*todo: if there is external combo chip, power on chip return 0*/
}
static int wmt_detect_init(void)
{
    dev_t devID = MKDEV(gWmtDetectMajor, 0);
    int cdevErr = -1;
    int ret = -1;

    ret = register_chrdev_region(devID, WMT_DETECT_DEV_NUM, WMT_DETECT_DRVIER_NAME);
    if (ret) {
        WMT_DETECT_ERR_FUNC("fail to register chrdev\n");
        return ret;
    }

    cdev_init(&gWmtDetectCdev, &gWmtDetectFops);
    gWmtDetectCdev.owner = THIS_MODULE;

    cdevErr = cdev_add(&gWmtDetectCdev, devID, WMT_DETECT_DEV_NUM);
    if (cdevErr) {
        WMT_DETECT_ERR_FUNC("cdev_add() fails (%d) \n", cdevErr);
        goto err1;
    }

	pDetectClass = class_create(THIS_MODULE, WMT_DETECT_DEVICE_NAME);
	if(IS_ERR(pDetectClass))
	{
		WMT_DETECT_ERR_FUNC("class create fail, error code(%ld)\n",PTR_ERR(pDetectClass));
		goto err1;
	}

	pDetectDev = device_create(pDetectClass,NULL,devID,NULL,WMT_DETECT_DEVICE_NAME);
	if(IS_ERR(pDetectDev))
	{
		WMT_DETECT_ERR_FUNC("device create fail, error code(%ld)\n",PTR_ERR(pDetectDev));
		goto err2;
	}
	
  WMT_DETECT_INFO_FUNC("driver(major %d) installed success\n", gWmtDetectMajor);
	
	/*init SDIO-DETECT module*/
	sdio_detect_init();

    return 0;

err2:

	if(pDetectClass)
	{
		class_destroy(pDetectClass);
		pDetectClass = NULL;
	}

err1:

    if (cdevErr == 0) {
        cdev_del(&gWmtDetectCdev);
    }

    if (ret == 0) {
        unregister_chrdev_region(devID, WMT_DETECT_DEV_NUM);
        gWmtDetectMajor = -1;
    }

    WMT_DETECT_ERR_FUNC("fail \n");

    return -1;
}

static void wmt_detect_exit (void)
{
	dev_t dev = MKDEV(gWmtDetectMajor, 0);

	if(pDetectDev)
	{
		device_destroy(pDetectClass, dev);
		pDetectDev = NULL;
	}

	if(pDetectClass)
	{
		class_destroy(pDetectClass);
		pDetectClass = NULL;
	}
	
	cdev_del(&gWmtDetectCdev);
	unregister_chrdev_region(dev, WMT_DETECT_DEV_NUM);

#if !(MTK_WCN_REMOVE_KO)
/*deinit SDIO-DETECT module*/
	sdio_detect_exit();
#endif
	
	WMT_DETECT_INFO_FUNC("done\n");
}

module_init(wmt_detect_init);
module_exit(wmt_detect_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhiguo.Niu & Chaozhong.Liang @ MBJ/WCNSE/SS1");

module_param(gWmtDetectMajor, uint, 0);
