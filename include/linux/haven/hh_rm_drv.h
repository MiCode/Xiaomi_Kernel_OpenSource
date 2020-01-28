/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_RM_DRV_H
#define __HH_RM_DRV_H

#include <linux/types.h>

#include "hh_common.h"

/* Notification type Message IDs */
/* Memory APIs */
#define HH_RM_NOTIF_MEM_SHARED		0x51100011
#define HH_RM_NOTIF_MEM_RELEASED	0x51100012

#define HH_RM_MEM_TYPE_NORMAL	0
#define HH_RM_MEM_TYPE_IO	1

#define HH_RM_TRANS_TYPE_DONATE	0
#define HH_RM_TRANS_TYPE_LEND	1
#define HH_RM_TRANS_TYPE_SHARE	2

#define HH_RM_ACL_X		BIT(0)
#define HH_RM_ACL_W		BIT(1)
#define HH_RM_ACL_R		BIT(2)

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
} __packed;

/* VM APIs */
#define HH_RM_NOTIF_VM_STATUS		0x56100008
#define HH_RM_NOTIF_VM_IRQ_LENT		0x56100011
#define HH_RM_NOTIF_VM_IRQ_RELEASED	0x56100012

#define HH_RM_VM_STATUS_NO_STATE	0
#define HH_RM_VM_STATUS_RUNNING		1
#define HH_RM_VM_STATUS_PAUSED		2
#define HH_RM_VM_STATUS_SHUTDOWN	3
#define HH_RM_VM_STATUS_SHUTOFF		4
#define HH_RM_VM_STATUS_CRASHED		5

#define HH_RM_OS_STATUS_NONE		0
#define HH_RM_OS_STATUS_EARLY_BOOT	1
#define HH_RM_OS_STATUS_BOOT		2
#define HH_RM_OS_STATUS_INIT		3
#define HH_RM_OS_STATUS_RUN		4

struct hh_rm_notif_vm_status_payload {
	u16 vmid;
	u16 reserved;
	u8 vm_status;
	u8 os_status;
	u16 app_status;
} __packed;

struct hh_rm_notif_vm_irq_lent_payload {
	u16 owner_vmid;
	u32 virq_handle;
	u32 virq_label;
} __packed;

struct hh_rm_notif_vm_irq_released_payload {
	u32 virq_handle;
} __packed;

/* VM Services */
#define HH_RM_NOTIF_VM_CONSOLE_CHARS	0X56100080

struct hh_rm_notif_vm_console_chars {
	u16 vmid;
	u16 num_bytes;
	u8 bytes[0];
} __packed;

/* End Notification type APIs */

int hh_rm_register_notifier(struct notifier_block *nb);
int hh_rm_unregister_notifier(struct notifier_block *nb);

/* Client APIs for IRQ management */
int hh_rm_vm_irq_accept(hh_virq_handle_t virq_handle, int virq);
int hh_rm_vm_irq_lend_notify(hh_vmid_t vmid, int virq, int label);

/* Client APIs for VM management */
int hh_rm_vm_alloc_vmid(enum hh_vm_names vm_name);
int hh_rm_get_vmid(enum hh_vm_names vm_name, hh_vmid_t *vmid);
int hh_rm_get_vm_name(hh_vmid_t vmid, enum hh_vm_names *vm_name);
int hh_rm_vm_start(int vmid);

/* Client APIs for VM Services */
int hh_rm_console_open(hh_vmid_t vmid);
int hh_rm_console_close(hh_vmid_t vmid);
int hh_rm_console_write(hh_vmid_t vmid, const char *buf, size_t size);
int hh_rm_console_flush(hh_vmid_t vmid);

#endif
