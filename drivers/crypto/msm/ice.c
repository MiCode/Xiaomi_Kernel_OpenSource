/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/scm.h>
#include <linux/device-mapper.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <crypto/ice.h>
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

const struct qcom_ice_variant_ops qcom_ice_ops;
static LIST_HEAD(ice_devices);
/*
 * ICE HW device structure.
 */
struct ice_device {
	struct list_head	list;
	struct device		*pdev;
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
};

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
	regval = 0;
	regval = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_ENDIAN_SWAP);
	regval |= 0xF;
	qcom_ice_writel(ice_dev, regval, QCOM_ICE_REGS_ENDIAN_SWAP);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
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

	/* ~0x100 => CONTROLLER_RESET = RESET_ON
	 *           IGNORE_CONTROLLER_RESET = USE
	 *           ICE_RESET = RESET_ON
	 */
	reg &= ~0x100;
	qcom_ice_writel(ice_dev, reg, QCOM_ICE_REGS_RESET);

	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();

	reg = qcom_ice_readl(ice_dev, QCOM_ICE_REGS_CONTROL);

	/*
	 * ~0x7 => DECR_BYPASS = BYPASS_DISABLE
	 *        ENCR_BYPASS = BYPASS_DISABLE
	 *        GLOBAL_BYPASS = BYPASS_DISABLE
	 */
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

	if (maj_rev != ICE_CORE_CURRENT_MAJOR_VERSION) {
		pr_err("%s: Unknown QC ICE device at 0x%lu, rev %d.%d.%d\n",
			__func__, (unsigned long)ice_dev->mmio,
			maj_rev, min_rev, step_rev);
		return -EIO;
	}

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

static int qcom_ice_get_device_tree_data(struct platform_device *pdev,
		struct ice_device *ice_dev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int rc = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s: Error = %d No memory available for IORESOURCE\n",
			__func__, rc);
		return -ENOMEM;
	}

	ice_dev->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(ice_dev->mmio)) {
		rc = PTR_ERR(ice_dev->mmio);
		pr_err("%s: Error = %d mapping ICE io memory\n",
			__func__, rc);
		goto out;
	}

out:
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
		iounmap(ice_dev->mmio);

	list_del_init(&ice_dev->list);
	kfree(ice_dev);

	return 1;
}

static int  qcom_ice_suspend(struct platform_device *pdev)
{
	/* ICE driver does need to do anything */
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
	qcom_ice_enable(ice_dev);
	qcom_ice_enable_test_bus_config(ice_dev);
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
		qcom_ice_low_power_mode_enable(ice_dev);

		qcom_ice_enable_test_bus_config(ice_dev);

		qcom_ice_optimization_enable(ice_dev);
		qcom_ice_enable(ice_dev);

		/*
		 * When ICE resets, it wipes all of keys from LUTs
		 * ICE driver should call TZ to restore keys
		 */
		if (qcom_ice_restore_config())
			ice_dev->error_cb(ice_dev->host_controller_data,
					ICE_ERROR_ICE_KEY_RESTORE_FAILED);

		if (ice_dev->is_clear_irq_pending)
			qcom_ice_clear_irq(ice_dev);
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

	if (ice_dev->success_cb && ice_dev->host_controller_data)
		ice_dev->success_cb(ice_dev->host_controller_data,
				ICE_RESUME_COMPLETION);

	return 0;
}
EXPORT_SYMBOL(qcom_ice_resume);

static int qcom_ice_reset(struct  platform_device *pdev)
{
	/*
	 * There are two ways by which ICE can be reset
	 * 1. storage driver calls ICE reset before proceeding with its reset
	 *    ICE completes resets sequence and returns to storage driver
	 * 2. ICE generates QCOM_DBG_OPEN_EVENT interrupt which should cause
	 *    ICE RESET
	 *    ICE driver listen for KEYS_RAM_RESET_COMPLETED and send
	 *    completion notice to  storage driver
	 *
	 * Upon storage reset ice reset function will be invoked.
	 * ICE reset function is responsible for
	 *  - Setting ICE RESET bit
	 *  - ICE HW enabling sequence
	 *  - Key restoration
	 *  - Clear ICE interrupts
	 * A completion event should be triggered upon reset completion
	 */
	struct ice_device *ice_dev;

	ice_dev = platform_get_drvdata(pdev);
	if (!ice_dev) {
		pr_err("%s: INVALID ice_dev\n", __func__);
		return -EINVAL;
	}

	ice_dev->is_clear_irq_pending = true;

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
	if (bio_flagged(req->bio, BIO_DONTFREE)) {
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
