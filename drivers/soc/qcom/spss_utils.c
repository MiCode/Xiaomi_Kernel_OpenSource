// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

/*
 * Secure-Processor-SubSystem (SPSS) utilities.
 *
 * This driver provides utilities for the Secure Processor (SP).
 *
 * The SP daemon needs to load different SPSS images based on:
 *
 * 1. Test/Production key used to sign the SPSS image (read fuses).
 * 2. SPSS HW version (selected via Device Tree).
 *
 */

#define pr_fmt(fmt)	"spss_utils [%s]: " fmt, __func__

#include <linux/kernel.h>   /* min() */
#include <linux/module.h>   /* MODULE_LICENSE */
#include <linux/device.h>   /* class_create() */
#include <linux/slab.h>     /* kzalloc() */
#include <linux/fs.h>       /* file_operations */
#include <linux/cdev.h>     /* cdev_add() */
#include <linux/errno.h>    /* EINVAL, ETIMEDOUT */
#include <linux/printk.h>   /* pr_err() */
#include <linux/bitops.h>   /* BIT(x) */
#include <linux/platform_device.h> /* platform_driver_register() */
#include <linux/of.h>       /* of_property_count_strings() */
#include <linux/of_address.h>   /* of_address_to_resource() */
#include <linux/io.h>       /* ioremap_nocache() */
#include <linux/notifier.h>
#include <linux/sizes.h>    /* SZ_4K */
#include <linux/uaccess.h>  /* copy_from_user() */
#include <linux/completion.h>	/* wait_for_completion_timeout() */
#include <linux/reboot.h>	/* kernel_restart() */

#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/secure_buffer.h>     /* VMID_HLOS */

#include <uapi/linux/ioctl.h>       /* ioctl() */
#include <uapi/linux/spss_utils.h>  /* IOCTL to user space */

/* driver name */
#define DEVICE_NAME	"spss_utils"

enum spss_firmware_type {
	SPSS_FW_TYPE_DEV = 'd',
	SPSS_FW_TYPE_TEST = 't',
	SPSS_FW_TYPE_PROD = 'p',
	SPSS_FW_TYPE_NONE = 'z',
};

static enum spss_firmware_type firmware_type = SPSS_FW_TYPE_TEST;
static const char *dev_firmware_name;
static const char *test_firmware_name;
static const char *prod_firmware_name;
static const char *none_firmware_name = "nospss";
static const char *firmware_name = "NA";
static struct device *spss_dev;
static u32 spss_debug_reg_addr; /* SP_SCSR_MBn_SP2CL_GPm(n,m) */
static u32 spss_emul_type_reg_addr; /* TCSR_SOC_EMULATION_TYPE */
static void *iar_notif_handle;
static struct notifier_block *iar_nb;
static bool is_iar_active;
static bool is_ssr_disabled;
/* To differentiate legacy and new generation of hardware supported features*/
static bool is_cmac_and_iar_feature_supported = true;

#define CMAC_SIZE_IN_BYTES (128/8) /* 128 bit = 16 bytes */
#define CMAC_SIZE_IN_DWORDS (CMAC_SIZE_IN_BYTES/sizeof(u32)) /* 4 dwords */

static u32 pil_addr;
static u32 pil_size;

/*
 * The saved fw cmac is stored in file in IAR-DB.
 * It is provided via ioctl from user space spu service.
 */
static u32 saved_fw_cmac[CMAC_SIZE_IN_DWORDS]; /* saved fw cmac */

/*
 * The calculated fw cmac is calculated by SPU PBL.
 * It is read from shared memory and provided back to user space service
 * via device attribute.
 */
static u32 calc_fw_cmac[CMAC_SIZE_IN_DWORDS]; /* calculated pbl fw cmac */

/*
 * The saved apps cmac is stored in file in IAR-DB.
 * It is provided via ioctl from user space spu service.
 */
static u32 saved_apps_cmac[MAX_SPU_UEFI_APPS][CMAC_SIZE_IN_DWORDS];

/*
 * The calculated apps cmac is calculated by SPU firmware.
 * It is read from shared memory and provided back to user space service
 * via device attribute.
 */
static u32 calc_apps_cmac[MAX_SPU_UEFI_APPS][CMAC_SIZE_IN_DWORDS];

static void __iomem *cmac_mem;
static phys_addr_t cmac_mem_addr;
#define  CMAC_MEM_SIZE SZ_4K /* XPU align to 4KB */

#define SPSS_BASE_ADDR_MASK 0xFFFF0000
#define SPSS_RMB_CODE_SIZE_REG_OFFSET 0x1008

#define SPU_EMULATUION (BIT(0) | BIT(1))
#define SPU_PRESENT_IN_EMULATION BIT(0)

/* IOCTL max request size is 1KB */
#define MAX_IOCTL_REQ_SIZE  1024

/* Events notification */
static struct completion spss_events[SPSS_NUM_EVENTS];
static bool spss_events_signaled[SPSS_NUM_EVENTS];

/* Protect from ioctl signal func called by multiple-proc at the same time */
static struct mutex event_lock;

/**
 * struct device state
 */
struct spss_utils_device {
	/* char device info */
	struct cdev *cdev;
	dev_t device_no;
	struct class *driver_class;
	struct device *class_dev;
	struct platform_device *pdev;
};

/* Device State */
static struct spss_utils_device *spss_utils_dev;

/* static functions declaration */
static int spss_set_saved_fw_cmac(u32 *cmac, size_t cmac_size);
static int spss_set_saved_uefi_apps_cmac(void);

static int spss_get_fw_calc_cmac(void);
static int spss_get_apps_calc_cmac(void);

/*==========================================================================*/
/*		Device Sysfs */
/*==========================================================================*/

static ssize_t firmware_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	if (firmware_name == NULL)
		ret = scnprintf(buf, PAGE_SIZE, "%s\n", "unknown");
	else
		ret = scnprintf(buf, PAGE_SIZE, "%s\n", firmware_name);

	return ret;
}

static DEVICE_ATTR_RO(firmware_name);

static ssize_t test_fuse_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	switch (firmware_type) {
	case SPSS_FW_TYPE_DEV:
		ret = scnprintf(buf, PAGE_SIZE, "%s", "dev");
		break;
	case SPSS_FW_TYPE_TEST:
		ret = scnprintf(buf, PAGE_SIZE, "%s", "test");
		break;
	case SPSS_FW_TYPE_PROD:
		ret = scnprintf(buf, PAGE_SIZE, "%s", "prod");
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static DEVICE_ATTR_RO(test_fuse_state);

static ssize_t spss_debug_reg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	void __iomem *spss_debug_reg = NULL;
	u32 val1, val2;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	pr_debug("spss_debug_reg_addr [0x%x].\n", spss_debug_reg_addr);

	spss_debug_reg = ioremap_nocache(spss_debug_reg_addr, sizeof(u32)*2);

	if (!spss_debug_reg) {
		pr_err("can't map debug reg addr\n");
		return -EINVAL;
	}

	val1 = readl_relaxed(spss_debug_reg);
	val2 = readl_relaxed(((char *) spss_debug_reg) + sizeof(u32));

	ret = scnprintf(buf, PAGE_SIZE, "val1 [0x%x] val2 [0x%x]\n",
			val1, val2);

	iounmap(spss_debug_reg);

	return ret;
}

static DEVICE_ATTR_RO(spss_debug_reg);

static ssize_t calc_fw_cmac_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	/* first make sure the calc cmac is updated */
	spss_get_fw_calc_cmac();

	memcpy(buf, calc_fw_cmac, sizeof(calc_fw_cmac));

	return sizeof(calc_fw_cmac);
}

static DEVICE_ATTR_RO(calc_fw_cmac);

static ssize_t calc_apps_cmac_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	/* first make sure the calc cmac is updated */
	spss_get_apps_calc_cmac();

	memcpy(buf, calc_apps_cmac, sizeof(calc_apps_cmac));

	return sizeof(calc_apps_cmac);
}

static DEVICE_ATTR_RO(calc_apps_cmac);

/*--------------------------------------------------------------------------*/
static int spss_create_sysfs(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_firmware_name);
	if (ret < 0) {
		pr_err("failed to create sysfs file for firmware_name.\n");
		return ret;
	}

	ret = device_create_file(dev, &dev_attr_test_fuse_state);
	if (ret < 0) {
		pr_err("failed to create sysfs file for test_fuse_state.\n");
		goto remove_firmware_name;
	}

	ret = device_create_file(dev, &dev_attr_spss_debug_reg);
	if (ret < 0) {
		pr_err("failed to create sysfs file for spss_debug_reg.\n");
		goto remove_test_fuse_state;
	}

	if (!is_cmac_and_iar_feature_supported)
		goto out;

	ret = device_create_file(dev, &dev_attr_calc_fw_cmac);

	if (ret < 0) {
		pr_err("failed to create sysfs file for calc_fw_cmac.\n");
		goto remove_spss_debug_reg;
	}

	ret = device_create_file(dev, &dev_attr_calc_apps_cmac);
	if (ret < 0) {
		pr_err("failed to create sysfs file for calc_apps_cmac.\n");
		goto remove_calc_fw_cmac;
	}

out:
	return 0;

remove_calc_fw_cmac:
		device_remove_file(dev, &dev_attr_calc_fw_cmac);
remove_spss_debug_reg:
		device_remove_file(dev, &dev_attr_spss_debug_reg);
remove_test_fuse_state:
		device_remove_file(dev, &dev_attr_test_fuse_state);
remove_firmware_name:
		device_remove_file(dev, &dev_attr_firmware_name);

	return ret;
}
static void spss_destroy_sysfs(struct device *dev)
{

	if (is_cmac_and_iar_feature_supported) {
		device_remove_file(dev, &dev_attr_calc_apps_cmac);
		device_remove_file(dev, &dev_attr_calc_fw_cmac);
	}
	device_remove_file(dev, &dev_attr_spss_debug_reg);
	device_remove_file(dev, &dev_attr_test_fuse_state);
	device_remove_file(dev, &dev_attr_firmware_name);
}
/*==========================================================================*/
/*  IOCTL */
/*==========================================================================*/
static int spss_wait_for_event(struct spss_ioc_wait_for_event *req)
{
	int ret;
	uint32_t event_id;
	uint32_t timeout_sec;
	long timeleft = 1;

	event_id = req->event_id;
	timeout_sec = req->timeout_sec;

	if (event_id >= SPSS_NUM_EVENTS) {
		pr_err("event_id [%d] invalid\n", event_id);
		return -EINVAL;
	}

	pr_debug("wait for event [%d], timeout_sec [%d]\n",
		event_id, timeout_sec);

	if (timeout_sec) {
		unsigned long jiffies = 0;

		jiffies = msecs_to_jiffies(timeout_sec*1000);
		timeleft = wait_for_completion_interruptible_timeout(
			&spss_events[event_id], jiffies);
		ret = timeleft;
	} else {
		ret = wait_for_completion_interruptible(
			&spss_events[event_id]);
	}

	if (timeleft == 0) {
		pr_err("wait for event [%d] timeout [%d] sec expired\n",
			event_id, timeout_sec);
		req->status = EVENT_STATUS_TIMEOUT;
	} else if (ret < 0) {
		pr_err("wait for event [%d] interrupted. ret [%d]\n",
			event_id, ret);
		req->status = EVENT_STATUS_ABORTED;
		if (ret == -ERESTARTSYS)	/* handle LPM event */
			return ret;
	} else {
		pr_debug("wait for event [%d] completed.\n", event_id);
		req->status = EVENT_STATUS_SIGNALED;
	}

	return 0;
}

static int spss_signal_event(struct spss_ioc_signal_event *req)
{
	uint32_t event_id;

	mutex_lock(&event_lock);

	event_id = req->event_id;

	if (event_id >= SPSS_NUM_EVENTS) {
		pr_err("event_id [%d] invalid\n", event_id);
		mutex_unlock(&event_lock);
		return -EINVAL;
	}

	if (spss_events_signaled[event_id]) {
		pr_err("event_id [%d] already signaled\n", event_id);
		mutex_unlock(&event_lock);
		return -EINVAL;
	}

	pr_debug("signal event [%d]\n", event_id);
	complete_all(&spss_events[event_id]);
	req->status = EVENT_STATUS_SIGNALED;
	spss_events_signaled[event_id] = true;

	mutex_unlock(&event_lock);

	return 0;
}

static int spss_is_event_signaled(struct spss_ioc_is_signaled *req)
{
	uint32_t event_id;

	mutex_lock(&event_lock);

	event_id = req->event_id;

	if (event_id >= SPSS_NUM_EVENTS) {
		pr_err("event_id [%d] invalid\n", event_id);
		mutex_unlock(&event_lock);
		return -EINVAL;
	}

	if (spss_events_signaled[event_id])
		req->status = EVENT_STATUS_SIGNALED;
	else
		req->status = EVENT_STATUS_NOT_SIGNALED;

	mutex_unlock(&event_lock);

	return 0;
}

static int spss_handle_set_fw_and_apps_cmac(struct spss_ioc_set_fw_and_apps_cmac *req)
{
	int ret = 0;
	u32 cmac_buf_size = req->cmac_buf_size;
	void __user *cmac_buf_ptr = u64_to_user_ptr(req->cmac_buf_ptr);
	u32 num_of_cmacs = req->num_of_cmacs;
	/* Saved cmacs of spu firmware and UEFI loaded spu apps */
	u32 fw_and_apps_cmacs[1+MAX_SPU_UEFI_APPS][CMAC_SIZE_IN_DWORDS];

	pr_debug("cmac_buf_size [0x%x].\n", (int) req->cmac_buf_size);
	pr_debug("cmac_buf_ptr  [0x%x].\n", (int) req->cmac_buf_ptr);
	pr_debug("num_of_cmacs  [0x%x].\n", (int) req->num_of_cmacs);

	if (cmac_buf_size != sizeof(fw_and_apps_cmacs)) {
		pr_err("cmac_buf_size [0x%x] invalid.\n", cmac_buf_size);
		return -EINVAL;
	}

	if (num_of_cmacs > (u32)(MAX_SPU_UEFI_APPS+1)) {
		pr_err("num_of_cmacs [0x%x] invalid.\n", num_of_cmacs);
		return -EINVAL;
	}

	/* copy the saved cmacs from user buffer to loacl variable */
	ret = copy_from_user(fw_and_apps_cmacs, cmac_buf_ptr, cmac_buf_size);
	if (ret < 0) {
		pr_err("copy_from_user() from cmac_buf_ptr failed.\n");
		return -EFAULT;
	}

	/* store the saved fw cmac */
	memcpy(saved_fw_cmac, fw_and_apps_cmacs[0],
		sizeof(saved_fw_cmac));

	pr_debug("saved fw cmac: 0x%08x,0x%08x,0x%08x,0x%08x\n",
		saved_fw_cmac[0], saved_fw_cmac[1],
		saved_fw_cmac[2], saved_fw_cmac[3]);

	/* store the saved apps cmac */
	memcpy(saved_apps_cmac, fw_and_apps_cmacs[1],
		sizeof(saved_apps_cmac));

	/*
	 * SPSS is loaded now by UEFI,
	 * so PIL-IAR-callback is not being called on power-up by PIL.
	 * therefore get the saved spu fw cmac and apps cmac from ioctl.
	 * The PIL-IAR-callback shall be called on spss SSR.
	 * The saved cmacs are used on SUBSYS_BEFORE_AUTH_AND_RESET event !
	 */
	spss_set_saved_fw_cmac(saved_fw_cmac, sizeof(saved_fw_cmac));
	spss_set_saved_uefi_apps_cmac();

	pr_debug("completed ok\n");

	return 0;
}

static long spss_utils_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret;
	void *buf = (void *) arg;
	uint8_t data[MAX_IOCTL_REQ_SIZE] = {0};
	size_t size = 0;
	void *req = (void *) data;

	if (buf == NULL) {
		pr_err("invalid ioctl arg\n");
		return -EINVAL;
	}

	size = _IOC_SIZE(cmd);
	if (size && (cmd & IOC_IN)) {
		if (size > sizeof(data)) {
			pr_err("cmd [0x%x] size [0x%x] too large\n",
				cmd, size);
			return -EINVAL;
		}

		if (copy_from_user(data, (void __user *)arg, size)) {
			pr_err("copy_from_user() failed, cmd [0x%x]\n",
				cmd, size);
			return -EFAULT;
		}
	}

	switch (cmd) {
	case SPSS_IOC_SET_FW_AND_APPS_CMAC:
		pr_debug("ioctl [SPSS_IOC_SET_FW_AND_APPS_CMAC]\n");

		if (!is_cmac_and_iar_feature_supported) {
			pr_err("legacy SPSS not support cmac,iar feature.\n");
			return -EINVAL;
		}

		/* spdaemon uses this ioctl only when IAR is active */
		is_iar_active = true;

		if (cmac_mem == NULL) {
			cmac_mem = ioremap_nocache(cmac_mem_addr, CMAC_MEM_SIZE);
			if (!cmac_mem) {
				pr_err("can't map cmac_mem.\n");
				return -EFAULT;
			}
		}

		ret = spss_handle_set_fw_and_apps_cmac(req);
		if (ret < 0)
			return ret;
		break;

	case SPSS_IOC_WAIT_FOR_EVENT:
		/* check input params */
		if (size != sizeof(struct spss_ioc_wait_for_event)) {
			pr_err("cmd [0x%x] invalid size [0x%x]\n", cmd, size);
			return -EINVAL;
		}
		ret = spss_wait_for_event(req);
		copy_to_user((void __user *)arg, data, size);
		if (ret < 0)
			return ret;
		break;

	case SPSS_IOC_SIGNAL_EVENT:
		/* check input params */
		if (size != sizeof(struct spss_ioc_signal_event)) {
			pr_err("cmd [0x%x] invalid size [0x%x]\n", cmd, size);
			return -EINVAL;
		}
		ret = spss_signal_event(req);
		copy_to_user((void __user *)arg, data, size);
		if (ret < 0)
			return ret;
		break;

	case SPSS_IOC_IS_EVENT_SIGNALED:
		/* check input params */
		if (size != sizeof(struct spss_ioc_is_signaled)) {
			pr_err("cmd [0x%x] invalid size [0x%x]\n", cmd, size);
			return -EINVAL;
		}
		ret = spss_is_event_signaled(req);
		copy_to_user((void __user *)arg, data, size);
		if (ret < 0)
			return ret;
		break;

	case SPSS_IOC_SET_SSR_STATE:
		/* check input params */
		if (size != sizeof(uint32_t)) {
			pr_err("cmd [0x%x] invalid size [0x%x]\n", cmd, size);
			return -EINVAL;
		}

		if (is_iar_active) {
			uint32_t tmp = 0;

			memcpy(&tmp, data, sizeof(tmp));
			is_ssr_disabled = (bool) tmp; /* u32 to bool */

			pr_info("SSR disabled state updated to: %d\n",
				 is_ssr_disabled);
		}

		pr_info("is_iar_active [%d] is_ssr_disabled [%d].\n",
			is_iar_active, is_ssr_disabled);
		break;

	default:
		pr_err("invalid ioctl cmd [0x%x]\n", cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}

static const struct file_operations spss_utils_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = spss_utils_ioctl,
	.compat_ioctl = spss_utils_ioctl,
};

static int spss_utils_create_chardev(struct device *dev)
{
	int ret;
	unsigned int baseminor = 0;
	unsigned int count = 1;
	void *priv = (void *) spss_utils_dev;

	spss_utils_dev->cdev =
		kzalloc(sizeof(*spss_utils_dev->cdev), GFP_KERNEL);
	if (!spss_utils_dev->cdev)
		return -ENOMEM;

	/* get device_no */
	ret = alloc_chrdev_region(&spss_utils_dev->device_no, baseminor, count,
				 DEVICE_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}

	spss_utils_dev->driver_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(spss_utils_dev->driver_class)) {
		ret = -ENOMEM;
		pr_err("class_create failed %d\n", ret);
		goto exit_unreg_chrdev_region;
	}

	spss_utils_dev->class_dev =
	    device_create(spss_utils_dev->driver_class, NULL,
				  spss_utils_dev->device_no, priv,
				  DEVICE_NAME);

	if (IS_ERR(spss_utils_dev->class_dev)) {
		pr_err("class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(spss_utils_dev->cdev, &spss_utils_fops);
	spss_utils_dev->cdev->owner = THIS_MODULE;

	ret = cdev_add(spss_utils_dev->cdev,
		       MKDEV(MAJOR(spss_utils_dev->device_no), 0),
		       1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto exit_destroy_device;
	}

	pr_debug("char device created.\n");
	return 0;

exit_destroy_device:
	device_destroy(spss_utils_dev->driver_class, spss_utils_dev->device_no);
exit_destroy_class:
	class_destroy(spss_utils_dev->driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(spss_utils_dev->device_no, 1);
	return ret;
}

static void spss_utils_destroy_chardev(void)
{
	device_destroy(spss_utils_dev->driver_class, spss_utils_dev->device_no);
	class_destroy(spss_utils_dev->driver_class);
	unregister_chrdev_region(spss_utils_dev->device_no, 1);
}

/*==========================================================================*/
/*		Device Tree */
/*==========================================================================*/

/* get the ACTUAL spss PIL firmware size from spu reg */
static int get_pil_size(phys_addr_t base_addr)
{
	u32 spss_code_size_addr = 0;
	void __iomem *spss_code_size_reg = NULL;
	u32 pil_size = 0;

	spss_code_size_addr = base_addr + SPSS_RMB_CODE_SIZE_REG_OFFSET;
	spss_code_size_reg = ioremap_nocache(spss_code_size_addr, sizeof(u32));
	if (!spss_code_size_reg) {
		pr_err("can't map spss_code_size_addr\n");
		return -EINVAL;
	}
	pil_size = readl_relaxed(spss_code_size_reg);
	iounmap(spss_code_size_reg);

	if (pil_size % SZ_4K) {
		pr_err("pil_size [0x%08x] is not 4K aligned.\n", pil_size);
		return -EFAULT;
	}

	return pil_size;
}

/**
 * spss_parse_dt() - Parse Device Tree info.
 */
static int spss_parse_dt(struct device_node *node)
{
	int ret;
	u32 spss_fuse1_addr = 0;
	u32 spss_fuse1_bit = 0;
	u32 spss_fuse1_mask = 0;
	void __iomem *spss_fuse1_reg = NULL;
	u32 spss_fuse2_addr = 0;
	u32 spss_fuse2_bit = 0;
	u32 spss_fuse2_mask = 0;
	void __iomem *spss_fuse2_reg = NULL;
	struct device_node *np;
	struct resource r;
	u32 val1 = 0;
	u32 val2 = 0;
	void __iomem *spss_emul_type_reg = NULL;
	u32 spss_emul_type_val = 0;
	phys_addr_t spss_regs_base_addr = 0;

	ret = of_property_read_string(node, "qcom,spss-dev-firmware-name",
		&dev_firmware_name);
	if (ret < 0) {
		pr_err("can't get dev fw name\n");
		return -EINVAL;
	}

	ret = of_property_read_string(node, "qcom,spss-test-firmware-name",
		&test_firmware_name);
	if (ret < 0) {
		pr_err("can't get test fw name\n");
		return -EINVAL;
	}

	ret = of_property_read_string(node, "qcom,spss-prod-firmware-name",
		&prod_firmware_name);
	if (ret < 0) {
		pr_err("can't get prod fw name\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse1-addr",
		&spss_fuse1_addr);
	if (ret < 0) {
		pr_err("can't get fuse1 addr\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse2-addr",
		&spss_fuse2_addr);
	if (ret < 0) {
		pr_err("can't get fuse2 addr\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse1-bit",
		&spss_fuse1_bit);
	if (ret < 0) {
		pr_err("can't get fuse1 bit\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse2-bit",
		&spss_fuse2_bit);
	if (ret < 0) {
		pr_err("can't get fuse2 bit\n");
		return -EINVAL;
	}


	spss_fuse1_mask = BIT(spss_fuse1_bit);
	spss_fuse2_mask = BIT(spss_fuse2_bit);

	pr_debug("spss fuse1 addr [0x%x] bit [%d]\n",
		(int) spss_fuse1_addr, (int) spss_fuse1_bit);
	pr_debug("spss fuse2 addr [0x%x] bit [%d]\n",
		(int) spss_fuse2_addr, (int) spss_fuse2_bit);

	spss_fuse1_reg = ioremap_nocache(spss_fuse1_addr, sizeof(u32));

	if (!spss_fuse1_reg) {
		pr_err("can't map fuse1 addr\n");
		return -EINVAL;
	}

	spss_fuse2_reg = ioremap_nocache(spss_fuse2_addr, sizeof(u32));

	if (!spss_fuse2_reg) {
		iounmap(spss_fuse1_reg);
		pr_err("can't map fuse2 addr\n");
		return -EINVAL;
	}

	val1 = readl_relaxed(spss_fuse1_reg);
	val2 = readl_relaxed(spss_fuse2_reg);

	pr_debug("spss fuse1 value [0x%08x]\n", (int) val1);
	pr_debug("spss fuse2 value [0x%08x]\n", (int) val2);

	pr_debug("spss fuse1 mask [0x%08x]\n", (int) spss_fuse1_mask);
	pr_debug("spss fuse2 mask [0x%08x]\n", (int) spss_fuse2_mask);

	/**
	 * Set firmware_type based on fuses:
	 *	SPSS_CONFIG_MODE 11:        dev
	 *	SPSS_CONFIG_MODE 01 or 10:  test
	 *	SPSS_CONFIG_MODE 00:        prod
	 */
	if ((val1 & spss_fuse1_mask) && (val2 & spss_fuse2_mask))
		firmware_type = SPSS_FW_TYPE_DEV;
	else if ((val1 & spss_fuse1_mask) || (val2 & spss_fuse2_mask))
		firmware_type = SPSS_FW_TYPE_TEST;
	else
		firmware_type = SPSS_FW_TYPE_PROD;

	iounmap(spss_fuse1_reg);
	iounmap(spss_fuse2_reg);

	pr_debug("firmware_type value [%c]\n", firmware_type);

	ret = of_property_read_u32(node, "qcom,spss-debug-reg-addr",
		&spss_debug_reg_addr);
	if (ret < 0) {
		pr_err("can't get debug regs addr\n");
		return ret;
	}

	ret = of_property_read_u32(node, "qcom,spss-emul-type-reg-addr",
			     &spss_emul_type_reg_addr);
	if (ret < 0) {
		pr_err("can't get spss-emulation-type-reg addr\n");
		return -EINVAL;
	}

	spss_emul_type_reg = ioremap_nocache(spss_emul_type_reg_addr,
					     sizeof(u32));
	if (!spss_emul_type_reg) {
		pr_err("can't map soc-emulation-type reg addr\n");
		return -EINVAL;
	}

	spss_emul_type_val = readl_relaxed(spss_emul_type_reg);

	pr_debug("spss_emul_type value [0x%08x]\n", (int)spss_emul_type_val);
	if (spss_emul_type_val & SPU_EMULATUION) {
		if (spss_emul_type_val & SPU_PRESENT_IN_EMULATION) {
			firmware_type = SPSS_FW_TYPE_TEST;
		} else {
			/* for some emulation platforms SPSS is not present */
			firmware_type = SPSS_FW_TYPE_NONE;
		}
		pr_debug("remap firmware_type value [%c]\n", firmware_type);
	}
	iounmap(spss_emul_type_reg);

	if (!is_cmac_and_iar_feature_supported) {
		pr_info("legacy SPSS not support cmac & iar feature.\n");
		goto out;
	}
	/* PIL-SPSS area */
	np = of_parse_phandle(node, "pil-mem", 0);
	if (!np) {
		pr_err("no pil-mem entry, check pil-addr\n");
		ret = of_property_read_u32(node, "qcom,pil-addr",
			&pil_addr);
		if (ret < 0) {
			pr_err("can't get pil_addr\n");
			return -EFAULT;
		}
	} else {
		ret = of_address_to_resource(np, 0, &r);
		of_node_put(np);
		if (ret)
			return ret;
		pil_addr = (u32)r.start;
	}

	spss_regs_base_addr =
		(spss_debug_reg_addr & SPSS_BASE_ADDR_MASK);
	ret = get_pil_size(spss_regs_base_addr);
	if (ret < 0) {
		pr_err("failed to get pil_size.\n");
		return -EFAULT;
	}
	pil_size = (u32) ret;

	pr_debug("pil_addr [0x%08x].\n", pil_addr);
	pr_debug("pil_size [0x%08x].\n", pil_size);

	/* cmac buffer after spss firmware end */
	cmac_mem_addr = pil_addr + pil_size;
	pr_info("iar_buf_addr [0x%08x].\n", cmac_mem_addr);

	memset(saved_fw_cmac, 0xA5, sizeof(saved_fw_cmac));
	memset(saved_apps_cmac, 0xA5, sizeof(saved_apps_cmac));

out:
	return 0;
}

static int spss_set_saved_fw_cmac(u32 *cmac, size_t cmac_size)
{
	u8 __iomem *reg = NULL;
	int i;

	if (cmac_mem == NULL) {
		pr_err("invalid cmac_mem.\n");
		return -EFAULT;
	}

	reg = cmac_mem;

	for (i = 0; i < cmac_size/sizeof(u32); i++)
		writel_relaxed(cmac[i], reg + i*sizeof(u32));

	pr_debug("saved fw cmac: 0x%08x,0x%08x,0x%08x,0x%08x\n",
		cmac[0], cmac[1], cmac[2], cmac[3]);

	return 0;
}

static int spss_get_fw_calc_cmac(void)
{
	u8 __iomem *reg = NULL;
	int i;
	u32 val;
	u32 cmac[CMAC_SIZE_IN_DWORDS] = {0};

	if (cmac_mem == NULL) {
		pr_err("invalid cmac_mem.\n");
		return -EFAULT;
	}

	reg = cmac_mem; /* IAR buffer base */
	reg += CMAC_SIZE_IN_BYTES; /* skip the saved cmac */

	memset(calc_fw_cmac, 0, sizeof(calc_fw_cmac));

	/* get pbl fw cmac from ddr */
	for (i = 0; i < CMAC_SIZE_IN_DWORDS; i++) {
		val = readl_relaxed(reg);
		calc_fw_cmac[i] = val;
		reg += sizeof(u32);
	}

	/* check for any pattern to mark invalid cmac */
	if (cmac[0] == cmac[1])
		return -EINVAL; /* not valid cmac */

	memcpy(calc_fw_cmac, cmac, sizeof(calc_fw_cmac));

	pr_debug("calc_fw_cmac : 0x%08x,0x%08x,0x%08x,0x%08x\n",
	    calc_fw_cmac[0], calc_fw_cmac[1],
	    calc_fw_cmac[2], calc_fw_cmac[3]);

	return 0;
}

static int spss_get_apps_calc_cmac(void)
{
	u8 __iomem *reg = NULL;
	int i, j;
	u32 val;

	if (cmac_mem == NULL) {
		pr_err("invalid cmac_mem.\n");
		return -EFAULT;
	}

	reg = cmac_mem; /* IAR buffer base */
	reg += CMAC_SIZE_IN_BYTES; /* skip the saved fw cmac */
	reg += CMAC_SIZE_IN_BYTES; /* skip the calc fw cmac */
	reg += CMAC_SIZE_IN_BYTES; /* skip the saved 1st app cmac */

	memset(calc_apps_cmac, 0, sizeof(calc_apps_cmac));

	/* get apps cmac from ddr */
	for (j = 0; j < ARRAY_SIZE(calc_apps_cmac); j++) {
		u32 cmac[CMAC_SIZE_IN_DWORDS] = {0};

		memset(cmac, 0, sizeof(cmac));

		for (i = 0; i < ARRAY_SIZE(cmac); i++) {
			val = readl_relaxed(reg);
			cmac[i] = val;
			reg += sizeof(u32);
		}
		reg += CMAC_SIZE_IN_BYTES; /* skip the saved cmac */

		/* check for any pattern to mark end of cmacs */
		if (cmac[0] == cmac[1])
			break; /* no more valid cmacs */

		memcpy(calc_apps_cmac[j], cmac, sizeof(calc_apps_cmac[j]));

		pr_debug("app [%d] cmac : 0x%08x,0x%08x,0x%08x,0x%08x\n", j,
			calc_apps_cmac[j][0], calc_apps_cmac[j][1],
			calc_apps_cmac[j][2], calc_apps_cmac[j][3]);
	}

	return 0;
}

static int spss_set_saved_uefi_apps_cmac(void)
{
	u8 __iomem *reg = NULL;
	int i, j;
	u32 val;

	if (cmac_mem == NULL) {
		pr_err("invalid cmac_mem.\n");
		return -EFAULT;
	}

	reg = cmac_mem; /* IAR buffer base */
	reg += (2*CMAC_SIZE_IN_BYTES); /* skip the saved and calc fw cmac */

	/* get saved apps cmac from ddr - were written by UEFI spss driver */
	for (j = 0; j < MAX_SPU_UEFI_APPS; j++) {
		if (saved_apps_cmac[j][0] == saved_apps_cmac[j][1])
			break; /* no more cmacs */
		for (i = 0; i < CMAC_SIZE_IN_DWORDS; i++) {
			val = saved_apps_cmac[j][i];
			writel_relaxed(val, reg);
			reg += sizeof(u32);
		}
		reg += CMAC_SIZE_IN_BYTES; /* skip the calc app cmac */

		pr_debug("app[%d] saved cmac: 0x%08x,0x%08x,0x%08x,0x%08x\n",
			j,
			saved_apps_cmac[j][0], saved_apps_cmac[j][1],
			saved_apps_cmac[j][2], saved_apps_cmac[j][3]);
	}

	return 0;
}

static int spss_utils_pil_callback(struct notifier_block *nb,
				  unsigned long code,
				  void *data)
{
	int i, event_id;

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("[SUBSYS_BEFORE_SHUTDOWN] event.\n");
		mutex_lock(&event_lock);
		/* Reset NVM-ready and SPU-ready events */
		for (i = SPSS_EVENT_ID_NVM_READY;
			i <= SPSS_EVENT_ID_SPU_READY; i++) {
			reinit_completion(&spss_events[i]);
			spss_events_signaled[i] = false;
		}
		mutex_unlock(&event_lock);
		pr_debug("reset spss events.\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		pr_debug("[SUBSYS_AFTER_SHUTDOWN] event.\n");
		mutex_lock(&event_lock);
		event_id = SPSS_EVENT_ID_SPU_POWER_DOWN;
		complete_all(&spss_events[event_id]);
		spss_events_signaled[event_id] = true;

		event_id = SPSS_EVENT_ID_SPU_POWER_UP;
		reinit_completion(&spss_events[event_id]);
		spss_events_signaled[event_id] = false;
		mutex_unlock(&event_lock);
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_info("[SUBSYS_BEFORE_POWERUP] event.\n");

		if (!is_cmac_and_iar_feature_supported) {
			pr_info("legacy SPSS not support cmac,iar feature.\n");
			break;
		}
		if (is_iar_active && is_ssr_disabled) {
			pr_err("SPSS SSR disabled, requesting reboot\n");
			kernel_restart("SPSS SSR disabled, requesting reboot");
		}
		break;
	case SUBSYS_AFTER_POWERUP:
		pr_info("[SUBSYS_AFTER_POWERUP] event.\n");
		mutex_lock(&event_lock);
		event_id = SPSS_EVENT_ID_SPU_POWER_UP;
		complete_all(&spss_events[event_id]);
		spss_events_signaled[event_id] = true;

		event_id = SPSS_EVENT_ID_SPU_POWER_DOWN;
		reinit_completion(&spss_events[event_id]);
		spss_events_signaled[event_id] = false;
		mutex_unlock(&event_lock);

		if (!is_cmac_and_iar_feature_supported) {
			pr_info("legacy SPSS not support cmac,iar feature.\n");
			break;
		}
		/*
		 * For IAR-DB-Recovery, read cmac regadless of is_iar_active.
		 * please notice that HYP unmap this area, it is a race.
		 */
		if (cmac_mem == NULL) {
			cmac_mem = ioremap_nocache(cmac_mem_addr, CMAC_MEM_SIZE);
			if (!cmac_mem) {
				pr_err("can't map cmac_mem.\n");
				return -EFAULT;
			}
		}

		spss_get_fw_calc_cmac();
		spss_get_apps_calc_cmac();
		break;
	case SUBSYS_RAMDUMP_NOTIFICATION:
		pr_debug("[SUBSYS_RAMDUMP_NOTIFICATION] event.\n");
		break;
	case SUBSYS_PROXY_VOTE:
		pr_debug("[SUBSYS_PROXY_VOTE] event.\n");
		break;
	case SUBSYS_PROXY_UNVOTE:
		pr_debug("[SUBSYS_PROXY_UNVOTE] event.\n");
		break;
	case SUBSYS_BEFORE_AUTH_AND_RESET:
		pr_debug("[SUBSYS_BEFORE_AUTH_AND_RESET] event.\n");

		if (!is_cmac_and_iar_feature_supported) {
			pr_info("legacy SPSS not support cmac,iar feature.\n");
			break;
		}
		/* do nothing if IAR is not active */
		if (!is_iar_active)
			return NOTIFY_OK;
		/* Called on SSR as spss firmware is loaded by UEFI on boot */
		spss_set_saved_fw_cmac(saved_fw_cmac, sizeof(saved_fw_cmac));
		spss_set_saved_uefi_apps_cmac();
		break;
	default:
		pr_err("unknown code [0x%x] .\n", (int) code);
		break;

	}

	return NOTIFY_OK;
}

/**
 * spss_probe() - initialization sequence
 */
static int spss_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct device_node *np = NULL;
	struct device *dev = &pdev->dev;

	np = pdev->dev.of_node;
	spss_dev = dev;

	platform_set_drvdata(pdev, dev);
	/* Based on flag will differentiate legacy spss
	 * supported features
	 */
	if (of_property_read_bool(pdev->dev.of_node,
				"qcom,no-cmac-and-iar-feature-support")) {
		pr_info("legacy SPSS not support cmac & iar feature.\n");
		is_cmac_and_iar_feature_supported = false;
	}

	ret = spss_parse_dt(np);
	if (ret < 0)
		return ret;

	switch (firmware_type) {
	case SPSS_FW_TYPE_DEV:
		firmware_name = dev_firmware_name;
		break;
	case SPSS_FW_TYPE_TEST:
		firmware_name = test_firmware_name;
		break;
	case SPSS_FW_TYPE_PROD:
		firmware_name = prod_firmware_name;
		break;
	case SPSS_FW_TYPE_NONE:
		firmware_name = none_firmware_name;
		break;
	default:
		pr_err("invalid firmware type %d, sysfs entry not created\n",
			firmware_type);
		return -EINVAL;
	}

	ret = subsystem_set_fwname("spss", firmware_name);
	if (ret < 0) {
		if (ret != -EINVAL)
			pr_err("fail to set firmware name for PIL (%d)\n", ret);
		return -EPROBE_DEFER;
	}

	spss_utils_dev = kzalloc(sizeof(*spss_utils_dev), GFP_KERNEL);
	if (spss_utils_dev == NULL)
		return -ENOMEM;

	ret = spss_utils_create_chardev(dev);
	if (ret < 0)
		return ret;

	ret = spss_create_sysfs(dev);
	if (ret < 0)
		return ret;

	iar_nb = kzalloc(sizeof(*iar_nb), GFP_KERNEL);
	if (!iar_nb)
		return -ENOMEM;

	iar_nb->notifier_call = spss_utils_pil_callback;

	iar_notif_handle = subsys_notif_register_notifier("spss", iar_nb);
	if (IS_ERR_OR_NULL(iar_notif_handle)) {
		pr_err("register fail for IAR notifier\n");
		kfree(iar_nb);
		iar_notif_handle = NULL;
		iar_nb = NULL;
	}

	for (i = 0 ; i < SPSS_NUM_EVENTS; i++) {
		init_completion(&spss_events[i]);
		spss_events_signaled[i] = false;
	}
	mutex_init(&event_lock);

	is_iar_active = false;
	is_ssr_disabled = false;

	pr_info("Probe completed successfully, [%s].\n", firmware_name);

	return 0;
}

static int spss_remove(struct platform_device *pdev)
{
	spss_utils_destroy_chardev();
	spss_destroy_sysfs(spss_dev);

	if (!iar_notif_handle && !iar_nb)
		subsys_notif_unregister_notifier(iar_notif_handle, iar_nb);

	kfree(iar_nb);
	iar_nb = 0;

	kfree(spss_utils_dev);
	spss_utils_dev = 0;

	if (cmac_mem != NULL) {
		iounmap(cmac_mem);
		cmac_mem = NULL;
	}

	return 0;
}

static const struct of_device_id spss_match_table[] = {
	{ .compatible = "qcom,spss-utils", },
	{ },
};

static struct platform_driver spss_driver = {
	.probe = spss_probe,
	.remove = spss_remove,
	.driver = {
		.name = DEVICE_NAME,
		.of_match_table = of_match_ptr(spss_match_table),
	},
};

/*==========================================================================*/
/*		Driver Init/Exit					*/
/*==========================================================================*/
static int __init spss_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&spss_driver);
	if (ret)
		pr_err("register platform driver failed, ret [%d]\n", ret);

	return ret;
}
late_initcall(spss_init); /* start after PIL driver */

static void __exit spss_exit(void)
{
	platform_driver_unregister(&spss_driver);
}
module_exit(spss_exit)

MODULE_SOFTDEP("pre: subsys-pil-tz");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Secure Processor Utilities");
