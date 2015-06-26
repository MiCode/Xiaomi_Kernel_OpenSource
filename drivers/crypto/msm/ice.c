/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/async.h>
#include <linux/of.h>
#include <linux/device-mapper.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/regulator/consumer.h>
#include <linux/msm-bus.h>
#include <crypto/ice.h>
#include <soc/qcom/scm.h>
#include "iceregs.h"

#define SCM_IO_READ	0x1
#define SCM_IO_WRITE	0x2
#define TZ_SYSCALL_CREATE_SMC_ID(o, s, f) \
	((uint32_t)((((o & 0x3f) << 24) | (s & 0xff) << 8) | (f & 0xff)))

#define TZ_OWNER_QSEE_OS                 50
#define TZ_SVC_KEYSTORE                  5     /* Keystore management */

#define TZ_OS_KS_RESTORE_KEY_ID \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_QSEE_OS, TZ_SVC_KEYSTORE, 0x06)

#define TZ_SYSCALL_CREATE_PARAM_ID_0 0

#define TZ_OS_KS_RESTORE_KEY_ID_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_0

#define ICE_REV(x, y) (((x) & ICE_CORE_##y##_REV_MASK) >> ICE_CORE_##y##_REV)
#define QCOM_ICE_DEV	"ice"
#define QCOM_ICE_TYPE_NAME_LEN 8

const struct qcom_ice_variant_ops qcom_ice_ops;

struct ice_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	u32 min_freq;
	u32 curr_freq;
	bool enabled;
};

struct qcom_ice_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	int saved_vote;
	bool is_max_bw_needed;
	struct device_attribute max_bus_bw;
};

static LIST_HEAD(ice_devices);
/*
 * ICE HW device structure.
 */
struct ice_device {
	struct list_head	list;
	struct device		*pdev;
	struct cdev		cdev;
	dev_t			device_no;
	struct class		*driver_class;
	void __iomem		*mmio;
	int			irq;
	bool			is_irq_enabled;
	bool			is_ice_enabled;
	bool			is_ice_disable_fuse_blown;
	bool			is_clear_irq_pending;
	ice_success_cb		success_cb;
	ice_error_cb		error_cb;
	void			*host_controller_data; /* UFS/EMMC/other? */
	spinlock_t		lock;
	struct list_head	clk_list_head;
	u32			ice_hw_version;
	bool			is_ice_clk_available;
	char			ice_instance_type[QCOM_ICE_TYPE_NAME_LEN];
	struct regulator	*reg;
	bool			is_regulator_available;
	struct qcom_ice_bus_vote bus_vote;
};

static int qcom_ice_enable_clocks(struct ice_device *, bool);

static int qcom_ice_set_bus_vote(struct ice_device *ice_dev, int vote)
{
	int err = 0;

	if (vote != ice_dev->bus_vote.curr_vote) {
		err = msm_bus_scale_client_update_request(
				ice_dev->bus_vote.client_handle, vote);
		if (err) {
			dev_err(ice_dev->pdev,
				"%s:failed:client_handle=0x%x, vote=%d, err=%d\n",
				__func__, ice_dev->bus_vote.client_handle,
				vote, err);
			goto out;
		}
		ice_dev->bus_vote.curr_vote = vote;
	}
out:
	return err;
}

static int qcom_ice_get_bus_vote(struct ice_device *ice_dev,
		const char *speed_mode)
{
	struct device *dev = ice_dev->pdev;
	struct device_node *np = dev->of_node;
	int err;
	const char *key = "qcom,bus-vector-names";

	if (!speed_mode) {
		err = -EINVAL;
		goto out;
	}

	if (ice_dev->bus_vote.is_max_bw_needed && !!strcmp(speed_mode, "MIN"))
		err = of_property_match_string(np, key, "MAX");
	else
		err = of_property_match_string(np, key, speed_mode);
out:
	if (err < 0)
		dev_err(dev, "%s: Invalid %s mode %d\n",
				__func__, speed_mode, err);
	return err;
}

static int qcom_ice_bus_register(struct ice_device *ice_dev)
{
	int err = 0;
	struct msm_bus_scale_pdata *bus_pdata;
	struct device *dev = ice_dev->pdev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *np = dev->of_node;

	bus_pdata = msm_bus_cl_get_pdata(pdev);
	if (!bus_pdata) {
		dev_err(dev, "%s: failed to get bus vectors\n", __func__);
		err = -ENODATA;
		goto out;
	}

	err = of_property_count_strings(np, "qcom,bus-vector-names");
	if (err < 0 || err != bus_pdata->num_usecases) {
		dev_err(dev, "%s: Error = %d with qcom,bus-vector-names\n",
				__func__, err);
		goto out;
	}
	err = 0;

	ice_dev->bus_vote.client_handle =
			msm_bus_scale_register_client(bus_pdata);
	if (!ice_dev->bus_vote.client_handle) {
		dev_err(dev, "%s: msm_bus_scale_register_client failed\n",
				__func__);
		err = -EFAULT;
		goto out;
	}

	/* cache the vote index for minimum and maximum bandwidth */
	ice_dev->bus_vote.min_bw_vote = qcom_ice_get_bus_vote(ice_dev, "MIN");
	ice_dev->bus_vote.max_bw_vote = qcom_ice_get_bus_vote(ice_dev, "MAX");
out:
	return err;
}

static int qcom_ice_get_vreg(struct ice_device *ice_dev)
{
	int ret = 0;

	if (!ice_dev->is_regulator_available)
		return 0;

	if (ice_dev->reg)
		return 0;

	ice_dev->reg = devm_regulator_get(ice_dev->pdev, "vdd-hba");
	if (IS_ERR(ice_dev->reg)) {
		ret = PTR_ERR(ice_dev->reg);
		dev_err(ice_dev->pdev, "%s: %s get failed, err=%d\n",
			__func__, "vdd-hba-supply", ret);
	}
	return ret;
}

static void qcom_ice_config_proc_ignore(struct ice_device *ice_dev)
{
	u32 regval;
	if (ICE_REV(ice_dev->ice_hw_version, MAJOR) == 2 &&
	    ICE_REV(ice_dev->ice_hw_version, MINOR) == 0 &&
	    ICE_REV(ice_dev->ice_hw_version, STEP) == 0) {
		regval = qcom_ice_readl(ice_dev,
				QCOM_ICE_REGS_ADVANCED_CONTROL);
		regval |= 0x800;
		qcom_ice_writel(ice_dev, regval,
				QCOM_ICE_REGS_ADVANCED_CONTROL);
		/* Ensure register is updated */
		mb();
	}
}

static void qcom_ice_low_power_mode_enable(struct ice_device *ice_dev)
{
	u32 regval;
	regval = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_ADVANCED_CONTROL);
	/*
	 * Enable low power mode sequence
	 * [0]-0, [1]-0, [2]-0, [3]-E, [4]-0, [5]-0, [6]-0, [7]-0
	 */
	regval |= 0x7000;
	qcom_ice_writel(ice_dev, regval, QCOM_ICE_REGS_ADVANCED_CONTROL);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static void qcom_ice_enable_test_bus_config(struct ice_device *ice_dev)
{
	/*
	 * Configure & enable ICE_TEST_BUS_REG to reflect ICE intr lines
	 * MAIN_TEST_BUS_SELECTOR = 0 (ICE_CONFIG)
	 * TEST_BUS_REG_EN = 1 (ENABLE)
	 */
	u32 regval;

	if (ICE_REV(ice_dev->ice_hw_version, MAJOR) >= 2)
		return;

	regval = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_TEST_BUS_CONTROL);
	regval &= 0x0FFFFFFF;
	/* TBD: replace 0x2 with define in iceregs.h */
	regval |= 0x2;
	qcom_ice_writel(ice_dev, regval, QCOM_ICE_REGS_TEST_BUS_CONTROL);

	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static void qcom_ice_optimization_enable(struct ice_device *ice_dev)
{
	u32 regval;

	regval = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_ADVANCED_CONTROL);
	if (ICE_REV(ice_dev->ice_hw_version, MAJOR) >= 2)
		regval |= 0x8D800100;
	else if (ICE_REV(ice_dev->ice_hw_version, MAJOR) == 1)
		regval |= 0x3F007100;

	/* ICE Optimizations Enable Sequence */
	udelay(5);
	/* [0]-0, [1]-0, [2]-8, [3]-E, [4]-0, [5]-0, [6]-F, [7]-A */
	qcom_ice_writel(ice_dev, regval, QCOM_ICE_REGS_ADVANCED_CONTROL);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();

	/* ICE HPG requires sleep before writing */
	udelay(5);
	if (ICE_REV(ice_dev->ice_hw_version, MAJOR) == 1) {
		regval = 0;
		regval = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_ENDIAN_SWAP);
		regval |= 0xF;
		qcom_ice_writel(ice_dev, regval, QCOM_ICE_REGS_ENDIAN_SWAP);
		/*
		 * Ensure previous instructions were completed before issue
		 * next ICE commands
		 */
		mb();
	}
}

static void qcom_ice_enable(struct ice_device *ice_dev)
{
	unsigned int reg;

	/*
	 * To enable ICE, perform following
	 * 1. Set IGNORE_CONTROLLER_RESET to USE in ICE_RESET register
	 * 2. Disable GLOBAL_BYPASS bit in ICE_CONTROL register
	 */
	reg = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_RESET);

	if (ICE_REV(ice_dev->ice_hw_version, MAJOR) >= 2)
		reg &= 0x0;
	else if (ICE_REV(ice_dev->ice_hw_version, MAJOR) == 1)
		reg &= ~0x100;

	qcom_ice_writel(ice_dev, reg, QCOM_ICE_REGS_RESET);

	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();

	reg = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_CONTROL);

	if (ICE_REV(ice_dev->ice_hw_version, MAJOR) >= 2)
		reg &= 0xFFFE;
	else if (ICE_REV(ice_dev->ice_hw_version, MAJOR) == 1)
		reg &= ~0x7;
	qcom_ice_writel(ice_dev, reg, QCOM_ICE_REGS_CONTROL);

	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static int qcom_ice_verify_ice(struct ice_device *ice_dev)
{
	unsigned int rev;
	unsigned int maj_rev, min_rev, step_rev;

	rev = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_VERSION);
	maj_rev = (rev & ICE_CORE_MAJOR_REV_MASK) >> ICE_CORE_MAJOR_REV;
	min_rev = (rev & ICE_CORE_MINOR_REV_MASK) >> ICE_CORE_MINOR_REV;
	step_rev = (rev & ICE_CORE_STEP_REV_MASK) >> ICE_CORE_STEP_REV;

	if (maj_rev > ICE_CORE_CURRENT_MAJOR_VERSION) {
		pr_err("%s: Unknown QC ICE device at 0x%lu, rev %d.%d.%d\n",
			__func__, (unsigned long)ice_dev->mmio,
			maj_rev, min_rev, step_rev);
		return -EIO;
	}
	ice_dev->ice_hw_version = rev;

	dev_info(ice_dev->pdev, "QC ICE %d.%d.%d device found @0x%p\n",
					maj_rev, min_rev, step_rev,
					ice_dev->mmio);

	return 0;
}

static void qcom_ice_enable_intr(struct ice_device *ice_dev)
{
	unsigned reg = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_NON_SEC_IRQ_MASK);

	reg &= ~QCOM_ICE_NON_SEC_IRQ_MASK;
	qcom_ice_writel(ice_dev, reg, QCOM_ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static void qcom_ice_disable_intr(struct ice_device *ice_dev)
{
	unsigned reg = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_NON_SEC_IRQ_MASK);

	reg |= QCOM_ICE_NON_SEC_IRQ_MASK;
	qcom_ice_writel(ice_dev, reg, QCOM_ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static int qcom_ice_clear_irq(struct ice_device *ice_dev)
{
	qcom_ice_writel(ice_dev, QCOM_ICE_NON_SEC_IRQ_MASK,
			QCOM_ICE_REGS_NON_SEC_IRQ_CLR);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
	ice_dev->is_clear_irq_pending = false;

	return 0;
}

static irqreturn_t qcom_ice_isr(int isr, void *data)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int intr_status, clear_reg;
	struct ice_device *ice_dev = data;
	enum ice_error_code err;
	unsigned long flags;

	spin_lock_irqsave(&ice_dev->lock, flags);
	intr_status = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_NON_SEC_IRQ_STTS);
	if (intr_status) {
		clear_reg = qcom_ice_readl(ice_dev,
				QCOM_ICE_REGS_NON_SEC_IRQ_CLR);

		/* Check the source of interrupt */
		if (intr_status & QCOM_ICE_STREAM1_PREMATURE_LBA_CHANGE) {
			err = ICE_ERROR_STREAM1_PREMATURE_LBA_CHANGE;
			clear_reg |= QCOM_ICE_STREAM1_PREMATURE_LBA_CHANGE;
		} else if (intr_status &
				QCOM_ICE_STREAM2_PREMATURE_LBA_CHANGE) {
			err = ICE_ERROR_STREAM2_PREMATURE_LBA_CHANGE;
			clear_reg |= QCOM_ICE_STREAM2_PREMATURE_LBA_CHANGE;
		} else if (intr_status & QCOM_ICE_STREAM1_NOT_EXPECTED_LBO) {
			err = ICE_ERROR_STREAM1_UNEXPECTED_LBA;
			clear_reg |= QCOM_ICE_STREAM1_NOT_EXPECTED_LBO;
		} else if (intr_status & QCOM_ICE_STREAM2_NOT_EXPECTED_LBO) {
			err = ICE_ERROR_STREAM2_UNEXPECTED_LBA;
			clear_reg |= QCOM_ICE_STREAM2_NOT_EXPECTED_LBO;
		}
		ice_dev->error_cb(ice_dev->host_controller_data, err);

		/* Interrupt has been handled. Clear the IRQ */
		qcom_ice_clear_irq(ice_dev);
		retval = IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&ice_dev->lock, flags);
	return retval;
}

static void qcom_ice_parse_ice_instance_type(struct platform_device *pdev,
		struct ice_device *ice_dev)
{
	int ret = -1;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	char *type;

	ret = of_property_read_string_index(np,
				"qcom,instance-type", 0, (const char **)&type);
	if (ret) {
		pr_err("%s: Could not get ICE instance type\n", __func__);
		goto out;
	}
	strlcpy(ice_dev->ice_instance_type, type, QCOM_ICE_TYPE_NAME_LEN);
out:
	return;
}

static int qcom_ice_parse_clock_info(struct platform_device *pdev,
		struct ice_device *ice_dev)
{
	int ret = -1, cnt, i, len;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	char *name;
	struct ice_clk_info *clki;
	u32 *clkfreq = NULL;

	if (!np)
		goto out;

	cnt = of_property_count_strings(np, "clock-names");
	if (cnt <= 0) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n",
				__func__);
		ret = cnt;
		goto out;
	}

	if (!of_get_property(np, "qcom,op-freq-hz", &len)) {
		dev_info(dev, "qcom,op-freq-hz property not specified\n");
		goto out;
	}

	len = len/sizeof(*clkfreq);
	if (len != cnt)
		goto out;

	clkfreq = devm_kzalloc(dev, len * sizeof(*clkfreq), GFP_KERNEL);
	if (!clkfreq) {
		dev_err(dev, "%s: no memory\n", "qcom,op-freq-hz");
		ret = -ENOMEM;
		goto out;
	}
	ret = of_property_read_u32_array(np, "qcom,op-freq-hz", clkfreq, len);

	INIT_LIST_HEAD(&ice_dev->clk_list_head);

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(np,
				"clock-names", i, (const char **)&name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}
		clki->max_freq = clkfreq[i];
		clki->name = kstrdup(name, GFP_KERNEL);
		list_add_tail(&clki->list, &ice_dev->clk_list_head);
	}
out:
	if (clkfreq)
		devm_kfree(dev, (void *)clkfreq);
	return ret;
}

static int qcom_ice_get_device_tree_data(struct platform_device *pdev,
		struct ice_device *ice_dev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int irq, rc = -1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s: No memory available for IORESOURCE\n", __func__);
		return -ENOMEM;
	}

	ice_dev->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(ice_dev->mmio)) {
		rc = PTR_ERR(ice_dev->mmio);
		pr_err("%s: Error = %d mapping ICE io memory\n", __func__, rc);
		goto out;
	}

	if (!of_parse_phandle(pdev->dev.of_node, "vdd-hba-supply", 0)) {
		pr_err("%s: No vdd-hba-supply regulator, assuming not needed\n",
								 __func__);
		ice_dev->is_regulator_available = false;
	} else {
		ice_dev->is_regulator_available = true;
	}
	ice_dev->is_ice_clk_available = of_property_read_bool(
						(&pdev->dev)->of_node,
						"qcom,enable-ice-clk");


	if (ice_dev->is_ice_clk_available) {
		rc = qcom_ice_parse_clock_info(pdev, ice_dev);
		if (rc)
			goto err_dev;

		irq = platform_get_irq(pdev, 0);
		if (irq < 0) {
			dev_err(dev, "IRQ resource not available\n");
			rc = -ENODEV;
			goto err_dev;
		}
		rc = devm_request_irq(dev, irq, qcom_ice_isr, 0,
				dev_name(dev), ice_dev);
		if (rc) {
			goto err_dev;
		} else {
			ice_dev->irq = irq;
			ice_dev->is_irq_enabled = true;
		}
		pr_info("ICE IRQ = %d\n", ice_dev->irq);
	}
	qcom_ice_parse_ice_instance_type(pdev, ice_dev);
	return 0;
err_dev:
	if (rc && ice_dev->mmio)
		devm_iounmap(dev, ice_dev->mmio);
out:
	return rc;
}

/*
 * ICE HW instance can exist in UFS or eMMC based storage HW.
 * Userspace does not know what kind of ICE it is dealing with.
 * Though userspace can find which storage device it is booting
 * from but all kind of storage types don't support ICE from
 * beginning. So ICE device is created for user space to ping
 * if ICE exist for that kind of storage.
 */
static const struct file_operations qcom_ice_fops = {
	.owner = THIS_MODULE,
};

static int register_ice_device(struct ice_device *ice_dev)
{
	int rc = 0;
	unsigned baseminor = 0;
	unsigned count = 1;
	struct device *class_dev;
	char tmp_dev_name[16];
	memset(tmp_dev_name, 0, 16);

	strlcpy(tmp_dev_name, QCOM_ICE_DEV, 8);
	strlcat(tmp_dev_name, ice_dev->ice_instance_type, 8);

	pr_debug("%s: instance type = %s device name = %s\n", __func__,
				ice_dev->ice_instance_type, tmp_dev_name);

	rc = alloc_chrdev_region(&ice_dev->device_no, baseminor, count,
							tmp_dev_name);
	if (rc < 0) {
		pr_err("alloc_chrdev_region failed %d for %s\n",
						rc, tmp_dev_name);
		return rc;
	}
	ice_dev->driver_class = class_create(THIS_MODULE, tmp_dev_name);
	if (IS_ERR(ice_dev->driver_class)) {
		rc = -ENOMEM;
		pr_err("class_create failed %d for %s\n", rc, tmp_dev_name);
		goto exit_unreg_chrdev_region;
	}
	class_dev = device_create(ice_dev->driver_class, NULL,
					ice_dev->device_no, NULL, tmp_dev_name);

	if (!class_dev) {
		pr_err("class_device_create failed %d for %s\n",
							rc, tmp_dev_name);
		rc = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&ice_dev->cdev, &qcom_ice_fops);
	ice_dev->cdev.owner = THIS_MODULE;

	rc = cdev_add(&ice_dev->cdev, MKDEV(MAJOR(ice_dev->device_no), 0), 1);
	if (rc < 0) {
		pr_err("cdev_add failed %d for %s\n", rc, tmp_dev_name);
		goto exit_destroy_device;
	}
	return  0;

exit_destroy_device:
	device_destroy(ice_dev->driver_class, ice_dev->device_no);

exit_destroy_class:
	class_destroy(ice_dev->driver_class);

exit_unreg_chrdev_region:
	unregister_chrdev_region(ice_dev->device_no, 1);
	return rc;
}

static int qcom_ice_probe(struct platform_device *pdev)
{
	struct ice_device *ice_dev;
	int rc = 0;

	if (!pdev) {
		pr_err("%s: Invalid platform_device passed\n",
			__func__);
		return -EINVAL;
	}

	ice_dev = kzalloc(sizeof(struct ice_device), GFP_KERNEL);

	if (!ice_dev) {
		rc = -ENOMEM;
		pr_err("%s: Error %d allocating memory for ICE device:\n",
			__func__, rc);
		goto out;
	}

	ice_dev->pdev = &pdev->dev;
	if (!ice_dev->pdev) {
		rc = -EINVAL;
		pr_err("%s: Invalid device passed in platform_device\n",
								__func__);
		goto err_ice_dev;
	}

	if (pdev->dev.of_node)
		rc = qcom_ice_get_device_tree_data(pdev, ice_dev);
	else {
		rc = -EINVAL;
		pr_err("%s: ICE device node not found\n", __func__);
	}

	if (rc)
		goto err_ice_dev;

	pr_debug("%s: Registering ICE device\n", __func__);
	rc = register_ice_device(ice_dev);
	if (rc) {
		pr_err("create character device failed.\n");
		goto err_ice_dev;
	}
	spin_lock_init(&ice_dev->lock);
	/*
	 * If ICE is enabled here, it would be waste of power.
	 * We would enable ICE when first request for crypto
	 * operation arrives.
	 */
	ice_dev->is_ice_enabled = false;

	platform_set_drvdata(pdev, ice_dev);
	list_add_tail(&ice_dev->list, &ice_devices);

	goto out;

err_ice_dev:
	kfree(ice_dev);
out:
	return rc;
}

static int qcom_ice_remove(struct platform_device *pdev)
{
	struct ice_device *ice_dev;

	ice_dev = (struct ice_device *)platform_get_drvdata(pdev);

	if (!ice_dev)
		return 0;

	qcom_ice_disable_intr(ice_dev);

	device_init_wakeup(&pdev->dev, false);
	if (ice_dev->mmio)
		devm_iounmap(&pdev->dev, ice_dev->mmio);

	list_del_init(&ice_dev->list);
	kfree(ice_dev);

	return 1;
}

static int  qcom_ice_suspend(struct platform_device *pdev)
{
	return 0;
}

static int qcom_ice_restore_config(void)
{
	struct scm_desc desc = {0};
	int ret;

	/*
	 * TZ would check KEYS_RAM_RESET_COMPLETED status bit before processing
	 * restore config command. This would prevent two calls from HLOS to TZ
	 * One to check KEYS_RAM_RESET_COMPLETED status bit second to restore
	 * config
	 */

	desc.arginfo = TZ_OS_KS_RESTORE_KEY_ID_PARAM_ID;

	ret = scm_call2(TZ_OS_KS_RESTORE_KEY_ID, &desc);

	if (ret)
		pr_err("%s: Error: 0x%x\n", __func__, ret);

	return ret;
}

static int qcom_ice_init_clocks(struct ice_device *ice)
{
	int ret = -1;
	struct ice_clk_info *clki;
	struct device *dev = ice->pdev;
	struct list_head *head = &ice->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s:ICE Clock list null/empty\n", __func__);
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		/* Not all clocks would have a rate to be set */
		ret = 0;
		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(dev,
				"%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
				clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
			dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
		}
	}
out:
	return ret;
}

static int qcom_ice_enable_clocks(struct ice_device *ice, bool enable)
{
	int ret = 0;
	struct ice_clk_info *clki;
	struct device *dev = ice->pdev;
	struct list_head *head = &ice->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s:ICE Clock list null/empty\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!ice->is_ice_clk_available) {
		dev_err(dev, "%s:ICE Clock not available\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		if (enable)
			ret = clk_prepare_enable(clki->clk);
		else
			clk_disable_unprepare(clki->clk);

		if (ret) {
			dev_err(dev, "Unable to %s ICE core clk\n",
				enable?"enable":"disable");
			goto out;
		}
	}
out:
	return ret;
}

static int qcom_ice_secure_ice_init(struct ice_device *ice_dev)
{
	/* We need to enable source for ICE secure interrupts */
	int ret = 0;
	u32 regval;

	regval = scm_call_atomic1(SCM_SVC_TZ, SCM_IO_READ,
			(unsigned long)ice_dev->mmio +
			QCOM_ICE_LUT_KEYS_ICE_SEC_IRQ_MASK);

	regval &= ~QCOM_ICE_SEC_IRQ_MASK;
	ret = scm_call_atomic2(SCM_SVC_TZ, SCM_IO_WRITE,
			(unsigned long)ice_dev->mmio +
			QCOM_ICE_LUT_KEYS_ICE_SEC_IRQ_MASK, regval);

	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();

	if (!ret)
		pr_err("%s: failed(0x%x) to init secure ICE config\n",
								__func__, ret);
	return ret;
}

static int qcom_ice_update_sec_cfg(struct ice_device *ice_dev)
{
	int ret = 0, scm_ret = 0;

	/* scm command buffer structrue */
	struct qcom_scm_cmd_buf {
		unsigned int device_id;
		unsigned int spare;
	} cbuf = {0};

	/*
	 * Ideally, we should check ICE version to decide whether to proceed or
	 * or not. Since version wont be available when this function is called
	 * we need to depend upon is_ice_clk_available to decide
	 */
	if (ice_dev->is_ice_clk_available)
		goto out;

	/*
	 * Store dev_id in ice_device structure so that emmc/ufs cases can be
	 * handled properly
	 */
	#define RESTORE_SEC_CFG_CMD	0x2
	#define ICE_TZ_DEV_ID	20

	cbuf.device_id = ICE_TZ_DEV_ID;
	ret = scm_restore_sec_cfg(cbuf.device_id, cbuf.spare, &scm_ret);
	if (ret || scm_ret) {
		pr_err("%s: failed, ret %d scm_ret %d\n",
						__func__, ret, scm_ret);
		if (!ret)
			ret = scm_ret;
	}
out:

	return ret;
}

static void qcom_ice_finish_init(void *data, async_cookie_t cookie)
{
	struct ice_device *ice_dev = data;
	unsigned reg;

	if (!ice_dev) {
		pr_err("%s: Null data received\n", __func__);
		return;
	}

	if (ice_dev->is_ice_clk_available) {
		if (qcom_ice_init_clocks(ice_dev)) {
			ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_IMPROPER_INITIALIZATION);
			return;
		}
		if (qcom_ice_bus_register(ice_dev))
			ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_IMPROPER_INITIALIZATION);
	}

	/*
	 * It is possible that ICE device is not probed when host is probed
	 * This would cause host probe to be deferred. When probe for host is
	 * defered, it can cause power collapse for host and that can wipe
	 * configurations of host & ice. It is prudent to restore the config
	 */
	if (qcom_ice_update_sec_cfg(ice_dev)) {
		ice_dev->error_cb(ice_dev->host_controller_data,
			ICE_ERROR_ICE_TZ_INIT_FAILED);
		return;
	}

	if (qcom_ice_verify_ice(ice_dev)) {
		ice_dev->error_cb(ice_dev->host_controller_data,
			ICE_ERROR_UNEXPECTED_ICE_DEVICE);
		return;
	}

	/* if ICE_DISABLE_FUSE is blown, return immediately */
	reg = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_FUSE_SETTING);
	reg &= ICE_FUSE_SETTING_MASK;

	if (reg) {
		ice_dev->is_ice_disable_fuse_blown = true;
		pr_err("%s: Error: ICE_ERROR_HW_DISABLE_FUSE_BLOWN\n",
								__func__);
		ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_HW_DISABLE_FUSE_BLOWN);
		return;
	}

	if (!qcom_ice_secure_ice_init(ice_dev)) {
		pr_err("%s: Error: ICE_ERROR_ICE_TZ_INIT_FAILED\n", __func__);
		ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_ICE_TZ_INIT_FAILED);
		return;
	}

	qcom_ice_low_power_mode_enable(ice_dev);
	qcom_ice_optimization_enable(ice_dev);
	qcom_ice_config_proc_ignore(ice_dev);
	qcom_ice_enable_test_bus_config(ice_dev);
	qcom_ice_enable(ice_dev);
	ice_dev->is_ice_enabled = true;
	qcom_ice_enable_intr(ice_dev);

	ice_dev->success_cb(ice_dev->host_controller_data,
						ICE_INIT_COMPLETION);
	return;
}

static int qcom_ice_init(struct platform_device *pdev,
			void *host_controller_data,
			ice_success_cb success_cb,
			ice_error_cb error_cb)
{
	/*
	 * A completion event for host controller would be triggered upon
	 * initialization completion
	 * When ICE is initialized, it would put ICE into Global Bypass mode
	 * When any request for data transfer is received, it would enable
	 * the ICE for that particular request
	 */
	struct ice_device *ice_dev;

	ice_dev = platform_get_drvdata(pdev);
	if (!ice_dev) {
		pr_err("%s: invalid device\n", __func__);
		return -EINVAL;
	}

	ice_dev->success_cb = success_cb;
	ice_dev->error_cb = error_cb;
	ice_dev->host_controller_data = host_controller_data;

	/*
	 * As ICE init may take time, create an async task to complete rest
	 * of init
	 */
	async_schedule(qcom_ice_finish_init, ice_dev);

	return 0;
}
EXPORT_SYMBOL(qcom_ice_init);


static void qcom_ice_finish_power_collapse(void *data, async_cookie_t cookie)
{
	struct ice_device *ice_dev = data;

	if (ice_dev->is_ice_disable_fuse_blown) {
		ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_HW_DISABLE_FUSE_BLOWN);
		return;
	}

	if (ice_dev->is_ice_enabled) {
		/*
		 * ICE resets into global bypass mode with optimization and
		 * low power mode disabled. Hence we need to redo those seq's.
		 */
		qcom_ice_low_power_mode_enable(ice_dev);

		qcom_ice_enable_test_bus_config(ice_dev);

		qcom_ice_optimization_enable(ice_dev);
		qcom_ice_enable(ice_dev);

		if (ICE_REV(ice_dev->ice_hw_version, MAJOR) == 1) {
			/*
			 * When ICE resets, it wipes all of keys from LUTs
			 * ICE driver should call TZ to restore keys
			 */
			if (qcom_ice_restore_config())
				ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_ICE_KEY_RESTORE_FAILED);
		}
		/*
		 * INTR Status are not retained. So there is no need to
		 * clear those.
		 */
		ice_dev->is_clear_irq_pending = false;
	}

	if (ice_dev->success_cb && ice_dev->host_controller_data)
		ice_dev->success_cb(ice_dev->host_controller_data,
				ICE_RESUME_COMPLETION);
	return;
}

static int  qcom_ice_resume(struct platform_device *pdev)
{
	/*
	 * ICE is power collapsed when storage controller is power collapsed
	 * ICE resume function is responsible for:
	 * ICE HW enabling sequence
	 * Key restoration
	 * A completion event should be triggered
	 * upon resume completion
	 * Storage driver will be fully operational only
	 * after receiving this event
	 */
	struct ice_device *ice_dev;

	ice_dev = platform_get_drvdata(pdev);

	if (!ice_dev)
		return -EINVAL;

	if (ice_dev->is_ice_clk_available) {
		/*
		 * Storage is calling this function after power collapse which
		 * would put ICE into GLOBAL_BYPASS mode. Make sure to enable
		 * ICE
		 */
		qcom_ice_enable(ice_dev);
	}

	if (ice_dev->success_cb && ice_dev->host_controller_data)
		ice_dev->success_cb(ice_dev->host_controller_data,
				ICE_RESUME_COMPLETION);

	return 0;
}
EXPORT_SYMBOL(qcom_ice_resume);

static int qcom_ice_reset(struct  platform_device *pdev)
{
	struct ice_device *ice_dev;

	ice_dev = platform_get_drvdata(pdev);
	if (!ice_dev) {
		pr_err("%s: INVALID ice_dev\n", __func__);
		return -EINVAL;
	}

	async_schedule(qcom_ice_finish_power_collapse, ice_dev);
	return 0;
}
EXPORT_SYMBOL(qcom_ice_reset);

static int qcom_ice_config(struct platform_device *pdev, struct request *req,
			struct ice_data_setting *setting)
{
	struct ice_crypto_setting *crypto_data;
	struct ice_device *ice_dev;
	union map_info *info;

	if (!pdev || !req || !setting) {
		pr_err("%s: Invalid params passed\n", __func__);
		return -EINVAL;
	}

	/*
	 * It is not an error to have a request with no  bio
	 * Such requests must bypass ICE. So first set bypass and then
	 * return if bio is not available in request
	 */
	if (setting) {
		setting->encr_bypass = true;
		setting->decr_bypass = true;
	}

	if (!req->bio) {
		/* It is not an error to have a request with no  bio */
		return 0;
	}

	/*
	 * info field in req->end_io_data could be used by mulitple dm or
	 * non-dm entities. To ensure that we are running operation on dm
	 * based request, check BIO_DONT_FREE flag
	 */
	if (bio_flagged(req->bio, BIO_INLINECRYPT)) {
		info = dm_get_rq_mapinfo(req);
		if (!info) {
			pr_err("%s info not available in request\n", __func__);
			return 0;
		}

		ice_dev = platform_get_drvdata(pdev);

		crypto_data = (struct ice_crypto_setting *)info->ptr;

		if (!ice_dev)
			return 0;

		if (ice_dev->is_ice_disable_fuse_blown) {
			pr_err("%s ICE disabled fuse is blown\n", __func__);
			return -ENODEV;
		}

		if (crypto_data->key_index >= 0) {

			memcpy(&setting->crypto_data, crypto_data,
					sizeof(struct ice_crypto_setting));

			if (rq_data_dir(req) == WRITE)
				setting->encr_bypass = false;
			else if (rq_data_dir(req) == READ)
				setting->decr_bypass = false;
			else {
				/* Should I say BUG_ON */
				setting->encr_bypass = true;
				setting->decr_bypass = true;
			}
		}
	}

	/*
	 * It is not an error. If target is not req-crypt based, all request
	 * from storage driver would come here to check if there is any ICE
	 * setting required
	 */
	return 0;
}
EXPORT_SYMBOL(qcom_ice_config);

static int qcom_ice_status(struct platform_device *pdev)
{
	struct ice_device *ice_dev;
	unsigned int test_bus_reg_status;

	if (!pdev) {
		pr_err("%s: Invalid params passed\n", __func__);
		return -EINVAL;
	}

	ice_dev = platform_get_drvdata(pdev);

	if (!ice_dev)
		return -ENODEV;

	if (!ice_dev->is_ice_enabled)
		return -ENODEV;

	test_bus_reg_status = qcom_ice_readl(ice_dev,
					QCOM_ICE_REGS_TEST_BUS_REG);

	if ((test_bus_reg_status & QCOM_ICE_TEST_BUS_REG_NON_SECURE_INTR) ||
	    (test_bus_reg_status & QCOM_ICE_TEST_BUS_REG_NON_SECURE_INTR))
		return 1;
	else
		return 0;

}
EXPORT_SYMBOL(qcom_ice_status);

const struct qcom_ice_variant_ops qcom_ice_ops = {
	.name             = "qcom",
	.init             = qcom_ice_init,
	.reset            = qcom_ice_reset,
	.resume           = qcom_ice_resume,
	.suspend          = qcom_ice_suspend,
	.config           = qcom_ice_config,
	.status           = qcom_ice_status,
};

/* Following struct is required to match device with driver from dts file */
static struct of_device_id qcom_ice_match[] = {
	{	.compatible = "qcom,ice",
		.data = (void *)&qcom_ice_ops},
	{},
};
MODULE_DEVICE_TABLE(of, qcom_ice_match);

struct platform_device *qcom_ice_get_pdevice(struct device_node *node)
{
	struct platform_device *ice_pdev = NULL;
	struct ice_device *ice_dev = NULL;

	if (!node) {
		pr_err("%s: invalid node %p", __func__, node);
		goto out;
	}

	if (!of_device_is_available(node)) {
		pr_err("%s: device unavailable\n", __func__);
		goto out;
	}


	if (list_empty(&ice_devices)) {
		pr_err("%s: invalid device list\n", __func__);
		ice_pdev = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	list_for_each_entry(ice_dev, &ice_devices, list) {
		if (ice_dev->pdev->of_node == node) {
			pr_info("%s: found ice device %p\n", __func__, ice_dev);
			break;
		}
	}

	ice_pdev = to_platform_device(ice_dev->pdev);
	pr_info("%s: matching platform device %p\n", __func__, ice_pdev);
out:
	return ice_pdev;
}

static struct ice_device *get_ice_device_from_storage_type
					(const char *storage_type)
{
	struct ice_device *ice_dev = NULL;

	if (list_empty(&ice_devices)) {
		pr_err("%s: invalid device list\n", __func__);
		ice_dev = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	list_for_each_entry(ice_dev, &ice_devices, list) {
		if (!strcmp(ice_dev->ice_instance_type, storage_type)) {
			pr_info("%s: found ice device %p\n", __func__, ice_dev);
			break;
		}
	}
out:
	return ice_dev;
}

int qcom_ice_setup_ice_hw(const char *storage_type, int enable)
{
	int ret = -1, vote;
	struct ice_device *ice_dev = NULL;

	ice_dev = get_ice_device_from_storage_type(storage_type);
	if (!ice_dev)
		goto out;

	/* Setup Clocks */
	if (qcom_ice_enable_clocks(ice_dev, enable)) {
		pr_err("%s:%p:%s Could not enable clocks\n", __func__, ice_dev,
				ice_dev->ice_instance_type);
		goto out;
	}

	/* Setup Bus Vote */
	vote = qcom_ice_get_bus_vote(ice_dev, enable ? "MAX" : "MIN");
	if (vote < 0)
		goto out_clocks;
	ret = qcom_ice_set_bus_vote(ice_dev, vote);
	if (ret) {
		pr_err("%s:%p: failed %d\n", __func__, ice_dev, ret);
		goto out_clocks;
	}

	/* Setup Regulator */
	if (ice_dev->is_regulator_available) {
		if (qcom_ice_get_vreg(ice_dev)) {
			pr_err("%s: Could not get regulator\n", __func__);
			goto out_bw_vote;
		}
		ret = (enable) ? regulator_enable(ice_dev->reg)
				: regulator_disable(ice_dev->reg);
		if (ret) {
			pr_err("%s:%p: Could not enable/disable regulator\n",
					__func__, ice_dev);
			goto out_bw_vote;
		}
	}
	return ret;

out_bw_vote:
	if (vote)
		qcom_ice_set_bus_vote(ice_dev, !vote);
out_clocks:
	if (enable)
		qcom_ice_enable_clocks(ice_dev, !enable);
out:
	return ret;
}

struct qcom_ice_variant_ops *qcom_ice_get_variant_ops(struct device_node *node)
{
	if (node) {
		const struct of_device_id *match;
		match = of_match_node(qcom_ice_match, node);
		if (match)
			return (struct qcom_ice_variant_ops *)(match->data);
		pr_err("%s: error matching\n", __func__);
	} else {
		pr_err("%s: invalid node\n", __func__);
	}
	return NULL;
}
EXPORT_SYMBOL(qcom_ice_get_variant_ops);

static struct platform_driver qcom_ice_driver = {
	.probe          = qcom_ice_probe,
	.remove         = qcom_ice_remove,
	.driver         = {
		.owner  = THIS_MODULE,
		.name   = "qcom_ice",
		.of_match_table = qcom_ice_match,
	},
};
module_platform_driver(qcom_ice_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QTI Inline Crypto Engine driver");
