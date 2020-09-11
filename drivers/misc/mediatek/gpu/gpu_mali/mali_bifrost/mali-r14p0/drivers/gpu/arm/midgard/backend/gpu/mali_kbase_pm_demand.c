/*
 *
 * (C) COPYRIGHT 2010-2018 ARM Limited. All rights reserved.
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
 * A simple demand based power management policy
 */

#include <mali_kbase.h>
#include <mali_kbase_pm.h>

static bool demand_shaders_needed(struct kbase_device *kbdev)
{
	return (kbdev->shader_needed_cnt > 0);
}

static bool demand_get_core_active(struct kbase_device *kbdev)
{
	return kbase_pm_is_active(kbdev);
}

static void demand_init(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

static void demand_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

/*
 * The struct kbase_pm_policy structure for the demand power policy.
 *
 * This is the static structure that defines the demand power policy's callback
 * and name.
 */
const struct kbase_pm_policy kbase_pm_demand_policy_ops = {
	"demand",			/* name */
	demand_init,			/* init */
	demand_term,			/* term */
	demand_shaders_needed,		/* shaders_needed */
	demand_get_core_active,		/* get_core_active */
	0u,				/* flags */
	KBASE_PM_POLICY_ID_DEMAND,	/* id */
};

KBASE_EXPORT_TEST_API(kbase_pm_demand_policy_ops);
