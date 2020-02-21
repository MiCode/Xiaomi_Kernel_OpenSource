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

#define CMAC_SIZE_IN_BYTES (128/8) /* 128 bit = 16 bytes */
#define CMAC_SIZE_IN_DWORDS (CMAC_SIZE_IN_BYTES/sizeof(u32)) /* 4 dwords */

/* Asym , Crypt , Keym */
#define NUM_UEFI_APPS 3

static u32 pil_addr;
static u32 pil_size;
static u32 cmac_buf[CMAC_SIZE_IN_DWORDS]; /* saved cmac */
static u32 pbl_cmac_buf[CMAC_SIZE_IN_DWORDS]; /* pbl cmac */

static u32 calc_apps_cmac[NUM_UEFI_APPS][CMAC_SIZE_IN_DWORDS];
static u32 saved_apps_cmac[NUM_UEFI_APPS][CMAC_SIZE_IN_DWORDS];

#define FW_AND_APPS_CMAC_SIZE \
	(CMAC_SIZE_IN_DWORDS + NUM_UEFI_APPS*CMAC_SIZE_IN_DWORDS)

static u32 iar_state;
static bool is_iar_enabled;

static void __iomem *cmac_mem;
static size_t cmac_mem_size = SZ_4K; /* XPU align to 4KB */
static phys_addr_t cmac_mem_addr;

#define SPU_EMULATUION (BIT(0) | BIT(1))
#define SPU_PRESENT_IN_EMULATION BIT(2)

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
static int spss_set_fw_cmac(u32 *cmac, size_t cmac_size);
static int spss_get_pbl_and_apps_calc_cmac(void);

static int spss_get_saved_uefi_apps_cmac(void);
static int spss_set_saved_uefi_apps_cmac(void);

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

static ssize_t cmac_buf_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "0x%08x,0x%08x,0x%08x,0x%08x\n",
		cmac_buf[0], cmac_buf[1], cmac_buf[2], cmac_buf[3]);

	return ret;
}

static DEVICE_ATTR_RO(cmac_buf);

static ssize_t iar_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	/* show IAR-STATE from soc fuse */
	ret = snprintf(buf, PAGE_SIZE, "0x%x\n", iar_state);

	return ret;
}

static DEVICE_ATTR_RO(iar_state);

static ssize_t iar_enabled_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "0x%x\n", is_iar_enabled);

	return ret;
}

static DEVICE_ATTR_RO(iar_enabled);

static ssize_t pbl_cmac_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	/* first make sure the pbl cmac is updated */
	spss_get_pbl_and_apps_calc_cmac();

	ret = snprintf(buf, PAGE_SIZE, "0x%08x,0x%08x,0x%08x,0x%08x\n",
	    pbl_cmac_buf[0], pbl_cmac_buf[1], pbl_cmac_buf[2], pbl_cmac_buf[3]);

	return ret;
}

static DEVICE_ATTR_RO(pbl_cmac);

static ssize_t apps_cmac_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	/* first make sure the pbl cmac is updated */
	spss_get_pbl_and_apps_calc_cmac();

	memcpy(buf, calc_apps_cmac, sizeof(calc_apps_cmac));

	return sizeof(calc_apps_cmac);
}

static DEVICE_ATTR_RO(apps_cmac);

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

	ret = device_create_file(dev, &dev_attr_cmac_buf);
	if (ret < 0) {
		pr_err("failed to create sysfs file for cmac_buf.\n");
		goto remove_spss_debug_reg;
	}

	ret = device_create_file(dev, &dev_attr_iar_state);
	if (ret < 0) {
		pr_err("failed to create sysfs file for iar_state.\n");
		goto remove_cmac_buf;
	}

	ret = device_create_file(dev, &dev_attr_iar_enabled);
	if (ret < 0) {
		pr_err("failed to create sysfs file for iar_enabled.\n");
		goto remove_iar_state;
	}

	ret = device_create_file(dev, &dev_attr_pbl_cmac);
	if (ret < 0) {
		pr_err("failed to create sysfs file for pbl_cmac.\n");
		goto remove_iar_enabled;
	}

	ret = device_create_file(dev, &dev_attr_apps_cmac);
	if (ret < 0) {
		pr_err("failed to create sysfs file for apps_cmac.\n");
		goto remove_pbl_cmac;
	}


	return 0;

remove_pbl_cmac:
		device_remove_file(dev, &dev_attr_pbl_cmac);
remove_iar_enabled:
		device_remove_file(dev, &dev_attr_iar_enabled);
remove_iar_state:
		device_remove_file(dev, &dev_attr_iar_state);
remove_cmac_buf:
		device_remove_file(dev, &dev_attr_cmac_buf);
remove_spss_debug_reg:
		device_remove_file(dev, &dev_attr_spss_debug_reg);
remove_test_fuse_state:
		device_remove_file(dev, &dev_attr_test_fuse_state);
remove_firmware_name:
		device_remove_file(dev, &dev_attr_firmware_name);

	return ret;
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

static long spss_utils_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret;
	void *buf = (void *) arg;
	unsigned char data[64] = {0};
	size_t size = 0;
	u32 i = 0;
	/* Saved cmacs of spu firmware and UEFI loaded spu apps */
	u32 fw_and_apps_cmacs[FW_AND_APPS_CMAC_SIZE];
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
	case SPSS_IOC_SET_FW_CMAC:
		if (size != sizeof(fw_and_apps_cmacs)) {
			pr_err("cmd [0x%x] invalid size [0x%x]\n", cmd, size);
			return -EINVAL;
		}

		/* spdaemon uses this ioctl only when IAR is active */
		is_iar_active = true;

		memcpy(fw_and_apps_cmacs, data, sizeof(fw_and_apps_cmacs));
		memcpy(cmac_buf, fw_and_apps_cmacs, sizeof(cmac_buf));

		for (i = 0; i < NUM_UEFI_APPS; ++i) {
			int x = (i+1)*CMAC_SIZE_IN_DWORDS;

			memcpy(saved_apps_cmac[i],
				fw_and_apps_cmacs + x,
				CMAC_SIZE_IN_BYTES);
		}

		/*
		 * SPSS is loaded now by UEFI,
		 * so IAR callback is not being called on power-up by PIL.
		 * therefore read the spu pbl fw cmac and apps cmac from ioctl.
		 * The callback shall be called on spss SSR.
		 */
		pr_debug("read pbl cmac from shared memory\n");
		spss_set_fw_cmac(cmac_buf, sizeof(cmac_buf));
		spss_set_saved_uefi_apps_cmac();
		spss_get_saved_uefi_apps_cmac();
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

	default:
		pr_err("invalid ioctl cmd [0x%x]\n", cmd);
		return -EINVAL;
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

/*==========================================================================*/
/*		Device Tree */
/*==========================================================================*/

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
	/* IAR_FEATURE_ENABLED soc fuse */
	u32 spss_fuse3_addr = 0;
	u32 spss_fuse3_bit = 0;
	u32 spss_fuse3_mask = 0;
	void __iomem *spss_fuse3_reg = NULL;
	/* IAR_STATE soc fuses */
	u32 spss_fuse4_addr = 0;
	u32 spss_fuse4_bit = 0;
	u32 spss_fuse4_mask = 0;
	void __iomem *spss_fuse4_reg = NULL;
	struct device_node *np;
	struct resource r;
	u32 val1 = 0;
	u32 val2 = 0;
	void __iomem *spss_emul_type_reg = NULL;
	u32 spss_emul_type_val = 0;

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
	if ((spss_emul_type_val & SPU_EMULATUION) &&
	    !(spss_emul_type_val & SPU_PRESENT_IN_EMULATION)) {
		/* for some emulation platforms SPSS is not present */
		firmware_type = SPSS_FW_TYPE_NONE;
	}
	iounmap(spss_emul_type_reg);

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

	ret = of_property_read_u32(node, "qcom,pil-size",
		&pil_size);
	if (ret < 0) {
		pr_err("can't get pil_size\n");
		return -EFAULT;
	}

	pr_debug("pil_addr [0x%08x].\n", pil_addr);
	pr_debug("pil_size [0x%08x].\n", pil_size);

	/* cmac buffer after spss firmware end */
	cmac_mem_addr = pil_addr + pil_size;
	pr_info("iar_buf_addr [0x%08x].\n", cmac_mem_addr);

	ret = of_property_read_u32(node, "qcom,spss-fuse3-addr",
		&spss_fuse3_addr);
	if (ret < 0) {
		pr_err("can't get fuse3 addr.\n");
		return -EFAULT;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse3-bit",
		&spss_fuse3_bit);
	if (ret < 0) {
		pr_err("can't get fuse3 bit.\n");
		return -EFAULT;
	}

	spss_fuse3_reg = ioremap_nocache(spss_fuse3_addr, sizeof(u32));

	if (!spss_fuse3_reg) {
		pr_err("can't map fuse3 addr.\n");
		return -EFAULT;
	}

	/* read IAR_FEATURE_ENABLED from soc fuse */
	val1 = readl_relaxed(spss_fuse3_reg);
	spss_fuse3_mask = (1<<spss_fuse3_bit);
	pr_debug("iar_enabled fuse, addr [0x%x] val [0x%x] mask [0x%x].\n",
		spss_fuse3_addr, val1, spss_fuse3_mask);
	if (val1 & spss_fuse3_mask)
		is_iar_enabled = true;
	else
		is_iar_enabled = false;

	memset(cmac_buf, 0xA5, sizeof(cmac_buf));

	ret = of_property_read_u32(node, "qcom,spss-fuse4-addr",
		&spss_fuse4_addr);
	if (ret < 0) {
		pr_err("can't get fuse4 addr.\n");
		return -EFAULT;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse4-bit",
		&spss_fuse4_bit);
	if (ret < 0) {
		pr_err("can't get fuse4 bit.\n");
		return -EFAULT;
	}

	spss_fuse4_reg = ioremap_nocache(spss_fuse4_addr, sizeof(u32));

	if (!spss_fuse4_reg) {
		pr_err("can't map fuse4 addr.\n");
		return -EFAULT;
	}

	val1 = readl_relaxed(spss_fuse4_reg);
	spss_fuse4_mask = (0x07 << spss_fuse4_bit); /* 3 bits */
	pr_debug("IAR_STATE fuse, addr [0x%x] val [0x%x] mask [0x%x].\n",
	spss_fuse4_addr, val1, spss_fuse4_mask);
	val1 = ((val1 & spss_fuse4_mask) >> spss_fuse4_bit) & 0x07;

	iar_state = val1;

	pr_debug("iar_state [%d]\n", iar_state);

	return 0;
}

static int spss_set_fw_cmac(u32 *cmac, size_t cmac_size)
{
	u8 __iomem *reg = NULL;
	int i;

	if (cmac_mem == NULL) {
		cmac_mem = ioremap_nocache(cmac_mem_addr, cmac_mem_size);
		if (!cmac_mem) {
			pr_err("can't map cmac_mem.\n");
			return -EFAULT;
		}
	}

	pr_debug("pil_addr [0x%x]\n", pil_addr);
	pr_debug("pil_size [0x%x]\n", pil_size);
	pr_debug("cmac_mem [%pK]\n", cmac_mem);
	reg = cmac_mem;
	pr_debug("reg [%pK]\n", reg);

	for (i = 0; i < cmac_size/4; i++) {
		writel_relaxed(cmac[i], reg + i*sizeof(u32));
		pr_debug("cmac[%d] [0x%x]\n", i, cmac[i]);
	}
	reg += cmac_size;

	return 0;
}

static int spss_get_pbl_and_apps_calc_cmac(void)
{
	u8 __iomem *reg = NULL;
	int i, j;
	u32 val;

	if (cmac_mem == NULL)
		return -EFAULT;

	reg = cmac_mem; /* IAR buffer base */
	reg += CMAC_SIZE_IN_BYTES; /* skip the saved cmac */
	pr_debug("reg [%pK]\n", reg);

	/* get pbl fw cmac from ddr */
	for (i = 0; i < CMAC_SIZE_IN_DWORDS; i++) {
		val = readl_relaxed(reg);
		pbl_cmac_buf[i] = val;
		reg += sizeof(u32);
	}
	reg += CMAC_SIZE_IN_BYTES; /* skip the saved cmac */

	pr_debug("pbl_cmac_buf : 0x%08x,0x%08x,0x%08x,0x%08x\n",
	    pbl_cmac_buf[0], pbl_cmac_buf[1],
	    pbl_cmac_buf[2], pbl_cmac_buf[3]);

	/* get apps cmac from ddr */
	for (j = 0; j < NUM_UEFI_APPS; j++) {
		for (i = 0; i < CMAC_SIZE_IN_DWORDS; i++) {
			val = readl_relaxed(reg);
			calc_apps_cmac[j][i] = val;
			reg += sizeof(u32);
		}
		reg += CMAC_SIZE_IN_BYTES; /* skip the saved cmac */

		pr_debug("app [%d] cmac : 0x%08x,0x%08x,0x%08x,0x%08x\n", j,
			calc_apps_cmac[j][0], calc_apps_cmac[j][1],
			calc_apps_cmac[j][2], calc_apps_cmac[j][3]);
	}

	return 0;
}

static int spss_get_saved_uefi_apps_cmac(void)
{
	u8 __iomem *reg = NULL;
	int i, j;
	u32 val;

	if (cmac_mem == NULL)
		return -EFAULT;

	reg = cmac_mem; /* IAR buffer base */
	reg += (2*CMAC_SIZE_IN_BYTES); /* skip the saved and calc fw cmac */
	pr_debug("reg [%pK]\n", reg);

	/* get saved apps cmac from ddr - were written by UEFI spss driver */
	for (j = 0; j < NUM_UEFI_APPS; j++) {
		for (i = 0; i < CMAC_SIZE_IN_DWORDS; i++) {
			val = readl_relaxed(reg);
			saved_apps_cmac[j][i] = val;
			reg += sizeof(u32);
		}
		reg += CMAC_SIZE_IN_BYTES; /* skip the calc cmac */

		pr_debug("app[%d] saved cmac: 0x%08x,0x%08x,0x%08x,0x%08x\n",
			j,
			saved_apps_cmac[j][0], saved_apps_cmac[j][1],
			saved_apps_cmac[j][2], saved_apps_cmac[j][3]);
	}

	return 0;
}

static int spss_set_saved_uefi_apps_cmac(void)
{
	u8 __iomem *reg = NULL;
	int i, j;
	u32 val;

	if (cmac_mem == NULL)
		return -EFAULT;

	reg = cmac_mem; /* IAR buffer base */
	reg += (2*CMAC_SIZE_IN_BYTES); /* skip the saved and calc fw cmac */
	pr_debug("reg [%pK]\n", reg);

	/* get saved apps cmac from ddr - were written by UEFI spss driver */
	for (j = 0; j < NUM_UEFI_APPS; j++) {
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
		pr_debug("[SUBSYS_BEFORE_POWERUP] event.\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		pr_debug("[SUBSYS_AFTER_POWERUP] event.\n");
		mutex_lock(&event_lock);
		event_id = SPSS_EVENT_ID_SPU_POWER_UP;
		complete_all(&spss_events[event_id]);
		spss_events_signaled[event_id] = true;

		event_id = SPSS_EVENT_ID_SPU_POWER_DOWN;
		reinit_completion(&spss_events[event_id]);
		spss_events_signaled[event_id] = false;
		mutex_unlock(&event_lock);
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
		/* do nothing if IAR is not active */
		if (!is_iar_active)
			return NOTIFY_OK;
		pr_debug("[SUBSYS_BEFORE_AUTH_AND_RESET] event.\n");
		/* Called on SSR as spss firmware is loaded by UEFI */
		spss_set_fw_cmac(cmac_buf, sizeof(cmac_buf));
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
	struct device *dev = NULL;

	if (!pdev) {
		pr_err("invalid pdev.\n");
		return -ENODEV;
	}

	np = pdev->dev.of_node;
	if (!np) {
		pr_err("invalid DT node.\n");
		return -EINVAL;
	}

	spss_utils_dev = kzalloc(sizeof(*spss_utils_dev), GFP_KERNEL);
	if (spss_utils_dev == NULL)
		return -ENOMEM;

	dev = &pdev->dev;
	spss_dev = dev;

	if (dev == NULL) {
		pr_err("invalid dev.\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, dev);

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
		pr_err("fail to set fw name\n");
		return -EINVAL;
	}

	ret = spss_utils_create_chardev(dev);
	if (ret < 0)
		return ret;

	ret = spss_create_sysfs(dev);
	if (ret < 0)
		return ret;

	pr_info("Initialization completed ok, firmware_name [%s].\n",
		firmware_name);

	iar_nb = kzalloc(sizeof(*iar_nb), GFP_KERNEL);
	if (!iar_nb)
		return -ENOMEM;

	iar_nb->notifier_call = spss_utils_pil_callback;

	iar_notif_handle = subsys_notif_register_notifier("spss", iar_nb);
	if (IS_ERR_OR_NULL(iar_notif_handle)) {
		pr_err("register fail for IAR notifier\n");
		kfree(iar_nb);
	}

	for (i = 0 ; i < SPSS_NUM_EVENTS; i++) {
		init_completion(&spss_events[i]);
		spss_events_signaled[i] = false;
	}
	mutex_init(&event_lock);

	return 0;
}

static const struct of_device_id spss_match_table[] = {
	{ .compatible = "qcom,spss-utils", },
	{ },
};

static struct platform_driver spss_driver = {
	.probe = spss_probe,
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

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Secure Processor Utilities");
