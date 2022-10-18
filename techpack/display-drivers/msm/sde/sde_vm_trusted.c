// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_irq_lend.h>
#include <linux/gunyah/gh_mem_notifier.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include "sde_kms.h"
#include "sde_vm_common.h"
#include "sde_vm.h"
#include "sde_vm_msgq.h"

#define to_vm_trusted(vm) ((struct sde_vm_trusted *)vm)

static int __sgl_cmp(const void *a, const void *b)
{
	struct gh_sgl_entry *l = (struct gh_sgl_entry *)a;
	struct gh_sgl_entry *r = (struct gh_sgl_entry *)b;

	return  (l->ipa_base - r->ipa_base);
}

int _sde_vm_validate_sgl(struct gh_sgl_desc *expected,
			 struct gh_sgl_desc *assigned)
{
	u32 idx;

	/*
	 * fragmented address spaces are not supported.
	 * So the number of sgl entries is expected to be the same.
	 */
	if (expected->n_sgl_entries != assigned->n_sgl_entries)
		return -E2BIG;

	sort(assigned->sgl_entries, assigned->n_sgl_entries,
			sizeof(assigned->sgl_entries[0]), __sgl_cmp, NULL);

	for (idx = 0; idx < expected->n_sgl_entries; idx++) {
		struct gh_sgl_entry *e = &expected->sgl_entries[idx];
		struct gh_sgl_entry *a = &assigned->sgl_entries[idx];

		if ((e->ipa_base != a->ipa_base) || (e->size != a->size)) {
			SDE_DEBUG("sgl mismatch: (%llu - %llu) vs (%llu - %llu)\n",
				   e->ipa_base, e->size, a->ipa_base, a->size);
			return -EINVAL;
		}
	}

	return 0;
}

static int __irq_cmp(const void *a, const void *b)
{
	struct sde_vm_irq_entry *l = (struct sde_vm_irq_entry *)a;
	struct sde_vm_irq_entry *r = (struct sde_vm_irq_entry *)b;

	return  (l->label - r->label);
}

static void sde_vm_mem_lend_notification_handler(enum gh_mem_notifier_tag tag,
					       unsigned long notif_type,
					void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_shared_payload *payload;
	struct sde_vm_trusted *sde_vm;

	if (notif_type != GH_RM_NOTIF_MEM_SHARED ||
			tag != GH_MEM_NOTIFIER_TAG_DISPLAY)
		return;

	if (!entry_data || !notif_msg)
		return;

	payload = (struct gh_rm_notif_mem_shared_payload *)notif_msg;

	if (payload->trans_type != GH_RM_TRANS_TYPE_LEND ||
	    payload->label != SDE_VM_MEM_LABEL)
		return;

	sde_vm = (struct sde_vm_trusted *)entry_data;

	mutex_lock(&sde_vm->base.vm_res_lock);

	sde_vm->base.io_mem_handle = payload->mem_handle;

	mutex_unlock(&sde_vm->base.vm_res_lock);

	SDE_INFO("mem lend notification for tag: %d label: %d handle: %d\n",
			tag, payload->label, payload->mem_handle);
}

void sde_vm_irq_lend_notification_handler(void *req,
		unsigned long notif_type, enum gh_irq_label label)
{
	SDE_INFO("IRQ LEND notification for label: %d\n", label);
}

static int _sde_vm_release_irq(struct sde_vm *vm)
{
	struct sde_vm_trusted *sde_vm = (struct sde_vm_trusted *)vm;
	struct sde_vm_irq_desc *irq_desc = sde_vm->irq_desc;
	int i, rc = 0;

	for (i = atomic_read(&sde_vm->base.n_irq_lent) - 1; i >= 0; i--) {
		struct sde_vm_irq_entry *entry = &irq_desc->irq_entries[i];

		rc = gh_irq_release(entry->label);
		if (rc) {
			SDE_ERROR("failed to release IRQ label: %d rc = %d\n",
				  entry->label, rc);
			goto done;
		}

		atomic_dec(&sde_vm->base.n_irq_lent);

		rc = gh_irq_release_notify(entry->label);
		if (rc) {
			SDE_ERROR(
				 "irq release notify failed,label: %d rc: %d\n",
				 entry->label, rc);
			goto done;
		}

		SDE_INFO("sde vm irq release for label: %d succeeded\n",
				entry->label);
	}
done:
	return rc;
}

static int _sde_vm_release_mem(struct sde_vm *vm)
{
	int rc = 0;
	struct sde_vm_trusted *sde_vm = (struct sde_vm_trusted *)vm;

	if (sde_vm->base.io_mem_handle < 0)
		return 0;

	rc = gh_rm_mem_release(sde_vm->base.io_mem_handle, 0);
	if (rc) {
		SDE_ERROR("gh_rm_mem_release failed, rc=%d\n", rc);
		goto done;
	}

	rc = gh_rm_mem_notify(sde_vm->base.io_mem_handle,
			GH_RM_MEM_NOTIFY_OWNER_RELEASED,
			GH_MEM_NOTIFIER_TAG_DISPLAY, 0);
	if (rc) {
		SDE_ERROR("hyp mem notify on release failed, rc = %d\n", rc);
		goto done;
	}

	sde_vm->base.io_mem_handle = -1;

	SDE_INFO("sde vm mem release succeeded\n");
done:
	return rc;

}

static int _sde_vm_release(struct sde_kms *kms)
{
	struct sde_vm_trusted *sde_vm;
	int rc = 0;

	if (!kms->vm)
		return 0;

	sde_vm = to_vm_trusted(kms->vm);

	sde_kms_vm_trusted_resource_deinit(kms);

	rc = _sde_vm_release_mem(kms->vm);
	if (rc) {
		SDE_ERROR("mem_release failed, rc = %d\n", rc);
		goto end;
	}

	rc = _sde_vm_release_irq(kms->vm);
	if (rc)
		SDE_ERROR("irq_release failed, rc = %d\n", rc);

end:
	return rc;
}

int _sde_vm_resource_init(struct sde_kms *sde_kms,
		struct drm_atomic_state *state)
{
	int rc = 0;

	rc = sde_kms_vm_trusted_resource_init(sde_kms, state);
	if (rc)
		SDE_ERROR("vm resource init failed\n");

	return rc;
}

int _sde_vm_populate_res(struct sde_kms *sde_kms, struct sde_vm_trusted *vm)
{
	struct msm_io_res io_res;
	int rc = 0;

	INIT_LIST_HEAD(&io_res.mem);
	INIT_LIST_HEAD(&io_res.irq);

	rc = sde_vm_get_resources(sde_kms, &io_res);
	if (rc) {
		SDE_ERROR("fail to get resources\n");
		return rc;
	}

	vm->sgl_desc = sde_vm_populate_sgl(&io_res);
	if (IS_ERR_OR_NULL(vm->sgl_desc)) {
		SDE_ERROR("failed to parse sgl list\n");
		return PTR_ERR(vm->sgl_desc);
	}

	vm->irq_desc = sde_vm_populate_irq(&io_res);
	if (IS_ERR_OR_NULL(vm->irq_desc)) {
		SDE_ERROR("failed to parse irq list\n");
		return PTR_ERR(vm->irq_desc);
	}

	sort(vm->irq_desc->irq_entries, vm->irq_desc->n_irq,
		sizeof(vm->irq_desc->irq_entries[0]), __irq_cmp, NULL);
	sort(vm->sgl_desc->sgl_entries, vm->sgl_desc->n_sgl_entries,
		sizeof(vm->sgl_desc->sgl_entries[0]), __sgl_cmp, NULL);

	return rc;
}

static bool _sde_vm_owns_hw(struct sde_kms *sde_kms)
{
	struct sde_vm_trusted *sde_vm;
	bool owns_irq, owns_mem_io;

	sde_vm = to_vm_trusted(sde_kms->vm);

	owns_irq = (sde_vm->irq_desc->n_irq ==
			atomic_read(&sde_vm->base.n_irq_lent));
	owns_mem_io = (sde_vm->base.io_mem_handle >= 0);

	return (owns_irq && owns_mem_io);
}

static void  _sde_vm_deinit(struct sde_kms *kms, struct sde_vm_ops *ops)
{
	struct sde_vm_trusted *sde_vm;

	if (!kms->vm)
		return;

	sde_vm = to_vm_trusted(kms->vm);

	memset(ops, 0, sizeof(*ops));

	sde_vm_msgq_deinit(kms->vm);

	if (sde_vm->base.mem_notification_cookie)
		gh_mem_notifier_unregister(
				sde_vm->base.mem_notification_cookie);

	kfree(sde_vm->sgl_desc);

	if (sde_vm->irq_desc)
		sde_vm_free_irq(sde_vm->irq_desc);

	kfree(sde_vm);
}

static int _sde_vm_accept_mem(struct sde_vm *vm)
{
	struct gh_sgl_desc *sgl_desc;
	struct gh_acl_desc *acl_desc;
	struct sde_vm_trusted *sde_vm;
	int rc = 0;

	sde_vm = to_vm_trusted(vm);

	acl_desc = sde_vm_populate_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		SDE_ERROR("failed to populate acl data, rc=%ld\n",
			   PTR_ERR(acl_desc));
		rc = PTR_ERR(acl_desc);
		goto done;
	}

	sgl_desc = gh_rm_mem_accept(sde_vm->base.io_mem_handle,
				    GH_RM_MEM_TYPE_IO,
				    GH_RM_TRANS_TYPE_LEND,
				    GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS|
				    GH_RM_MEM_ACCEPT_VALIDATE_LABEL|
				    GH_RM_MEM_ACCEPT_DONE,
				    SDE_VM_MEM_LABEL,
				    acl_desc, NULL, NULL, 0);
	if (IS_ERR_OR_NULL(sgl_desc)) {
		SDE_ERROR("gh_rm_mem_accept failed with error, rc=%ld\n",
			   PTR_ERR(sgl_desc));
		rc = -EINVAL;

		/* ACCEPT didn't go through. So no need to call the RELEASE */
		sde_vm->base.io_mem_handle = -1;
		goto accept_fail;
	}

	rc = _sde_vm_validate_sgl(sde_vm->sgl_desc, sgl_desc);
	if (rc) {
		SDE_ERROR(
			"failed in sgl validation for SDE_VM_MEM_LABEL label, rc = %d\n",
			rc);
		goto accept_fail;
	}

	SDE_INFO("mem accept succeeded for SDE_VM_MEM_LABEL label\n");

	return 0;

accept_fail:
	kfree(acl_desc);
done:
	return rc;
}

static int _sde_vm_accept_irq(struct sde_vm *vm)
{
	struct sde_vm_trusted *sde_vm;
	struct sde_vm_irq_desc *irq_desc;
	struct irq_data *exp_irq_data, *acc_irq_data;
	int accepted_irq, expected_irq;
	int rc = 0, i;

	sde_vm = to_vm_trusted(vm);
	irq_desc = sde_vm->irq_desc;

	for (i = 0; i < irq_desc->n_irq; i++) {
		struct sde_vm_irq_entry *irq_entry = &irq_desc->irq_entries[i];

		expected_irq = irq_entry->irq;
		accepted_irq = gh_irq_accept(irq_entry->label, -1,
				IRQ_TYPE_LEVEL_HIGH);
		if (accepted_irq < 0) {
			SDE_ERROR("failed to accept irq for label: %d\n",
					irq_entry->label);
			rc = -EINVAL;
			goto end;
		}

		atomic_inc(&sde_vm->base.n_irq_lent);

		exp_irq_data = irq_get_irq_data(expected_irq);
		if (!exp_irq_data) {
			SDE_ERROR("failed to get irq data for irq: %d\n",
					expected_irq);
			rc = -EINVAL;
			goto end;
		}

		acc_irq_data = irq_get_irq_data(accepted_irq);
		if (!acc_irq_data) {
			SDE_ERROR("failed to get irq data for irq: %d\n",
					accepted_irq);
			rc = -EINVAL;
			goto end;
		}

		if (exp_irq_data->hwirq != acc_irq_data->hwirq) {
			SDE_ERROR("IRQ mismatch on ACCEPT for label %d\n",
					irq_entry->label);
			rc = -EINVAL;
			goto end;
		}

		SDE_INFO("IRQ accept succeeded for label %u irq: %lu\n",
				irq_entry->label, exp_irq_data->hwirq);
	}
end:
	return rc;
}

static int _sde_vm_accept(struct sde_kms *kms)
{
	int rc = 0;

	rc = _sde_vm_accept_mem(kms->vm);
	if (rc)
		goto res_accept_fail;

	rc = _sde_vm_accept_irq(kms->vm);
	if (rc)
		goto res_accept_fail;

	return 0;

res_accept_fail:
	_sde_vm_release_irq(kms->vm);
	_sde_vm_release_mem(kms->vm);

	return rc;
}

static void _sde_vm_set_ops(struct sde_vm_ops *ops)
{
	memset(ops, 0, sizeof(*ops));

	ops->vm_client_pre_release = sde_vm_pre_release;
	ops->vm_client_post_acquire = sde_vm_post_acquire;
	ops->vm_release = _sde_vm_release;
	ops->vm_acquire = _sde_vm_accept;
	ops->vm_owns_hw = _sde_vm_owns_hw;
	ops->vm_deinit = _sde_vm_deinit;
	ops->vm_prepare_commit = sde_kms_vm_trusted_prepare_commit;
	ops->vm_post_commit = sde_kms_vm_trusted_post_commit;
	ops->vm_request_valid = sde_vm_request_valid;
	ops->vm_acquire_fail_handler = _sde_vm_release;
	ops->vm_msg_send = sde_vm_msg_send;
	ops->vm_resource_init = _sde_vm_resource_init;
}

int sde_vm_trusted_init(struct sde_kms *kms)
{
	struct sde_vm_trusted *sde_vm;
	void *cookie;
	int rc = 0;

	sde_vm = kzalloc(sizeof(*sde_vm), GFP_KERNEL);
	if (!sde_vm)
		return -ENOMEM;

	_sde_vm_set_ops(&sde_vm->base.vm_ops);

	sde_vm->base.sde_kms = kms;

	mutex_init(&sde_vm->base.vm_res_lock);

	/**
	 * Optimize resource population by reading the entire HW resource
	 * space once during init. Once trusted vm starts supporting
	 * per-display space assignment, this read has to be done on each event
	 * notification.
	 */
	rc = _sde_vm_populate_res(kms, sde_vm);
	if (rc) {
		SDE_ERROR("failed to populate trusted vm res, rc= %d\n", rc);
		goto init_fail;
	}

	cookie = gh_mem_notifier_register(GH_MEM_NOTIFIER_TAG_DISPLAY,
			       sde_vm_mem_lend_notification_handler, sde_vm);
	if (!cookie) {
		SDE_ERROR("fails to register RM mem lend notifier\n");
		goto init_fail;
	}
	sde_vm->base.mem_notification_cookie = cookie;

	rc = gh_irq_wait_for_lend_v2(GH_IRQ_LABEL_SDE, GH_PRIMARY_VM,
				  sde_vm_irq_lend_notification_handler,
				  (void *)sde_vm);
	if (rc) {
		SDE_ERROR("wait for irq lend on label: %d failed, rc=%d\n",
			   GH_IRQ_LABEL_SDE, rc);
		goto init_fail;
	}

	kms->vm = &sde_vm->base;

	atomic_set(&sde_vm->base.n_irq_lent, 0);

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
