/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2019-2021 ARM Limited. All rights reserved.
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
 */

#ifndef _MALI_ARBIF_XENBACK_HYPERCALL_H_
#define _MALI_ARBIF_XENBACK_HYPERCALL_H_

#include <xen/interface/mali_xen_hyp.h>

/**
 * HYPERVISOR_arb_gpu_op() - Make an arbiter GPU operation hypercall to xen
 *
 * @op:          Pointer to a xen_arb_gpu_op which describes the operation
 *               being requested
 *
 * This is temporary code to allow the xen page table shadowing. Xen is
 * notified of MMU operations to allow it to create an identical page table
 * for the GPU MMU with gives the GPU the same view of virtual memory as
 * the guest OS which is using it.
 *
 * Return: the result of the hypercall operation. 0 for success, a non-zero
 *         error code otherwise
 */
int HYPERVISOR_arb_gpu_op(struct xen_arb_gpu_op *op);

#endif
