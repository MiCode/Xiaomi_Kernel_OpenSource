/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __PLATFORM_PMM_H__
#define __PLATFORM_PMM_H__

struct mali_gpu_utilization_data;

typedef enum mali_power_mode
{
    MALI_POWER_MODE_ON  = 0x1,
    MALI_POWER_MODE_DEEP_SLEEP,
    MALI_POWER_MODE_LIGHT_SLEEP,
    //MALI_POWER_MODE_NUM
} mali_power_mode;

typedef enum mali_dvfs_action
{
    MALI_DVFS_CLOCK_UP = 0x1,
    MALI_DVFS_CLOCK_DOWN,
    MALI_DVFS_CLOCK_NOP
} mali_dvfs_action;

/** @brief Platform power management initialisation of MALI
 *
 * This is called from the entrypoint of the driver to initialize the platform
 *
 */
void mali_pmm_init(void);

/** @brief Platform power management deinitialisation of MALI
 *
 * This is called on the exit of the driver to terminate the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
void mali_pmm_deinit(void);

/** @brief Platform power management mode change of MALI
 *
 * This is called on the power state transistion of MALI.
 * @param mode defines the power modes
 */
void mali_pmm_tri_mode(mali_power_mode mode);

/** @brief Platform power management specific GPU utilization handler
 *
 * When GPU utilization handler is enabled, this function will be
 * periodically called.
 *
 * @param data Collected Mali GPU's work loading.
 */
void mali_pmm_utilization_handler(struct mali_gpu_utilization_data *data);
unsigned int gpu_get_current_utilization(void);

void mali_platform_power_mode_change(mali_power_mode power_mode);

#endif

