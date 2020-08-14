/*
 *
 * (C) COPYRIGHT 2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
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
 * SPDX-License-Identifier: GPL-2.0
 *
 */

/* Detect if the Mali shim header */
#define _TRACE_POWER_GPU_FREQUENCY_H
#include <trace/events/power_gpu_frequency.h>
#undef _TRACE_POWER_GPU_FREQUENCY_H

/* Create the trace points if Mali shim */
#ifdef _TRACE_POWER_GPU_FREQUENCY_MALI
#define CREATE_TRACE_POINTS
#include <trace/events/power_gpu_frequency.h>

EXPORT_TRACEPOINT_SYMBOL_GPL(gpu_frequency);
#endif
