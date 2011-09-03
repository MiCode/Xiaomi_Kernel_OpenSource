/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/fb.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/android_pmem.h>
#include <linux/vmalloc.h>
#include <linux/pm_runtime.h>

#include <linux/ashmem.h>
#include <linux/major.h>

#include "kgsl.h"
#include "kgsl_debugfs.h"
#include "kgsl_cffdump.h"
#include "kgsl_log.h"
#include "kgsl_sharedmem.h"
#include "kgsl_device.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

static int kgsl_pagetable_count = KGSL_PAGETABLE_COUNT;
module_param_named(ptcount, kgsl_pagetable_count, int, 0);
MODULE_PARM_DESC(kgsl_pagetable_count,
"Minimum number of pagetables for KGSL to allocate at initialization time");

static inline struct kgsl_mem_entry *
kgsl_mem_entry_create(void)
{
	struct kgsl_mem_entry *entry = kzalloc(sizeof(*entry), GFP_KERNEL);

	if (!entry)
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*entry));
	else
		kref_init(&entry->refcount);

	return entry;
}

void
kgsl_mem_entry_destroy(struct kref *kref)
{
	struct kgsl_mem_entry *entry = container_of(kref,
						    struct kgsl_mem_entry,
						    refcount);
	size_t size = entry->memdesc.size;

	kgsl_sharedmem_free(&entry->memdesc);

	if (entry->memtype == KGSL_USER_MEMORY)
		entry->priv->stats.user -= size;
	else if (entry->memtype == KGSL_MAPPED_MEMORY) {
		if (entry->file_ptr)
			fput(entry->file_ptr);

		kgsl_driver.stats.mapped -= size;
		entry->priv->stats.mapped -= size;
	}

	kfree(entry);
}
EXPORT_SYMBOL(kgsl_mem_entry_destroy);

static
void kgsl_mem_entry_attach_process(struct kgsl_mem_entry *entry,
				   struct kgsl_process_private *process)
{
	spin_lock(&process->mem_lock);
	list_add(&entry->list, &process->mem_list);
	spin_unlock(&process->mem_lock);

	entry->priv = process;
}

/* Allocate a new context id */

static struct kgsl_context *
kgsl_create_context(struct kgsl_device_private *dev_priv)
{
	struct kgsl_context *context;
	int ret, id;

	context = kzalloc(sizeof(*context), GFP_KERNEL);

	if (context == NULL)
		return NULL;

	while (1) {
		if (idr_pre_get(&dev_priv->device->context_idr,
				GFP_KERNEL) == 0) {
			kfree(context);
			return NULL;
		}

		ret = idr_get_new(&dev_priv->device->context_idr,
				  context, &id);

		if (ret != -EAGAIN)
			break;
	}

	if (ret) {
		kfree(context);
		return NULL;
	}

	context->id = id;
	context->dev_priv = dev_priv;

	return context;
}

static void
kgsl_destroy_context(struct kgsl_device_private *dev_priv,
		     struct kgsl_context *context)
{
	int id;

	if (context == NULL)
		return;

	/* Fire a bug if the devctxt hasn't been freed */
	BUG_ON(context->devctxt);

	id = context->id;
	kfree(context);

	idr_remove(&dev_priv->device->context_idr, id);
}

/* to be called when a process is destroyed, this walks the memqueue and
 * frees any entryies that belong to the dying process
 */
static void kgsl_memqueue_cleanup(struct kgsl_device *device,
				     struct kgsl_process_private *private)
{
	struct kgsl_mem_entry *entry, *entry_tmp;

	if (!private)
		return;

	BUG_ON(!mutex_is_locked(&device->mutex));

	list_for_each_entry_safe(entry, entry_tmp, &device->memqueue, list) {
		if (entry->priv == private) {
			list_del(&entry->list);
			kgsl_mem_entry_put(entry);
		}
	}
}

static void kgsl_memqueue_freememontimestamp(struct kgsl_device *device,
				  struct kgsl_mem_entry *entry,
				  uint32_t timestamp,
				  enum kgsl_timestamp_type type)
{
	BUG_ON(!mutex_is_locked(&device->mutex));

	entry->free_timestamp = timestamp;

	list_add_tail(&entry->list, &device->memqueue);
}

static void kgsl_memqueue_drain(struct kgsl_device *device)
{
	struct kgsl_mem_entry *entry, *entry_tmp;
	uint32_t ts_processed;

	BUG_ON(!mutex_is_locked(&device->mutex));

	/* get current EOP timestamp */
	ts_processed = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	list_for_each_entry_safe(entry, entry_tmp, &device->memqueue, list) {
		KGSL_MEM_INFO(device,
			"ts_processed %d ts_free %d gpuaddr %x)\n",
			ts_processed, entry->free_timestamp,
			entry->memdesc.gpuaddr);
		if (!timestamp_cmp(ts_processed, entry->free_timestamp))
			break;

		list_del(&entry->list);
		kgsl_mem_entry_put(entry);
	}
}

static void kgsl_memqueue_drain_unlocked(struct kgsl_device *device)
{
	mutex_lock(&device->mutex);
	kgsl_check_suspended(device);
	kgsl_memqueue_drain(device);
	mutex_unlock(&device->mutex);
}

static void kgsl_check_idle_locked(struct kgsl_device *device)
{
	if (device->pwrctrl.nap_allowed == true &&
	    device->state == KGSL_STATE_ACTIVE &&
		device->requested_state == KGSL_STATE_NONE) {
		device->requested_state = KGSL_STATE_NAP;
		if (kgsl_pwrctrl_sleep(device) != 0)
			mod_timer(&device->idle_timer,
				  jiffies +
				  device->pwrctrl.interval_timeout);
	}
}

static void kgsl_check_idle(struct kgsl_device *device)
{
	mutex_lock(&device->mutex);
	kgsl_check_idle_locked(device);
	mutex_unlock(&device->mutex);
}

struct kgsl_device *kgsl_get_device(int dev_idx)
{
	int i;
	struct kgsl_device *ret = NULL;

	mutex_lock(&kgsl_driver.devlock);

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->id == dev_idx) {
			ret = kgsl_driver.devp[i];
			break;
		}
	}

	mutex_unlock(&kgsl_driver.devlock);
	return ret;
}
EXPORT_SYMBOL(kgsl_get_device);

static struct kgsl_device *kgsl_get_minor(int minor)
{
	struct kgsl_device *ret = NULL;

	if (minor < 0 || minor >= KGSL_DEVICE_MAX)
		return NULL;

	mutex_lock(&kgsl_driver.devlock);
	ret = kgsl_driver.devp[minor];
	mutex_unlock(&kgsl_driver.devlock);

	return ret;
}

int kgsl_register_ts_notifier(struct kgsl_device *device,
			      struct notifier_block *nb)
{
	BUG_ON(device == NULL);
	return atomic_notifier_chain_register(&device->ts_notifier_list,
					      nb);
}
EXPORT_SYMBOL(kgsl_register_ts_notifier);

int kgsl_unregister_ts_notifier(struct kgsl_device *device,
				struct notifier_block *nb)
{
	BUG_ON(device == NULL);
	return atomic_notifier_chain_unregister(&device->ts_notifier_list,
						nb);
}
EXPORT_SYMBOL(kgsl_unregister_ts_notifier);

int kgsl_check_timestamp(struct kgsl_device *device, unsigned int timestamp)
{
	unsigned int ts_processed;

	ts_processed = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	return timestamp_cmp(ts_processed, timestamp);
}
EXPORT_SYMBOL(kgsl_check_timestamp);

static int kgsl_suspend_device(struct kgsl_device *device, pm_message_t state)
{
	int status = -EINVAL;
	unsigned int nap_allowed_saved;
	struct kgsl_pwrscale_policy *policy_saved;

	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "suspend start\n");

	mutex_lock(&device->mutex);
	nap_allowed_saved = device->pwrctrl.nap_allowed;
	device->pwrctrl.nap_allowed = false;
	policy_saved = device->pwrscale.policy;
	device->pwrscale.policy = NULL;
	device->requested_state = KGSL_STATE_SUSPEND;
	/* Make sure no user process is waiting for a timestamp *
	 * before supending */
	if (device->active_cnt != 0) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->suspend_gate);
		mutex_lock(&device->mutex);
	}
	/* Don't let the timer wake us during suspended sleep. */
	del_timer_sync(&device->idle_timer);
	switch (device->state) {
		case KGSL_STATE_INIT:
			break;
		case KGSL_STATE_ACTIVE:
			/* Wait for the device to become idle */
			device->ftbl->idle(device, KGSL_TIMEOUT_DEFAULT);
		case KGSL_STATE_NAP:
		case KGSL_STATE_SLEEP:
			/* Get the completion ready to be waited upon. */
			INIT_COMPLETION(device->hwaccess_gate);
			device->ftbl->suspend_context(device);
			device->ftbl->stop(device);
			device->state = KGSL_STATE_SUSPEND;
			KGSL_PWR_WARN(device, "state -> SUSPEND, device %d\n",
				device->id);
			break;
		default:
			KGSL_PWR_ERR(device, "suspend fail, device %d\n",
					device->id);
			goto end;
	}
	device->requested_state = KGSL_STATE_NONE;
	device->pwrctrl.nap_allowed = nap_allowed_saved;
	device->pwrscale.policy = policy_saved;
	status = 0;

end:
	mutex_unlock(&device->mutex);
	KGSL_PWR_WARN(device, "suspend end\n");
	return status;
}

static int kgsl_resume_device(struct kgsl_device *device)
{
	int status = -EINVAL;

	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "resume start\n");
	mutex_lock(&device->mutex);
	if (device->state == KGSL_STATE_SUSPEND) {
		device->requested_state = KGSL_STATE_ACTIVE;
		kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_NOMINAL);
		status = device->ftbl->start(device, 0);
		if (status == 0) {
			device->state = KGSL_STATE_ACTIVE;
			KGSL_PWR_WARN(device,
					"state -> ACTIVE, device %d\n",
					device->id);
		} else {
			KGSL_PWR_ERR(device,
					"resume failed, device %d\n",
					device->id);
			device->state = KGSL_STATE_INIT;
			goto end;
		}
		complete_all(&device->hwaccess_gate);
	}
	device->requested_state = KGSL_STATE_NONE;

end:
	mutex_unlock(&device->mutex);
	kgsl_check_idle(device);
	KGSL_PWR_WARN(device, "resume end\n");
	return status;
}

static int kgsl_suspend(struct device *dev)
{

	pm_message_t arg = {0};
	struct kgsl_device *device = dev_get_drvdata(dev);
	return kgsl_suspend_device(device, arg);
}

static int kgsl_resume(struct device *dev)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	return kgsl_resume_device(device);
}

static int kgsl_runtime_suspend(struct device *dev)
{
	return 0;
}

static int kgsl_runtime_resume(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops kgsl_pm_ops = {
	.suspend = kgsl_suspend,
	.resume = kgsl_resume,
	.runtime_suspend = kgsl_runtime_suspend,
	.runtime_resume = kgsl_runtime_resume,
};
EXPORT_SYMBOL(kgsl_pm_ops);

void kgsl_early_suspend_driver(struct early_suspend *h)
{
	struct kgsl_device *device = container_of(h,
					struct kgsl_device, display_off);
	mutex_lock(&device->mutex);
	kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_NOMINAL);
	mutex_unlock(&device->mutex);
}
EXPORT_SYMBOL(kgsl_early_suspend_driver);

int kgsl_suspend_driver(struct platform_device *pdev,
					pm_message_t state)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);
	return kgsl_suspend_device(device, state);
}
EXPORT_SYMBOL(kgsl_suspend_driver);

int kgsl_resume_driver(struct platform_device *pdev)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);
	return kgsl_resume_device(device);
}
EXPORT_SYMBOL(kgsl_resume_driver);

void kgsl_late_resume_driver(struct early_suspend *h)
{
	struct kgsl_device *device = container_of(h,
					struct kgsl_device, display_off);
	mutex_lock(&device->mutex);
	kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_TURBO);
	mutex_unlock(&device->mutex);
}
EXPORT_SYMBOL(kgsl_late_resume_driver);

/* file operations */
static struct kgsl_process_private *
kgsl_get_process_private(struct kgsl_device_private *cur_dev_priv)
{
	struct kgsl_process_private *private;

	mutex_lock(&kgsl_driver.process_mutex);
	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == task_tgid_nr(current)) {
			private->refcnt++;
			goto out;
		}
	}

	/* no existing process private found for this dev_priv, create one */
	private = kzalloc(sizeof(struct kgsl_process_private), GFP_KERNEL);
	if (private == NULL) {
		KGSL_DRV_ERR(cur_dev_priv->device, "kzalloc(%d) failed\n",
			sizeof(struct kgsl_process_private));
		goto out;
	}

	spin_lock_init(&private->mem_lock);
	private->refcnt = 1;
	private->pid = task_tgid_nr(current);

	INIT_LIST_HEAD(&private->mem_list);

#ifdef CONFIG_MSM_KGSL_MMU
	{
		unsigned long pt_name;

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
		pt_name = task_tgid_nr(current);
#else
		pt_name = KGSL_MMU_GLOBAL_PT;
#endif
		private->pagetable = kgsl_mmu_getpagetable(pt_name);
		if (private->pagetable == NULL) {
			kfree(private);
			private = NULL;
			goto out;
		}
	}
#endif

	list_add(&private->list, &kgsl_driver.process_list);

	kgsl_process_init_sysfs(private);

out:
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
}

static void
kgsl_put_process_private(struct kgsl_device *device,
			 struct kgsl_process_private *private)
{
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_mem_entry *entry_tmp = NULL;

	if (!private)
		return;

	mutex_lock(&kgsl_driver.process_mutex);

	if (--private->refcnt)
		goto unlock;

	KGSL_MEM_INFO(device,
			"Memory usage: user (%d/%d) mapped (%d/%d)\n",
			private->stats.user, private->stats.user_max,
			private->stats.mapped, private->stats.mapped_max);

	kgsl_process_uninit_sysfs(private);

	list_del(&private->list);

	list_for_each_entry_safe(entry, entry_tmp, &private->mem_list, list) {
		list_del(&entry->list);
		kgsl_mem_entry_put(entry);
	}

	kgsl_mmu_putpagetable(private->pagetable);
	kfree(private);
unlock:
	mutex_unlock(&kgsl_driver.process_mutex);
}

static int kgsl_release(struct inode *inodep, struct file *filep)
{
	int result = 0;
	struct kgsl_device_private *dev_priv = filep->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	int next = 0;

	filep->private_data = NULL;

	mutex_lock(&device->mutex);
	kgsl_check_suspended(device);

	while (1) {
		context = idr_get_next(&device->context_idr, &next);
		if (context == NULL)
			break;

		if (context->dev_priv == dev_priv) {
			device->ftbl->drawctxt_destroy(device, context);
			kgsl_destroy_context(dev_priv, context);
		}

		next = next + 1;
	}

	device->open_count--;
	if (device->open_count == 0) {
		result = device->ftbl->stop(device);
		device->state = KGSL_STATE_INIT;
		KGSL_PWR_WARN(device, "state -> INIT, device %d\n", device->id);
	}
	/* clean up any to-be-freed entries that belong to this
	 * process and this device
	 */
	kgsl_memqueue_cleanup(device, private);

	mutex_unlock(&device->mutex);
	kfree(dev_priv);

	kgsl_put_process_private(device, private);

	pm_runtime_put(device->parentdev);
	return result;
}

static int kgsl_open(struct inode *inodep, struct file *filep)
{
	int result;
	struct kgsl_device_private *dev_priv;
	struct kgsl_device *device;
	unsigned int minor = iminor(inodep);

	device = kgsl_get_minor(minor);
	BUG_ON(device == NULL);

	if (filep->f_flags & O_EXCL) {
		KGSL_DRV_ERR(device, "O_EXCL not allowed\n");
		return -EBUSY;
	}

	result = pm_runtime_get_sync(device->parentdev);
	if (result < 0) {
		KGSL_DRV_ERR(device,
			"Runtime PM: Unable to wake up the device, rc = %d\n",
			result);
		return result;
	}
	result = 0;

	dev_priv = kzalloc(sizeof(struct kgsl_device_private), GFP_KERNEL);
	if (dev_priv == NULL) {
		KGSL_DRV_ERR(device, "kzalloc failed(%d)\n",
			sizeof(struct kgsl_device_private));
		result = -ENOMEM;
		goto err_pmruntime;
	}

	dev_priv->device = device;
	filep->private_data = dev_priv;

	/* Get file (per process) private struct */
	dev_priv->process_priv = kgsl_get_process_private(dev_priv);
	if (dev_priv->process_priv ==  NULL) {
		result = -ENOMEM;
		goto err_freedevpriv;
	}

	mutex_lock(&device->mutex);
	kgsl_check_suspended(device);

	if (device->open_count == 0) {
		result = device->ftbl->start(device, true);

		if (result) {
			mutex_unlock(&device->mutex);
			goto err_putprocess;
		}
		device->state = KGSL_STATE_ACTIVE;
		KGSL_PWR_WARN(device,
				"state -> ACTIVE, device %d\n", minor);
	}
	device->open_count++;
	mutex_unlock(&device->mutex);

	KGSL_DRV_INFO(device, "Initialized %s: mmu=%s pagetable_count=%d\n",
		device->name, kgsl_mmu_enabled() ? "on" : "off",
		kgsl_pagetable_count);

	return result;

err_putprocess:
	kgsl_put_process_private(device, dev_priv->process_priv);
err_freedevpriv:
	filep->private_data = NULL;
	kfree(dev_priv);
err_pmruntime:
	pm_runtime_put(device->parentdev);
	return result;
}


/*call with private->mem_lock locked */
static struct kgsl_mem_entry *
kgsl_sharedmem_find(struct kgsl_process_private *private, unsigned int gpuaddr)
{
	struct kgsl_mem_entry *entry = NULL, *result = NULL;

	BUG_ON(private == NULL);

	gpuaddr &= PAGE_MASK;

	list_for_each_entry(entry, &private->mem_list, list) {
		if (entry->memdesc.gpuaddr == gpuaddr) {
			result = entry;
			break;
		}
	}
	return result;
}

/*call with private->mem_lock locked */
struct kgsl_mem_entry *
kgsl_sharedmem_find_region(struct kgsl_process_private *private,
				unsigned int gpuaddr,
				size_t size)
{
	struct kgsl_mem_entry *entry = NULL, *result = NULL;

	BUG_ON(private == NULL);

	list_for_each_entry(entry, &private->mem_list, list) {
		if (gpuaddr >= entry->memdesc.gpuaddr &&
		    ((gpuaddr + size) <=
			(entry->memdesc.gpuaddr + entry->memdesc.size))) {
			result = entry;
			break;
		}
	}

	return result;
}
EXPORT_SYMBOL(kgsl_sharedmem_find_region);

uint8_t *kgsl_gpuaddr_to_vaddr(const struct kgsl_memdesc *memdesc,
	unsigned int gpuaddr, unsigned int *size)
{
	BUG_ON(memdesc->hostptr == NULL);

	if (memdesc->gpuaddr == 0 || (gpuaddr < memdesc->gpuaddr ||
		gpuaddr >= memdesc->gpuaddr + memdesc->size))
		return NULL;

	*size = memdesc->size - (gpuaddr - memdesc->gpuaddr);
	return memdesc->hostptr + (gpuaddr - memdesc->gpuaddr);
}
EXPORT_SYMBOL(kgsl_gpuaddr_to_vaddr);

/*call all ioctl sub functions with driver locked*/
static long kgsl_ioctl_device_getproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_device_getproperty *param = data;

	switch (param->type) {
	case KGSL_PROP_VERSION:
	{
		struct kgsl_version version;
		if (param->sizebytes != sizeof(version)) {
			result = -EINVAL;
			break;
		}

		version.drv_major = KGSL_VERSION_MAJOR;
		version.drv_minor = KGSL_VERSION_MINOR;
		version.dev_major = dev_priv->device->ver_major;
		version.dev_minor = dev_priv->device->ver_minor;

		if (copy_to_user(param->value, &version, sizeof(version)))
			result = -EFAULT;

		break;
	}
	default:
		result = dev_priv->device->ftbl->getproperty(
					dev_priv->device, param->type,
					param->value, param->sizebytes);
	}


	return result;
}

static long kgsl_ioctl_device_waittimestamp(struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
{
	int result = 0;
	struct kgsl_device_waittimestamp *param = data;

	/* Set the active count so that suspend doesn't do the
	   wrong thing */

	dev_priv->device->active_cnt++;

	/* Don't wait forever, set a max value for now */
	if (param->timeout == -1)
		param->timeout = 10 * MSEC_PER_SEC;

	result = dev_priv->device->ftbl->waittimestamp(dev_priv->device,
					param->timestamp,
					param->timeout);

	kgsl_memqueue_drain(dev_priv->device);

	/* Fire off any pending suspend operations that are in flight */

	INIT_COMPLETION(dev_priv->device->suspend_gate);
	dev_priv->device->active_cnt--;
	complete(&dev_priv->device->suspend_gate);

	return result;
}
static bool check_ibdesc(struct kgsl_device_private *dev_priv,
			 struct kgsl_ibdesc *ibdesc, unsigned int numibs,
			 bool parse)
{
	bool result = true;
	unsigned int i;
	for (i = 0; i < numibs; i++) {
		struct kgsl_mem_entry *entry;
		spin_lock(&dev_priv->process_priv->mem_lock);
		entry = kgsl_sharedmem_find_region(dev_priv->process_priv,
			ibdesc[i].gpuaddr, ibdesc[i].sizedwords * sizeof(uint));
		spin_unlock(&dev_priv->process_priv->mem_lock);
		if (entry == NULL) {
			KGSL_DRV_ERR(dev_priv->device,
				"invalid cmd buffer gpuaddr %08x " \
				"sizedwords %d\n", ibdesc[i].gpuaddr,
				ibdesc[i].sizedwords);
			result = false;
			break;
		}

		if (parse && !kgsl_cffdump_parse_ibs(dev_priv, &entry->memdesc,
			ibdesc[i].gpuaddr, ibdesc[i].sizedwords, true)) {
			KGSL_DRV_ERR(dev_priv->device,
				"invalid cmd buffer gpuaddr %08x " \
				"sizedwords %d numibs %d/%d\n",
				ibdesc[i].gpuaddr,
				ibdesc[i].sizedwords, i+1, numibs);
			result = false;
			break;
		}
	}
	return result;
}

static long kgsl_ioctl_rb_issueibcmds(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_ringbuffer_issueibcmds *param = data;
	struct kgsl_ibdesc *ibdesc;
	struct kgsl_context *context;

#ifdef CONFIG_MSM_KGSL_DRM
	kgsl_gpu_mem_flush(DRM_KGSL_GEM_CACHE_OP_TO_DEV);
#endif

	context = kgsl_find_context(dev_priv, param->drawctxt_id);
	if (context == NULL) {
		result = -EINVAL;
		KGSL_DRV_ERR(dev_priv->device,
			"invalid drawctxt drawctxt_id %d\n",
			param->drawctxt_id);
		goto done;
	}

	if (param->flags & KGSL_CONTEXT_SUBMIT_IB_LIST) {
		KGSL_DRV_INFO(dev_priv->device,
			"Using IB list mode for ib submission, numibs: %d\n",
			param->numibs);
		if (!param->numibs) {
			KGSL_DRV_ERR(dev_priv->device,
				"Invalid numibs as parameter: %d\n",
				 param->numibs);
			result = -EINVAL;
			goto done;
		}

		ibdesc = kzalloc(sizeof(struct kgsl_ibdesc) * param->numibs,
					GFP_KERNEL);
		if (!ibdesc) {
			KGSL_MEM_ERR(dev_priv->device,
				"kzalloc(%d) failed\n",
				sizeof(struct kgsl_ibdesc) * param->numibs);
			result = -ENOMEM;
			goto done;
		}

		if (copy_from_user(ibdesc, (void *)param->ibdesc_addr,
				sizeof(struct kgsl_ibdesc) * param->numibs)) {
			result = -EFAULT;
			KGSL_DRV_ERR(dev_priv->device,
				"copy_from_user failed\n");
			goto free_ibdesc;
		}
	} else {
		KGSL_DRV_INFO(dev_priv->device,
			"Using single IB submission mode for ib submission\n");
		/* If user space driver is still using the old mode of
		 * submitting single ib then we need to support that as well */
		ibdesc = kzalloc(sizeof(struct kgsl_ibdesc), GFP_KERNEL);
		if (!ibdesc) {
			KGSL_MEM_ERR(dev_priv->device,
				"kzalloc(%d) failed\n",
				sizeof(struct kgsl_ibdesc));
			result = -ENOMEM;
			goto done;
		}
		ibdesc[0].gpuaddr = param->ibdesc_addr;
		ibdesc[0].sizedwords = param->numibs;
		param->numibs = 1;
	}

	if (!check_ibdesc(dev_priv, ibdesc, param->numibs, true)) {
		KGSL_DRV_ERR(dev_priv->device, "bad ibdesc");
		result = -EINVAL;
		goto free_ibdesc;
	}

	/* Let the pwrscale policy know that a new command buffer
	   is being issued */

	kgsl_pwrscale_busy(dev_priv->device);

	result = dev_priv->device->ftbl->issueibcmds(dev_priv,
					     context,
					     ibdesc,
					     param->numibs,
					     &param->timestamp,
					     param->flags);

	if (result != 0)
		goto free_ibdesc;

	/* this is a check to try to detect if a command buffer was freed
	 * during issueibcmds().
	 */
	if (!check_ibdesc(dev_priv, ibdesc, param->numibs, false)) {
		KGSL_DRV_ERR(dev_priv->device, "bad ibdesc AFTER issue");
		result = -EINVAL;
		goto free_ibdesc;
	}

free_ibdesc:
	kfree(ibdesc);
done:

#ifdef CONFIG_MSM_KGSL_DRM
	kgsl_gpu_mem_flush(DRM_KGSL_GEM_CACHE_OP_FROM_DEV);
#endif

	return result;
}

static long kgsl_ioctl_cmdstream_readtimestamp(struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
{
	struct kgsl_cmdstream_readtimestamp *param = data;

	param->timestamp =
		dev_priv->device->ftbl->readtimestamp(dev_priv->device,
		param->type);

	return 0;
}

static long kgsl_ioctl_cmdstream_freememontimestamp(struct kgsl_device_private
						    *dev_priv, unsigned int cmd,
						    void *data)
{
	int result = 0;
	struct kgsl_cmdstream_freememontimestamp *param = data;
	struct kgsl_mem_entry *entry = NULL;

	spin_lock(&dev_priv->process_priv->mem_lock);
	entry = kgsl_sharedmem_find(dev_priv->process_priv, param->gpuaddr);
	if (entry)
		list_del(&entry->list);
	spin_unlock(&dev_priv->process_priv->mem_lock);

	if (entry) {
		kgsl_memqueue_freememontimestamp(dev_priv->device, entry,
					param->timestamp, param->type);
		kgsl_memqueue_drain(dev_priv->device);
	} else {
		KGSL_DRV_ERR(dev_priv->device,
			"invalid gpuaddr %08x\n", param->gpuaddr);
		result = -EINVAL;
	}

	return result;
}

static long kgsl_ioctl_drawctxt_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_create *param = data;
	struct kgsl_context *context = NULL;

	context = kgsl_create_context(dev_priv);

	if (context == NULL) {
		result = -ENOMEM;
		goto done;
	}

	if (dev_priv->device->ftbl->drawctxt_create)
		result = dev_priv->device->ftbl->drawctxt_create(
			dev_priv->device, dev_priv->process_priv->pagetable,
			context, param->flags);

	param->drawctxt_id = context->id;

done:
	if (result && context)
		kgsl_destroy_context(dev_priv, context);

	return result;
}

static long kgsl_ioctl_drawctxt_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_destroy *param = data;
	struct kgsl_context *context;

	context = kgsl_find_context(dev_priv, param->drawctxt_id);

	if (context == NULL) {
		result = -EINVAL;
		goto done;
	}

	if (dev_priv->device->ftbl->drawctxt_destroy)
		dev_priv->device->ftbl->drawctxt_destroy(dev_priv->device,
			context);

	kgsl_destroy_context(dev_priv, context);

done:
	return result;
}

static long kgsl_ioctl_sharedmem_free(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;

	spin_lock(&private->mem_lock);
	entry = kgsl_sharedmem_find(private, param->gpuaddr);
	if (entry)
		list_del(&entry->list);
	spin_unlock(&private->mem_lock);

	if (entry) {
		kgsl_mem_entry_put(entry);
	} else {
		KGSL_CORE_ERR("invalid gpuaddr %08x\n", param->gpuaddr);
		result = -EINVAL;
	}

	return result;
}

static struct vm_area_struct *kgsl_get_vma_from_start_addr(unsigned int addr)
{
	struct vm_area_struct *vma;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, addr);
	up_read(&current->mm->mmap_sem);
	if (!vma)
		KGSL_CORE_ERR("find_vma(%x) failed\n", addr);

	return vma;
}

static long
kgsl_ioctl_sharedmem_from_vmalloc(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	int result = 0, len = 0;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_sharedmem_from_vmalloc *param = data;
	struct kgsl_mem_entry *entry = NULL;
	struct vm_area_struct *vma;

	if (!kgsl_mmu_enabled())
		return -ENODEV;

	/* Make sure all pending freed memory is collected */
	kgsl_memqueue_drain_unlocked(dev_priv->device);

	if (!param->hostptr) {
		KGSL_CORE_ERR("invalid hostptr %x\n", param->hostptr);
		result = -EINVAL;
		goto error;
	}

	vma = kgsl_get_vma_from_start_addr(param->hostptr);
	if (!vma) {
		result = -EINVAL;
		goto error;
	}

	/*
	 * If the user specified a length, use it, otherwise try to
	 * infer the length if the vma region
	 */
	if (param->gpuaddr != 0) {
		len = param->gpuaddr;
	} else {
		/*
		 * For this to work, we have to assume the VMA region is only
		 * for this single allocation.  If it isn't, then bail out
		 */
		if (vma->vm_pgoff || (param->hostptr != vma->vm_start)) {
			KGSL_CORE_ERR("VMA region does not match hostaddr\n");
			result = -EINVAL;
			goto error;
		}

		len = vma->vm_end - vma->vm_start;
	}

	/* Make sure it fits */
	if (len == 0 || param->hostptr + len > vma->vm_end) {
		KGSL_CORE_ERR("Invalid memory allocation length %d\n", len);
		result = -EINVAL;
		goto error;
	}

	entry = kgsl_mem_entry_create();
	if (entry == NULL) {
		result = -ENOMEM;
		goto error;
	}

	result = kgsl_sharedmem_vmalloc_user(&entry->memdesc,
					     private->pagetable, len,
					     param->flags);
	if (result != 0)
		goto error_free_entry;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	result = remap_vmalloc_range(vma, (void *) entry->memdesc.hostptr, 0);
	if (result) {
		KGSL_CORE_ERR("remap_vmalloc_range failed: %d\n", result);
		goto error_free_vmalloc;
	}

	param->gpuaddr = entry->memdesc.gpuaddr;

	entry->memtype = KGSL_USER_MEMORY;

	kgsl_mem_entry_attach_process(entry, private);

	/* Process specific statistics */
	KGSL_STATS_ADD(len, private->stats.user,
		       private->stats.user_max);

	kgsl_check_idle(dev_priv->device);
	return 0;

error_free_vmalloc:
	kgsl_sharedmem_free(&entry->memdesc);

error_free_entry:
	kfree(entry);

error:
	kgsl_check_idle(dev_priv->device);
	return result;
}

static inline int _check_region(unsigned long start, unsigned long size,
				uint64_t len)
{
	uint64_t end = ((uint64_t) start) + size;
	return (end > len);
}

#ifdef CONFIG_ANDROID_PMEM
static int kgsl_get_phys_file(int fd, unsigned long *start, unsigned long *len,
			      unsigned long *vstart, struct file **filep)
{
	struct file *fbfile;
	int ret = 0;
	dev_t rdev;
	struct fb_info *info;

	*filep = NULL;
	if (!get_pmem_file(fd, start, vstart, len, filep))
		return 0;

	fbfile = fget(fd);
	if (fbfile == NULL) {
		KGSL_CORE_ERR("fget_light failed\n");
		return -1;
	}

	rdev = fbfile->f_dentry->d_inode->i_rdev;
	info = MAJOR(rdev) == FB_MAJOR ? registered_fb[MINOR(rdev)] : NULL;
	if (info) {
		*start = info->fix.smem_start;
		*len = info->fix.smem_len;
		*vstart = (unsigned long)__va(info->fix.smem_start);
		ret = 0;
	} else {
		KGSL_CORE_ERR("framebuffer minor %d not found\n",
			      MINOR(rdev));
		ret = -1;
	}

	fput(fbfile);

	return ret;
}

static int kgsl_setup_phys_file(struct kgsl_mem_entry *entry,
				struct kgsl_pagetable *pagetable,
				unsigned int fd, unsigned int offset,
				size_t size)
{
	int ret;
	unsigned long phys, virt, len;
	struct file *filep;

	ret = kgsl_get_phys_file(fd, &phys, &len, &virt, &filep);
	if (ret)
		return ret;

	if (phys == 0) {
		ret = -EINVAL;
		goto err;
	}

	if (offset >= len) {
		ret = -EINVAL;
		goto err;
	}

	if (size == 0)
		size = len;

	/* Adjust the size of the region to account for the offset */
	size += offset & ~PAGE_MASK;

	size = ALIGN(size, PAGE_SIZE);

	if (_check_region(offset & PAGE_MASK, size, len)) {
		KGSL_CORE_ERR("Offset (%ld) + size (%d) is larger"
			      "than pmem region length %ld\n",
			      offset & PAGE_MASK, size, len);
		ret = -EINVAL;
		goto err;

	}

	entry->file_ptr = filep;

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = size;
	entry->memdesc.physaddr = phys + (offset & PAGE_MASK);
	entry->memdesc.hostptr = (void *) (virt + (offset & PAGE_MASK));
	entry->memdesc.ops = &kgsl_contiguous_ops;

	return 0;
err:
	put_pmem_file(filep);
	return ret;
}
#else
static int kgsl_setup_phys_file(struct kgsl_mem_entry *entry,
				struct kgsl_pagetable *pagetable,
				unsigned int fd, unsigned int offset,
				size_t size)
{
	return -EINVAL;
}
#endif

static int kgsl_setup_hostptr(struct kgsl_mem_entry *entry,
			      struct kgsl_pagetable *pagetable,
			      void *hostptr, unsigned int offset,
			      size_t size)
{
	struct vm_area_struct *vma;
	unsigned int len;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, (unsigned int) hostptr);
	up_read(&current->mm->mmap_sem);

	if (!vma) {
		KGSL_CORE_ERR("find_vma(%p) failed\n", hostptr);
		return -EINVAL;
	}

	/* We don't necessarily start at vma->vm_start */
	len = vma->vm_end - (unsigned long) hostptr;

	if (offset >= len)
		return -EINVAL;

	if (!KGSL_IS_PAGE_ALIGNED((unsigned long) hostptr) ||
	    !KGSL_IS_PAGE_ALIGNED(len)) {
		KGSL_CORE_ERR("user address len(%u)"
			      "and start(%p) must be page"
			      "aligned\n", len, hostptr);
		return -EINVAL;
	}

	if (size == 0)
		size = len;

	/* Adjust the size of the region to account for the offset */
	size += offset & ~PAGE_MASK;

	size = ALIGN(size, PAGE_SIZE);

	if (_check_region(offset & PAGE_MASK, size, len)) {
		KGSL_CORE_ERR("Offset (%ld) + size (%d) is larger"
			      "than region length %d\n",
			      offset & PAGE_MASK, size, len);
		return -EINVAL;
	}

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = size;
	entry->memdesc.hostptr = hostptr + (offset & PAGE_MASK);
	entry->memdesc.ops = &kgsl_userptr_ops;

	return 0;
}

#ifdef CONFIG_ASHMEM
static int kgsl_setup_ashmem(struct kgsl_mem_entry *entry,
			     struct kgsl_pagetable *pagetable,
			     int fd, void *hostptr, size_t size)
{
	int ret;
	struct vm_area_struct *vma;
	struct file *filep, *vmfile;
	unsigned long len;
	unsigned int hostaddr = (unsigned int) hostptr;

	vma = kgsl_get_vma_from_start_addr(hostaddr);
	if (vma == NULL)
		return -EINVAL;

	if (vma->vm_pgoff || vma->vm_start != hostaddr) {
		KGSL_CORE_ERR("Invalid vma region\n");
		return -EINVAL;
	}

	len = vma->vm_end - vma->vm_start;

	if (size == 0)
		size = len;

	if (size != len) {
		KGSL_CORE_ERR("Invalid size %d for vma region %p\n",
			      size, hostptr);
		return -EINVAL;
	}

	ret = get_ashmem_file(fd, &filep, &vmfile, &len);

	if (ret) {
		KGSL_CORE_ERR("get_ashmem_file failed\n");
		return ret;
	}

	if (vmfile != vma->vm_file) {
		KGSL_CORE_ERR("ashmem shmem file does not match vma\n");
		ret = -EINVAL;
		goto err;
	}

	entry->file_ptr = filep;

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = ALIGN(size, PAGE_SIZE);
	entry->memdesc.hostptr = hostptr;
	entry->memdesc.ops = &kgsl_userptr_ops;

	return 0;

err:
	put_ashmem_file(filep);
	return ret;
}
#else
static int kgsl_setup_ashmem(struct kgsl_mem_entry *entry,
			     struct kgsl_pagetable *pagetable,
			     int fd, void *hostptr, size_t size)
{
	return -EINVAL;
}
#endif

static long kgsl_ioctl_map_user_mem(struct kgsl_device_private *dev_priv,
				     unsigned int cmd, void *data)
{
	int result = -EINVAL;
	struct kgsl_map_user_mem *param = data;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_process_private *private = dev_priv->process_priv;
	enum kgsl_user_mem_type memtype;

	entry = kgsl_mem_entry_create();

	if (entry == NULL)
		return -ENOMEM;

	kgsl_memqueue_drain_unlocked(dev_priv->device);

	if (_IOC_SIZE(cmd) == sizeof(struct kgsl_sharedmem_from_pmem))
		memtype = KGSL_USER_MEM_TYPE_PMEM;
	else
		memtype = param->memtype;

	switch (memtype) {
	case KGSL_USER_MEM_TYPE_PMEM:
		if (param->fd == 0 || param->len == 0)
			break;

		result = kgsl_setup_phys_file(entry, private->pagetable,
					      param->fd, param->offset,
					      param->len);
		break;

	case KGSL_USER_MEM_TYPE_ADDR:
		if (!kgsl_mmu_enabled()) {
			KGSL_DRV_ERR(dev_priv->device,
				"Cannot map paged memory with the "
				"MMU disabled\n");
			break;
		}

		if (param->hostptr == 0)
			break;

		result = kgsl_setup_hostptr(entry, private->pagetable,
					    (void *) param->hostptr,
					    param->offset, param->len);
		break;

	case KGSL_USER_MEM_TYPE_ASHMEM:
		if (!kgsl_mmu_enabled()) {
			KGSL_DRV_ERR(dev_priv->device,
				"Cannot map paged memory with the "
				"MMU disabled\n");
			break;
		}

		if (param->hostptr == 0)
			break;

		result = kgsl_setup_ashmem(entry, private->pagetable,
					   param->fd, (void *) param->hostptr,
					   param->len);
		break;
	default:
		KGSL_CORE_ERR("Invalid memory type: %x\n", memtype);
		break;
	}

	if (result)
		goto error;

	result = kgsl_mmu_map(private->pagetable,
			      &entry->memdesc,
			      GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);

	if (result)
		goto error_put_file_ptr;

	/* Adjust the returned value for a non 4k aligned offset */
	param->gpuaddr = entry->memdesc.gpuaddr + (param->offset & ~PAGE_MASK);

	entry->memtype = KGSL_MAPPED_MEMORY;

	KGSL_STATS_ADD(param->len, kgsl_driver.stats.mapped,
		       kgsl_driver.stats.mapped_max);

	/* Statistics */
	KGSL_STATS_ADD(param->len, private->stats.mapped,
		       private->stats.mapped_max);

	kgsl_mem_entry_attach_process(entry, private);

	kgsl_check_idle(dev_priv->device);
	return result;

 error_put_file_ptr:
	if (entry->file_ptr)
		fput(entry->file_ptr);

error:
	kfree(entry);
	kgsl_check_idle(dev_priv->device);
	return result;
}

/*This function flushes a graphics memory allocation from CPU cache
 *when caching is enabled with MMU*/
static long
kgsl_ioctl_sharedmem_flush_cache(struct kgsl_device_private *dev_priv,
				 unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_mem_entry *entry;
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;

	spin_lock(&private->mem_lock);
	entry = kgsl_sharedmem_find(private, param->gpuaddr);
	if (!entry) {
		KGSL_CORE_ERR("invalid gpuaddr %08x\n", param->gpuaddr);
		result = -EINVAL;
		goto done;
	}
	if (!entry->memdesc.hostptr)
		entry->memdesc.hostptr =
				kgsl_gpuaddr_to_vaddr(&entry->memdesc,
					param->gpuaddr, &entry->memdesc.size);

	if (!entry->memdesc.hostptr) {
		KGSL_CORE_ERR("invalid hostptr with gpuaddr %08x\n",
			param->gpuaddr);
			goto done;
	}

	kgsl_cache_range_op(&entry->memdesc, KGSL_CACHE_OP_CLEAN);

	/* Statistics - keep track of how many flushes each process does */
	private->stats.flushes++;
done:
	spin_unlock(&private->mem_lock);
	return result;
}

static long
kgsl_ioctl_gpumem_alloc(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpumem_alloc *param = data;
	struct kgsl_mem_entry *entry;
	int result;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	/* Make sure all pending freed memory is collected */
	kgsl_memqueue_drain_unlocked(dev_priv->device);

	result = kgsl_allocate_user(&entry->memdesc, private->pagetable,
		param->size, param->flags);

	if (result == 0) {
		entry->memtype = KGSL_USER_MEMORY;
		kgsl_mem_entry_attach_process(entry, private);
		param->gpuaddr = entry->memdesc.gpuaddr;

		KGSL_STATS_ADD(entry->memdesc.size, private->stats.user,
		       private->stats.user_max);
	} else
		kfree(entry);

	kgsl_check_idle(dev_priv->device);
	return result;
}
static long kgsl_ioctl_cff_syncmem(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_cff_syncmem *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;

	spin_lock(&private->mem_lock);
	entry = kgsl_sharedmem_find_region(private, param->gpuaddr, param->len);
	if (entry)
		kgsl_cffdump_syncmem(dev_priv, &entry->memdesc, param->gpuaddr,
				     param->len, true);
	else
		result = -EINVAL;
	spin_unlock(&private->mem_lock);
	return result;
}

static long kgsl_ioctl_cff_user_event(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_cff_user_event *param = data;

	kgsl_cffdump_user_event(param->cff_opcode, param->op1, param->op2,
			param->op3, param->op4, param->op5);

	return result;
}

typedef long (*kgsl_ioctl_func_t)(struct kgsl_device_private *,
	unsigned int, void *);

#define KGSL_IOCTL_FUNC(_cmd, _func, _lock) \
	[_IOC_NR(_cmd)] = { .cmd = _cmd, .func = _func, .lock = _lock }

static const struct {
	unsigned int cmd;
	kgsl_ioctl_func_t func;
	int lock;
} kgsl_ioctl_funcs[] = {
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_GETPROPERTY,
			kgsl_ioctl_device_getproperty, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_WAITTIMESTAMP,
			kgsl_ioctl_device_waittimestamp, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS,
			kgsl_ioctl_rb_issueibcmds, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_READTIMESTAMP,
			kgsl_ioctl_cmdstream_readtimestamp, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP,
			kgsl_ioctl_cmdstream_freememontimestamp, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_CREATE,
			kgsl_ioctl_drawctxt_create, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_DESTROY,
			kgsl_ioctl_drawctxt_destroy, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_MAP_USER_MEM,
			kgsl_ioctl_map_user_mem, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FROM_PMEM,
			kgsl_ioctl_map_user_mem, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FREE,
			kgsl_ioctl_sharedmem_free, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC,
			kgsl_ioctl_sharedmem_from_vmalloc, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE,
			kgsl_ioctl_sharedmem_flush_cache, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_ALLOC,
			kgsl_ioctl_gpumem_alloc, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CFF_SYNCMEM,
			kgsl_ioctl_cff_syncmem, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CFF_USER_EVENT,
			kgsl_ioctl_cff_user_event, 0),
};

static long kgsl_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	unsigned int nr = _IOC_NR(cmd);
	kgsl_ioctl_func_t func;
	int lock, ret;
	char ustack[64];
	void *uptr = NULL;

	BUG_ON(dev_priv == NULL);

	/* Workaround for an previously incorrectly defined ioctl code.
	   This helps ensure binary compatability */

	if (cmd == IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_OLD)
		cmd = IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP;
	else if (cmd == IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_OLD)
		cmd = IOCTL_KGSL_CMDSTREAM_READTIMESTAMP;

	if (cmd & (IOC_IN | IOC_OUT)) {
		if (_IOC_SIZE(cmd) < sizeof(ustack))
			uptr = ustack;
		else {
			uptr = kzalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (uptr == NULL) {
				KGSL_MEM_ERR(dev_priv->device,
					"kzalloc(%d) failed\n", _IOC_SIZE(cmd));
				ret = -ENOMEM;
				goto done;
			}
		}

		if (cmd & IOC_IN) {
			if (copy_from_user(uptr, (void __user *) arg,
				_IOC_SIZE(cmd))) {
				ret = -EFAULT;
				goto done;
			}
		} else
			memset(uptr, 0, _IOC_SIZE(cmd));
	}

	if (nr < ARRAY_SIZE(kgsl_ioctl_funcs) &&
	    kgsl_ioctl_funcs[nr].func != NULL) {
		func = kgsl_ioctl_funcs[nr].func;
		lock = kgsl_ioctl_funcs[nr].lock;
	} else {
		func = dev_priv->device->ftbl->ioctl;
		if (!func) {
			KGSL_DRV_INFO(dev_priv->device,
				      "invalid ioctl code %08x\n", cmd);
			ret = -EINVAL;
			goto done;
		}
		lock = 1;
	}

	if (lock) {
		mutex_lock(&dev_priv->device->mutex);
		kgsl_check_suspended(dev_priv->device);
	}

	ret = func(dev_priv, cmd, uptr);

	if (lock) {
		kgsl_check_idle_locked(dev_priv->device);
		mutex_unlock(&dev_priv->device->mutex);
	}

	if (ret == 0 && (cmd & IOC_OUT)) {
		if (copy_to_user((void __user *) arg, uptr, _IOC_SIZE(cmd)))
			ret = -EFAULT;
	}

done:
	if (_IOC_SIZE(cmd) >= sizeof(ustack))
		kfree(uptr);

	return ret;
}

static int
kgsl_mmap_memstore(struct kgsl_device *device, struct vm_area_struct *vma)
{
	struct kgsl_memdesc *memdesc = &device->memstore;
	int result;
	unsigned int vma_size = vma->vm_end - vma->vm_start;

	/* The memstore can only be mapped as read only */

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	if (memdesc->size  !=  vma_size) {
		KGSL_MEM_ERR(device, "memstore bad size: %d should be %d\n",
			     vma_size, memdesc->size);
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	result = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vma_size, vma->vm_page_prot);
	if (result != 0)
		KGSL_MEM_ERR(device, "remap_pfn_range failed: %d\n",
			     result);

	return result;
}

static int
kgsl_gpumem_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;

	if (!entry->memdesc.ops->vmfault)
		return VM_FAULT_SIGBUS;

	return entry->memdesc.ops->vmfault(&entry->memdesc, vma, vmf);
}

static void
kgsl_gpumem_vm_close(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry  = vma->vm_private_data;
	kgsl_mem_entry_put(entry);
}

static struct vm_operations_struct kgsl_gpumem_vm_ops = {
	.fault = kgsl_gpumem_vm_fault,
	.close = kgsl_gpumem_vm_close,
};

static int kgsl_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vma_offset = vma->vm_pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	struct kgsl_device *device = dev_priv->device;

	/* Handle leagacy behavior for memstore */

	if (vma_offset == device->memstore.physaddr)
		return kgsl_mmap_memstore(device, vma);

	/* Find a chunk of GPU memory */

	spin_lock(&private->mem_lock);
	list_for_each_entry(entry, &private->mem_list, list) {
		if (vma_offset == entry->memdesc.gpuaddr) {
			kgsl_mem_entry_get(entry);
			break;
		}
	}
	spin_unlock(&private->mem_lock);

	if (entry == NULL)
		return -EINVAL;

	if (!entry->memdesc.ops->vmflags || !entry->memdesc.ops->vmfault)
		return -EINVAL;

	vma->vm_flags |= entry->memdesc.ops->vmflags(&entry->memdesc);

	vma->vm_private_data = entry;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &kgsl_gpumem_vm_ops;
	vma->vm_file = file;

	return 0;
}

static const struct file_operations kgsl_fops = {
	.owner = THIS_MODULE,
	.release = kgsl_release,
	.open = kgsl_open,
	.mmap = kgsl_mmap,
	.unlocked_ioctl = kgsl_ioctl,
};

struct kgsl_driver kgsl_driver  = {
	.process_mutex = __MUTEX_INITIALIZER(kgsl_driver.process_mutex),
	.ptlock = __SPIN_LOCK_UNLOCKED(kgsl_driver.ptlock),
	.devlock = __MUTEX_INITIALIZER(kgsl_driver.devlock),
};
EXPORT_SYMBOL(kgsl_driver);

void kgsl_unregister_device(struct kgsl_device *device)
{
	int minor;

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < KGSL_DEVICE_MAX; minor++) {
		if (device == kgsl_driver.devp[minor])
			break;
	}

	mutex_unlock(&kgsl_driver.devlock);

	if (minor == KGSL_DEVICE_MAX)
		return;

	kgsl_cffdump_close(device->id);
	kgsl_pwrctrl_uninit_sysfs(device);

	wake_lock_destroy(&device->idle_wakelock);
	pm_qos_remove_request(&device->pm_qos_req_dma);

	idr_destroy(&device->context_idr);

	if (device->memstore.hostptr)
		kgsl_sharedmem_free(&device->memstore);

	kgsl_mmu_close(device);

	if (device->work_queue) {
		destroy_workqueue(device->work_queue);
		device->work_queue = NULL;
	}

	device_destroy(kgsl_driver.class,
		       MKDEV(MAJOR(kgsl_driver.major), minor));

	mutex_lock(&kgsl_driver.devlock);
	kgsl_driver.devp[minor] = NULL;
	mutex_unlock(&kgsl_driver.devlock);
}
EXPORT_SYMBOL(kgsl_unregister_device);

int
kgsl_register_device(struct kgsl_device *device)
{
	int minor, ret;
	dev_t dev;

	/* Find a minor for the device */

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < KGSL_DEVICE_MAX; minor++) {
		if (kgsl_driver.devp[minor] == NULL) {
			kgsl_driver.devp[minor] = device;
			break;
		}
	}

	mutex_unlock(&kgsl_driver.devlock);

	if (minor == KGSL_DEVICE_MAX) {
		KGSL_CORE_ERR("minor devices exhausted\n");
		return -ENODEV;
	}

	/* Create the device */
	dev = MKDEV(MAJOR(kgsl_driver.major), minor);
	device->dev = device_create(kgsl_driver.class,
				    device->parentdev,
				    dev, device,
				    device->name);

	if (IS_ERR(device->dev)) {
		ret = PTR_ERR(device->dev);
		KGSL_CORE_ERR("device_create(%s): %d\n", device->name, ret);
		goto err_devlist;
	}

	dev_set_drvdata(device->parentdev, device);

	/* Generic device initialization */
	init_waitqueue_head(&device->wait_queue);

	kgsl_cffdump_open(device->id);

	init_completion(&device->hwaccess_gate);
	init_completion(&device->suspend_gate);

	ATOMIC_INIT_NOTIFIER_HEAD(&device->ts_notifier_list);

	setup_timer(&device->idle_timer, kgsl_timer, (unsigned long) device);
	ret = kgsl_create_device_workqueue(device);
	if (ret)
		goto err_devlist;

	INIT_WORK(&device->idle_check_ws, kgsl_idle_check);

	INIT_LIST_HEAD(&device->memqueue);

	ret = kgsl_mmu_init(device);
	if (ret != 0)
		goto err_dest_work_q;

	ret = kgsl_allocate_contiguous(&device->memstore,
		sizeof(struct kgsl_devmemstore));

	if (ret != 0)
		goto err_close_mmu;

	wake_lock_init(&device->idle_wakelock, WAKE_LOCK_IDLE, device->name);
	pm_qos_add_request(&device->pm_qos_req_dma, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	idr_init(&device->context_idr);

	/* sysfs and debugfs initalization - failure here is non fatal */

	/* Initialize logging */
	kgsl_device_debugfs_init(device);

	/* Initialize common sysfs entries */
	kgsl_pwrctrl_init_sysfs(device);

	return 0;

err_close_mmu:
	kgsl_mmu_close(device);
err_dest_work_q:
	destroy_workqueue(device->work_queue);
	device->work_queue = NULL;
err_devlist:
	mutex_lock(&kgsl_driver.devlock);
	kgsl_driver.devp[minor] = NULL;
	mutex_unlock(&kgsl_driver.devlock);

	return ret;
}
EXPORT_SYMBOL(kgsl_register_device);

int kgsl_device_platform_probe(struct kgsl_device *device,
			       irqreturn_t (*dev_isr) (int, void*))
{
	int status = -EINVAL;
	struct kgsl_memregion *regspace = NULL;
	struct resource *res;
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);

	pm_runtime_enable(device->parentdev);

	status = kgsl_pwrctrl_init(device);
	if (status)
		goto error;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   device->iomemname);
	if (res == NULL) {
		KGSL_DRV_ERR(device, "platform_get_resource_byname failed\n");
		status = -EINVAL;
		goto error_pwrctrl_close;
	}
	if (res->start == 0 || resource_size(res) == 0) {
		KGSL_DRV_ERR(device, "dev %d invalid regspace\n", device->id);
		status = -EINVAL;
		goto error_pwrctrl_close;
	}

	regspace = &device->regspace;
	regspace->mmio_phys_base = res->start;
	regspace->sizebytes = resource_size(res);

	if (!request_mem_region(regspace->mmio_phys_base,
				regspace->sizebytes, device->name)) {
		KGSL_DRV_ERR(device, "request_mem_region failed\n");
		status = -ENODEV;
		goto error_pwrctrl_close;
	}

	regspace->mmio_virt_base = ioremap(regspace->mmio_phys_base,
					   regspace->sizebytes);

	if (regspace->mmio_virt_base == NULL) {
		KGSL_DRV_ERR(device, "ioremap failed\n");
		status = -ENODEV;
		goto error_release_mem;
	}

	status = request_irq(device->pwrctrl.interrupt_num, dev_isr,
			     IRQF_TRIGGER_HIGH, device->name, device);
	if (status) {
		KGSL_DRV_ERR(device, "request_irq(%d) failed: %d\n",
			      device->pwrctrl.interrupt_num, status);
		goto error_iounmap;
	}
	device->pwrctrl.have_irq = 1;
	disable_irq(device->pwrctrl.interrupt_num);

	KGSL_DRV_INFO(device,
		"dev_id %d regs phys 0x%08x size 0x%08x virt %p\n",
		device->id, regspace->mmio_phys_base,
		regspace->sizebytes, regspace->mmio_virt_base);


	status = kgsl_register_device(device);
	if (!status)
		return status;

	free_irq(device->pwrctrl.interrupt_num, NULL);
	device->pwrctrl.have_irq = 0;
error_iounmap:
	iounmap(regspace->mmio_virt_base);
	regspace->mmio_virt_base = NULL;
error_release_mem:
	release_mem_region(regspace->mmio_phys_base, regspace->sizebytes);
error_pwrctrl_close:
	kgsl_pwrctrl_close(device);
error:
	return status;
}
EXPORT_SYMBOL(kgsl_device_platform_probe);

void kgsl_device_platform_remove(struct kgsl_device *device)
{
	struct kgsl_memregion *regspace = &device->regspace;

	kgsl_unregister_device(device);

	if (regspace->mmio_virt_base != NULL) {
		iounmap(regspace->mmio_virt_base);
		regspace->mmio_virt_base = NULL;
		release_mem_region(regspace->mmio_phys_base,
					regspace->sizebytes);
	}
	kgsl_pwrctrl_close(device);

	pm_runtime_disable(device->parentdev);
}
EXPORT_SYMBOL(kgsl_device_platform_remove);

static int __devinit
kgsl_ptdata_init(void)
{
	INIT_LIST_HEAD(&kgsl_driver.pagetable_list);

	return kgsl_ptpool_init(&kgsl_driver.ptpool, KGSL_PAGETABLE_SIZE,
		kgsl_pagetable_count);
}

static void kgsl_core_exit(void)
{
	unregister_chrdev_region(kgsl_driver.major, KGSL_DEVICE_MAX);

	kgsl_ptpool_destroy(&kgsl_driver.ptpool);

	device_unregister(&kgsl_driver.virtdev);

	if (kgsl_driver.class) {
		class_destroy(kgsl_driver.class);
		kgsl_driver.class = NULL;
	}

	kgsl_drm_exit();
	kgsl_cffdump_destroy();
}

static int __init kgsl_core_init(void)
{
	int result = 0;

	/* alloc major and minor device numbers */
	result = alloc_chrdev_region(&kgsl_driver.major, 0, KGSL_DEVICE_MAX,
				  KGSL_NAME);
	if (result < 0) {
		KGSL_CORE_ERR("alloc_chrdev_region failed err = %d\n", result);
		goto err;
	}

	cdev_init(&kgsl_driver.cdev, &kgsl_fops);
	kgsl_driver.cdev.owner = THIS_MODULE;
	kgsl_driver.cdev.ops = &kgsl_fops;
	result = cdev_add(&kgsl_driver.cdev, MKDEV(MAJOR(kgsl_driver.major), 0),
		       KGSL_DEVICE_MAX);

	if (result) {
		KGSL_CORE_ERR("kgsl: cdev_add() failed, dev_num= %d,"
			     " result= %d\n", kgsl_driver.major, result);
		goto err;
	}

	kgsl_driver.class = class_create(THIS_MODULE, KGSL_NAME);

	if (IS_ERR(kgsl_driver.class)) {
		result = PTR_ERR(kgsl_driver.class);
		KGSL_CORE_ERR("failed to create class %s", KGSL_NAME);
		goto err;
	}

	/* Make a virtual device for managing core related things
	   in sysfs */
	kgsl_driver.virtdev.class = kgsl_driver.class;
	dev_set_name(&kgsl_driver.virtdev, "kgsl");
	result = device_register(&kgsl_driver.virtdev);
	if (result) {
		KGSL_CORE_ERR("driver_register failed\n");
		goto err;
	}

	/* Make kobjects in the virtual device for storing statistics */

	kgsl_driver.ptkobj =
	  kobject_create_and_add("pagetables",
				 &kgsl_driver.virtdev.kobj);

	kgsl_driver.prockobj =
		kobject_create_and_add("proc",
				       &kgsl_driver.virtdev.kobj);

	kgsl_core_debugfs_init();

	kgsl_sharedmem_init_sysfs();
	kgsl_cffdump_init();

	INIT_LIST_HEAD(&kgsl_driver.process_list);

	result = kgsl_ptdata_init();
	if (result)
		goto err;

	result = kgsl_drm_init(NULL);

	if (result)
		goto err;

	return 0;

err:
	kgsl_core_exit();
	return result;
}

module_init(kgsl_core_init);
module_exit(kgsl_core_exit);

MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("MSM GPU driver");
MODULE_LICENSE("GPL");
