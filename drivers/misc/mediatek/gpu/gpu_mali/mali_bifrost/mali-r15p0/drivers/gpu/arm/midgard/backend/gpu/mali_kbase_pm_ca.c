/*
 *
 * (C) COPYRIGHT 2013-2018 ARM Limited. All rights reserved.
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
 * Base kernel core availability APIs
 */

#include <mali_kbase.h>
#include <mali_kbase_pm.h>
#include <backend/gpu/mali_kbase_pm_internal.h>

int kbase_pm_ca_init(struct kbase_device *kbdev)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;
#ifdef CONFIG_MALI_DEVFREQ
	if (kbdev->current_core_mask)
		pm_backend->ca_cores_enabled = kbdev->current_core_mask;
	else
		pm_backend->ca_cores_enabled =
				kbdev->gpu_props.props.raw_props.shader_present;
#endif
	pm_backend->ca_in_transition = false;

	return 0;
}

void kbase_pm_ca_term(struct kbase_device *kbdev)
{
}

#ifdef CONFIG_MALI_DEVFREQ
void kbase_devfreq_set_core_mask(struct kbase_device *kbdev, u64 core_mask)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;
	unsigned long flags;

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);

	pm_backend->ca_cores_enabled = core_mask;

	kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	dev_dbg(kbdev->dev, "Devfreq policy : new core mask=%llX\n",
			pm_backend->ca_cores_enabled);
}
#endif

u64 kbase_pm_ca_get_core_mask(struct kbase_device *kbdev)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;

	lockdep_assert_held(&kbdev->hwaccess_lock);

	/* All cores must be enabled when instrumentation is in use */
	if (pm_backend->instr_enabled)
		return kbdev->gpu_props.props.raw_props.shader_present &
				kbdev->pm.debug_core_mask_all;

#ifdef CONFIG_MALI_DEVFREQ
	return pm_backend->ca_cores_enabled & kbdev->pm.debug_core_mask_all;
#else
	return kbdev->gpu_props.props.raw_props.shader_present &
			kbdev->pm.debug_core_mask_all;
#endif
}

KBASE_EXPORT_TEST_API(kbase_pm_ca_get_core_mask);

void kbase_pm_ca_instr_enable(struct kbase_device *kbdev)
{
	unsigned long flags;

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	kbdev->pm.backend.instr_enabled = true;

	kbase_pm_update_cores_state_nolock(kbdev);
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
}

void kbase_pm_ca_instr_disable(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);
	kbdev->pm.backend.instr_enabled = false;

	kbase_pm_update_cores_state_nolock(kbdev);
}
