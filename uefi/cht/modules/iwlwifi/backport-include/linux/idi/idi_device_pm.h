#ifndef __BACKPORT_LINUX_IDI_DEVICE_PM_H
#define __BACKPORT_LINUX_IDI_DEVICE_PM_H

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#include <mach/idi_device_pm.h>

static inline const char *
idi_dev_pm_user_name(struct idi_peripheral_device *pdev)
{
	return NULL;
}

#else
#include_next <linux/idi/idi_device_pm.h>

static inline const char *
idi_dev_pm_user_name(struct idi_peripheral_device *pdev)
{
	return pdev->pm_platdata->pm_user_name;
}
#endif

#endif /* __BACKPORT_LINUX_IDI_DEVICE_PM_H */
