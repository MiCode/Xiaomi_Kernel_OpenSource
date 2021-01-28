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

#ifndef _KBASE_CLK_RATE_TRACE_MGR_
#define _KBASE_CLK_RATE_TRACE_MGR_

#include <mali_kbase_ccswe.h>

/**
 * struct kbase_clk_data - Data stored per enumerated GPU clock.
 *
 * @clk_rtm:            Pointer to clock rate trace manager object.
 * @gpu_clk_handle:     Handle unique to the enumerated GPU clock.
 * @plat_private:       Private data for the platform to store into
 * @clk_rate_change_nb: notifier block containing the pointer to callback
 *                      function that is invoked whenever the rate of
 *                      enumerated GPU clock changes.
 * @clock_val:          Current rate of the enumerated GPU clock.
 * @ccswe:              Cycle count software estimator.
 * @index:              Index at which the GPU clock was enumerated.
 */
struct kbase_clk_data {
	struct kbase_clk_rate_trace_manager *clk_rtm;
	void *gpu_clk_handle;
	void *plat_private;
	struct notifier_block clk_rate_change_nb;
	unsigned long clock_val;
	struct kbase_ccswe ccswe;
	u8 index;
};

/**
 * kbase_clk_rate_trace_manager_init - Initialize GPU clock rate trace manager.
 *
 * @kbdev:      Device pointer
 *
 * Return: 0 if success, or an error code on failure.
 */
int kbase_clk_rate_trace_manager_init(struct kbase_device *kbdev);

/**
 * kbase_clk_rate_trace_manager_term - Terminate GPU clock rate trace manager.
 *
 *  @kbdev:      Device pointer
 */
void kbase_clk_rate_trace_manager_term(struct kbase_device *kbdev);

/**
 * kbase_clk_rate_trace_manager_gpu_active - Inform GPU clock rate trace
 *                                           manager of GPU becoming active.
 *
 * @kbdev:      Device pointer
 */
void kbase_clk_rate_trace_manager_gpu_active(struct kbase_device *kbdev);

/**
 * kbase_clk_rate_trace_manager_gpu_idle - Inform GPU clock rate trace
 *                                         manager of GPU becoming idle.
 * @kbdev:      Device pointer
 */
void kbase_clk_rate_trace_manager_gpu_idle(struct kbase_device *kbdev);

#endif /* _KBASE_CLK_RATE_TRACE_MGR_ */

