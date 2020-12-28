// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/list_sort.h>
#include "linux/sde_rsc.h"
#include "dsi/dsi_display.h"
#include "dp/dp_display.h"
#include "sde_kms.h"
#include "sde_vm_common.h"
#include "sde_crtc.h"

struct hh_notify_vmid_desc *sde_vm_populate_vmid(hh_vmid_t vmid)
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

struct hh_acl_desc *sde_vm_populate_acl(enum hh_vm_names vm_name)
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

int __mem_sort_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct msm_io_mem_entry *left =
		container_of(a, struct msm_io_mem_entry, list);
	struct msm_io_mem_entry *right =
		container_of(b, struct msm_io_mem_entry, list);

	return (left->base - right->base);
}

bool __merge_on_overlap(struct msm_io_mem_entry *res,
		const struct msm_io_mem_entry *left,
		const struct msm_io_mem_entry *right)
{
	phys_addr_t l_s = left->base;
	phys_addr_t l_e = left->base + left->size;
	phys_addr_t r_s = right->base;
	phys_addr_t r_e = right->base + right->size;

	memset(res, 0, sizeof(*res));

	if (r_s <= l_e) {
		res->base = min(l_s, r_s);
		res->size = max(l_e, r_e) - res->base;

		return true;
	}

	return false;
}

void _sde_vm_sort_and_align(struct list_head *mem)
{
	struct msm_io_mem_entry *entry, *tmp, *prev = NULL;
	struct msm_io_mem_entry merged_entry;

	list_for_each_entry(entry, mem, list) {
		entry->base = ALIGN_DOWN(entry->base, PAGE_SIZE);
		entry->size = ALIGN(entry->size, PAGE_SIZE);
	}

	list_sort(NULL, mem, __mem_sort_cmp);

	list_for_each_entry_safe(entry, tmp, mem, list) {
		if (prev && __merge_on_overlap(&merged_entry, prev, entry)) {
			prev->base = merged_entry.base;
			prev->size = merged_entry.size;

			list_del(&entry->list);
			entry = prev;
		}
		prev = entry;
	}

	list_for_each_entry(entry, mem, list)
		SDE_DEBUG("base: 0x%x - size: 0x%x\n",
				entry->base, entry->size);
}

struct hh_sgl_desc *sde_vm_populate_sgl(struct msm_io_res *io_res)
{
	struct hh_sgl_desc *sgl_desc;
	struct msm_io_mem_entry *mem;
	u32 i = 0, num_mem_entry = 0;

	_sde_vm_sort_and_align(&io_res->mem);

	list_for_each_entry(mem, &io_res->mem, list)
		num_mem_entry++;

	sgl_desc = kzalloc(offsetof(struct hh_sgl_desc,
			   sgl_entries[num_mem_entry]), GFP_KERNEL);
	if (!sgl_desc)
		return ERR_PTR(ENOMEM);

	sgl_desc->n_sgl_entries = num_mem_entry;
	list_for_each_entry(mem, &io_res->mem, list) {
		sgl_desc->sgl_entries[i].ipa_base = mem->base;
		sgl_desc->sgl_entries[i].size = mem->size;
		i++;
	}

	msm_dss_clean_io_mem(&io_res->mem);

	return sgl_desc;
}

struct sde_vm_irq_desc *sde_vm_populate_irq(struct msm_io_res *io_res)
{
	struct msm_io_irq_entry *irq;
	u32 i = 0, num_irq = 0;
	struct sde_vm_irq_desc *irq_desc;

	list_for_each_entry(irq, &io_res->irq, list)
		num_irq++;

	irq_desc = kzalloc(sizeof(*irq_desc), GFP_KERNEL);
	if (!irq_desc)
		return ERR_PTR(ENOMEM);

	irq_desc->irq_entries = kcalloc(num_irq,
					sizeof(struct sde_vm_irq_entry),
					GFP_KERNEL);
	if (!irq_desc->irq_entries) {
		sde_vm_free_irq(irq_desc);
		return ERR_PTR(ENOMEM);
	}

	list_for_each_entry(irq, &io_res->irq, list) {
		struct sde_vm_irq_entry *entry = &irq_desc->irq_entries[i];

		entry->irq = irq->irq_num;
		entry->label = irq->label;
		i++;
	}

	irq_desc->n_irq = num_irq;

	msm_dss_clean_io_irq(&io_res->irq);

	return irq_desc;
}

void sde_vm_free_irq(struct sde_vm_irq_desc *irq_desc)
{
	if (irq_desc && irq_desc->irq_entries)
		kfree(irq_desc->irq_entries);

	kfree(irq_desc);
}

int sde_vm_get_resources(struct sde_kms *sde_kms, struct msm_io_res *io_res)
{
	struct msm_drm_private *priv = sde_kms->dev->dev_private;
	struct msm_vm_client_entry *entry;
	int rc = 0;

	rc = sde_kms_get_io_resources(sde_kms, io_res);
	if (rc)
		goto fail_get_res;

	list_for_each_entry(entry, &priv->vm_client_list, list) {
		if (!entry->ops.vm_get_io_resources)
			continue;

		rc = entry->ops.vm_get_io_resources(io_res, entry->data);
		if (rc) {
			SDE_ERROR("get_io_resources failed for device: %d\n",
					 entry->dev->id);
			goto fail_get_res;
		}
	}

	return rc;

fail_get_res:
	msm_dss_clean_io_mem(&io_res->mem);
	msm_dss_clean_io_irq(&io_res->irq);

	return rc;
}

void sde_vm_free_resources(struct msm_io_res *io_res)
{
	msm_dss_clean_io_mem(&io_res->mem);
	msm_dss_clean_io_irq(&io_res->irq);
}

int sde_vm_post_acquire(struct sde_kms *kms)
{
	struct msm_drm_private *priv = kms->dev->dev_private;
	struct msm_vm_client_entry *entry;
	int rc = 0;

	list_for_each_entry(entry, &priv->vm_client_list, list) {
		if (!entry->ops.vm_post_hw_acquire)
			continue;

		rc = entry->ops.vm_post_hw_acquire(entry->data);
		if (rc) {
			SDE_ERROR("post_acquire failed for device: %d\n",
					   entry->dev->id);
			goto post_acquire_rollback;
		}
	}

	return rc;

post_acquire_rollback:
	list_for_each_entry_continue_reverse(entry, &priv->vm_client_list,
			list) {
		if (!entry->ops.vm_pre_hw_release)
			continue;

		rc = entry->ops.vm_pre_hw_release(entry->data);
		if (rc) {
			SDE_ERROR(
				"post_acquire failed during rollback for device: %d\n",
				entry->dev->id);
			break;
		}
	}

	return rc;
}

int sde_vm_pre_release(struct sde_kms *kms)
{
	struct msm_drm_private *priv = kms->dev->dev_private;
	struct msm_vm_client_entry *entry;
	int rc = 0;

	list_for_each_entry(entry, &priv->vm_client_list, list) {
		if (!entry->ops.vm_pre_hw_release)
			continue;

		rc = entry->ops.vm_pre_hw_release(entry->data);
		if (rc) {
			SDE_ERROR("pre_release failed for device: %d\n",
					   entry->dev->id);
			goto pre_release_rollback;
		}
	}

	return rc;

pre_release_rollback:
	list_for_each_entry_continue_reverse(entry, &priv->vm_client_list,
			list) {
		if (!entry->ops.vm_post_hw_acquire)
			continue;

		rc = entry->ops.vm_post_hw_acquire(entry->data);
		if (rc) {
			SDE_ERROR(
				"post_acquire failed during rollback for device: %d\n",
				entry->dev->id);
			break;
		}
	}

	return rc;
}

int sde_vm_request_valid(struct sde_kms *sde_kms,
			  enum sde_crtc_vm_req old_state,
			  enum sde_crtc_vm_req new_state)
{
	struct sde_vm_ops *vm_ops;
	int rc = 0;

	vm_ops = &sde_kms->vm->vm_ops;

	switch (new_state) {
	case VM_REQ_RELEASE:
	case VM_REQ_NONE:
		if ((old_state == VM_REQ_RELEASE) ||
			!vm_ops->vm_owns_hw(sde_kms))
			rc = -EINVAL;
		break;
	case VM_REQ_ACQUIRE:
		if (old_state != VM_REQ_RELEASE) {
			rc = -EINVAL;
		} else if (!vm_ops->vm_owns_hw(sde_kms)) {
			if (vm_ops->vm_acquire)
				rc = vm_ops->vm_acquire(sde_kms);
			else
				rc = -EINVAL;
		}
		break;
	default:
		SDE_ERROR("invalid vm request\n");
		rc = -EINVAL;
	};

	SDE_DEBUG("old req: %d new req: %d owns_hw: %d, rc: %d\n",
			old_state, new_state,
			vm_ops->vm_owns_hw(sde_kms), rc);
	SDE_EVT32(old_state, new_state, vm_ops->vm_owns_hw(sde_kms), rc);

	return rc;
}
