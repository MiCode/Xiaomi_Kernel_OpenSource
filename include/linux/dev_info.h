/* dev_info.h
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


#ifndef __DEV_INFO_H
#define __DEV_INFO_H

#include <linux/ioctl.h>	  
#include <linux/slab.h>

static struct devinfo_struct{
	char *device_type;			// Module type, as LCM, CAMERA/Sub CAMERA, MCP, TP ...
	char *device_module;		// device module, PID
	char *device_vendor;		// device Vendor information, VID
	char *device_ic;			// device module solutions 
	char *device_version;		// device module firmware version, as TP version no
	char *device_info;			// more device infos,as capcity,resolution ...
	char *device_used;			// indicate whether this device is used in this set  
	struct list_head device_link;
};

#define DEVINFO_NULL "(null)"
#define DEVINFO_UNUSED "false"
#define DEVINFO_USED	"true"

#define DEVINFO_CHECK_ADD_DEVICE(devinfo)  \
	do { \
            if(!devinfo_check_add_device(devinfo)){ \
                    kfree(devinfo); \
                    printk("[devinfo] %s free devinfo for not register into devinfo list .\n", __func__);  \
	      }else{ \
	              printk("[devinfo] %s register devinfo into devinfo list .\n", __func__); \
	      } \
	} while (0)

extern int devinfo_check_add_device(struct devinfo_struct *dev);


#endif
