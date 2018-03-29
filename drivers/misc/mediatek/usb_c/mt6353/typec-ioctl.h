/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TYPEC_IOCTL_H
#define _TYPEC_IOCTL_H

int typec_cdev_init(struct device *parent, struct typec_hba *hba, int id);
void typec_cdev_remove(struct typec_hba *hba);
int typec_cdev_module_init(void);
void typec_cdev_module_exit(void);

#endif /* _TYPEC_IOCTL_H */
