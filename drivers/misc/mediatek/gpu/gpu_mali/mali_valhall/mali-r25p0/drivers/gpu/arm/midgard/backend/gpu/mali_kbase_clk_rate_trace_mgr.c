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

/*
 * Implementation of the GPU clock rate trace manager.
 */

#include <mali_kbase.h>
#include <mali_kbase_config_defaults.h>
#include <linux/clk.h>
#include "mali_kbase_clk_rate_trace_mgr.h"
#include <trace/events/power_gpu_frequency.h>

#ifndef CLK_RATE_TRACE_OPS
#define CLK_RATE_TRACE_OPS (NULL)
#endif


static void gpu_clk_rate_trace_write(struct kbase_device *kbdev,
		unsigned int clk_index, unsigned long new_rate)
{
	dev_dbg(kbdev->dev, "GPU clock %u rate changed to %lu",
		clk_index, new_rate);

	/* Raise standard `power/gpu_frequency` ftrace event */
	trace_gpu_frequency(new_rate, clk_index);
}

static int gpu_clk_rate_change_notifier(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct kbase_gpu_clk_notifier_data *ndata = data;
	struct kbase_clk_data *clk_data =
		container_of(nb, struct kbase_clk_data, clk_rate_change_nb);
	struct kbase_clk_rate_trace_manager *clk_rtm = clk_data->clk_rtm;
	struct kbase_device *kbdev =
		container_of(clk_rtm, struct kbase_device, pm.clk_rtm);

	if (WARN_ON_ONCE(clk_data->gpu_clk_handle != ndata->gpu_clk_handle))
		return NOTIFY_BAD;

	spin_lock(&clk_rtm->lock);
	if (event == POST_RATE_CHANGE) {
		if (!clk_rtm->gpu_idle &&
		    (clk_data->clock_val != ndata->new_rate)) {
			clk_rtm->gpu_clk_rate_trace_write(kbdev,
							  clk_data->index,
							  ndata->new_rate);
		}

		clk_data->clock_val = ndata->new_rate;
	}
	spin_unlock(&clk_rtm->lock);

	return NOTIFY_DONE;
}

static int gpu_clk_data_init(struct kbase_device *kbdev,
		void *gpu_clk_handle, unsigned int index)
{
	struct kbase_clk_rate_trace_op_conf *callbacks =
		(struct kbase_clk_rate_trace_op_conf *)CLK_RATE_TRACE_OPS;
	struct kbase_clk_data *clk_data;
	int ret = 0;

	if (WARN_ON(!callbacks) ||
	    WARN_ON(!gpu_clk_handle) ||
	    WARN_ON(index >= BASE_MAX_NR_CLOCKS_REGULATORS))
		return -EINVAL;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data) {
		dev_err(kbdev->dev, "Failed to allocate data for clock enumerated at index %u", index);
		return -ENOMEM;
	}

	clk_data->index = (u8)index;
	clk_data->gpu_clk_handle = gpu_clk_handle;
	/* Store the initial value of clock */
	clk_data->clock_val =
		callbacks->get_gpu_clk_rate(kbdev, gpu_clk_handle);
	/* GPU is idle */
	kbdev->pm.clk_rtm.gpu_clk_rate_trace_write(kbdev, clk_data->index, 0);

	clk_data->clk_rtm = &kbdev->pm.clk_rtm;
	kbdev->pm.clk_rtm.clks[index] = clk_data;

	clk_data->clk_rate_change_nb.notifier_call =
			gpu_clk_rate_change_notifier;

	ret = callbacks->gpu_clk_notifier_register(kbdev, gpu_clk_handle,
			&clk_data->clk_rate_change_nb);
	if (ret) {
		dev_err(kbdev->dev, "Failed to register notifier for clock enumerated at index %u", index);
		kfree(clk_data);
	}

	return ret;
}

int kbase_clk_rate_trace_manager_init(struct kbase_device *kbdev)
{
	struct kbase_clk_rate_trace_op_conf *callbacks =
		(struct kbase_clk_rate_trace_op_conf *)CLK_RATE_TRACE_OPS;
	struct kbase_clk_rate_trace_manager *clk_rtm = &kbdev->pm.clk_rtm;
	unsigned int i;
	int ret = 0;

	/* Return early if no callbacks provided for clock rate tracing */
	if (!callbacks)
		return 0;

	/* GPU shall be idle at this point and shall remain idle whilst this
	 * function is executing as /dev/mali0 file won't be visible yet to the
	 * userspace and so the gpu active/idle transitions also won't happen.
	 */
	if (WARN_ON(kbase_pm_is_active(kbdev)))
		return -EINVAL;

	spin_lock_init(&clk_rtm->lock);
	clk_rtm->gpu_idle = true;

	/* If this function pointer is deemed superfluous, it can be replaced
	 * with a direct function call.
	 */
	clk_rtm->gpu_clk_rate_trace_write = gpu_clk_rate_trace_write;

	for (i = 0; i < BASE_MAX_NR_CLOCKS_REGULATORS; i++) {
		void *gpu_clk_handle =
			callbacks->enumerate_gpu_clk(kbdev, i);

		if (!gpu_clk_handle)
			break;

		ret = gpu_clk_data_init(kbdev, gpu_clk_handle, i);
		if (ret)
			goto error;
	}

	/* Activate clock rate trace manager if at least one GPU clock was
	 * enumerated.
	 */
	if (i)
		WRITE_ONCE(clk_rtm->clk_rate_trace_ops, callbacks);
	else
		dev_info(kbdev->dev, "No clock(s) available for rate tracing");

	return 0;

error:
	while (i--) {
		clk_rtm->clk_rate_trace_ops->gpu_clk_notifier_unregister(
				kbdev, clk_rtm->clks[i]->gpu_clk_handle,
				&clk_rtm->clks[i]->clk_rate_change_nb);
		kfree(clk_rtm->clks[i]);
	}

	return ret;
}

void kbase_clk_rate_trace_manager_term(struct kbase_device *kbdev)
{
	struct kbase_clk_rate_trace_manager *clk_rtm = &kbdev->pm.clk_rtm;
	unsigned int i;

	if (!clk_rtm->clk_rate_trace_ops)
		return;

	for (i = 0; i < BASE_MAX_NR_CLOCKS_REGULATORS; i++) {
		if (!clk_rtm->clks[i])
			break;

		clk_rtm->clk_rate_trace_ops->gpu_clk_notifier_unregister(
				kbdev, clk_rtm->clks[i]->gpu_clk_handle,
				&clk_rtm->clks[i]->clk_rate_change_nb);
		kfree(clk_rtm->clks[i]);
	}

	WRITE_ONCE(clk_rtm->clk_rate_trace_ops, NULL);
}

void kbase_clk_rate_trace_manager_gpu_active(struct kbase_device *kbdev)
{
	struct kbase_clk_rate_trace_manager *clk_rtm = &kbdev->pm.clk_rtm;
	unsigned int i;

	if (!clk_rtm->clk_rate_trace_ops)
		return;

	spin_lock(&clk_rtm->lock);

	for (i = 0; i < BASE_MAX_NR_CLOCKS_REGULATORS; i++) {
		if (!clk_rtm->clks[i])
			break;

		if (unlikely(!clk_rtm->clks[i]->clock_val))
			continue;

		clk_rtm->gpu_clk_rate_trace_write(kbdev,
						  clk_rtm->clks[i]->index,
						  clk_rtm->clks[i]->clock_val);
	}

	clk_rtm->gpu_idle = false;
	spin_unlock(&clk_rtm->lock);
}

void kbase_clk_rate_trace_manager_gpu_idle(struct kbase_device *kbdev)
{
	struct kbase_clk_rate_trace_manager *clk_rtm = &kbdev->pm.clk_rtm;
	unsigned int i;

	if (!clk_rtm->clk_rate_trace_ops)
		return;

	spin_lock(&clk_rtm->lock);

	for (i = 0; i < BASE_MAX_NR_CLOCKS_REGULATORS; i++) {
		if (!clk_rtm->clks[i])
			break;

		if (unlikely(!clk_rtm->clks[i]->clock_val))
			continue;

		clk_rtm->gpu_clk_rate_trace_write(kbdev,
						  clk_rtm->clks[i]->index, 0);
	}

	clk_rtm->gpu_idle = true;
	spin_unlock(&clk_rtm->lock);
}
