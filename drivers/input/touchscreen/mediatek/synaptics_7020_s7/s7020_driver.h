/*
 * Copyright (C) 2013 LG Electironics, Inc.
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



/****************************************************************************
* Debugging Macros
****************************************************************************/
#define TPD_TAG                  "[Synaptics S7020] "
#define TPD_FUN(f)               printk(KERN_ERR TPD_TAG"[%s %d]\n", __FUNCTION__, __LINE__)
#define TPD_ERR(fmt, args...)    printk(KERN_ERR TPD_TAG"[%s %d] : "fmt, __FUNCTION__, __LINE__, ##args)
#define TPD_LOG(fmt, args...)    printk(KERN_ERR TPD_TAG fmt, ##args)