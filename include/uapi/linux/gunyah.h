/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _UAPI_LINUX_GUNYAH
#define _UAPI_LINUX_GUNYAH

/*
 * Userspace interface for /dev/gunyah - gunyah based virtual machine
 *
 * Note: this interface is considered experimental and may change without
 *       notice.
 */

#include <linux/types.h>
#include <linux/ioctl.h>

#define GH_IOCTL_TYPE			0xB2

/*
 * fw_name is used to find the secure VM image by name to be loaded.
 */
#define GH_VM_FW_NAME_MAX		16

/** @struct gh_fw_name
 * A structure to be passed to GH_VM_SET_FM_NAME ioctl
 * @name - name of the secure VM image
 */
struct gh_fw_name {
	char name[GH_VM_FW_NAME_MAX];
};

/*
 * gh_vm_exit_reasons specifies the various reasons why
 * the secondary VM ended its execution. VCPU_RUN returns these values
 * to userspace.
 */
#define GH_VM_EXIT_REASON_UNKNOWN		0
#define GH_VM_EXIT_REASON_SHUTDOWN		1
#define GH_VM_EXIT_REASON_RESTART		2
#define GH_VM_EXIT_REASON_PANIC			3
#define GH_VM_EXIT_REASON_NSWD			4
#define GH_VM_EXIT_REASON_HYP_ERROR		5
#define GH_VM_EXIT_REASON_ASYNC_EXT_ABORT	6
#define GH_VM_EXIT_REASON_FORCE_STOPPED		7
#define GH_VM_EXIT_REASONS_MAX			8

/*
 * ioctls for /dev/gunyah fds:
 */
/**
 * GH_CREATE_VM - Driver creates a VM sepecific structure. An anon file is
 *		  also created per VM. This would be the first IOCTL made
 *		  on /dev/gunyah node to obtain a per VM fd for futher
 *		  VM specific operations like VCPU creation, memory etc.
 *
 * Return: an fd for the per VM file created, -errno on failure
 */
#define GH_CREATE_VM			_IO(GH_IOCTL_TYPE, 0x01)

/*
 * ioctls for VM fd.
 */
/**
 * GH_CREATE_VCPU - Driver creates a VCPU sepecific structure. It takes
 *		    vcpu id as the input. This also creates an anon file
 *		    per vcpu which is used for further vcpu specific
 *		    operations.
 *
 * Return: an fd for the per VCPU file created, -errno on failure
 */
#define GH_CREATE_VCPU			_IO(GH_IOCTL_TYPE, 0x40)
/*
 * ioctls for VM properties
 */
/**
 * GH_VM_SET_FW_NAME - Userspace will specify the name of the firmware
 *		       image that needs to be loaded into VM's memory
 *		       after authentication. The loaded VM memory details
 *		       are forwarded to Gunyah Hypervisor underneath.
 *
 * Input: gh_fw_name structure with Secure VM name as name attribute of
 *        the struct.
 * Return: 0 if success, -errno on failure
 */
#define GH_VM_SET_FW_NAME		_IOW(GH_IOCTL_TYPE, 0x41, struct gh_fw_name)
/**
 * GH_VM_GET_FW_NAME - Userspace can use this IOCTL to query the name of
 *		       the secure VM image that was loaded.
 *
 * Input: gh_fw_name structure to be filled with Secure VM name as the
 *	  name attribute of the struct.
 * Return: 0 if success and firmware name in struct fw_name that
 *         represents the firmware image name currently associated with
 *         the VM if a call to GH_VM_SET_FW_NAME ioctl previously was
 *         successful, -errno on failure
 */
#define GH_VM_GET_FW_NAME		_IOR(GH_IOCTL_TYPE, 0x42, struct gh_fw_name)
/*
 * ioctls for vcpu fd.
 */
/**
 * GH_VCPU_RUN - This command is used to run the vcpus created. VCPU_RUN
 *		 is called on vcpu fd created previously. VCPUs are
 *		 started individually if proxy scheduling is chosen as the
 *		 scheduling policy and vcpus are started simultaneously
 *		 in case of VMs whose scheduling is controlled by the
 *		 hypervisor. In the latter case, VCPU_RUN is blocked
 *		 until the VM terminates.
 *
 * Return: Reason for vm termination, -errno on failure
 */
#define GH_VCPU_RUN			_IO(GH_IOCTL_TYPE, 0x80)

#endif /* _UAPI_LINUX_GUNYAH */
