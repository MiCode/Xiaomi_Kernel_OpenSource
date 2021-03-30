// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#include <linux/notifier.h>
#include <linux/haven/hh_rm_drv.h>
#include <linux/haven/hh_irq_lend.h>
#include <linux/haven/hh_mem_notifier.h>
#include "sde_kms.h"
#include "sde_vm.h"
#include "sde_vm_common.h"
#include "sde_vm_msgq.h"

#define to_vm_primary(vm) ((struct sde_vm_primary *)vm)

static bool _sde_vm_owns_hw(struct sde_kms *sde_kms)
{
	struct sde_vm_primary *sde_vm;
	bool owns_irq, owns_mem_io;

	sde_vm = to_vm_primary(sde_kms->vm);

	owns_irq = !atomic_read(&sde_vm->base.n_irq_lent);
	owns_mem_io = (sde_vm->base.io_mem_handle < 0);

	return (owns_irq & owns_mem_io);
}

void sde_vm_irq_release_notification_handler(void *req,
		unsigned long notif_type, enum hh_irq_label label)
{
	SDE_INFO("irq release notification for label: %d\n", label);
}

static void sde_vm_mem_release_notification_handler(
		enum hh_mem_notifier_tag tag, unsigned long notif_type,
		void *entry_data, void *notif_msg)
{
	SDE_INFO("mem release notification for tag: %d\n", tag);
}

int _sde_vm_reclaim_mem(struct sde_kms *sde_kms)
{
	struct sde_vm_primary *sde_vm = to_vm_primary(sde_kms->vm);
	int rc = 0;

	if (sde_vm->base.io_mem_handle < 0)
		return 0;

	rc = hh_rm_mem_reclaim(sde_vm->base.io_mem_handle, 0);
	if (rc) {
		SDE_ERROR("failed to reclaim IO memory, rc=%d\n", rc);
		goto reclaim_fail;
	}

	SDE_INFO("mem reclaim succeeded\n");
reclaim_fail:
	sde_vm->base.io_mem_handle = -1;

	return rc;
}

int _sde_vm_reclaim_irq(struct sde_kms *sde_kms)
{
	struct sde_vm_primary *sde_vm = to_vm_primary(sde_kms->vm);
	struct sde_vm_irq_desc *irq_desc;
	int rc = 0, i;

	if (!sde_vm->irq_desc)
		return 0;

	irq_desc = sde_vm->irq_desc;

	for (i = atomic_read(&sde_vm->base.n_irq_lent) - 1; i >= 0; i--) {
		struct sde_vm_irq_entry *entry = &irq_desc->irq_entries[i];

		rc = hh_irq_reclaim(entry->label);
		if (rc) {
			SDE_ERROR("failed to reclaim irq label: %d rc = %d\n",
					entry->label, rc);
			goto reclaim_fail;
		}

		atomic_dec(&sde_vm->base.n_irq_lent);

		SDE_INFO("irq reclaim succeeded for label: %d\n", entry->label);
	}

reclaim_fail:
	sde_vm_free_irq(sde_vm->irq_desc);
	sde_vm->irq_desc = NULL;
	atomic_set(&sde_vm->base.n_irq_lent, 0);

	return rc;
}

static int _sde_vm_reclaim(struct sde_kms *sde_kms)
{
	int rc = 0;

	rc = _sde_vm_reclaim_mem(sde_kms);
	if (rc) {
		SDE_ERROR("vm reclaim mem failed, rc=%d\n", rc);
		goto end;
	}

	rc = _sde_vm_reclaim_irq(sde_kms);
	if (rc)
		SDE_ERROR("vm reclaim irq failed, rc=%d\n", rc);

end:
	return rc;
}

static int _sde_vm_lend_mem(struct sde_vm *vm,
					 struct msm_io_res *io_res)
{
	struct sde_vm_primary *sde_vm;
	struct hh_acl_desc *acl_desc;
	struct hh_sgl_desc *sgl_desc;
	struct hh_notify_vmid_desc *vmid_desc;
	hh_memparcel_handle_t mem_handle;
	hh_vmid_t trusted_vmid;
	int rc = 0;

	sde_vm = to_vm_primary(vm);

	acl_desc = sde_vm_populate_acl(HH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		SDE_ERROR("failed to populate acl descriptor, rc = %d\n",
			   PTR_ERR(acl_desc));
		return -EINVAL;
	}

	sgl_desc = sde_vm_populate_sgl(io_res);
	if (IS_ERR_OR_NULL(sgl_desc)) {
		SDE_ERROR("failed to populate sgl descriptor, rc = %d\n",
			   PTR_ERR(sgl_desc));
		rc = -EINVAL;
		goto sgl_fail;
	}

	rc = hh_rm_mem_lend(HH_RM_MEM_TYPE_IO, 0, SDE_VM_MEM_LABEL,
				 acl_desc, sgl_desc, NULL, &mem_handle);
	if (rc) {
		SDE_ERROR("hyp lend failed with error, rc: %d\n", rc);
		goto fail;
	}

	sde_vm->base.io_mem_handle = mem_handle;

	hh_rm_get_vmid(HH_TRUSTED_VM, &trusted_vmid);

	vmid_desc = sde_vm_populate_vmid(trusted_vmid);

	rc = hh_rm_mem_notify(mem_handle, HH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
				  HH_MEM_NOTIFIER_TAG_DISPLAY, vmid_desc);
	if (rc) {
		SDE_ERROR("hyp mem notify failed, rc = %d\n", rc);
		goto notify_fail;
	}

	SDE_INFO("IO memory lend suceeded for tag: %d\n",
			HH_MEM_NOTIFIER_TAG_DISPLAY);

notify_fail:
	kfree(vmid_desc);
fail:
	kfree(sgl_desc);
sgl_fail:
	kfree(acl_desc);

	return rc;
}

static int _sde_vm_lend_irq(struct sde_vm *vm, struct msm_io_res *io_res)
{
	struct sde_vm_primary *sde_vm;
	struct sde_vm_irq_desc *irq_desc;
	int i, rc = 0;

	sde_vm = to_vm_primary(vm);

	irq_desc = sde_vm_populate_irq(io_res);

	/* cache the irq list for validation during reclaim */
	sde_vm->irq_desc = irq_desc;

	for (i  = 0; i < irq_desc->n_irq; i++) {
		struct sde_vm_irq_entry *entry = &irq_desc->irq_entries[i];

		rc = hh_irq_lend_v2(entry->label, HH_TRUSTED_VM, entry->irq,
				 sde_vm_irq_release_notification_handler,
				 sde_vm);
		if (rc) {
			SDE_ERROR("irq lend failed for irq label: %d, rc=%d\n",
				  entry->label, rc);
			goto done;
		}

		atomic_inc(&sde_vm->base.n_irq_lent);

		rc = hh_irq_lend_notify(entry->label);
		if (rc) {
			SDE_ERROR("irq lend notify failed, label: %d, rc=%d\n",
				entry->label, rc);
			goto done;
		}

		SDE_INFO("vm lend suceeded for IRQ label: %d\n", entry->label);
	}

done:
	return rc;
}

static int _sde_vm_release(struct sde_kms *kms)
{
	struct msm_io_res io_res;
	struct sde_vm_primary *sde_vm;
	int rc = 0;

	if (!kms->vm)
		return 0;

	sde_vm = to_vm_primary(kms->vm);

	INIT_LIST_HEAD(&io_res.mem);
	INIT_LIST_HEAD(&io_res.irq);

	rc = sde_vm_get_resources(kms, &io_res);
	if (rc) {
		SDE_ERROR("fail to get resources\n");
		goto done;
	}

	rc = _sde_vm_lend_mem(kms->vm, &io_res);
	if (rc) {
		SDE_ERROR("fail to lend notify resources\n");
		goto res_lend_fail;
	}

	rc = _sde_vm_lend_irq(kms->vm, &io_res);
	if (rc) {
		SDE_ERROR("failed to lend irq's\n");
		goto res_lend_fail;
	}

	goto done;

res_lend_fail:
	_sde_vm_reclaim(kms);
done:
	sde_vm_free_resources(&io_res);

	return rc;
}

static void _sde_vm_deinit(struct sde_kms *sde_kms, struct sde_vm_ops *ops)
{
	struct sde_vm_primary *sde_vm;

	if (!sde_kms->vm)
		return;

	memset(ops, 0, sizeof(*ops));

	sde_vm = to_vm_primary(sde_kms->vm);

	sde_vm_msgq_deinit(sde_kms->vm);

	if (sde_vm->base.mem_notification_cookie)
		hh_mem_notifier_unregister(
				sde_vm->base.mem_notification_cookie);

	if (sde_vm->irq_desc)
		sde_vm_free_irq(sde_vm->irq_desc);

	kfree(sde_vm);
}

static void _sde_vm_set_ops(struct sde_vm_ops *ops)
{
	memset(ops, 0, sizeof(*ops));

	ops->vm_client_pre_release = sde_vm_pre_release;
	ops->vm_client_post_acquire = sde_vm_post_acquire;
	ops->vm_release = _sde_vm_release;
	ops->vm_acquire = _sde_vm_reclaim;
	ops->vm_owns_hw = _sde_vm_owns_hw;
	ops->vm_deinit = _sde_vm_deinit;
	ops->vm_prepare_commit = sde_kms_vm_primary_prepare_commit;
	ops->vm_post_commit = sde_kms_vm_primary_post_commit;
	ops->vm_request_valid = sde_vm_request_valid;
	ops->vm_msg_send = sde_vm_msg_send;
}

int sde_vm_primary_init(struct sde_kms *kms)
{
	struct sde_vm_primary *sde_vm;
	void *cookie;
	int rc = 0;

	sde_vm = kzalloc(sizeof(*sde_vm), GFP_KERNEL);
	if (!sde_vm)
		return -ENOMEM;

	_sde_vm_set_ops(&sde_vm->base.vm_ops);

	cookie = hh_mem_notifier_register(HH_MEM_NOTIFIER_TAG_DISPLAY,
			       sde_vm_mem_release_notification_handler, sde_vm);
	if (!cookie) {
		SDE_ERROR("fails to register RM mem release notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}

	sde_vm->base.mem_notification_cookie = cookie;
	sde_vm->base.sde_kms = kms;
	sde_vm->base.io_mem_handle = -1; // 0 is a valid handle
	kms->vm = &sde_vm->base;

	mutex_init(&sde_vm->base.vm_res_lock);

	rc = sde_vm_msgq_init(kms->vm);
	if (rc) {
		SDE_ERROR("failed to initialize the msgq, rc=%d\n", rc);
		goto init_fail;
	}

	return 0;
init_fail:
	_sde_vm_deinit(kms, &sde_vm->base.vm_ops);

	return rc;
}
