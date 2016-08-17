/** @file mbt_char.h
  *
  * @brief This file contains mbtchar driver specific defines etc
  *
  * Copyright (C) 2010-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */
#ifndef __MBT_CHAR_H__
#define __MBT_CHAR_H__

#include <linux/cdev.h>
#include <linux/device.h>

/** Define ioctl */
#define MBTCHAR_IOCTL_RELEASE       _IO('M', 1)
#define MBTCHAR_IOCTL_QUERY_TYPE    _IO('M', 2)

#define MBTCHAR_MAJOR_NUM            (0)

/** Interface specific macros */
#define MBTCHAR_MINOR_BASE           (0)
#define FMCHAR_MINOR_BASE            (10)
#define NFCCHAR_MINOR_BASE           (20)
#define DEBUGCHAR_MINOR_BASE         (30)

/** Declaration of char_dev struct */
struct char_dev
{
	struct list_head list;
	int minor;
	int dev_type;
	struct cdev cdev;
	struct m_dev *m_dev;
};

/** Changes permissions of the dev */
int mbtchar_chmod(char *name, mode_t mode);

/** Changes ownership of the dev */
int mbtchar_chown(char *name, uid_t user, gid_t group);

/**	This function creates the char dev */
int register_char_dev(struct char_dev *dev, struct class *char_class,
		      char *mod_name, char *dev_name);

/**	This function deletes the char dev */
int unregister_char_dev(struct char_dev *dev, struct class *char_class,
			char *dev_name);

/**	This function cleans module */
void chardev_cleanup(struct class *char_class);

/**	This function cleans module */
void chardev_cleanup_one(struct m_dev *m_dev, struct class *char_class);

#endif /*__MBT_CHAR_H__*/
