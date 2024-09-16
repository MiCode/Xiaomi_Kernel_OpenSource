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

#include <mtk_wcn_cmb_stub.h>
#include <linux/platform_device.h>

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-DETECT]"

#include "wmt_detect.h"
#include "wmt_gpio.h"
#include "wmt_dev.h"

#if MTK_WCN_REMOVE_KO
#include "conn_drv_init.h"
#endif
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define WMT_DETECT_MAJOR 154
#define WMT_DETECT_DEV_NUM 1
#define WMT_DETECT_DRVIER_NAME "mtk_wcn_detect"
#define WMT_DETECT_DEVICE_NAME "wmtdetect"

struct class *pDetectClass;
struct device *pDetectDev;
static int gWmtDetectMajor = WMT_DETECT_MAJOR;
static struct cdev gWmtDetectCdev;
int gWmtDetectDbgLvl = WMT_DETECT_LOG_INFO;
static ENUM_WMT_CHIP_TYPE g_chip_type = WMT_CHIP_TYPE_INVALID;

static int wmt_detect_open(struct inode *inode, struct file *file)
{
	WMT_DETECT_PR_INFO("open major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	return 0;
}

static int wmt_detect_close(struct inode *inode, struct file *file)
{
	WMT_DETECT_PR_INFO("close major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	return 0;
}

static ssize_t wmt_detect_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	WMT_DETECT_PR_INFO(" ++\n");
	WMT_DETECT_PR_INFO(" --\n");

	return 0;
}

ssize_t wmt_detect_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	WMT_DETECT_PR_INFO(" ++\n");
	WMT_DETECT_PR_INFO(" --\n");

	return 0;
}

static long wmt_detect_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	WMT_DETECT_PR_INFO("cmd (%d),arg(%ld)\n", cmd, arg);

	switch (cmd) {
	case COMBO_IOCTL_GET_CHIP_ID:
		/*just get chipid from sdio-detect module */
		/*check if external combo chip exists or not */
		/*if yes, just return combo chip id */
		/*if no, get soc chipid */
		retval = mtk_wcn_wmt_chipid_query();
		break;

	case COMBO_IOCTL_SET_CHIP_ID:
		WMT_DETECT_PR_INFO("chipid(%ld)\n", arg);
		mtk_wcn_wmt_set_chipid(arg);
		wmt_detect_set_chip_type(arg);
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
		/*get soc chipid by HAL interface */
		break;

	case COMBO_IOCTL_GET_ADIE_CHIP_ID:
		retval = wmt_plat_get_adie_chipid();
		break;

	case COMBO_IOCTL_MODULE_CLEANUP:
		retval = sdio_detect_exit();
		break;

	case COMBO_IOCTL_DO_MODULE_INIT:
#if (MTK_WCN_REMOVE_KO)
		/*deinit SDIO-DETECT module */
		WMT_DETECT_PR_INFO("built-in mode\n");
		retval = do_connectivity_driver_init(arg);
#else
		WMT_DETECT_PR_INFO("kernel object mode\n");
		retval = mtk_wcn_common_drv_init();
#endif
		break;

	default:
		WMT_DETECT_PR_WARN("unknown cmd (%d)\n", cmd);
		retval = 0;
		break;
	}
	return retval;
}
#ifdef CONFIG_COMPAT
static long WMT_compat_detect_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	WMT_DETECT_PR_INFO("cmd (%d)\n", cmd);
	ret = wmt_detect_unlocked_ioctl(filp, cmd, arg);
	return ret;
}
#endif
const struct file_operations gWmtDetectFops = {
	.open = wmt_detect_open,
	.release = wmt_detect_close,
	.read = wmt_detect_read,
	.write = wmt_detect_write,
	.unlocked_ioctl = wmt_detect_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = WMT_compat_detect_ioctl,
#endif
};

int wmt_detect_ext_chip_pwr_on(void)
{
	/*pre power on external chip */
	/* wmt_plat_pwr_ctrl(FUNC_ON); */
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	WMT_DETECT_PR_INFO("++\n");
	if (wmt_detect_chip_pwr_ctrl(1) != 0)
		return -2;
	if (wmt_detect_sdio_pwr_ctrl(1) != 0)
		return -3;
	return 0;
#else
	WMT_DETECT_PR_INFO("combo chip is not supported\n");
	return -1;
#endif
}

int wmt_detect_ext_chip_pwr_off(void)
{
	/*pre power off external chip */
	/* wmt_plat_pwr_ctrl(FUNC_OFF); */
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	WMT_DETECT_PR_INFO("--\n");
	wmt_detect_sdio_pwr_ctrl(0);
	return wmt_detect_chip_pwr_ctrl(0);
#else
	WMT_DETECT_PR_INFO("combo chip is not supported\n");
	return 0;
#endif
}

int wmt_detect_ext_chip_detect(void)
{
	int iRet = -1;
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	unsigned int chipId = -1;
	/*if there is no external combo chip, return -1 */
	int bgfEintStatus = -1;

	WMT_DETECT_PR_INFO("++\n");
	/*wait for a stable time */
	msleep(20);

	/*read BGF_EINT_PIN status */
	bgfEintStatus = wmt_detect_read_ext_cmb_status();

	if (bgfEintStatus == 0) {
		/*external chip does not exist */
		WMT_DETECT_PR_INFO("external combo chip not detected\n");
		iRet = -2;
	} else if (bgfEintStatus == 1) {
		/*combo chip exists */
		WMT_DETECT_PR_INFO("external combo chip detected\n");

		/*detect chipid by sdio_detect module */
		chipId = sdio_detect_query_chipid(1);
		if (hif_sdio_is_chipid_valid(chipId) >= 0)
			WMT_DETECT_PR_INFO("valid external combo chip id (0x%x)\n", chipId);
		else
			WMT_DETECT_PR_INFO("invalid external combo chip id (0x%x)\n", chipId);
		iRet = 0;
	} else {
		/*Error exists */
		WMT_DETECT_PR_ERR("error happens when detecting combo chip\n");
		iRet = -3;
	}
	WMT_DETECT_PR_INFO("--\n");
	/*return 0 */
#endif
	return iRet;
	/*todo: if there is external combo chip, power on chip return 0 */
}

#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
static int wmt_detect_probe(struct platform_device *pdev)
{
	int ret = 0;

	WMT_DETECT_PR_INFO("platform name: %s\n", pdev->name);
	ret = wmt_gpio_init(pdev);
	if (-1 == ret)
		WMT_DETECT_PR_ERR("gpio init fail ret:%d\n", ret);
	return ret;
}

static int wmt_detect_remove(struct platform_device *pdev)
{
	wmt_gpio_deinit();
	return 0;
}
#endif

int wmt_detect_set_chip_type(int chip_id)
{
	switch (chip_id) {
	case 0x6620:
	case 0x6628:
	case 0x6630:
	case 0x6632:
		g_chip_type = WMT_CHIP_TYPE_COMBO;
		break;
	case -1:
		break;
	default:
		g_chip_type = WMT_CHIP_TYPE_SOC;
		break;
	}
	return 0;
}
ENUM_WMT_CHIP_TYPE wmt_detect_get_chip_type(void)
{
	return g_chip_type;
}


#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
static const struct of_device_id wmt_detect_match[] = {
	{ .compatible = "mediatek,connectivity-combo", },
	{}
};
MODULE_DEVICE_TABLE(of, wmt_detect_match);

static struct platform_driver wmt_detect_driver = {
	.probe = wmt_detect_probe,
	.remove = wmt_detect_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "mediatek,connectivity-combo",
		.of_match_table = wmt_detect_match,
	},
};
#endif

/*module_platform_driver(wmt_detect_driver);*/
static int wmt_detect_driver_init(void)
{
	dev_t devID = MKDEV(gWmtDetectMajor, 0);
	int cdevErr = -1;
	int ret = -1;

	/*init SDIO-DETECT module */
	sdio_detect_init();

#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	ret = platform_driver_register(&wmt_detect_driver);
	if (ret)
		WMT_DETECT_PR_ERR("platform driver register fail ret:%d\n", ret);
#endif

	ret = register_chrdev_region(devID, WMT_DETECT_DEV_NUM, WMT_DETECT_DRVIER_NAME);
	if (ret) {
		WMT_DETECT_PR_ERR("fail to register chrdev\n");
		goto err0;
	}

	cdev_init(&gWmtDetectCdev, &gWmtDetectFops);
	gWmtDetectCdev.owner = THIS_MODULE;

	cdevErr = cdev_add(&gWmtDetectCdev, devID, WMT_DETECT_DEV_NUM);
	if (cdevErr) {
		WMT_DETECT_PR_ERR("cdev_add() fails (%d)\n", cdevErr);
		goto err1;
	}

	pDetectClass = class_create(THIS_MODULE, WMT_DETECT_DEVICE_NAME);
	if (IS_ERR(pDetectClass)) {
		WMT_DETECT_PR_ERR("class create fail, error code(%ld)\n", PTR_ERR(pDetectClass));
		goto err1;
	}

	pDetectDev = device_create(pDetectClass, NULL, devID, NULL, WMT_DETECT_DEVICE_NAME);
	if (IS_ERR(pDetectDev)) {
		WMT_DETECT_PR_ERR("device create fail, error code(%ld)\n", PTR_ERR(pDetectDev));
		goto err2;
	}

	WMT_DETECT_PR_INFO("driver(major %d) installed success\n", gWmtDetectMajor);

	return 0;

err2:

	if (pDetectClass) {
		class_destroy(pDetectClass);
		pDetectClass = NULL;
	}

err1:

	if (cdevErr == 0)
		cdev_del(&gWmtDetectCdev);

	if (ret == 0) {
		unregister_chrdev_region(devID, WMT_DETECT_DEV_NUM);
		gWmtDetectMajor = -1;
	}

err0:
	sdio_detect_exit();

#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	platform_driver_unregister(&wmt_detect_driver);
#endif

	return ret ? ret : -1;
}

static void wmt_detect_driver_exit(void)
{
	dev_t dev = MKDEV(gWmtDetectMajor, 0);

	mtk_wcn_common_drv_exit();

	if (pDetectDev) {
		device_destroy(pDetectClass, dev);
		pDetectDev = NULL;
	}

	if (pDetectClass) {
		class_destroy(pDetectClass);
		pDetectClass = NULL;
	}

	cdev_del(&gWmtDetectCdev);
	unregister_chrdev_region(dev, WMT_DETECT_DEV_NUM);

	sdio_detect_exit();

#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	if (wmt_detect_driver.driver.p)
		platform_driver_unregister(&wmt_detect_driver);
#endif

	WMT_DETECT_PR_INFO("done\n");
}

module_init(wmt_detect_driver_init);
module_exit(wmt_detect_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhiguo.Niu & Chaozhong.Liang @ MBJ/WCNSE/SS1");

module_param(gWmtDetectMajor, uint, 0);
