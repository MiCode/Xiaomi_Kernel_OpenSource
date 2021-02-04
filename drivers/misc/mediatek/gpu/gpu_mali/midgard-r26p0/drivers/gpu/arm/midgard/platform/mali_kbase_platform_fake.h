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

/**
 * kbase_platform_fake_register - Entry point for fake platform registration
 *
 * This function is called early on in the initialization during execution of
 * kbase_driver_init.
 *
 * Return: 0 to indicate success, non-zero for failure.
 */
int kbase_platform_fake_register(void);

/**
 * kbase_platform_fake_unregister - Entry point for fake platform unregistration
 *
 * This function is called in the termination during execution of
 * kbase_driver_exit.
 */
void kbase_platform_fake_unregister(void);

#endif /* CONFIG_MALI_PLATFORM_FAKE */
