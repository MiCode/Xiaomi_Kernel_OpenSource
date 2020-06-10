/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>
#include <linux/of_irq.h>
#if defined(CONFIG_DRM)
#include <drm/drm_panel.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include "focaltech_core.h"

#if defined(CONFIG_FTS_TRUSTED_TOUCH)
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "linux/haven/hh_irq_lend.h"
#include "linux/haven/hh_msgq.h"
#include "linux/haven/hh_mem_notifier.h"
#include "linux/haven/hh_rm_drv.h"
#include <linux/sort.h>
#endif

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      3000000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_LOAD_MAX_UA                     30000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;

#if defined(CONFIG_DRM)
static struct drm_panel *active_panel;
#endif

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);
static irqreturn_t fts_irq_handler(int irq, void *data);
static int fts_ts_probe_delayed(struct fts_ts_data *fts_data);

#ifdef CONFIG_FTS_TRUSTED_TOUCH

static struct hh_acl_desc *fts_ts_vm_get_acl(enum hh_vm_names vm_name)
{
	struct hh_acl_desc *acl_desc;
	hh_vmid_t vmid;

	hh_rm_get_vmid(vm_name, &vmid);

	acl_desc = kzalloc(offsetof(struct hh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
	if (!acl_desc)
		return ERR_PTR(ENOMEM);

	acl_desc->n_acl_entries = 1;
	acl_desc->acl_entries[0].vmid = vmid;
	acl_desc->acl_entries[0].perms = HH_RM_ACL_R | HH_RM_ACL_W;

	return acl_desc;
}

static struct hh_sgl_desc *fts_ts_vm_get_sgl(
				struct trusted_touch_vm_info *vm_info)
{
	struct hh_sgl_desc *sgl_desc;
	int i;

	sgl_desc = kzalloc(offsetof(struct hh_sgl_desc,
			sgl_entries[vm_info->iomem_list_size]), GFP_KERNEL);
	if (!sgl_desc)
		return ERR_PTR(ENOMEM);

	sgl_desc->n_sgl_entries = vm_info->iomem_list_size;

	for (i = 0; i < vm_info->iomem_list_size; i++) {
		sgl_desc->sgl_entries[i].ipa_base = vm_info->iomem_bases[i];
		sgl_desc->sgl_entries[i].size = vm_info->iomem_sizes[i];
	}

	return sgl_desc;
}

static int fts_ts_populate_vm_info(struct fts_ts_data *fts_data)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	struct device_node *np = fts_data->client->dev.of_node;
	int num_regs, num_sizes = 0;

	vm_info = kzalloc(sizeof(struct trusted_touch_vm_info), GFP_KERNEL);
	if (!vm_info) {
		rc = -ENOMEM;
		goto error;
	}

	fts_data->vm_info = vm_info;
	vm_info->irq_label = HH_IRQ_LABEL_TRUSTED_TOUCH;
	vm_info->vm_name = HH_TRUSTED_VM;
	rc = of_property_read_u32(np, "focaltech,trusted-touch-spi-irq",
			&vm_info->hw_irq);
	if (rc) {
		pr_err("Failed to read trusted touch SPI irq:%d\n", rc);
		goto vm_error;
	}
	num_regs = of_property_count_u32_elems(np,
			"focaltech,trusted-touch-io-bases");
	if (num_regs < 0) {
		pr_err("Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	num_sizes = of_property_count_u32_elems(np,
			"focaltech,trusted-touch-io-sizes");
	if (num_sizes < 0) {
		pr_err("Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	if (num_regs != num_sizes) {
		pr_err("IO bases and sizes doe not match\n");
		rc = -EINVAL;
		goto vm_error;
	}

	vm_info->iomem_list_size = num_regs;

	vm_info->iomem_bases = kcalloc(num_regs, sizeof(*vm_info->iomem_bases),
								GFP_KERNEL);
	if (!vm_info->iomem_bases) {
		rc = -ENOMEM;
		goto vm_error;
	}

	rc = of_property_read_u32_array(np, "focaltech,trusted-touch-io-bases",
			vm_info->iomem_bases, vm_info->iomem_list_size);
	if (rc) {
		pr_err("Failed to read trusted touch io bases:%d\n", rc);
		goto io_bases_error;
	}

	vm_info->iomem_sizes = kzalloc(
			sizeof(*vm_info->iomem_sizes) * num_sizes, GFP_KERNEL);
	if (!vm_info->iomem_sizes) {
		rc = -ENOMEM;
		goto io_bases_error;
	}

	rc = of_property_read_u32_array(np, "focaltech,trusted-touch-io-sizes",
			vm_info->iomem_sizes, vm_info->iomem_list_size);
	if (rc) {
		pr_err("Failed to read trusted touch io sizes:%d\n", rc);
		goto io_sizes_error;
	}
	return rc;

io_sizes_error:
	kfree(vm_info->iomem_sizes);
io_bases_error:
	kfree(vm_info->iomem_bases);
vm_error:
	kfree(vm_info);
error:
	return rc;
}

static void fts_ts_destroy_vm_info(struct fts_ts_data *fts_data)
{
	kfree(fts_data->vm_info->iomem_sizes);
	kfree(fts_data->vm_info->iomem_bases);
	kfree(fts_data->vm_info);
}

static void fts_ts_vm_deinit(struct fts_ts_data *fts_data)
{
	if (fts_data->vm_info->mem_cookie)
		hh_mem_notifier_unregister(fts_data->vm_info->mem_cookie);
	fts_ts_destroy_vm_info(fts_data);
}

#ifdef CONFIG_ARCH_QTI_VM
static int fts_ts_vm_mem_release(struct fts_ts_data *fts_data);
static void fts_ts_trusted_touch_vm_mode_disable(struct fts_ts_data *fts_data);

static int fts_ts_sgl_cmp(const void *a, const void *b)
{
	struct hh_sgl_entry *left = (struct hh_sgl_entry *)a;
	struct hh_sgl_entry *right = (struct hh_sgl_entry *)b;

	return (left->ipa_base - right->ipa_base);
}

static int fts_ts_vm_compare_sgl_desc(struct hh_sgl_desc *expected,
		struct hh_sgl_desc *received)
{
	int idx;

	if (expected->n_sgl_entries != received->n_sgl_entries)
		return -E2BIG;
	sort(received->sgl_entries, received->n_sgl_entries,
			sizeof(received->sgl_entries[0]), fts_ts_sgl_cmp, NULL);
	sort(expected->sgl_entries, expected->n_sgl_entries,
			sizeof(expected->sgl_entries[0]), fts_ts_sgl_cmp, NULL);

	for (idx = 0; idx < expected->n_sgl_entries; idx++) {
		struct hh_sgl_entry *left = &expected->sgl_entries[idx];
		struct hh_sgl_entry *right = &received->sgl_entries[idx];

		if ((left->ipa_base != right->ipa_base) ||
				(left->size != right->size)) {
			pr_err("sgl mismatch: left_base:%d right base:%d left size:%d right size:%d\n",
					left->ipa_base, right->ipa_base,
					left->size, right->size);
			return -EINVAL;
		}
	}
	return 0;
}

static int fts_ts_vm_handle_vm_hardware(struct fts_ts_data *fts_data)
{
	int rc = 0;

	if (atomic_read(&fts_data->delayed_vm_probe_pending)) {
		rc = fts_ts_probe_delayed(fts_data);
		if (rc) {
			pr_err(" Delayed probe failure on VM!\n");
			return rc;
		}
		atomic_set(&fts_data->delayed_vm_probe_pending, 0);
		return rc;
	}

	fts_irq_enable();
	return rc;
}

static void fts_ts_vm_irq_on_lend_callback(void *data, enum hh_irq_label label)
{
	struct fts_ts_data *fts_data = data;
	struct irq_data *irq_data;
	int irq = 0;
	int const resource_timeout = msecs_to_jiffies(2000);
	int rc = 0;

	irq = hh_irq_accept(fts_data->vm_info->irq_label, -1,
						IRQ_TYPE_EDGE_RISING);
	if (irq < 0) {
		pr_err("failed to accept irq\n");
		goto irq_fail;
	}
	atomic_set(&fts_data->vm_info->tvm_owns_irq, 1);
	irq_data = irq_get_irq_data(irq);
	if (!irq_data) {
		pr_err("Invalid irq data for trusted touch\n");
		goto irq_fail;
	}
	if (!irq_data->hwirq) {
		pr_err("Invalid irq in irq data\n");
		goto irq_fail;
	}
	if (irq_data->hwirq != fts_data->vm_info->hw_irq) {
		pr_err("Invalid irq lent\n");
		goto irq_fail;
	}

	pr_debug("irq:returned from accept:%d\n", irq);
	fts_data->irq = irq;
	if (!wait_for_completion_timeout(&fts_data->resource_checkpoint,
				resource_timeout)) {
		pr_err("Resources not acquired in TVM\n");
		goto irq_fail;
	}

	rc = fts_ts_vm_handle_vm_hardware(fts_data);
	if (rc) {
		pr_err(" Delayed probe failure on VM!\n");
		goto irq_fail;
	}

	atomic_set(&fts_data->trusted_touch_enabled, 1);
	return;
irq_fail:
	fts_ts_trusted_touch_vm_mode_disable(fts_data);
}

static void fts_ts_vm_mem_on_lend_handler(enum hh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct hh_rm_notif_mem_shared_payload *payload;
	struct hh_sgl_desc *sgl_desc, *expected_sgl_desc;
	struct hh_acl_desc *acl_desc;
	struct trusted_touch_vm_info *vm_info;
	struct fts_ts_data *fts_data;
	int rc = 0;

	if (notif_type != HH_RM_NOTIF_MEM_SHARED ||
			tag != HH_MEM_NOTIFIER_TAG_TOUCH) {
		pr_err("Invalid command passed from rm\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		pr_err("Invalid entry data passed from rm\n");
		return;
	}

	fts_data = (struct fts_ts_data *)entry_data;
	vm_info = fts_data->vm_info;
	if (!vm_info) {
		pr_err("Invalid vm_info\n");
		return;
	}

	payload = (struct hh_rm_notif_mem_shared_payload  *)notif_msg;
	if (payload->trans_type != HH_RM_TRANS_TYPE_LEND ||
			payload->label != TRUSTED_TOUCH_MEM_LABEL) {
		pr_err("Invalid label or transaction type\n");
		goto onlend_fail;
	}

	acl_desc = fts_ts_vm_get_acl(HH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		pr_err("failed to populated acl data:rc=%d\n",
				PTR_ERR(acl_desc));
		goto onlend_fail;
	}

	sgl_desc = hh_rm_mem_accept(payload->mem_handle, HH_RM_MEM_TYPE_IO,
			HH_RM_TRANS_TYPE_LEND,
			HH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
			HH_RM_MEM_ACCEPT_VALIDATE_LABEL |
			HH_RM_MEM_ACCEPT_DONE,  payload->label, acl_desc,
			NULL, NULL, 0);
	if (IS_ERR_OR_NULL(sgl_desc)) {
		pr_err("failed to do mem accept :rc=%d\n",
				PTR_ERR(sgl_desc));
		goto acl_fail;
	}
	atomic_set(&vm_info->tvm_owns_iomem, 1);

	/* Initiate i2c session on tvm */
	rc = pm_runtime_get_sync(fts_data->client->adapter->dev.parent);
	if (rc < 0) {
		pr_err("failed to get sync rc:%d\n", rc);
		(void)fts_ts_vm_mem_release(fts_data);
		atomic_set(&fts_data->vm_info->tvm_owns_iomem, 0);
		goto acl_fail;
	}
	complete(&fts_data->resource_checkpoint);

	expected_sgl_desc = fts_ts_vm_get_sgl(vm_info);
	if (fts_ts_vm_compare_sgl_desc(expected_sgl_desc, sgl_desc)) {
		pr_err("IO sg list does not match\n");
		goto sgl_cmp_fail;
	}

	vm_info->vm_mem_handle = payload->mem_handle;
	kfree(expected_sgl_desc);
	kfree(acl_desc);
	return;

sgl_cmp_fail:
	kfree(expected_sgl_desc);
acl_fail:
	kfree(acl_desc);
onlend_fail:
	fts_ts_trusted_touch_vm_mode_disable(fts_data);
}

static int fts_ts_vm_mem_release(struct fts_ts_data *fts_data)
{
	int rc = 0;

	rc = hh_rm_mem_release(fts_data->vm_info->vm_mem_handle, 0);
	if (rc)
		pr_err("VM mem release failed: rc=%d\n", rc);

	rc = hh_rm_mem_notify(fts_data->vm_info->vm_mem_handle,
				HH_RM_MEM_NOTIFY_OWNER_RELEASED,
				HH_MEM_NOTIFIER_TAG_TOUCH, 0);
	if (rc)
		pr_err("Failed to notify mem release to PVM: rc=%d\n");

	fts_data->vm_info->vm_mem_handle = 0;
	return rc;
}

static void fts_ts_trusted_touch_vm_mode_disable(struct fts_ts_data *fts_data)
{
	int rc = 0;


	if (atomic_read(&fts_data->vm_info->tvm_owns_iomem) &&
			atomic_read(&fts_data->vm_info->tvm_owns_irq))
		fts_irq_disable();

	if (atomic_read(&fts_data->vm_info->tvm_owns_iomem)) {
		fts_release_all_finger();
		rc = fts_ts_vm_mem_release(fts_data);
		if (rc)
			pr_err("Failed to release mem rc:%d\n", rc);
		else
			atomic_set(&fts_data->vm_info->tvm_owns_iomem, 0);
		pm_runtime_put_sync(fts_data->client->adapter->dev.parent);
	}

	if (atomic_read(&fts_data->vm_info->tvm_owns_irq)) {
		rc = hh_irq_release(fts_data->vm_info->irq_label);
		if (rc)
			pr_err("Failed to release irq rc:%d\n", rc);
		else
			atomic_set(&fts_data->vm_info->tvm_owns_irq, 0);
	}
	atomic_set(&fts_data->trusted_touch_enabled, 0);
	reinit_completion(&fts_data->resource_checkpoint);
	pr_debug("trusted touch disabled\n");
}

int fts_ts_handle_trusted_touch_tvm(struct fts_ts_data *fts_data, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if (atomic_read(&fts_data->trusted_touch_enabled) == 0) {
			pr_err("Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&fts_data->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			fts_ts_trusted_touch_vm_mode_disable(fts_data);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&fts_data->trusted_touch_enabled)) {
			pr_err("Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&fts_data->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			pr_err("Cannot turnon trusted touch(vm mode) in VM\n");
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	default:
		FTS_ERROR("unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}

	return err;
}

#else

static int fts_ts_clk_prepare_enable(struct fts_ts_data *fts_data)
{
	int ret;

	ret = clk_prepare_enable(fts_data->iface_clk);
	if (ret) {
		FTS_ERROR("error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fts_data->core_clk);
	if (ret) {
		clk_disable_unprepare(fts_data->iface_clk);
		FTS_ERROR("error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void fts_ts_clk_disable_unprepare(struct fts_ts_data *fts_data)
{
	clk_disable_unprepare(fts_data->core_clk);
	clk_disable_unprepare(fts_data->iface_clk);
}

static int fts_ts_bus_get(struct fts_ts_data *fts_data)
{
	int rc = 0;

	mutex_lock(&fts_data->fts_clk_io_ctrl_mutex);
	rc = pm_runtime_get_sync(fts_data->client->adapter->dev.parent);
	if (rc >= 0 &&  fts_data->core_clk != NULL &&
				fts_data->iface_clk != NULL) {
		rc = fts_ts_clk_prepare_enable(fts_data);
		if (rc)
			pm_runtime_put_sync(
				fts_data->client->adapter->dev.parent);
	}
	mutex_unlock(&fts_data->fts_clk_io_ctrl_mutex);
	return rc;
}

static void fts_ts_bus_put(struct fts_ts_data *fts_data)
{
	mutex_lock(&fts_data->fts_clk_io_ctrl_mutex);
	if (fts_data->core_clk != NULL && fts_data->iface_clk != NULL)
		fts_ts_clk_disable_unprepare(fts_data);
	pm_runtime_put_sync(fts_data->client->adapter->dev.parent);
	mutex_unlock(&fts_data->fts_clk_io_ctrl_mutex);
}

static struct hh_notify_vmid_desc *fts_ts_vm_get_vmid(hh_vmid_t vmid)
{
	struct hh_notify_vmid_desc *vmid_desc;

	vmid_desc = kzalloc(offsetof(struct hh_notify_vmid_desc,
				vmid_entries[1]), GFP_KERNEL);
	if (!vmid_desc)
		return ERR_PTR(ENOMEM);

	vmid_desc->n_vmid_entries = 1;
	vmid_desc->vmid_entries[0].vmid = vmid;
	return vmid_desc;
}

static void fts_ts_trusted_touch_complete(struct fts_ts_data *fts_data)
{
	if (atomic_read(&fts_data->vm_info->pvm_owns_iomem) &&
			atomic_read(&fts_data->vm_info->pvm_owns_irq)) {
		fts_irq_enable();
		fts_ts_bus_put(fts_data);
		complete(&fts_data->trusted_touch_powerdown);
		atomic_set(&fts_data->trusted_touch_enabled, 0);
		pr_debug("reenabled interrupts on PVM\n");
	} else {
		pr_err("PVM does not own irq and IOMEM\n");
	}
}

static void fts_ts_vm_irq_on_release_callback(void *data,
					enum hh_irq_label label)
{
	struct fts_ts_data *fts_data = data;
	int rc = 0;

	rc = hh_irq_reclaim(fts_data->vm_info->irq_label);
	if (rc)
		pr_err("failed to reclaim irq on pvm rc:%d\n", rc);
	else
		atomic_set(&fts_data->vm_info->pvm_owns_irq, 1);
}

static void fts_ts_vm_mem_on_release_handler(enum hh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct hh_rm_notif_mem_released_payload *payload;
	struct trusted_touch_vm_info *vm_info;
	struct fts_ts_data *fts_data;
	int rc = 0;

	if (notif_type != HH_RM_NOTIF_MEM_RELEASED ||
			tag != HH_MEM_NOTIFIER_TAG_TOUCH) {
		pr_err(" Invalid tag or command passed\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		pr_err(" Invalid data or notification message\n");
		return;
	}

	payload = (struct hh_rm_notif_mem_released_payload  *)notif_msg;
	fts_data = (struct fts_ts_data *)entry_data;
	vm_info = fts_data->vm_info;
	if (!vm_info) {
		pr_err(" Invalid vm_info\n");
		return;
	}

	if (payload->mem_handle != vm_info->vm_mem_handle) {
		pr_err("Invalid mem handle detected\n");
		return;
	}

	rc = hh_rm_mem_reclaim(payload->mem_handle, 0);
	if (rc) {
		pr_err("Trusted touch VM mem release failed rc:%d\n", rc);
		return;
	}
	atomic_set(&vm_info->pvm_owns_iomem, 1);
	vm_info->vm_mem_handle = 0;
}

static int fts_ts_vm_mem_lend(struct fts_ts_data *fts_data)
{
	struct hh_acl_desc *acl_desc;
	struct hh_sgl_desc *sgl_desc;
	struct hh_notify_vmid_desc *vmid_desc;
	hh_memparcel_handle_t mem_handle;
	hh_vmid_t trusted_vmid;
	int rc = 0;

	acl_desc = fts_ts_vm_get_acl(HH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		pr_err("Failed to get acl of IO memories for Trusted touch\n");
		PTR_ERR(acl_desc);
		return -EINVAL;
	}

	sgl_desc = fts_ts_vm_get_sgl(fts_data->vm_info);
	if (IS_ERR(sgl_desc)) {
		pr_err("Failed to get sgl of IO memories for Trusted touch\n");
		PTR_ERR(sgl_desc);
		rc = -EINVAL;
		goto sgl_error;
	}

	rc = hh_rm_mem_lend(HH_RM_MEM_TYPE_IO, 0, TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, sgl_desc, NULL, &mem_handle);
	if (rc) {
		pr_err("Failed to lend IO memories for Trusted touch rc:%d\n",
							rc);
		goto error;
	}

	hh_rm_get_vmid(HH_TRUSTED_VM, &trusted_vmid);

	vmid_desc = fts_ts_vm_get_vmid(trusted_vmid);

	rc = hh_rm_mem_notify(mem_handle, HH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
			HH_MEM_NOTIFIER_TAG_TOUCH, vmid_desc);
	if (rc) {
		pr_err("Failed to notify mem lend to hypervisor rc:%d\n", rc);
		goto vmid_error;
	}

	fts_data->vm_info->vm_mem_handle = mem_handle;
vmid_error:
	kfree(vmid_desc);
error:
	kfree(sgl_desc);
sgl_error:
	kfree(acl_desc);

	return rc;
}

static int fts_ts_trusted_touch_vm_mode_enable(struct fts_ts_data *fts_data)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info = fts_data->vm_info;

	/* i2c session start and resource acquire */
	if (fts_ts_bus_get(fts_data) < 0) {
		FTS_ERROR("fts_ts_bus_get failed\n");
		rc = -EIO;
		return rc;
	}

	/* flush pending interurpts from FIFO */
	fts_irq_disable();
	fts_release_all_finger();

	rc = fts_ts_vm_mem_lend(fts_data);
	if (rc) {
		pr_err("Failed to lend memory\n");
		return -EINVAL;
	}
	atomic_set(&vm_info->pvm_owns_iomem, 0);

	rc = hh_irq_lend(vm_info->irq_label, vm_info->vm_name,
		fts_data->irq, &fts_ts_vm_irq_on_release_callback, fts_data);
	if (rc) {
		pr_err("Failed to lend irq\n");
		return -EINVAL;
	}
	atomic_set(&vm_info->pvm_owns_irq, 0);

	reinit_completion(&fts_data->trusted_touch_powerdown);
	atomic_set(&fts_data->trusted_touch_enabled, 1);
	pr_debug("trusted touch enabled\n");
	return rc;
}

int fts_ts_handle_trusted_touch_pvm(struct fts_ts_data *fts_data, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if (atomic_read(&fts_data->trusted_touch_enabled) == 0) {
			pr_err("Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&fts_data->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			fts_ts_trusted_touch_complete(fts_data);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&fts_data->trusted_touch_enabled)) {
			pr_err("Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&fts_data->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			err = fts_ts_trusted_touch_vm_mode_enable(fts_data);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	default:
		FTS_ERROR("unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

#endif

static int fts_ts_vm_init(struct fts_ts_data *fts_data)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	void *mem_cookie;

	rc = fts_ts_populate_vm_info(fts_data);
	if (rc) {
		pr_err("Cannot setup vm pipeline\n");
		rc = -EINVAL;
		goto fail;
	}

	vm_info = fts_data->vm_info;
#ifdef CONFIG_ARCH_QTI_VM
	mem_cookie = hh_mem_notifier_register(HH_MEM_NOTIFIER_TAG_TOUCH,
			fts_ts_vm_mem_on_lend_handler, fts_data);
	if (!mem_cookie) {
		pr_err("Failed to register on lend mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	rc = hh_irq_wait_for_lend(vm_info->irq_label, HH_PRIMARY_VM,
			&fts_ts_vm_irq_on_lend_callback, fts_data);
	atomic_set(&vm_info->tvm_owns_irq, 0);
	atomic_set(&vm_info->tvm_owns_iomem, 0);
	init_completion(&fts_data->resource_checkpoint);
#else
	mem_cookie = hh_mem_notifier_register(HH_MEM_NOTIFIER_TAG_TOUCH,
			fts_ts_vm_mem_on_release_handler, fts_data);
	if (!mem_cookie) {
		pr_err("Failed to register on release mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	atomic_set(&vm_info->pvm_owns_irq, 1);
	atomic_set(&vm_info->pvm_owns_iomem, 1);
#endif
	return rc;
init_fail:
	fts_ts_vm_deinit(fts_data);
fail:
	return rc;
}

static void fts_ts_dt_parse_trusted_touch_info(struct fts_ts_data *fts_data)
{
	struct device_node *np = fts_data->client->dev.of_node;
	int rc = 0;
	const char *selection;
	const char *environment;

	rc = of_property_read_string(np, "focaltech,trusted-touch-mode",
								&selection);
	if (rc) {
		dev_warn(&fts_data->client->dev,
			"%s: No trusted touch mode selection made\n", __func__);
		atomic_set(&fts_data->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		return;
	}

	if (!strcmp(selection, "vm_mode")) {
		atomic_set(&fts_data->trusted_touch_mode,
						TRUSTED_TOUCH_VM_MODE);
		pr_err("Selected trusted touch mode to VM mode\n");
	} else {
		atomic_set(&fts_data->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		pr_err("Invalid trusted_touch mode\n");
	}

	rc = of_property_read_string(np, "focaltech,touch-environment",
						&environment);
	if (rc) {
		dev_warn(&fts_data->client->dev,
			"%s: No trusted touch mode environment\n", __func__);
	}
	fts_data->touch_environment = environment;
	pr_err("Trusted touch environment:%s\n",
			fts_data->touch_environment);
}

static void fts_ts_trusted_touch_init(struct fts_ts_data *fts_data)
{
	int rc = 0;

	atomic_set(&fts_data->trusted_touch_initialized, 0);
	fts_ts_dt_parse_trusted_touch_info(fts_data);

	if (atomic_read(&fts_data->trusted_touch_mode) ==
						TRUSTED_TOUCH_MODE_NONE)
		return;

	init_completion(&fts_data->trusted_touch_powerdown);

	/* Get clocks */
	fts_data->core_clk = devm_clk_get(fts_data->client->dev.parent,
						"m-ahb");
	if (IS_ERR(fts_data->core_clk)) {
		fts_data->core_clk = NULL;
		dev_warn(&fts_data->client->dev,
				"%s: core_clk is not defined\n", __func__);
	}

	fts_data->iface_clk = devm_clk_get(fts_data->client->dev.parent,
						"se-clk");
	if (IS_ERR(fts_data->iface_clk)) {
		fts_data->iface_clk = NULL;
		dev_warn(&fts_data->client->dev,
			"%s: iface_clk is not defined\n", __func__);
	}

	if (atomic_read(&fts_data->trusted_touch_mode) ==
						TRUSTED_TOUCH_VM_MODE) {
		rc = fts_ts_vm_init(fts_data);
		if (rc)
			pr_err("Failed to init VM\n");
	}
	atomic_set(&fts_data->trusted_touch_initialized, 1);
}

#endif

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	u8 idl = 0;
	u8 chip_idh = fts_data->ic_info.ids.chip_idh;
	u8 chip_idl = fts_data->ic_info.ids.chip_idl;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &idl);
		if ((ret < 0) || (idh != chip_idh) || (idl != chip_idl)) {
			FTS_DEBUG("TP Not Ready,ReadData:0x%02x%02x", idh, idl);
		} else if ((idh == chip_idh) && (idl == chip_idl)) {
			FTS_INFO("TP Ready,Device ID:0x%02x%02x", idh, idl);
			return 0;
		}
		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid();
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(ts_data);
	/* recover TP gesture state 0xD0 */
	fts_gesture_recovery(ts_data);
	FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	FTS_DEBUG("tp reset");
	gpio_direction_output(fts_data->pdata->reset_gpio, 0);
	msleep(1);
	gpio_direction_output(fts_data->pdata->reset_gpio, 1);
	if (hdelayms) {
		msleep(hdelayms);
	}

	return 0;
}

void fts_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = {0xEB, 0xAA, 0x09};

	ret = fts_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
		return;
	}

	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	ret = fts_read(NULL, 0, buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd read fail");
	} else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2])) {
		FTS_DEBUG("hidi2c change to stdi2c successful");
	} else {
		FTS_DEBUG("hidi2c change to stdi2c not support or fail");
	}

}

static int fts_get_chip_types(
	struct fts_ts_data *ts_data,
	u8 id_h, u8 id_l, bool fw_valid)
{
	int i = 0;
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

	if ((0x0 == id_h) || (0x0 == id_l)) {
		FTS_ERROR("id_h/id_l is 0");
		return -EINVAL;
	}

	FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
				|| ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
				|| ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
			break;
		}
	}

	if (i >= ctype_entries)
		return -ENODATA;

	ts_data->ic_info.ids = ctype[i];
	return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
	int ret = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	u32 id_cmd_len = 0;

	id_cmd[0] = FTS_CMD_START1;
	id_cmd[1] = FTS_CMD_START2;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		FTS_ERROR("start cmd write fail");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);
	id_cmd[0] = FTS_CMD_READ_ID;
	id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
	if (ts_data->ic_info.is_incell)
		id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		id_cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
	if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
		FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
		return -EIO;
	}

	id[0] = chip_id[0];
	id[1] = chip_id[1];
	return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("i2c read invalid, read:0x%02x%02x",
				chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
			if (!ret)
				break;
            else
                FTS_DEBUG("TP not ready, read:0x%02x%02x",
                          chip_id[0], chip_id[1]);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		if (ts_data->ic_info.hid_supported) {
			fts_hid2std();
		}

		ret = fts_read_bootid(ts_data, &chip_id[0]);
		if (ret <  0) {
			FTS_ERROR("read boot id fail");
			return ret;
		}

		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}

	FTS_INFO("get ic information, chip id = 0x%02x%02x",
		ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

	return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, int datalen)
{
	int i = 0;
	int count = 0;
	char *tmpbuf = NULL;

	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
		if (count >= 1024)
			break;
	}
	FTS_DEBUG("point buffer:%s", tmpbuf);

	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
}

void fts_release_all_finger(void)
{
	struct input_dev *input_dev = fts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
	u32 finger_count = 0;
	u32 max_touches = fts_data->pdata->max_touch_number;
#endif

	FTS_FUNC_ENTER();
	mutex_lock(&fts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(input_dev);
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);

	fts_data->touchs = 0;
	fts_data->key_state = 0;
	mutex_unlock(&fts_data->report_mutex);
	FTS_FUNC_EXIT();
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	int i = 0;
	int x = data->events[index].x;
	int y = data->events[index].y;
	int *x_dim = &data->pdata->key_x_coords[0];
	int *y_dim = &data->pdata->key_y_coords[0];

	if (!data->pdata->have_key) {
		return -EINVAL;
	}

	for (i = 0; i < data->pdata->key_number; i++) {
		if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
			(y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
			if (EVENT_DOWN(data->events[index].flag)
				&& !(data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->pdata->keys[i], 1);
				data->key_state |= (1 << i);
				FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
			} else if (EVENT_UP(data->events[index].flag)
				&& (data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->pdata->keys[i], 0);
				data->key_state &= ~(1 << i);
				FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
			}
			return 0;
		}
	}
	return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	bool va_reported = false;
	u32 max_touch_num = data->pdata->max_touch_number;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0)
			continue;

		va_reported = true;
		input_mt_slot(data->input_dev, events[i].id);

		if (EVENT_DOWN(events[i].flag)) {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);

			if ((data->log_level >= 2) ||
				((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
					events[i].id,
					events[i].x, events[i].y,
					events[i].p, events[i].area);
			}
		} else {
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			data->touchs &= ~BIT(events[i].id);
			if (data->log_level >= 1) {
				FTS_DEBUG("[B]P%d UP!", events[i].id);
			}
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < max_touch_num; i++)  {
			if (BIT(i) & (data->touchs ^ touchs)) {
				if (data->log_level >= 1) {
					FTS_DEBUG("[B]P%d UP!", i);
				}
			va_reported = true;
			input_mt_slot(data->input_dev, i);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;

	if (va_reported) {
		/* touchs==0, there's no point but key */
		if (EVENT_NO_DOWN(data) || (!touchs)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[B]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
	int i = 0;
	int touchs = 0;
	bool va_reported = false;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0) {
			continue;
		}

		va_reported = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

			input_mt_sync(data->input_dev);

			if ((data->log_level >= 2) ||
				((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
					events[i].id,
					events[i].x, events[i].y,
					events[i].p, events[i].area);
			}
			touchs++;
		}
	}

	/* last point down, current no point but key */
	if (data->touchs && !touchs) {
		va_reported = true;
	}
	data->touchs = touchs;

	if (va_reported) {
		if (EVENT_NO_DOWN(data)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[A]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			input_mt_sync(data->input_dev);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}
#endif

static int fts_read_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	u8 *buf = data->point_buf;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x01;

	if (data->gesture_mode) {
		if (0 == fts_gesture_readdata(data, NULL)) {
			FTS_INFO("succuss to get gesture data in irq handler");
			return 1;
		}
	}

	ret = fts_read(buf, 1, buf + 1, data->pnt_buf_size - 1);
	if (ret < 0) {
		FTS_ERROR("read touchdata failed, ret:%d", ret);
		return ret;
	}

	if (data->log_level >= 3) {
		fts_show_touch_buffer(buf, data->pnt_buf_size);
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int i = 0;
	u8 pointid = 0;
	int base = 0;
	struct ts_event *events = data->events;
	int max_touch_num = data->pdata->max_touch_number;
	u8 *buf = data->point_buf;

	ret = fts_read_touchdata(data);
	if (ret) {
		return ret;
	}

	data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	data->touch_point = 0;

	if (data->ic_info.is_incell) {
		if ((data->point_num == 0x0F) && (buf[2] == 0xFF) && (buf[3] == 0xFF)
			&& (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
			FTS_DEBUG("touch buff is 0xff, need recovery state");
			fts_release_all_finger();
			fts_tp_state_recovery(data);
			return -EIO;
		}
	}

	if (data->point_num > max_touch_num) {
		FTS_INFO("invalid point_num(%d)", data->point_num);
		return -EIO;
	}

	for (i = 0; i < max_touch_num; i++) {
		base = FTS_ONE_TCH_LEN * i;
		pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else if (pointid >= max_touch_num) {
			FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			return -EINVAL;
		}

		data->touch_point++;
		events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
				(buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
				(buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
		events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
		events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
		events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

		if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
			FTS_INFO("abnormal touch data from fw");
			return -EIO;
		}
	}

	if (data->touch_point == 0) {
		FTS_INFO("no touch point information");
		return -EIO;
	}

	return 0;
}

static void fts_irq_read_report(void)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(1);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_prc_queue_work(ts_data);
#endif

	ret = fts_read_parse_touchdata(ts_data);
	if (ret == 0) {
		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data);
#else
		fts_input_report_a(ts_data);
#endif
		mutex_unlock(&ts_data->report_mutex);
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(0);
#endif
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
	struct fts_ts_data *fts_data = data;

	if (!fts_data) {
		pr_err("%s: Invalid fts_data\n", __func__);
		return IRQ_HANDLED;
	}

#ifdef CONFIG_FTS_TRUSTED_TOUCH
#ifndef CONFIG_ARCH_QTI_VM
	if (atomic_read(&fts_data->trusted_touch_enabled)) {
		if (atomic_read(&fts_data->vm_info->pvm_owns_iomem) &&
			atomic_read(&fts_data->vm_info->pvm_owns_irq)) {
			pr_err("Trusted Touch Enabled: Cannot service irq\n");
			return IRQ_HANDLED;
		}
	}
#endif
#endif
	fts_irq_read_report();
	return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

#ifdef CONFIG_ARCH_QTI_VM
	pdata->irq_gpio_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
				pdata->irq_gpio_flags,
				FTS_DRIVER_NAME, ts_data);
#else
	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
				pdata->irq_gpio_flags,
				FTS_DRIVER_NAME, ts_data);
#endif
	return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int key_num = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;

	FTS_FUNC_ENTER();
	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;

	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = ts_data->dev;

	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	if (pdata->have_key) {
		FTS_INFO("set key capabilities");
		for (key_num = 0; key_num < pdata->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
	}

#if FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		input_dev = NULL;
		return ret;
	}

	ts_data->input_dev = input_dev;

	FTS_FUNC_EXIT();
	return 0;
}

static int fts_report_buffer_init(struct fts_ts_data *ts_data)
{
	int point_num = 0;
	int events_num = 0;

	point_num = ts_data->pdata->max_touch_number;
	ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
	ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size + 1, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf");
		return -ENOMEM;
	}

	events_num = point_num * sizeof(struct ts_event);
	ts_data->events = (struct ts_event *)kzalloc(events_num, GFP_KERNEL);
	if (!ts_data->events) {
		FTS_ERROR("failed to alloc memory for point events");
		kfree_safe(ts_data->point_buf);
		return -ENOMEM;
	}

	return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
	int ret = 0;

	ts->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl)) {
		FTS_ERROR("Failed to get pinctrl, please check dts");
		ret = PTR_ERR(ts->pinctrl);
		goto err_pinctrl_get;
	}

	ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ts->pins_active)) {
		FTS_ERROR("Pin state[active] not found");
		ret = PTR_ERR(ts->pins_active);
		goto err_pinctrl_lookup;
	}

	ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ts->pins_suspend)) {
		FTS_ERROR("Pin state[suspend] not found");
		ret = PTR_ERR(ts->pins_suspend);
		goto err_pinctrl_lookup;
	}

	ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
	if (IS_ERR_OR_NULL(ts->pins_release)) {
		FTS_ERROR("Pin state[release] not found");
		ret = PTR_ERR(ts->pins_release);
	}

	return 0;
err_pinctrl_lookup:
	if (ts->pinctrl) {
		devm_pinctrl_put(ts->pinctrl);
	}
err_pinctrl_get:
	ts->pinctrl = NULL;
	ts->pins_release = NULL;
	ts->pins_suspend = NULL;
	ts->pins_active = NULL;
	return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_active) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
		if (ret < 0) {
			FTS_ERROR("Set normal pin state error:%d", ret);
		}
	}

	return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_suspend) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
		if (ret < 0) {
			FTS_ERROR("Set suspend pin state error:%d", ret);
		}
	}

	return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl) {
		if (IS_ERR_OR_NULL(ts->pins_release)) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
			if (ret < 0)
				FTS_ERROR("Set gesture pin state error:%d", ret);
		}
	}

	return ret;
}
#endif /* FTS_PINCTRL_EN */

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		FTS_ERROR("vdd is invalid");
		return -EINVAL;
	}

	FTS_FUNC_ENTER();
	if (enable) {
		if (ts_data->power_disabled) {
			FTS_DEBUG("regulator enable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_enable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
			}

			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_enable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
				}
			}
			ts_data->power_disabled = false;
		}
	} else {
		if (!ts_data->power_disabled) {
			FTS_DEBUG("regulator disable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_disable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
			}
			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_disable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
				}
			}
			ts_data->power_disabled = true;
		}
	}

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts_data->vdd = regulator_get(ts_data->dev, "vdd");
	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		ret = PTR_ERR(ts_data->vdd);
		FTS_ERROR("get vdd regulator failed,ret=%d", ret);
		return ret;
	}

	if (regulator_count_voltages(ts_data->vdd) > 0) {
		ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
						FTS_VTG_MAX_UV);
		if (ret) {
			FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}

		ret = regulator_set_load(ts_data->vdd, FTS_LOAD_MAX_UA);
		if (ret) {
			FTS_ERROR("vdd regulator set_load failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}
	}

	ts_data->vcc_i2c = regulator_get(ts_data->dev, "vcc_i2c");
	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
			ret = regulator_set_voltage(ts_data->vcc_i2c,
						FTS_I2C_VTG_MIN_UV,
						FTS_I2C_VTG_MAX_UV);
			if (ret) {
				FTS_ERROR("vcc_i2c regulator set_vtg failed,ret=%d", ret);
				regulator_put(ts_data->vcc_i2c);
			}
		}
	}

#if FTS_PINCTRL_EN
	fts_pinctrl_init(ts_data);
	fts_pinctrl_select_normal(ts_data);
#endif

	ts_data->power_disabled = true;
	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret) {
		FTS_ERROR("fail to enable power(regulator)");
	}

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif

	fts_power_source_ctrl(ts_data, DISABLE);

	if (!IS_ERR_OR_NULL(ts_data->vdd)) {
		if (regulator_count_voltages(ts_data->vdd) > 0)
			regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
		regulator_put(ts_data->vdd);
	}

	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
			regulator_set_voltage(ts_data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
		regulator_put(ts_data->vcc_i2c);
	}

	return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_suspend(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, DISABLE);
	if (ret < 0) {
		FTS_ERROR("power off fail, ret=%d", ret);
	}

	return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_normal(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret < 0) {
		FTS_ERROR("power on fail, ret=%d", ret);
	}

	return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	/* request irq gpio */
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}

		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}

	/* request reset gpio */
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]reset gpio request failed");
			goto err_irq_gpio_dir;
		}

		ret = gpio_direction_output(data->pdata->reset_gpio, 1);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for reset gpio failed");
			goto err_reset_gpio_dir;
		}
	}

	FTS_FUNC_EXIT();
	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
				struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		FTS_ERROR("invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		FTS_ERROR("Unable to read %s, please check dts", name);
		pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
		pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
		pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
		pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
		return -ENODATA;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	}

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
		pdata->y_min, pdata->y_max);
	return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;

	FTS_FUNC_ENTER();

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	/* key */
	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech,keys",
						pdata->keys, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Keys undefined!");
		else if (pdata->key_number > FTS_MAX_KEYS)
			pdata->key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
						pdata->key_x_coords,
						pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key Y Coords undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
						pdata->key_y_coords,
						pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key X Coords undefined!");

		FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
			"coords:(%d,%d),(%d,%d),(%d,%d)",
			pdata->key_number,
			pdata->keys[0], pdata->keys[1], pdata->keys[2],
			pdata->key_x_coords[0], pdata->key_y_coords[0],
			pdata->key_x_coords[1], pdata->key_y_coords[1],
			pdata->key_x_coords[2], pdata->key_y_coords[2]);
	}

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
			0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		FTS_ERROR("Unable to get reset_gpio");

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
			0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		FTS_ERROR("Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		if (temp_val < 2)
			pdata->max_touch_number = 2; /* max_touch_number must >= 2 */
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}

	FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
		pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);

	FTS_FUNC_EXIT();
	return 0;
}

#if defined(CONFIG_DRM)
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
					resume_work);

	fts_ts_resume(ts_data->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct drm_panel_notifier *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
			fb_notif);

	if (!evdata)
		return 0;

	if (!(event == DRM_PANEL_EARLY_EVENT_BLANK ||
		event == DRM_PANEL_EVENT_BLANK)) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_DEBUG("FB event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		if (event == DRM_PANEL_EARLY_EVENT_BLANK) {
			FTS_DEBUG("resume: event = %lu, not care\n", event);
		} else if (event == DRM_PANEL_EVENT_BLANK) {
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		}
		break;

	case DRM_PANEL_BLANK_POWERDOWN:
		if (event == DRM_PANEL_EARLY_EVENT_BLANK) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (event == DRM_PANEL_EVENT_BLANK) {
			FTS_DEBUG("suspend: event = %lu, not care\n", event);
		}
		break;

	default:
		FTS_DEBUG("FB BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}

#elif defined(CONFIG_FB)
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
					resume_work);

	fts_ts_resume(ts_data->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
					fb_notif);

	if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_INFO("FB event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case FB_BLANK_UNBLANK:
		if (FB_EARLY_EVENT_BLANK == event) {
			FTS_INFO("resume: event = %lu, not care\n", event);
		} else if (FB_EVENT_BLANK == event) {
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		}
		break;

	case FB_BLANK_POWERDOWN:
		if (FB_EARLY_EVENT_BLANK == event) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (FB_EVENT_BLANK == event) {
			FTS_INFO("suspend: event = %lu, not care\n", event);
		}
		break;

	default:
		FTS_INFO("FB BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
					early_suspend);

	fts_ts_suspend(ts_data->dev);
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
					early_suspend);

	fts_ts_resume(ts_data->dev);
}
#endif

static int fts_ts_probe_delayed(struct fts_ts_data *fts_data)
{
	int error = 0;
	int ret = 0;

/* Avoid setting up hardware for TVM during probe */
#ifdef CONFIG_FTS_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
	if (!atomic_read(&fts_data->delayed_vm_probe_pending)) {
		atomic_set(&fts_data->delayed_vm_probe_pending, 1);
		return 0;
	}
	goto tvm_setup;
#endif
#endif
	ret = fts_gpio_configure(fts_data);
	if (ret) {
		FTS_ERROR("configure the gpios fail");
		goto err_gpio_config;
	}

#if FTS_POWER_SOURCE_CUST_EN
	ret = fts_power_source_init(fts_data);
	if (ret) {
		FTS_ERROR("fail to get power(regulator)");
		goto err_power_init;
	}
#endif

#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	ret = fts_get_ic_information(fts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_irq_req;
	}

#ifdef CONFIG_ARCH_QTI_VM
tvm_setup:
#endif
	error = fts_irq_registration(fts_data);
	if (error) {
		FTS_ERROR("request irq failed");
#ifdef CONFIG_ARCH_QTI_VM
		return error;
#endif
		goto err_irq_req;
	}

#ifdef CONFIG_ARCH_QTI_VM
	return error;
#endif

	ret = fts_fwupg_init(fts_data);
	if (ret)
		FTS_ERROR("init fw upgrade fail");

	return error;

err_irq_req:
	if (gpio_is_valid(fts_data->pdata->reset_gpio))
		gpio_free(fts_data->pdata->reset_gpio);
	if (gpio_is_valid(fts_data->pdata->irq_gpio))
		gpio_free(fts_data->pdata->irq_gpio);
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
	fts_power_source_exit(fts_data);
#endif
err_gpio_config:
	return error;
}

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int pdata_size = sizeof(struct fts_ts_platform_data);

	FTS_FUNC_ENTER();
	FTS_INFO("%s", FTS_DRIVER_VERSION);
	ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		FTS_ERROR("allocate memory for platform_data fail");
		return -ENOMEM;
	}

	if (ts_data->dev->of_node) {
		ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
		if (ret)
			FTS_ERROR("device-tree parse fail");
	} else {
		if (ts_data->dev->platform_data) {
			memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
		} else {
			FTS_ERROR("platform_data is null");
			return -ENODEV;
		}
	}

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue) {
		FTS_ERROR("create fts workqueue fail");
	}

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);

	/* Init communication interface */
	ret = fts_bus_init(ts_data);
	if (ret) {
		FTS_ERROR("bus initialize fail");
		goto err_bus_init;
	}

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("input initialize fail");
		goto err_input_init;
	}

	ret = fts_report_buffer_init(ts_data);
	if (ret) {
		FTS_ERROR("report buffer init fail");
		goto err_report_buffer;
	}

	ret = fts_create_apk_debug_channel(ts_data);
	if (ret) {
		FTS_ERROR("create apk debug node fail");
	}

	ret = fts_create_sysfs(ts_data);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
	}

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
	}
#endif

	ret = fts_ex_mode_init(ts_data);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
	}

	ret = fts_gesture_init(ts_data);
	if (ret) {
		FTS_ERROR("init gesture fail");
	}


#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret) {
		FTS_ERROR("init esd check fail");
	}
#endif

#if defined(CONFIG_DRM)
	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
	}
	ts_data->fb_notif.notifier_call = fb_notifier_callback;

	if (active_panel &&
		drm_panel_notifier_register(active_panel,
			&ts_data->fb_notif) < 0)
		FTS_ERROR("register notifier failed!\n");

#elif defined(CONFIG_FB)
	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
	}
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
	if (ret) {
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	ts_data->early_suspend.suspend = fts_ts_early_suspend;
	ts_data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&ts_data->early_suspend);
#endif

#ifdef CONFIG_FTS_TRUSTED_TOUCH
	fts_ts_trusted_touch_init(ts_data);
	mutex_init(&(ts_data->fts_clk_io_ctrl_mutex));
#endif
	ret = fts_ts_probe_delayed(ts_data);
	if (ret) {
		FTS_ERROR("Failed to enable resources\n");
		goto err_probe_delayed;
	}

	FTS_FUNC_EXIT();
	return 0;

err_probe_delayed:
	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);
err_report_buffer:
	input_unregister_device(ts_data->input_dev);
err_input_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
	kfree_safe(ts_data->bus_tx_buf);
	kfree_safe(ts_data->bus_rx_buf);
	kfree_safe(ts_data->pdata);

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit(ts_data);
#endif

	fts_release_apk_debug_channel(ts_data);
	fts_remove_sysfs(ts_data);
	fts_ex_mode_exit(ts_data);

	fts_fwupg_exit(ts_data);


#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

	fts_gesture_exit(ts_data);
	fts_bus_exit(ts_data);

	free_irq(ts_data->irq, ts_data);
	input_unregister_device(ts_data->input_dev);

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

#if defined(CONFIG_DRM)
	if (active_panel)
		drm_panel_notifier_unregister(active_panel, &ts_data->fb_notif);

#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
#endif

	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);

	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);

#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_exit(ts_data);
#endif

	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);

	kfree_safe(ts_data->pdata);
	kfree_safe(ts_data);

	FTS_FUNC_EXIT();

	return 0;
}

static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	FTS_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}
#ifdef CONFIG_FTS_TRUSTED_TOUCH
	if (atomic_read(&fts_data->trusted_touch_enabled))
		wait_for_completion_interruptible(
			&fts_data->trusted_touch_powerdown);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend();
#endif

	if (ts_data->gesture_mode) {
		fts_gesture_suspend(ts_data);
	} else {
		fts_irq_disable();

		FTS_INFO("make TP enter into sleep mode");
		ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

		if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
			ret = fts_power_source_suspend(ts_data);
			if (ret < 0) {
				FTS_ERROR("power enter suspend fail");
			}
#endif
		}
	}

	fts_release_all_finger();
	ts_data->suspended = true;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = fts_data;

	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}

	fts_release_all_finger();

	if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
		fts_power_source_resume(ts_data);
#endif
		fts_reset_proc(200);
	}

	fts_wait_tp_to_valid();
	fts_ex_mode_recovery(ts_data);

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume();
#endif

	if (ts_data->gesture_mode) {
		fts_gesture_resume(ts_data);
	} else {
		fts_irq_enable();
	}

	ts_data->suspended = false;
	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
* TP Driver
*****************************************************************************/
static int fts_ts_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}
	}

	return PTR_ERR(panel);
}

static int fts_ts_check_default_tp(struct device_node *dt, const char *prop)
{
	const char **active_tp = NULL;
	int count, tmp, score = 0;
	const char *active;
	int ret, i;

	count = of_property_count_strings(dt->parent, prop);
	if (count <= 0 || count > 3)
		return -ENODEV;

	active_tp = kcalloc(count, sizeof(char *),  GFP_KERNEL);
	if (!active_tp) {
		FTS_ERROR("FTS alloc failed\n");
		return -ENOMEM;
	}

	ret = of_property_read_string_array(dt->parent, prop,
			active_tp, count);
	if (ret < 0) {
		FTS_ERROR("fail to read %s %d\n", prop, ret);
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < count; i++) {
		active = active_tp[i];
		if (active != NULL) {
			tmp = of_device_is_compatible(dt, active);
			if (tmp > 0)
				score++;
		}
	}

	if (score <= 0) {
		FTS_INFO("not match this driver\n");
		ret = -ENODEV;
		goto out;
	}
	ret = 0;
out:
	kfree(active_tp);
	return ret;
}

static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct fts_ts_data *ts_data = NULL;
	struct device_node *dp = client->dev.of_node;

	FTS_INFO("Touch Screen(I2C BUS) driver prboe...");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		return -ENODEV;
	}

	ret = fts_ts_check_dt(dp);
	if (ret == -EPROBE_DEFER)
		return ret;

	if (ret) {
		if (!fts_ts_check_default_tp(dp, "qcom,i2c-touch-active"))
			ret = -EPROBE_DEFER;
		else
			ret = -ENODEV;

		return ret;
	}

	/* malloc memory for global struct variable */
	ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("allocate memory for fts_data fail");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->client = client;
	ts_data->dev = &client->dev;
	ts_data->log_level = 1;
	ts_data->fw_is_running = 0;
	i2c_set_clientdata(client, ts_data);

	ret = fts_ts_probe_entry(ts_data);
	if (ret) {
		FTS_ERROR("Touch Screen(I2C BUS) driver probe fail");
		kfree_safe(ts_data);
		return ret;
	}

	FTS_INFO("Touch Screen(I2C BUS) driver prboe successfully");
	return 0;
}

static int fts_ts_remove(struct i2c_client *client)
{
	return fts_ts_remove_entry(i2c_get_clientdata(client));
}

static const struct i2c_device_id fts_ts_id[] = {
	{FTS_DRIVER_NAME, 0},
	{},
};
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "focaltech,fts_ts", },
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ret = i2c_add_driver(&fts_ts_driver);
	if ( ret != 0 ) {
		FTS_ERROR("Focaltech touch screen driver init failed!");
	}
	FTS_FUNC_EXIT();
	return ret;
}

static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}
#ifdef CONFIG_ARCH_QTI_VM
module_init(fts_ts_init);
#else
late_initcall(fts_ts_init);
#endif
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
