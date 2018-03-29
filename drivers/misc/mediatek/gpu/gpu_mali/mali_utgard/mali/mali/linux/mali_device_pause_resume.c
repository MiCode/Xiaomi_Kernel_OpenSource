/**
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2014, 2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_device_pause_resume.c
 * Implementation of the Mali pause/resume functionality
 */

#include <linux/module.h>
#include <linux/mali/mali_utgard.h>
#include "mali_pm.h"

void mali_dev_pause(void)
{
	/*
	 * Deactive all groups to prevent hardware being touched
	 * during the period of mali device pausing
	 */
	mali_pm_os_suspend(MALI_FALSE);
}

EXPORT_SYMBOL(mali_dev_pause);

void mali_dev_resume(void)
{
	mali_pm_os_resume();
}

EXPORT_SYMBOL(mali_dev_resume);
