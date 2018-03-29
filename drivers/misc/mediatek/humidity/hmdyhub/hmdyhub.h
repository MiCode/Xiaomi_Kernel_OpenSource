/* BOSCH Pressure Sensor Driver
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

#ifndef _HMDYHUB_H
#define _HMDYHUB_H

#include <linux/ioctl.h>


#define HMDYHUB_DRIVER_VERSION "V0.1"

#define HMDYHUB_DEV_NAME        "hmdy_hub_pl"

#define HMDYHUB_DATA_NUM 1
#define HMDYHUB_PRESSURE         0
#define HMDYHUB_BUFSIZE			60

#endif
