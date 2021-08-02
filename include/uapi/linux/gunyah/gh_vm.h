/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_LINUX_GH_VM
#define _UAPI_LINUX_GH_VM

/*
 * Userspace interface for /dev/gunyah - gunyah based virtual machine
 *
 * Note: you must update GH_VM_API_VERSION if you change this interface.
 */

#include <linux/types.h>
#include <linux/ioctl.h>

#define GH_VM_IOCTL_TYPE	'g'

#define GH_VM_API_MAJOR_VERSION		0x1
#define GH_VM_API_MINOR_VERSION		0x0

/*
 * gh_vm_api_version is passed as an arg to GH_VM_GET_API_VERSION IOCTL
 ** type - enum gh_vm_types to choose the loader.
 ** name - Name of the firmware to be loaded.
 */
struct gh_vm_api_version {
	unsigned int major;
	unsigned int minor;
};

#define GH_VM_GET_API_VERSION	_IOR(GH_VM_IOCTL_TYPE, 0x1, struct gh_vm_api_version)

/*
 * gh_vm_types specifies the kind of loader the VM needs for its loading.
 * SEC loader is used for pil based loading and authentication.
 */
enum gh_vm_types {
	GH_VM_TYPE_SEC,
	GH_VM_TYPES_MAX,
};

#define GH_VM_NAME_MAX	16

/*
 * gh_vm_create is passed as an arg to GH_VM_CREATE IOCTL
 ** type - enum gh_vm_types to choose the loader.
 ** name - Name of the firmware to be loaded.
 */
struct gh_vm_create {
	__u32 type;
	char name[GH_VM_NAME_MAX];
};

#define GH_VM_CREATE		_IOW(GH_VM_IOCTL_TYPE, 0x2, struct gh_vm_create)

/*
 * Secure VM IOCTLs
 */
#define GH_VM_SEC_START		_IO(GH_VM_IOCTL_TYPE, 0x3)

/*
 * gh_vm_sec_stop provides the ways of STOP that can be
 * issued by userspace to end the execution of SVM.
 */
enum gh_vm_sec_stop {
	GH_VM_STOP_SHUTDOWN,
	GH_VM_STOP_RESTART,
	GH_VM_STOP_CRASH,
	GH_VM_STOP_FORCE_STOP,
	GH_VM_STOP_MAX,
};

#define GH_VM_SEC_STOP			_IOW(GH_VM_IOCTL_TYPE, 0x4, __u32)

/*
 * gh_vm_sec_exit_reasons specifies the various reasons why
 * the secondary VM ended its execution.
 */
enum gh_vm_sec_exit_reasons {
	GH_VM_EXIT_REASON_UNKNOWN,
	GH_VM_EXIT_REASON_SHUTDOWN,
	GH_VM_EXIT_REASON_RESTART,
	GH_VM_EXIT_REASON_PANIC,
	GH_VM_EXIT_REASON_NSWD,
	GH_VM_EXIT_REASON_HYP_ERROR,
	GH_VM_EXIT_REASON_ASYNC_EXT_ABORT,
	GH_VM_EXIT_REASON_FORCE_STOPPED,
	GH_VM_EXIT_REASONS_MAX,
};

/*
 * gh_vm_sec_exit_status is passed as an argument to
   GH_VM_SEC_WAIT_FOR_EXIT ioctl
 ** reason - exit reason specified by kernel
 **          in the form of gh_vm_sec_exit_reasons
 */
struct gh_vm_sec_exit_status {
	__u32 reason;
};

#define GH_VM_SEC_WAIT_FOR_EXIT \
	_IOR(GH_VM_IOCTL_TYPE, 0x5,  struct gh_vm_sec_exit_status)

/*
 * gh_vm_sec_restart_levels specifies how a fatal error in the SVM should
 * be handled. SYSTEM will panic the entire system including PVM and
 * RELATIVE will just cleaup SVM and restart SVM.
 */
enum gh_vm_sec_restart_levels {
	GH_VM_RESTART_LEVEL_SYSTEM,	/* Restart the entire system */
	GH_VM_RESTART_LEVEL_RELATIVE,	/* Restart only the VM that crashed */
	GH_VM_RESTART_LEVELS_MAX,
};

#define GH_VM_SEC_SET_RESTART_LEVEL	_IOW(GH_VM_IOCTL_TYPE, 0x6, __u32)

/* End Secure VM IOCTLs */

#endif /* _UAPI_LINUX_GH_VM */
