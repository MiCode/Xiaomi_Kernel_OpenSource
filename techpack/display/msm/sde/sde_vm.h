/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_VM_H__
#define __SDE_VM_H__

#include "msm_drv.h"

#define TRUSTED_VM_MAX_ENCODER_PER_CRTC	1

struct sde_kms;

/* sde_vm_msg_type - msg_type for dispaly custom messages */
enum sde_vm_msg_type {
	SDE_VM_MSG_MAX,
};

/**
 * sde_vm_msg_work - sde msgq work definition
 * @work - base kthread work object
 * @msg_buf - payload buffer
 * @msg_size - payload buffer size
 * @sde_vm - handle to vm structure
 */
struct sde_vm_msg_work {
	struct kthread_work work;
	void *msg_buf;
	size_t msg_size;
	struct sde_vm *sde_vm;
};

/**
 * sde_vm_msg_header - header definition for custom messages. Must
 *                     be placed at the beginning of each custom
 *                     message definition.
 * @msg_type - type of the message
 */
struct sde_vm_msg_header {
	enum sde_vm_msg_type msg_type;
};

/**
 * sde_vm_irq_entry - VM irq specification
 * @label - VM_IRQ_LABEL assigned by Hyp RM
 * @irq - linux mapped irq number
 */
struct sde_vm_irq_entry {
	u32 label;
	u32 irq;
};

/**
 * sde_vm_irq_desc - list of IRQ's to be handled
 * @n_irq - irq count
 * @irq_entries - list of sde_vm_irq_entry
 */
struct sde_vm_irq_desc {
	u32 n_irq;
	struct sde_vm_irq_entry *irq_entries;
};

enum sde_crtc_vm_req;

/**
 * sde_vm_ops - VM specific function hooks
 */
struct sde_vm_ops {
	/**
	 * vm_acquire - hook to handle HW accept
	 * @kms - handle to sde_kms
	 * @return - return 0 on success
	 */
	int (*vm_acquire)(struct sde_kms *kms);

	/**
	 * vm_release - hook to handle HW release
	 * @kms - handle to sde_kms
	 * @return - return 0 on success
	 */
	int (*vm_release)(struct sde_kms *kms);

	/**
	 * vm_owns_hw - hook to query the HW status of the VM
	 * @kms - handle to sde_kms
	 * @return - return true when vm owns the hw
	 */
	bool (*vm_owns_hw)(struct sde_kms *kms);

	/**
	 * vm_prepare_commit - hook to handle operations before the first
			       commit after acquiring the HW
	 * @sde_kms - handle to sde_kms
	 * @state - global atomic state to be parsed
	 * @return - return 0 on success
	 */
	int (*vm_prepare_commit)(struct sde_kms *sde_kms,
			struct drm_atomic_state *state);

	/**
	 * vm_post_commit - hook to handle operations after
			    last commit before release
	 * @sde_kms - handle to sde_kms
	 * @state - global atomic state to be parsed
	 * @return - return 0 on success
	 */
	int (*vm_post_commit)(struct sde_kms *sde_kms,
			struct drm_atomic_state *state);

	/**
	 * vm_deinit - deinitialize VM layer
	 * @kms - pointer to sde_kms
	 * @ops - primary VM specific ops functions
	 */
	void (*vm_deinit)(struct sde_kms *kms, struct sde_vm_ops *ops);

	/**
	 * vm_check - hook to check with vm_clients for its readiness to release
		      the HW reasources
	 */
	int (*vm_check)(void);

	/**
	 * vm_client_pre_release - hook to invoke vm_client list for pre_release
				   handling
	 * @kms - handle to sde_kms
	 */
	int (*vm_client_pre_release)(struct sde_kms *kms);

	/**
	 * vm_client_post_acquire - hook to invoke vm_client list for
	 *			    post_acquire resource handling
	 * @kms - handle to sde_kms
	 */
	int (*vm_client_post_acquire)(struct sde_kms *kms);

	/**
	 * vm_request_valid - hook to validate the RM_REQ state change
	 * @sde_kms - handle to sde_kms
	 * @old_state - current vm_req state
	 * @new_state - new vm_req state
	 */
	int (*vm_request_valid)(struct sde_kms *sde_kms,
			enum sde_crtc_vm_req old_state,
			enum sde_crtc_vm_req new_state);

	/**
	 * vm_acquire_fail_handler - hook to the handler when resource
	 *                           accept/reclaim fails.
	 * @sde_kms - handle to sde_kms
	 */
	int (*vm_acquire_fail_handler)(struct sde_kms *sde_kms);

	/** vm_msg_recv_cb - sde kms callback hook for msgq data
	 * @sde_vm - handle to sde_vm struct
	 * @data - paylod data
	 * @size - size of payload data
	 */
	void (*vm_msg_recv_cb)(struct sde_vm *sde_vm, void *data, size_t size);

	/** vm_msg_send - hook to send custom data to VM
	 * @sde_vm - handle to sde_vm struct
	 * @msg - payload data
	 * @msg_size - payload data size
	 * @return - 0 on success, errorcode otherwise
	 */
	int (*vm_msg_send)(struct sde_vm *sde_vm, void *msg, size_t msg_size);

	/**
	 * vm_resource_init - hook to the handler when resource
	 *                           accept/reclaim fails.
	 * @sde_kms - handle to sde_kms
	 * @state: current update atomic commit state
	 */
	int (*vm_resource_init)(struct sde_kms *sde_kms,
			struct drm_atomic_state *state);
};

/**
 * sde_vm - VM layer descriptor. Abstract for all the VM's
 * @vm_res_lock - mutex to protect resource updates
 * @mem_notificaiton_cookie - Hyp RM notification identifier
 * @n_irq_lent - irq count
 * @io_mem_handle - RM identifier for the IO range
 * @sde_kms - handle to sde_kms
 * @vm_ops - VM operation hooks for respective VM type
 * @msgq_listener_thread - handle to msgq receiver thread
 * @vm_work - kthread work obj for msgq
 * @msgq_handle - handle to display msgq
 */
struct sde_vm {
	struct mutex vm_res_lock;
	void *mem_notification_cookie;
	atomic_t n_irq_lent;
	int io_mem_handle;
	struct sde_kms *sde_kms;
	struct sde_vm_ops vm_ops;
	struct task_struct *msgq_listener_thread;
	struct sde_vm_msg_work vm_work;
	void *msgq_handle;
};

/**
 * sde_vm_primary - VM layer descriptor for Primary VM
 * @base - parent struct object
 * @irq_desc - cache copy of irq list for validating reclaim
 */
struct sde_vm_primary {
	struct sde_vm base;
	struct sde_vm_irq_desc *irq_desc;
};

/**
 * sde_vm_trusted - VM layer descriptor for Trusted VM
 * @base - parent struct object
 * @sgl_desc - hyp RM sgl list descriptor for IO ranges
 * @irq_desc - irq list
 */
struct sde_vm_trusted {
	struct sde_vm base;
	struct sde_vm_irq_desc *irq_desc;
	struct hh_sgl_desc *sgl_desc;
};

#if IS_ENABLED(CONFIG_DRM_SDE_VM)
/**
 * sde_vm_primary_init - Initialize primary VM layer
 * @kms - pointer to sde_kms
 * @return - 0 on success
 */
int sde_vm_primary_init(struct sde_kms *kms);

/**
 * sde_vm_trusted_init - Initialize Trusted VM layer
 * @kms - pointer to sde_kms
 * @ops - primary VM specific ops functions
 * @return - 0 on success
 */
int sde_vm_trusted_init(struct sde_kms *kms);

/**
 * sde_vm_is_enabled - check whether TUI feature is enabled
 * @sde_kms - pointer to sde_kms
 * @return - true if enabled, false otherwise
 */
static inline bool sde_vm_is_enabled(struct sde_kms *sde_kms)
{
	return !!sde_kms->vm;
}

/**
 * sde_vm_lock - lock vm variables
 * @sde_kms - pointer to sde_kms
 */
static inline void sde_vm_lock(struct sde_kms *sde_kms)
{
	if (!sde_kms->vm)
		return;

	mutex_lock(&sde_kms->vm->vm_res_lock);
}

/**
 * sde_vm_unlock - unlock vm variables
 * @sde_kms - pointer to sde_kms
 */
static inline void sde_vm_unlock(struct sde_kms *sde_kms)
{
	if (!sde_kms->vm)
		return;

	mutex_unlock(&sde_kms->vm->vm_res_lock);
}

/**
 * sde_vm_get_ops - helper API to retrieve sde_vm_ops
 * @sde_kms - pointer to sde_kms
 * @return - pointer to sde_vm_ops
 */
static inline struct sde_vm_ops *sde_vm_get_ops(struct sde_kms *sde_kms)
{
	if (!sde_kms->vm)
		return NULL;

	return &sde_kms->vm->vm_ops;
}
#else
static inline int sde_vm_primary_init(struct sde_kms *kms)
{
	return 0;
}

static inline int sde_vm_trusted_init(struct sde_kms *kms)
{
	return 0;
}

static inline bool sde_vm_is_enabled(struct sde_kms *sde_kms)
{
	return false;
}

static inline void sde_vm_lock(struct sde_kms *sde_kms)
{
}

static inline void sde_vm_unlock(struct sde_kms *sde_kms)
{
}

static inline struct sde_vm_ops *sde_vm_get_ops(struct sde_kms *sde_kms)
{
	return NULL;
}

#endif /* IS_ENABLED(CONFIG_DRM_SDE_VM) */
#endif /* __SDE_VM_H__ */
