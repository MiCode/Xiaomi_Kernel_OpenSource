/* SPDX-License-Identifier: GPL-2.0 */

/*
 *
 * (C) COPYRIGHT 2019-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *
 */

/**
 * @file
 * Part of the Mali reference arbiter
 */

#include <xen/interface/io/ring.h>


/**
 * GPU Back-end/Front-end Common Data Structures & Macros
 */
#ifndef _MALI_ARBIF_XEN_H_
#define _MALI_ARBIF_XEN_H_

/* Request FE-->BE I need the GPU */
#define GUEST_GPU_REQUEST (0x10)
/* Request FE-->BE GPU is now suspended */
#define GUEST_GPU_SUSPEND_COMPLETE (0x11)
/* Request FE-->BE GPU has been reset and is suspended */
#define GUEST_GPU_RESET_COMPLETE (0x12)
/* Unknown command */
#define GUEST_ERROR (0x1F)

/* Response BE-->FE You have the GPU */
#define HOST_GPU_GRANTED (0x20)
/* Response BE-->FE Your time is up, (suspend now!)*/
#define HOST_GPU_SUSPEND (0x21)
/* Response BE-->FE Reset GPU now */
#define HOST_GPU_RESET (0x22)
/* Unknown command */
#define HOST_ERROR (0x2F)

/* temporary... */
#define VM_ARB_GPU_IDLE (0x23)
#define VM_ARB_GPU_ACTIVE (0x24)
#define VM_ARB_GPU_REQUEST GUEST_GPU_REQUEST
#define VM_ARB_GPU_STOPPED GUEST_GPU_SUSPEND_COMPLETE

#define ARB_VM_GPU_STOP HOST_GPU_SUSPEND
#define ARB_VM_GPU_GRANTED HOST_GPU_GRANTED
#define ARB_VM_GPU_LOST HOST_GPU_RESET

/* Be sure to bump this number if you change this file */
#define XEN_GPU_MAGIC "43"

/**
 * STATUS RETURN CODES.
 */
 /* Operation failed for some unspecified reason (-EIO). */
#define GPUIF_RSP_ERROR (-1)
 /* Operation completed successfully. */
#define GPUIF_RSP_OKAY (0)

/**
 * struct xengpu_request - Xen GPU command format
 * @cmd: The command id, for example GPU_REQUEST
 * @cmd_param0: optional parameter
 * @cmd_param1: optional second parameter
 * @cmd_param2: optional third parameter
 *
 * This struct is used to define the structure of the messages exchanged
 * between front-end and back-end
 */
struct xengpu_request {
	uint16_t cmd;
	uint64_t cmd_param0;
	uint64_t cmd_param1;
	uint64_t cmd_param2;
} __attribute__((__packed__));

/**
 * struct xengpu_response - Xen GPU response format
 * @status: The status of the operation previously requested (unused)
 *
 * This struct is used to define the structure of the response messages
 * returned by Xen for command sent
 */
struct xengpu_response {
	uint16_t status;
} __attribute__((__packed__));

/**
 * creates ring structures:
 * 1. shared data structure, struct xengpu_sring (holds requests/responses)
 * 2. private FE & BE variables, struct xengpu_front_sring,
 *    struct xengpu_back_sring
 */
DEFINE_RING_TYPES(xengpu, struct xengpu_request, struct xengpu_response);

#endif /* _MALI_ARBIF_XEN_H_ */
