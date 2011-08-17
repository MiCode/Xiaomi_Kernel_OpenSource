/* include/linux/smb329.h - smb329 switch charger driver
 *
 * Copyright (C) 2009 HTC Corporation.
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

#ifndef _LINUX_SMB329_H
#define _LINUX_SMB329_H

#ifdef __KERNEL__

enum {
	SMB329_DISABLE_CHG,
	SMB329_ENABLE_SLOW_CHG,
	SMB329_ENABLE_FAST_CHG,
};

extern int smb329_set_charger_ctrl(uint32_t ctl);

#endif /* __KERNEL__ */

#endif /* _LINUX_SMB329_H */

