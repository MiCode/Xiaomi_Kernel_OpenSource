/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_MFD_SC27XX_PMIC_H
#define __LINUX_MFD_SC27XX_PMIC_H

extern enum usb_charger_type sprd_pmic_detect_charger_type(struct device *dev);
extern int sprd_pmic_power_boot_time(u32 *count);

#endif /* __LINUX_MFD_SC27XX_PMIC_H */
