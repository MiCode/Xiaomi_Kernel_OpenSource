/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_RM_DRV_H
#define __HH_RM_DRV_H

#include <linux/types.h>
#include <linux/notifier.h>

#include "hh_common.h"

/* Notification type Message IDs */
/* Memory APIs */
#define HH_RM_NOTIF_MEM_SHARED		0x51100011
#define HH_RM_NOTIF_MEM_RELEASED	0x51100012
#define HH_RM_NOTIF_MEM_ACCEPTED	0x51100013

#define HH_RM_MEM_TYPE_NORMAL	0
#define HH_RM_MEM_TYPE_IO	1

#define HH_RM_TRANS_TYPE_DONATE	0
#define HH_RM_TRANS_TYPE_LEND	1
#define HH_RM_TRANS_TYPE_SHARE	2

#define HH_RM_ACL_X		BIT(0)
#define HH_RM_ACL_W		BIT(1)
#define HH_RM_ACL_R		BIT(2)

#define HH_RM_MEM_RELEASE_CLEAR BIT(0)
#define HH_RM_MEM_RECLAIM_CLEAR BIT(0)

#define HH_RM_MEM_ACCEPT_VALIDATE_SANITIZED	BIT(0)
#define HH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS	BIT(1)
#define HH_RM_MEM_ACCEPT_VALIDATE_LABEL		BIT(2)
#define HH_RM_MEM_ACCEPT_DONE			BIT(7)

#define HH_RM_MEM_SHARE_SANITIZE		BIT(0)
#define HH_RM_MEM_LEND_SANITIZE			BIT(0)

#define HH_RM_MEM_NOTIFY_RECIPIENT_SHARED	BIT(0)
#define HH_RM_MEM_NOTIFY_RECIPIENT	HH_RM_MEM_NOTIFY_RECIPIENT_SHARED
#define HH_RM_MEM_NOTIFY_OWNER_RELEASED		BIT(1)
#define HH_RM_MEM_NOTIFY_OWNER		HH_RM_MEM_NOTIFY_OWNER_RELEASED
#define HH_RM_MEM_NOTIFY_OWNER_ACCEPTED		BIT(2)

struct hh_rm_mem_shared_acl_entry;
struct hh_rm_mem_shared_sgl_entry;
struct hh_rm_mem_shared_attr_entry;

struct hh_rm_notif_mem_shared_payload {
	u32 mem_handle;
	u8 mem_type;
	u8 trans_type;
	u8 flags;
	u8 reserved1;
	u16 owner_vmid;
	u16 reserved2;
	u32 label;
	hh_label_t mem_info_tag;
	/* TODO: How to arrange multiple variable length struct arrays? */
} __packed;

struct hh_rm_mem_shared_acl_entry {
	u16 acl_vmid;
	u8 acl_rights;
	u8 reserved;
} __packed;

struct hh_rm_mem_shared_sgl_entry {
	u32 sgl_size_low;
	u32 sgl_size_high;
} __packed;

struct hh_rm_mem_shared_attr_entry {
	u16 attributes;
	u16 attributes_vmid;
} __packed;

struct hh_rm_notif_mem_released_payload {
	u32 mem_handle;
	u16 participant_vmid;
	u16 reserved;
	hh_label_t mem_info_tag;
} __packed;

struct hh_rm_notif_mem_accepted_payload {
	u32 mem_handle;
	u16 participant_vmid;
	u16 reserved;
	hh_label_t mem_info_tag;
} __packed;

struct hh_acl_entry {
	u16 vmid;
	u8 perms;
	u8 reserved;
} __packed;

struct hh_sgl_entry {
	u64 ipa_base;
	u64 size;
} __packed;

struct hh_mem_attr_entry {
	u16 attr;
	u16 vmid;
} __packed;

struct hh_acl_desc {
	u32 n_acl_entries;
	struct hh_acl_entry acl_entries[];
} __packed;

struct hh_sgl_desc {
	u16 n_sgl_entries;
	u16 reserved;
	struct hh_sgl_entry sgl_entries[];
} __packed;

struct hh_mem_attr_desc {
	u16 n_mem_attr_entries;
	u16 reserved;
	struct hh_mem_attr_entry attr_entries[];
} __packed;

struct hh_notify_vmid_entry {
	u16 vmid;
	u16 reserved;
} __packed;

struct hh_notify_vmid_desc {
	u16 n_vmid_entries;
	u16 reserved;
	struct hh_notify_vmid_entry vmid_entries[];
} __packed;

/* VM APIs */
#define HH_RM_NOTIF_VM_STATUS		0x56100008
#define HH_RM_NOTIF_VM_IRQ_LENT		0x56100011
#define HH_RM_NOTIF_VM_IRQ_RELEASED	0x56100012
#define HH_RM_NOTIF_VM_IRQ_ACCEPTED	0x56100013

#define HH_RM_VM_STATUS_NO_STATE	0
#define HH_RM_VM_STATUS_INIT		1
#define HH_RM_VM_STATUS_READY		2
#define HH_RM_VM_STATUS_RUNNING		3
#define HH_RM_VM_STATUS_PAUSED		4
#define HH_RM_VM_STATUS_SHUTDOWN	5
#define HH_RM_VM_STATUS_SHUTOFF		6
#define HH_RM_VM_STATUS_CRASHED		7
#define HH_RM_VM_STATUS_INIT_FAILED	8

#define HH_RM_OS_STATUS_NONE		0
#define HH_RM_OS_STATUS_EARLY_BOOT	1
#define HH_RM_OS_STATUS_BOOT		2
#define HH_RM_OS_STATUS_INIT		3
#define HH_RM_OS_STATUS_RUN		4

struct hh_rm_notif_vm_status_payload {
	hh_vmid_t vmid;
	u16 reserved;
	u8 vm_status;
	u8 os_status;
	u16 app_status;
} __packed;

struct hh_rm_notif_vm_irq_lent_payload {
	hh_vmid_t owner_vmid;
	u16 reserved;
	hh_virq_handle_t virq_handle;
	hh_label_t virq_label;
} __packed;

struct hh_rm_notif_vm_irq_released_payload {
	hh_virq_handle_t virq_handle;
} __packed;

struct hh_rm_notif_vm_irq_accepted_payload {
	hh_virq_handle_t virq_handle;
} __packed;

/* VM Services */
#define HH_RM_NOTIF_VM_CONSOLE_CHARS	0X56100080

struct hh_rm_notif_vm_console_chars {
	hh_vmid_t vmid;
	u16 num_bytes;
	u8 bytes[0];
} __packed;

struct notifier_block;

typedef int (*hh_virtio_mmio_cb_t)(hh_vmid_t peer, const char *vm_name,
	hh_label_t label, hh_capid_t cap_id, int linux_irq, u64 base, u64 size);
typedef int (*hh_vcpu_affinity_cb_t)(hh_label_t label, hh_capid_t cap_id);

#if IS_ENABLED(CONFIG_HH_RM_DRV)
/* RM client registration APIs */
int hh_rm_register_notifier(struct notifier_block *nb);
int hh_rm_unregister_notifier(struct notifier_block *nb);

/* Client APIs for IRQ management */
int hh_rm_virq_to_irq(u32 virq, u32 type);
int hh_rm_irq_to_virq(int irq, u32 *virq);

int hh_rm_vm_irq_lend(hh_vmid_t vmid,
		      int virq,
		      int label,
		      hh_virq_handle_t *virq_handle);
int hh_rm_vm_irq_lend_notify(hh_vmid_t vmid, hh_virq_handle_t virq_handle);
int hh_rm_vm_irq_accept(hh_virq_handle_t virq_handle, int virq);
int hh_rm_vm_irq_accept_notify(hh_vmid_t vmid, hh_virq_handle_t virq_handle);
int hh_rm_vm_irq_release(hh_virq_handle_t virq_handle);
int hh_rm_vm_irq_release_notify(hh_vmid_t vmid, hh_virq_handle_t virq_handle);


int hh_rm_vm_irq_reclaim(hh_virq_handle_t virq_handle);

int hh_rm_set_virtio_mmio_cb(hh_virtio_mmio_cb_t fnptr);
void gh_rm_unset_virtio_mmio_cb(void);
int hh_rm_set_vcpu_affinity_cb(hh_vcpu_affinity_cb_t fnptr);

/* Client APIs for VM management */
int hh_rm_vm_alloc_vmid(enum hh_vm_names vm_name, int *vmid);
int hh_rm_get_vmid(enum hh_vm_names vm_name, hh_vmid_t *vmid);
int hh_rm_get_vm_name(hh_vmid_t vmid, enum hh_vm_names *vm_name);
int hh_rm_get_vminfo(enum hh_vm_names vm_name, struct hh_vminfo *vminfo);
int hh_rm_vm_start(int vmid);
int hh_rm_get_vm_id_info(enum hh_vm_names vm_name, hh_vmid_t vmid);

/* Client APIs for VM query */
int hh_rm_populate_hyp_res(hh_vmid_t vmid, const char *vm_name);

/* Client APIs for VM Services */
int hh_rm_console_open(hh_vmid_t vmid);
int hh_rm_console_close(hh_vmid_t vmid);
int hh_rm_console_write(hh_vmid_t vmid, const char *buf, size_t size);
int hh_rm_console_flush(hh_vmid_t vmid);
int hh_rm_mem_qcom_lookup_sgl(u8 mem_type, hh_label_t label,
			      struct hh_acl_desc *acl_desc,
			      struct hh_sgl_desc *sgl_desc,
			      struct hh_mem_attr_desc *mem_attr_desc,
			      hh_memparcel_handle_t *handle);
int hh_rm_mem_release(hh_memparcel_handle_t handle, u8 flags);
int hh_rm_mem_reclaim(hh_memparcel_handle_t handle, u8 flags);
struct hh_sgl_desc *hh_rm_mem_accept(hh_memparcel_handle_t handle, u8 mem_type,
				     u8 trans_type, u8 flags, hh_label_t label,
				     struct hh_acl_desc *acl_desc,
				     struct hh_sgl_desc *sgl_desc,
				     struct hh_mem_attr_desc *mem_attr_desc,
				     u16 map_vmid);
int hh_rm_mem_share(u8 mem_type, u8 flags, hh_label_t label,
		    struct hh_acl_desc *acl_desc, struct hh_sgl_desc *sgl_desc,
		    struct hh_mem_attr_desc *mem_attr_desc,
		    hh_memparcel_handle_t *handle);
int hh_rm_mem_lend(u8 mem_type, u8 flags, hh_label_t label,
		   struct hh_acl_desc *acl_desc, struct hh_sgl_desc *sgl_desc,
		   struct hh_mem_attr_desc *mem_attr_desc,
		   hh_memparcel_handle_t *handle);
int hh_rm_mem_notify(hh_memparcel_handle_t handle, u8 flags,
		     hh_label_t mem_info_tag,
		     struct hh_notify_vmid_desc *vmid_desc);

#else
/* RM client register notifications APIs */
static inline int hh_rm_register_notifier(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int hh_rm_unregister_notifier(struct notifier_block *nb)
{
	return -ENODEV;
}

/* Client APIs for IRQ management */
static inline int hh_rm_virq_to_irq(u32 virq)
{
	return -EINVAL;
}

static inline int hh_rm_vm_irq_lend(hh_vmid_t vmid,
				    int virq,
				    int label,
				    hh_virq_handle_t *virq_handle)
{
	return -EINVAL;
}

static inline int hh_rm_irq_to_virq(int irq, u32 *virq)
{
	return -EINVAL;
}

static inline int hh_rm_vm_irq_lend_notify(hh_vmid_t vmid,
					   hh_virq_handle_t virq_handle)
{
	return -EINVAL;
}

static inline int hh_rm_vm_irq_accept(hh_virq_handle_t virq_handle, int virq)
{
	return -EINVAL;

}

static inline int hh_rm_vm_irq_accept_notify(hh_vmid_t vmid,
					     hh_virq_handle_t virq_handle)
{
	return -EINVAL;
}

static inline int hh_rm_vm_irq_release(hh_virq_handle_t virq_handle)
{
	return -EINVAL;
}

static inline int hh_rm_vm_irq_release_notify(hh_vmid_t vmid,
					      hh_virq_handle_t virq_handle)
{
	return -EINVAL;
}

static inline int hh_rm_vm_irq_reclaim(hh_virq_handle_t virq_handle)
{
	return -EINVAL;
}

/* Client APIs for VM management */
static inline int hh_rm_vm_alloc_vmid(enum hh_vm_names vm_name, int *vmid)
{
	return -EINVAL;
}

static inline int hh_rm_get_vmid(enum hh_vm_names vm_name, hh_vmid_t *vmid)
{
	return -EINVAL;
}

static inline int hh_rm_get_vm_name(hh_vmid_t vmid, enum hh_vm_names *vm_name)
{
	return -EINVAL;
}

static inline int hh_rm_get_vminfo(enum hh_vm_names vm_name, struct hh_vminfo *vminfo);
{
	return -EINVAL;
}

static inline int hh_rm_vm_start(int vmid)
{
	return -EINVAL;
}

static inline int hh_rm_get_vm_id_info(enum hh_vm_names vm_name, hh_vmid_t vmid)
{
	return -EINVAL;
}

/* Client APIs for VM query */
static inline int hh_rm_populate_hyp_res(hh_vmid_t vmid, const char *vm_name)
{
	return -EINVAL;
}

/* Client APIs for VM Services */
static inline int hh_rm_console_open(hh_vmid_t vmid)
{
	return -EINVAL;
}

static inline int hh_rm_console_close(hh_vmid_t vmid)
{
	return -EINVAL;
}

static inline int hh_rm_console_write(hh_vmid_t vmid, const char *buf,
					size_t size)
{
	return -EINVAL;
}

static inline int hh_rm_console_flush(hh_vmid_t vmid)
{
	return -EINVAL;
}

static inline int hh_rm_mem_qcom_lookup_sgl(u8 mem_type, hh_label_t label,
			      struct hh_acl_desc *acl_desc,
			      struct hh_sgl_desc *sgl_desc,
			      struct hh_mem_attr_desc *mem_attr_desc,
			      hh_memparcel_handle_t *handle)
{
	return -EINVAL;
}

static inline int hh_rm_mem_release(hh_memparcel_handle_t handle, u8 flags)
{
	return -EINVAL;
}

static inline int hh_rm_mem_reclaim(hh_memparcel_handle_t handle, u8 flags)
{
	return -EINVAL;
}

static inline struct hh_sgl_desc *hh_rm_mem_accept(hh_memparcel_handle_t handle,
				     u8 mem_type,
				     u8 trans_type, u8 flags, hh_label_t label,
				     struct hh_acl_desc *acl_desc,
				     struct hh_sgl_desc *sgl_desc,
				     struct hh_mem_attr_desc *mem_attr_desc,
				     u16 map_vmid)
{
	return ERR_PTR(-EINVAL);
}

static inline int hh_rm_mem_share(u8 mem_type, u8 flags, hh_label_t label,
		    struct hh_acl_desc *acl_desc, struct hh_sgl_desc *sgl_desc,
		    struct hh_mem_attr_desc *mem_attr_desc,
		    hh_memparcel_handle_t *handle)
{
	return -EINVAL;
}

static inline int hh_rm_mem_lend(u8 mem_type, u8 flags, hh_label_t label,
		   struct hh_acl_desc *acl_desc, struct hh_sgl_desc *sgl_desc,
		   struct hh_mem_attr_desc *mem_attr_desc,
		   hh_memparcel_handle_t *handle)
{
	return -EINVAL;
}

static inline int hh_rm_mem_notify(hh_memparcel_handle_t handle, u8 flags,
				   hh_label_t mem_info_tag,
				   struct hh_notify_vmid_desc *vmid_desc)
{
	return -EINVAL;
}

static inline int hh_rm_set_virtio_mmio_cb(hh_virtio_mmio_cb_t fnptr)
{
	return -EINVAL;
}

static inline void gh_rm_unset_virtio_mmio_cb(void)
{

}

static inline int hh_rm_set_vcpu_affinity_cb(hh_vcpu_affinity_cb fnptr)
{
	return -EINVAL;
}
#endif
#endif
