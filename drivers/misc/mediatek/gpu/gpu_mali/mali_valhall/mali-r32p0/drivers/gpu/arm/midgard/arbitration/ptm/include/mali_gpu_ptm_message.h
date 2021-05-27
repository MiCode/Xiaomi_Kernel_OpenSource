/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT  */

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */


/*
 * Definitions for the PTM_MESSAGE register layout
 */

#ifndef _MALI_GPU_PTM_MESSAGE_H_
#define _MALI_GPU_PTM_MESSAGE_H_

/* Message specific defs of the Partition Manager HW */
#define PTM_MESSAGE_SIZE		0x0020
#define PTM_INCOMING_MESSAGE0		0x0000
#define PTM_INCOMING_MESSAGE1		0x0004
#define PTM_OUTGOING_MESSAGE_STATUS	0x0008
#define PTM_OUTGOING_MESSAGE0		0x000c
#define PTM_OUTGOING_MESSAGE1		0x0010
#define PTM_OUTGOING_MSG_STATUS_MASK	0x01

#endif
