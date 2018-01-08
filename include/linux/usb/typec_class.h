/*type c class
 * Copyright (c) 2015-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef _LINUX_TYPEC_CLASS_H
#define _LINUX_TYPEC_CLASS_H

#define BUF_LEN_MAX		16

/*  mode definition
  *  0: Device mode
  *  1: Host mode
  *  2: DRP mode
  */
struct typec_dev {
	const char	*name;

	/* set/get mode */
	int (*set_mode)(struct typec_dev *sdev, int mode);
	int (*get_mode)(struct typec_dev *sdev);

	/* get insert direction for factory test */
	int (*get_direction) (struct typec_dev *sdev);

	/* private data */
	struct device	*dev;
};

extern int typec_dev_register(struct typec_dev *dev);
extern void typec_dev_unregister(struct typec_dev *dev);

#endif
